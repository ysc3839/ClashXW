#!/bin/sh

po2rc --charset=utf-16le --lang=LANG_CHINESE --sublang=SUBLANG_CHINESE_SIMPLIFIED -t ./rc/ClashXW-translatable.rc ./rc/zh_CN.po ./generated/zh_CN.rc
echo '#include "zh_CN.rc"' > ./generated/translate.rc

./po2ymo.py ./source/zh_CN.po ./generated/zh_CN.ymo
echo 'LANGUAGE LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED' >> ./generated/translate.rc
echo '1 YMO "zh_CN.ymo"' >> ./generated/translate.rc
