#!/bin/sh
#
# $Id$
#
# mkgdaldist.sh - prepares GDAL source distribution package

set -eu

# Doxygen 1.7.1 has a bug related to man pages. See https://trac.osgeo.org/gdal/ticket/6048
doxygen --version | xargs python -c "import sys; v = sys.argv[1].split('.'); v=int(v[0])*10000+int(v[1])*100+int(v[2]); sys.exit(v < 10704)"
rc=$?
if test $rc != 0; then
    echo "Wrong Doxygen version. 1.7.4 or later required"
    exit $rc;
fi

GITURL="https://github.com/OSGeo/gdal"

if [ $# -lt 1 ] ; then
  echo "Usage: mkgdaldist.sh <version> [-date date] [-branch branch|-tag tag] [-rc n] [-url url]"
  echo " <version> - version number used in name of generated archive."
  echo " -date     - date of package generation, current date used if not provided"
  echo " -branch   - path to git branch, master is used if not provided"
  echo " -tag      - path to git tag"
  echo " -rc       - gives a release candidate id to embed in filenames"
  echo " -url      - git url, ${GITURL} if not provided"
  echo "Example: mkgdaldist.sh 2.2.4 -branch v2.2.4 -rc RC1"
  echo "or       mkgdaldist.sh 2.3.0beta1 -tag v2.3.0beta1"
  exit
fi

#
# Processing script input arguments
#
GDAL_VERSION=$1
COMPRESSED_VERSION=$(echo "$GDAL_VERSION" | tr -d .)

forcedate=no
if test "$#" -ge 3; then
  if test "$2" = "-date" ; then
    forcedate=$3
    shift
    shift
  fi
fi

BRANCH="master"
if test "$#" -ge 3; then
  if test "$2" = "-branch"; then
    BRANCH=$3
    shift
    shift
  fi
fi

TAG=""
if test "$#" -ge 3; then
  if test "$2" = "-tag"; then
    TAG=$3
    shift
    shift
 fi
fi

RC=""
if test "$#" -ge 3; then
  if test "$2" = "-rc"; then
    RC=$3
    shift
    shift
  fi
fi

if test "$#" -ge 3; then
  if test "$2" = "-url"; then
    GITURL=$3
    shift
    shift
  fi
fi

#
# Checkout GDAL sources from the repository
#
echo "* Downloading GDAL sources from git..."
rm -rf dist_wrk
mkdir dist_wrk
cd dist_wrk

if test "$TAG" != ""; then
   echo "Generating package '${GDAL_VERSION}' from '${TAG}' tag"
   git clone "${GITURL}" gdal
else
   echo "Generating package '${GDAL_VERSION}' from '${BRANCH}' branch"
   git clone -b "${BRANCH}" --single-branch "${GITURL}" gdal
fi

if [ ! -d gdal ] ; then
	echo "git clone reported an error ... abandoning mkgdaldist"
	cd ..
	rm -rf dist_wrk
	exit
fi

cd gdal

if test "$TAG" != ""; then
   echo "Checkout tag $TAG"
   git checkout "$TAG" || exit 1
fi

#
# Make some updates and cleaning
#
if test "$forcedate" != "no" ; then
  echo "* Updating release date..."
  echo "Forcing Date To: $forcedate"
  rm -f gcore/gdal_new.h
  sed -e "/define GDAL_RELEASE_DATE/s/20[0-9][0-9][0-9][0-9][0-9][0-9]/$forcedate/" gcore/gdal.h > gcore/gdal_new.h
  mv gcore/gdal_new.h gcore/gdal.h
fi

echo "* Cleaning .git and .gitignore under $PWD..."
rm -rf .git
rm -f .gitignore

echo "* Substituting \$Id\$..."
find . -name "*.h" -o -name "*.c" -o -name "*.cpp" -o -name "*.dox" \
     -o -name "*.py" -o -name "*.i" -o -name "*.sh" -o -name "*.cs" \
     -o -name "*.java" -o -name "*.m4" -o -name "*.xml" \
     -o -name "*.xsd" | while read -r i ; do
    ID=$(basename "$i")
    ID="$ID $(git log -1 --format='%H %ai %aN' "$i" | sed 's/ +0000/Z/')"
    sed -i "s/\\\$Id\\\$/\\\$Id: ${ID} \\\$/" "$i"
done


CWD=${PWD}

#
# Generate ./configure
#
echo "* Generating ./configure..."
./autogen.sh
rm -rf autom4te.cache

#
# Generate man pages
#
echo "* Generating man pages..."
if test -d "man"; then
    rm -rf man
fi

if test -f "doc/Makefile"; then
    (cd doc; make man)
    mkdir -p man/man1
    cp doc/build/man/*.1 man/man1
    rm -rf doc/build
    rm -f doc/.doxygen_up_to_date
else
    (cat Doxyfile ; echo "ENABLED_SECTIONS=man"; echo "INPUT=apps swig/python/gdal-utils/scripts"; echo "FILE_PATTERNS=*.cpp *.dox"; echo "GENERATE_HTML=NO"; echo "GENERATE_MAN=YES"; echo "QUIET=YES") | doxygen -
    rm -f doxygen_sqlite3.db
    rm -f man/man1/*_dist_wrk_gdal_gdal_apps_.1
fi

if test ! -d "man"; then
    echo " make man failed"
fi

cd "$CWD"

# They currently require SWIG 1.3.X, which is not convenient as we need
# newer SWIG for newer Python versions
echo "SWIG C# interfaces *NOT* generated !"

#
# Generate SWIG interface for C#
#
#echo "* Generating SWIG C# interfaces..."
#CWD=${PWD}
#cd swig/csharp
#./mkinterface.sh
#cd ${CWD}

#
# Generate SWIG interface for Perl
#
echo "* Generating SWIG Perl interfaces..."
CWD=${PWD}

rm -f swig/perl/*wrap*
touch GDALmake.opt
(cd swig/perl && make generate)
rm GDALmake.opt

#
# Make distribution packages
#
echo "* Making distribution packages..."
rm -f VERSION
echo "$GDAL_VERSION" > VERSION

cd ..
mv gdal/autotest "gdalautotest-${GDAL_VERSION}"
mv gdal "gdal-${GDAL_VERSION}"

rm -f "../gdal-${GDAL_VERSION}${RC}.tar.xz" "../gdal-${GDAL_VERSION}${RC}.tar.gz" "../gdal${COMPRESSED_VERSION}${RC}.zip"

tar cf "../gdal-${GDAL_VERSION}${RC}.tar" "gdal-${GDAL_VERSION}"
xz -k9e "../gdal-${GDAL_VERSION}${RC}.tar"
gzip -9 "../gdal-${GDAL_VERSION}${RC}.tar"
zip -qr "../gdal${COMPRESSED_VERSION}${RC}.zip" "gdal-${GDAL_VERSION}"

rm -f "../gdalautotest-${GDAL_VERSION}${RC}.tar.gz"
rm -f "../gdalautotest-${GDAL_VERSION}${RC}.zip"
tar cf "../gdalautotest-${GDAL_VERSION}${RC}.tar.gz" "gdalautotest-${GDAL_VERSION}"
zip -qr "../gdalautotest-${GDAL_VERSION}${RC}.zip" "gdalautotest-${GDAL_VERSION}"

cd "gdal-${GDAL_VERSION}"
echo "GDAL_VER=${GDAL_VERSION}" > GDALmake.opt
cd frmts/grass
make dist
mv ./*.tar.gz ../../../..
cd ../../..

echo "* Generating MD5 sums ..."

MY_OSTYPE=$(uname -s)
if test "$MY_OSTYPE" = "Darwin" ; then
MD5=md5
else
MD5=md5sum
fi

cd ..
$MD5 "gdal-${GDAL_VERSION}${RC}.tar.xz" > "gdal-${GDAL_VERSION}${RC}.tar.xz.md5"
$MD5 "gdal-${GDAL_VERSION}${RC}.tar.gz" > "gdal-${GDAL_VERSION}${RC}.tar.gz.md5"
$MD5 "gdal${COMPRESSED_VERSION}${RC}.zip" > "gdal${COMPRESSED_VERSION}${RC}.zip.md5"

echo "* Cleaning..."
rm -rf dist_wrk

echo "*** The End ***"
