#!/bin/sh

if ! test -d $HOME/gdal_vce2008; then
    echo "$HOME/gdal_vce2008 does not exist. Run ./prepare-gdal-vce2008.sh first"
    exit 1
fi

cd $HOME/gdal_vce2008/gdal

# Hack for libjpeg12
if ! test -f frmts/jpeg/libjpeg12/jmorecfg.h; then
    cd frmts/jpeg/libjpeg12
    cp ../libjpeg/*.h .
    cp jmorecfg.h.12 jmorecfg.h
    cp ../libjpeg/*.c .
    mv jcapimin.c jcapimin12.c
    mv jcapistd.c jcapistd12.c
    mv jccoefct.c jccoefct12.c
    mv jccolor.c jccolor12.c
    mv jcdctmgr.c jcdctmgr12.c
    mv jchuff.c jchuff12.c
    mv jcinit.c jcinit12.c
    mv jcmainct.c jcmainct12.c
    mv jcmarker.c jcmarker12.c
    mv jcmaster.c jcmaster12.c
    mv jcomapi.c jcomapi12.c
    mv jcparam.c jcparam12.c
    mv jcphuff.c jcphuff12.c
    mv jcprepct.c jcprepct12.c
    mv jcsample.c jcsample12.c
    mv jctrans.c jctrans12.c
    mv jdapimin.c jdapimin12.c
    mv jdapistd.c jdapistd12.c
    mv jdatadst.c jdatadst12.c
    mv jdatasrc.c jdatasrc12.c
    mv jdcoefct.c jdcoefct12.c
    mv jdcolor.c jdcolor12.c
    mv jddctmgr.c jddctmgr12.c
    mv jdhuff.c jdhuff12.c
    mv jdinput.c jdinput12.c
    mv jdmainct.c jdmainct12.c
    mv jdmarker.c jdmarker12.c
    mv jdmaster.c jdmaster12.c
    mv jdmerge.c jdmerge12.c
    mv jdphuff.c jdphuff12.c
    mv jdpostct.c jdpostct12.c
    mv jdsample.c jdsample12.c
    mv jdtrans.c jdtrans12.c
    mv jerror.c jerror12.c
    mv jfdctflt.c jfdctflt12.c
    mv jfdctfst.c jfdctfst12.c
    mv jfdctint.c jfdctint12.c
    mv jidctflt.c jidctflt12.c
    mv jidctfst.c jidctfst12.c
    mv jidctint.c jidctint12.c
    mv jidctred.c jidctred12.c
    mv jmemansi.c jmemansi12.c
    mv jmemmgr.c jmemmgr12.c
    mv jquant1.c jquant112.c
    mv jquant2.c jquant212.c
    mv jutils.c jutils12.c
    cd $OLDPWD
fi

wine cmd /c build_vce2008.bat
