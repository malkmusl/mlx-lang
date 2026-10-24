// The Mlx port's Editor functions of the pasteboard (as
// editing/glib/EditorGLib.cpp are GTK's and WPE's, which the port does not
// compile: they need WebContentReaderGLib's reader of web content). The
// selection goes to the pasteboard as text and markup; a paste takes the
// pasteboard's text.
#include "config.h"
#include "Editor.h"

#include "CommonAtomStrings.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "FrameSelection.h"
#include "HTMLImageElement.h"
#include "LocalFrame.h"
#include "Pasteboard.h"
#include "SimpleRange.h"
#include "markup.h"

namespace WebCore {

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, OptionSet<PasteOption> options)
{
    auto range = selectedRange();
    if (!range)
        return;

    bool chosePlainText;
    auto fragment = webContentFromPasteboard(*pasteboard, *range, options.contains(PasteOption::AllowPlainText), chosePlainText);

    if (fragment && options.contains(PasteOption::AsQuotation))
        quoteFragmentForPasting(*fragment);

    if (fragment && shouldInsertFragment(*fragment, *range, EditorInsertAction::Pasted))
        pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(*pasteboard), chosePlainText, options.contains(PasteOption::IgnoreMailBlockquote) ? MailBlockquoteHandling::IgnoreBlockquote : MailBlockquoteHandling::RespectBlockquote);
}

void Editor::platformCopyFont()
{
}

void Editor::platformPasteFont()
{
}

void Editor::writeImageToPasteboard(Pasteboard& pasteboard, Element&, const URL& url, const String& title)
{
    PasteboardImage pasteboardImage;
    pasteboardImage.url.url = url;
    pasteboardImage.url.title = title;
    pasteboard.write(pasteboardImage);
}

void Editor::writeSelectionToPasteboard(Pasteboard& pasteboard)
{
    auto markup = serializePreservingVisualAppearance(document().selection().selection(), ResolveURLs::YesExcludingURLsForPrivacy, SerializeComposedTree::Yes, IgnoreUserSelectNone::Yes);
    pasteboard.writePlainText(selectedTextForDataTransfer(), canSmartCopyOrDelete() ? Pasteboard::CanSmartReplace : Pasteboard::CannotSmartReplace);
    pasteboard.writeString(textHTMLContentTypeAtom(), markup);
}

RefPtr<DocumentFragment> Editor::webContentFromPasteboard(Pasteboard& pasteboard, const SimpleRange& context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;
    if (!allowPlainText)
        return nullptr;
    PasteboardPlainText text;
    pasteboard.read(text);
    if (text.text.isNull())
        return nullptr;
    chosePlainText = true;
    return createFragmentFromText(context, text.text);
}

} // namespace WebCore
