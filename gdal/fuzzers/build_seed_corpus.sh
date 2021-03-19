#!/bin/bash
# WARNING: this script is used by https://github.com/google/oss-fuzz/blob/master/projects/gdal/build.sh
# and should not be renamed or moved without updating the above

set -e

if [ "$OUT" == "" ]; then
    echo "OUT env var not defined"
    exit 1
fi

echo "Building gdal_translate_fuzzer_seed_corpus.zip"
cat > test.tar <<EOF
FUZZER_FRIENDLY_ARCHIVE
***NEWFILE***:cmd.txt
-outsize
20
20
-of
GTiff
-b
1
-ot
Byte
-r
nearest
-a_srs
EPSG:26711
-stats
-scale
-mo
FOO=BAR
-co
COMPRESS=NONE
-srcwin
0
0
20
20
***NEWFILE***:in
EOF
cat $(dirname $0)/../../autotest/gcore/data/byte.tif >> test.tar
rm -f $OUT/gdal_translate_fuzzer_seed_corpus.zip
zip -r $OUT/gdal_translate_fuzzer_seed_corpus.zip test.tar >/dev/null
rm test.tar


echo "Building gdal_vector_translate_fuzzer_seed_corpus.zip"
cat > test.tar <<EOF
FUZZER_FRIENDLY_ARCHIVE
***NEWFILE***:cmd.txt
non_significant_output_name
-f
Memory
-s_srs
EPSG:4326
-t_srs
EPSG:32631
first
second
***NEWFILE***:in/first.csv
int_field,float_field,string_field,WKT
1,2.34,\"foo\",\"POINT(1 2)\"
***NEWFILE***:in/first.csvt
Integer,Real,String,WKT
***NEWFILE***:in/second.csv
int_field,float_field,string_field,WKT
1,2.34,\"foo\",\"POINT(1 2)\"
***NEWFILE***:in/second.csvt
Integer,Real,String,WKT
EOF
rm -f $OUT/gdal_vector_translate_fuzzer_seed_corpus.zip
zip -r $OUT/gdal_vector_translate_fuzzer_seed_corpus.zip test.tar >/dev/null
rm test.tar

echo "Building gtiff_fuzzer_seed_corpus.zip"
rm -f $OUT/gtiff_fuzzer_seed_corpus.zip
cd $(dirname $0)/../../autotest/gcore/data
zip -r $OUT/gtiff_fuzzer_seed_corpus.zip ./*.tif >/dev/null
cd $OLDPWD
cd $(dirname $0)/../../autotest/gdrivers/data
zip -r $OUT/gtiff_fuzzer_seed_corpus.zip ./*.tif >/dev/null
cd $OLDPWD

echo "Building hfa_fuzzer_seed_corpus.zip"
rm -f $OUT/hfa_fuzzer_seed_corpus.zip
cd $(dirname $0)/../../autotest/gcore/data
zip -r $OUT/hfa_fuzzer_seed_corpus.zip ./*.img >/dev/null
cd $OLDPWD
cd $(dirname $0)/../../autotest/gdrivers/data/hfa
zip -r $OUT/hfa_fuzzer_seed_corpus.zip ./*.img >/dev/null
cd $OLDPWD

echo "Building adrg_fuzzer_seed_corpus.zip"
{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:ABCDEF01.GEN\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/adrg/SMALL_ADRG/ABCDEF01.GEN
    printf "***NEWFILE***:ABCDEF01.IMG\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/adrg/SMALL_ADRG/ABCDEF01.IMG
} > adrg.tar
rm -f $OUT/adrg_fuzzer_seed_corpus.zip
zip -r $OUT/adrg_fuzzer_seed_corpus.zip adrg.tar >/dev/null
rm adrg.tar

echo "Building srp_fuzzer_seed_corpus.zip"
{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:FKUSRP01.GEN\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/srp/USRP_PCB0/FKUSRP01.GEN
    printf "***NEWFILE***:FKUSRP01.IMG\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/srp/USRP_PCB0/FKUSRP01.IMG
    printf "***NEWFILE***:FKUSRP01.QAL\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/srp/USRP_PCB0/FKUSRP01.QAL
} > srp.tar
rm -f $OUT/srp_fuzzer_seed_corpus.zip
zip -r $OUT/srp_fuzzer_seed_corpus.zip srp.tar >/dev/null
rm srp.tar


echo "Building mrf_fuzzer_seed_corpus.zip"
rm -f $OUT/mrf_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/gdrivers/data/mrf
for subdir in *; do
    (cd $subdir
     printf "FUZZER_FRIENDLY_ARCHIVE\\n" > $CUR_DIR/mrf_$subdir.tar
     for file in *; do
         printf "***NEWFILE***:%s\\n" "$file" >> $CUR_DIR/mrf_$subdir.tar
         cat $file >> $CUR_DIR/mrf_$subdir.tar
     done
    )
done
cd $CUR_DIR
zip -r $OUT/mrf_fuzzer_seed_corpus.zip mrf_*.tar >/dev/null
rm mrf_*.tar

echo "Building envi_fuzzer_seed_corpus.zip"
rm -f $OUT/envi_fuzzer_seed_corpus.zip

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:my.hdr\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/envi/aea.hdr
    printf "***NEWFILE***:my.dat\\n" >> aea.tar
    cat $(dirname $0)/../../autotest/gdrivers/data/envi/aea.dat
} > aea.tar
zip -r $OUT/envi_fuzzer_seed_corpus.zip aea.tar >/dev/null
rm aea.tar

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:my.hdr\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/envi/aea_compressed.hdr
    printf "***NEWFILE***:my.dat\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/envi/aea_compressed.dat
} > aea_compressed.tar
zip -r $OUT/envi_fuzzer_seed_corpus.zip aea_compressed.tar >/dev/null
rm aea_compressed.tar

echo "Building ehdr_fuzzer_seed_corpus.zip"
rm -f $OUT/ehdr_fuzzer_seed_corpus.zip

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:my.hdr\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/ehdr/ehdr11.hdr
    printf "***NEWFILE***:my.dat\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/ehdr/ehdr11.flt
} > ehdr11.tar
zip -r $OUT/ehdr_fuzzer_seed_corpus.zip ehdr11.tar >/dev/null
rm ehdr11.tar

echo "Building genbin_fuzzer_seed_corpus.zip"
rm -f $OUT/genbin_fuzzer_seed_corpus.zip

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:my.hdr\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/genbin/tm4628_96.hdr
    printf "***NEWFILE***:my.bil\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/genbin/tm4628_96.bil
} > genbin.tar
zip -r $OUT/genbin_fuzzer_seed_corpus.zip genbin.tar >/dev/null
rm genbin.tar

echo "Building isce_fuzzer_seed_corpus.zip"
rm -f $OUT/isce_fuzzer_seed_corpus.zip

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:isce.slc\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/isce/isce.slc
    printf "***NEWFILE***:isce.slc.xml\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/isce/isce.slc.xml
} > isce.tar
zip -r $OUT/isce_fuzzer_seed_corpus.zip isce.tar >/dev/null
rm isce.tar

echo "Building roipac_fuzzer_seed_corpus.zip"
rm -f $OUT/roipac_fuzzer_seed_corpus.zip

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:srtm.dem\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/roipac/srtm.dem
    printf "***NEWFILE***:srtm.dem.rsc\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/roipac/srtm.dem.rsc
} > roipac.tar
zip -r $OUT/roipac_fuzzer_seed_corpus.zip roipac.tar >/dev/null
rm roipac.tar

echo "Building rraster_fuzzer_seed_corpus.zip"
rm -f $OUT/rraster_fuzzer_seed_corpus.zip

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:my.grd\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/rraster/byte_rraster.grd
    printf "***NEWFILE***:my.gri\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/rraster/byte_rraster.gri
} > rraster.tar
zip -r $OUT/rraster_fuzzer_seed_corpus.zip rraster.tar >/dev/null
rm rraster.tar

echo "Building gdal_vrt_fuzzer_seed_corpus.zip"
rm -f $OUT/gdal_vrt_fuzzer_seed_corpus.zip

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:byte.tif\\n"
    cat $(dirname $0)/../../autotest/gcore/data/byte.tif
    printf "***NEWFILE***:test.vrt\\n"
    cat $(dirname $0)/../../autotest/gcore/data/byte.vrt
} > gdal_vrt.tar
zip -r $OUT/gdal_vrt_fuzzer_seed_corpus.zip gdal_vrt.tar >/dev/null
rm gdal_vrt.tar

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:small.raw\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/small.raw
    printf "***NEWFILE***:test.vrt\\n"
    cat $(dirname $0)/../../autotest/gdrivers/data/small.vrt
} > gdal_vrt_rawlink.tar
zip -r $OUT/gdal_vrt_fuzzer_seed_corpus.zip gdal_vrt_rawlink.tar >/dev/null
rm gdal_vrt_rawlink.tar


echo "Building aig_fuzzer_seed_corpus.zip"
printf "FUZZER_FRIENDLY_ARCHIVE\\n" > aig.tar
for x in hdr.adf sta.adf dblbnd.adf vat.adf w001001.adf abc3x1.clr prj.adf w001001x.adf; do
    printf "***NEWFILE***:%s\\n" "$x" >> aig.tar
    cat $(dirname $0)/../../autotest/gdrivers/data/aigrid/abc3x1/$x >> aig.tar
done
rm -f $OUT/aig_fuzzer_seed_corpus.zip
zip -r $OUT/aig_fuzzer_seed_corpus.zip aig.tar >/dev/null
rm aig.tar

echo "Building get_jpeg2000_structure_fuzzer_seed_corpus.zip"
rm -f $OUT/get_jpeg2000_structure_fuzzer_seed_corpus.zip
cd $(dirname $0)/../../autotest/gdrivers/data/jpeg2000
zip -r $OUT/get_jpeg2000_structure_fuzzer_seed_corpus.zip ./*.jp2 ./*.j2k >/dev/null
cd $OLDPWD


echo "Building gdal_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/gcore/data
rm -f $OUT/gdal_fuzzer_seed_corpus.zip
find . -type f -exec zip -j $OUT/gdal_fuzzer_seed_corpus.zip {} \; >/dev/null
cd $OLDPWD
cd $(dirname $0)/../../autotest/gdrivers/data
find . -type f -exec zip -j $OUT/gdal_fuzzer_seed_corpus.zip {} \; >/dev/null
cd $OLDPWD

echo "Building gdal_filesystem_fuzzer_seed_corpus.zip"
cp $OUT/gdal_fuzzer_seed_corpus.zip $OUT/gdal_filesystem_fuzzer_seed_corpus.zip

echo "Building gdal_sdts_fuzzer_seed_corpus.zip"
rm -f $OUT/gdal_sdts_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/gdrivers/data/STDS_1107834_truncated
printf "FUZZER_FRIENDLY_ARCHIVE\\n" > $CUR_DIR/gdal_sdts.tar
for file in *.DDF; do
    printf "***NEWFILE***:%s\\n" "$file" >> $CUR_DIR/gdal_sdts.tar
    cat $file >> $CUR_DIR/gdal_sdts.tar
done
cd $CUR_DIR
zip -r $OUT/gdal_sdts_fuzzer_seed_corpus.zip gdal_sdts.tar >/dev/null
rm gdal_sdts.tar


echo "Building ers_fuzzer_seed_corpus.zip"
rm -f $OUT/ers_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/gdrivers/data/ers
{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:%s\\n" "test.ers"
    cat srtm.ers
    printf "***NEWFILE***:%s\\n" "test"
    cat srtm
} > $CUR_DIR/srtm.tar

{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:%s\\n" "test.ers"
    cat ers_dem.ers
    printf "***NEWFILE***:%s\\n" "test"
    cat ers_dem
} > $CUR_DIR/ers_dem.tar

cd $CUR_DIR
zip -r $OUT/ers_fuzzer_seed_corpus.zip srtm.tar ers_dem.tar >/dev/null
rm srtm.tar ers_dem.tar


echo "Building ogr_sdts_fuzzer_seed_corpus.zip"
rm -f $OUT/ogr_sdts_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/ogr/data/sdts/D3607551_rd0s_1_sdts_truncated
printf "FUZZER_FRIENDLY_ARCHIVE\\n" > $CUR_DIR/ogr_sdts.tar
for file in *.DDF; do
    printf "***NEWFILE***:%s\\n" "$file" >> $CUR_DIR/ogr_sdts.tar
    cat $file >> $CUR_DIR/ogr_sdts.tar
done
cd $CUR_DIR
zip -r $OUT/ogr_sdts_fuzzer_seed_corpus.zip ogr_sdts.tar >/dev/null
rm ogr_sdts.tar

echo "Building ogr_fuzzer_seed_corpus.zip"
CUR_DIR=$PWD
cd $(dirname $0)/../../autotest/ogr/data
rm -f $OUT/ogr_fuzzer_seed_corpus.zip
find . -type f -exec zip -j $OUT/ogr_fuzzer_seed_corpus.zip {} \; >/dev/null
cd $CUR_DIR

echo "Building cad_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data/cad
rm -f $OUT/cad_fuzzer_seed_corpus.zip
zip -r $OUT/cad_fuzzer_seed_corpus.zip . >/dev/null
cd $OLDPWD

echo "Building csv_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data/csv
rm -f $OUT/csv_fuzzer_seed_corpus.zip
zip -r $OUT/csv_fuzzer_seed_corpus.zip ./*.csv >/dev/null
cd $OLDPWD

echo "Building xlsx_fuzzer_seed_corpus.zip"
rm -f $OUT/xlsx_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/ogr/data/xlsx
for filename in *.xlsx; do
    mkdir tmpxlsx
    (cd tmpxlsx
     unzip ../$filename >/dev/null
     printf "FUZZER_FRIENDLY_ARCHIVE\\n" > $CUR_DIR/xlsx_$filename.tar
     find . -type f | while read -r i ; do
         printf "***NEWFILE***:%s\\n" "$i" >> $CUR_DIR/xlsx_$filename.tar
         cat $i >> $CUR_DIR/xlsx_$filename.tar
     done
    )
    rm -rf tmpxlsx
done
cd $CUR_DIR
zip -r $OUT/xlsx_fuzzer_seed_corpus.zip xlsx_*.tar >/dev/null
rm xlsx_*.tar

echo "Building ods_fuzzer_seed_corpus.zip"
rm -f $OUT/ods_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/ogr/data/ods
for filename in *.ods; do
    mkdir tmpods
    unzip -d tmpods $filename >/dev/null
    printf "FUZZER_FRIENDLY_ARCHIVE\\n" > $CUR_DIR/ods_$filename.tar
    find . -type f | while read -r i ; do
        printf "***NEWFILE***:%s\\n" "$i" >> $CUR_DIR/ods_$filename.tar
        cat $i >> $CUR_DIR/ods_$filename.tar
    done
    rm -rf tmpods
done
cd $CUR_DIR
zip -r $OUT/ods_fuzzer_seed_corpus.zip ods_*.tar >/dev/null
rm ods_*.tar


echo "Building rec_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data/rec
rm -f $OUT/rec_fuzzer_seed_corpus.zip
zip -r $OUT/rec_fuzzer_seed_corpus.zip ./*.rec >/dev/null
cd $OLDPWD

echo "Building shape_fuzzer_seed_corpus.zip"
{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n" > poly_shp.tar
    printf "***NEWFILE***:my.shp\\n" >> poly_shp.tar
    cat $(dirname $0)/../../autotest/ogr/data/poly.shp
    printf "***NEWFILE***:my.shx\\n"
    cat $(dirname $0)/../../autotest/ogr/data/poly.shx
    printf "***NEWFILE***:my.dbf\\n"
    cat $(dirname $0)/../../autotest/ogr/data/poly.dbf
    printf "***NEWFILE***:my.prj\\n"
    cat $(dirname $0)/../../autotest/ogr/data/poly.PRJ
} > poly_shp.tar
rm -f $OUT/shape_fuzzer_seed_corpus.zip
zip -r $OUT/shape_fuzzer_seed_corpus.zip poly_shp.tar >/dev/null
rm poly_shp.tar

echo "Building mitab_tab_fuzzer_seed_corpus.zip"

printf "FUZZER_FRIENDLY_ARCHIVE\\n" > all_geoms_tab.tar
for ext in tab map dat id; do
    printf "***NEWFILE***:my.%s\\n" "$ext" >> all_geoms_tab.tar
    cat $(dirname $0)/../../autotest/ogr/data/mitab/all_geoms.$ext >> all_geoms_tab.tar
done

printf "FUZZER_FRIENDLY_ARCHIVE\\n" > poly_indexed.tar
for ext in tab map dat id; do
    printf "***NEWFILE***:my.%s\\n" "$ext" >> poly_indexed.tar
    cat $(dirname $0)/../../autotest/ogr/data/mitab/poly_indexed.$ext >> poly_indexed.tar
done

printf "FUZZER_FRIENDLY_ARCHIVE\\n" > view.tar
printf "***NEWFILE***:my.tab\\n" >> view.tar
cat $(dirname $0)/../../autotest/ogr/data/mitab/view_first_table_second_table.tab >> view.tar
for ext in tab map dat id ind; do
    printf "***NEWFILE***:first_table.%s\\n" "$ext" >> view.tar
    cat $(dirname $0)/../../autotest/ogr/data/mitab/first_table.$ext >> view.tar
done
for ext in tab map dat id ind; do
    printf "***NEWFILE***:second_table.%s\\n" "$ext" >> view.tar
    cat $(dirname $0)/../../autotest/ogr/data/mitab/second_table.$ext >> view.tar
done

rm -f $OUT/mitab_tab_fuzzer_seed_corpus.zip
zip -r $OUT/mitab_tab_fuzzer_seed_corpus.zip all_geoms_tab.tar poly_indexed.tar view.tar >/dev/null
rm all_geoms_tab.tar poly_indexed.tar view.tar

echo "Building mitab_mif_fuzzer_seed_corpus.zip"
{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:my.mif\\n"
    cat $(dirname $0)/../../autotest/ogr/data/mitab/small.mif
    printf "***NEWFILE***:my.mid\\n" >> small_mif.tar
    cat $(dirname $0)/../../autotest/ogr/data/mitab/small.mid
} > small_mif.tar
rm -f $OUT/mitab_mif_fuzzer_seed_corpus.zip
zip -r $OUT/mitab_mif_fuzzer_seed_corpus.zip small_mif.tar >/dev/null
rm small_mif.tar

echo "Building openfilegdb_fuzzer_seed_corpus.zip"
rm -rf testopenfilegdb.gdb
unzip $(dirname $0)/../../autotest/ogr/data/filegdb/testopenfilegdb.gdb.zip >/dev/null
printf "FUZZER_FRIENDLY_ARCHIVE\\n" > testopenfilegdb.gdb.tar
for f in testopenfilegdb.gdb/*; do
    printf "***NEWFILE***:%s\\n" "$f" >> testopenfilegdb.gdb.tar
    cat $f >> testopenfilegdb.gdb.tar
done

rm -rf testopenfilegdb92.gdb
unzip $(dirname $0)/../../autotest/ogr/data/filegdb/testopenfilegdb92.gdb.zip >/dev/null
printf "FUZZER_FRIENDLY_ARCHIVE\\n" > testopenfilegdb92.gdb.tar
for f in testopenfilegdb92.gdb/*; do
    printf "***NEWFILE***:%s\\n" "$f" >> testopenfilegdb92.gdb.tar
    cat $f >> testopenfilegdb92.gdb.tar
done

rm -f $OUT/openfilegdb_fuzzer_seed_corpus.zip
zip -r $OUT/openfilegdb_fuzzer_seed_corpus.zip testopenfilegdb.gdb.tar testopenfilegdb92.gdb.tar >/dev/null
rm -r testopenfilegdb.gdb
rm testopenfilegdb.gdb.tar
rm -r testopenfilegdb92.gdb
rm testopenfilegdb92.gdb.tar

echo "Building avcbin_fuzzer_seed_corpus.zip"
rm -f $OUT/avcbin_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/ogr/data/avc/testavc
printf "FUZZER_FRIENDLY_ARCHIVE\\n" > $CUR_DIR/avcbin.tar
find . -type f | while read -r f ; do
    printf "***NEWFILE***:%s\\n" "$f" >> $CUR_DIR/avcbin.tar
    cat $f >> $CUR_DIR/avcbin.tar
done
cd $CUR_DIR
zip -r $OUT/avcbin_fuzzer_seed_corpus.zip avcbin.tar >/dev/null
rm avcbin.tar

echo "Building avce00_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data/avc
rm -f $OUT/avce00_fuzzer_seed_corpus.zip
zip -r $OUT/avce00_fuzzer_seed_corpus.zip ./*.e00 >/dev/null
cd $OLDPWD

echo "Building gml_fuzzer_seed_corpus.zip"
rm -f $OUT/gml_fuzzer_seed_corpus.zip
{
    printf "FUZZER_FRIENDLY_ARCHIVE\\n"
    printf "***NEWFILE***:test.gml\\n"
    cat $(dirname $0)/../../autotest/ogr/data/gml/archsites.gml
    printf "***NEWFILE***:test.xsd\\n"
    cat $(dirname $0)/../../autotest/ogr/data/gml/archsites.xsd
} > $CUR_DIR/archsites_gml.tar
zip -r $OUT/gml_fuzzer_seed_corpus.zip archsites_gml.tar >/dev/null
rm archsites_gml.tar

echo "Building fgb_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data/testfgb
rm -f $OUT/fgb_fuzzer_seed_corpus.zip
zip -r $OUT/fgb_fuzzer_seed_corpus.zip ./*.fgb >/dev/null
cd $OLDPWD

echo "Building lvbag_fuzzer_seed_corpus.zip"
cd $(dirname $0)/../../autotest/ogr/data/lvbag
rm -f $OUT/lvbag_fuzzer_seed_corpus.zip
zip -r $OUT/lvbag_fuzzer_seed_corpus.zip ./*.xml >/dev/null
cd $OLDPWD


echo "Copying data to $OUT"
cp $(dirname $0)/../data/* $OUT
