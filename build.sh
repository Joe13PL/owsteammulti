#!/bin/sh
# Builds dist/wsock32.dll from Git Bash when vcvars32.bat is not available.
# Point VC and SDK at your MSVC toolset / Windows SDK if they differ.
set -e
cd "$(dirname "$0")"
: "${VC:=C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207}"
: "${SDK:=C:/Program Files (x86)/Windows Kits/10}"
: "${SDKV:=10.0.22621.0}"
export PATH="$(cygpath -u "$VC")/bin/Hostx64/x86:$(cygpath -u "$VC")/bin/Hostx64/x64:$PATH"
export INCLUDE="$VC/include;$SDK/Include/$SDKV/ucrt;$SDK/Include/$SDKV/um;$SDK/Include/$SDKV/shared"
export LIB="$VC/lib/x86;$SDK/Lib/$SDKV/ucrt/x86;$SDK/Lib/$SDKV/um/x86"
mkdir -p build
cl -nologo -LD -O2 -MT -EHsc -W3 src/owsteamnet.cpp -Fobuild/ -Fe:dist/wsock32.dll -link -NOLOGO -DEF:src/exports.def -IMPLIB:build/wsock32.lib
echo "Built dist/wsock32.dll"
