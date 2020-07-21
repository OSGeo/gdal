#!/bin/sh

SONAME=/tmp/g2clib.so

gcc -fPIC -I../../port degrib/g2clib/*.c -shared -o $SONAME

OUT_FILE=degrib/g2clib/gdal_g2clib_symbol_rename.h

rm $OUT_FILE 2>/dev/null

echo "/* This is a generated file by rename_g2clib_symbols.h. *DO NOT EDIT MANUALLY !* */" >> $OUT_FILE

symbol_list=$(objdump -t $SONAME  | grep .text | awk '{print $6}' | grep -v -e .text -e __do_global -e __bss_start -e _edata -e _end -e _fini -e _init -e call_gmon_start  -e register_tm_clones | sort)

for symbol in $symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rodata_symbol_list=$(objdump -t $SONAME  | grep "\\.rodata" |  awk '{print $6}' | grep -v "\\.")
for symbol in $rodata_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

data_symbol_list=$(objdump -t $SONAME  | grep "\\.data"  | grep -v -e __dso_handle -e __TMC_END__ | awk '{print $6}' | grep -v "\\.")
for symbol in $data_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

bss_symbol_list=$(objdump -t $SONAME  | grep "\\.bss" |  awk '{print $6}' | grep -v "\\.")
for symbol in $bss_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rm $SONAME

