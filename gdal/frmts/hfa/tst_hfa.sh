#!/bin/sh

#
#	The -f force flag will force new logs to overwrite existing ones.
#
FORCE=0
if test "$1" = "-f" ; then
  FORCE=1
  shift
fi

#
#	Loop over all passed imagine files. 
#
for a in $* ; do
  BASE=`dirname $a`/`basename $a .img`
  hfatest -dt -dd $a > ${BASE}.rpt.new
  
  if test $FORCE = 1 ; then
    mv ${BASE}.rpt.new ${BASE}.rpt
    continue;
  fi

  if diff ${BASE}.rpt ${BASE}.rpt.new ; then
    rm ${BASE}.rpt.new 
  else
    echo Differences between ${BASE}.rpt and ${BASE}.rpt.new
  fi
done
