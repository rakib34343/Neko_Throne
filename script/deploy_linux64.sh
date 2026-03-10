#!/bin/bash
set -e

if [[ $(uname -m) == 'aarch64' || $(uname -m) == 'arm64' ]]; then
  ARCH="arm64"
  ARCH1="aarch64"
else
  ARCH="amd64"
  ARCH1="x86_64"
fi

source script/env_deploy.sh
DEST=$DEPLOYMENT/linux-$ARCH
rm -rf $DEST
mkdir -p $DEST

#### copy binary ####
cp $BUILD/Neko_Throne $DEST

#### copy translations to lang/ ####
if [ -d "$BUILD/lang" ]; then
  mkdir -p $DEST/lang
  cp $BUILD/lang/*.qm $DEST/lang/ 2>/dev/null || true
fi

#### copy Throne.png ####
cp ./res/public/Throne.png $DEST

echo ">> Current directory: $(pwd)"
echo ">> Looking for: download-artifact/*linux-$ARCH"
ls -la download-artifact/ || echo "download-artifact not found!"
cd download-artifact
cd *linux-$ARCH
tar xvzf artifacts.tgz -C ../../
cd ../..

sudo add-apt-repository -y universe
sudo apt-get update -qq
sudo apt-get install -y -qq libfuse2 patchelf
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20250213-2/linuxdeploy-$ARCH1.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-$ARCH1.AppImage
chmod +x linuxdeploy-$ARCH1.AppImage linuxdeploy-plugin-qt-$ARCH1.AppImage

export EXTRA_QT_PLUGINS="iconengines;wayland-shell-integration;wayland-decoration-client"
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
./linuxdeploy-$ARCH1.AppImage --appdir $DEST --executable $DEST/Neko_Throne --desktop-file $SRC_ROOT/res/public/Throne.desktop --icon-file $DEST/Throne.png --plugin qt 2>&1 | grep -v 'WARNING: Could not find copyright' | grep -v 'WARNING: Not calling strip' | grep -v 'WARNING: Using deprecated' || true
rm linuxdeploy-$ARCH1.AppImage linuxdeploy-plugin-qt-$ARCH1.AppImage
cd $DEST
rm -r ./usr/translations ./usr/bin ./usr/share ./apprun-hooks

# fix plugins rpath
rm -r ./usr/plugins
mkdir ./usr/plugins
mkdir ./usr/plugins/platforms
cp $QT_PLUGIN_PATH/platforms/libqxcb.so ./usr/plugins/platforms
cp $QT_PLUGIN_PATH/platforms/libqwayland.so ./usr/plugins/platforms
cp -r $QT_PLUGIN_PATH/platformthemes ./usr/plugins
cp -r $QT_PLUGIN_PATH/imageformats ./usr/plugins
cp -r $QT_PLUGIN_PATH/iconengines ./usr/plugins
cp -r $QT_PLUGIN_PATH/wayland-shell-integration ./usr/plugins
cp -r $QT_PLUGIN_PATH/wayland-decoration-client ./usr/plugins
cp -r $QT_PLUGIN_PATH/tls ./usr/plugins
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platforms/libqxcb.so
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platforms/libqwayland.so
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platformthemes/libqgtk3.so
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platformthemes/libqxdgdesktopportal.so

# fix extra libs...
mkdir ./usr/lib2
ls ./usr/lib/
cp ./usr/lib/libQt* ./usr/lib/libxcb-cursor* ./usr/lib/libxcb-util* ./usr/lib/libicuuc* ./usr/lib/libicui18n* ./usr/lib/libicudata* ./usr/lib2
rm -r ./usr/lib
mv ./usr/lib2 ./usr/lib

# fix lib rpath
cd $DEST
patchelf --set-rpath '$ORIGIN/usr/lib' ./Neko_Throne

# handle debug info
objcopy --only-keep-debug $DEST/Neko_Throne $DEST/Neko_Throne.debug
strip --strip-debug --strip-unneeded $DEST/Neko_Throne
objcopy --add-gnu-debuglink=$DEST/Neko_Throne.debug $DEST/Neko_Throne