using WiiCompiled.Setup.Windows;
using System.Net;
using System.Net.Http.Headers;

var root = Path.Combine(Path.GetTempPath(), "WiiCompiled-io-test-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
try
{
    var destination = Path.Combine(root, "RetroRewind6");
    Directory.CreateDirectory(destination);
    File.WriteAllText(Path.Combine(destination, "version"), "old");
    string Stage()
    {
        var path = Path.Combine(root, Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(path);
        File.WriteAllText(Path.Combine(path, "version"), "new");
        return path;
    }
    void Check(string value)
    {
        if (File.ReadAllText(Path.Combine(destination, "version")) != value)
            throw new Exception("Pack replacement did not preserve " + value);
    }
    using (var swap = new PackReplacement(Stage(), destination)) { Check("new"); }
    Check("old");
    try { using var swap = new PackReplacement(Path.Combine(root, "missing"), destination); }
    catch (DirectoryNotFoundException) { }
    Check("old");
    using (var swap = new PackReplacement(Stage(), destination)) { swap.Commit(); }
    Check("new");
    if (Directory.EnumerateDirectories(root, "*.backup-*").Any()) throw new Exception("Backup was not cleaned after success");
    Console.WriteLine("Pack swap, rollback and failed move tests passed.");
    foreach (var changedVersion in new[] { false, true })
    {
        using var handler = new ResumeHandler(changedVersion);
        using var client = new HttpClient(handler);
        var download = Path.Combine(root, "download.bin");
        await ResumableDownload.DownloadAsync(client, new Uri("https://example.invalid/pack.zip"), download, (_, _) => { }, CancellationToken.None);
        var expected = changedVersion ? "NEW-PACK" : "ORIGINAL";
        if (File.ReadAllText(download) != expected || handler.Calls != 2)
            throw new Exception("Resume mixed versions or lost bytes.");
        using var canceled = new CancellationTokenSource();canceled.Cancel();
        try
        {
            await ResumableDownload.DownloadAsync(client, new Uri("https://example.invalid/pack.zip"), download, (_, _) => { }, canceled.Token);
            throw new Exception("Canceled download was not canceled.");
        }
        catch (OperationCanceledException) { }
        if (handler.Calls != 2) throw new Exception("Canceled download made a request.");
    }
    Console.WriteLine("Interrupted download resume, changed ETag restart and cancellation tests passed.");
}
finally { Directory.Delete(root, true); }

sealed class InterruptedStream : MemoryStream
{
    public InterruptedStream() : base(System.Text.Encoding.ASCII.GetBytes("ORIGINAL")) { }
    public override ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken = default)
    {
        if (Position >= 4) throw new IOException("Simulated broken connection");
        return base.ReadAsync(buffer[..Math.Min(buffer.Length, 4)], cancellationToken);
    }
}
sealed class ResumeHandler(bool changedVersion) : HttpMessageHandler
{
    public int Calls { get; private set; }
    protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
    {
        Calls++;
        var response = new HttpResponseMessage(HttpStatusCode.OK);
        response.Headers.ETag = new EntityTagHeaderValue("\"v1\"");
        if (Calls == 1)
        {
            response.Content = new StreamContent(new InterruptedStream());
            response.Content.Headers.ContentLength = 8;
        }
        else
        {
            if (request.Headers.Range?.Ranges.Single().From != 4 || request.Headers.IfRange?.EntityTag?.Tag != "\"v1\"")
                throw new Exception("Resume request did not protect the previous version.");
            if (changedVersion)
            {
                response.Headers.ETag = new EntityTagHeaderValue("\"v2\"");
                response.Content = new StringContent("NEW-PACK");
            }
            else
            {
                response.StatusCode = HttpStatusCode.PartialContent;
                response.Content = new StringContent("INAL");
                response.Content.Headers.ContentRange = new ContentRangeHeaderValue(4, 7, 8);
            }
        }
        return Task.FromResult(response);
    }
}
