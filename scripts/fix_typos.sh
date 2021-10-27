#!/bin/sh
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
#  Project:  GDAL
#  Purpose:  (Interactive) script to identify and fix typos
#  Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
###############################################################################

set -eu

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)"/"$(dirname "$0")
        ;;
esac
GDAL_ROOT=$SCRIPT_DIR/..
cd "$GDAL_ROOT"

if ! test -d fix_typos; then
    # Get our fork of codespell that adds --words-white-list and full filename support for -S option
    mkdir fix_typos
    (cd fix_typos
     git clone https://github.com/rouault/codespell
     (cd codespell && git checkout gdal_improvements)
     # Aggregate base dictionary + QGIS one + Debian Lintian one
     curl https://raw.githubusercontent.com/qgis/QGIS/master/scripts/spell_check/spelling.dat | sed "s/:/->/" | sed "s/:%//" | grep -v "colour->" | grep -v "colours->" > qgis.txt
     curl https://salsa.debian.org/lintian/lintian/-/raw/master/data/spelling/corrections | grep "||" | grep -v "#" | sed "s/||/->/" > debian.txt
     cat codespell/data/dictionary.txt qgis.txt debian.txt | awk 'NF' > gdal_dict.txt
     echo "difered->deferred" >> gdal_dict.txt
     echo "differed->deferred" >> gdal_dict.txt
     grep -v 404 < gdal_dict.txt > gdal_dict.txt.tmp
     mv gdal_dict.txt.tmp gdal_dict.txt
    )
fi

EXCLUDED_FILES="*/.svn*,*/.git/*,configure,config.log,config.status,config.guess,config.sub,*/autom4te.cache/*,*.ai,*.svg"
EXCLUDED_FILES="$EXCLUDED_FILES,*/hdf-eos/*,teststream.out,ogrogdilayer.cpp"
EXCLUDED_FILES="$EXCLUDED_FILES,*/doc/build/*,*/data/*,figures.mp,*/tmp/*,*/ruby/*"
EXCLUDED_FILES="$EXCLUDED_FILES,*/fix_typos/*,fix_typos.sh,*.eps,geopackage_aspatial.html"
EXCLUDED_FILES="$EXCLUDED_FILES,*/kdu_cache_wrapper.h,*/PublicDecompWT/*,*/man/*,./html/*"
EXCLUDED_FILES="$EXCLUDED_FILES,*/ingres/*,*/fme/*,*/segy/*,*/wrk/*"
EXCLUDED_FILES="$EXCLUDED_FILES,PROVENANCE.TXT,libtool,ltmain.sh,libtool.m4,./m4/*"
EXCLUDED_FILES="$EXCLUDED_FILES,WFSServersList.txt"
EXCLUDED_FILES="$EXCLUDED_FILES,*/sosi/*" # norwegian
EXCLUDED_FILES="$EXCLUDED_FILES,*/ci/travis/csa_part_1/*,*/ci/travis/csa_part_2/*"
EXCLUDED_FILES="$EXCLUDED_FILES,*/internal_libqhull/*,*/zlib/*,*/libjpeg/*,*/libjpeg12/*,*/libpng/*,*/libcsf/*,*/degrib/*"
AUTHORIZED_LIST="poSession,FIDN,TRAFIC,HTINK,repID,oCurr,INTREST,oPosition"
AUTHORIZED_LIST="$AUTHORIZED_LIST,CPL_SUPRESS_CPLUSPLUS,SRP_NAM,ADRG_NAM,'SRP_NAM,AuxilaryTarget"
# IRIS driver metadata item names: FIXME ?
AUTHORIZED_LIST="$AUTHORIZED_LIST,TOP_OF_HEIGTH_INTERVAL,BOTTOM_OF_HEIGTH_INTERVAL"
# libjpeg
AUTHORIZED_LIST="$AUTHORIZED_LIST,JBUF_PASS_THRU"
# libgif
AUTHORIZED_LIST="$AUTHORIZED_LIST,IS_WRITEABLE,E_GIF_ERR_NOT_WRITEABLE"
# libtiff
AUTHORIZED_LIST="$AUTHORIZED_LIST,THRESHHOLD_BILEVEL,THRESHHOLD_HALFTONE,THRESHHOLD_ERRORDIFFUSE"
# mffdataset.cpp
AUTHORIZED_LIST="$AUTHORIZED_LIST,oUTMorLL"
# hf2dataset.cpp
AUTHORIZED_LIST="$AUTHORIZED_LIST,fVertPres"
# kml_generate_test_files.py
AUTHORIZED_LIST="$AUTHORIZED_LIST,Lod,LOD"
# Many .py (nd for nodata)
AUTHORIZED_LIST="$AUTHORIZED_LIST,nd"
# vsis3.py
AUTHORIZED_LIST="$AUTHORIZED_LIST,iam"
# NITF
AUTHORIZED_LIST="$AUTHORIZED_LIST,tre,TRE,psTreNode,nTreIMASDASize,nTreIMRFCASize,pachTreIMRFCA,nTreIndex,nTreOffset,pnTreOffset,pszTreName,pachTRE,nTreLength,nTreMinLength,nTreMaxLength"
# cpl_vsil_win32.cpp
AUTHORIZED_LIST="$AUTHORIZED_LIST,ERROR_FILENAME_EXCED_RANGE"
# autotest/cpp
AUTHORIZED_LIST="$AUTHORIZED_LIST,TUT_USE_SEH,SEH_OK,SEH_CTOR,SEH_TEST,SEH_DUMMY,SEH"
AUTHORIZED_LIST="$AUTHORIZED_LIST,NPY_ARRAY_WRITEABLE"
AUTHORIZED_LIST="$AUTHORIZED_LIST,cartesian,Cartesian,CARTESIAN,Australia"
AUTHORIZED_LIST="$AUTHORIZED_LIST,cJP2_Error_Decompression_Cancelled"
AUTHORIZED_LIST="$AUTHORIZED_LIST,ADVERTIZE_UTF8,SetAdvertizeUTF8,m_bAdvertizeUTF8,bAdvertizeUTF8In"
AUTHORIZED_LIST="$AUTHORIZED_LIST,poTransactionBehaviour,IOGRTransactionBehaviour"
AUTHORIZED_LIST="$AUTHORIZED_LIST,FindProjParm,GetProjParm,GetNormProjParm,SetProjParm,SetNormProjParm,StripCTParms,SpatialReference_GetProjParm,SpatialReference_SetProjParm,SpatialReference_GetNormProjParm,SpatialReference_SetNormProjParm"
AUTHORIZED_LIST="$AUTHORIZED_LIST,te" # gdalwarp switch
AUTHORIZED_LIST="$AUTHORIZED_LIST,PoDoFo"
AUTHORIZED_LIST="$AUTHORIZED_LIST,LaTeX,BibTeX"
AUTHORIZED_LIST="$AUTHORIZED_LIST,PSOT,Psot" # JPEG2000
AUTHORIZED_LIST="$AUTHORIZED_LIST,ALOS,Alos"
AUTHORIZED_LIST="$AUTHORIZED_LIST,LINZ,linz" # organization name
AUTHORIZED_LIST="$AUTHORIZED_LIST,inout"
AUTHORIZED_LIST="$AUTHORIZED_LIST,koordinates" # company name
AUTHORIZED_LIST="$AUTHORIZED_LIST,lod" # level of detail"
AUTHORIZED_LIST="$AUTHORIZED_LIST,HSI_UNKNOWN"
AUTHORIZED_LIST="$AUTHORIZED_LIST,O_CREAT"
AUTHORIZED_LIST="$AUTHORIZED_LIST,unpreciseMathCall" # cppcheck warning code
AUTHORIZED_LIST="$AUTHORIZED_LIST,FlateDecode" # PDF
AUTHORIZED_LIST="$AUTHORIZED_LIST,FileCreatPropList,H5F_ACC_CREAT,H5F_ACC_TRUNC" # KEA and HDF5 driver
AUTHORIZED_LIST="$AUTHORIZED_LIST,cJP2_Colorspace_RGBa,cJP2_Colorspace_Palette_RGBa,cJP2_Colorspace_Palette_CIE_LABa" # JP2Lura
AUTHORIZED_LIST="$AUTHORIZED_LIST,CURLE_FILE_COULDNT_READ_FILE"
AUTHORIZED_LIST="$AUTHORIZED_LIST,nParms,ProjParm,ProjParmId,GTIFFetchProjParms,gdal_GTIFFetchProjParms" # API of libgeotiff

python3 fix_typos/codespell/codespell.py -w -i 3 -q 2 -S "$EXCLUDED_FILES,./autotest/*,./build*/*" \
    -x scripts/typos_allowlist.txt --words-white-list=$AUTHORIZED_LIST \
    -D ./fix_typos/gdal_dict.txt .

python3 fix_typos/codespell/codespell.py -w -i 3 -q 2 -S "$EXCLUDED_FILES" \
    -x scripts/typos_allowlist.txt --words-white-list=$AUTHORIZED_LIST \
    autotest
