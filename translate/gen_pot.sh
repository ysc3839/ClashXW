#!/bin/sh

FILES="ClashXW.cpp EdgeWebView2.hpp MenuUtil.hpp OSLicensesDlg.hpp"

COPYRIGHT_HOLDER="Richard Yu <yurichard3839@gmail.com>"
PKG_NAME="ClashXW"

pipenv run rc2po --pot --charset=utf-16le --lang=LANG_ENGLISH --sublang=SUBLANG_ENGLISH_US ./rc/ClashXW-translatable.rc ./rc/resource.pot

cd ..
xgettext -o translate/source/messages.pot --c++ --add-comments=/ --keyword=_ --keyword=C_:1c,2 --copyright-holder="$COPYRIGHT_HOLDER" --package-name="$PKG_NAME" $FILES
