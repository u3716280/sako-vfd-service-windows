namespace SakoVfdService.Windows.Models;

public sealed record DeviceInfo(
    string IpAddress,
    string Hardware,
    string Firmware,
    int SlaveId,
    int BaudRate,
    string NetworkMode,
    string DeviceId = "",
    string DeviceName = "SAKO Service Tool",
    string ApiVersion = "legacy")
{
    public string DisplayName => $"{DeviceName}  ·  {IpAddress}";
    public string Details => $"{Firmware}  ·  ID {DeviceId}  ·  Slave {SlaveId}  ·  {BaudRate} bps";
}

public sealed record DiscoveryProgress(int Tested, int Total, DeviceInfo? Found = null);
