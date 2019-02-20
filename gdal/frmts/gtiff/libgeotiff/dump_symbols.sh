#!/bin/sh
# GDAL specific script to extract exported libtiff symbols that can be renamed
# to keep them internal to GDAL as much as possible

PROJ_INCLUDE=-I/home/even/proj/install-proj-master/include
gcc ./*.c -fPIC -shared -o libgeotiff.so -I. -I../../../port ${PROJ_INCLUDE}

OUT_FILE=gdal_libgeotiff_symbol_rename.h

rm $OUT_FILE 2>/dev/null

echo "/* This is a generated file by dump_symbols.h. *DO NOT EDIT MANUALLY !* */" >> $OUT_FILE

symbol_list=$(objdump -t libgeotiff.so  | grep .text | awk '{print $6}' | grep -v -e .text -e __do_global -e __bss_start -e _edata -e _end -e _fini -e _init -e call_gmon_start -e CPL_IGNORE_RET_VAL_INT -e register_tm_clones | sort)
for symbol in $symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rodata_symbol_list=$(objdump -t libgeotiff.so  | grep "\\.rodata" |  awk '{print $6}' | grep -v "\\.")
for symbol in $rodata_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

data_symbol_list=$(objdump -t libgeotiff.so  | grep "\\.data" | grep -v __dso_handle | grep -v __TMC_END__ |  awk '{print $6}' | grep -v "\\.")
for symbol in $data_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

bss_symbol_list=$(objdump -t libgeotiff.so  | grep "\\.bss" |  awk '{print $6}' | grep -v "\\.")
for symbol in $bss_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rm libgeotiff.so
