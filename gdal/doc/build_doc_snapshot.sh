#!/bin/sh

set -eu

TAG=$1

ARCHIVE="gdal${TAG}doc.zip"
echo "Building ${ARCHIVE}..."

TMPDIR=gdaldoc

rm -f .doxygen_up_to_date
rm -rf build/html
rm -rf build/latex
make html
make latexpdf

rm -rf "${TMPDIR}"
mkdir ${TMPDIR}
cp -r build/html/* ${TMPDIR}
rm -f ${TMPDIR}/gdal.pdf
cp build/latex/gdal.pdf ${TMPDIR}
ORIG_DIR=$PWD
cd ${TMPDIR}
wget https://download.osgeo.org/gdal/for_doc/javadoc.zip -O /tmp/javadoc.zip
wget https://download.osgeo.org/gdal/for_doc/python-doc.tar.gz -O /tmp/python-doc.tar.gz
unzip -q /tmp/javadoc.zip
tar xzf /tmp/python-doc.tar.gz
cd ${ORIG_DIR}

rm -f "${ARCHIVE}"
zip -r "${ARCHIVE}" ${TMPDIR}/*
rm -rf "${TMPDIR}"
