using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;
using Microsoft.Win32;
using Microsoft.Web.WebView2.Core;
using SakoVfdService.Windows.Models;
using SakoVfdService.Windows.Services;
using Wpf.Ui.Controls;
using MessageBox = System.Windows.MessageBox;
using MessageBoxButton = System.Windows.MessageBoxButton;
using MessageBoxImage = System.Windows.MessageBoxImage;
using MessageBoxResult = System.Windows.MessageBoxResult;

namespace SakoVfdService.Windows;

public partial class MainWindow : FluentWindow
{
    private readonly LanDiscoveryService _discovery = new();
    private readonly SakoDeviceClient _device = new();
    private readonly DispatcherTimer _healthTimer = new() { Interval = TimeSpan.FromSeconds(3) };
    private CancellationTokenSource? _scanCancellation, _backupCancellation, _restoreCancellation;
    private bool _healthInProgress, _connecting, _dark;
    private string _language = "th";
    private string? _connectedIp;
    private DeviceInfo? _connectedDevice;
    private IReadOnlyDictionary<string, BackupParameter>? _restoreItems;
    private AppPreferences.Settings _preferences = new();

    public MainWindow()
    {
        InitializeComponent();
        _preferences = AppPreferences.Load();
        _language = _preferences.Language is "en" ? "en" : "th";
        _dark = _preferences.Theme == "dark";
        ManualIp.Text = _preferences.LastIp ?? string.Empty;
        LanguageBox.SelectedIndex = _language == "th" ? 0 : 1;
        BaudBox.SelectedIndex = 0; FormatBox.SelectedIndex = 0;
        ApplyTheme(); ApplyLanguage();
        LogList.ItemsSource = AppLog.Entries;
        AppLog.Add("Windows application started; waiting for user discovery.");
        _healthTimer.Tick += async (_, _) => await UpdateHealthAsync();
        DeviceBrowser.NavigationStarting += DeviceBrowser_NavigationStarting;
        Closing += Window_Closing;
    }

    private bool Thai => _language == "th";
    private static string ComboText(ComboBox box) => (box.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "";

    private async void Scan_Click(object sender, RoutedEventArgs e)
    {
        _scanCancellation?.Cancel(); _scanCancellation?.Dispose();
        _scanCancellation = new(); var token = _scanCancellation.Token;
        DeviceList.Items.Clear(); ScanProgress.Value = 0; ScanButton.IsEnabled = false; CancelButton.IsEnabled = true;
        ScanStatus.Text = Thai ? "กำลังค้นหาใน LAN…" : "Scanning LAN…"; AppLog.Add("LAN discovery started.");
        try
        {
            var progress = new Progress<DiscoveryProgress>(p =>
            {
                ScanProgress.Value = p.Total == 0 ? 0 : 100d * p.Tested / p.Total;
                ScanStatus.Text = Thai ? $"ตรวจแล้ว {p.Tested}/{p.Total} · พบ {DeviceList.Items.Count}" : $"Probed {p.Tested}/{p.Total} · {DeviceList.Items.Count} found";
                if (p.Found is not null && !DeviceList.Items.Cast<DeviceInfo>().Any(d => d.IpAddress == p.Found.IpAddress))
                { DeviceList.Items.Add(p.Found); AppLog.Add($"Discovered {p.Found.DisplayName} ({p.Found.ApiVersion})."); }
            });
            var results = await _discovery.ScanAsync(progress, token);
            ScanProgress.Value = 100; ScanStatus.Text = Thai ? $"ค้นหาเสร็จ พบ {results.Count} อุปกรณ์ — กรุณาเลือกอุปกรณ์" : $"Scan complete: {results.Count} found — select a device";
            AppLog.Add($"LAN discovery completed with {results.Count} device(s).");
        }
        catch (OperationCanceledException) { ScanStatus.Text = Thai ? "ยกเลิกการค้นหาแล้ว" : "Scan cancelled"; AppLog.Add("LAN discovery cancelled."); }
        catch (Exception ex) { ScanStatus.Text = ex.Message; AppLog.Add("Discovery error: " + ex.Message); }
        finally { ScanButton.IsEnabled = true; CancelButton.IsEnabled = false; }
    }

    private void Cancel_Click(object sender, RoutedEventArgs e) => _scanCancellation?.Cancel();
    private async void DeviceList_SelectionChanged(object sender, SelectionChangedEventArgs e) { if (DeviceList.SelectedItem is DeviceInfo d) await ConnectAsync(d.IpAddress); }
    private async void ManualConnect_Click(object sender, RoutedEventArgs e)
    {
        var ip = ManualIp.Text.Trim();
        if (!IPAddress.TryParse(ip, out var a) || a.AddressFamily != AddressFamily.InterNetwork)
        { ScanStatus.Text = Thai ? "กรุณากรอก IPv4 ที่ถูกต้อง" : "Enter a valid IPv4 address."; return; }
        await ConnectAsync(ip);
    }

    private async Task ConnectAsync(string ip)
    {
        if (_connecting) return; _connecting = true;
        try
        {
            EspState.Text = $"ESP32: {ip}…";
            var info = await _discovery.ProbeAsync(ip);
            if (info is null) { EspState.Text = "ESP32: Not found"; ScanStatus.Text = Thai ? "ไม่พบ SAKO Firmware ที่ IP นี้" : "No compatible SAKO firmware at this IP."; return; }
            DeviceBrowser.Visibility = Visibility.Visible; EmptyView.Visibility = Visibility.Collapsed;
            await DeviceBrowser.EnsureCoreWebView2Async();
            DeviceBrowser.CoreWebView2.Settings.AreDevToolsEnabled = false;
            _connectedIp = ip; _connectedDevice = info;
            DeviceBrowser.Source = new Uri($"http://{ip}/");
            AddressText.Text = $"{info.DeviceName} · http://{ip}/ · FW {info.Firmware}";
            EspState.Text = $"ESP32: Online ({ip})"; ScanStatus.Text = Thai ? $"เชื่อมต่อ {info.DeviceName} แล้ว" : $"Connected to {info.DeviceName}";
            ManualIp.Text = ip; _preferences = _preferences with { LastIp = ip }; SavePreferences();
            DeviceNameBox.Text = info.DeviceName; SlaveBox.Text = info.SlaveId.ToString(); SelectCombo(BaudBox, info.BaudRate.ToString());
            _healthTimer.Start(); AppLog.Add($"Connected to {info.DisplayName}, firmware {info.Firmware}, API {info.ApiVersion}.");
            await LoadNativeSettingsAsync(); await UpdateHealthAsync();
        }
        catch (WebView2RuntimeNotFoundException) { EspState.Text = "WebView2 missing"; ScanStatus.Text = Thai ? "ต้องติดตั้ง Microsoft Edge WebView2 Evergreen Runtime" : "Install Microsoft Edge WebView2 Evergreen Runtime."; }
        catch (Exception ex) { EspState.Text = "ESP32: Connection failed"; ScanStatus.Text = ex.Message; AppLog.Add("Connection error: " + ex.Message); }
        finally { _connecting = false; }
    }

    private async Task LoadNativeSettingsAsync()
    {
        if (_connectedIp is null) return;
        try
        {
            using var status = await _device.GetStatusAsync(_connectedIp); var root = status.RootElement;
            if (root.TryGetProperty("device_name", out var n)) DeviceNameBox.Text = n.GetString() ?? DeviceNameBox.Text;
            if (root.TryGetProperty("slave", out var s)) SlaveBox.Text = s.GetInt32().ToString();
            if (root.TryGetProperty("baud", out var b)) SelectCombo(BaudBox, b.GetInt32().ToString());
            if (root.TryGetProperty("serial_format", out var f)) SelectCombo(FormatBox, f.GetString() ?? "8N2");
        }
        catch (Exception ex) { AppLog.Add("Settings read failed: " + ex.Message); }
    }

    private static void SelectCombo(ComboBox box, string value)
    { foreach (var item in box.Items.OfType<ComboBoxItem>()) if (item.Content?.ToString() == value) { box.SelectedItem = item; return; } }

    private async Task UpdateHealthAsync()
    {
        if (_healthInProgress || _connectedIp is null) return; _healthInProgress = true;
        try
        {
            var info = await _discovery.ProbeAsync(_connectedIp);
            if (info is null) { EspState.Text = "ESP32: Offline"; VfdState.Text = "VFD: Unknown"; return; }
            EspState.Text = $"ESP32: Online ({_connectedIp})";
            VfdState.Text = await _discovery.CheckVfdConnectedAsync(_connectedIp) switch { true => "VFD: Connected", false => "VFD: Disconnected", _ => "VFD: Unknown" };
        }
        finally { _healthInProgress = false; }
    }

    private async void BackupStart_Click(object sender, RoutedEventArgs e)
    {
        if (_connectedIp is null) { NeedConnection(); return; }
        _backupCancellation?.Cancel(); _backupCancellation = new(); BackupStartButton.IsEnabled = false; BackupCancelButton.IsEnabled = true; BackupDownloadButton.IsEnabled = false;
        try
        {
            using var _ = await _device.StartBackupAsync(_connectedIp, _backupCancellation.Token); AppLog.Add("Device backup started.");
            while (true)
            {
                await Task.Delay(700, _backupCancellation.Token); using var status = await _device.GetBackupStatusAsync(_connectedIp, _backupCancellation.Token); var r = status.RootElement;
                int total = GetInt(r,"total"), done = GetInt(r,"processed"); BackupProgress.Maximum = Math.Max(1,total); BackupProgress.Value = done;
                BackupStatusText.Text = $"{GetText(r,"stage")} · {done}/{total} · OK {GetInt(r,"success")} · Failed {GetInt(r,"failed")}";
                if (!GetBool(r,"active")) { BackupDownloadButton.IsEnabled = GetBool(r,"ready"); break; }
            }
            AppLog.Add("Device backup completed.");
        }
        catch (OperationCanceledException) { BackupStatusText.Text = Thai ? "ยกเลิกแล้ว" : "Cancelled"; }
        catch (Exception ex) { BackupStatusText.Text = ex.Message; AppLog.Add("Backup error: " + ex.Message); }
        finally { BackupStartButton.IsEnabled = true; BackupCancelButton.IsEnabled = false; }
    }

    private async void BackupCancel_Click(object sender, RoutedEventArgs e)
    { _backupCancellation?.Cancel(); if (_connectedIp is not null) try { using var _ = await _device.CancelBackupAsync(_connectedIp); } catch { } }

    private async void BackupDownload_Click(object sender, RoutedEventArgs e)
    {
        if (_connectedIp is null) return;
        var dialog = new SaveFileDialog { Filter="JSON (*.json)|*.json", FileName=$"SAKO_SKI780_{DateTime.Now:yyyyMMdd_HHmmss}.json" };
        if (dialog.ShowDialog()!=true) return;
        try { var bytes=await _device.DownloadBackupAsync(_connectedIp); await File.WriteAllBytesAsync(dialog.FileName,bytes); BackupStatusText.Text=Thai?"บันทึกและตรวจสอบ JSON แล้ว":"JSON saved and validated"; AppLog.Add("Backup saved: "+dialog.FileName); }
        catch(Exception ex){BackupStatusText.Text=ex.Message;AppLog.Add("Backup save error: "+ex.Message);}
    }

    private void Compare_Click(object sender, RoutedEventArgs e)
    {
        var d=new OpenFileDialog{Filter="JSON (*.json)|*.json",Multiselect=true,Title=Thai?"เลือก Backup 2 ไฟล์":"Select two backups"};
        if(d.ShowDialog()!=true)return;if(d.FileNames.Length!=2){CompareSummary.Text=Thai?"ต้องเลือก 2 ไฟล์":"Select exactly two files";return;}
        try{var rows=BackupFileService.Compare(BackupFileService.Load(d.FileNames[0]),BackupFileService.Load(d.FileNames[1]));CompareGrid.ItemsSource=rows;CompareSummary.Text=Thai?$"พบ {rows.Count} รายการที่ต่างกัน":$"{rows.Count} differences";AppLog.Add($"Compared backups: {rows.Count} differences.");}catch(Exception ex){CompareSummary.Text=ex.Message;}
    }

    private void RestoreLoad_Click(object sender,RoutedEventArgs e)
    {
        var d=new OpenFileDialog{Filter="JSON (*.json)|*.json"};if(d.ShowDialog()!=true)return;
        try{var all=BackupFileService.Load(d.FileName);_restoreItems=all.Where(x=>x.Value.Access.Equals("RW",StringComparison.OrdinalIgnoreCase)).ToDictionary(x=>x.Key,x=>x.Value,StringComparer.OrdinalIgnoreCase);RestoreRunButton.IsEnabled=_restoreItems.Count>0;RestoreStatusText.Text=Thai?$"ตรวจสอบแล้ว: RW {_restoreItems.Count} จากทั้งหมด {all.Count}":$"Validated: {_restoreItems.Count} RW of {all.Count}";AppLog.Add($"Restore file validated: {_restoreItems.Count} trusted RW candidates.");}catch(Exception ex){_restoreItems=null;RestoreRunButton.IsEnabled=false;RestoreStatusText.Text=ex.Message;}
    }

    private async void RestoreRun_Click(object sender,RoutedEventArgs e)
    {
        if(_connectedIp is null){NeedConnection();return;}if(_restoreItems is null||_restoreItems.Count==0)return;
        var warning=Thai?"ยืนยันว่า VFD หยุดสนิทแล้ว และต้องการ Restore เฉพาะค่า RW? ระบบจะตรวจสอบ Read-back ทุกค่า":"Confirm the VFD is stopped and restore RW values only? Every value will be read back.";
        if(MessageBox.Show(warning,"SAKO Safe Restore",MessageBoxButton.YesNo,MessageBoxImage.Warning)!=MessageBoxResult.Yes)return;
        _restoreCancellation=new();RestoreRunButton.IsEnabled=false;RestoreCancelButton.IsEnabled=true;RestoreProgress.Maximum=_restoreItems.Count;RestoreProgress.Value=0;var ok=0;
        try{foreach(var item in _restoreItems.Values){_restoreCancellation.Token.ThrowIfCancellationRequested();RestoreStatusText.Text=$"{item.Code} · {ok}/{_restoreItems.Count}";await _device.RestoreParameterAsync(_connectedIp,item.Code,item.RawValue,_restoreCancellation.Token);ok++;RestoreProgress.Value=ok;}RestoreStatusText.Text=Thai?$"Restore สำเร็จ {ok} ค่า และตรวจสอบ Read-back แล้ว":$"Restored and verified {ok} values";AppLog.Add($"Restore completed: {ok} RW parameters.");}
        catch(OperationCanceledException){RestoreStatusText.Text=Thai?$"ยกเลิกหลังสำเร็จ {ok} ค่า":$"Cancelled after {ok} values";}
        catch(Exception ex){RestoreStatusText.Text=$"{(Thai?"หยุด Restore":"Restore stopped")}: {ex.Message}";AppLog.Add("Restore stopped: "+ex.Message);}
        finally{RestoreRunButton.IsEnabled=true;RestoreCancelButton.IsEnabled=false;}
    }

    private void RestoreCancel_Click(object sender,RoutedEventArgs e)=>_restoreCancellation?.Cancel();

    private async void SaveSettings_Click(object sender,RoutedEventArgs e)
    {
        if(_connectedIp is null){NeedConnection();return;}if(!int.TryParse(SlaveBox.Text,out var slave)||slave<1||slave>247){SettingsResult.Text="Slave ID: 1–247";return;}
        if(!int.TryParse(ComboText(BaudBox),out var baud)){SettingsResult.Text="Invalid baud";return;}
        try{await _device.SaveSerialSettingsAsync(_connectedIp,slave,baud,ComboText(FormatBox));
            var name=DeviceNameBox.Text.Trim();if(name.Length>0&&!name.Equals(_connectedDevice?.DeviceName,StringComparison.Ordinal))
                try{await _device.SaveDeviceNameAsync(_connectedIp,name);}catch(HttpRequestException ex) when(ex.StatusCode==HttpStatusCode.NotFound){SettingsResult.Text=Thai?"บันทึก Serial แล้ว; Firmware ปัจจุบันยังไม่รองรับชื่ออุปกรณ์":"Serial saved; current firmware does not support device names";return;}
            SettingsResult.Text=Thai?"บันทึกค่าแล้ว (ไม่มีการแก้ Parameter VFD)":"Settings saved (no VFD parameter changed)";AppLog.Add("Native device settings saved.");}
        catch(Exception ex){SettingsResult.Text=ex.Message;AppLog.Add("Settings error: "+ex.Message);}
    }

    private void ExportLog_Click(object sender,RoutedEventArgs e){var d=new SaveFileDialog{Filter="Text (*.txt)|*.txt",FileName=$"SAKO_Diagnostics_{DateTime.Now:yyyyMMdd_HHmmss}.txt"};if(d.ShowDialog()==true)try{AppLog.Export(d.FileName);}catch(Exception ex){MessageBox.Show(ex.Message);}}

    private void Language_Changed(object sender,SelectionChangedEventArgs e){if(!IsLoaded&&LanguageBox.SelectedItem is null)return;if(LanguageBox.SelectedItem is ComboBoxItem i)_language=i.Tag?.ToString()=="en"?"en":"th";ApplyLanguage();_preferences=_preferences with{Language=_language};SavePreferences();}
    private void ApplyLanguage()
    {
        if(DiscoverTitle is null)return;
        SubtitleText.Text=Thai?"ศูนย์บริการ Native Windows สำหรับ SAKO SKI780":"Native Windows service console for SAKO SKI780";DiscoverTitle.Text=Thai?"ค้นหาอุปกรณ์":"Discover devices";DiscoverHelp.Text=Thai?"กดค้นหา แล้วเลือกอุปกรณ์ที่ต้องการเชื่อมต่อ":"Run discovery, then select the device to connect";ScanButton.Content=Thai?"ค้นหาใน LAN":"Discover LAN";CancelButton.Content=Thai?"ยกเลิก":"Cancel";FoundTitle.Text=Thai?"อุปกรณ์ที่พบ":"Discovered devices";ManualTitle.Text=Thai?"เชื่อมต่อด้วย IP":"Manual IP";ManualButton.Content=Thai?"ตรวจสอบ IP":"Check IP";PrivacyText.Text=Thai?"เชื่อมต่อภายใน LAN เท่านั้น ไม่มีการส่งข้อมูลขึ้น Cloud":"Local LAN only; no data is sent to a cloud service";RefreshButton.Content=Thai?"รีเฟรช":"Refresh";BrowserButton.Content=Thai?"เปิดใน Browser":"Open in browser";EmptyTitle.Text=Thai?"รอเลือก SAKO Service Tool":"Select a SAKO Service Tool";EmptyHelp.Text=Thai?"กดค้นหา จากนั้นเลือกอุปกรณ์ที่ต้องการติดต่อ โปรแกรมจะไม่เชื่อมต่ออัตโนมัติ":"Run discovery and select a device. The app never auto-connects.";
        BackupTitle.Text=Thai?"สำรองข้อมูล VFD":"VFD backup";BackupHelp.Text=Thai?"อ่านค่า RO/RW จาก VFD และบันทึกเป็น JSON ลงในคอมพิวเตอร์":"Read RO/RW values and save validated JSON on this PC";BackupStartButton.Content=Thai?"เริ่ม Backup":"Start backup";BackupCancelButton.Content=Thai?"ยกเลิก":"Cancel";BackupDownloadButton.Content=Thai?"บันทึก JSON...":"Save JSON...";CompareTitle.Text=Thai?"เปรียบเทียบ Backup":"Compare backups";CompareHelp.Text=Thai?"เลือกไฟล์ JSON สองไฟล์ เพื่อแสดงเฉพาะค่าที่ต่างกัน":"Select two JSON files and show only differences";CompareButton.Content=Thai?"เลือก 2 ไฟล์...":"Select 2 files...";RestoreTitle.Text=Thai?"Restore เฉพาะ RW":"RW-only restore";RestoreHelp.Text=Thai?"ตรวจสอบไฟล์และส่งเฉพาะรายการ RW; ต้องยืนยัน VFD หยุด และ Firmware รองรับ Safe Restore":"Only validated RW entries are sent; VFD stop confirmation and Safe Restore firmware are required";RestoreLoadButton.Content=Thai?"เลือก Backup...":"Select backup...";RestoreRunButton.Content=Thai?"เริ่ม Restore":"Start restore";RestoreCancelButton.Content=Thai?"ยกเลิก":"Cancel";
        SettingsTitle.Text=Thai?"ตั้งค่าอุปกรณ์":"Device settings";SettingsHelp.Text=Thai?"เปลี่ยนชื่ออุปกรณ์และพอร์ต Modbus ของ ESP32 (ไม่แก้ Parameter ใน VFD)":"Change the device name and ESP32 Modbus port (does not change VFD parameters)";DeviceNameLabel.Text=Thai?"ชื่ออุปกรณ์":"Device name";SaveSettingsButton.Content=Thai?"บันทึกค่า":"Save";DiagnosticsTitle.Text=Thai?"บันทึกการทำงาน":"Diagnostics log";ExportLogButton.Content=Thai?"ส่งออก Log...":"Export log...";ThemeButton.Content=_dark?(Thai?"สว่าง":"Light"):(Thai?"มืด":"Dark");
    }

    private void Theme_Click(object sender,RoutedEventArgs e){_dark=!_dark;ApplyTheme();ApplyLanguage();_preferences=_preferences with{Theme=_dark?"dark":"light"};SavePreferences();}
    private void ApplyTheme(){SetBrush("AppBackground",_dark?"#151A21":"#F4F7FA");SetBrush("PanelBackground",_dark?"#202833":"#FFFFFF");SetBrush("PanelBorder",_dark?"#394655":"#DDE5ED");SetBrush("PrimaryText",_dark?"#EDF3F8":"#1C3655");SetBrush("SecondaryText",_dark?"#AEBCC9":"#637689");SetBrush("SoftBackground",_dark?"#2A3542":"#EEF4FA");}
    private void SetBrush(string key,string color)=>Resources[key]=new SolidColorBrush((Color)ColorConverter.ConvertFromString(color));
    private void SavePreferences(){try{AppPreferences.Save(_preferences);}catch{}}
    private void NeedConnection()=>MessageBox.Show(Thai?"กรุณาค้นหาและเลือกอุปกรณ์ก่อน":"Discover and select a device first.","SAKO VFD Service");
    private static int GetInt(JsonElement r,string n)=>r.TryGetProperty(n,out var v)&&v.TryGetInt32(out var i)?i:0;
    private static bool GetBool(JsonElement r,string n)=>r.TryGetProperty(n,out var v)&&v.ValueKind==JsonValueKind.True;
    private static string GetText(JsonElement r,string n)=>r.TryGetProperty(n,out var v)?v.ToString():"";

    private void DeviceBrowser_NavigationStarting(object? sender,CoreWebView2NavigationStartingEventArgs e){if(string.IsNullOrWhiteSpace(_connectedIp))return;if(e.Uri.StartsWith($"blob:http://{_connectedIp}/",StringComparison.OrdinalIgnoreCase))return;if(!Uri.TryCreate(e.Uri,UriKind.Absolute,out var u)||u.Scheme!=Uri.UriSchemeHttp||u.Host!=_connectedIp||u.Port!=80){e.Cancel=true;AppLog.Add("Blocked external embedded navigation: "+e.Uri);}}
    private void Refresh_Click(object sender,RoutedEventArgs e){if(DeviceBrowser.CoreWebView2 is not null)DeviceBrowser.Reload();}
    private void OpenBrowser_Click(object sender,RoutedEventArgs e){if(_connectedIp is not null)Process.Start(new ProcessStartInfo($"http://{_connectedIp}/"){UseShellExecute=true});}
    private void Window_Closing(object? sender,CancelEventArgs e){_healthTimer.Stop();_scanCancellation?.Cancel();_backupCancellation?.Cancel();_restoreCancellation?.Cancel();_discovery.Dispose();_device.Dispose();DeviceBrowser.Dispose();}
}
