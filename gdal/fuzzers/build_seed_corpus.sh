#!/bin/bash

set -e

if [ "$OUT" == "" ]; then
    echo "OUT env var not defined"
    exit 1
fi

echo "Building gdal_translate_fuzzer_seed_corpus.zip"
echo "FUZZER_FRIENDLY_ARCHIVE" > test.tar
echo "***NEWFILE***:cmd.txt" >> test.tar
echo "-outsize" >> test.tar
echo "20" >> test.tar
echo "20" >> test.tar
echo "-of" >> test.tar
echo "GTiff" >> test.tar
echo "-b" >> test.tar
echo "1" >> test.tar
echo "-ot" >> test.tar
echo "Byte" >> test.tar
echo "-r" >> test.tar
echo "nearest" >> test.tar
echo "-a_srs" >> test.tar
echo "EPSG:26711" >> test.tar
echo "-stats" >> test.tar
echo "-scale" >> test.tar
echo "-mo" >> test.tar
echo "FOO=BAR" >> test.tar
echo "-co" >> test.tar
echo "COMPRESS=NONE" >> test.tar
echo "-srcwin" >> test.tar
echo "0" >> test.tar
echo "0" >> test.tar
echo "20" >> test.tar
echo "20" >> test.tar
echo "***NEWFILE***:in" >> test.tar
cat $(dirname $0)/../../autotest/gcore/data/byte.tif >> test.tar
rm -f $OUT/gdal_translate_fuzzer_seed_corpus.zip
zip -r $OUT/gdal_translate_fuzzer_seed_corpus.zip test.tar >/dev/null
rm test.tar

echo "Building gtiff_fuzzer_seed_corpus.zip"
rm -f $OUT/gtiff_fuzzer_seed_corpus.zip
cd $(dirname $0)/../../autotest/gcore/data
zip -r $OUT/gtiff_fuzzer_seed_corpus.zip *.tif >/dev/null
cd $OLDPWD
cd $(dirname $0)/../../autotest/gdrivers/data
zip -r $OUT/gtiff_fuzzer_seed_corpus.zip *.tif >/dev/null
cd $OLDPWD

echo "Building hfa_fuzzer_seed_corpus.zip"
rm -f $OUT/hfa_fuzzer_seed_corpus.zip
cd $(dirname $0)/../../autotest/gcore/data
zip -r $OUT/hfa_fuzzer_seed_corpus.zip *.img >/dev/null
cd $OLDPWD
cd $(dirname $0)/../../autotest/gdrivers/data
zip -r $OUT/hfa_fuzzer_seed_corpus.zip *.img >/dev/null
cd $OLDPWD

echo "Building adrg_fuzzer_seed_corpus.zip"
printf "FUZZER_FRIENDLY_ARCHIVE\n" > adrg.tar
printf "***NEWFILE***:ABCDEF01.GEN\n" >> adrg.tar
cat $(dirname $0)/../../autotest/gdrivers/data/SMALL_ADRG/ABCDEF01.GEN >> adrg.tar
printf "***NEWFILE***:ABCDEF01.IMG\n" >> adrg.tar
cat $(dirname $0)/../../autotest/gdrivers/data/SMALL_ADRG/ABCDEF01.IMG >> adrg.tar
rm -f $OUT/adrg_fuzzer_seed_corpus.zip
zip -r $OUT/adrg_fuzzer_seed_corpus.zip adrg.tar >/dev/null
rm adrg.tar

echo "Building srp_fuzzer_seed_corpus.zip"
printf "FUZZER_FRIENDLY_ARCHIVE\n" > srp.tar
printf "***NEWFILE***:FKUSRP01.GEN\n" >> srp.tar
cat $(dirname $0)/../../autotest/gdrivers/data/USRP_PCB0/FKUSRP01.GEN >> srp.tar
printf "***NEWFILE***:FKUSRP01.IMG\n" >> srp.tar
cat $(dirname $0)/../../autotest/gdrivers/data/USRP_PCB0/FKUSRP01.IMG >> srp.tar
printf "***NEWFILE***:FKUSRP01.QAL\n" >> srp.tar
cat $(dirname $0)/../../autotest/gdrivers/data/USRP_PCB0/FKUSRP01.QAL >> srp.tar
rm -f $OUT/srp_fuzzer_seed_corpus.zip
zip -r $OUT/srp_fuzzer_seed_corpus.zip srp.tar >/dev/null
rm srp.tar


echo "Building mrf_fuzzer_seed_corpus.zip"
rm -f $OUT/mrf_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/gdrivers/data/mrf
for subdir in *; do
    cd $subdir
    printf "FUZZER_FRIENDLY_ARCHIVE\n" > $CUR_DIR/mrf_$subdir.tar
    for file in *; do
        printf "***NEWFILE***:$file\n" >> $CUR_DIR/mrf_$subdir.tar
        cat $file >> $CUR_DIR/mrf_$subdir.tar
    done
    cd ..
done
cd $CUR_DIR
zip -r $OUT/mrf_fuzzer_seed_corpus.zip mrf_*.tar >/dev/null
rm mrf_*.tar

echo "Building envi_fuzzer_seed_corpus.zip"
rm -f $OUT/envi_fuzzer_seed_corpus.zip

printf "FUZZER_FRIENDLY_ARCHIVE\n" > aea.tar
printf "***NEWFILE***:my.hdr\n" >> aea.tar
cat $(dirname $0)/../../autotest/gdrivers/data/aea.hdr >> aea.tar
printf "***NEWFILE***:my.dat\n" >> aea.tar
cat $(dirname $0)/../../autotest/gdrivers/data/aea.dat >> aea.tar
zip -r $OUT/envi_fuzzer_seed_corpus.zip aea.tar >/dev/null
rm aea.tar

printf "FUZZER_FRIENDLY_ARCHIVE\n" > aea_compressed.tar
printf "***NEWFILE***:my.hdr\n" >> aea_compressed.tar
cat $(dirname $0)/../../autotest/gdrivers/data/aea_compressed.hdr >> aea_compressed.tar
printf "***NEWFILE***:my.dat\n" >> aea_compressed.tar
cat $(dirname $0)/../../autotest/gdrivers/data/aea_compressed.dat >> aea_compressed.tar
zip -r $OUT/envi_fuzzer_seed_corpus.zip aea_compressed.tar >/dev/null
rm aea_compressed.tar

echo "Building aig_fuzzer_seed_corpus.zip"
printf "FUZZER_FRIENDLY_ARCHIVE\n" > aig.tar
for x in hdr.adf sta.adf dblbnd.adf vat.adf w001001.adf abc3x1.clr prj.adf w001001x.adf; do
    printf "***NEWFILE***:$x\n" >> aig.tar
    cat $(dirname $0)/../../autotest/gdrivers/data/abc3x1/$x >> aig.tar
done
rm -f $OUT/aig_fuzzer_seed_corpus.zip
zip -r $OUT/aig_fuzzer_seed_corpus.zip aig.tar >/dev/null
rm aig.tar

echo "Building get_jpeg2000_structure_fuzzer_seed_corpus.zip"
rm -f $OUT/get_jpeg2000_structure_fuzzer_seed_corpus.zip
cd $(dirname $0)/../../autotest/gdrivers/data
zip -r $OUT/get_jpeg2000_structure_fuzzer_seed_corpus.zip *.jp2 >/dev/null
cd $OLDPWD


echo "Building gdal_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/gcore/data
rm -f $OUT/gdal_fuzzer_seed_corpus.zip
zip -r $OUT/gdal_fuzzer_seed_corpus.zip . >/dev/null
cd $OLDPWD
cd $(dirname $0)/../../autotest/gdrivers/data
zip -r $OUT/gdal_fuzzer_seed_corpus.zip . >/dev/null
cd $OLDPWD

echo "Building gdal_filesystem_fuzzer_seed_corpus.zip"
cp $OUT/gdal_fuzzer_seed_corpus.zip $OUT/gdal_filesystem_fuzzer_seed_corpus.zip

echo "Building gdal_sdts_fuzzer_seed_corpus.zip"
rm -f $OUT/gdal_sdts_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/gdrivers/data/STDS_1107834_truncated
printf "FUZZER_FRIENDLY_ARCHIVE\n" > $CUR_DIR/gdal_sdts.tar
for file in *.DDF; do
    printf "***NEWFILE***:$file\n" >> $CUR_DIR/gdal_sdts.tar
    cat $file >> $CUR_DIR/gdal_sdts.tar
done
cd $CUR_DIR
zip -r $OUT/gdal_sdts_fuzzer_seed_corpus.zip gdal_sdts.tar >/dev/null
rm gdal_sdts.tar

echo "Building ogr_sdts_fuzzer_seed_corpus.zip"
rm -f $OUT/ogr_sdts_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/ogr/data/D3607551_rd0s_1_sdts_truncated
printf "FUZZER_FRIENDLY_ARCHIVE\n" > $CUR_DIR/ogr_sdts.tar
for file in *.DDF; do
    printf "***NEWFILE***:$file\n" >> $CUR_DIR/ogr_sdts.tar
    cat $file >> $CUR_DIR/ogr_sdts.tar
done
cd $CUR_DIR
zip -r $OUT/ogr_sdts_fuzzer_seed_corpus.zip ogr_sdts.tar >/dev/null
rm ogr_sdts.tar

echo "Building ogr_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data
rm -f $OUT/ogr_fuzzer_seed_corpus.zip
zip -r $OUT/ogr_fuzzer_seed_corpus.zip . >/dev/null
cd $OLDPWD

echo "Building cad_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data/cad
rm -f $OUT/cad_fuzzer_seed_corpus.zip
zip -r $OUT/cad_fuzzer_seed_corpus.zip . >/dev/null
cd $OLDPWD

echo "Building csv_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data
rm -f $OUT/csv_fuzzer_seed_corpus.zip
zip -r $OUT/csv_fuzzer_seed_corpus.zip *.csv >/dev/null
cd $OLDPWD

echo "Building bna_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data
rm -f $OUT/bna_fuzzer_seed_corpus.zip
zip -r $OUT/bna_fuzzer_seed_corpus.zip *.bna >/dev/null
cd $OLDPWD

echo "Building xlsx_fuzzer_seed_corpus.zip"
rm -f $OUT/xlsx_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/ogr/data
for filename in *.xlsx; do
    mkdir tmpxlsx
    cd tmpxlsx
    unzip ../$filename
    printf "FUZZER_FRIENDLY_ARCHIVE\n" > $CUR_DIR/xlsx_$filename.tar
    for i in `find -type f`; do
        printf "***NEWFILE***:$i\n" >> $CUR_DIR/xlsx_$filename.tar
        cat $i >> $CUR_DIR/xlsx_$filename.tar
    done
    cd ..
    rm -rf tmpxlsx
done
cd $CUR_DIR
zip -r $OUT/xlsx_fuzzer_seed_corpus.zip xlsx_*.tar >/dev/null
rm xlsx_*.tar

echo "Building ods_fuzzer_seed_corpus.zip"
rm -f $OUT/ods_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/ogr/data
for filename in *.ods; do
    mkdir tmpods
    cd tmpods
    unzip ../$filename
    printf "FUZZER_FRIENDLY_ARCHIVE\n" > $CUR_DIR/ods_$filename.tar
    for i in `find -type f`; do
        printf "***NEWFILE***:$i\n" >> $CUR_DIR/ods_$filename.tar
        cat $i >> $CUR_DIR/ods_$filename.tar
    done
    cd ..
    rm -rf tmpods
done
cd $CUR_DIR
zip -r $OUT/ods_fuzzer_seed_corpus.zip ods_*.tar >/dev/null
rm ods_*.tar


echo "Building rec_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data
rm -f $OUT/rec_fuzzer_seed_corpus.zip
zip -r $OUT/rec_fuzzer_seed_corpus.zip *.rec >/dev/null
cd $OLDPWD

echo "Building shape_fuzzer_seed_corpus.zip"
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
rm poly_shp.tar

echo "Building mitab_tab_fuzzer_seed_corpus.zip"
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
rm all_geoms_tab.tar

echo "Building mitab_mif_fuzzer_seed_corpus.zip"
printf "FUZZER_FRIENDLY_ARCHIVE\n" > small_mif.tar
printf "***NEWFILE***:my.mif\n" >> small_mif.tar
cat $(dirname $0)/../../autotest/ogr/data/small.mif >> small_mif.tar
printf "***NEWFILE***:my.mid\n" >> small_mif.tar
cat $(dirname $0)/../../autotest/ogr/data/small.mid >> small_mif.tar
rm -f $OUT/mitab_mif_fuzzer_seed_corpus.zip
zip -r $OUT/mitab_mif_fuzzer_seed_corpus.zip small_mif.tar >/dev/null
rm small_mif.tar

echo "Building openfilegdb_fuzzer_seed_corpus.zip"
rm -rf testopenfilegdb.gdb
unzip $(dirname $0)/../../autotest/ogr/data/testopenfilegdb.gdb.zip >/dev/null
printf "FUZZER_FRIENDLY_ARCHIVE\n" > testopenfilegdb.gdb.tar
for f in testopenfilegdb.gdb/*; do
    printf "***NEWFILE***:$f\n" >> testopenfilegdb.gdb.tar
    cat $f >> testopenfilegdb.gdb.tar
done

rm -rf testopenfilegdb92.gdb
unzip $(dirname $0)/../../autotest/ogr/data/testopenfilegdb92.gdb.zip >/dev/null
printf "FUZZER_FRIENDLY_ARCHIVE\n" > testopenfilegdb92.gdb.tar
for f in testopenfilegdb92.gdb/*; do
    printf "***NEWFILE***:$f\n" >> testopenfilegdb92.gdb.tar
    cat $f >> testopenfilegdb92.gdb.tar
done

rm -f $OUT/openfilegdb_fuzzer_seed_corpus.zip
zip -r $OUT/openfilegdb_fuzzer_seed_corpus.zip testopenfilegdb.gdb.tar testopenfilegdb92.gdb.tar >/dev/null
rm -r testopenfilegdb.gdb
rm testopenfilegdb.gdb.tar
rm -r testopenfilegdb92.gdb
rm testopenfilegdb92.gdb.tar

echo "Copying data to $OUT"
cp $(dirname $0)/../data/* $OUT
