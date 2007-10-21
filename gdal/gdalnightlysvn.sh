#!/bin/sh
#
# $Id$
#
# Script generates GDAL nightly snapshot packages.
#
VERBOSE=0

if [ $# != 1 ] ; then
    echo "Missing SVN branch!"
    echo "Available branches: trunk, stable"
    exit 1
fi

if [ $# -eq 1 ] ; then
  BRANCH=$1
  if test ! "$BRANCH" = "trunk" -a ! "$BRANCH" = "stable"; then
    echo "Unknown branch passed!"
    echo "Available branches: trunk, stable"
    exit 1
  fi
fi

if test ${VERBOSE} = 1; then
	echo "Selecting GDAL branch: ${BRANCH}"
fi

GDAL="gdal"
SVNBRANCH="trunk"
if test "$BRANCH" = "stable"; then
  GDAL="gdal-1.4"
  SVNBRANCH="branches/1.4"
fi

CWD=/osgeo/gdal
DATE=`date +%Y%m%d`
DATEVER=`date +%Y.%m.%d`
NIGHTLYVER="svn-${BRANCH}-${DATEVER}"
BUILDER="./mkgdaldist.sh"
GDALDIR="${CWD}/${GDAL}"
DAILYDIR="${CWD}/gdal-web/dl/daily"
LOG="/dev/null"

if test ${VERBOSE} = 1; then
	LOG=/osgeo/gdal/nightly.log
	echo -n "Building GDAL Nightly: ${NIGHTLYVER}..."
fi

if test ! -d ${GDALDIR}; then
	mkdir ${GDALDIR}
fi

cd ${GDALDIR}
${BUILDER} ${NIGHTLYVER} -date ${DATE} -branch ${SVNBRANCH} >& ${LOG}
cd ${CWD}

if test ${VERBOSE} = 1; then
	echo "Done"
fi

if test ${VERBOSE} = 1; then
	echo -n "Uploading ${NIGHTLYVER} to daily download..."
fi

find ${DAILYDIR} -name '*${BRANCH}*'
if test $? -eq 0 ; then
	find ${DAILYDIR} -name '*${BRANCH}*' | xargs rm -f
fi
mv ${GDALDIR}/*${NIGHTLYVER}* ${DAILYDIR}
mv ${GDALDIR}/gdalsvn*.zip ${DAILYDIR}

if test ${VERBOSE} = 1; then
	echo "Done"
fi

