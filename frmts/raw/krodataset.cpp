/******************************************************************************
 *
 * Project:  KRO format reader/writer
 * Purpose:  Implementation of KOLOR Raw Format
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 * Financial Support: SITES (http://www.sites.fr)
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "rawdataset.h"

CPL_CVSID("$Id$")

// http://www.autopano.net/wiki-en/Format_KRO

/************************************************************************/
/* ==================================================================== */
/*                                KRODataset                            */
/* ==================================================================== */
/************************************************************************/

class KRODataset final: public RawDataset
{
    VSILFILE    *fpImage;  // image data file.

    CPL_DISALLOW_COPY_ASSIGN(KRODataset)

  public:
                    KRODataset() : fpImage(nullptr) {}
                   ~KRODataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create(const char * pszFilename,
                                int nXSize, int nYSize, int nBandsIn,
                                GDALDataType eType, char ** papszOptions );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                                  KRODataset                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             ~KRODataset()                            */
/************************************************************************/

KRODataset::~KRODataset()

{
    FlushCache(true);

    if( fpImage != nullptr )
    {
        if( VSIFCloseL( fpImage ) != 0 )
        {
          CPLError( CE_Failure, CPLE_FileIO, "I/O error" );
        }
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int KRODataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 20 )
        return FALSE;

    if( !STARTS_WITH_CI( reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                         "KRO\x01") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *KRODataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    KRODataset *poDS = new KRODataset();
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Read the file header.                                           */
/* -------------------------------------------------------------------- */
    char achHeader[20] = { '\0' };
    CPL_IGNORE_RET_VAL(VSIFReadL( achHeader, 1, 20, poDS->fpImage ));

    int nXSize;
    memcpy(&nXSize, achHeader + 4, 4);
    CPL_MSBPTR32( &nXSize );

    int nYSize = 0;
    memcpy(&nYSize, achHeader + 8, 4);
    CPL_MSBPTR32( &nYSize );

    int nDepth = 0;
    memcpy(&nDepth, achHeader + 12, 4);
    CPL_MSBPTR32( &nDepth );

    int nComp = 0;
    memcpy(&nComp, achHeader + 16, 4);
    CPL_MSBPTR32( &nComp );

    if( !GDALCheckDatasetDimensions(nXSize, nYSize) ||
        !GDALCheckBandCount(nComp, FALSE) )
    {
        delete poDS;
        return nullptr;
    }

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;

    GDALDataType eDT = GDT_Unknown;
    if( nDepth == 8 )
    {
        eDT = GDT_Byte;
    }
    else if( nDepth == 16 )
    {
        eDT = GDT_UInt16;
    }
    else if( nDepth == 32 )
    {
        eDT = GDT_Float32;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unhandled depth : %d", nDepth );
        delete poDS;
        return nullptr;
    }

    const int nDataTypeSize = nDepth / 8;

    if( nComp == 0 || nDataTypeSize == 0 ||
        poDS->nRasterXSize > INT_MAX / (nComp * nDataTypeSize) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Too large width / number of bands" );
        delete poDS;
        return nullptr;
    }

    vsi_l_offset nExpectedSize = static_cast<vsi_l_offset>(poDS->nRasterXSize)
        * poDS->nRasterYSize * nComp * nDataTypeSize + 20;
    VSIFSeekL(poDS->fpImage, 0, SEEK_END);
    if( VSIFTellL(poDS->fpImage) < nExpectedSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "File too short" );
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create bands.                                                   */
/* -------------------------------------------------------------------- */
    CPLErrorReset();
    for( int iBand = 0; iBand < nComp; iBand++ )
    {
        RawRasterBand *poBand =
            new RawRasterBand( poDS, iBand+1, poDS->fpImage,
                               20 + nDataTypeSize * iBand,
                               nComp * nDataTypeSize,
                               poDS->nRasterXSize * nComp * nDataTypeSize,
                               eDT, !CPL_IS_LSB, RawRasterBand::OwnFP::NO );
        if( nComp == 3 || nComp == 4 )
        {
            poBand->SetColorInterpretation( static_cast<GDALColorInterp>(GCI_RedBand + iBand) );
        }
        poDS->SetBand( iBand+1, poBand );
        if( CPLGetLastErrorType() != CE_None )
        {
            delete poDS;
            return nullptr;
        }
    }

    if( nComp > 1 )
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *KRODataset::Create( const char * pszFilename,
                                 int nXSize,
                                 int nYSize,
                                 int nBandsIn,
                                 GDALDataType eType,
                                 char ** /* papszOptions */ )
{
    if( eType != GDT_Byte && eType != GDT_UInt16 && eType != GDT_Float32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create KRO file with unsupported data type '%s'.",
                  GDALGetDataTypeName( eType ) );
        return nullptr;
    }
    if( nXSize == 0 || nYSize == 0 || nBandsIn == 0 )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to create file.                                             */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "wb" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.",
                  pszFilename );
        return nullptr;
    }

    size_t nRet = VSIFWriteL("KRO\01", 4, 1, fp);

/* -------------------------------------------------------------------- */
/*      Create a file level header.                                     */
/* -------------------------------------------------------------------- */
    int nTmp = nXSize;
    CPL_MSBPTR32(&nTmp);
    nRet += VSIFWriteL(&nTmp, 4, 1, fp);

    nTmp = nYSize;
    CPL_MSBPTR32(&nTmp);
    nRet += VSIFWriteL(&nTmp, 4, 1, fp);

    nTmp = GDALGetDataTypeSizeBits(eType);
    CPL_MSBPTR32(&nTmp);
    nRet += VSIFWriteL(&nTmp, 4, 1, fp);

    nTmp = nBandsIn;
    CPL_MSBPTR32(&nTmp);
    nRet += VSIFWriteL(&nTmp, 4, 1, fp);

/* -------------------------------------------------------------------- */
/*      Zero out image data                                             */
/* -------------------------------------------------------------------- */

    CPL_IGNORE_RET_VAL(
        VSIFSeekL( fp,
                   static_cast<vsi_l_offset>(nXSize) * nYSize *
                   GDALGetDataTypeSizeBytes(eType) * nBandsIn - 1,
              SEEK_CUR));
    GByte byNul = 0;
    nRet += VSIFWriteL(&byNul, 1, 1, fp);
    if( VSIFCloseL(fp) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, "I/O error" );
        return nullptr;
    }

    if( nRet != 6 )
        return nullptr;

    return
        reinterpret_cast<GDALDataset *>( GDALOpen( pszFilename, GA_Update ) );
}

/************************************************************************/
/*                         GDALRegister_KRO()                           */
/************************************************************************/

void GDALRegister_KRO()

{
    if( GDALGetDriverByName( "KRO" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "KRO" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "KOLOR Raw" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "kro" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Float32" );

    poDriver->pfnIdentify = KRODataset::Identify;
    poDriver->pfnOpen = KRODataset::Open;
    poDriver->pfnCreate = KRODataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
