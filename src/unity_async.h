/* =========================================================================
    Unity - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-26 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
========================================================================= */

/* Async test runner for Unity.
 *
 * For use on cyclic platforms where the function under test is driven by an
 * OS scheduler rather than called synchronously. Build with
 * UNITY_EXCLUDE_SETJMP_H defined: this switches TEST_ABORT() from longjmp to
 * a plain return, so assertion failures propagate via the CurrentTestFailed
 * flag instead of unwinding the stack.
 *
 * Usage:
 *
 *   1. Call UnityBegin() once at the start of the test suite.
 *   2. Call UNITY_ASYNC_TEST_BEGIN("test_name", __LINE__) to start a test.
 *   3. Each cycle, call your test state machine function. Assertions inside it
 *      work normally: a failure sets CurrentTestFailed and returns from the
 *      assertion function. Use UNITY_ASYNC_TEST_FAILED() to check whether to
 *      cut the test short.
 *   4. When the test is complete (pass or fail), call UNITY_ASYNC_TEST_END().
 *   5. Repeat steps 2-4 for each test, then call UnityEnd().
 */

#ifndef UNITY_ASYNC_H
#define UNITY_ASYNC_H

#ifndef UNITY_EXCLUDE_SETJMP_H
#error "unity_async.h requires UNITY_EXCLUDE_SETJMP_H to be defined"
#endif

#include "unity.h"

/* Begin an async test. Call once when starting a new test. */
#define UNITY_ASYNC_TEST_BEGIN(name, line)          \
    do {                                            \
        Unity.CurrentTestName       = (name);       \
        Unity.CurrentTestLineNumber = (UNITY_LINE_TYPE)(line); \
        Unity.NumberOfTests++;                      \
        Unity.CurrentTestFailed     = 0;            \
        Unity.CurrentTestIgnored    = 0;            \
        UNITY_CLR_DETAILS();                        \
        UNITY_EXEC_TIME_START();                    \
    } while (0)

/* Conclude an async test. Call once when the test is complete. */
#define UNITY_ASYNC_TEST_END()      \
    do {                            \
        UNITY_EXEC_TIME_STOP();     \
        UnityConcludeTest();        \
    } while (0)

/* Returns non-zero if the current test has already failed.
 * Useful for bailing out of a state machine early. */
#define UNITY_ASYNC_TEST_FAILED() (Unity.CurrentTestFailed != 0)

#endif /* UNITY_ASYNC_H */
