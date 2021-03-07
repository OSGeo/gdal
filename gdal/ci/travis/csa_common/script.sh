#!/bin/sh

set -e

sudo apt-get install jq

rm -f filtered_scanbuild.txt
files=$(find gdal/scanbuildoutput -name "*.sarif")
for f in $files; do
    # CSA 10 uses artifactLocation. Earlier versions used fileLocation
    (sed 's/fileLocation/artifactLocation/g' < $f) |jq '.runs[].results[] | (if .locations[].physicalLocation.artifactLocation.uri | (contains("/usr/include") or contains("degrib") or contains("libpng") or contains("libjpeg") or contains("EHapi") or contains("GDapi") or contains("SWapi") or contains("osr_cs_wkt_parser") or contains("ods_formula_parser") or contains("swq_parser") or contains("json_tokener") ) then empty else { "uri": .locations[].physicalLocation.artifactLocation.uri, "msg": .message.text, "location": .codeFlows[-1].threadFlows[-1].locations[-1] } end)' > tmp.txt
    if [ -s tmp.txt ]; then
        echo "Errors from $f: "
        cat $f
        echo ""
        cat tmp.txt >> filtered_scanbuild.txt
    fi
done
if [ -s filtered_scanbuild.txt ]; then
    echo ""
    echo ""
    echo "========================"
    echo "Summary of errors found:"
    cat filtered_scanbuild.txt
    /bin/false
fi
