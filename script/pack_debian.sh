#!/bin/bash
set -e

raw_version="$1"
# dpkg-deb requires version to start with a digit
if [[ ! "$raw_version" =~ ^[0-9] ]]; then
    version="0.0.0~${raw_version}"
else
    version="$raw_version"
fi

mkdir -p Throne/DEBIAN
mkdir -p Throne/opt
cp -r linux-amd64 Throne/opt
mv Throne/opt/linux-amd64 Throne/opt/Throne
rm -f Throne/opt/Throne/Neko_Throne.debug Throne/opt/Throne/Throne.debug 2>/dev/null || true

# basic
cat >Throne/DEBIAN/control <<-EOF
Package: Throne
Version: $version
Architecture: amd64
Maintainer: Mahdi Mahdi.zrei@gmail.com
Depends: desktop-file-utils
Description: Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
EOF

cat >Throne/DEBIAN/postinst <<-EOF
# TUN mode needs root: setuid on Core so user is not prompted for password after each install/upgrade
chown root:root /opt/Throne/NekoCore 2>/dev/null && chmod u+s /opt/Throne/NekoCore 2>/dev/null || true

cat >/usr/share/applications/Throne.desktop<<-END
[Desktop Entry]
Name=Neko Throne
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Exec=sh -c "PATH=/opt/Throne:\$PATH /opt/Throne/Neko_Throne -appdata"
Icon=/opt/Throne/Throne.png
Terminal=false
Type=Application
Categories=Network;Application;
END

update-desktop-database
EOF

sudo chmod 0755 Throne/DEBIAN/postinst

# desktop && PATH

sudo dpkg-deb --build Throne
