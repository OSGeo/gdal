#!/bin/sh

DATADIR=testdata

for file in 1183CEL0.DDF CA49995B.000 TWFLA009.DDF SC01CATD.DDF SC01LE01.DDF TSTPAGEO.DDF ; \
  do

  echo "---------------------------------------------------------------------"
  echo "-- $file"
  echo "---------------------------------------------------------------------"
  8211dump $DATADIR/$file | head -500

done



