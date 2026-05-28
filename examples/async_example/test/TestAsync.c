/* Example usage of the ASYNC_* state-machine test API.
 *
 * This file shows a minimal PLC-style cyclic test function.
 * Build with UNITY_EXCLUDE_SETJMP_H defined.
 *
 * Expanded macro output is shown in comments so the structure is clear.
 */

#define UNITY_EXCLUDE_SETJMP_H
#include "unity_async.h"

/* Simulated global tick counter (milliseconds). */
unsigned long asyncTick = 0;

/* Simulated signals from the calling environment. */
static int  sfcEnable  = 0;
static int  sfcRunning = 0;
static int  sfcDone    = 0;
static int  sfcOutput  = 0;

/* State variable owned by the test function. */
static int state = 0;

/* prevStart tracks the rising edge of the start signal. */
static int prevStart = 0;

/* ---------------------------------------------------------------------------
 * sfcControlModeTest — called once per PLC cycle
 * ---------------------------------------------------------------------------
 * Macro expansions are annotated inline so you can follow the structure
 * without running the preprocessor.
 * --------------------------------------------------------------------------- */
void sfcControlModeTest(int start, int cancel)
{
    /* Declares:
     *   static int          _asyncPhase      = 1;
     *   static int          _asyncNextCaseState = 0;
     *   static UNITY_UINT32 _asyncPhaseEntryTick = 0u;
     *   static int          _asyncTestBegun  = 0;
     */
    ASYNC_DECLARE_SUITE_VARS();

    /* Rising edge: start the suite. */
    if (start && !prevStart)
    {
        UnityBegin("sfcControlModeTest");

        /* Expands to:
         *   state = ASYNC_TEARDOWN_STATE (0x7FFE);
         *   _asyncPhase = 1; _asyncNextCaseState = 0; ...
         */
        ASYNC_SUITE_BEGIN();

        sfcEnable  = 0;
        sfcRunning = 0;
        sfcDone    = 0;
        sfcOutput  = 0;
    }

    switch (state)
    {
        /* ----------------------------------------------------------------
         * Global teardown — runs between every test case.
         * Expands to:
         *   case 0x7FFE:
         *       Unity.CurrentTestFailed = 0; Unity.CurrentTestIgnored = 0;
         *       if (0) {
         * ---------------------------------------------------------------- */
        ASYNC_SUITE_TEARDOWN;

            /* Expands to:
             *   } if (_asyncPhase == 1 && !UNITY_ASYNC_TEST_FAILED()) {
             */
            ASYNC_TEST_PHASE(1);
                sfcEnable  = 0;
                sfcRunning = 0;
                sfcDone    = 0;
                sfcOutput  = 0;

            /* Expands to:
             *   state = ASYNC_SETUP_STATE; _asyncPhase = 1; ... break;
             *   } break;
             */
            ASYNC_SUITE_TEARDOWN_DONE();

        /* ----------------------------------------------------------------
         * Global setup — runs between teardown and every test case.
         * ---------------------------------------------------------------- */
        ASYNC_SUITE_SETUP;

            ASYNC_TEST_PHASE(1);
                sfcEnable = 1;

            ASYNC_TEST_PHASE(2);
                /* Wait for the SFC to acknowledge enable. */
                if (sfcRunning)
                {
                    /* Expands to:
                     *   _asyncPhase = 3; _asyncPhaseEntryTick = asyncTick; break;
                     */
                    ASYNC_TEST_GO_TO(3);
                }
                /* Expands to:
                 *   if ((asyncTick - _asyncPhaseEntryTick) >= 200u) {
                 *       TEST_FAIL_MESSAGE("Async timeout"); break; }
                 */
                ASYNC_TEST_TIMEOUT(200);

            ASYNC_TEST_PHASE(3);
                /* SFC is running — ready for tests. */
                ASYNC_SUITE_SETUP_DONE();

        /* ================================================================
         * Test case 0: output should be 0 on first enable
         *
         * Expands to:
         *   case 0:   (first __COUNTER__ value)
         *       if (!_asyncTestBegun) {
         *           UNITY_ASYNC_TEST_BEGIN("outputClearedOnEnable", __LINE__);
         *           _asyncTestBegun = 1; _asyncPhaseEntryTick = asyncTick;
         *       }
         *       if (0) {
         * ================================================================ */
        ASYNC_TEST_CASE("outputClearedOnEnable");

            ASYNC_TEST_PHASE(1);
                TEST_ASSERT_EQUAL_INT(0, sfcOutput);

                /* Expands to:
                 *   UNITY_ASYNC_TEST_END(); _asyncNextCaseState = state+1;
                 *   state = ASYNC_TEARDOWN_STATE; _asyncPhase = 1; ... break;
                 *   } if (UNITY_ASYNC_TEST_FAILED()) { ... } break;
                 */
                ASYNC_TEST_SUCCESS();

        /* ================================================================
         * Test case 1: output goes high within 500 ms of a trigger
         * ================================================================ */
        ASYNC_TEST_CASE("outputRisesOnTrigger");

            ASYNC_TEST_PHASE(1);
                sfcDone = 1;
                ASYNC_TEST_GO_TO(2);

            ASYNC_TEST_PHASE(2);
                if (sfcOutput)
                {
                    ASYNC_TEST_GO_TO(3);
                }
                ASYNC_TEST_TIMEOUT(500);

            ASYNC_TEST_PHASE(3);
                TEST_ASSERT_EQUAL_INT(1, sfcOutput);
                ASYNC_TEST_SUCCESS();

        /* ================================================================
         * Test case 2: multiple assertions in one phase — if the first
         * fails, Unity's RETURN_IF_FAIL_OR_IGNORE silently skips the rest.
         * ================================================================ */
        ASYNC_TEST_CASE("multipleAssertions");

            ASYNC_TEST_PHASE(1);
                TEST_ASSERT_EQUAL_INT(1, sfcRunning);
                TEST_ASSERT_EQUAL_INT(1, sfcOutput);
                TEST_ASSERT_EQUAL_INT(0, sfcEnable); /* will fail if sfcEnable is 1 */
                ASYNC_TEST_SUCCESS();

        /* ----------------------------------------------------------------
         * Suite termination.
         * Expands to:
         *   default: (void)UnityEnd(); state = ASYNC_DONE_STATE; break;
         *   case ASYNC_DONE_STATE: break;
         * ---------------------------------------------------------------- */
        ASYNC_SUITE_DONE();
    }

    prevStart = start;
}
