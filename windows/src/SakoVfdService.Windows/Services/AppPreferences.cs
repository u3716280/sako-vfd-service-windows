using System.IO;
using System.Text.Json;

namespace SakoVfdService.Windows.Services;

public static class AppPreferences
{
    private static readonly string FilePath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SAKO-VFD-Service", "preferences.json");

    public sealed record Settings(string? LastIp = null, string Language = "th", string Theme = "light");

    public static Settings Load()
    {
        try
        {
            if (!File.Exists(FilePath)) return new();
            return JsonSerializer.Deserialize<Settings>(File.ReadAllText(FilePath),
                new JsonSerializerOptions { PropertyNameCaseInsensitive = true }) ?? new();
        }
        catch (Exception) { /* Missing/corrupt settings must not block startup. */ }
        return new();
    }

    public static void Save(Settings settings)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(settings, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (IOException) { /* App still usable without persistence. */ }
        catch (UnauthorizedAccessException) { }
    }
}
