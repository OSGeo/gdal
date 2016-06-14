#!/bin/sh
# GDAL specific script to extract exported libtiff symbols that can be renamed
# to keep them internal to GDAL as much as possible

gcc *.c -fPIC -shared -o libtiff.so -I. -I../../../port  -DPIXARLOG_SUPPORT -DZIP_SUPPORT -DOJPEG_SUPPORT -DLZMA_SUPPORT

OUT_FILE=gdal_libtiff_symbol_rename.h

rm $OUT_FILE 2>/dev/null

echo "/* This is a generated file by dump_symbols.h. *DO NOT EDIT MANUALLY !* */" >> $OUT_FILE

# We exclude the TIFFSwab functions for renaming since tif_swab.c uses ifdef to determine if the symbols must be defined
symbol_list=$(objdump -t libtiff.so  | grep .text | awk '{print $6}' | grep -v .text | grep -v TIFFInit | grep -v TIFFSwab | grep -v __do_global | grep -v __bss_start | grep -v _edata | grep -v _end | grep -v _fini | grep -v _init | sort)
for symbol in $symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rodata_symbol_list=$(objdump -t libtiff.so  | grep "\.rodata" |  awk '{print $6}' | grep -v "\.")
for symbol in $rodata_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

data_symbol_list=$(objdump -t libtiff.so  | grep "\.data" |  awk '{print $6}' | grep -v "\.")
for symbol in $data_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

bss_symbol_list=$(objdump -t libtiff.so  | grep "\.bss" |  awk '{print $6}' | grep -v "\.")
for symbol in $bss_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rm libtiff.so

# Was excluded by grep -v TIFFInit
echo "#define TIFFInitDumpMode gdal_TIFFInitDumpMode" >> $OUT_FILE

# Pasted and adapter from tif_codec.c
echo "#define TIFFReInitJPEG_12 gdal_TIFFReInitJPEG_12" >> $OUT_FILE
echo "#ifdef LZW_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitLZW gdal_TIFFInitLZW" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef PACKBITS_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitPackBits gdal_TIFFInitPackBits" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef THUNDER_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitThunderScan gdal_TIFFInitThunderScan" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef NEXT_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitNeXT gdal_TIFFInitNeXT" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef JPEG_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitJPEG gdal_TIFFInitJPEG" >> $OUT_FILE
# Manually added
echo "#define TIFFInitJPEG_12 gdal_TIFFInitJPEG_12" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef OJPEG_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitOJPEG gdal_TIFFInitOJPEG" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef CCITT_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitCCITTRLE gdal_TIFFInitCCITTRLE" >> $OUT_FILE
echo "#define TIFFInitCCITTRLEW gdal_TIFFInitCCITTRLEW" >> $OUT_FILE
echo "#define TIFFInitCCITTFax3 gdal_TIFFInitCCITTFax3" >> $OUT_FILE
echo "#define TIFFInitCCITTFax4 gdal_TIFFInitCCITTFax4" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef JBIG_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitJBIG gdal_TIFFInitJBIG" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef ZIP_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitZIP gdal_TIFFInitZIP" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef PIXARLOG_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitPixarLog gdal_TIFFInitPixarLog" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef LOGLUV_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitSGILog gdal_TIFFInitSGILog" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
echo "#ifdef LZMA_SUPPORT" >> $OUT_FILE
echo "#define TIFFInitLZMA gdal_TIFFInitLZMA" >> $OUT_FILE
echo "#endif" >> $OUT_FILE
