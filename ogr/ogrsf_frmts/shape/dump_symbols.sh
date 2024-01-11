#!/bin/sh
# GDAL specific script to extract exported shapelib symbols that can be renamed
# to keep them internal to GDAL as much as possible

if ! test -f $PWD/port/cpl_config.h; then
  echo "ERROR: This script should be run from the top of the build directory"
  return 1
fi

if ! test -f $PWD/../port/cpl_port.h; then
  echo "ERROR: The build directory should be a sub-directory immediately under the root of the source tree"
  return 1
fi

SHAPELIB_DIR=$PWD/../ogr/ogrsf_frmts/shape

gcc $SHAPELIB_DIR/shpopen.c $SHAPELIB_DIR/dbfopen.c $SHAPELIB_DIR/shptree.c $SHAPELIB_DIR/sbnsearch.c $SHAPELIB_DIR/shp_vsi.c -fPIC -shared -o shapelib.so -I$SHAPELIB_DIR/ -I../port -Iport

OUT_FILE=$SHAPELIB_DIR/gdal_shapelib_symbol_rename.h

rm $OUT_FILE 2>/dev/null

symbol_list=$(objdump -t shapelib.so  | grep .text | awk '{print $6}' | grep -v -e .text -e __do_global -e __bss_start -e _edata -e _end -e _fini -e _init -e call_gmon_start -e register_tm_clones -e VSI_SHP | sort)
for symbol in $symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

rodata_symbol_list=$(objdump -t shapelib.so  | grep "\\.rodata" |  awk '{print $6}' | grep -v "\\." | grep -v cpl_cvsid)
for symbol in $rodata_symbol_list
do
    echo "#define $symbol gdal_$symbol" >> $OUT_FILE
done

data_symbol_list=$(objdump -t shapelib.so  | grep "\\.data" | grep -v -e __dso_handle -e "__TMC_END__" | awk '{print $6}' | grep -v "\\." | grep -v sOGR)
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
echo "#ifndef GDAL_SHAPELIB_SYMBOL_RENAME_H_INCLUDED" >> $OUT_FILE
echo "#define GDAL_SHAPELIB_SYMBOL_RENAME_H_INCLUDED" >> $OUT_FILE
cat $OUT_FILE.tmp >> $OUT_FILE
echo "#endif /* GDAL_SHAPELIB_SYMBOL_RENAME_H_INCLUDED */" >> $OUT_FILE
rm -f $OUT_FILE.tmp

rm shapelib.so
