#!/bin/sh

set -e

cd gdal

if grep -r "\.c" scanbuildoutput | grep "<string>" | grep -v "<key>" | grep -v degrib | grep -v libjpeg | grep -v libpng | grep -v EHapi | grep -v GDapi | grep -v SWapi | grep -v osr_cs_wkt_parser | grep -v ods_formula_parser | grep -v swq_parser; then echo "error" && /bin/false; else echo "ok"; fi 

cd ..
