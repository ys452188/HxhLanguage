#!/data/data/com.termux/files/usr/bin/sh
set -e
#构建Termux上的包

#编译
mkdir -p build
cd build
cmake ..
make
#复制
cd ..
mkdir -p TermuxPackage/data/data/com.termux/files/usr/bin/
cp -r build/hxc TermuxPackage/data/data/com.termux/files/usr/bin/
cp -r build/hxvm TermuxPackage/data/data/com.termux/files/usr/bin/
chmod +x build/hxc TermuxPackage/data/data/com.termux/files/usr/bin/hxc
chmod +x build/hxc TermuxPackage/data/data/com.termux/files/usr/bin/hxvm
chmod 755 TermuxPackage/DEBIAN
chmod 644 TermuxPackage/DEBIAN/control
chmod 0555 TermuxPackage/DEBIAN/postinst
chmod 0555 TermuxPackage/DEBIAN/prerm

#打包
termux-elf-cleaner TermuxPackage/data/data/com.termux/files/usr/bin/hxc
termux-elf-cleaner TermuxPackage/data/data/com.termux/files/usr/bin/hxvm
dpkg-deb --build TermuxPackage hxhlang_0.114_aarch64.deb
chmod +x hxhlang_0.114_aarch64.deb