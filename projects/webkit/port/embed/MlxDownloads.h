// Downloads of the Mlx port's embedding (PORT.md, "Downloads"): what
// WebCore decides not to show but to save. A frame's load whose response
// is an attachment (Content-Disposition), of a type no frame shows, or
// asked for by a link's download attribute after the load began, is
// turned into a download (MlxFrameLoaderClient's
// convertMainResourceLoadToDownload): the body of that one request goes
// to the file (MlxNetwork::divert), a POST's answer and a URL good once
// included. A link's download attribute (WebCore's startDownload), and a
// download resumed, make a request of their own (MlxNetwork::fetch);
// blob:, data: and file: URLs are read at once.
//
// The bytes go into "NAME.EXT.download" as they come, on the thread that
// reads them, as Safari keeps a download (port/mlx/download.mlx: std.fs),
// in the embedder's downloads directory under the name the response or
// the page suggests, kept apart from what is there ("NAME (1).EXT"); once
// all came the file is renamed NAME.EXT. A download paused keeps its
// partial file and goes on with a Range request where the server allows
// (a 206: its If-Range validator, ETag or Last-Modified, the same), else
// from the start; one cancelled leaves nothing.
//
// The list is kept in the profile (openDatabase: downloads.sqlite, which
// MlxProfile opens): a restart finds it, a download that was running then
// paused, its partial file kept. The embedder lists them (Voyager's
// toolbar popover and voyager://downloads, webcore-dump's output) and
// hears of each change.
#pragma once

#include <wtf/Function.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceLoader;
class ResourceRequest;
class ResourceResponse;

class MlxDownloads {
public:
    enum class State : uint8_t { Running, Paused, Done, Failed, Cancelled };

    struct Download {
        unsigned id { 0 };
        String url;
        // The file's name, and its path: the final one once done, the
        // partial file's ("NAME.EXT.download") while running or paused.
        String name;
        String path;
        String mimeType;
        State state { State::Running };
        uint64_t received { 0 };
        // The length the response gave (0: not known).
        uint64_t expected { 0 };
        // Bytes a second lately (while running; 0: not known yet).
        double rate { 0 };
        // When it started and ended (seconds since the epoch).
        double started { 0 };
        double finished { 0 };
        // Whether it can go on after a pause (a GET; else pausing it
        // cancels it).
        bool resumable { false };
        String error;
    };

    // Where downloads are saved (made when missing); none: downloads fail.
    WEBCORE_EXPORT static void setDirectory(const String&);
    WEBCORE_EXPORT static const String& directory();

    // The profile's list of downloads (an SQLite database at PATH, made
    // when missing): what it holds is listed (a download that was running
    // is paused), and each change is written to it.
    WEBCORE_EXPORT static void openDatabase(const String& path);

    // A download of the request's URL made with a request of its own (the
    // response, when there was one, names it), under SUGGESTEDNAME when
    // not empty.
    WEBCORE_EXPORT static void start(const ResourceRequest&, const ResourceResponse*, const String& suggestedName);
    // The loader's response turned into a download: its body (the one
    // request's) goes to the file; a load whose body is not the network's
    // (data:, blob:, file:) is read again.
    WEBCORE_EXPORT static void convert(ResourceLoader*, const ResourceRequest&, const ResourceResponse&, const String& suggestedName);

    // A running download paused (cancelled when it cannot be resumed), a
    // paused one resumed, either cancelled (its partial file removed).
    WEBCORE_EXPORT static void pause(unsigned id);
    WEBCORE_EXPORT static void resume(unsigned id);
    WEBCORE_EXPORT static void cancel(unsigned id);
    // The finished downloads (done, failed, cancelled) off the list; their
    // files stay.
    WEBCORE_EXPORT static void clearFinished();

    // The downloads, in the order they started.
    WEBCORE_EXPORT static const Vector<Download>& list();
    WEBCORE_EXPORT static const Download* find(unsigned id);
    // Whether one is still running.
    WEBCORE_EXPORT static bool running();
    // Called on the main thread with each download that started, went on
    // or ended (0 ID: the list changed otherwise, cleared).
    WEBCORE_EXPORT static void setObserver(Function<void(const Download*)>&&);
};

} // namespace WebCore
