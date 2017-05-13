#!/bin/bash

if [ "$OUT" == "" ]; then
    echo "OUT env var not defined"
    exit 1
fi

cd $(dirname $0)/../../autotest/gcore/data
zip -r $OUT/gdal_fuzzer_seed_corpus.zip .
cd $OLDPWD

cd $(dirname $0)/../../autotest/ogr/data
zip -r $OUT/ogr_fuzzer_seed_corpus.zip .
cd $OLDPWD
