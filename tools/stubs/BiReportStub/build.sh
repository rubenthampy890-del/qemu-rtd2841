#!/usr/bin/env bash
# Build script for BiReportStub.apk
# Target: Android 9 (API 28), compiles AIDL, Java, DEX, APK, aligns, and signs.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

JAVA_BIN="/opt/homebrew/opt/openjdk@17/bin"
SDK_BUILD_TOOLS="/Users/basilthampy/Library/Android/sdk/build-tools/35.0.0"
ANDROID_JAR="/Users/basilthampy/Library/Android/sdk/platforms/android-34/android.jar"

export JAVA_HOME="/opt/homebrew/opt/openjdk@17"
export PATH="${JAVA_BIN}:${PATH}"

cd "${SCRIPT_DIR}"
rm -rf gen obj bin
mkdir -p gen obj bin

echo "[*] Compiling AIDL interfaces..."
"${SDK_BUILD_TOOLS}/aidl" --lang=java -I aidl -o gen aidl/com/tcl/bi/service/IBiReportService.aidl
"${SDK_BUILD_TOOLS}/aidl" --lang=java -I aidl -o gen aidl/com/tcl/ttvs/bi/IBIService.aidl

echo "[*] Compiling Java classes..."
"${JAVA_BIN}/javac" -source 8 -target 8 -cp "${ANDROID_JAR}:gen" -d obj $(find src gen -name "*.java")

echo "[*] Converting bytecode to DEX..."
"${SDK_BUILD_TOOLS}/d8" --lib "${ANDROID_JAR}" --output bin/ $(find obj -name "*.class")

echo "[*] Packaging APK..."
"${SDK_BUILD_TOOLS}/aapt" package -f -M AndroidManifest.xml -I "${ANDROID_JAR}" -F bin/unaligned.apk bin

echo "[*] Aligning APK..."
"${SDK_BUILD_TOOLS}/zipalign" -f -p 4 bin/unaligned.apk BiReportStub.apk

echo "[*] Signing APK..."
if [ ! -f /tmp/debug.keystore ]; then
    "${JAVA_BIN}/keytool" -genkey -v -keystore /tmp/debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US"
fi
"${SDK_BUILD_TOOLS}/apksigner" sign --ks /tmp/debug.keystore --ks-pass pass:android --key-pass pass:android BiReportStub.apk
"${SDK_BUILD_TOOLS}/apksigner" verify BiReportStub.apk

echo "[*] Deploying to apks/..."
mkdir -p "${REPO_ROOT}/apks"
cp -f BiReportStub.apk "${REPO_ROOT}/apks/BiReportStub.apk"
echo "[+] BiReportStub.apk built and verified successfully (${SCRIPT_DIR}/BiReportStub.apk)"
