/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  GDALDataset driver for DTED translator.
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
 * Revision 1.7  2002/01/26 05:51:40  warmerda
 * added metadata read/write support
 *
 * Revision 1.6  2001/11/13 15:43:41  warmerda
 * preliminary dted creation working
 *
 * Revision 1.5  2001/11/11 23:50:59  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.4  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
 * Revision 1.3  2000/02/28 16:32:20  warmerda
 * use SetBand method
 *
 * Revision 1.2  2000/01/12 19:43:17  warmerda
 * Don't open dataset twice in Open().
 *
 * Revision 1.1  1999/12/07 18:01:28  warmerda
 * New
 *
 */

#include "dted_api.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$");

static GDALDriver	*poDTEDDriver = NULL;

CPL_C_START
void	GDALRegister_DTED(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				DTEDDataset				*/
/* ==================================================================== */
/************************************************************************/

class DTEDRasterBand;

class DTEDDataset : public GDALDataset
{
    friend class DTEDRasterBand;

    DTEDInfo	*psDTED;

  public:
    virtual	~DTEDDataset();
    
    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            DTEDRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class DTEDRasterBand : public GDALRasterBand
{
    friend class DTEDDataset;

  public:

    		DTEDRasterBand( DTEDDataset *, int );
    
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           DTEDRasterBand()                            */
/************************************************************************/

DTEDRasterBand::DTEDRasterBand( DTEDDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_Int16;

    nBlockXSize = 1;
    nBlockYSize = poDS->GetRasterYSize();;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr DTEDRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    DTEDDataset	*poDTED_DS = (DTEDDataset *) poDS;
    int		nYSize = poDTED_DS->psDTED->nYSize;
    GInt16	*panData;

    CPLAssert( nBlockYOff == 0 );

/* -------------------------------------------------------------------- */
/*      Read the data.                                                  */
/* -------------------------------------------------------------------- */
    panData = (GInt16 *) pImage;
    if( !DTEDReadProfile( poDTED_DS->psDTED, nBlockXOff, panData ) )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Flip line to orient it top to bottom instead of bottom to       */
/*      top.                                                            */
/* -------------------------------------------------------------------- */
    for( int i = nYSize/2; i >= 0; i-- )
    {
        GInt16	nTemp;

        nTemp = panData[i];
        panData[i] = panData[nYSize - i - 1];
        panData[nYSize - i - 1] = nTemp;
    }

    return CE_None;
}

/************************************************************************/
/*                            ~DTEDDataset()                            */
/************************************************************************/

DTEDDataset::~DTEDDataset()

{
    if( psDTED != NULL )
        DTEDClose( psDTED );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DTEDDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    DTEDInfo	*psDTED;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    psDTED = DTEDOpen( poOpenInfo->pszFilename, "rb", TRUE );
    
    if( psDTED == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    DTEDDataset 	*poDS;

    poDS = new DTEDDataset();

    poDS->psDTED = psDTED;
    poDS->poDriver = poDTEDDriver;
    
/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = psDTED->nXSize;
    poDS->nRasterYSize = psDTED->nYSize;
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;;
    for( i = 0; i < poDS->nBands; i++ )
        poDS->SetBand( i+1, new DTEDRasterBand( poDS, i+1 ) );

/* -------------------------------------------------------------------- */
/*      Collect any metadata available.                                 */
/* -------------------------------------------------------------------- */
    char *pszValue;

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_VERTACCURACY );
    poDS->SetMetadataItem( "DTED_VerticalAccuracy", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_SECURITYCODE );
    poDS->SetMetadataItem( "DTED_SecurityCode", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_PRODUCER );
    poDS->SetMetadataItem( "DTED_Producer", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_COMPILATION_DATE );
    poDS->SetMetadataItem( "DTED_CompilationDate", pszValue );
    CPLFree( pszValue );
    
    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr DTEDDataset::GetGeoTransform( double * padfTransform )

{
    padfTransform[0] = psDTED->dfULCornerX;
    padfTransform[1] = psDTED->dfPixelSizeX;
    padfTransform[2] = 0.0;
    padfTransform[3] = psDTED->dfULCornerY;
    padfTransform[4] = 0.0;
    padfTransform[5] = psDTED->dfPixelSizeY * -1;

    return( CE_None );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *DTEDDataset::GetProjectionRef()

{
    return( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]" );
}

/************************************************************************/
/*                           DTEDCreateCopy()                           */
/*                                                                      */
/*      For now we will assume the input is exactly one proper          */
/*      cell.                                                           */
/************************************************************************/

static GDALDataset *
DTEDCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                int bStrict, char ** papszOptions, 
                GDALProgressFunc pfnProgress, void * pProgressData )

{
/* -------------------------------------------------------------------- */
/*      Work out the level.                                             */
/* -------------------------------------------------------------------- */
    int	nLevel;

    if( poSrcDS->GetRasterXSize() == 121 )
        nLevel = 0;
    else if( poSrcDS->GetRasterXSize() == 1201 )
        nLevel = 1;
    else if( poSrcDS->GetRasterXSize() == 3601 )
        nLevel = 2;
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
               "The source does not appear to be a properly formatted cell." );
        nLevel = 1;
    }

/* -------------------------------------------------------------------- */
/*      Work out the LL origin.                                         */
/* -------------------------------------------------------------------- */
    int  nLLOriginLat, nLLOriginLong;
    double adfGeoTransform[6];

    poSrcDS->GetGeoTransform( adfGeoTransform );

    nLLOriginLat = (int) 
        floor(adfGeoTransform[3] 
              + poSrcDS->GetRasterYSize() * adfGeoTransform[5] + 0.5);
    
    nLLOriginLong = (int) floor(adfGeoTransform[0] + 0.5);

/* -------------------------------------------------------------------- */
/*      Create the output dted file.                                    */
/* -------------------------------------------------------------------- */
    const char *pszError;

    pszError = DTEDCreate( pszFilename, nLevel, nLLOriginLat, nLLOriginLong );

    if( pszError != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", pszError );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the DTED file so we can output the data to it.             */
/* -------------------------------------------------------------------- */
    DTEDInfo *psDTED;

    psDTED = DTEDOpen( pszFilename, "rb+", FALSE );
    if( psDTED == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read all the data in one dollup.                                */
/* -------------------------------------------------------------------- */
    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( 1 );
    GInt16	*panData;
    
    panData = (GInt16 *) 
        CPLMalloc(sizeof(GInt16) * psDTED->nXSize * psDTED->nYSize);
    
    poSrcBand->RasterIO( GF_Read, 0, 0, psDTED->nXSize, psDTED->nYSize, 
                         (void *) panData, psDTED->nXSize, psDTED->nYSize, 
                         GDT_Int16, 0, 0 );

/* -------------------------------------------------------------------- */
/*      Write all the profiles.                                         */
/* -------------------------------------------------------------------- */
    GInt16	anProfData[3601];

    for( int iProfile = 0; iProfile < psDTED->nXSize; iProfile++ )
    {
        for( int iY = 0; iY < psDTED->nYSize; iY++ )
            anProfData[iY] = panData[iProfile + iY * psDTED->nXSize];

        DTEDWriteProfile( psDTED, iProfile, anProfData );
    }

    CPLFree( panData );

/* -------------------------------------------------------------------- */
/*      Try to copy any matching available metadata.                    */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetMetadataItem( "DTED_VerticalAccuracy" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_VERTACCURACY, 
                         poSrcDS->GetMetadataItem( "DTED_VerticalAccuracy" ) );

    if( poSrcDS->GetMetadataItem( "DTED_SecurityCode" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_SECURITYCODE, 
                         poSrcDS->GetMetadataItem( "DTED_SecurityCode" ) );

    if( poSrcDS->GetMetadataItem( "DTED_Producer" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_PRODUCER, 
                         poSrcDS->GetMetadataItem( "DTED_Producer" ) );

    if( poSrcDS->GetMetadataItem( "DTED_CompilationDate" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_COMPILATION_DATE, 
                         poSrcDS->GetMetadataItem( "DTED_CompilationDate" ) );

/* -------------------------------------------------------------------- */
/*      Try to open the resulting DTED file.                            */
/* -------------------------------------------------------------------- */
    DTEDClose( psDTED );

    return (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
}

/************************************************************************/
/*                         GDALRegister_DTED()                          */
/************************************************************************/

void GDALRegister_DTED()

{
    GDALDriver	*poDriver;

    if( poDTEDDriver == NULL )
    {
        poDTEDDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "DTED";
        poDriver->pszLongName = "DTED Elevation Raster";
        
        poDriver->pfnOpen = DTEDDataset::Open;
        poDriver->pfnCreateCopy = DTEDCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

