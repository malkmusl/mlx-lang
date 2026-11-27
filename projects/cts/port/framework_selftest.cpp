// The conformance suite framework's own self-tests (delibs: debase,
// depool, dethread, deutil, decpp) and a test log written and read back
// (qphelper), built by mlx-cc and run on mlxlibc: what the suite's
// internal module (modules/internal/ditDelibsTests.cpp) runs as
// dE-IT.delibs.*, without the rest of the framework under it.
// tools/check_cts.sh --framework builds and runs it; tests/521 reads what
// it says. One line a test, `ok NAME` or `FAIL NAME: reason`, then
// `framework self-tests: N passed, M failed`; it exits 0 when none failed.
#include "deDefs.h"
#include "deInt32.h"
#include "deFloat16.h"
#include "deMath.h"
#include "deSha1.h"
#include "deMemory.h"
#include "deCommandLine.h"
#include "dePoolArray.h"
#include "dePoolHeap.h"
#include "dePoolHash.h"
#include "dePoolSet.h"
#include "dePoolHashSet.h"
#include "dePoolHashArray.h"
#include "dePoolMultiSet.h"
#include "deThreadTest.h"
#include "deBlockBuffer.hpp"
#include "deFilePath.hpp"
#include "dePoolArray.hpp"
#include "deRingBuffer.hpp"
#include "deSharedPtr.hpp"
#include "deThreadSafeRingBuffer.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "deCommandLine.hpp"
#include "deArrayBuffer.hpp"
#include "deStringUtil.hpp"
#include "deSpinBarrier.hpp"
#include "deSTLUtil.hpp"
#include "deAppendList.hpp"
#include "qpTestLog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <stdexcept>

namespace
{

// A failed DE_TEST_ASSERT, as an exception the test's call is left by.
void assertionFailed(const char *reason, const char *file, int line)
{
    throw std::runtime_error(std::string(reason) + " at " + file + ":" + de::toString(line));
}

int passed = 0;
int failed = 0;

void check(const char *name, void (*test)(void))
{
    deSetAssertFailureCallback(assertionFailed);
    try
    {
        test();
        printf("ok   %s\n", name);
        passed++;
    }
    catch (const std::exception &e)
    {
        printf("FAIL %s: %s\n", name, e.what());
        failed++;
    }
    deSetAssertFailureCallback(nullptr);
}

// A test log: a session and a case with a text, closed, read back.
void testLogSelfTest(void)
{
    const char *path = "framework-selftest.qpa";
    qpTestLog *log   = qpTestLog_createFileLog(path, QP_TEST_LOG_NO_FLUSH);
    DE_TEST_ASSERT(log != nullptr);
    DE_TEST_ASSERT(qpTestLog_beginSession(log, "#sessionInfo releaseName mlx\n"));
    DE_TEST_ASSERT(qpTestLog_startCase(log, "dE-IT.self.log", QP_TEST_CASE_TYPE_SELF_VALIDATE));
    DE_TEST_ASSERT(qpTestLog_writeText(log, "Note", "A note", QP_KEY_TAG_NONE, "written & read <back>"));
    DE_TEST_ASSERT(qpTestLog_endCase(log, QP_TEST_RESULT_PASS, "Pass"));
    qpTestLog_destroy(log);

    FILE *file = fopen(path, "rb");
    DE_TEST_ASSERT(file != nullptr);
    std::string text;
    char piece[256];
    size_t length;
    while ((length = fread(piece, 1, sizeof(piece), file)) > 0)
        text.append(piece, length);
    fclose(file);
    remove(path);
    DE_TEST_ASSERT(text.find("#beginTestCaseResult dE-IT.self.log") != std::string::npos);
    DE_TEST_ASSERT(text.find("written &amp; read &lt;back&gt;") != std::string::npos);
    DE_TEST_ASSERT(text.find("<Result StatusCode=\"Pass\">Pass</Result>") != std::string::npos);
    DE_TEST_ASSERT(text.find("#endTestCaseResult") != std::string::npos);
}

} // namespace

int main(void)
{
    check("debase.int32", deInt32_selfTest);
    check("debase.float16", deFloat16_selfTest);
    check("debase.math", deMath_selfTest);
    check("debase.sha1", deSha1_selfTest);
    check("debase.memory", deMemory_selfTest);
    check("depool.array", dePoolArray_selfTest);
    check("depool.heap", dePoolHeap_selfTest);
    check("depool.hash", dePoolHash_selfTest);
    check("depool.set", dePoolSet_selfTest);
    check("depool.hash_set", dePoolHashSet_selfTest);
    check("depool.hash_array", dePoolHashArray_selfTest);
    check("depool.multi_set", dePoolMultiSet_selfTest);
    check("dethread.thread", deThread_selfTest);
    check("dethread.mutex", deMutex_selfTest);
    check("dethread.semaphore", deSemaphore_selfTest);
    check("dethread.atomic", deAtomic_selfTest);
    check("dethread.singleton", deSingleton_selfTest);
    check("deutil.command_line", deCommandLine_selfTest);
    check("decpp.block_buffer", de::BlockBuffer_selfTest);
    check("decpp.file_path", de::FilePath_selfTest);
    check("decpp.pool_array", de::PoolArray_selfTest);
    check("decpp.ring_buffer", de::RingBuffer_selfTest);
    check("decpp.shared_ptr", de::SharedPtr_selfTest);
    check("decpp.thread_safe_ring_buffer", de::ThreadSafeRingBuffer_selfTest);
    check("decpp.unique_ptr", de::UniquePtr_selfTest);
    check("decpp.random", de::Random_selfTest);
    check("decpp.commandline", de::cmdline::selfTest);
    check("decpp.array_buffer", de::ArrayBuffer_selfTest);
    check("decpp.string_util", de::StringUtil_selfTest);
    check("decpp.spin_barrier", de::SpinBarrier_selfTest);
    check("decpp.stl_util", de::STLUtil_selfTest);
    check("decpp.append_list", de::AppendList_selfTest);
    check("qphelper.test_log", testLogSelfTest);
    printf("framework self-tests: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
