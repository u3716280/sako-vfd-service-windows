using System.IO;
using System.Text.Json;

namespace SakoVfdService.Windows.Services;

public sealed record BackupParameter(string Code, int RawValue, string Access);
public sealed record BackupDifference(string Code, int? First, int? Second, string State);

public static class BackupFileService
{
    public static IReadOnlyDictionary<string, BackupParameter> Load(string path)
    {
        using var stream = File.OpenRead(path);
        using var json = JsonDocument.Parse(stream, new JsonDocumentOptions { MaxDepth = 20 });
        var root = json.RootElement;
        if (root.ValueKind != JsonValueKind.Object || !root.TryGetProperty("parameters", out var items) || items.ValueKind != JsonValueKind.Array)
            throw new InvalidDataException("Not a SAKO backup JSON file.");
        var result = new Dictionary<string, BackupParameter>(StringComparer.OrdinalIgnoreCase);
        foreach (var item in items.EnumerateArray())
        {
            if (!item.TryGetProperty("code", out var c) || c.ValueKind != JsonValueKind.String ||
                !item.TryGetProperty("raw_value", out var v) || !v.TryGetInt32(out var raw)) continue;
            var code = c.GetString()!;
            var access = item.TryGetProperty("access", out var a) ? a.GetString() ?? "Unknown" : "Unknown";
            result[code] = new(code, raw, access);
        }
        if (result.Count == 0) throw new InvalidDataException("Backup contains no valid parameters.");
        return result;
    }

    public static IReadOnlyList<BackupDifference> Compare(IReadOnlyDictionary<string,BackupParameter> first,
        IReadOnlyDictionary<string,BackupParameter> second)
    {
        var keys = first.Keys.Union(second.Keys, StringComparer.OrdinalIgnoreCase).OrderBy(x => x, StringComparer.OrdinalIgnoreCase);
        var rows = new List<BackupDifference>();
        foreach (var key in keys)
        {
            first.TryGetValue(key, out var a); second.TryGetValue(key, out var b);
            if (a?.RawValue == b?.RawValue && a is not null && b is not null) continue;
            rows.Add(new(key, a?.RawValue, b?.RawValue, a is null ? "Added" : b is null ? "Removed" : "Changed"));
        }
        return rows;
    }
}
