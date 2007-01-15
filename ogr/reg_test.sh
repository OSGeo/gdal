#!/bin/sh

# Perform a simple regression test.  This will only work on my 
# system since it has hardcoded assumptions about locations of
# test datasets.

DATADIR=/u/data

test_ogrsf -ro $DATADIR/esri/shape/eg_data

test_ogrsf -ro $DATADIR/ntf
test_ogrsf -ro $DATADIR/ntf/osni
test_ogrsf -ro $DATADIR/ntf/dtm/DTMSS68.NTF

test_ogrsf -ro $DATADIR/sdts/cape_royal_AZ_DLG24/HP01CATD.DDF
test_ogrsf -ro $DATADIR/sdts/safe_hy_bug/HP01CATD.DDF
test_ogrsf -ro $DATADIR/sdts/michigan_2000000/MIBDCATD.DDF
test_ogrsf -ro $DATADIR/sdts/safe_dem/7096CATD.DDF

test_ogrsf -ro $DATADIR/s57/newfiles/I/CA39995I.000



