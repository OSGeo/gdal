#!/bin/sh
# GDAL specific script to extract exported shapelib symbols that can be renamed
# to keep them internal to GDAL as much as possible

gcc shpopen.c dbfopen.c shptree.c sbnsearch.c -fPIC -shared -o shapelib.so -I.

OUT_FILE=gdal_shapelib_symbol_rename.h

rm $OUT_FILE 2>/dev/null

symbol_list=$(objdump -t shapelib.so  | grep .text | awk '{print $6}' | grep -v -e .text -e __do_global -e __bss_start -e _edata -e _end -e _fini -e _init -e call_gmon_start -e register_tm_clones | sort)
for symbol in $symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rodata_symbol_list=$(objdump -t shapelib.so  | grep "\\.rodata" |  awk '{print $6}' | grep -v "\\.")
for symbol in $rodata_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

data_symbol_list=$(objdump -t shapelib.so  | grep "\\.data" | grep -v -e __dso_handle -e "__TMC_END__" | awk '{print $6}' | grep -v "\\.")
for symbol in $data_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

bss_symbol_list=$(objdump -t shapelib.so  | grep "\\.bss" | grep -v bBigEndian |  awk '{print $6}' | grep -v "\\.")
for symbol in $bss_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

sort -u < $OUT_FILE > $OUT_FILE.tmp

echo "/* This is a generated file by dump_symbols.h. *DO NOT EDIT MANUALLY !* */" > $OUT_FILE
cat $OUT_FILE.tmp >> $OUT_FILE
rm -f $OUT_FILE.tmp

rm shapelib.so
