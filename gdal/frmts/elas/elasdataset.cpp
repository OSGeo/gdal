/******************************************************************************
 * $Id$
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

#include "gdal_pam.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_ELAS(void);
CPL_C_END

typedef struct {
    GInt32	NBIH;	/* bytes in header, normaly 1024 */
    GInt32      NBPR;	/* bytes per data record (all bands of scanline) */
    GInt32	IL;	/* initial line - normally 1 */
    GInt32	LL;	/* last line */
    GInt32	IE;	/* initial element (pixel), normally 1 */
    GInt32	LE;	/* last element (pixel) */
    GInt32	NC;	/* number of channels (bands) */
    GInt32	H4321;	/* header record identifier - always 4321. */
    char	YLabel[4]; /* Should be "NOR" for UTM */
    GInt32      YOffset;/* topleft pixel center northing */
    char	XLabel[4]; /* Should be "EAS" for UTM */
    GInt32      XOffset;/* topleft pixel center easting */
    float	YPixSize;/* height of pixel in georef units */
    float	XPixSize;/* width of pixel in georef units */
    float	Matrix[4]; /* 2x2 transformation matrix.  Should be
                              1,0,0,1 for pixel/line, or
                              1,0,0,-1 for UTM */
    GByte	IH19[4];/* data type, and size flags */
    GInt32	IH20;	/* number of secondary headers */
    char	unused1[8];
    GInt32	LABL;	/* used by LABL module */
    char	HEAD;	/* used by HEAD module */
    char	Comment1[64];
    char	Comment2[64];
    char	Comment3[64];
    char	Comment4[64];
    char	Comment5[64];
    char	Comment6[64];
    GUInt16	ColorTable[256];  /* RGB packed with 4 bits each */
    char	unused2[32];
} ELASHeader;


/************************************************************************/
/* ==================================================================== */
/*				ELASDataset				*/
/* ==================================================================== */
/************************************************************************/

class ELASRasterBand;

class ELASDataset : public GDALPamDataset
{
    friend class ELASRasterBand;

    VSILFILE	*fp;

    ELASHeader  sHeader;
    int		bHeaderModified;

    GDALDataType eRasterDataType;

    int		nLineOffset;
    int		nBandOffset;     // within a line.
    
    double	adfGeoTransform[6];

  public:
                 ELASDataset();
                 ~ELASDataset();

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );

    virtual void FlushCache( void );
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
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * ); 
};


/************************************************************************/
/*                           ELASRasterBand()                            */
/************************************************************************/

ELASRasterBand::ELASRasterBand( ELASDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    this->eAccess = poDS->eAccess;

    eDataType = poDS->eRasterDataType;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ELASRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    ELASDataset	*poGDS = (ELASDataset *) poDS;
    CPLErr		eErr = CE_None;
    long		nOffset;
    int			nDataSize;

    CPLAssert( nBlockXOff == 0 );

    nDataSize = GDALGetDataTypeSize(eDataType) * poGDS->GetRasterXSize() / 8;
    nOffset = poGDS->nLineOffset * nBlockYOff + 1024 + (nBand-1) * nDataSize;
    
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
        eErr = CE_Failure;
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr ELASRasterBand::IWriteBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    ELASDataset	*poGDS = (ELASDataset *) poDS;
    CPLErr		eErr = CE_None;
    long		nOffset;
    int			nDataSize;

    CPLAssert( nBlockXOff == 0 );
    CPLAssert( eAccess == GA_Update );

    nDataSize = GDALGetDataTypeSize(eDataType) * poGDS->GetRasterXSize() / 8;
    nOffset = poGDS->nLineOffset * nBlockYOff + 1024 + (nBand-1) * nDataSize;
    
    if( VSIFSeekL( poGDS->fp, nOffset, SEEK_SET ) != 0
        || VSIFWriteL( pImage, 1, nDataSize, poGDS->fp ) != (size_t) nDataSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Seek or write of %d bytes at %ld failed.\n",
                  nDataSize, nOffset );
        eErr = CE_Failure;
    }

    return eErr;
}

/************************************************************************/
/* ==================================================================== */
/*      ELASDataset                                                     */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            ELASDataset()                             */
/************************************************************************/

ELASDataset::ELASDataset()

{
    fp = NULL;

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
        VSIFCloseL( fp );
        fp = NULL;
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
        VSIFSeekL( fp, 0, SEEK_SET );
        VSIFWriteL( &sHeader, 1024, 1, fp );
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
    ELASDataset 	*poDS;
    const char	 	*pszAccess;

    if( poOpenInfo->eAccess == GA_Update )
        pszAccess = "r+b";
    else
        pszAccess = "rb";

    poDS = new ELASDataset();

    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, pszAccess );
    if( poDS->fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to open `%s' with acces `%s' failed.\n",
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
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Extract information of interest from the header.                */
/* -------------------------------------------------------------------- */
    int		nStart, nEnd, nELASDataType, nBytesPerSample;
    
    poDS->nLineOffset = CPL_MSBWORD32( poDS->sHeader.NBPR );

    nStart = CPL_MSBWORD32( poDS->sHeader.IL );
    nEnd = CPL_MSBWORD32( poDS->sHeader.LL );
    poDS->nRasterYSize = nEnd - nStart + 1;

    nStart = CPL_MSBWORD32( poDS->sHeader.IE );
    nEnd = CPL_MSBWORD32( poDS->sHeader.LE );
    poDS->nRasterXSize = nEnd - nStart + 1;

    poDS->nBands = CPL_MSBWORD32( poDS->sHeader.NC );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(poDS->nBands, FALSE))
    {
        delete poDS;
        return NULL;
    }

    nELASDataType = (poDS->sHeader.IH19[2] & 0x7e) >> 2;
    nBytesPerSample = poDS->sHeader.IH19[3];
    
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
                  "Unrecognised image data type %d, with BytesPerSample=%d.\n",
                  nELASDataType, nBytesPerSample );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*	Band offsets are always multiples of 256 within a multi-band	*/
/*	scanline of data.						*/
/* -------------------------------------------------------------------- */
    poDS->nBandOffset =
        (poDS->nRasterXSize * GDALGetDataTypeSize(poDS->eRasterDataType)/8);

    if( poDS->nBandOffset % 256 != 0 )
    {
        poDS->nBandOffset =
            poDS->nBandOffset - (poDS->nBandOffset % 256) + 256;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    for( iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, new ELASRasterBand( poDS, iBand+1 ) );
    }

/* -------------------------------------------------------------------- */
/*	Extract the projection coordinates, if present.			*/
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
        poDS->adfGeoTransform[5] = -1.0 * ABS(poDS->sHeader.YPixSize);

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
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->papszSiblingFiles );

    return( poDS );
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
    int		nBandOffset;
    
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
    FILE	*fp;

    fp = VSIFOpen( pszFilename, "w" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*	How long will each band of a scanline be?			*/
/* -------------------------------------------------------------------- */
    nBandOffset = nXSize * GDALGetDataTypeSize(eType)/8;

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
    ELASHeader	sHeader;

    memset( &sHeader, 0, 1024 );

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
    sHeader.IH19[3] = (GByte) (GDALGetDataTypeSize(eType) / 8);

    if( eType == GDT_Byte )
        sHeader.IH19[2] = 1 << 2;
    else if( eType == GDT_Float32 )
        sHeader.IH19[2] = 16 << 2;
    else if( eType == GDT_Float64 )
        sHeader.IH19[2] = 17 << 2;

/* -------------------------------------------------------------------- */
/*      Write the header data.                                          */
/* -------------------------------------------------------------------- */
    VSIFWrite( &sHeader, 1024, 1, fp );

/* -------------------------------------------------------------------- */
/*      Now write out zero data for all the imagery.  This is           */
/*      inefficient, but simplies the IReadBlock() / IWriteBlock() logic.*/
/* -------------------------------------------------------------------- */
    GByte	*pabyLine;

    pabyLine = (GByte *) CPLCalloc(nBandOffset,nBands);
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

    return( CE_None );
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
    int		nXOff, nYOff;
    
    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );

    bHeaderModified = TRUE;

    nXOff = (int) (adfGeoTransform[0] + adfGeoTransform[1]*0.5);
    nYOff = (int) (adfGeoTransform[3] + adfGeoTransform[5]*0.5);

    sHeader.XOffset = CPL_MSBWORD32(nXOff);
    sHeader.YOffset = CPL_MSBWORD32(nYOff);

    sHeader.XPixSize = (float) ABS(adfGeoTransform[1]);
    sHeader.YPixSize = (float) ABS(adfGeoTransform[5]);

    CPL_MSBPTR32(&(sHeader.XPixSize));
    CPL_MSBPTR32(&(sHeader.YPixSize));

    strncpy( sHeader.YLabel, "NOR ", 4 );
    strncpy( sHeader.XLabel, "EAS ", 4 );

    sHeader.Matrix[0] = 1.0;
    sHeader.Matrix[1] = 0.0;
    sHeader.Matrix[2] = 0.0;
    sHeader.Matrix[3] = -1.0;
    
    CPL_MSBPTR32(&(sHeader.Matrix[0]));
    CPL_MSBPTR32(&(sHeader.Matrix[1]));
    CPL_MSBPTR32(&(sHeader.Matrix[2]));
    CPL_MSBPTR32(&(sHeader.Matrix[3]));
    
    return( CE_None );
}


/************************************************************************/
/*                          GDALRegister_ELAS()                        */
/************************************************************************/

void GDALRegister_ELAS()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ELAS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ELAS" );
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
}
