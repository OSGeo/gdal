/******************************************************************************
 *
 * Project:  ELAS Translator
 * Purpose:  Complete implementation of ELAS translator module for GDAL.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_frmts.h"
#include "gdal_pam.h"

#include <cmath>
#include <algorithm>

using std::fill;

CPL_CVSID("$Id$")

typedef struct ELASHeader {
    ELASHeader();

    GInt32      NBIH;   /* bytes in header, normally 1024 */
    GInt32      NBPR;   /* bytes per data record (all bands of scanline) */
    GInt32      IL;     /* initial line - normally 1 */
    GInt32      LL;     /* last line */
    GInt32      IE;     /* initial element (pixel), normally 1 */
    GInt32      LE;     /* last element (pixel) */
    GInt32      NC;     /* number of channels (bands) */
    GUInt32     H4321;  /* header record identifier - always 4321. */
    char        YLabel[4]; /* Should be "NOR" for UTM */
    GInt32      YOffset;/* topleft pixel center northing */
    char        XLabel[4]; /* Should be "EAS" for UTM */
    GInt32      XOffset;/* topleft pixel center easting */
    float       YPixSize;/* height of pixel in georef units */
    float       XPixSize;/* width of pixel in georef units */
    float       Matrix[4]; /* 2x2 transformation matrix.  Should be
                              1,0,0,1 for pixel/line, or
                              1,0,0,-1 for UTM */
    GByte       IH19[4];  /* data type, and size flags */
    GInt32      IH20;   /* number of secondary headers */
    char        unused1[8];
    GInt32      LABL;   /* used by LABL module */
    char        HEAD;   /* used by HEAD module */
    char        Comment1[64];
    char        Comment2[64];
    char        Comment3[64];
    char        Comment4[64];
    char        Comment5[64];
    char        Comment6[64];
    GUInt16     ColorTable[256];  /* RGB packed with 4 bits each */
    char        unused2[32];
} _ELASHeader;

ELASHeader::ELASHeader() :
    NBIH(0),
    NBPR(0),
    IL(0),
    LL(0),
    IE(0),
    LE(0),
    NC(0),
    H4321(0),
    YOffset(0),
    XOffset(0),
    YPixSize(0.0),
    XPixSize(0.0),
    IH20(0),
    LABL(0),
    HEAD(0)
{
    fill( YLabel, YLabel + CPL_ARRAYSIZE(YLabel), 0 );
    fill( XLabel, XLabel + CPL_ARRAYSIZE(XLabel), 0 );
    fill( Matrix, Matrix + CPL_ARRAYSIZE(Matrix), 0.f );
    fill( IH19, IH19 + CPL_ARRAYSIZE(IH19), 0 );
    fill( unused1, unused1 + CPL_ARRAYSIZE(unused1), 0 );
    fill( Comment1, Comment1 + CPL_ARRAYSIZE(Comment1), 0 );
    fill( Comment2, Comment2 + CPL_ARRAYSIZE(Comment2), 0 );
    fill( Comment3, Comment3 + CPL_ARRAYSIZE(Comment3), 0 );
    fill( Comment4, Comment4 + CPL_ARRAYSIZE(Comment4), 0 );
    fill( Comment5, Comment5 + CPL_ARRAYSIZE(Comment5), 0 );
    fill( Comment6, Comment6 + CPL_ARRAYSIZE(Comment6), 0 );
    fill( ColorTable, ColorTable + CPL_ARRAYSIZE(ColorTable), 0 );
    fill( unused2, unused2 + CPL_ARRAYSIZE(unused2), 0 );
}

/************************************************************************/
/* ==================================================================== */
/*                              ELASDataset                             */
/* ==================================================================== */
/************************************************************************/

class ELASRasterBand;

class ELASDataset : public GDALPamDataset
{
    friend class ELASRasterBand;

    VSILFILE    *fp;

    ELASHeader  sHeader;
    int         bHeaderModified;

    GDALDataType eRasterDataType;

    int         nLineOffset;
    int         nBandOffset;  // Within a line.

    double      adfGeoTransform[6];

  public:
                 ELASDataset();
    virtual ~ELASDataset();

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual CPLErr SetGeoTransform( double * ) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );

    virtual void FlushCache( void ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                            ELASRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class ELASRasterBand : public GDALPamRasterBand
{
    friend class ELASDataset;

  public:

                   ELASRasterBand( ELASDataset *, int );

    // should override RasterIO eventually.

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           ELASRasterBand()                            */
/************************************************************************/

ELASRasterBand::ELASRasterBand( ELASDataset *poDSIn, int nBandIn )

{
    poDS = poDSIn;
    nBand = nBandIn;

    eAccess = poDSIn->eAccess;

    eDataType = poDSIn->eRasterDataType;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ELASRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                   int nBlockYOff,
                                   void * pImage )
{
    CPLAssert( nBlockXOff == 0 );

    ELASDataset *poGDS = (ELASDataset *) poDS;

    int nDataSize = GDALGetDataTypeSizeBytes(eDataType) * poGDS->GetRasterXSize();
    long nOffset = poGDS->nLineOffset * nBlockYOff + 1024 + (nBand-1) * nDataSize;

/* -------------------------------------------------------------------- */
/*      If we can't seek to the data, we will assume this is a newly    */
/*      created file, and that the file hasn't been extended yet.       */
/*      Just read as zeros.                                             */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( poGDS->fp, nOffset, SEEK_SET ) != 0
        || VSIFReadL( pImage, 1, nDataSize, poGDS->fp ) != (size_t) nDataSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Seek or read of %d bytes at %ld failed.\n",
                  nDataSize, nOffset );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr ELASRasterBand::IWriteBlock( CPL_UNUSED int nBlockXOff,
                                    int nBlockYOff,
                                    void * pImage )
{
    CPLAssert( nBlockXOff == 0 );
    CPLAssert( eAccess == GA_Update );

    ELASDataset *poGDS = (ELASDataset *) poDS;

    int nDataSize = GDALGetDataTypeSizeBytes(eDataType) * poGDS->GetRasterXSize();
    long nOffset = poGDS->nLineOffset * nBlockYOff + 1024 + (nBand-1) * nDataSize;

    if( VSIFSeekL( poGDS->fp, nOffset, SEEK_SET ) != 0
        || VSIFWriteL( pImage, 1, nDataSize, poGDS->fp ) != (size_t) nDataSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Seek or write of %d bytes at %ld failed.\n",
                  nDataSize, nOffset );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*      ELASDataset                                                     */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ELASDataset()                             */
/************************************************************************/

ELASDataset::ELASDataset() :
    fp(NULL),
    bHeaderModified(0),
    eRasterDataType(GDT_Unknown),
    nLineOffset(0),
    nBandOffset(0)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~ELASDataset()                            */
/************************************************************************/

ELASDataset::~ELASDataset()

{
    FlushCache();

    if( fp != NULL )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
    }
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We also write out the header, if it is modified.                */
/************************************************************************/

void ELASDataset::FlushCache()

{
    GDALDataset::FlushCache();

    if( bHeaderModified )
    {
        CPL_IGNORE_RET_VAL(VSIFSeekL( fp, 0, SEEK_SET ));
        CPL_IGNORE_RET_VAL(VSIFWriteL( &sHeader, 1024, 1, fp ));
        bHeaderModified = FALSE;
    }
}

/************************************************************************/
/*                              Identify()                               */
/************************************************************************/

int ELASDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*  First we check to see if the file has the expected header           */
/*  bytes.                                                               */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 256 )
        return FALSE;

    if( CPL_MSBWORD32(*((GInt32 *) (poOpenInfo->pabyHeader+0))) != 1024
        || CPL_MSBWORD32(*((GInt32 *) (poOpenInfo->pabyHeader+28))) != 4321 )
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ELASDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify(poOpenInfo) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    const char *pszAccess = poOpenInfo->eAccess == GA_Update ? "r+b" : "rb";

    ELASDataset *poDS = new ELASDataset();

    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, pszAccess );
    if( poDS->fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to open `%s' with access `%s' failed.\n",
                  poOpenInfo->pszFilename, pszAccess );
        delete poDS;
        return NULL;
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Read the header information.                                    */
/* -------------------------------------------------------------------- */
    poDS->bHeaderModified = FALSE;
    if( VSIFReadL( &(poDS->sHeader), 1024, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Attempt to read 1024 byte header filed on file %s\n",
                  poOpenInfo->pszFilename );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Extract information of interest from the header.                */
/* -------------------------------------------------------------------- */
    poDS->nLineOffset = CPL_MSBWORD32( poDS->sHeader.NBPR );

    int nStart = CPL_MSBWORD32( poDS->sHeader.IL );
    int nEnd = CPL_MSBWORD32( poDS->sHeader.LL );
    GIntBig nDiff = static_cast<GIntBig>(nEnd) - nStart + 1;
    if( nDiff <= 0 || nDiff > INT_MAX )
    {
        delete poDS;
        return NULL;
    }
    poDS->nRasterYSize = static_cast<int>(nDiff);

    nStart = CPL_MSBWORD32( poDS->sHeader.IE );
    nEnd = CPL_MSBWORD32( poDS->sHeader.LE );
    nDiff = static_cast<GIntBig>(nEnd) - nStart + 1;
    if( nDiff <= 0 || nDiff > INT_MAX )
    {
        delete poDS;
        return NULL;
    }
    poDS->nRasterXSize = static_cast<int>(nDiff);

    poDS->nBands = CPL_MSBWORD32( poDS->sHeader.NC );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(poDS->nBands, FALSE))
    {
        delete poDS;
        return NULL;
    }

    const int nELASDataType = (poDS->sHeader.IH19[2] & 0x7e) >> 2;
    const int nBytesPerSample = poDS->sHeader.IH19[3];

    if( nELASDataType == 0 && nBytesPerSample == 1 )
        poDS->eRasterDataType = GDT_Byte;
    else if( nELASDataType == 1 && nBytesPerSample == 1 )
        poDS->eRasterDataType = GDT_Byte;
    else if( nELASDataType == 16 && nBytesPerSample == 4 )
        poDS->eRasterDataType = GDT_Float32;
    else if( nELASDataType == 17 && nBytesPerSample == 8 )
        poDS->eRasterDataType = GDT_Float64;
    else
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unrecognized image data type %d, with BytesPerSample=%d.\n",
                  nELASDataType, nBytesPerSample );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Band offsets are always multiples of 256 within a multi-band    */
/*      scanline of data.                                               */
/* -------------------------------------------------------------------- */
    if( GDALGetDataTypeSizeBytes(poDS->eRasterDataType) >
                                    (INT_MAX - 256) / poDS->nRasterXSize )
    {
        delete poDS;
        return NULL;
    }
    poDS->nBandOffset =
        (poDS->nRasterXSize * GDALGetDataTypeSizeBytes(poDS->eRasterDataType));

    if( poDS->nBandOffset > 1000000 )
    {
        VSIFSeekL( poDS->fp, 0, SEEK_END );
        if( VSIFTellL( poDS->fp ) < static_cast<vsi_l_offset>(poDS->nBandOffset) )
        {
            CPLError(CE_Failure, CPLE_FileIO, "File too short");
            delete poDS;
            return NULL;
        }
    }

    if( poDS->nBandOffset % 256 != 0 )
    {
        poDS->nBandOffset =
            poDS->nBandOffset - (poDS->nBandOffset % 256) + 256;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    /* coverity[tainted_data] */
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, new ELASRasterBand( poDS, iBand+1 ) );
    }

/* -------------------------------------------------------------------- */
/*      Extract the projection coordinates, if present.                 */
/* -------------------------------------------------------------------- */
    if( poDS->sHeader.XOffset != 0 )
    {
        CPL_MSBPTR32(&(poDS->sHeader.XPixSize));
        CPL_MSBPTR32(&(poDS->sHeader.YPixSize));

        poDS->adfGeoTransform[0] =
            (GInt32) CPL_MSBWORD32(poDS->sHeader.XOffset);
        poDS->adfGeoTransform[1] = poDS->sHeader.XPixSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] =
            (GInt32) CPL_MSBWORD32(poDS->sHeader.YOffset);
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -1.0 * std::abs(poDS->sHeader.YPixSize);

        CPL_MSBPTR32(&(poDS->sHeader.XPixSize));
        CPL_MSBPTR32(&(poDS->sHeader.YPixSize));

        poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[1] * 0.5;
        poDS->adfGeoTransform[3] -= poDS->adfGeoTransform[5] * 0.5;
    }
    else
    {
        poDS->adfGeoTransform[0] = 0.0;
        poDS->adfGeoTransform[1] = 1.0;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 1.0;
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new GeoTIFF or TIFF file.                              */
/************************************************************************/

GDALDataset *ELASDataset::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char ** /* notdef: papszParmList */ )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if (nBands <= 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "ELAS driver does not support %d bands.\n", nBands);
        return NULL;
    }

    if( eType != GDT_Byte && eType != GDT_Float32 && eType != GDT_Float64 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create an ELAS dataset with an illegal\n"
                  "data type (%d).\n",
                  eType );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    FILE *fp = VSIFOpen( pszFilename, "w" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      How long will each band of a scanline be?                       */
/* -------------------------------------------------------------------- */
    int nBandOffset = nXSize * GDALGetDataTypeSizeBytes(eType);

    if( nBandOffset % 256 != 0 )
    {
        nBandOffset = nBandOffset - (nBandOffset % 256) + 256;
    }

/* -------------------------------------------------------------------- */
/*      Setup header data block.                                        */
/*                                                                      */
/*      Note that CPL_MSBWORD32() will swap little endian words to      */
/*      big endian on little endian platforms.                          */
/* -------------------------------------------------------------------- */
    ELASHeader sHeader;

    sHeader.NBIH = CPL_MSBWORD32(1024);

    sHeader.NBPR = CPL_MSBWORD32(nBands * nBandOffset);

    sHeader.IL = CPL_MSBWORD32(1);
    sHeader.LL = CPL_MSBWORD32(nYSize);

    sHeader.IE = CPL_MSBWORD32(1);
    sHeader.LE = CPL_MSBWORD32(nXSize);

    sHeader.NC = CPL_MSBWORD32(nBands);

    sHeader.H4321 = CPL_MSBWORD32(4321);

    sHeader.IH19[0] = 0x04;
    sHeader.IH19[1] = 0xd2;
    sHeader.IH19[3] = (GByte) (GDALGetDataTypeSizeBytes(eType));

    if( eType == GDT_Byte )
        sHeader.IH19[2] = 1 << 2;
    else if( eType == GDT_Float32 )
        sHeader.IH19[2] = 16 << 2;
    else if( eType == GDT_Float64 )
        sHeader.IH19[2] = 17 << 2;

/* -------------------------------------------------------------------- */
/*      Write the header data.                                          */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFWrite( &sHeader, 1024, 1, fp ));

/* -------------------------------------------------------------------- */
/*      Now write out zero data for all the imagery.  This is           */
/*      inefficient, but simplies the IReadBlock() / IWriteBlock() logic.*/
/* -------------------------------------------------------------------- */
    GByte *pabyLine = (GByte *) CPLCalloc(nBandOffset,nBands);
    for( int iLine = 0; iLine < nYSize; iLine++ )
    {
        if( VSIFWrite( pabyLine, 1, nBandOffset, fp ) != (size_t) nBandOffset )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Error writing ELAS image data ... likely insufficient"
                      " disk space.\n" );
            VSIFClose( fp );
            CPLFree( pabyLine );
            return NULL;
        }
    }

    CPLFree( pabyLine );

    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */
    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ELASDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ELASDataset::SetGeoTransform( double * padfTransform )

{
/* -------------------------------------------------------------------- */
/*      I don't think it supports rotation, but perhaps it is possible  */
/*      for us to use the 2x2 transform matrix to accomplish some       */
/*      sort of rotation.                                               */
/* -------------------------------------------------------------------- */
    if( padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to set rotated geotransform on ELAS file.\n"
                  "ELAS does not support rotation.\n" );

        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Remember the new transform, and update the header.              */
/* -------------------------------------------------------------------- */
    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );

    bHeaderModified = TRUE;

    const int nXOff = (int) (adfGeoTransform[0] + adfGeoTransform[1]*0.5);
    const int nYOff = (int) (adfGeoTransform[3] + adfGeoTransform[5]*0.5);

    sHeader.XOffset = CPL_MSBWORD32(nXOff);
    sHeader.YOffset = CPL_MSBWORD32(nYOff);

    sHeader.XPixSize = static_cast<float>(std::abs(adfGeoTransform[1]));
    sHeader.YPixSize = static_cast<float>(std::abs(adfGeoTransform[5]));

    CPL_MSBPTR32(&(sHeader.XPixSize));
    CPL_MSBPTR32(&(sHeader.YPixSize));

    memcpy( sHeader.YLabel, "NOR ", 4 );
    memcpy( sHeader.XLabel, "EAS ", 4 );

    sHeader.Matrix[0] = 1.0;
    sHeader.Matrix[1] = 0.0;
    sHeader.Matrix[2] = 0.0;
    sHeader.Matrix[3] = -1.0;

    CPL_MSBPTR32(&(sHeader.Matrix[0]));
    CPL_MSBPTR32(&(sHeader.Matrix[1]));
    CPL_MSBPTR32(&(sHeader.Matrix[2]));
    CPL_MSBPTR32(&(sHeader.Matrix[3]));

    return CE_None;
}

/************************************************************************/
/*                          GDALRegister_ELAS()                         */
/************************************************************************/

void GDALRegister_ELAS()

{
    if( GDALGetDriverByName( "ELAS" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ELAS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "ELAS" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Float32 Float64" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = ELASDataset::Open;
    poDriver->pfnIdentify = ELASDataset::Identify;
    poDriver->pfnCreate = ELASDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
