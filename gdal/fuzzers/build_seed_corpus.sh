#!/bin/bash

if [ "$OUT" == "" ]; then
    echo "OUT env var not defined"
    exit 1
fi

cd $(dirname $0)/../../autotest/gcore/data
rm -f $OUT/gdal_fuzzer_seed_corpus.zip
zip -r $OUT/gdal_fuzzer_seed_corpus.zip . >/dev/null
cd $OLDPWD

cd $(dirname $0)/../../autotest/ogr/data
rm -f $OUT/ogr_fuzzer_seed_corpus.zip
zip -r $OUT/ogr_fuzzer_seed_corpus.zip . >/dev/null
cd $OLDPWD

printf "FUZZER_FRIENDLY_ARCHIVE\n" > poly_shp.tar
printf "***NEWFILE***:my.shp\n" >> poly_shp.tar
cat $(dirname $0)/../../autotest/ogr/data/poly.shp >> poly_shp.tar
printf "***NEWFILE***:my.shx\n" >> poly_shp.tar
cat $(dirname $0)/../../autotest/ogr/data/poly.shx >> poly_shp.tar
printf "***NEWFILE***:my.dbf\n" >> poly_shp.tar
cat $(dirname $0)/../../autotest/ogr/data/poly.dbf >> poly_shp.tar
printf "***NEWFILE***:my.prj\n" >> poly_shp.tar
cat $(dirname $0)/../../autotest/ogr/data/poly.PRJ >> poly_shp.tar
rm -f $OUT/shape_fuzzer_seed_corpus.zip
zip -r $OUT/shape_fuzzer_seed_corpus.zip poly_shp.tar >/dev/null

printf "FUZZER_FRIENDLY_ARCHIVE\n" > all_geoms_tab.tar
printf "***NEWFILE***:my.tab\n" >> all_geoms_tab.tar
cat $(dirname $0)/../../autotest/ogr/data/all_geoms.tab >> all_geoms_tab.tar
printf "***NEWFILE***:my.map\n" >> all_geoms_tab.tar
cat $(dirname $0)/../../autotest/ogr/data/all_geoms.map >> all_geoms_tab.tar
printf "***NEWFILE***:my.dat\n" >> all_geoms_tab.tar
cat $(dirname $0)/../../autotest/ogr/data/all_geoms.dat >> all_geoms_tab.tar
printf "***NEWFILE***:my.id\n" >> all_geoms_tab.tar
cat $(dirname $0)/../../autotest/ogr/data/all_geoms.id >> all_geoms_tab.tar
rm -f $OUT/mitab_tab_fuzzer_seed_corpus.zip
zip -r $OUT/mitab_tab_fuzzer_seed_corpus.zip all_geoms_tab.tar >/dev/null

printf "FUZZER_FRIENDLY_ARCHIVE\n" > small_mif.tar
printf "***NEWFILE***:my.mif\n" >> small_mif.tar
cat $(dirname $0)/../../autotest/ogr/data/small.mif >> small_mif.tar
printf "***NEWFILE***:my.mid\n" >> small_mif.tar
cat $(dirname $0)/../../autotest/ogr/data/small.mid >> small_mif.tar
rm -f $OUT/mitab_mif_fuzzer_seed_corpus.zip
zip -r $OUT/mitab_mif_fuzzer_seed_corpus.zip small_mif.tar >/dev/null
