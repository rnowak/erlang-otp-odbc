@echo off
REM Build or fetch odbcserver.exe for Windows.
REM
REM Strategy:
REM   1. If priv\bin\odbcserver.exe already exists, do nothing.
REM   2. Try to download a prebuilt binary from GitHub Releases.
REM   3. If download fails, try to build locally with MSVC (cl.exe).
REM   4. Fail with a helpful message if neither works.
REM
REM Environment variables (optional):
REM   ODBC_PREBUILT_REPO  - GitHub repo (default: rnowak/erlang-otp-odbc)
REM   ODBC_PREBUILT_VSN   - Version to download (default: read from app.src)

setlocal enabledelayedexpansion

set SCRIPTDIR=%~dp0
set BASEDIR=%SCRIPTDIR%..
set PRIVDIR=%BASEDIR%\priv
set BINDIR=%PRIVDIR%\bin
set DEST=%BINDIR%\odbcserver.exe

REM --- Already built? ---
if exist "%DEST%" (
    echo odbcserver.exe already exists, skipping build.
    exit /b 0
)

REM --- Configuration ---
if "%ODBC_PREBUILT_REPO%"=="" set ODBC_PREBUILT_REPO=rnowak/erlang-otp-odbc
if "%ODBC_PREBUILT_VSN%"=="" (
    for /f "tokens=2 delims=," %%i in ('findstr /C:"vsn" "%BASEDIR%\src\odbc.app.src"') do (
        set RAW=%%i
        set ODBC_PREBUILT_VSN=!RAW: =!
        set ODBC_PREBUILT_VSN=!ODBC_PREBUILT_VSN:"=!
        set ODBC_PREBUILT_VSN=!ODBC_PREBUILT_VSN:}=!
    )
)

set FILENAME=odbcserver-%ODBC_PREBUILT_VSN%-win64.exe
set URL=https://github.com/%ODBC_PREBUILT_REPO%/releases/download/v%ODBC_PREBUILT_VSN%/%FILENAME%

REM --- Try downloading prebuilt binary ---
echo Attempting to download prebuilt binary ...
echo   URL: %URL%

if not exist "%BINDIR%" mkdir "%BINDIR%"

REM Try curl first (available on Windows 10+)
where curl >nul 2>&1
if %ERRORLEVEL% equ 0 (
    curl -fSL --retry 2 -o "%DEST%" "%URL%" 2>nul
    if !ERRORLEVEL! equ 0 (
        echo Downloaded prebuilt binary to %DEST%
        exit /b 0
    )
    echo curl download failed, trying PowerShell ...
)

REM Try PowerShell as fallback
powershell -NoProfile -Command ^
    "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; try { Invoke-WebRequest -Uri '%URL%' -OutFile '%DEST%' -UseBasicParsing; exit 0 } catch { exit 1 }" 2>nul
if %ERRORLEVEL% equ 0 (
    echo Downloaded prebuilt binary to %DEST%
    exit /b 0
)

echo Prebuilt binary not available for version %ODBC_PREBUILT_VSN%.
if exist "%DEST%" del "%DEST%"

REM --- Try local MSVC build ---
where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Cannot build odbcserver.exe.
    echo   - No prebuilt binary available for v%ODBC_PREBUILT_VSN%
    echo   - MSVC compiler ^(cl.exe^) not found
    echo.
    echo To fix, either:
    echo   1. Use a released version that has prebuilt binaries
    echo   2. Run from a Visual Studio Developer Command Prompt
    echo   3. Install Visual Studio Build Tools and run vcvarsall.bat
    exit /b 1
)

echo Building locally with MSVC ...

REM --- Erlang paths ---
for /f "delims=" %%i in ('erl -noshell -eval "io:format(\"~ts/erts-~ts/include/\", [code:root_dir(), erlang:system_info(version)])." -s init stop') do set ERTS_INCLUDE_DIR=%%i
for /f "delims=" %%i in ('erl -noshell -eval "io:format(\"~ts\", [code:lib_dir(erl_interface, include)])." -s init stop') do set EI_INCLUDE_DIR=%%i
for /f "delims=" %%i in ('erl -noshell -eval "io:format(\"~ts\", [code:lib_dir(erl_interface, lib)])." -s init stop') do set EI_LIB_DIR=%%i

set CFLAGS=/nologo /MD /O2 /W3 /DWIN32 /D_WIN32_WINNT=0x0600 /DHAVE_STRUCT_SOCKADDR_IN6_SIN6_ADDR /Dssize_t=SSIZE_T
set INCLUDES=/I"%ERTS_INCLUDE_DIR%" /I"%EI_INCLUDE_DIR%" /I.
set LIBS=/link /LIBPATH:"%EI_LIB_DIR%" ei_md.lib ws2_32.lib odbc32.lib odbccp32.lib

pushd "%SCRIPTDIR%"
cl.exe %CFLAGS% %INCLUDES% /Fe:"%DEST%" odbcserver.c %LIBS%
set BUILD_ERR=%ERRORLEVEL%
popd

if %BUILD_ERR% neq 0 (
    echo MSVC build failed!
    exit /b 1
)

echo Build succeeded: %DEST%
