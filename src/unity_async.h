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
 * This header provides two APIs:
 *
 *   Low-level API (UNITY_ASYNC_TEST_*)
 *   -----------------------------------
 *   Thin wrappers around Unity internals for manual async test management.
 *
 *     1. Call UnityBegin() once at the start of the test suite.
 *     2. Call UNITY_ASYNC_TEST_BEGIN("test_name", __LINE__) to start a test.
 *     3. Each cycle, run your test logic. Assertions work normally: failure
 *        sets CurrentTestFailed and returns from the assertion helper.
 *        Unity's RETURN_IF_FAIL_OR_IGNORE then silently skips all subsequent
 *        assertions in that same call stack.  Check UNITY_ASYNC_TEST_FAILED()
 *        to cut the test short at higher levels.
 *     4. When done (pass or fail), call UNITY_ASYNC_TEST_END().
 *     5. Repeat for each test, then call UnityEnd().
 *
 *   State-machine API (ASYNC_*)
 *   ----------------------------
 *   Higher-level macros that build a complete switch-based test suite with
 *   auto-numbered test cases, multi-cycle phases, timeouts, and global
 *   setup/teardown.
 *
 *   Quick-start:
 *     1. Build with UNITY_EXCLUDE_SETJMP_H defined.
 *     2. Call UnityBegin() once when the suite starts.
 *     3. Declare framework variables at the top of your test function:
 *          ASYNC_DECLARE_SUITE_VARS();
 *     4. On your start edge, call ASYNC_SUITE_BEGIN().
 *     5. Inside your cyclic switch(testCase) block:
 *          ASYNC_SUITE_TEARDOWN ... ASYNC_SUITE_TEARDOWN_DONE()
 *          ASYNC_SUITE_SETUP    ... ASYNC_SUITE_SETUP_DONE()
 *          ASYNC_TEST_CASE("name") ... ASYNC_TEST_DONE()   (repeat)
 *          ASYNC_SUITE_DONE()
 *     6. If using cancellation, on your cancel edge, call ASYNC_SUITE_CANCEL()
 *
 *   Assertions: use standard TEST_ASSERT_* macros directly inside phases.
 *   After the first failure Unity silently skips subsequent assertions
 *   (RETURN_IF_FAIL_OR_IGNORE) until the next test case begins.
 *
 *   Variable name overrides (define before including this header):
 *     ASYNC_CASE_VAR - test case variable (default: asyncTestCase)
 *     ASYNC_PHASE_VAR - phase-within-case variable (default: asyncTestPhase)
 *     ASYNC_TICK_VAR - global millisecond counter (default: asyncTick)
 *
 *   Inside phases, use:
 *     ASYNC_TEST_GO_TO(n)          - advance to phase n immediately
 *     ASYNC_WAIT_FOR(cond, ms, n)  - wait for cond, advance to phase n, fail on timeout
 *     ASYNC_TEST_TIMEOUT(ms)       - fail if the current phase exceeds ms ticks
 *
 *   Constraint: do not use __COUNTER__ elsewhere in the same translation unit.
 *   ASYNC_TEST_CASE relies on it for compile-time case-label assignment.
 */

#ifndef UNITY_ASYNC_H
#define UNITY_ASYNC_H

#ifndef UNITY_EXCLUDE_SETJMP_H
#error "unity_async.h requires UNITY_EXCLUDE_SETJMP_H to be defined"
#endif

#include "unity.h"

/* =========================================================================
 * Low-level async API
 * ========================================================================= */

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

/* Returns non-zero if the current test has already failed. */
#define UNITY_ASYNC_TEST_FAILED() (Unity.CurrentTestFailed != 0)

/* =========================================================================
 * State-machine API — configurable names
 * ========================================================================= */

#ifndef ASYNC_CASE_VAR
#define ASYNC_CASE_VAR asyncTestCase
#endif

#ifndef ASYNC_PHASE_VAR
#define ASYNC_PHASE_VAR asyncTestPhase
#endif

#ifndef ASYNC_TICK_VAR
#define ASYNC_TICK_VAR asyncTick
#endif

/* =========================================================================
 * State-machine API — reserved state constants used for asyncTestCase
 *
 * These values are well above any realistic __COUNTER__-based test case
 * number.  Do not use them as explicit state values in your own code.
 * ========================================================================= */

#define ASYNC_TEARDOWN_STATE 0x7FFE
#define ASYNC_SETUP_STATE    0x7FFD
#define ASYNC_DONE_STATE     0x7FFC

/* =========================================================================
 * State-machine API — framework variable declarations
 *
 * Place once at the top of the test function (before the switch).
 * ASYNC_CASE_VAR and ASYNC_PHASE_VAR are owned by the caller and must be
 * declared as 32 bit ints.
 * ASYNC_TICK_VAR must be a globally accessible unsigned 32-bit counter.
 * ========================================================================= */

#define ASYNC_DECLARE_SUITE_VARS()                               \
    static int          _asyncNextCaseState  = 0;               \
    static UNITY_UINT32 _asyncPhaseEntryTick = 0u;              \
    static int          _asyncTestBegun      = 0

/* =========================================================================
 * State-machine API — suite initialisation
 *
 * Call once on the rising edge that starts the test suite.
 * The suite begins in TEARDOWN so the teardown block always runs before the
 * first test, giving setup/teardown symmetric behaviour.
 * ========================================================================= */

#define ASYNC_SUITE_BEGIN()                                      \
    do {                                                         \
        ASYNC_CASE_VAR      = ASYNC_SETUP_STATE;                 \
        ASYNC_PHASE_VAR      = 1;                                \
        _asyncNextCaseState  = 0;                                \
        _asyncPhaseEntryTick = 0u;                               \
        _asyncTestBegun      = 0;                                \
    } while (0)

/* =========================================================================
 * State-machine API — phase transitions
 *
 * ASYNC_TEST_GO_TO and ASYNC_TEST_TIMEOUT do NOT use do{}while(0) because
 * they must be able to break out of the enclosing switch statement.
 * Always terminate them with a semicolon at the call site.
 * ========================================================================= */

/* Transition to phase n and reset the timeout window. */
#define ASYNC_TEST_GO_TO(n)                                      \
    ASYNC_PHASE_VAR      = (n);                                  \
    _asyncPhaseEntryTick = (UNITY_UINT32)(ASYNC_TICK_VAR);       \
    break

/* Fail the test if the current phase has been running for >= ms ticks.
 * Must not be the sole body of an unbraced if/else. */
#define ASYNC_TEST_TIMEOUT(ms)                                   \
    if (((UNITY_UINT32)(ASYNC_TICK_VAR) - _asyncPhaseEntryTick)  \
            >= (UNITY_UINT32)(ms))                               \
    {                                                            \
        TEST_FAIL_MESSAGE("Async timeout");                      \
        break;                                                   \
    }

/* Wait until cond is true, then advance to phase next.
 * Fail the test if cond has not become true within ms ticks.
 * Combines the common if-check + GO_TO + TIMEOUT pattern into one line.
 * Must not be the sole body of an unbraced if/else. */
#define ASYNC_WAIT_FOR(cond, ms, next)               \
    if (cond) { ASYNC_TEST_GO_TO(next); }            \
    ASYNC_TEST_TIMEOUT(ms)

/* =========================================================================
 * State-machine API — global setup/teardown blocks
 *
 * Place ASYNC_SUITE_TEARDOWN and ASYNC_SUITE_SETUP inside the switch,
 * before any test cases.  Each block must contain at least one
 * ASYNC_TEST_PHASE and must end with the matching _DONE macro.
 *
 * The dangling-brace pattern is used throughout: each _SETUP/_TEARDOWN
 * macro opens an if(0){ that is closed by the first ASYNC_TEST_PHASE.
 * Each ASYNC_TEST_PHASE closes the previous block and opens the next.
 * The _DONE macros close the final block.
 * ========================================================================= */

#define ASYNC_SUITE_TEARDOWN                                     \
    case ASYNC_TEARDOWN_STATE:                                   \
        Unity.CurrentTestFailed  = 0;                           \
        Unity.CurrentTestIgnored = 0;                           \
        if (0) {

#define ASYNC_SUITE_TEARDOWN_DONE()                              \
        ASYNC_CASE_VAR      = ASYNC_SETUP_STATE;                \
        ASYNC_PHASE_VAR      = 1;                                \
        _asyncPhaseEntryTick = (UNITY_UINT32)(ASYNC_TICK_VAR);   \
        break;                                                   \
    }                                                            \
    break

#define ASYNC_SUITE_SETUP                                        \
    case ASYNC_SETUP_STATE:                                      \
        Unity.CurrentTestFailed  = 0;                           \
        Unity.CurrentTestIgnored = 0;                           \
        if (0) {

#define ASYNC_SUITE_SETUP_DONE()                                 \
        ASYNC_CASE_VAR      = _asyncNextCaseState;              \
        ASYNC_PHASE_VAR      = 1;                                \
        _asyncPhaseEntryTick = (UNITY_UINT32)(ASYNC_TICK_VAR);   \
        _asyncTestBegun      = 0;                                \
        break;                                                   \
    }                                                            \
    break

/* =========================================================================
 * State-machine API — test cases
 *
 * ASYNC_TEST_CASE opens a test case.  __COUNTER__ assigns sequential state
 * numbers (0, 1, 2 ...) automatically.
 *
 * ASYNC_TEST_PHASE opens a phase block (closing the previous one).
 * The block is skipped automatically if an assertion has already failed.
 *
 * ASYNC_TEST_DONE closes the final phase, records a pass, and transitions
 * to teardown.  It also provides a failure handler that fires when all phase
 * blocks were skipped due to an earlier assertion failure.
 * ========================================================================= */

#define ASYNC_TEST_CASE(name)                                    \
    case __COUNTER__:                                            \
        if (!_asyncTestBegun) {                                  \
            UNITY_ASYNC_TEST_BEGIN((name), __LINE__);            \
            _asyncTestBegun      = 1;                            \
            _asyncPhaseEntryTick = (UNITY_UINT32)(ASYNC_TICK_VAR); \
        }                                                        \
        if (0) {

#define ASYNC_TEST_PHASE(n)                                      \
    }                                                            \
    if (ASYNC_PHASE_VAR == (n) && !UNITY_ASYNC_TEST_FAILED()) {

#define ASYNC_TEST_DONE()                                     \
        UNITY_ASYNC_TEST_END();                                  \
        _asyncNextCaseState  = ASYNC_CASE_VAR + 1;              \
        ASYNC_CASE_VAR      = ASYNC_TEARDOWN_STATE;             \
        ASYNC_PHASE_VAR      = 1;                                \
        _asyncPhaseEntryTick = (UNITY_UINT32)(ASYNC_TICK_VAR);   \
        _asyncTestBegun      = 0;                                \
        break;                                                   \
    }                                                            \
    if (UNITY_ASYNC_TEST_FAILED()) {                             \
        UNITY_ASYNC_TEST_END();                                  \
        _asyncNextCaseState  = ASYNC_CASE_VAR + 1;              \
        ASYNC_CASE_VAR      = ASYNC_TEARDOWN_STATE;             \
        ASYNC_PHASE_VAR      = 1;                                \
        _asyncPhaseEntryTick = (UNITY_UINT32)(ASYNC_TICK_VAR);   \
        _asyncTestBegun      = 0;                                \
    }                                                            \
    break

/* =========================================================================
 * State-machine API — suite termination
 *
 * Place at the end of the switch, after all test cases.
 * Calls UnityEnd() once when all cases have finished, then idles.
 * ========================================================================= */

#define ASYNC_SUITE_DONE()                                       \
    default:                                                     \
        (void)UnityEnd();                                        \
        ASYNC_CASE_VAR = ASYNC_DONE_STATE;                      \
        break;                                                   \
    case ASYNC_DONE_STATE:                                       \
        break

/* =========================================================================
 * State-machine API — suite cancellation
 * ========================================================================= */

#define ASYNC_SUITE_CANCEL()                                       \
        (void)UnityEnd();                                        \
        ASYNC_CASE_VAR = ASYNC_DONE_STATE;

#endif /* UNITY_ASYNC_H */
