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
 * Revision 1.17  2006/03/27 14:52:46  fwarmerdam
 * Added overview support.
 *
 * Revision 1.16  2005/05/05 14:01:36  fwarmerdam
 * PAM Enable
 *
 * Revision 1.15  2005/04/15 19:28:57  fwarmerdam
 * added AREA_OR_POINT=Point metadata
 *
 * Revision 1.14  2004/01/30 18:27:25  gwalter
 * Fixed bug in tile sizing.
 *
 * Revision 1.13  2004/01/29 23:35:22  gwalter
 * Add a few more metadata fields, make sure that
 * nodata value is recognized.
 *
 * Revision 1.12  2003/05/30 16:17:21  warmerda
 * fix warnings with casting and unused parameters
 *
 * Revision 1.11  2002/11/23 18:54:17  warmerda
 * added CREATIONDATATYPES metadata for drivers
 *
 * Revision 1.10  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.9  2002/06/12 21:12:24  warmerda
 * update to metadata based driver info
 *
 * Revision 1.8  2002/03/05 14:26:01  warmerda
 * expanded tabs
 *
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
#include "gdal_pam.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_DTED(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                              DTEDDataset                             */
/* ==================================================================== */
/************************************************************************/

class DTEDRasterBand;

class DTEDDataset : public GDALPamDataset
{
    friend class DTEDRasterBand;

    DTEDInfo    *psDTED;

  public:
    virtual     ~DTEDDataset();
    
    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            DTEDRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class DTEDRasterBand : public GDALPamRasterBand
{
    friend class DTEDDataset;

    int 	bNoDataSet;
    double	dfNoDataValue;

  public:

                DTEDRasterBand( DTEDDataset *, int );
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual double  GetNoDataValue( int *pbSuccess = NULL );
};


/************************************************************************/
/*                           DTEDRasterBand()                            */
/************************************************************************/

DTEDRasterBand::DTEDRasterBand( DTEDDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_Int16;

    bNoDataSet = TRUE;
    dfNoDataValue = (double) DTED_NODATA_VALUE;

    nBlockXSize = 1;
    nBlockYSize = poDS->GetRasterYSize();;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr DTEDRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    DTEDDataset *poDTED_DS = (DTEDDataset *) poDS;
    int         nYSize = poDTED_DS->psDTED->nYSize;
    GInt16      *panData;

    (void) nBlockXOff;
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
        GInt16  nTemp;

        nTemp = panData[i];
        panData[i] = panData[nYSize - i - 1];
        panData[nYSize - i - 1] = nTemp;
    }

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double DTEDRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataSet;

    return dfNoDataValue;
}

/************************************************************************/
/*                            ~DTEDDataset()                            */
/************************************************************************/

DTEDDataset::~DTEDDataset()

{
    FlushCache();
    if( psDTED != NULL )
        DTEDClose( psDTED );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DTEDDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int         i;
    DTEDInfo    *psDTED;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    psDTED = DTEDOpen( poOpenInfo->pszFilename, "rb", TRUE );
    
    if( psDTED == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    DTEDDataset         *poDS;

    poDS = new DTEDDataset();

    poDS->psDTED = psDTED;
    
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

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_VERTACCURACY_UHL );
    poDS->SetMetadataItem( "DTED_VerticalAccuracy_UHL", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_VERTACCURACY_ACC );
    poDS->SetMetadataItem( "DTED_VerticalAccuracy_ACC", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_SECURITYCODE_UHL );
    poDS->SetMetadataItem( "DTED_SecurityCode_UHL", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_SECURITYCODE_DSI );
    poDS->SetMetadataItem( "DTED_SecurityCode_DSI", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_UNIQUEREF_UHL );
    poDS->SetMetadataItem( "DTED_UniqueRef_UHL", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_UNIQUEREF_DSI );
    poDS->SetMetadataItem( "DTED_UniqueRef_DSI", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_DATA_EDITION );
    poDS->SetMetadataItem( "DTED_DataEdition", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_MATCHMERGE_VERSION );
    poDS->SetMetadataItem( "DTED_MatchMergeVersion", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_MAINT_DATE );
    poDS->SetMetadataItem( "DTED_MaintenanceDate", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_MATCHMERGE_DATE );
    poDS->SetMetadataItem( "DTED_MatchMergeDate", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_MAINT_DESCRIPTION );
    poDS->SetMetadataItem( "DTED_MaintenanceDescription", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_PRODUCER );
    poDS->SetMetadataItem( "DTED_Producer", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_VERTDATUM );
    poDS->SetMetadataItem( "DTED_VerticalDatum", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_DIGITIZING_SYS );
    poDS->SetMetadataItem( "DTED_DigitizingSystem", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_COMPILATION_DATE );
    poDS->SetMetadataItem( "DTED_CompilationDate", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_HORIZACCURACY );
    poDS->SetMetadataItem( "DTED_HorizontalAccuracy", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_REL_HORIZACCURACY );
    poDS->SetMetadataItem( "DTED_RelHorizontalAccuracy", pszValue );
    CPLFree( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_REL_VERTACCURACY );
    poDS->SetMetadataItem( "DTED_RelVerticalAccuracy", pszValue );
    CPLFree( pszValue );
    
    poDS->SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

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
    (void) pProgressData;
    (void) pfnProgress;
    (void) papszOptions;
    (void) bStrict;

/* -------------------------------------------------------------------- */
/*      Work out the level.                                             */
/* -------------------------------------------------------------------- */
    int nLevel;

    if( poSrcDS->GetRasterYSize() == 121 )
        nLevel = 0;
    else if( poSrcDS->GetRasterYSize() == 1201 )
        nLevel = 1;
    else if( poSrcDS->GetRasterYSize() == 3601 )
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
    GInt16      *panData;
    
    panData = (GInt16 *) 
        CPLMalloc(sizeof(GInt16) * psDTED->nXSize * psDTED->nYSize);
    
    poSrcBand->RasterIO( GF_Read, 0, 0, psDTED->nXSize, psDTED->nYSize, 
                         (void *) panData, psDTED->nXSize, psDTED->nYSize, 
                         GDT_Int16, 0, 0 );

/* -------------------------------------------------------------------- */
/*      Write all the profiles.                                         */
/* -------------------------------------------------------------------- */
    GInt16      anProfData[3601];
    double       dfNodataCount=0.0;
    GByte       iPartialCell;

    for( int iProfile = 0; iProfile < psDTED->nXSize; iProfile++ )
    {
        for( int iY = 0; iY < psDTED->nYSize; iY++ )
        {
            anProfData[iY] = panData[iProfile + iY * psDTED->nXSize];
            if ( anProfData[iY] == DTED_NODATA_VALUE )
                dfNodataCount = dfNodataCount+1.0;
        }
        DTEDWriteProfile( psDTED, iProfile, anProfData );
    }
    CPLFree( panData );

/* -------------------------------------------------------------------- */
/* Partial cell indicator: 0 for complete coverage; 1-99 for incomplete */
/* -------------------------------------------------------------------- */
    char pszPartialCell[2];
    
    if ( dfNodataCount < 0.5 )
        iPartialCell = 0;
    else
    {
      iPartialCell = int(floor(100.0 - 
           (dfNodataCount*100.0/(psDTED->nXSize * psDTED->nYSize))));
        if (iPartialCell < 1)
           iPartialCell=1;       
    }
    sprintf(pszPartialCell,"%02d",iPartialCell);
    strncpy((char *) (psDTED->pachDSIRecord+289), pszPartialCell, 2 );

/* -------------------------------------------------------------------- */
/*      Try to copy any matching available metadata.                    */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetMetadataItem( "DTED_VerticalAccuracy_UHL" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_VERTACCURACY_UHL, 
                     poSrcDS->GetMetadataItem( "DTED_VerticalAccuracy_UHL" ) );

    if( poSrcDS->GetMetadataItem( "DTED_VerticalAccuracy_ACC" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_VERTACCURACY_ACC, 
                    poSrcDS->GetMetadataItem( "DTED_VerticalAccuracy_ACC" ) );

    if( poSrcDS->GetMetadataItem( "DTED_SecurityCode_UHL" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_SECURITYCODE_UHL, 
                    poSrcDS->GetMetadataItem( "DTED_SecurityCode_UHL" ) );

    if( poSrcDS->GetMetadataItem( "DTED_SecurityCode_DSI" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_SECURITYCODE_DSI, 
                    poSrcDS->GetMetadataItem( "DTED_SecurityCode_DSI" ) );

    if( poSrcDS->GetMetadataItem( "DTED_UniqueRef_UHL" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_UNIQUEREF_UHL, 
                         poSrcDS->GetMetadataItem( "DTED_UniqueRef_UHL" ) );

    if( poSrcDS->GetMetadataItem( "DTED_UniqueRef_DSI" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_UNIQUEREF_DSI, 
                         poSrcDS->GetMetadataItem( "DTED_UniqueRef_DSI" ) );

    if( poSrcDS->GetMetadataItem( "DTED_DataEdition" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_DATA_EDITION, 
                         poSrcDS->GetMetadataItem( "DTED_DataEdition" ) );

    if( poSrcDS->GetMetadataItem( "DTED_MatchMergeVersion" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_MATCHMERGE_VERSION, 
                     poSrcDS->GetMetadataItem( "DTED_MatchMergeVersion" ) );

    if( poSrcDS->GetMetadataItem( "DTED_MaintenanceDate" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_MAINT_DATE, 
                         poSrcDS->GetMetadataItem( "DTED_MaintenanceDate" ) );

    if( poSrcDS->GetMetadataItem( "DTED_MatchMergeDate" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_MATCHMERGE_DATE, 
                         poSrcDS->GetMetadataItem( "DTED_MatchMergeDate" ) );

    if( poSrcDS->GetMetadataItem( "DTED_MaintenanceDescription" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_MAINT_DESCRIPTION, 
                 poSrcDS->GetMetadataItem( "DTED_MaintenanceDescription" ) );

    if( poSrcDS->GetMetadataItem( "DTED_Producer" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_PRODUCER, 
                         poSrcDS->GetMetadataItem( "DTED_Producer" ) );

    if( poSrcDS->GetMetadataItem( "DTED_VerticalDatum" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_VERTDATUM, 
                         poSrcDS->GetMetadataItem( "DTED_VerticalDatum" ) );

    if( poSrcDS->GetMetadataItem( "DTED_DigitizingSystem" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_DIGITIZING_SYS, 
                         poSrcDS->GetMetadataItem( "DTED_DigitizingSystem" ) );

    if( poSrcDS->GetMetadataItem( "DTED_CompilationDate" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_COMPILATION_DATE, 
                         poSrcDS->GetMetadataItem( "DTED_CompilationDate" ) );

    if( poSrcDS->GetMetadataItem( "DTED_HorizontalAccuracy" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_HORIZACCURACY, 
                     poSrcDS->GetMetadataItem( "DTED_HorizontalAccuracy" ) );

    if( poSrcDS->GetMetadataItem( "DTED_RelHorizontalAccuracy" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_REL_HORIZACCURACY, 
                   poSrcDS->GetMetadataItem( "DTED_RelHorizontalAccuracy" ) );

    if( poSrcDS->GetMetadataItem( "DTED_RelVerticalAccuracy" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_REL_VERTACCURACY, 
                     poSrcDS->GetMetadataItem( "DTED_RelVerticalAccuracy" ) );

/* -------------------------------------------------------------------- */
/*      Try to open the resulting DTED file.                            */
/* -------------------------------------------------------------------- */
    DTEDClose( psDTED );

/* -------------------------------------------------------------------- */
/*      Reopen and copy missing information into a PAM file.            */
/* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = (GDALPamDataset *) 
        GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_DTED()                          */
/************************************************************************/

void GDALRegister_DTED()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "DTED" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "DTED" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "DTED Elevation Raster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#DTED" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16" );
        
        poDriver->pfnOpen = DTEDDataset::Open;
        poDriver->pfnCreateCopy = DTEDCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

