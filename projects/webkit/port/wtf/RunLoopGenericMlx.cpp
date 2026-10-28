// WTF's generic run loop (USE_GENERIC_EVENT_LOOP, JSCOnly's lists/wtf.txt):
// WebKit's RunLoopGeneric.cpp, compiled here, with its timer tree's
// RedBlackTree::remove made to leave the removed node unlinked.
//
// WebKit's RedBlackTree::remove (wtf/RedBlackTree.h) takes a node out of
// the tree but leaves its m_left and m_right, CheckedPtrs, at the children
// it had: the removed node still counts them. A scheduled task the run
// loop's tree held (RunLoop::TimerBase's ScheduledTask) is removed when
// its timer stops or fires; when one of its old children's timer is then
// destroyed, WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR finds that child still
// counted, scribbles and leaks it, and the removed task's next insert
// (NodeBase::reset) or its own destruction releases the dead child:
// crashDueToCheckedPtrToDeadObject. Any compiler's build does it with
// that order of timers (projects/webkit/tests/wtf-runloop.cpp makes it
// with three); WebCore's main thread met it about once in 30 runs of
// crypto's RSA key generation (PORT.md, "What M8's fifth part leaves").
//
// The tree's remove for the run loop's tasks is WebKit's, followed by
// the reset insert does first (the children it releases are alive: they
// are in the tree), as an explicit specialization declared before
// RunLoopGeneric.cpp's first use of it.
#include "config.h"
#include <wtf/RedBlackTree.h>
#include <wtf/RunLoop.h>

namespace WTF {
using RunLoopScheduledTask = RunLoop::TimerBase::ScheduledTask;
template<> RunLoopScheduledTask* RedBlackTree<RunLoopScheduledTask, MonotonicTime>::remove(RunLoopScheduledTask*);
}

#include "../../upstream/Source/WTF/wtf/generic/RunLoopGeneric.cpp"

namespace WTF {

template<> RunLoopScheduledTask* RedBlackTree<RunLoopScheduledTask, MonotonicTime>::remove(RunLoopScheduledTask* z)
{
    // WebKit's RedBlackTree::remove(NodeType*).
    ASSERT(z);
    ASSERT(z->parent() || z == m_root);

    RunLoopScheduledTask* y;
    if (!z->left() || !z->right())
        y = z;
    else
        y = z->successor();

    RunLoopScheduledTask* x;
    if (y->left())
        x = y->left();
    else
        x = y->right();

    RunLoopScheduledTask* xParent;
    if (x) {
        x->setParent(y->parent());
        xParent = x->parent();
    } else
        xParent = y->parent();
    if (!y->parent())
        m_root = x;
    else {
        if (y == y->parent()->left())
            y->parent()->setLeft(x);
        else
            y->parent()->setRight(x);
    }

    if (y != z) {
        if (y->color() == Black)
            removeFixup(x, xParent);

        y->setParent(z->parent());
        y->setColor(z->color());
        y->setLeft(z->left());
        y->setRight(z->right());

        if (z->left())
            z->left()->setParent(y);
        if (z->right())
            z->right()->setParent(y);
        if (z->parent()) {
            if (z->parent()->left() == z)
                z->parent()->setLeft(y);
            else
                z->parent()->setRight(y);
        } else {
            ASSERT(m_root == z);
            m_root = y;
        }
    } else if (y->color() == Black)
        removeFixup(x, xParent);

    // The port's: z links nothing of the tree any more.
    z->reset();
    return z;
}

} // namespace WTF
