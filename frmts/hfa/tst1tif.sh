#!/bin/sh

TIF_FILE=$1
LOG_FILE=$2

if test "$LOG_FILE" = "" ; then
  echo "Usage: tst1tif.sh tif_file log_file"
  exit 1
fi

rm -f _tmptif.log

if test \! -r $TIF_FILE ; then
  echo $TIF_FILE not found.
  exit 1
fi

tiffinfo $TIF_FILE > _tmptif.log 2> /dev/null
tiffdump $TIF_FILE >> _tmptif.log
listgeo $TIF_FILE >> _tmptif.log

if test \! -r $LOG_FILE ; then 
  echo "No existing log $LOG_FILE found, using newly generated log."
  cp _tmptif.log $LOG_FILE
  rm -f _tmptif.log
  exit 0 
fi

if diff $LOG_FILE _tmptif.log ; then
  rm -f _tmptif.log
  exit 0
else
  echo Difference found between ${LOG_FILE}.new and $LOG_FILE
  cp _tmptif.log ${LOG_FILE}.new
  rm -f _tmptif.log
  exit 1
fi





