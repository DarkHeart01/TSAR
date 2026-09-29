@echo off
echo [*] Building payload.exe...

cl /nologo /O2 /GS- /MT payload.cpp ^
   /link ^
   /SUBSYSTEM:WINDOWS ^
   /ENTRY:payload_entry ^
   /NODEFAULTLIB ^
   /OUT:payload.exe ^
   ws2_32.lib kernel32.lib user32.lib

if %ERRORLEVEL% NEQ 0 (
    echo [-] Build FAILED
    exit /b 1
)

echo [+] payload.exe built

REM Verify entry point is set correctly
echo [*] Verifying entry point...
dumpbin /headers payload.exe | findstr "entry point"

echo [+] 