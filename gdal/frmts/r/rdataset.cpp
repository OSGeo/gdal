/******************************************************************************
 *
 * Project:  R Format Driver
 * Purpose:  Read/write R stats package object format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "rdataset.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

// constexpr int R_NILSXP = 0;
constexpr int R_LISTSXP = 2;
constexpr int R_CHARSXP = 9;
constexpr int R_INTSXP = 13;
constexpr int R_REALSXP = 14;
constexpr int R_STRSXP = 16;

namespace {

// TODO(schwehr): Move this to port/? for general use.
bool SafeMult(GIntBig a, GIntBig b, GIntBig *result) {
    if (a == 0 || b == 0) {
      *result = 0;
      return true;
    }

    bool result_positive = (a >= 0 && b >= 0) || (a < 0 && b < 0);
    if (result_positive) {
        // Cannot convert min() to positive.
        if (a == std::numeric_limits<GIntBig>::min() ||
            b == std::numeric_limits<GIntBig>::min()) {
            *result = 0;
            return false;
        }
        if (a < 0) {
            a = -a;
            b = -b;
        }
        if (a > std::numeric_limits<GIntBig>::max() / b) {
            *result = 0;
            return false;
        }
        *result = a * b;
        return true;
    }

    if (b < a) std::swap(a, b);
    if (a < (std::numeric_limits<GIntBig>::min() + 1) / b) {
        *result = 0;
        return false;
    }

    *result = a * b;
    return true;
}

}  // namespace

/************************************************************************/
/*                            RRasterBand()                             */
/************************************************************************/

RRasterBand::RRasterBand( RDataset *poDSIn, int nBandIn,
                          const double *padfMatrixValuesIn ) :
    padfMatrixValues(padfMatrixValuesIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Float64;

    nBlockXSize = poDSIn->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RRasterBand::IReadBlock( int /* nBlockXOff */,
                                int nBlockYOff,
                                void * pImage )
{
    memcpy(pImage, padfMatrixValues + nBlockYOff * nBlockXSize,
           nBlockXSize * 8);
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              RDataset()                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                              RDataset()                              */
/************************************************************************/

RDataset::RDataset() :
    fp(nullptr),
    bASCII(FALSE),
    nStartOfData(0),
    padfMatrixValues(nullptr)
{}

/************************************************************************/
/*                             ~RDataset()                              */
/************************************************************************/

RDataset::~RDataset()
{
    FlushCache(true);
    CPLFree(padfMatrixValues);

    if( fp )
        VSIFCloseL(fp);
}

/************************************************************************/
/*                             ASCIIFGets()                             */
/*                                                                      */
/*      Fetch one line from an ASCII source into osLastStringRead.      */
/************************************************************************/

const char *RDataset::ASCIIFGets()

{
    char chNextChar = '\0';

    osLastStringRead.resize(0);

    do
    {
        chNextChar = '\n';
        VSIFReadL(&chNextChar, 1, 1, fp);
        if( chNextChar != '\n' )
            osLastStringRead += chNextChar;
    } while( chNextChar != '\n' && chNextChar != '\0' );

    return osLastStringRead;
}

/************************************************************************/
/*                            ReadInteger()                             */
/************************************************************************/

int RDataset::ReadInteger()

{
    if( bASCII )
    {
        return atoi(ASCIIFGets());
    }

    GInt32 nValue = 0;

    if( VSIFReadL(&nValue, 4, 1, fp) != 1 )
        return -1;
    CPL_MSBPTR32(&nValue);

    return nValue;
}

/************************************************************************/
/*                             ReadFloat()                              */
/************************************************************************/

double RDataset::ReadFloat()

{
    if( bASCII )
    {
        return CPLAtof(ASCIIFGets());
    }

    double dfValue = 0.0;

    if( VSIFReadL(&dfValue, 8, 1, fp) != 1 )
        return -1;
    CPL_MSBPTR64(&dfValue);

    return dfValue;
}

/************************************************************************/
/*                             ReadString()                             */
/************************************************************************/

const char *RDataset::ReadString()

{
    if( ReadInteger() % 256 != R_CHARSXP )
    {
        osLastStringRead = "";
        return "";
    }

    const int nLenSigned = ReadInteger();
    if( nLenSigned < 0 )
    {
        osLastStringRead = "";
        return "";
    }
    const size_t nLen = static_cast<size_t>(nLenSigned);

    char *pachWrkBuf = static_cast<char *>(VSIMalloc(nLen));
    if (pachWrkBuf == nullptr)
    {
        osLastStringRead = "";
        return "";
    }
    if( VSIFReadL(pachWrkBuf, 1, nLen, fp) != nLen )
    {
        osLastStringRead = "";
        CPLFree(pachWrkBuf);
        return "";
    }

    if( bASCII )
    {
        // Suck up newline and any extra junk.
        ASCIIFGets();
    }

    osLastStringRead.assign(pachWrkBuf, nLen);
    CPLFree(pachWrkBuf);

    return osLastStringRead;
}

/************************************************************************/
/*                              ReadPair()                              */
/************************************************************************/

bool RDataset::ReadPair( CPLString &osObjName, int &nObjCode )

{
    nObjCode = ReadInteger();
    if( nObjCode == 254 )
        return true;

    if( (nObjCode % 256) != R_LISTSXP )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Did not find expected object pair object.");
        return false;
    }

    int nPairCount = ReadInteger();
    if( nPairCount != 1 )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Did not find expected pair count of 1.");
        return false;
    }

    // Read the object name.
    const char *pszName = ReadString();
    if( pszName == nullptr || pszName[0] == '\0' )
        return false;

    osObjName = pszName;

    // Confirm that we have a numeric matrix object.
    nObjCode = ReadInteger();

    return true;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int RDataset::Identify( GDALOpenInfo *poOpenInfo )
{
    if( poOpenInfo->nHeaderBytes < 50 )
        return FALSE;

    // If the extension is .rda and the file type is gzip
    // compressed we assume it is a gzipped R binary file.
    if( memcmp(poOpenInfo->pabyHeader, "\037\213\b", 3) == 0 &&
        EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "rda") )
        return TRUE;

    // Is this an ASCII or XDR binary R file?
    if( !STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "RDA2\nA\n") &&
        !STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "RDX2\nX\n") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RDataset::Open( GDALOpenInfo * poOpenInfo )
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if( poOpenInfo->pabyHeader == nullptr )
        return nullptr;
#else
    // During fuzzing, do not use Identify to reject crazy content.
    if( !Identify(poOpenInfo) )
        return nullptr;
#endif

    // Confirm the requested access is supported.
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The R driver does not support update access to existing"
                 " datasets.");
        return nullptr;
    }

    // Do we need to route the file through the decompression machinery?
    const bool bCompressed =
        memcmp(poOpenInfo->pabyHeader, "\037\213\b", 3) == 0;
    const CPLString osAdjustedFilename =
        std::string(bCompressed ? "/vsigzip/" : "") + poOpenInfo->pszFilename;

    // Establish this as a dataset and open the file using VSI*L.
    RDataset *poDS = new RDataset();

    poDS->fp = VSIFOpenL(osAdjustedFilename, "r");
    if( poDS->fp == nullptr )
    {
        delete poDS;
        return nullptr;
    }

    poDS->bASCII = STARTS_WITH_CI(
        reinterpret_cast<char *>(poOpenInfo->pabyHeader), "RDA2\nA\n");

    // Confirm this is a version 2 file.
    VSIFSeekL(poDS->fp, 7, SEEK_SET);
    if( poDS->ReadInteger() != R_LISTSXP )
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "It appears %s is not a version 2 R object file after all!",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    // Skip the version values.
    poDS->ReadInteger();
    poDS->ReadInteger();

    // Confirm we have a numeric vector object in a pairlist.
    CPLString osObjName;
    int nObjCode = 0;

    if( !poDS->ReadPair(osObjName, nObjCode) )
    {
        delete poDS;
        return nullptr;
    }

    if( nObjCode % 256 != R_REALSXP )
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to find expected numeric vector object.");
        return nullptr;
    }

    poDS->SetMetadataItem("R_OBJECT_NAME", osObjName);

    // Read the count.
    const int nValueCount = poDS->ReadInteger();
    if( nValueCount < 0 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "nValueCount < 0: %d", nValueCount);
        delete poDS;
        return nullptr;
    }

    poDS->nStartOfData = VSIFTellL(poDS->fp);

    // TODO(schwehr): Factor in the size of doubles.
    VSIStatBufL stat;
    const int dStatSuccess =
        VSIStatExL(osAdjustedFilename, &stat, VSI_STAT_SIZE_FLAG);
    if( dStatSuccess != 0 ||
        static_cast<vsi_l_offset>(nValueCount) >
        stat.st_size - poDS->nStartOfData )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Corrupt file.  "
            "Object claims to be larger than available bytes. "
            "%d > " CPL_FRMT_GUIB,
            nValueCount,
            stat.st_size - poDS->nStartOfData);
        delete poDS;
        return nullptr;
    }

    // Read/Skip ahead to attributes.
    if( poDS->bASCII )
    {
        poDS->padfMatrixValues =
            static_cast<double *>(VSIMalloc2(nValueCount, sizeof(double)));
        if (poDS->padfMatrixValues == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot allocate %d doubles", nValueCount);
            delete poDS;
            return nullptr;
        }
        for( int iValue = 0; iValue < nValueCount; iValue++ )
            poDS->padfMatrixValues[iValue] = poDS->ReadFloat();
    }
    else
    {
        VSIFSeekL(poDS->fp, 8 * nValueCount, SEEK_CUR);
    }

    // Read pairs till we run out, trying to find a few items that
    // have special meaning to us.
    poDS->nRasterXSize = 0;
    poDS->nRasterYSize = 0;
    int nBandCount = 0;

    while( poDS->ReadPair(osObjName, nObjCode) && nObjCode != 254 )
    {
        if( osObjName == "dim" && nObjCode % 256 == R_INTSXP )
        {
            const int nCount = poDS->ReadInteger();
            if( nCount == 2 )
            {
                poDS->nRasterXSize = poDS->ReadInteger();
                poDS->nRasterYSize = poDS->ReadInteger();
                nBandCount = 1;
            }
            else if( nCount == 3 )
            {
                poDS->nRasterXSize = poDS->ReadInteger();
                poDS->nRasterYSize = poDS->ReadInteger();
                nBandCount = poDS->ReadInteger();
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "R 'dim' dimension wrong.");
                delete poDS;
                return nullptr;
            }
        }
        else if( nObjCode % 256 == R_REALSXP )
        {
            int nCount = poDS->ReadInteger();
            while( nCount > 0 && !VSIFEofL(poDS->fp) )
            {
                nCount --;
                poDS->ReadFloat();
            }
        }
        else if( nObjCode % 256 == R_INTSXP )
        {
            int nCount = poDS->ReadInteger();
            while( nCount > 0 && !VSIFEofL(poDS->fp) )
            {
                nCount --;
                poDS->ReadInteger();
            }
        }
        else if( nObjCode % 256 == R_STRSXP )
        {
            int nCount = poDS->ReadInteger();
            while( nCount > 0 && !VSIFEofL(poDS->fp) )
            {
                nCount --;
                poDS->ReadString();
            }
        }
        else if( nObjCode % 256 == R_CHARSXP )
        {
            poDS->ReadString();
        }
    }

    if( poDS->nRasterXSize == 0 )
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to find dim dimension information for R dataset.");
        return nullptr;
    }

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBandCount, TRUE))
    {
        delete poDS;
        return nullptr;
    }

    GIntBig result = 0;
    bool ok = SafeMult(nBandCount, poDS->nRasterXSize, &result);
    ok &= SafeMult(result, poDS->nRasterYSize, &result);

    if( !ok || nValueCount <  result )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Not enough pixel data.");
        delete poDS;
        return nullptr;
    }

    // Create the raster band object(s).
    for( int iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poBand = nullptr;

        if( poDS->bASCII )
            poBand = new RRasterBand(
                poDS, iBand + 1,
                poDS->padfMatrixValues +
                    iBand * poDS->nRasterXSize * poDS->nRasterYSize);
        else
            poBand = new RawRasterBand(
                poDS, iBand + 1, poDS->fp,
                poDS->nStartOfData +
                    poDS->nRasterXSize * poDS->nRasterYSize * 8 * iBand,
                8, poDS->nRasterXSize * 8,
                GDT_Float64, !CPL_IS_LSB,
                RawRasterBand::OwnFP::NO);

        poDS->SetBand(iBand + 1, poBand);
    }

    // Initialize any PAM information.
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    // Check for overviews.
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_R()                              */
/************************************************************************/

void GDALRegister_R()

{
    if( GDALGetDriverByName("R") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("R");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "R Object Data Store");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/r.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "rda");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Float32");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='ASCII' type='boolean' description='For ASCII output, default NO'/>"
"   <Option name='COMPRESS' type='boolean' description='Produced Compressed output, default YES'/>"
"</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = RDataset::Open;
    poDriver->pfnIdentify = RDataset::Identify;
    poDriver->pfnCreateCopy = RCreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
