/******************************************************************************
 * $Id$
 *
 * Project:  GXF Reader
 * Purpose:  GDAL binding for GXF reader.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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

#include "gxfopen.h"
#include "gdal_pam.h"

CPL_CVSID("$Id$");

#ifndef PI
#  define PI 3.14159265358979323846
#endif

CPL_C_START
void	GDALRegister_GXF(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				GXFDataset				*/
/* ==================================================================== */
/************************************************************************/

class GXFRasterBand;

class GXFDataset : public GDALPamDataset
{
    friend class GXFRasterBand;
    
    GXFHandle	hGXF;

    char	*pszProjection;
    double      dfNoDataValue;
    GDALDataType eDataType;

  public:
                GXFDataset();
		~GXFDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            GXFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GXFRasterBand : public GDALPamRasterBand
{
    friend class GXFDataset;
    
  public:

    		GXFRasterBand( GXFDataset *, int );
    double      GetNoDataValue(int* bGotNoDataValue);

    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           GXFRasterBand()                            */
/************************************************************************/

GXFRasterBand::GXFRasterBand( GXFDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = poDS->eDataType;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                          GetNoDataValue()                          */
/************************************************************************/

double GXFRasterBand::GetNoDataValue(int* bGotNoDataValue)

{
    GXFDataset	*poGXF_DS = (GXFDataset *) poDS;
    if (bGotNoDataValue)
        *bGotNoDataValue = (fabs(poGXF_DS->dfNoDataValue - -1e12) > .1);
    if (eDataType == GDT_Float32)
        return (double)(float)poGXF_DS->dfNoDataValue;
    else
        return poGXF_DS->dfNoDataValue;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GXFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    GXFDataset	*poGXF_DS = (GXFDataset *) poDS;
    double	*padfBuffer;
    float	*pafBuffer = (float *) pImage;
    int		i;
    CPLErr	eErr;

    CPLAssert( nBlockXOff == 0 );

    if (eDataType == GDT_Float32)
    {
        padfBuffer = (double *) VSIMalloc2(sizeof(double), nBlockXSize);
        if( padfBuffer == NULL )
            return CE_Failure;
        eErr = GXFGetScanline( poGXF_DS->hGXF, nBlockYOff, padfBuffer );
        
        for( i = 0; i < nBlockXSize; i++ )
            pafBuffer[i] = (float) padfBuffer[i];
    
        CPLFree( padfBuffer );
    }
    else if (eDataType == GDT_Float64)
        eErr = GXFGetScanline( poGXF_DS->hGXF, nBlockYOff, (double*)pImage );
    else
        eErr = CE_Failure;
    
    return eErr;
}

/************************************************************************/
/* ==================================================================== */
/*				GXFDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             GXFDataset()                             */
/************************************************************************/

GXFDataset::GXFDataset()

{
    pszProjection = NULL;
    hGXF = NULL;
    dfNoDataValue = 0;
    eDataType = GDT_Float32;
}

/************************************************************************/
/*                            ~GXFDataset()                             */
/************************************************************************/

GXFDataset::~GXFDataset()

{
    FlushCache();
    if( hGXF != NULL )
        GXFClose( hGXF );
    CPLFree( pszProjection );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GXFDataset::GetGeoTransform( double * padfTransform )

{
    CPLErr	eErr;
    double	dfXOrigin, dfYOrigin, dfXSize, dfYSize, dfRotation;

    eErr = GXFGetPosition( hGXF, &dfXOrigin, &dfYOrigin,
                           &dfXSize, &dfYSize, &dfRotation );

    if( eErr != CE_None )
        return eErr;

    // Transform to radians. 
    dfRotation = (dfRotation / 360.0) * 2 * PI;

    padfTransform[1] = dfXSize * cos(dfRotation);
    padfTransform[2] = dfYSize * sin(dfRotation);
    padfTransform[4] = dfXSize * sin(dfRotation);
    padfTransform[5] = -1 * dfYSize * cos(dfRotation);

    // take into account that GXF is point or center of pixel oriented.
    padfTransform[0] = dfXOrigin - 0.5*padfTransform[1] - 0.5*padfTransform[2];
    padfTransform[3] = dfYOrigin - 0.5*padfTransform[4] - 0.5*padfTransform[5];
    
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GXFDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GXFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    GXFHandle	hGXF;
    int		i, bFoundKeyword, bFoundIllegal;
    
/* -------------------------------------------------------------------- */
/*      Before trying GXFOpen() we first verify that there is at        */
/*      least one "\n#keyword" type signature in the first chunk of     */
/*      the file.                                                       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 50 )
        return NULL;

    bFoundKeyword = FALSE;
    bFoundIllegal = FALSE;
    for( i = 0; i < poOpenInfo->nHeaderBytes-1; i++ )
    {
        if( (poOpenInfo->pabyHeader[i] == 10
             || poOpenInfo->pabyHeader[i] == 13)
            && poOpenInfo->pabyHeader[i+1] == '#' )
        {
            bFoundKeyword = TRUE;
        }
        if( poOpenInfo->pabyHeader[i] == 0 )
        {
            bFoundIllegal = TRUE;
            break;
        }
    }

    if( !bFoundKeyword || bFoundIllegal )
        return NULL;
    
    
/* -------------------------------------------------------------------- */
/*      At this point it is plausible that this is a GXF file, but      */
/*      we also now verify that there is a #GRID keyword before         */
/*      passing it off to GXFOpen().  We check in the first 50K.        */
/* -------------------------------------------------------------------- */
#define BIGBUFSIZE 50000
    int nBytesRead, bGotGrid = FALSE;
    FILE *fp;

    fp = VSIFOpen( poOpenInfo->pszFilename, "rb" );
    if( fp == NULL )
        return NULL;

    char *pszBigBuf = (char *) CPLMalloc(BIGBUFSIZE);
    nBytesRead = VSIFRead( pszBigBuf, 1, BIGBUFSIZE, fp );
    VSIFClose( fp );

    for( i = 0; i < nBytesRead - 5 && !bGotGrid; i++ )
    {
        if( pszBigBuf[i] == '#' && EQUALN(pszBigBuf+i+1,"GRID",4) )
            bGotGrid = TRUE;
    }

    CPLFree( pszBigBuf );

    if( !bGotGrid )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    
    hGXF = GXFOpen( poOpenInfo->pszFilename );
    
    if( hGXF == NULL )
        return( NULL );
        
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        GXFClose(hGXF);
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The GXF driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GXFDataset 	*poDS;

    poDS = new GXFDataset();
    
    const char* pszGXFDataType = CPLGetConfigOption("GXF_DATATYPE", "Float32");
    GDALDataType eDT = GDALGetDataTypeByName(pszGXFDataType);
    if (!(eDT == GDT_Float32 || eDT == GDT_Float64))
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported value for GXF_DATATYPE : %s", pszGXFDataType);
        eDT = GDT_Float32;
    }

    poDS->hGXF = hGXF;
    poDS->eDataType = eDT;
    
/* -------------------------------------------------------------------- */
/*	Establish the projection.					*/
/* -------------------------------------------------------------------- */
    poDS->pszProjection = GXFGetMapProjectionAsOGCWKT( hGXF );

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    GXFGetRawInfo( hGXF, &(poDS->nRasterXSize), &(poDS->nRasterYSize), NULL,
                   NULL, NULL, &(poDS->dfNoDataValue) );

    if  (poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Invalid dimensions : %d x %d", 
                  poDS->nRasterXSize, poDS->nRasterYSize); 
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->SetBand( 1, new GXFRasterBand( poDS, 1 ));

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
/*                          GDALRegister_GXF()                          */
/************************************************************************/

void GDALRegister_GXF()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "GXF" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GXF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "GeoSoft Grid Exchange Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#GXF" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gxf" );

        poDriver->pfnOpen = GXFDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
