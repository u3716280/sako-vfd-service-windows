using System.ComponentModel;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using Microsoft.Web.WebView2.Core;
using SakoVfdService.Windows.Models;
using SakoVfdService.Windows.Services;
using Wpf.Ui.Controls;

namespace SakoVfdService.Windows;

public partial class MainWindow : FluentWindow
{
    private readonly LanDiscoveryService _discovery = new();
    private readonly DispatcherTimer _healthTimer;
    private CancellationTokenSource? _scanCancellation;
    private bool _healthInProgress;
    private bool _connecting;
    private string? _connectedIp;

    public MainWindow()
    {
        InitializeComponent();
        ManualIp.Text = AppPreferences.LoadLastIp() ?? string.Empty;
        _healthTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(3) };
        _healthTimer.Tick += HealthTimer_Tick;
        DeviceBrowser.NavigationStarting += DeviceBrowser_NavigationStarting;
        Closing += Window_Closing;
    }

    private async void Scan_Click(object sender, RoutedEventArgs e)
    {
        _scanCancellation?.Cancel();
        _scanCancellation?.Dispose();
        _scanCancellation = new CancellationTokenSource();
        var token = _scanCancellation.Token;
        DeviceList.Items.Clear();
        ScanProgress.Value = 0;
        ScanButton.IsEnabled = false;
        CancelButton.IsEnabled = true;
        ScanStatus.Text = "Scanning LAN…";

        try
        {
            var progress = new Progress<DiscoveryProgress>(p =>
            {
                ScanProgress.Value = p.Total == 0 ? 0 : 100.0 * p.Tested / p.Total;
                ScanStatus.Text = $"Probed {p.Tested}/{p.Total} hosts  ·  {DeviceList.Items.Count} found";
                if (p.Found is not null &&
                    !DeviceList.Items.Cast<DeviceInfo>().Any(d => d.IpAddress == p.Found.IpAddress))
                    DeviceList.Items.Add(p.Found);
            });
            var results = await _discovery.ScanAsync(progress, token);
            ScanProgress.Value = 100;
            ScanStatus.Text = $"Scan complete — {results.Count} device(s) found";
        }
        catch (OperationCanceledException)
        {
            ScanStatus.Text = "Scan cancelled";
        }
        catch (Exception ex)
        {
            ScanStatus.Text = $"Scan error: {ex.Message}";
        }
        finally
        {
            ScanButton.IsEnabled = true;
            CancelButton.IsEnabled = false;
        }
    }

    private void Cancel_Click(object sender, RoutedEventArgs e) => _scanCancellation?.Cancel();

    private async void DeviceList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (DeviceList.SelectedItem is DeviceInfo device)
            await ConnectAsync(device.IpAddress);
    }

    private async void ManualConnect_Click(object sender, RoutedEventArgs e)
    {
        var ip = ManualIp.Text.Trim();
        if (!IPAddress.TryParse(ip, out var address) || address.AddressFamily != AddressFamily.InterNetwork)
        {
            ScanStatus.Text = "Please enter a valid IPv4 address (e.g. 192.168.1.120).";
            return;
        }
        await ConnectAsync(ip);
    }

    private async Task ConnectAsync(string ip)
    {
        if (_connecting) return;
        _connecting = true;
        try
        {
            EspState.Text = $"ESP32: Checking {ip}…";
            var device = await _discovery.ProbeAsync(ip);
            if (device is null)
            {
                EspState.Text = "ESP32: Not found";
                ScanStatus.Text = $"{ip} is unreachable or does not match SAKO firmware /api/status.";
                return;
            }

            // Ensure WebView2 is initialized after it becomes visible.
            DeviceBrowser.Visibility = Visibility.Visible;
            EmptyView.Visibility = Visibility.Collapsed;
            await DeviceBrowser.EnsureCoreWebView2Async();
            DeviceBrowser.CoreWebView2.Settings.AreDevToolsEnabled = false;
            DeviceBrowser.CoreWebView2.Settings.AreDefaultContextMenusEnabled = true;
            // Restrict embedded navigation to the verified device. Downloads
            // initiated by its web app remain handled by WebView2 runtime.
            _connectedIp = ip;
            DeviceBrowser.Source = new Uri($"http://{ip}/");
            AddressText.Text = $"http://{ip}/  ·  {device.Firmware}";
            EspState.Text = $"ESP32: Online ({ip})";
            ScanStatus.Text = $"Connected to {device.Hardware}";
            ManualIp.Text = ip;
            AppPreferences.SaveLastIp(ip);
            _healthTimer.Start();
            await UpdateHealthAsync();
        }
        catch (WebView2RuntimeNotFoundException)
        {
            EspState.Text = "WebView2 runtime missing";
            ScanStatus.Text = "Install Microsoft Edge WebView2 Runtime to embed the ESP32 dashboard.";
        }
        catch (Exception ex)
        {
            EspState.Text = "ESP32: Connection failed";
            ScanStatus.Text = $"Connection error: {ex.Message}";
        }
        finally { _connecting = false; }
    }

    private void DeviceBrowser_NavigationStarting(object? sender, CoreWebView2NavigationStartingEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(_connectedIp)) return;
        if (e.Uri.StartsWith($"blob:http://{_connectedIp}/", StringComparison.OrdinalIgnoreCase)) return;
        if (!Uri.TryCreate(e.Uri, UriKind.Absolute, out var destination) ||
            destination.Scheme != Uri.UriSchemeHttp || destination.Host != _connectedIp || destination.Port != 80)
        {
            e.Cancel = true;
            ScanStatus.Text = "External navigation blocked in device view.";
        }
    }

    private async void HealthTimer_Tick(object? sender, EventArgs e) => await UpdateHealthAsync();

    private async Task UpdateHealthAsync()
    {
        if (_healthInProgress || _connectedIp is null) return;
        _healthInProgress = true;
        try
        {
            var ip = _connectedIp;
            var device = await _discovery.ProbeAsync(ip);
            if (device is null)
            {
                EspState.Text = "ESP32: Offline";
                VfdState.Text = "VFD: Unknown";
                return;
            }
            EspState.Text = $"ESP32: Online ({ip})";
            var connected = await _discovery.CheckVfdConnectedAsync(ip);
            VfdState.Text = connected switch
            {
                true => "VFD: Connected",
                false => "VFD: Disconnected",
                _ => "VFD: Unknown"
            };
        }
        finally { _healthInProgress = false; }
    }

    private void Refresh_Click(object sender, RoutedEventArgs e)
    {
        if (DeviceBrowser.CoreWebView2 is not null)
            DeviceBrowser.Reload();
    }

    private void OpenBrowser_Click(object sender, RoutedEventArgs e)
    {
        if (_connectedIp is null) return;
        Process.Start(new ProcessStartInfo($"http://{_connectedIp}/") { UseShellExecute = true });
    }

    private void Window_Closing(object? sender, CancelEventArgs e)
    {
        _healthTimer.Stop();
        _scanCancellation?.Cancel();
        _discovery.Dispose();
        DeviceBrowser.Dispose();
    }
}
