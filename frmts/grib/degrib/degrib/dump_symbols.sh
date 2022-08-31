#!/bin/sh
# GDAL specific script to extract exported degrib symbols that can be renamed
# to keep them internal to GDAL as much as possible

SCRIPT_DIR=$(dirname "$0")
cd "${SCRIPT_DIR}"

PATH_TO_BUILD_DIR=$1

gcc clock.c \
  degrib1.cpp \
  degrib2.cpp \
  grib1tab.cpp \
  hazard.c \
  inventory.cpp \
  metaname.cpp \
  metaparse.cpp \
  metaprint.cpp \
  myassert.c \
  myerror.cpp \
  myutil.c \
  scan.c \
  tdlpack.cpp \
  tendian.cpp \
  weather.c \
	-I. -I../../../../port -I${PATH_TO_BUILD_DIR}/port -fPIC -shared -o degrib.so

OUT_FILE=gdal_degrib_symbol_rename.h

rm $OUT_FILE 2>/dev/null

echo "/* This is a generated file by dump_symbols.h. *DO NOT EDIT MANUALLY !* */" >> $OUT_FILE

symbol_list=$(objdump -t degrib.so  | grep .text | awk '{print $6}' | grep -v -e "\.text" -e __do_global -e __bss_start -e _edata -e call_gmon_start -e register_tm_clones -e _Z | sort)
for symbol in $symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rodata_symbol_list=$(objdump -t degrib.so  | grep "\\.rodata" |  awk '{print $6}' | grep -v "\\." | grep -v _Z)
for symbol in $rodata_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

data_symbol_list=$(objdump -t degrib.so  | grep "\\.data"  | grep -v -e __dso_handle -e __TMC_END__ | awk '{print $6}' | grep -v "\\." | grep -v _Z)
for symbol in $data_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

bss_symbol_list=$(objdump -t degrib.so  | grep "\\.bss" |  awk '{print $6}' | grep -v "\\." | grep -v _Z)
for symbol in $bss_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rm degrib.so
