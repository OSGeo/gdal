#!/bin/bash

if [ "$OUT" == "" ]; then
    echo "OUT env var not defined"
    exit 1
fi

cd $(dirname $0)/../../autotest/gcore/data
zip -r $OUT/gdal_fuzzer_seed_corpus.zip .

