using System.Net;
using System.Net.Http.Headers;

namespace WiiCompiled.Setup.Windows;

internal static class ResumableDownload
{
    // Resume only with a strong validator. Never concatenate two pack versions.
    public static async Task DownloadAsync(HttpClient client, Uri uri, string path,
        Action<long, long?> progress, CancellationToken cancellation)
    {
        EntityTagHeaderValue? validator = null;
        Exception? lastError = null;
        for (var attempt = 0; attempt < 3; attempt++)
        {
            cancellation.ThrowIfCancellationRequested();
            try
            {
                var offset = validator is not null && File.Exists(path) ? new FileInfo(path).Length : 0;
                using var request = new HttpRequestMessage(HttpMethod.Get, uri);
                if (offset > 0)
                {
                    request.Headers.Range = new RangeHeaderValue(offset, null);
                    request.Headers.IfRange = new RangeConditionHeaderValue(validator!);
                }
                using var headerTimeout = CancellationTokenSource.CreateLinkedTokenSource(cancellation);
                headerTimeout.CancelAfter(TimeSpan.FromSeconds(30));
                using var response = await client.SendAsync(request, HttpCompletionOption.ResponseHeadersRead, headerTimeout.Token);
                response.EnsureSuccessStatusCode();
                if (response.StatusCode == HttpStatusCode.PartialContent)
                {
                    if (offset == 0 || response.Content.Headers.ContentRange?.From != offset ||
                        response.Headers.ETag?.ToString() != validator?.ToString())
                        throw new InvalidDataException("The resumed pack changed. Restart the download.");
                }
                else offset = 0; // Server ignored Range or If-Range detected a new pack.
                validator = response.Headers.ETag is { IsWeak: false } etag ? etag : null;
                var length = response.Content.Headers.ContentLength;
                long? total = length.HasValue ? offset + length.Value : null;
                if (response.Content.Headers.ContentRange?.Length is long full && total != full)
                    throw new InvalidDataException("The server returned an incomplete pack range.");
                var volume = new DriveInfo(Path.GetPathRoot(Path.GetFullPath(path))!);
                if (length.HasValue && length.Value > volume.AvailableFreeSpace - 512L * 1024 * 1024)
                    throw new IOException("Not enough disk space for the Retro Rewind download.");
                await using var input = await response.Content.ReadAsStreamAsync(cancellation);
                await using var output = new FileStream(path, offset > 0 ? FileMode.Append : FileMode.Create, FileAccess.Write, FileShare.None);
                var buffer = new byte[1024 * 1024];
                var received = offset;
                while (true)
                {
                    using var idleTimeout = CancellationTokenSource.CreateLinkedTokenSource(cancellation);
                    idleTimeout.CancelAfter(TimeSpan.FromSeconds(45));
                    var count = await input.ReadAsync(buffer, idleTimeout.Token);
                    if (count == 0) break;
                    await output.WriteAsync(buffer.AsMemory(0, count), cancellation);
                    received += count;
                    progress(received, total);
                }
                if (total.HasValue && received != total.Value) throw new IOException("The pack download was interrupted.");
                await output.FlushAsync(cancellation);
                return;
            }
            catch (Exception error) when (!cancellation.IsCancellationRequested &&
                (error is IOException or HttpRequestException or OperationCanceledException))
            {
                lastError = error;
                if (error is InvalidDataException) validator = null;
                if (attempt < 2) await Task.Delay(TimeSpan.FromSeconds(attempt + 1), cancellation);
            }
        }
        throw new IOException("Retro Rewind could not be downloaded after three attempts. Check your connection and free disk space, then retry.", lastError);
    }
}
