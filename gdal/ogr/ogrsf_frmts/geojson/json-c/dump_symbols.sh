#!/bin/sh
symbol_list="$(objdump -t ../../o/arraylist.o | grep text | awk '{print $6}' | grep -v .text) $(objdump -t ../../o/debug.o | grep text | awk '{print $6}' | grep -v .text) $(objdump -t ../../o/json_c_version.o | grep text | awk '{print $6}' | grep -v .text) $(objdump -t ../../o/json_object.o | grep text | awk '{print $6}' | grep -v .text) $(objdump -t ../../o/json_object_iterator.o | grep text | awk '{print $6}' | grep -v .text) $(objdump -t ../../o/json_tokener.o | grep text | awk '{print $6}' | grep -v .text | grep -v strndup) $(objdump -t ../../o/json_util.o | grep text | awk '{print $6}' | grep -v .text) $(objdump -t ../../o/linkhash.o | grep text | awk '{print $6}' | grep -v .text) $(objdump -t ../../o/printbuf.o | grep text | awk '{print $6}' | grep -v .text)"

echo "/* This is a generated file by dump_symbols.h. *DO NOT EDIT MANUALLY !* */"
echo "#ifndef symbol_renames"
echo "#define symbol_renames"
echo ""
for symbol in $symbol_list
do
    echo "#define $symbol gdal_$symbol"
done
echo ""
echo "#endif /*  symbol_renames */"

