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
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.17  2005/05/05 15:52:48  fwarmerdam
 * PAM Enabled
 *
 * Revision 1.16  2004/11/03 01:17:58  fwarmerdam
 * added extra testing to avoid mis-identification
 *
 * Revision 1.15  2004/04/06 19:23:48  warmerda
 * Don't forget to close the dataset!
 *
 * Revision 1.14  2003/10/06 20:50:45  warmerda
 * fixed author email
 *
 * Revision 1.13  2003/07/08 21:20:21  warmerda
 * avoid warnings
 *
 * Revision 1.12  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.11  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.10  2001/11/11 23:51:00  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.9  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.8  2000/11/16 14:56:16  warmerda
 * fail testopen on zero char, reduce min header size
 *
 * Revision 1.7  2000/02/28 16:33:49  warmerda
 * use SetBand method
 *
 * Revision 1.6  2000/02/14 16:24:57  warmerda
 * Fixed comment.
 *
 * Revision 1.5  1999/10/29 17:30:23  warmerda
 * read OGC rather than PROJ.4 definition
 *
 * Revision 1.4  1999/10/27 20:21:45  warmerda
 * added projection/transform support
 *
 * Revision 1.3  1999/01/11 15:32:12  warmerda
 * Added testing of file to verify it is GXF.
 *
 * Revision 1.2  1998/12/15 19:06:49  warmerda
 * IReadBlock(), and GXFGetRawInfo()
 *
 * Revision 1.1  1998/12/06 02:53:22  warmerda
 * New
 *
 */

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
    
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           GXFRasterBand()                            */
/************************************************************************/

GXFRasterBand::GXFRasterBand( GXFDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
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

    padfBuffer = (double *) CPLMalloc(sizeof(double) * nBlockXSize);
    eErr = GXFGetRawScanline( poGXF_DS->hGXF, nBlockYOff, padfBuffer );
    
    for( i = 0; i < nBlockXSize; i++ )
        pafBuffer[i] = (float) padfBuffer[i];

    CPLFree( padfBuffer );
    
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
    int nBytesRead, bGotGrid = FALSE;
    char szBigBuf[50000];
    FILE *fp;

    fp = VSIFOpen( poOpenInfo->pszFilename, "rb" );
    if( fp == NULL )
        return NULL;

    nBytesRead = VSIFRead( szBigBuf, 1, sizeof(szBigBuf), fp );
    VSIFClose( fp );

    for( i = 0; i < nBytesRead - 5 && !bGotGrid; i++ )
    {
        if( szBigBuf[i] == '#' && EQUALN(szBigBuf+i+1,"GRID",4) )
            bGotGrid = TRUE;
    }

    if( !bGotGrid )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    
    hGXF = GXFOpen( poOpenInfo->pszFilename );
    
    if( hGXF == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GXFDataset 	*poDS;

    poDS = new GXFDataset();

    poDS->hGXF = hGXF;
    
/* -------------------------------------------------------------------- */
/*	Establish the projection.					*/
/* -------------------------------------------------------------------- */
    poDS->pszProjection = GXFGetMapProjectionAsOGCWKT( hGXF );

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    GXFGetRawInfo( hGXF, &(poDS->nRasterXSize), &(poDS->nRasterYSize), NULL,
                   NULL, NULL, NULL );
    
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
