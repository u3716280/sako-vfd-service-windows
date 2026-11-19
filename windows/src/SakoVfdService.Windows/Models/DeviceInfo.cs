namespace SakoVfdService.Windows.Models;

public sealed record DeviceInfo(
    string IpAddress,
    string Hardware,
    string Firmware,
    int SlaveId,
    int BaudRate,
    string NetworkMode)
{
    public string DisplayName => $"SAKO Service Tool  ·  {IpAddress}";
    public string Details => $"{Firmware}  ·  Slave {SlaveId}  ·  {BaudRate} bps";
}

public sealed record DiscoveryProgress(int Tested, int Total, DeviceInfo? Found = null);
