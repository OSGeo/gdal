/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Implementation for ELAS DIPEx format variant.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
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

#include "rawdataset.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_DIPEx(void);
CPL_C_END

typedef struct {
    GInt32	NBIH;	/* bytes in header, normaly 1024 */
    GInt32      NBPR;	/* bytes per data record (all bands of scanline) */
    GInt32	IL;	/* initial line - normally 1 */
    GInt32	LL;	/* last line */
    GInt32	IE;	/* initial element (pixel), normally 1 */
    GInt32	LE;	/* last element (pixel) */
    GInt32	NC;	/* number of channels (bands) */
    GInt32	H4322;	/* header record identifier - always 4322. */
    char        unused1[40]; 
    GByte	IH19[4];/* data type, and size flags */
    GInt32	IH20;	/* number of secondary headers */
    GInt32	SRID;	
    char        unused2[12];
    double      YOffset;
    double      XOffset; 
    double      YPixSize;
    double      XPixSize;
    double      Matrix[4];
    char        unused3[344];
    GUInt16	ColorTable[256];  /* RGB packed with 4 bits each */
    char	unused4[32];
} DIPExHeader;

/************************************************************************/
/* ==================================================================== */
/*				DIPExDataset				*/
/* ==================================================================== */
/************************************************************************/

class DIPExRasterBand;

class DIPExDataset : public GDALPamDataset
{
    friend class DIPExRasterBand;

    VSILFILE	*fp;
    CPLString    osSRS;

    DIPExHeader  sHeader;

    GDALDataType eRasterDataType;

    double	adfGeoTransform[6];

  public:
                 DIPExDataset();
                 ~DIPExDataset();

    virtual CPLErr GetGeoTransform( double * );

    virtual const char *GetProjectionRef( void );
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                             DIPExDataset                              */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            DIPExDataset()                             */
/************************************************************************/

DIPExDataset::DIPExDataset()

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
/*                            ~DIPExDataset()                            */
/************************************************************************/

DIPExDataset::~DIPExDataset()

{
    if (fp)
        VSIFCloseL( fp );
    fp = NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DIPExDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 256 )
        return NULL;

    if( CPL_LSBWORD32(*((GInt32 *) (poOpenInfo->pabyHeader+0))) != 1024 )
        return NULL;

    if( CPL_LSBWORD32(*((GInt32 *) (poOpenInfo->pabyHeader+28))) != 4322 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    DIPExDataset 	*poDS;
    const char	 	*pszAccess;

    if( poOpenInfo->eAccess == GA_Update )
        pszAccess = "r+b";
    else
        pszAccess = "rb";

    poDS = new DIPExDataset();

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
    int		nStart, nEnd, nDIPExDataType, nBytesPerSample;
    int         nLineOffset;
    
    nLineOffset = CPL_LSBWORD32( poDS->sHeader.NBPR );

    nStart = CPL_LSBWORD32( poDS->sHeader.IL );
    nEnd = CPL_LSBWORD32( poDS->sHeader.LL );
    poDS->nRasterYSize = nEnd - nStart + 1;

    nStart = CPL_LSBWORD32( poDS->sHeader.IE );
    nEnd = CPL_LSBWORD32( poDS->sHeader.LE );
    poDS->nRasterXSize = nEnd - nStart + 1;

    poDS->nBands = CPL_LSBWORD32( poDS->sHeader.NC );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(poDS->nBands, FALSE))
    {
        delete poDS;
        return NULL;
    }

    nDIPExDataType = (poDS->sHeader.IH19[1] & 0x7e) >> 2;
    nBytesPerSample = poDS->sHeader.IH19[0];
    
    if( nDIPExDataType == 0 && nBytesPerSample == 1 )
        poDS->eRasterDataType = GDT_Byte;
    else if( nDIPExDataType == 1 && nBytesPerSample == 1 )
        poDS->eRasterDataType = GDT_Byte;
    else if( nDIPExDataType == 16 && nBytesPerSample == 4 )
        poDS->eRasterDataType = GDT_Float32;
    else if( nDIPExDataType == 17 && nBytesPerSample == 8 )
        poDS->eRasterDataType = GDT_Float64;
    else
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unrecognised image data type %d, with BytesPerSample=%d.\n",
                  nDIPExDataType, nBytesPerSample );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    for( iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, 
                       new RawRasterBand( poDS, iBand+1, poDS->fp, 
                                          1024 + iBand * nLineOffset, 
                                          nBytesPerSample, 
                                          nLineOffset * poDS->nBands,
                                          poDS->eRasterDataType, 
                                          CPL_IS_LSB, TRUE ) );
    }

/* -------------------------------------------------------------------- */
/*	Extract the projection coordinates, if present.			*/
/* -------------------------------------------------------------------- */
    CPL_LSBPTR64(&(poDS->sHeader.XPixSize));
    CPL_LSBPTR64(&(poDS->sHeader.YPixSize));
    CPL_LSBPTR64(&(poDS->sHeader.XOffset));
    CPL_LSBPTR64(&(poDS->sHeader.YOffset));

    if( poDS->sHeader.XOffset != 0 )
    {
        poDS->adfGeoTransform[0] = poDS->sHeader.XOffset;
        poDS->adfGeoTransform[1] = poDS->sHeader.XPixSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = poDS->sHeader.YOffset;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -1.0 * ABS(poDS->sHeader.YPixSize);

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
/*      Look for SRID.                                                  */
/* -------------------------------------------------------------------- */
    CPL_LSBPTR32( &(poDS->sHeader.SRID) );
    
    if( poDS->sHeader.SRID > 0 && poDS->sHeader.SRID < 33000 )
    {
        OGRSpatialReference oSR;

        if( oSR.importFromEPSG( poDS->sHeader.SRID ) == OGRERR_NONE )
        {
            char *pszWKT = NULL;
            oSR.exportToWkt( &pszWKT );
            poDS->osSRS = pszWKT;
            CPLFree( pszWKT );
        }
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
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *DIPExDataset::GetProjectionRef()

{
    return osSRS.c_str();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr DIPExDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    return( CE_None );
}

/************************************************************************/
/*                          GDALRegister_DIPEx()                        */
/************************************************************************/

void GDALRegister_DIPEx()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "DIPEx" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "DIPEx" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "DIPEx" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = DIPExDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
