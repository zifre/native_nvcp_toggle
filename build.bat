@echo off
REM NVCP Toggle - Build Script for MSVC
REM Run from Visual Studio Developer Command Prompt

setlocal

REM Set paths
set NVAPI_DIR=nvapi
set SRC=native_nvcp_toggle.c
set OUT=native_nvcp_toggle.exe

REM Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo ERROR: cl.exe not found. Run this from Visual Studio Developer Command Prompt.
    echo.
    echo Or run: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    pause
    exit /b 1
)

REM Compile resource file for icon
echo Compiling resources...
rc /nologo native_nvcp_toggle.rc
if %ERRORLEVEL% neq 0 (
    echo Resource compilation failed!
    pause
    exit /b 1
)

REM Build 32-bit version
echo Building 32-bit version...
cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS ^
    /I"%NVAPI_DIR%" ^
    "%SRC%" ^
    native_nvcp_toggle.res ^
    "%NVAPI_DIR%\x86\nvapi.lib" ^
    user32.lib gdi32.lib ^
    /Fe"%OUT%" ^
    /link /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build successful: %OUT%
echo.

REM Clean up build artifacts
del *.obj 2>nul
del *.res 2>nul

pause
