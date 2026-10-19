// The Mlx port's ResourceResponse functions (ResourceResponse.h).
#include "config.h"
#include "ResourceResponse.h"

#include "HTTPParsers.h"

namespace WebCore {

// The file name a download is saved under: Content-Disposition's, as
// the other ports read it.
String ResourceResponse::platformSuggestedFilename() const
{
    return filenameFromHTTPContentDisposition(httpHeaderField(HTTPHeaderName::ContentDisposition)).toString();
}

} // namespace WebCore
