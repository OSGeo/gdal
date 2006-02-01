/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Implementation for ELAS DIPX format variant.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2006/02/01 17:22:34  fwarmerdam
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_DIPX(void);
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
    char	unused2[8];
    GInt32	LABL;	/* used by LABL module */
    char	HEAD;	/* used by HEAD module */
    double      XOffset; 
    double      YOffset;
    double      XPixSize;
    double      YPixSize;
    double      Matrix[4];
    char        unused3[344];
    GUInt16	ColorTable[256];  /* RGB packed with 4 bits each */
    char	unused4[32];
} DIPXHeader;

/************************************************************************/
/* ==================================================================== */
/*				DIPXDataset				*/
/* ==================================================================== */
/************************************************************************/

class DIPXRasterBand;

class DIPXDataset : public GDALPamDataset
{
    friend class DIPXRasterBand;

    FILE	*fp;

    DIPXHeader  sHeader;

    GDALDataType eRasterDataType;

    double	adfGeoTransform[6];

  public:
                 DIPXDataset();
                 ~DIPXDataset();

    virtual CPLErr GetGeoTransform( double * );

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                             DIPXDataset                              */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            DIPXDataset()                             */
/************************************************************************/

DIPXDataset::DIPXDataset()

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
/*                            ~DIPXDataset()                            */
/************************************************************************/

DIPXDataset::~DIPXDataset()

{
    VSIFCloseL( fp );
    fp = NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DIPXDataset::Open( GDALOpenInfo * poOpenInfo )

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
    DIPXDataset 	*poDS;
    const char	 	*pszAccess;

    if( poOpenInfo->eAccess == GA_Update )
        pszAccess = "r+b";
    else
        pszAccess = "rb";

    poDS = new DIPXDataset();

    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, pszAccess );
    if( poDS->fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to open `%s' with acces `%s' failed.\n",
                  poOpenInfo->pszFilename, pszAccess );
        return NULL;
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Read the header information.                                    */
/* -------------------------------------------------------------------- */
    if( VSIFReadL( &(poDS->sHeader), 1024, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Attempt to read 1024 byte header filed on file:\n", 
                  "%s\n", poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Extract information of interest from the header.                */
/* -------------------------------------------------------------------- */
    int		nStart, nEnd, nDIPXDataType, nBytesPerSample;
    int         nLineOffset;
    
    nLineOffset = CPL_LSBWORD32( poDS->sHeader.NBPR );

    nStart = CPL_LSBWORD32( poDS->sHeader.IL );
    nEnd = CPL_LSBWORD32( poDS->sHeader.LL );
    poDS->nRasterYSize = nEnd - nStart + 1;

    nStart = CPL_LSBWORD32( poDS->sHeader.IE );
    nEnd = CPL_LSBWORD32( poDS->sHeader.LE );
    poDS->nRasterXSize = nEnd - nStart + 1;

    poDS->nBands = CPL_LSBWORD32( poDS->sHeader.NC );

    nDIPXDataType = (poDS->sHeader.IH19[1] & 0x7e) >> 2;
    nBytesPerSample = poDS->sHeader.IH19[0];
    
    if( nDIPXDataType == 0 && nBytesPerSample == 1 )
        poDS->eRasterDataType = GDT_Byte;
    else if( nDIPXDataType == 1 && nBytesPerSample == 1 )
        poDS->eRasterDataType = GDT_Byte;
    else if( nDIPXDataType == 16 && nBytesPerSample == 4 )
        poDS->eRasterDataType = GDT_Float32;
    else if( nDIPXDataType == 17 && nBytesPerSample == 8 )
        poDS->eRasterDataType = GDT_Float64;
    else
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unrecognised image data type %d, with BytesPerSample=%d.\n",
                  nDIPXDataType, nBytesPerSample );
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
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr DIPXDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    return( CE_None );
}

/************************************************************************/
/*                          GDALRegister_DIPX()                        */
/************************************************************************/

void GDALRegister_DIPX()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "DIPX" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "DIPX" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "DIPX" );
        
        poDriver->pfnOpen = DIPXDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
