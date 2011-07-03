#!/bin/sh
# GDAL specific script to extract exported libtiff symbols that can be renamed
# to keep them internal to GDAL as much as possible

gcc *.c -fPIC -shared -o libgeotiff.so -I. -I../../../port

OUT_FILE=gdal_libgeotiff_symbol_rename.h

rm $OUT_FILE 2>/dev/null

echo "/* This is a generated file by dump_symbols.h. *DO NOT EDIT MANUALLY !* */" >> $OUT_FILE

symbol_list=$(objdump -t libgeotiff.so  | grep .text | awk '{print $6}' | grep -v .text | grep -v __do_global | grep -v __bss_start | grep -v _edata | grep -v _end | grep -v _fini | grep -v _init | sort)
for symbol in $symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rodata_symbol_list=$(objdump -t libgeotiff.so  | grep "\.rodata" |  awk '{print $6}' | grep -v "\.")
for symbol in $data_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

data_symbol_list=$(objdump -t libgeotiff.so  | grep "\.data" |  awk '{print $6}' | grep -v "\.")
for symbol in $data_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

bss_symbol_list=$(objdump -t libgeotiff.so  | grep "\.bss" |  awk '{print $6}' | grep -v "\.")
for symbol in $bss_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rm libgeotiff.so
