@echo off
rem Builds dist\wsock32.dll (32-bit) with Visual Studio / Build Tools (C++ x86 toolset).
setlocal
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set VSDIR=%%i
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul || (echo vcvars32.bat not found & exit /b 1)
cd /d "%~dp0"
if not exist build mkdir build
cl -nologo -LD -O2 -MT -EHsc -W3 src\owsteamnet.cpp src\syncfix.cpp -Fobuild\ -Fe:dist\wsock32.dll -link -NOLOGO -DEF:src\exports.def -IMPLIB:build\wsock32.lib || exit /b 1
echo Built dist\wsock32.dll
