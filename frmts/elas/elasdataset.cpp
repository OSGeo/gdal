/******************************************************************************
 * $Id$
 *
 * Project:  ELAS Translator
 * Purpose:  Complete implementation of ELAS translator module for GDAL.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/05/13 19:17:48  warmerda
 * New
 *
 */

#include "gdal_priv.h"

static GDALDriver	*poELASDriver = NULL;

CPL_C_START
void	GDALRegister_ELAS(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				ELASDataset				*/
/* ==================================================================== */
/************************************************************************/

class ELASRasterBand;

class ELASDataset : public GDALDataset
{
    friend	ELASRasterBand;

    FILE	*fp;

    GByte	abyHeader[1024];
    int		bHeaderModified;

    GDALDataType eRasterDataType;

    int		nLineOffset;
    int		nBandOffset;     // within a line.
    
    char	*pszProjection;
    double	adfGeoTransform[6];

  public:
                 ELASDataset();
                 ~ELASDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

    static GDALDataset *Open( GDALOpenInfo * );
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

class ELASRasterBand : public GDALRasterBand
{
    friend	ELASDataset;

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

CPLErr ELASRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
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
    if( VSIFSeek( poGDS->fp, nOffset, SEEK_SET ) != 0 
        || VSIFRead( pImage, 1, nDataSize, poGDS->fp ) != (size_t) nDataSize )
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

CPLErr ELASRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
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
    
    if( VSIFSeek( poGDS->fp, nOffset, SEEK_SET ) != 0
        || VSIFWrite( pImage, 1, nDataSize, poGDS->fp ) != (size_t) nDataSize )
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

    pszProjection = CPLStrdup("");
}

/************************************************************************/
/*                            ~ELASDataset()                            */
/************************************************************************/

ELASDataset::~ELASDataset()

{
    FlushCache();

    VSIFClose( fp );
    fp = NULL;

    CPLFree( pszProjection );
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
        VSIFWrite( abyHeader, 1024, 1, fp );
        bHeaderModified = FALSE;
    }
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ELASDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 4 )
        return NULL;

    if( poOpenInfo->pabyHeader[0] != 0
        || poOpenInfo->pabyHeader[1] != 0
        || poOpenInfo->pabyHeader[2] != 4
        || poOpenInfo->pabyHeader[3] != 0 )
    {
        return NULL;
    }

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

    poDS->fp = VSIFOpen( poOpenInfo->pszFilename, pszAccess );
    if( poDS->fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to open `%s' with acces `%s' failed.\n",
                  poOpenInfo->pszFilename, pszAccess );
        return NULL;
    }

    poDS->poDriver = poELASDriver;
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Read the header information.                                    */
/* -------------------------------------------------------------------- */
    poDS->bHeaderModified = FALSE;
    if( VSIFRead( poDS->abyHeader, 1024, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Attempt to read 1024 byte header filed on file:\n", 
                  "%s\n", poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Extract information of interest from the header.                */
/* -------------------------------------------------------------------- */
    GInt32	*panHeader = (GInt32 *) poDS->abyHeader;
    int		nStart, nEnd, nELASDataType, nBytesPerSample;
    
    poDS->nLineOffset = CPL_MSBWORD32( panHeader[1] );

    nStart = CPL_MSBWORD32( panHeader[2] );
    nEnd = CPL_MSBWORD32( panHeader[3] );
    poDS->nRasterYSize = nEnd - nStart + 1;

    nStart = CPL_MSBWORD32( panHeader[4] );
    nEnd = CPL_MSBWORD32( panHeader[5] );
    poDS->nRasterXSize = nEnd - nStart + 1;

    poDS->nBands = CPL_MSBWORD32( panHeader[6] );

    nELASDataType = (poDS->abyHeader[74] & 0x7e) >> 2;
    nBytesPerSample = poDS->abyHeader[75];
    
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

    poDS->papoBands = (GDALRasterBand **) VSICalloc(sizeof(GDALRasterBand *),
                                                    poDS->nBands);

    for( iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        poDS->papoBands[iBand] = new ELASRasterBand( poDS, iBand+1 );
    }

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
    GByte	abyHeader[1024];
    GInt32	*panHeader = (GInt32 *) abyHeader;

    memset( abyHeader, 0, 1024 );

    abyHeader[2] = 4;

    panHeader[1] = CPL_MSBWORD32(nBands * nBandOffset);
    
    panHeader[2] = CPL_MSBWORD32(1);
    panHeader[3] = CPL_MSBWORD32(nYSize);

    panHeader[4] = CPL_MSBWORD32(1);
    panHeader[5] = CPL_MSBWORD32(nXSize);

    panHeader[6] = CPL_MSBWORD32(nBands);

    panHeader[7] = CPL_MSBWORD32(0x000010e1);

    abyHeader[72] = 0x04;
    abyHeader[73] = 0xd2;
    abyHeader[75] = GDALGetDataTypeSize(eType) / 8;

    if( eType == GDT_Byte )
        abyHeader[74] = 1 << 2;
    else if( eType == GDT_Float32 )
        abyHeader[74] = 16 << 2;
    else if( eType == GDT_Float64 )
        abyHeader[74] = 17 << 2;

/* -------------------------------------------------------------------- */
/*      Write the header data.                                          */
/* -------------------------------------------------------------------- */
    VSIFWrite( abyHeader, 1024, 1, fp );

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
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ELASDataset::GetProjectionRef()

{
    return( pszProjection );
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
/*                          GDALRegister_ELAS()                        */
/************************************************************************/

void GDALRegister_ELAS()

{
    GDALDriver	*poDriver;

    if( poELASDriver == NULL )
    {
        poELASDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "ELAS";
        poDriver->pszLongName = "ELAS";
        
        poDriver->pfnOpen = ELASDataset::Open;
        poDriver->pfnCreate = ELASDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
