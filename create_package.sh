#!/bin/sh
set -eu

CLASH_VER=v1.1.0

mkdir pkg
cd pkg

curl -LO https://github.com/Dreamacro/maxmind-geoip/releases/latest/download/Country.mmdb
curl -LO https://github.com/Dreamacro/clash-dashboard/archive/gh-pages.tar.gz
curl -LO https://github.com/Dreamacro/clash/releases/download/${CLASH_VER}/clash-windows-amd64-${CLASH_VER}.zip
curl -LO https://github.com/Dreamacro/clash/releases/download/${CLASH_VER}/clash-windows-386-${CLASH_VER}.zip

tar xvf gh-pages.tar.gz
unzip clash-windows-amd64-${CLASH_VER}.zip
unzip clash-windows-386-${CLASH_VER}.zip

mkdir -p 64/ClashAssets
	cd 64/ClashAssets
		cp -r ../../clash-dashboard-gh-pages Dashboard
		cp ../../Country.mmdb .
		mv ../../clash-windows-amd64.exe clash.exe
	cd ..

	cp ../../x64/Release/ClashXW64.exe .
	cp ../../x64/Release/WebView2Loader.dll .
cd ..

mkdir -p 32/ClashAssets
	cd 32/ClashAssets
		cp -r ../../clash-dashboard-gh-pages Dashboard
		cp ../../Country.mmdb .
		mv ../../clash-windows-386.exe clash.exe
	cd ..

	cp ../../Release/ClashXW32.exe .
	cp ../../Release/WebView2Loader.dll .
cd ..

powershell -NoProfile -ExecutionPolicy Unrestricted -Command '$ProgressPreference = "SilentlyContinue"; Compress-Archive 64\* 64.zip'
powershell -NoProfile -ExecutionPolicy Unrestricted -Command '$ProgressPreference = "SilentlyContinue"; Compress-Archive 32\* 32.zip'
