@echo off
rem Build and run the unit tests. Exit code = number of failed checks (or 1 on build failure).
rem Uses the current MSVC environment if cl.exe is on PATH (e.g. CI), else finds VS via vswhere.
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
)
if defined VSDIR call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 (echo cl.exe not found: install Visual Studio Build Tools with the C++ workload & exit /b 1)
if not exist out mkdir out
cl /nologo /W3 /MT /Fo:out\ /Fe:out\lang_test.exe lang_test.c /link user32.lib
if errorlevel 1 exit /b 1
out\lang_test.exe
exit /b %errorlevel%
