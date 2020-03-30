#!/bin/sh

FILES="ClashW.cpp EdgeWebView2.hpp MenuUtil.hpp OSLicensesDlg.hpp"

COPYRIGHT_HOLDER="Richard Yu <yurichard3839@gmail.com>"
PKG_NAME="ClashW"

cd ..
xgettext -o translate/messages.pot --c++ --add-comments=/ --keyword=_ --keyword=C_:1c,2 --copyright-holder="$COPYRIGHT_HOLDER" --package-name="$PKG_NAME" $FILES
