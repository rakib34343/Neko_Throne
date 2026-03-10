#!/bin/bash
set -e

source script/env_deploy.sh
if [[ $1 == 'i686' ]]; then
  ARCH="windowslegacy-386"
  DEST=$DEPLOYMENT/windows32
else
  if [[ $1 == 'x86_64' ]]; then
    ARCH="windowslegacy-amd64"
    DEST=$DEPLOYMENT/windowslegacy64
  else
    ARCH="windows-amd64"
    DEST=$DEPLOYMENT/windows64
  fi
fi
rm -rf $DEST
mkdir -p $DEST

#### copy exe ####
cp $BUILD/Neko_Throne.exe $DEST
cp $BUILD/*pdb $DEST || true

#### copy icon ####
cp ./res/public/Throne.png $DEST 2>/dev/null || true

#### copy translations to lang/ ####
if [ -d "$BUILD/lang" ]; then
  mkdir -p $DEST/lang
  cp $BUILD/lang/*.qm $DEST/lang/ 2>/dev/null || true
fi

#### extract Go artifacts ####
echo ">> Current directory: $(pwd)"
echo ">> ARCH=$ARCH, looking for: download-artifact/*$ARCH"
ls -la download-artifact/ || echo "download-artifact not found!"
cd download-artifact
cd *$ARCH
tar xvzf artifacts.tgz -C ../../
cd ../..

#### deploy Qt runtime DLLs (shared Qt only) ####
# Detect whether this is a shared or static Qt build.
# MSYS_NO_PATHCONV=1 prevents Git Bash from mangling /dependents into a path.
HAS_QT_DLL=0
if command -v dumpbin &>/dev/null; then
  HAS_QT_DLL=$(MSYS_NO_PATHCONV=1 dumpbin //dependents "$DEST/Neko_Throne.exe" 2>/dev/null | grep -ci "Qt6" || true)
elif command -v objdump &>/dev/null; then
  HAS_QT_DLL=$(objdump -p "$DEST/Neko_Throne.exe" 2>/dev/null | grep -ci "Qt6" || true)
fi

# Fallback: if detection returned 0 but windeployqt exists, assume shared Qt.
# Static Qt is rare in CI; failing to deploy DLLs is worse than a harmless no-op.
if [[ "$HAS_QT_DLL" -eq 0 ]] && command -v windeployqt &>/dev/null; then
  echo "=== DLL detection inconclusive, windeployqt found — assuming shared Qt ==="
  HAS_QT_DLL=1
fi

if [[ "$HAS_QT_DLL" -gt 0 ]]; then
  echo "=== Shared Qt build detected, running windeployqt ==="
  pushd "$DEST"
  windeployqt Neko_Throne.exe \
    --no-translations \
    --no-system-d3d-compiler \
    --no-opengl-sw \
    --no-svg \
    --verbose 2
  popd
  # Remove unnecessary DX shader compiler DLLs
  rm -f "$DEST/dxcompiler.dll" "$DEST/dxil.dll"
else
  echo "=== Static Qt build detected — no DLL deployment needed ==="
fi
