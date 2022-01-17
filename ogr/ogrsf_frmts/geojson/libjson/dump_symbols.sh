#!/bin/sh

symbol_list=""
for filename in arraylist.o debug.o json_c_version.o json_object.o json_tokener.o json_object_iterator.o json_util.o linkhash.o printbuf.o strerror_override.o random_seed.o; do
symbol_list="$symbol_list $(objdump -t ../../o/$filename | grep text | awk '{print $6}' | grep -v .text | grep -v .hidden)"
symbol_list="$symbol_list $(objdump -t ../../o/$filename | grep .data.rel.local | awk '{print $6}' | grep -v .data.rel.local | grep -v .hidden)"
done

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

