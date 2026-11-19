using System.IO;
using System.Text.Json;

namespace SakoVfdService.Windows.Services;

public static class AppPreferences
{
    private static readonly string FilePath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SAKO-VFD-Service", "preferences.json");

    public static string? LoadLastIp()
    {
        try
        {
            if (!File.Exists(FilePath)) return null;
            using var json = JsonDocument.Parse(File.ReadAllText(FilePath));
            if (json.RootElement.TryGetProperty("last_ip", out var last))
                return last.GetString();
        }
        catch (Exception) { /* Missing/corrupt settings must not block startup. */ }
        return null;
    }

    public static void SaveLastIp(string ip)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(new { last_ip = ip }));
        }
        catch (IOException) { /* App still usable without persistence. */ }
        catch (UnauthorizedAccessException) { }
    }
}
