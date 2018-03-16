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


echo "Building gdal_vector_translate_fuzzer_seed_corpus.zip"
echo "FUZZER_FRIENDLY_ARCHIVE" > test.tar
echo "***NEWFILE***:cmd.txt" >> test.tar
echo "non_significant_output_name" >> test.tar
echo "-f" >> test.tar
echo "Memory" >> test.tar
echo "-s_srs" >> test.tar
echo "EPSG:4326" >> test.tar
echo "-t_srs" >> test.tar
echo "EPSG:32631" >> test.tar
echo "first" >> test.tar
echo "second" >> test.tar
echo "***NEWFILE***:in/first.csv" >> test.tar
echo "int_field,float_field,string_field,WKT" >> test.tar
echo "1,2.34,\"foo\",\"POINT(1 2)\"" >> test.tar
echo "***NEWFILE***:in/first.csvt" >> test.tar
echo "Integer,Real,String,WKT" >> test.tar
echo "***NEWFILE***:in/second.csv" >> test.tar
echo "int_field,float_field,string_field,WKT" >> test.tar
echo "1,2.34,\"foo\",\"POINT(1 2)\"" >> test.tar
echo "***NEWFILE***:in/second.csvt" >> test.tar
echo "Integer,Real,String,WKT" >> test.tar
rm -f $OUT/gdal_vector_translate_fuzzer_seed_corpus.zip
zip -r $OUT/gdal_vector_translate_fuzzer_seed_corpus.zip test.tar >/dev/null
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

echo "Building ehdr_fuzzer_seed_corpus.zip"
rm -f $OUT/ehdr_fuzzer_seed_corpus.zip

printf "FUZZER_FRIENDLY_ARCHIVE\n" > ehdr11.tar
printf "***NEWFILE***:my.hdr\n" >> ehdr11.tar
cat $(dirname $0)/../../autotest/gdrivers/data/ehdr11.hdr >> ehdr11.tar
printf "***NEWFILE***:my.dat\n" >> ehdr11.tar
cat $(dirname $0)/../../autotest/gdrivers/data/ehdr11.flt >> ehdr11.tar
zip -r $OUT/ehdr_fuzzer_seed_corpus.zip ehdr11.tar >/dev/null
rm ehdr11.tar

echo "Building genbin_fuzzer_seed_corpus.zip"
rm -f $OUT/genbin_fuzzer_seed_corpus.zip

printf "FUZZER_FRIENDLY_ARCHIVE\n" > genbin.tar
printf "***NEWFILE***:my.hdr\n" >> genbin.tar
cat $(dirname $0)/../../autotest/gdrivers/data/tm4628_96.hdr >> genbin.tar
printf "***NEWFILE***:my.bil\n" >> genbin.tar
cat $(dirname $0)/../../autotest/gdrivers/data/tm4628_96.bil >> genbin.tar
zip -r $OUT/genbin_fuzzer_seed_corpus.zip genbin.tar >/dev/null
rm genbin.tar

echo "Building isce_fuzzer_seed_corpus.zip"
rm -f $OUT/isce_fuzzer_seed_corpus.zip

printf "FUZZER_FRIENDLY_ARCHIVE\n" > isce.tar
printf "***NEWFILE***:isce.slc\n" >> isce.tar
cat $(dirname $0)/../../autotest/gdrivers/data/isce.slc >> isce.tar
printf "***NEWFILE***:isce.slc.xml\n" >> isce.tar
cat $(dirname $0)/../../autotest/gdrivers/data/isce.slc.xml >> isce.tar
zip -r $OUT/isce_fuzzer_seed_corpus.zip isce.tar >/dev/null
rm isce.tar

echo "Building roipac_fuzzer_seed_corpus.zip"
rm -f $OUT/roipac_fuzzer_seed_corpus.zip

printf "FUZZER_FRIENDLY_ARCHIVE\n" > roipac.tar
printf "***NEWFILE***:srtm.dem\n" >> roipac.tar
cat $(dirname $0)/../../autotest/gdrivers/data/srtm.dem >> roipac.tar
printf "***NEWFILE***:srtm.dem.rsc\n" >> roipac.tar
cat $(dirname $0)/../../autotest/gdrivers/data/srtm.dem.rsc >> roipac.tar
zip -r $OUT/roipac_fuzzer_seed_corpus.zip roipac.tar >/dev/null
rm roipac.tar

echo "Building rraster_fuzzer_seed_corpus.zip"
rm -f $OUT/rraster_fuzzer_seed_corpus.zip

printf "FUZZER_FRIENDLY_ARCHIVE\n" > rraster.tar
printf "***NEWFILE***:my.grd\n" >> rraster.tar
cat $(dirname $0)/../../autotest/gdrivers/data/byte_rraster.grd >> rraster.tar
printf "***NEWFILE***:my.gri\n" >> rraster.tar
cat $(dirname $0)/../../autotest/gdrivers/data/byte_rraster.gri >> rraster.tar
zip -r $OUT/rraster_fuzzer_seed_corpus.zip rraster.tar >/dev/null
rm rraster.tar

echo "Building gdal_vrt_fuzzer_seed_corpus.zip"
rm -f $OUT/gdal_vrt_fuzzer_seed_corpus.zip

printf "FUZZER_FRIENDLY_ARCHIVE\n" > gdal_vrt.tar
printf "***NEWFILE***:byte.tif\n" >> gdal_vrt.tar
cat $(dirname $0)/../../autotest/gcore/data/byte.tif >> gdal_vrt.tar
printf "***NEWFILE***:test.vrt\n" >> gdal_vrt.tar
cat $(dirname $0)/../../autotest/gcore/data/byte.vrt >> gdal_vrt.tar
zip -r $OUT/gdal_vrt_fuzzer_seed_corpus.zip gdal_vrt.tar >/dev/null
rm gdal_vrt.tar

printf "FUZZER_FRIENDLY_ARCHIVE\n" > gdal_vrt_rawlink.tar
printf "***NEWFILE***:small.raw\n" >> gdal_vrt_rawlink.tar
cat $(dirname $0)/../../autotest/gdrivers/data/small.raw >> gdal_vrt_rawlink.tar
printf "***NEWFILE***:test.vrt\n" >> gdal_vrt_rawlink.tar
cat $(dirname $0)/../../autotest/gdrivers/data/small.vrt >> gdal_vrt_rawlink.tar
zip -r $OUT/gdal_vrt_fuzzer_seed_corpus.zip gdal_vrt_rawlink.tar >/dev/null
rm gdal_vrt_rawlink.tar


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
CUR_DIR=$PWD
cd $(dirname $0)/../../autotest/ogr/data
rm -f $OUT/ogr_fuzzer_seed_corpus.zip
zip -r $OUT/ogr_fuzzer_seed_corpus.zip . >/dev/null
cd mvt
zip $OUT/ogr_fuzzer_seed_corpus.zip * >/dev/null
cd ..
cd $CUR_DIR

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
    unzip ../$filename >/dev/null
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
    unzip ../$filename >/dev/null
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
for ext in tab map dat id; do
    printf "***NEWFILE***:my.$ext\n" >> all_geoms_tab.tar
    cat $(dirname $0)/../../autotest/ogr/data/all_geoms.$ext >> all_geoms_tab.tar
done

printf "FUZZER_FRIENDLY_ARCHIVE\n" > poly_indexed.tar
for ext in tab map dat id; do
    printf "***NEWFILE***:my.$ext\n" >> poly_indexed.tar
    cat $(dirname $0)/../../autotest/ogr/data/poly_indexed.$ext >> poly_indexed.tar
done

printf "FUZZER_FRIENDLY_ARCHIVE\n" > view.tar
printf "***NEWFILE***:my.tab\n" >> view.tar
cat $(dirname $0)/../../autotest/ogr/data/mitab/view_first_table_second_table.tab >> view.tar
for ext in tab map dat id ind; do
    printf "***NEWFILE***:first_table.$ext\n" >> view.tar
    cat $(dirname $0)/../../autotest/ogr/data/mitab/first_table.$ext >> view.tar
done
for ext in tab map dat id ind; do
    printf "***NEWFILE***:second_table.$ext\n" >> view.tar
    cat $(dirname $0)/../../autotest/ogr/data/mitab/second_table.$ext >> view.tar
done

rm -f $OUT/mitab_tab_fuzzer_seed_corpus.zip
zip -r $OUT/mitab_tab_fuzzer_seed_corpus.zip all_geoms_tab.tar poly_indexed.tar view.tar >/dev/null
rm all_geoms_tab.tar poly_indexed.tar view.tar

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

echo "Building avcbin_fuzzer_seed_corpus.zip"
rm -f $OUT/avcbin_fuzzer_seed_corpus.zip
CUR_DIR=$PWD
cd  $(dirname $0)/../../autotest/ogr/data/testavc
printf "FUZZER_FRIENDLY_ARCHIVE\n" > $CUR_DIR/avcbin.tar
for f in `find . -type f`; do
    printf "***NEWFILE***:$f\n" >> $CUR_DIR/avcbin.tar
    cat $f >> $CUR_DIR/avcbin.tar
done
cd $CUR_DIR
zip -r $OUT/avcbin_fuzzer_seed_corpus.zip avcbin.tar >/dev/null
rm avcbin.tar

echo "Building avce00_fuzzer_seed_corpus.zip"
rm -f $OUT/avce00_fuzzer_seed_corpus.zip
cd $(dirname $0)/../../autotest/ogr/data
rm -f $OUT/avce00_fuzzer_seed_corpus.zip
zip -r $OUT/avce00_fuzzer_seed_corpus.zip *.e00 >/dev/null
cd $OLDPWD

echo "Building gml_fuzzer_seed_corpus.zip"
rm -f $OUT/gml_fuzzer_seed_corpus.zip
printf "FUZZER_FRIENDLY_ARCHIVE\n" > $CUR_DIR/archsites_gml.tar
printf "***NEWFILE***:test.gml\n" >> $CUR_DIR/archsites_gml.tar
cat $(dirname $0)/../../autotest/ogr/data/archsites.gml >> $CUR_DIR/archsites_gml.tar
printf "***NEWFILE***:test.xsd\n" >> $CUR_DIR/archsites_gml.tar
cat $(dirname $0)/../../autotest/ogr/data/archsites.xsd >> $CUR_DIR/archsites_gml.tar
zip -r $OUT/gml_fuzzer_seed_corpus.zip archsites_gml.tar >/dev/null
rm archsites_gml.tar


echo "Copying data to $OUT"
cp $(dirname $0)/../data/* $OUT
