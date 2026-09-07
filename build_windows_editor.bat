@echo off
setlocal EnableExtensions

rem Build the Godot Windows development editor from this source tree.
rem Usage:
rem   build_windows_editor.bat [--mcp-bridge] [--with-vsproj] [--help]
rem
rem Run this script from any directory. It locates the source root from
rem the script's own location and discovers Visual Studio through vswhere.

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%" >nul || (
    echo ERROR: Unable to enter the script directory.
    exit /b 1
)

if not exist "SConstruct" (
    echo ERROR: SConstruct was not found next to this script.
    echo Place build_windows_editor.bat in the Godot source root.
    popd
    exit /b 1
)

set "GENERATE_VSPROJ=0"
set "ENABLE_MCP_BRIDGE=0"
:parse_arguments
if "%~1"=="" goto arguments_done
if /I "%~1"=="--with-vsproj" (
    set "GENERATE_VSPROJ=1"
    shift
    goto parse_arguments
)
if /I "%~1"=="--mcp-bridge" (
    set "ENABLE_MCP_BRIDGE=1"
    shift
    goto parse_arguments
)
if /I "%~1"=="--help" goto show_help
if /I "%~1"=="-h" goto show_help

echo ERROR: Unknown option "%~1".
goto show_help_error

:arguments_done
where py >nul 2>nul
if errorlevel 1 (
    echo ERROR: Python Launcher ^(py.exe^) was not found on PATH.
    echo Install Python for Windows, then run this script again.
    goto failure
)

py -m SCons --version >nul 2>nul
if errorlevel 1 (
    echo ERROR: SCons is not installed for the Python Launcher.
    echo Install it with: py -m pip install --user scons
    goto failure
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio Installer's vswhere.exe was not found.
    echo Install Visual Studio 2022 or Build Tools with Desktop development with C++.
    goto failure
)

set "VS_INSTALL_PATH="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_INSTALL_PATH=%%I"
)

if not defined VS_INSTALL_PATH (
    echo ERROR: No Visual Studio installation with MSVC x64/x86 tools was found.
    goto failure
)

if not exist "%VS_INSTALL_PATH%\VC\Auxiliary\Build\vcvars64.bat" (
    echo ERROR: vcvars64.bat was not found in the detected Visual Studio installation.
    goto failure
)

call "%VS_INSTALL_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo ERROR: Failed to initialize the MSVC x64 environment.
    goto failure
)

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: cl.exe was not found after initializing MSVC.
    goto failure
)

set "MCP_BRIDGE_ARGUMENT="
if "%ENABLE_MCP_BRIDGE%"=="1" set "MCP_BRIDGE_ARGUMENT=mcp_bridge=yes"

echo.
echo === Building Godot Windows development editor ===
py -m SCons platform=windows target=editor dev_build=yes opengl3=no %MCP_BRIDGE_ARGUMENT% -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo ERROR: Godot editor build failed.
    goto failure
)

if not exist "bin\godot.windows.editor.dev.x86_64.console.exe" (
    echo ERROR: Build reported success but the console editor executable is missing.
    goto failure
)

echo.
echo === Verifying build ===
bin\godot.windows.editor.dev.x86_64.console.exe --version
if errorlevel 1 (
    echo ERROR: The built editor failed version verification.
    goto failure
)

if "%GENERATE_VSPROJ%"=="0" goto success

echo.
echo === Generating Visual Studio solution ===
py -m SCons platform=windows target=editor dev_build=yes opengl3=no %MCP_BRIDGE_ARGUMENT% vsproj=yes
if errorlevel 1 (
    echo ERROR: Visual Studio solution generation failed.
    goto failure
)

if not exist "godot.sln" (
    echo ERROR: Solution generation reported success but godot.sln is missing.
    goto failure
)

echo Generated Visual Studio solution: godot.sln

:success
echo.
echo SUCCESS: Godot development editor is ready.
echo Editor: bin\godot.windows.editor.dev.x86_64.exe
if "%GENERATE_VSPROJ%"=="1" echo Solution: godot.sln
if "%ENABLE_MCP_BRIDGE%"=="1" echo MCP Control Center: enabled
popd
exit /b 0

:show_help
echo Usage: %~nx0 [--mcp-bridge] [--with-vsproj] [--help]
echo.
echo   --mcp-bridge  Enable the built-in MCP Control Center in this editor build.
echo   --with-vsproj Build the editor, then generate godot.sln for Visual Studio.
echo   --help, -h    Show this help text.
popd
exit /b 0

:show_help_error
echo.
echo Usage: %~nx0 [--mcp-bridge] [--with-vsproj] [--help]
popd
exit /b 2

:failure
popd
exit /b 1
