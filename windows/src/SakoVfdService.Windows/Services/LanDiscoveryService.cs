using System.Collections.Concurrent;
using System.Net;
using System.Net.Http;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text.Json;
using SakoVfdService.Windows.Models;

namespace SakoVfdService.Windows.Services;

/// <summary>
/// Finds existing ESP32 firmware v0.1.2 without requiring an mDNS firmware patch.
/// Bounded /24 scanning is intentional: never sweep arbitrary routed networks.
/// </summary>
public sealed class LanDiscoveryService : IDisposable
{
    private readonly HttpClient _http = new(new SocketsHttpHandler
    {
        ConnectTimeout = TimeSpan.FromMilliseconds(600),
        PooledConnectionLifetime = TimeSpan.FromMinutes(2)
    }) { Timeout = TimeSpan.FromSeconds(2) };

    public IReadOnlyList<string> GetCandidateAddresses()
    {
        var all = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var nic in NetworkInterface.GetAllNetworkInterfaces())
        {
            if (nic.OperationalStatus != OperationalStatus.Up ||
                nic.NetworkInterfaceType is NetworkInterfaceType.Loopback or NetworkInterfaceType.Tunnel)
                continue;

            foreach (var a in nic.GetIPProperties().UnicastAddresses)
            {
                if (a.Address.AddressFamily != AddressFamily.InterNetwork) continue;
                if (IPAddress.IsLoopback(a.Address)) continue;
                var octets = a.Address.GetAddressBytes();
                if (octets[0] == 169 && octets[1] == 254) continue;
                if (octets[0] == 0 || octets[0] >= 224) continue;

                // Most office Wi-Fi networks use /24. On larger networks this
                // scans only the local /24 containing this PC, by design.
                for (var host = 1; host < 255; host++)
                {
                    if (host == octets[3]) continue;
                    all.Add($"{octets[0]}.{octets[1]}.{octets[2]}.{host}");
                }
            }
        }
        return all.OrderBy(s => s, StringComparer.Ordinal).Take(768).ToList();
    }

    public async Task<IReadOnlyList<DeviceInfo>> ScanAsync(
        IProgress<DiscoveryProgress>? progress, CancellationToken cancellationToken)
    {
        var addresses = GetCandidateAddresses();
        var found = new ConcurrentDictionary<string, DeviceInfo>();
        var tested = 0;
        using var gate = new SemaphoreSlim(36);
        var tasks = addresses.Select(async ip =>
        {
            await gate.WaitAsync(cancellationToken);
            try
            {
                var device = await ProbeAsync(ip, cancellationToken);
                if (device is not null) found[ip] = device;
                var count = Interlocked.Increment(ref tested);
                progress?.Report(new DiscoveryProgress(count, addresses.Count, device));
            }
            finally { gate.Release(); }
        });
        await Task.WhenAll(tasks);
        return found.Values.OrderBy(d => d.IpAddress).ToArray();
    }

    public async Task<DeviceInfo?> ProbeAsync(string ip, CancellationToken cancellationToken = default)
    {
        if (!IPAddress.TryParse(ip, out var address) ||
            address.AddressFamily != AddressFamily.InterNetwork)
            return null;

        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(TimeSpan.FromMilliseconds(1050));
        try
        {
            using var response = await _http.GetAsync(
                $"http://{ip}/api/status", HttpCompletionOption.ResponseHeadersRead, timeout.Token);
            if (!response.IsSuccessStatusCode) return null;
            if (response.Content.Headers.ContentLength > 16_384) return null;
            await using var stream = await response.Content.ReadAsStreamAsync(timeout.Token);
            using var json = await JsonDocument.ParseAsync(stream,
                new JsonDocumentOptions { MaxDepth = 10 }, timeout.Token);
            var root = json.RootElement;
            if (root.ValueKind != JsonValueKind.Object) return null;
            var hardware = GetText(root, "hardware");
            var firmware = GetText(root, "firmware");
            // Do not identify unrelated HTTP servers as SAKO equipment.
            if (!hardware.Contains("Waveshare ESP32-S3-RS485-CAN", StringComparison.OrdinalIgnoreCase) ||
                !firmware.Contains("SAKO", StringComparison.OrdinalIgnoreCase))
                return null;
            return new DeviceInfo(ip, hardware, firmware,
                GetNumber(root, "slave"), GetNumber(root, "baud"), GetText(root, "network_mode"));
        }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException or
                                      OperationCanceledException or JsonException or IOException)
        {
            if (cancellationToken.IsCancellationRequested) throw new OperationCanceledException(cancellationToken);
            return null;
        }
    }

    public async Task<bool?> CheckVfdConnectedAsync(string ip, CancellationToken cancellationToken = default)
    {
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(TimeSpan.FromSeconds(2));
        try
        {
            using var response = await _http.GetAsync($"http://{ip}/api/monitor", timeout.Token);
            if (!response.IsSuccessStatusCode) return null;
            await using var stream = await response.Content.ReadAsStreamAsync(timeout.Token);
            using var json = await JsonDocument.ParseAsync(stream, cancellationToken: timeout.Token);
            var root = json.RootElement;
            return root.TryGetProperty("online", out var online) &&
                   online.ValueKind is JsonValueKind.True or JsonValueKind.False
                ? online.GetBoolean() : null;
        }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException or
                                      OperationCanceledException or JsonException or IOException)
        {
            return null;
        }
    }

    private static string GetText(JsonElement root, string name) =>
        root.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.String
            ? v.GetString() ?? string.Empty : string.Empty;

    private static int GetNumber(JsonElement root, string name) =>
        root.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.Number && v.TryGetInt32(out var n)
            ? n : 0;

    public void Dispose() => _http.Dispose();
}
