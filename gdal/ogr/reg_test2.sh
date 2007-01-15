#!/bin/sh

# Perform a simple regression test.  This will only work on my 
# system since it has hardcoded assumptions about locations of
# test datasets.

DATADIR=/u/data

ogrinfo -ro $DATADIR/esri/shape/eg_data 3dpoints brklinz | head -100
ogrinfo -ro $DATADIR/esri/shape/eg_data polygon | head -100 

ogrinfo -ro $DATADIR/ntf PANORAMA_POINT | head -100
ogrinfo -ro $DATADIR/ntf STRATEGI_TEXT | head -100
ogrinfo -ro $DATADIR/ntf BASEDATA_NODE | head -100
ogrinfo -ro $DATADIR/ntf BASEDATA_LINE | head -100

ogrinfo -ro $DATADIR/sdts/cape_royal_AZ_DLG24/HP01CATD.DDF LE01 | head -100
ogrinfo -ro $DATADIR/sdts/cape_royal_AZ_DLG24/HP01CATD.DDF PC01 | head -100
ogrinfo -ro $DATADIR/sdts/cape_royal_AZ_DLG24/HP01CATD.DDF AHPF | head -100

ogrinfo -ro $DATADIR/s57/newfiles/I/CA39995I.000 
ogrinfo -ro $DATADIR/s57/newfiles/I/CA39995I.000 PILBOP C_ASSO RUNWAY WATFAL
ogrinfo -ro $DATADIR/s57/newfiles/I/CA39995I.000 SOUNDG | head -100 


