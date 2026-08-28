# py.ps1: resolve the Python that has the debug-suite dependencies, and run it.
#
# A machine can have several interpreters with the useful one not on PATH.
# msys2's C:\msys64\mingw64\bin\python.exe in particular has no pip and a mingw
# ABI that PyPI wheels do not match, so PyBoy cannot be installed for it. This
# script finds one that can import pyboy and runs the script with it.
#
# Usage:
#   pwsh tools/py.ps1 tools/debugsuite/brun.py ...
#   pwsh tools/py.ps1 tools/game_cli.py state
#
# The alternative, one-time and needing no admin, is to put a real CPython on
# PATH, after which this shim is redundant:
#   setx PATH "$env:PATH;C:\Program Files\Python311;C:\Program Files\Python311\Scripts"

$ErrorActionPreference = 'Stop'

$candidates = @(
    'C:\Program Files\Python311\python.exe',
    'C:\Program Files\Python312\python.exe',
    'C:\Program Files\Python313\python.exe',
    "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"
)

# An interpreter on PATH wins if it can actually import pyboy: this shim then
# has nothing to add.
$onPath = (Get-Command python -ErrorAction SilentlyContinue).Source
if ($onPath) { $candidates = , $onPath + $candidates }

$py = $null
foreach ($c in $candidates) {
    if (-not (Test-Path $c)) { continue }
    & $c -W ignore -c "import pyboy" 2>$null
    if ($LASTEXITCODE -eq 0) { $py = $c; break }
    if (-not $py) { $fallback = $c }   # runs Python, just lacks pyboy
}
if (-not $py) { $py = $fallback }

if (-not $py) {
    Write-Error @"
No usable Python found.

Install CPython 3.11+ (python.org), then:
    & "C:\Program Files\Python311\python.exe" -m pip install --user pyboy numpy pillow

Do NOT use C:\msys64\mingw64\bin\python.exe -- it has no pip and its mingw ABI
does not match the PyPI wheels PyBoy ships.
"@
    exit 1
}

& $py @args
exit $LASTEXITCODE
