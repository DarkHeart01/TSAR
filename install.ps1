#Requires -RunAsAdministrator
<#
.SYNOPSIS
    JOCKY Framework — one-line installer
.DESCRIPTION
    iwr https://raw.githubusercontent.com/DarkHeart01/TSAR/main/install.ps1 | iex
#>

$ErrorActionPreference = "Stop"

$BANNER = @"

       __  ____  ______ __ __ __  __
      / / / __ \/ ____// //_// / / /
 __  / / / / / / /    / ,<  / /_/ /
/ /_/ / / /_/ / /___  / /| |/ __  /
\____/  \____/\____/ /_/ |_/_/ /_/

  JOCKY Framework Installer
  github.com/DarkHeart01/TSAR

"@

Write-Host $BANNER -ForegroundColor Cyan

# ── Helper ────────────────────────────────────────────────────────────────────

function Check-OK  { Write-Host "  [+] $args" -ForegroundColor Green }
function Check-Warn { Write-Host "  [!] $args" -ForegroundColor Yellow }
function Check-Fail { Write-Host "  [-] $args" -ForegroundColor Red; exit 1 }
function Section   { Write-Host "`n--- $args ---" -ForegroundColor Cyan }

# ── 1. Python ─────────────────────────────────────────────────────────────────

Section "Checking Python"

$py = $null
foreach ($cmd in @("python", "python3")) {
    try {
        $ver = & $cmd --version 2>&1
        if ($ver -match "Python 3\.(\d+)") {
            if ([int]$Matches[1] -ge 9) { $py = $cmd; break }
        }
    } catch {}
}

if (-not $py) {
    Check-Warn "Python 3.9+ not found — downloading installer..."
    $pyInstaller = "$env:TEMP\python-installer.exe"
    Invoke-WebRequest "https://www.python.org/ftp/python/3.12.4/python-3.12.4-amd64.exe" -OutFile $pyInstaller
    Start-Process $pyInstaller -ArgumentList "/quiet InstallAllUsers=1 PrependPath=1" -Wait
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
    $py = "python"
    Check-OK "Python installed"
} else {
    $ver = & $py --version 2>&1
    Check-OK "$ver found"
}

# ── 2. Repo location ──────────────────────────────────────────────────────────

Section "Locating repository"

# If running via iex (piped), clone the repo; if already inside the repo, use CWD
$repoRoot = $null

if (Test-Path "$PSScriptRoot\jocky-framework") {
    $repoRoot = $PSScriptRoot
    Check-OK "Running from repo: $repoRoot"
} elseif (Test-Path ".\jocky-framework") {
    $repoRoot = (Get-Location).Path
    Check-OK "Repo found at: $repoRoot"
} else {
    $installDir = "$env:USERPROFILE\TSAR"
    Check-Warn "Cloning repo to $installDir ..."
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Check-Fail "git not found. Install Git from https://git-scm.com and re-run."
    }
    git clone https://github.com/DarkHeart01/TSAR.git $installDir
    $repoRoot = $installDir
    Check-OK "Cloned to $repoRoot"
}

# ── 3. Python dependencies ────────────────────────────────────────────────────

Section "Installing Python dependencies"

& $py -m pip install --quiet --upgrade pip
& $py -m pip install --quiet -r "$repoRoot\jocky-framework\client\requirements.txt"
& $py -m pip install --quiet -r "$repoRoot\jocky-framework\server\requirements.txt"
& $py -m pip install --quiet pycryptodome
Check-OK "Python deps installed"

# ── 4. MSVC check ─────────────────────────────────────────────────────────────

Section "Checking MSVC (needed for build server linker)"

$clExe = Get-Command cl.exe -ErrorAction SilentlyContinue
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if ($clExe) {
    Check-OK "cl.exe found: $($clExe.Source)"
} elseif (Test-Path $vcvars) {
    Check-OK "MSVC BuildTools found (not in PATH — build.bat will activate it)"
} else {
    Check-Warn "MSVC not found."
    Check-Warn "Build server requires MSVC to link agent binaries."
    Check-Warn "Install from: https://aka.ms/vs/17/release/vs_BuildTools.exe"
    Check-Warn "Select: C++ build tools + Windows 10/11 SDK"
    Check-Warn "Skipping — CLI-only mode will still work against a remote build server."
}

# ── 5. Build jocky.exe (compiler driver) ─────────────────────────────────────

Section "Building jocky compiler driver"

$jockyExe  = "$repoRoot\jocky-framework\compiler\jocky\driver\jocky.exe"
$jockySrc  = "$repoRoot\jocky-framework\compiler\jocky\driver\jocky.cpp"

if (Test-Path $jockyExe) {
    Check-OK "jocky.exe already present"
} elseif (Test-Path $vcvars) {
    Check-Warn "Compiling jocky.exe from source..."
    $build = "call `"$vcvars`" && cl /nologo /O2 /MT `"$jockySrc`" /link /SUBSYSTEM:CONSOLE /OUT:`"$jockyExe`" kernel32.lib advapi32.lib"
    cmd /c $build
    if (Test-Path $jockyExe) { Check-OK "jocky.exe compiled" }
    else { Check-Warn "jocky.exe compile failed — build server may not work" }
} else {
    Check-Warn "MSVC not found, cannot compile jocky.exe — skipping"
}

# ── 6. Config ─────────────────────────────────────────────────────────────────

Section "Configuration"

$envFile = "$repoRoot\endpoint-management-server\.env"
$envExample = "$repoRoot\endpoint-management-server\.env.example"

if (-not (Test-Path $envFile)) {
    Write-Host ""
    Write-Host "  Enter C2 server details (leave blank to configure later):" -ForegroundColor Yellow
    $c2Host    = Read-Host "  C2 host IP or domain"
    $attackerIp = Read-Host "  Attacker IP (reverse shell callback)"
    $aesKey    = Read-Host "  AES key (64 hex chars, blank = use default dev key)"

    if (-not $aesKey) {
        $aesKey = "6a6f636b795f6465765f6165735f6b65795f6a6f636b795f6465765f6165736b"
    }
    if (-not $c2Host)     { $c2Host = "127.0.0.1" }
    if (-not $attackerIp) { $attackerIp = $c2Host }

    @"
JOCKY_C2_HOST=$c2Host
JOCKY_ATTACKER_IP=$attackerIp
JOCKY_AES_KEY=$aesKey
"@ | Set-Content $envFile -Encoding UTF8
    Check-OK ".env written"
} else {
    Check-OK ".env already exists"
}

# ── 7. Create launcher scripts ────────────────────────────────────────────────

Section "Creating launchers"

# jocky-cli.ps1
@"
`$env:JOCKY_TOKENS = "demotoken:operator"
Set-Location "$repoRoot\jocky-framework\client"
python -m jocky_client
"@ | Set-Content "$repoRoot\jocky-cli.ps1" -Encoding UTF8

# jocky-server.ps1
@"
`$env:JOCKY_TOKENS = "demotoken:operator"
Set-Location "$repoRoot\jocky-framework"
python -m uvicorn server.app.main:app --host 0.0.0.0 --port 8001
"@ | Set-Content "$repoRoot\jocky-server.ps1" -Encoding UTF8

Check-OK "jocky-cli.ps1    — starts the operator CLI"
Check-OK "jocky-server.ps1 — starts the local build server"

# ── Done ──────────────────────────────────────────────────────────────────────

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  JOCKY installed successfully" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Start build server:   .\jocky-server.ps1" -ForegroundColor White
Write-Host "  Start operator CLI:   .\jocky-cli.ps1" -ForegroundColor White
Write-Host ""
Write-Host "  Then inside the CLI:" -ForegroundColor Gray
Write-Host "    jocky > connect  --addr localhost:8001 --token demotoken" -ForegroundColor Gray
Write-Host "    jocky > c2connect --addr <C2_IP>:443 --user admin --pass <pass>" -ForegroundColor Gray
Write-Host "    jocky > deploy   --template jocky-framework\templates\jocky_agent_full.cpp ..." -ForegroundColor Gray
Write-Host ""
