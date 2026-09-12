# Wrapper for a scheduled (Task Scheduler) nightly run. Two independent
# checks, both logged, neither skipped because the other failed:
#
#   1. anastasis-unreal.ps1 verify   - build + dedicated editor + PIE smoke
#                                       (DEBUG markers, module origin). ~4+ min.
#   2. report-tests.ps1              - the full `Anastasis` Automation suite,
#                                       classified PASS / KNOWN_EXPECTED_FAILURE
#                                       / FAIL against known-expected-failures.txt
#                                       (a bare "Success" count would silently
#                                       fold marked divergences into real
#                                       passes). Up to ~15 min.
#
# Not a pre-push gate — see tools/git-hooks/pre-push for the fast compile-only
# gate that runs on every push.
#
# Appends to Saved/CanonicalVerification/scheduled-verify.log: one VERIFY_*
# line, the full report-tests.ps1 output, then one TESTS_* line, per run.
# (latest.json is overwritten each verify run by anastasis-unreal.ps1 and is
# not history; this log is the append-only record across runs.)
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Log = Join-Path $Root 'Saved/CanonicalVerification/scheduled-verify.log'
New-Item -ItemType Directory -Force (Split-Path $Log) | Out-Null

$bVerifyOk = $true
$stamp = Get-Date -Format 'o'
try {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'anastasis-unreal.ps1') verify
    if ($LASTEXITCODE -ne 0) { throw "verify exited $LASTEXITCODE" }
    "$stamp VERIFY_PASS" | Add-Content $Log
} catch {
    "$stamp VERIFY_FAIL $($_.Exception.Message)" | Add-Content $Log
    $bVerifyOk = $false
}

$bTestsOk = $true
$stamp = Get-Date -Format 'o'
try {
    $reportOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'report-tests.ps1') 2>&1
    $reportExit = $LASTEXITCODE
    $reportOutput | Out-String | Add-Content $Log
    if ($reportExit -ne 0) { throw "report-tests exited $reportExit" }
    "$stamp TESTS_PASS" | Add-Content $Log
} catch {
    "$stamp TESTS_FAIL $($_.Exception.Message)" | Add-Content $Log
    $bTestsOk = $false
}

if (-not ($bVerifyOk -and $bTestsOk)) { exit 1 }
