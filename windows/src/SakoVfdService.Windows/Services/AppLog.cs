using System.Collections.ObjectModel;
using System.IO;

namespace SakoVfdService.Windows.Services;

public static class AppLog
{
    public static ObservableCollection<string> Entries { get; } = [];
    public static void Add(string message)
    {
        var line = $"{DateTimeOffset.Now:yyyy-MM-dd HH:mm:ss zzz}  {message}";
        App.Current.Dispatcher.Invoke(() => { Entries.Insert(0, line); while (Entries.Count > 500) Entries.RemoveAt(Entries.Count - 1); });
    }
    public static void Export(string path) => File.WriteAllLines(path, Entries.Reverse());
}
