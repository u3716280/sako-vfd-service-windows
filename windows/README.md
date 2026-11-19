# SAKO VFD Service — Windows Fluent Companion v0.1

A Windows WPF .NET 10 + WPF UI (Fluent) application for locating the existing
Waveshare ESP32-S3-RS485-CAN firmware v0.1.2 on the same local network and
opening the ESP32 dashboard with Edge WebView2.

## Features

- Scans the PC's active IPv4 /24 local networks (up to 3) with bounded parallel requests.
- Validates each candidate using the *actual* firmware `/api/status` JSON, not just HTTP 200.
- Connect to discovered devices or manually enter IPv4 address.
- Opens the existing ESP32 dashboard (Monitoring, Parameters, Library, Backup) using WebView2.
- Shows **ESP32 online** separately from **VFD connected**, using `/api/monitor`.
- Remembers the most recently connected IP under `%LOCALAPPDATA%/SAKO-VFD-Service/`.
- Blocks navigating the embedded WebView to arbitrary external websites.
- **Read-only companion**: no VFD Modbus write or restore code in this Windows app.

## Build on GitHub Actions

1. Create a **new** repository named `sako-vfd-service-windows` in your GitHub account.
2. Upload this project's **contents** to its `main` branch (keep `.github/workflows/build-windows.yml`).
3. Open **Actions → Build SAKO VFD Windows Fluent App → Run workflow**, or push to `main`.
4. When it succeeds, download the `SakoVfdService-win-x64-portable` artifact.
5. Extract the ZIP fully, then run `SakoVfdService.exe` on Windows 10/11 x64.

Build uses `windows-2022`, .NET 10 SDK, and self-contained portable folder publish.
The project is not a single-file EXE because the WebView2 native loader and
managed/native runtime dependencies are distributed with the executable.
WebView2 **Evergreen Runtime** must still be installed on the PC (normally
present on Windows 11). This GitHub Actions job only builds the application;
it cannot communicate with a physical Waveshare board in your office LAN.

## Local build (Windows)

```powershell
dotnet restore .\src\SakoVfdService.Windows\SakoVfdService.Windows.csproj
dotnet run --project .\src\SakoVfdService.Windows\SakoVfdService.Windows.csproj
```

## Firmware compatibility

Targets `SAKO-SKI780-ESP32-v0.1.2-LAN` and uses:

- `GET /api/status` (requires `hardware` = `Waveshare ESP32-S3-RS485-CAN`, `firmware` contains `SAKO`)
- `GET /api/monitor` (`online` tells whether the VFD RS-485 link has fresh data)
- `GET /` (existing dashboard), plus its own API calls and downloads

No ESP32 firmware patch is required for this IP scan method. mDNS is a future
optimization, not a prerequisite. Windows and ESP32 must be on reachable LANs.
Wi-Fi client isolation, firewalls, and VLAN boundaries may block discovery.

## Security/limitations

- Scanning is limited to the local /24 of up to three up network adapters
  (up to 768 probes), not arbitrary remote ranges. Larger /23 networks may
  need manual IP entry if the ESP32 is in the other half.
- Discovery and embedded dashboard use plain HTTP, consistent with ESP32 v0.1.2;
  **do not expose this service to the public internet**. Only use trusted LANs.
- No app-side authentication is introduced here; firmware v0.1.2 has no app login.
- Device verification is for discovery, **not cryptographic authentication**.
- JSON backup and Library screens are served by the ESP32; the Windows app does
  not duplicate or modify the parameter definitions.
- Auto-detection cannot find devices behind client isolation or firewalls;
  use manual IP as fallback.
- Discovery over many hosts may take several seconds depending on LAN response.

## Validated

Source content verified against firmware WebApp.cpp, XAML parsed, and CI YAML
structure checked locally. **A real Windows .NET build and GUI test have not
been executed in this environment**. GitHub Actions performs the definitive build.
