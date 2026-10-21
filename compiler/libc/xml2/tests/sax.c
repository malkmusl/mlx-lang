/* sax.c: prints every SAX2 callback libxml2's parser makes for documents,
 * with its arguments and the parser's position, the errors as each
 * handler gets them, and the parser's state at the end: what
 * tools/check_xml2.sh compares between mlx-xml2 and the system's libxml2.
 *
 *   sax [options] FILE...
 *
 *   -c N       push the document N bytes at a time (0: all at once)
 *   -noent     XML_PARSE_NOENT        -huge   XML_PARSE_HUGE
 *   -recover   XML_PARSE_RECOVER      -noblanks XML_PARSE_NOBLANKS
 *   -nocdata   XML_PARSE_NOCDATA      -pedantic XML_PARSE_PEDANTIC
 *   -webkit    as WebKit's XMLDocumentParser: its handlers (SAX2's
 *              document, internal subset and entity declarations, its
 *              getEntity and deferred declarations), NOENT|HUGE, the
 *              document as UTF-16 with xmlSwitchEncoding before every
 *              chunk and in startDocument
 *   -fragment  as WebKit parses a fragment: xmlCreateMemoryParserCtxt,
 *              the handlers copied in, NODICT|NOENT|HUGE, xmlParseContent
 *   -serror    a structured error handler on the handler (sax->serror)
 *   -gserror   the global structured error handler
 *   -generic   no SAX error callbacks: libxml2's own report on stderr
 *   -nosax     no handler: SAX2's defaults (only errors print)
 *   -stop N    xmlStopParser at the Nth startElementNs
 *   -summary   what is printed of each document (but libxml2's own reports
 *              on stderr) counted and hashed, not printed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/SAX2.h>
#include <libxml/entities.h>
#include <libxml/xmlerror.h>

static xmlParserCtxtPtr mainContext;
/* Where the events go: stdout, or with -summary a file of what is printed
 * of the document, of which only the count of lines and a hash are. */
static FILE *out;
static int optSummary;
static int optWebkit, optFragment, optSerror, optGserror, optGeneric, optNosax;
static int optStop = -1;
static int startCount;

static void putEscaped(const unsigned char *s, int len)
{
    int i;
    if (s == NULL) {
        fputs("(null)", out);
        return;
    }
    if (len < 0)
        len = (int) strlen((const char *) s);
    fputc('"', out);
    for (i = 0; i < len; i++) {
        unsigned char c = s[i];
        if (c == '\n')
            fputs("\\n", out);
        else if (c == '\r')
            fputs("\\r", out);
        else if (c == '\t')
            fputs("\\t", out);
        else if (c == '"' || c == '\\')
            fprintf(out, "\\%c", c);
        else if (c < 0x20 || c == 0x7f)
            fprintf(out, "\\x%02x", c);
        else
            fputc(c, out);
    }
    fputc('"', out);
}

/* The position of the parser handing the callback (CLOSURE, a context when
 * the user data is the context) and of the document's. */
static void position(void *closure)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) closure;
    if (ctxt != mainContext && ctxt != NULL && ctxt->input != NULL)
        fprintf(out, " @%d:%d/%d:%d", ctxt->input->line, ctxt->input->col, mainContext->input->line, mainContext->input->col);
    else if (mainContext->input != NULL)
        fprintf(out, " @%d:%d", mainContext->input->line, mainContext->input->col);
    fprintf(out, " d%d", mainContext->depth);
    fputc('\n', out);
}

static void switchToUTF16(xmlParserCtxtPtr ctxt)
{
    xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_UTF16LE);
}

/* WebKit's deferred entity declarations: those whose content has a '&' are
 * declared at the first getEntity. */
struct Deferred {
    char *name;
    int type;
    char *publicId;
    char *systemId;
    char *content;
};
static struct Deferred deferred[64];
static int deferredCount;
static int deferredFlushed = 1;

static char *dupOrNull(const xmlChar *s)
{
    if (s == NULL)
        return NULL;
    return strdup((const char *) s);
}

static void flushDeferred(void)
{
    int i;
    deferredFlushed = 1;
    for (i = 0; i < deferredCount; i++) {
        if (xmlGetDocEntity(mainContext->myDoc, BAD_CAST deferred[i].name))
            continue;
        xmlSAX2EntityDecl(mainContext, BAD_CAST deferred[i].name, deferred[i].type, BAD_CAST deferred[i].publicId, BAD_CAST deferred[i].systemId, BAD_CAST deferred[i].content);
    }
}

static void onInternalSubset(void *closure, const xmlChar *name, const xmlChar *externalID, const xmlChar *systemID)
{
    fputs("internalSubset ", out);
    putEscaped(name, -1);
    fputc(' ', out);
    putEscaped(externalID, -1);
    fputc(' ', out);
    putEscaped(systemID, -1);
    position(closure);
    if (optWebkit)
        xmlSAX2InternalSubset(closure, name, externalID, systemID);
}

static void onExternalSubset(void *closure, const xmlChar *name, const xmlChar *externalID, const xmlChar *systemID)
{
    fputs("externalSubset ", out);
    putEscaped(name, -1);
    fputc(' ', out);
    putEscaped(externalID, -1);
    fputc(' ', out);
    putEscaped(systemID, -1);
    position(closure);
}

static void onEntityDecl(void *closure, const xmlChar *name, int type, const xmlChar *publicId, const xmlChar *systemId, xmlChar *content)
{
    fprintf(out, "entityDecl %d ", type);
    putEscaped(name, -1);
    fputc(' ', out);
    putEscaped(publicId, -1);
    fputc(' ', out);
    putEscaped(systemId, -1);
    fputc(' ', out);
    putEscaped(content, -1);
    position(closure);
    if (!optWebkit)
        return;
    if (type == XML_INTERNAL_GENERAL_ENTITY && content != NULL && strchr((const char *) content, '&') != NULL && deferredCount < 64) {
        deferred[deferredCount].name = dupOrNull(name);
        deferred[deferredCount].type = type;
        deferred[deferredCount].publicId = dupOrNull(publicId);
        deferred[deferredCount].systemId = dupOrNull(systemId);
        deferred[deferredCount].content = dupOrNull(content);
        deferredCount++;
        deferredFlushed = 0;
        return;
    }
    xmlSAX2EntityDecl(mainContext, name, type, publicId, systemId, content);
}

static void showEntity(xmlEntityPtr ent)
{
    if (ent == NULL) {
        fputs(" -> (none)", out);
        return;
    }
    fprintf(out, " -> etype %d length %d content ", ent->etype, ent->length);
    putEscaped(ent->content, -1);
}

static xmlEntityPtr onGetEntity(void *closure, const xmlChar *name)
{
    xmlEntityPtr ent;
    fputs("getEntity ", out);
    putEscaped(name, -1);
    if (optWebkit) {
        ent = xmlGetPredefinedEntity(name);
        if (ent == NULL) {
            if (!deferredFlushed)
                flushDeferred();
            ent = xmlGetDocEntity(mainContext->myDoc, name);
        }
    } else {
        ent = xmlSAX2GetEntity(closure, name);
    }
    showEntity(ent);
    position(closure);
    return ent;
}

static xmlEntityPtr onGetParameterEntity(void *closure, const xmlChar *name)
{
    xmlEntityPtr ent = xmlSAX2GetParameterEntity(closure, name);
    fputs("getParameterEntity ", out);
    putEscaped(name, -1);
    showEntity(ent);
    position(closure);
    return ent;
}

static void onNotationDecl(void *closure, const xmlChar *name, const xmlChar *publicId, const xmlChar *systemId)
{
    fputs("notationDecl ", out);
    putEscaped(name, -1);
    fputc(' ', out);
    putEscaped(publicId, -1);
    fputc(' ', out);
    putEscaped(systemId, -1);
    position(closure);
}

static void onUnparsedEntityDecl(void *closure, const xmlChar *name, const xmlChar *publicId, const xmlChar *systemId, const xmlChar *notationName)
{
    fputs("unparsedEntityDecl ", out);
    putEscaped(name, -1);
    fputc(' ', out);
    putEscaped(publicId, -1);
    fputc(' ', out);
    putEscaped(systemId, -1);
    fputc(' ', out);
    putEscaped(notationName, -1);
    position(closure);
}

static void onStartDocument(void *closure)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) closure;
    fputs("startDocument version ", out);
    putEscaped(ctxt->version, -1);
    fputs(" encoding ", out);
    putEscaped(ctxt->encoding, -1);
    fprintf(out, " standalone %d", ctxt->standalone);
    position(closure);
    if (optWebkit) {
        switchToUTF16(ctxt);
        xmlSAX2StartDocument(closure);
    }
}

static void onEndDocument(void *closure)
{
    fputs("endDocument", out);
    position(closure);
    if (optWebkit)
        xmlSAX2EndDocument(closure);
}

static void onStartElementNs(void *closure, const xmlChar *localname, const xmlChar *prefix, const xmlChar *URI, int nb_namespaces, const xmlChar **namespaces, int nb_attributes, int nb_defaulted, const xmlChar **attributes)
{
    int i;
    fputs("startElementNs ", out);
    putEscaped(localname, -1);
    fputc(' ', out);
    putEscaped(prefix, -1);
    fputc(' ', out);
    putEscaped(URI, -1);
    fprintf(out, " ns %d", nb_namespaces);
    for (i = 0; i < nb_namespaces; i++) {
        fputc(' ', out);
        putEscaped(namespaces[2 * i], -1);
        fputc('=', out);
        putEscaped(namespaces[2 * i + 1], -1);
    }
    fprintf(out, " atts %d defaulted %d", nb_attributes, nb_defaulted);
    for (i = 0; i < nb_attributes; i++) {
        const xmlChar **a = attributes + 5 * i;
        fputc(' ', out);
        putEscaped(a[1], -1);
        fputc(':', out);
        putEscaped(a[0], -1);
        fputc('{', out);
        putEscaped(a[2], -1);
        fputs("}=", out);
        putEscaped(a[3], (int) (a[4] - a[3]));
    }
    position(closure);
    startCount++;
    if (startCount == optStop) {
        fputs("xmlStopParser\n", out);
        xmlStopParser(mainContext);
    }
}

static void onEndElementNs(void *closure, const xmlChar *localname, const xmlChar *prefix, const xmlChar *URI)
{
    fputs("endElementNs ", out);
    putEscaped(localname, -1);
    fputc(' ', out);
    putEscaped(prefix, -1);
    fputc(' ', out);
    putEscaped(URI, -1);
    position(closure);
}

static void onCharacters(void *closure, const xmlChar *ch, int len)
{
    fprintf(out, "characters %d ", len);
    putEscaped(ch, len);
    position(closure);
}

static void onIgnorableWhitespace(void *closure, const xmlChar *ch, int len)
{
    fprintf(out, "ignorableWhitespace %d ", len);
    putEscaped(ch, len);
    position(closure);
}

static void onCdataBlock(void *closure, const xmlChar *ch, int len)
{
    fprintf(out, "cdataBlock %d ", len);
    putEscaped(ch, len);
    position(closure);
}

static void onComment(void *closure, const xmlChar *value)
{
    fputs("comment ", out);
    putEscaped(value, -1);
    position(closure);
}

static void onProcessingInstruction(void *closure, const xmlChar *target, const xmlChar *data)
{
    fputs("processingInstruction ", out);
    putEscaped(target, -1);
    fputc(' ', out);
    putEscaped(data, -1);
    position(closure);
}

static void onReference(void *closure, const xmlChar *name)
{
    fputs("reference ", out);
    putEscaped(name, -1);
    position(closure);
}

static void report(const char *kind, void *closure, const char *msg, va_list args)
{
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), msg, args);
    fprintf(out, "%s ", kind);
    putEscaped((const unsigned char *) buffer, -1);
    position(closure);
}

static void onWarning(void *closure, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    report("warning", closure, msg, args);
    va_end(args);
}

static void onError(void *closure, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    report("error", closure, msg, args);
    va_end(args);
}

static void onFatalError(void *closure, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    report("fatalError", closure, msg, args);
    va_end(args);
}

static void showError(const char *kind, xmlErrorPtr error)
{
    fprintf(out, "%s domain %d code %d level %d line %d col %d int1 %d str1 ", kind, error->domain, error->code, error->level, error->line, error->int2, error->int1);
    putEscaped((const unsigned char *) error->str1, -1);
    fputs(" str2 ", out);
    putEscaped((const unsigned char *) error->str2, -1);
    fputs(" str3 ", out);
    putEscaped((const unsigned char *) error->str3, -1);
    fputs(" file ", out);
    putEscaped((const unsigned char *) error->file, -1);
    fputs(" message ", out);
    putEscaped((const unsigned char *) error->message, -1);
}

static void onStructuredError(void *closure, xmlErrorPtr error)
{
    showError("serror", error);
    fprintf(out, " user %s", closure == mainContext ? "main" : closure == NULL ? "null" : "other");
    position(mainContext);
}

/* The document's UTF-8 as UTF-16LE (a byte that is not UTF-8 as U+FFFD),
 * as WebKit hands it to the parser. */
static unsigned char *toUTF16(const unsigned char *s, size_t n, size_t *outLength)
{
    unsigned char *out = malloc(n * 4 + 4);
    size_t i = 0, o = 0;
    while (i < n) {
        unsigned int c = s[i];
        size_t l = 1;
        if (c >= 0xF0 && c < 0xF8 && i + 3 < n && (s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80 && (s[i + 3] & 0xC0) == 0x80) {
            c = ((c & 7) << 18) | ((s[i + 1] & 0x3F) << 12) | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
            l = 4;
        } else if (c >= 0xE0 && c < 0xF0 && i + 2 < n && (s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80) {
            c = ((c & 15) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            l = 3;
        } else if (c >= 0xC0 && c < 0xE0 && i + 1 < n && (s[i + 1] & 0xC0) == 0x80) {
            c = ((c & 31) << 6) | (s[i + 1] & 0x3F);
            l = 2;
        } else if (c >= 0x80) {
            c = 0xFFFD;
        }
        i += l;
        if (c >= 0x10000) {
            unsigned int v = c - 0x10000;
            unsigned int hi = 0xD800 | (v >> 10);
            unsigned int lo = 0xDC00 | (v & 0x3FF);
            out[o++] = hi & 0xFF;
            out[o++] = hi >> 8;
            out[o++] = lo & 0xFF;
            out[o++] = lo >> 8;
        } else {
            out[o++] = c & 0xFF;
            out[o++] = c >> 8;
        }
    }
    *outLength = o;
    return out;
}

static unsigned char *readFile(const char *name, size_t *length)
{
    FILE *f = fopen(name, "rb");
    unsigned char *data;
    long size;
    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = malloc(size + 1);
    if (fread(data, 1, size, f) != (size_t) size) {
        fclose(f);
        free(data);
        return NULL;
    }
    data[size] = 0;
    fclose(f);
    *length = size;
    return data;
}

/* The count of the events in F and their FNV-1a hash. */
static void summarize(FILE *f)
{
    unsigned int hash = 2166136261u;
    long lines = 0;
    int c;
    fflush(f);
    fseek(f, 0, SEEK_SET);
    while ((c = fgetc(f)) != EOF) {
        hash = (hash ^ (unsigned int) c) * 16777619u;
        if (c == '\n')
            lines++;
    }
    printf("events %ld hash %08x\n", lines, hash);
}

static void handlers(xmlSAXHandler *sax)
{
    memset(sax, 0, sizeof(*sax));
    sax->internalSubset = onInternalSubset;
    sax->externalSubset = onExternalSubset;
    sax->entityDecl = onEntityDecl;
    sax->getEntity = onGetEntity;
    sax->startDocument = onStartDocument;
    sax->endDocument = onEndDocument;
    sax->startElementNs = onStartElementNs;
    sax->endElementNs = onEndElementNs;
    sax->characters = onCharacters;
    sax->ignorableWhitespace = onIgnorableWhitespace;
    sax->cdataBlock = onCdataBlock;
    sax->comment = onComment;
    sax->processingInstruction = onProcessingInstruction;
    if (!optGeneric) {
        sax->warning = onWarning;
        sax->error = onError;
        sax->fatalError = onFatalError;
    }
    if (!optWebkit) {
        sax->getParameterEntity = onGetParameterEntity;
        sax->notationDecl = onNotationDecl;
        sax->unparsedEntityDecl = onUnparsedEntityDecl;
        sax->reference = onReference;
    }
    if (optSerror)
        sax->serror = onStructuredError;
    sax->initialized = XML_SAX2_MAGIC;
}

static void parse(const char *name, int chunk, int options)
{
    size_t length = 0, at = 0;
    unsigned char *data = readFile(name, &length);
    unsigned char *bytes;
    xmlSAXHandler sax;
    xmlErrorPtr last;
    int ret;
    if (data == NULL) {
        fprintf(out, "%s: unreadable\n", name);
        return;
    }
    out = optSummary ? tmpfile() : stdout;
    printf("== %s\n", name);
    bytes = data;
    if (optWebkit) {
        size_t utf16Length = 0;
        bytes = toUTF16(data, length, &utf16Length);
        length = utf16Length;
    }
    handlers(&sax);
    startCount = 0;
    deferredCount = 0;
    deferredFlushed = 1;
    if (optFragment) {
        if (length == 0) {
            fprintf(out, "empty\n");
            free(data);
            return;
        }
        mainContext = xmlCreateMemoryParserCtxt((const char *) bytes, (int) length);
        if (mainContext == NULL) {
            fprintf(out, "no context\n");
            free(data);
            return;
        }
        if (!optNosax)
            memcpy(mainContext->sax, &sax, sizeof(sax));
        ret = xmlCtxtUseOptions(mainContext, XML_PARSE_NODICT | XML_PARSE_NOENT | XML_PARSE_HUGE | options);
        fprintf(out, "xmlCtxtUseOptions %d\n", ret);
        mainContext->sax2 = 1;
        mainContext->instate = XML_PARSER_CONTENT;
        mainContext->depth = 0;
        mainContext->str_xml = xmlDictLookup(mainContext->dict, BAD_CAST "xml", 3);
        mainContext->str_xmlns = xmlDictLookup(mainContext->dict, BAD_CAST "xmlns", 5);
        mainContext->str_xml_ns = xmlDictLookup(mainContext->dict, XML_XML_NAMESPACE, 36);
        xmlParseContent(mainContext);
        fprintf(out, "xmlParseContent done");
        position(mainContext);
    } else {
        mainContext = xmlCreatePushParserCtxt(optNosax ? NULL : &sax, NULL, NULL, 0, NULL);
        if (optWebkit)
            options |= XML_PARSE_NOENT | XML_PARSE_HUGE;
        ret = xmlCtxtUseOptions(mainContext, options);
        fprintf(out, "xmlCtxtUseOptions %d\n", ret);
        if (optWebkit)
            switchToUTF16(mainContext);
        while (at < length) {
            size_t n = chunk > 0 ? (size_t) chunk : length;
            if (optWebkit && (n & 1))
                n++;
            if (n > length - at)
                n = length - at;
            if (optWebkit)
                switchToUTF16(mainContext);
            ret = xmlParseChunk(mainContext, (const char *) bytes + at, (int) n, 0);
            at += n;
            if (ret != 0)
                fprintf(out, "xmlParseChunk %d at %d\n", ret, (int) at);
        }
        ret = xmlParseChunk(mainContext, NULL, 0, 1);
        fprintf(out, "xmlParseChunk end %d", ret);
        position(mainContext);
    }
    fprintf(out, "wellFormed %d nsWellFormed %d valid %d errNo %d instate %d disableSAX %d standalone %d charset %d inSubset %d nbentities %lu sizeentities %lu sizeentcopy %lu\n",
        mainContext->wellFormed, mainContext->nsWellFormed, mainContext->valid, mainContext->errNo, mainContext->instate, mainContext->disableSAX, mainContext->standalone, mainContext->charset, mainContext->inSubset,
        mainContext->nbentities, mainContext->sizeentities, mainContext->sizeentcopy);
    fprintf(out, "xmlByteConsumed %ld\n", xmlByteConsumed(mainContext));
    last = xmlCtxtGetLastError(mainContext);
    if (last != NULL) {
        showError("lastError", last);
        fputc('\n', out);
    }
    if (mainContext->myDoc != NULL) {
        xmlDtdPtr dtd = mainContext->myDoc->intSubset;
        fputs("myDoc version ", out);
        putEscaped(mainContext->myDoc->version, -1);
        fputs(" encoding ", out);
        putEscaped(mainContext->myDoc->encoding, -1);
        fprintf(out, " standalone %d", mainContext->myDoc->standalone);
        if (dtd != NULL) {
            xmlNodePtr cur;
            fputs(" intSubset ", out);
            putEscaped(dtd->name, -1);
            for (cur = dtd->children; cur != NULL; cur = cur->next) {
                if (cur->type == XML_ENTITY_DECL) {
                    xmlEntityPtr ent = (xmlEntityPtr) cur;
                    fprintf(out, " [%d ", ent->etype);
                    putEscaped(ent->name, -1);
                    fputc(' ', out);
                    putEscaped(ent->content, -1);
                    fputc(' ', out);
                    putEscaped(ent->URI, -1);
                    fprintf(out, " checked %d]", ent->checked);
                }
            }
        }
        fputc('\n', out);
        xmlFreeDoc(mainContext->myDoc);
        mainContext->myDoc = NULL;
    }
    xmlFreeParserCtxt(mainContext);
    mainContext = NULL;
    if (out != stdout) {
        summarize(out);
        fclose(out);
        out = stdout;
    }
    if (bytes != data)
        free(bytes);
    free(data);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    int i, chunk = 0, options = 0;
    out = stdout;
    xmlInitParser();
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-c") && i + 1 < argc)
            chunk = atoi(argv[++i]);
        else if (!strcmp(a, "-stop") && i + 1 < argc)
            optStop = atoi(argv[++i]);
        else if (!strcmp(a, "-noent"))
            options |= XML_PARSE_NOENT;
        else if (!strcmp(a, "-huge"))
            options |= XML_PARSE_HUGE;
        else if (!strcmp(a, "-recover"))
            options |= XML_PARSE_RECOVER;
        else if (!strcmp(a, "-noblanks"))
            options |= XML_PARSE_NOBLANKS;
        else if (!strcmp(a, "-nocdata"))
            options |= XML_PARSE_NOCDATA;
        else if (!strcmp(a, "-pedantic"))
            options |= XML_PARSE_PEDANTIC;
        else if (!strcmp(a, "-webkit"))
            optWebkit = 1;
        else if (!strcmp(a, "-fragment"))
            optFragment = 1;
        else if (!strcmp(a, "-serror"))
            optSerror = 1;
        else if (!strcmp(a, "-gserror"))
            optGserror = 1;
        else if (!strcmp(a, "-generic"))
            optGeneric = 1;
        else if (!strcmp(a, "-nosax"))
            optNosax = 1;
        else if (!strcmp(a, "-summary"))
            optSummary = 1;
        else
            break;
    }
    if (optGserror)
        xmlSetStructuredErrorFunc(NULL, onStructuredError);
    for (; i < argc; i++)
        parse(argv[i], chunk, options);
    return 0;
}
