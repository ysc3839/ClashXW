#!/bin/sh

FILES="ClashW.cpp EdgeWebView2.hpp MenuUtil.hpp OSLicensesDlg.hpp"

cd ..
xgettext -o translate/messages.pot --c++ --add-comments=/ --keyword=_ --keyword=C_:1c,2 $FILES
