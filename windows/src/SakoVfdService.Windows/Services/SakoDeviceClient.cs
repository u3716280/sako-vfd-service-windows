using System.Net.Http;
using System.Text.Json;

namespace SakoVfdService.Windows.Services;

public sealed class SakoDeviceClient : IDisposable
{
    private readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(6) };

    public async Task<JsonDocument> GetStatusAsync(string ip, CancellationToken token = default) =>
        await GetJsonAsync($"http://{ip}/api/status", token);

    public async Task<JsonDocument> GetBackupStatusAsync(string ip, CancellationToken token = default) =>
        await GetJsonAsync($"http://{ip}/api/backup/status", token);

    public async Task<JsonDocument> StartBackupAsync(string ip, CancellationToken token = default) =>
        await PostFormAsync($"http://{ip}/api/backup/start?time={Uri.EscapeDataString(DateTimeOffset.Now.ToString("O"))}", [], token);

    public async Task<JsonDocument> CancelBackupAsync(string ip, CancellationToken token = default) =>
        await PostFormAsync($"http://{ip}/api/backup/cancel", [], token);

    public async Task SaveSerialSettingsAsync(string ip, int slave, int baud, string format, CancellationToken token = default)
    {
        using var _ = await PostFormAsync($"http://{ip}/api/settings",
            new Dictionary<string,string> { ["slave"]=$"{slave}", ["baud"]=$"{baud}", ["format"]=format }, token);
    }

    public async Task SaveDeviceNameAsync(string ip, string name, CancellationToken token = default)
    {
        using var _ = await PostFormAsync($"http://{ip}/api/device",
            new Dictionary<string,string> { ["name"]=name }, token);
    }

    public async Task<byte[]> DownloadBackupAsync(string ip, CancellationToken token = default)
    {
        var bytes = await _http.GetByteArrayAsync($"http://{ip}/api/backup/download", token);
        using var json = JsonDocument.Parse(bytes, new JsonDocumentOptions { MaxDepth = 20 });
        if (json.RootElement.ValueKind != JsonValueKind.Object ||
            !json.RootElement.TryGetProperty("parameters", out var p) || p.ValueKind != JsonValueKind.Array)
            throw new InvalidDataException("Backup JSON does not contain a parameters array.");
        return bytes;
    }

    public async Task RestoreParameterAsync(string ip, string code, int rawValue, CancellationToken token = default)
    {
        using var _ = await PostFormAsync($"http://{ip}/api/restore/parameter",
            new Dictionary<string,string> { ["code"]=code, ["raw_value"]=$"{rawValue}", ["confirm"]="RESTORE_RW" }, token);
    }

    private async Task<JsonDocument> GetJsonAsync(string url, CancellationToken token)
    {
        using var response = await _http.GetAsync(url, token);
        return await ReadJsonAsync(response, token);
    }

    private async Task<JsonDocument> PostFormAsync(string url, IReadOnlyDictionary<string,string> fields, CancellationToken token)
    {
        using var body = new FormUrlEncodedContent(fields);
        using var response = await _http.PostAsync(url, body, token);
        return await ReadJsonAsync(response, token);
    }

    private static async Task<JsonDocument> ReadJsonAsync(HttpResponseMessage response, CancellationToken token)
    {
        var text = await response.Content.ReadAsStringAsync(token);
        if (!response.IsSuccessStatusCode)
        {
            string message = $"HTTP {(int)response.StatusCode}";
            try { using var e = JsonDocument.Parse(text); message = e.RootElement.GetProperty("error").GetString() ?? message; } catch { }
            throw new HttpRequestException(message, null, response.StatusCode);
        }
        return JsonDocument.Parse(text, new JsonDocumentOptions { MaxDepth = 20 });
    }

    public void Dispose() => _http.Dispose();
}
