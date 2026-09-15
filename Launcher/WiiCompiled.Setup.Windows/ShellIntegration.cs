using Microsoft.Win32;
using WiiCompiled.Setup.Common;

namespace WiiCompiled.Setup.Windows;

internal static class ShellIntegration
{
    private const string ShortcutFileName = "Mario Kart Wii VR Launcher.lnk";
    private const string LegacyShortcutFileName = "wiicompiled (base) (beta).lnk";

    public static void RegisterUninstaller(string installDirectory, bool retroInstalled)
    {
        using var key = Registry.CurrentUser.CreateSubKey(ProductInfo.UninstallKey, writable: true)
                        ?? throw new InvalidOperationException("Could not register the uninstaller.");
        var cli = Path.Combine(installDirectory, ProductInfo.SetupCopyName);
        key.SetValue("DisplayName", ProductInfo.Name);
        key.SetValue("DisplayVersion", ProductInfo.Version);
        key.SetValue("Publisher", "WiiCompiled");
        key.SetValue("InstallLocation", installDirectory);
        key.SetValue("DisplayIcon", cli);
        key.SetValue("UninstallString", $"\"{cli}\" --uninstall --install-dir \"{installDirectory}\"");
        key.SetValue("QuietUninstallString", $"\"{cli}\" --silent-uninstall --install-dir \"{installDirectory}\"");
        key.SetValue("NoModify", 1, RegistryValueKind.DWord);
        key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
        key.SetValue("EstimatedSize", EstimateSizeKb(installDirectory), RegistryValueKind.DWord);
        key.SetValue("Comments", retroInstalled ? "Includes the Retro Rewind profile" : "WiiCompiled");
    }

    public static void UnregisterUninstaller() =>
        Registry.CurrentUser.DeleteSubKeyTree(ProductInfo.UninstallKey, throwOnMissingSubKey: false);

    /// <summary>Creates desktop and Start Menu shortcuts for the integrated launcher.</summary>
    public static void CreateShortcuts(string installDirectory)
    {
        var portableLauncher = PortableRoot.TryFind(installDirectory) is { } portableRoot
            ? Path.Combine(portableRoot, "WheelWizard", "WheelWizard.exe")
            : null;
        var launcher = portableLauncher is not null && File.Exists(portableLauncher)
            ? portableLauncher
            : Path.Combine(installDirectory, "WheelWizard", "WheelWizard.exe");
        if (!File.Exists(launcher))
            throw new FileNotFoundException("The integrated WheelWizard launcher is missing.", launcher);
        var shellType = Type.GetTypeFromProgID("WScript.Shell")
                        ?? throw new InvalidOperationException("The Windows Script Host shell is unavailable.");
        dynamic shell = Activator.CreateInstance(shellType)!;
        foreach (var path in ShortcutPaths())
        {
            dynamic shortcut = shell.CreateShortcut(path);
            shortcut.TargetPath = launcher;
            shortcut.Arguments = "";
            shortcut.WorkingDirectory = Path.GetDirectoryName(launcher);
            shortcut.IconLocation = launcher + ",0";
            shortcut.Description = "Launch Mario Kart Wii VR and Retro Rewind VR";
            shortcut.Save();
        }
        foreach (var legacy in LegacyShortcutPaths()) File.Delete(legacy);
    }

    public static void RemoveShortcuts() => RemoveShortcuts(ShortcutPaths().Concat(LegacyShortcutPaths()));

    internal static void RemoveShortcuts(IEnumerable<string> shortcutPaths)
    {
        var failures = new List<Exception>();
        foreach (var path in shortcutPaths)
            DeleteFileBestEffort(path, failures);

        if (failures.Count != 0)
            throw new AggregateException("One or more WiiCompiled shortcuts could not be removed.", failures);
    }

    private static string[] ShortcutPaths() =>
    [
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), ShortcutFileName),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.StartMenu), "Programs", ShortcutFileName),
    ];

    private static string[] LegacyShortcutPaths() =>
    [
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), LegacyShortcutFileName),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.StartMenu), "Programs", LegacyShortcutFileName),
    ];

    private static void DeleteFileBestEffort(string path, List<Exception> failures)
    {
        try
        {
            File.Delete(path);
        }
        catch (Exception ex)
        {
            failures.Add(new IOException($"Could not delete shortcut {path}: {ex.Message}", ex));
        }
    }

    private static int EstimateSizeKb(string directory)
    {
        try
        {
            var bytes = Directory.EnumerateFiles(directory, "*", SearchOption.AllDirectories)
                .Sum(path => new FileInfo(path).Length);
            return (int)Math.Min(int.MaxValue, (bytes + 1023) / 1024);
        }
        catch
        {
            return 0;
        }
    }
}
