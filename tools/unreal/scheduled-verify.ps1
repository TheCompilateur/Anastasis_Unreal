# Wrapper for a scheduled (Task Scheduler) run of the full canonical verify
# (build + dedicated editor + PIE smoke). ~4+ min; not meant as a pre-push
# gate — see tools/git-hooks/pre-push for the fast compile-only gate.
#
# Appends one line per run to Saved/CanonicalVerification/scheduled-verify.log
# (latest.json is overwritten each run by anastasis-unreal.ps1 and is not history).
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Log = Join-Path $Root 'Saved/CanonicalVerification/scheduled-verify.log'
New-Item -ItemType Directory -Force (Split-Path $Log) | Out-Null

$stamp = Get-Date -Format 'o'
try {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'anastasis-unreal.ps1') verify
    if ($LASTEXITCODE -ne 0) { throw "verify exited $LASTEXITCODE" }
    "$stamp PASS" | Add-Content $Log
} catch {
    "$stamp FAIL $($_.Exception.Message)" | Add-Content $Log
    throw
}
