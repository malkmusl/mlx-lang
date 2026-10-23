// Downloads of the Mlx port's embedding (PORT.md, "Downloads"): what
// WebCore decides not to show but to save (MlxFrameLoaderClient: a
// response with Content-Disposition: attachment, a link's download
// attribute, a type no frame shows) is fetched again by the port's
// network (MlxNetwork::fetch: http:, https:), read from the process's
// blob registry (blob:), decoded (data:) or read (file:), and written by
// the Mlx side (port/mlx/download.mlx: std.fs) into the embedder's
// downloads directory under the name the response or the page suggests,
// kept apart from what is there ("NAME (1).EXT"). The embedder lists them (Voyager's
// sidebar and voyager://downloads, webcore-dump's output) and hears of
// each change.
#pragma once

#include <wtf/Function.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceRequest;
class ResourceResponse;

class MlxDownloads {
public:
    enum class State : uint8_t { Running, Done, Failed };

    struct Download {
        unsigned id { 0 };
        String url;
        // The name suggested and the file's path (once made).
        String name;
        String path;
        State state { State::Running };
        uint64_t received { 0 };
        // The length the response gave (0: not known).
        uint64_t expected { 0 };
        String error;
    };

    // Where downloads are saved (made when missing); none: downloads fail.
    WEBCORE_EXPORT static void setDirectory(const String&);
    WEBCORE_EXPORT static const String& directory();

    // A download of the request's URL (the response, when there was one,
    // names it and gives its length), under SUGGESTEDNAME when not empty.
    WEBCORE_EXPORT static void start(const ResourceRequest&, const ResourceResponse*, const String& suggestedName);

    // The downloads of the process, in the order they started.
    WEBCORE_EXPORT static const Vector<Download>& list();
    // Whether one is still running.
    WEBCORE_EXPORT static bool running();
    // Called on the main thread with each download that started, went on
    // or ended.
    WEBCORE_EXPORT static void setObserver(Function<void(const Download&)>&&);
};

} // namespace WebCore
