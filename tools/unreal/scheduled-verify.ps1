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
# Appends to Saved/CanonicalVerification/scheduled-verify.log: start cwd/root,
# verify output tail on failure (the inner VERIFY::FAIL/BUILD::FAIL reason),
# one VERIFY_* line, the full report-tests.ps1 output, then one TESTS_* line.
# (latest.json is overwritten each verify run by anastasis-unreal.ps1 and is
# not history; this log is the append-only record across runs.)
#
# Task Scheduler starts this process in System32. Both this wrapper and
# anastasis-unreal.ps1 Set-Location to the project root before work.
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
Set-Location -LiteralPath $Root
$Log = Join-Path $Root 'Saved/CanonicalVerification/scheduled-verify.log'
New-Item -ItemType Directory -Force (Split-Path $Log) | Out-Null

$bVerifyOk = $true
$stamp = Get-Date -Format 'o'
"$stamp START cwd=$(Get-Location) root=$Root" | Add-Content $Log
try {
    $verifyOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'anastasis-unreal.ps1') verify 2>&1
    $verifyExit = $LASTEXITCODE
    if ($verifyExit -ne 0) {
        ($verifyOutput | Select-Object -Last 60 | Out-String) | Add-Content $Log
        throw "verify exited $verifyExit"
    }
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
