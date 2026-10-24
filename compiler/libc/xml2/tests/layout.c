/* layout.c: the sizes of libxml2's structures and the offsets of the
 * fields WebKit reads, as the header lays them out; with MLX_XML2, also
 * as mlx-xml2 lays them out (its __mlx_xml2_layout), which must be the
 * same. tools/check_xml2.sh compares the header's numbers with those of
 * the system's libxml2 header too. */
#include <stdio.h>
#include <stddef.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/entities.h>

#ifdef MLX_XML2
long __mlx_xml2_layout(int index);
#endif

int main(void)
{
    static const char *names[] = {
        "sizeof(xmlParserCtxt)", "sizeof(xmlSAXHandler)", "sizeof(xmlDoc)", "sizeof(xmlDtd)",
        "sizeof(xmlEntity)", "sizeof(xmlParserInput)", "sizeof(xmlError)", "sizeof(xmlValidCtxt)",
        "xmlParserCtxt.lastError", "xmlParserCtxt.input", "xmlParserCtxt._private", "xmlParserCtxt.myDoc",
        "xmlParserCtxt.sax2", "xmlEntity.expandedSize", "xmlParserInput.col", "xmlSAXHandler.serror",
        "xmlParserCtxt.sizeentcopy",
    };
    long header[17];
    int i, bad = 0;
    header[0] = sizeof(xmlParserCtxt);
    header[1] = sizeof(xmlSAXHandler);
    header[2] = sizeof(xmlDoc);
    header[3] = sizeof(xmlDtd);
    header[4] = sizeof(xmlEntity);
    header[5] = sizeof(xmlParserInput);
    header[6] = sizeof(xmlError);
    header[7] = sizeof(xmlValidCtxt);
    header[8] = offsetof(xmlParserCtxt, lastError);
    header[9] = offsetof(xmlParserCtxt, input);
    header[10] = offsetof(xmlParserCtxt, _private);
    header[11] = offsetof(xmlParserCtxt, myDoc);
    header[12] = offsetof(xmlParserCtxt, sax2);
    header[13] = offsetof(xmlEntity, expandedSize);
    header[14] = offsetof(xmlParserInput, col);
    header[15] = offsetof(xmlSAXHandler, serror);
    header[16] = offsetof(xmlParserCtxt, sizeentcopy);
    for (i = 0; i < 17; i++) {
#ifdef MLX_XML2
        long library = __mlx_xml2_layout(i);
        if (library != header[i]) {
            printf("%s: the header's %ld, the library's %ld\n", names[i], header[i], library);
            bad = 1;
            continue;
        }
#endif
        printf("%s %ld\n", names[i], header[i]);
    }
    return bad;
}
