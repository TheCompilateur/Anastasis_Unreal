# Automation startup diagnosis — 2026-09-12

CLASSIFICATION: ENGINE_NOISE, caused by culture-sensitive engine smoke-test expectations (CONFIGURATION sensitivity). Not an ANASTASIS test failure. No engine/project semantic patch applied.

Same project, engine and sources:
- Default French Editor session: 20 `Condition failed` entries, attributed with `-LogCmds="LogAutomationTest Log"` to FUnifiedErrorTest_CreateErrorMessage, FUnifiedErrorTest_CreateErrorMessageWithContext, FStructuredLogFormatTest.
- Dedicated session with process-only `-culture=en`: all three Success, zero `Condition failed`; map/PIE smoke also completes.

Engine sources: Runtime/Core/Private/Misc/AutomationTest.cpp:1243-1266 dumps test names at Log verbosity, errors separately. Runtime/Core/Public/Misc/LowLevelTestAdapter.h:130 emits the generic CHECK failure. Runtime/Core/Tests/Experimental/UnifiedError/UnifiedErrorTests.cpp:479-534 compares formatted text against fixed strings. The A/B result establishes culture dependence; it does not identify each failed expression.

No persistent locale change, error suppression, test disabling or engine modification. The operator retains the user's default locale and captures named test results. Debt: upstream culture-independent engine tests; a full project test campaign remains outside this seal.
