namespace WiiCompiled.Setup.Windows;

internal sealed class PackReplacement : IDisposable
{
    public string Destination { get; }
    private readonly string _backup;
    private bool _finished;
    public PackReplacement(string source, string destination)
    {
        Destination = Path.GetFullPath(destination);
        _backup = Destination + ".backup-" + Guid.NewGuid().ToString("N");
        if (Directory.Exists(Destination)) Directory.Move(Destination, _backup);
        try { Directory.Move(source, Destination); }
        catch
        {
            if (Directory.Exists(_backup)) Directory.Move(_backup, Destination);
            throw;
        }
    }
    public void Commit()
    {
        _finished = true;
        try { if (Directory.Exists(_backup)) Directory.Delete(_backup, true); }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
    public void Dispose()
    {
        if (_finished) return;
        var failed = Destination + ".failed-" + Guid.NewGuid().ToString("N");
        if (Directory.Exists(Destination)) Directory.Move(Destination, failed);
        if (Directory.Exists(_backup)) Directory.Move(_backup, Destination);
        _finished = true;
        try { if (Directory.Exists(failed)) Directory.Delete(failed, true); }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}
