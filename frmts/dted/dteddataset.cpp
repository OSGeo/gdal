/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  GDALDataset driver for DTED translator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "dted_api.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

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

    char        *pszFilename;
    DTEDInfo    *psDTED;
    int         bVerifyChecksum;
    char       *pszProjection;

  public:
                 DTEDDataset();
    virtual     ~DTEDDataset();
    
    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
    
    const char* GetFileName() { return pszFilename; }
    void SetFileName(const char* pszFilename);
    
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
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual double  GetNoDataValue( int *pbSuccess = NULL );

    virtual const char* GetUnitType() { return "m"; }
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

    /* For some applications, it may be valuable to consider the whole DTED */
    /* file as single block, as the column orientation doesn't fit very well */
    /* with some scanline oriented algorithms */
    /* Of course you need to have a big enough case size, particularly for DTED 2 */
    /* datasets */
    nBlockXSize = CSLTestBoolean(CPLGetConfigOption("GDAL_DTED_SINGLE_BLOCK", "NO")) ?
                            poDS->GetRasterXSize() : 1;
    nBlockYSize = poDS->GetRasterYSize();
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

    if (nBlockXSize != 1)
    {
        panData = (GInt16 *) pImage;
        GInt16* panBuffer = (GInt16*) CPLMalloc(sizeof(GInt16) * nBlockYSize);
        int i;
        for(i=0;i<nBlockXSize;i++)
        {
            if( !DTEDReadProfileEx( poDTED_DS->psDTED, i, panBuffer,
                                    poDTED_DS->bVerifyChecksum ) )
            {
                CPLFree(panBuffer);
                return CE_Failure;
            }
            int j;
            for(j=0;j<nBlockYSize;j++)
            {
                panData[j * nBlockXSize + i] = panBuffer[nYSize - j - 1];
            }
        }

        CPLFree(panBuffer);
        return CE_None;
    }
    
/* -------------------------------------------------------------------- */
/*      Read the data.                                                  */
/* -------------------------------------------------------------------- */
    panData = (GInt16 *) pImage;
    if( !DTEDReadProfileEx( poDTED_DS->psDTED, nBlockXOff, panData,
                            poDTED_DS->bVerifyChecksum ) )
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
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr DTEDRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    DTEDDataset *poDTED_DS = (DTEDDataset *) poDS;
    GInt16      *panData;

    (void) nBlockXOff;
    CPLAssert( nBlockYOff == 0 );

    if (poDTED_DS->eAccess != GA_Update)
        return CE_Failure;

    if (nBlockXSize != 1)
    {
        panData = (GInt16 *) pImage;
        GInt16* panBuffer = (GInt16*) CPLMalloc(sizeof(GInt16) * nBlockYSize);
        int i;
        for(i=0;i<nBlockXSize;i++)
        {
            int j;
            for(j=0;j<nBlockYSize;j++)
            {
                panBuffer[j] = panData[j * nBlockXSize + i];
            }
            if( !DTEDWriteProfile( poDTED_DS->psDTED, i, panBuffer) )
            {
                CPLFree(panBuffer);
                return CE_Failure;
            }
        }

        CPLFree(panBuffer);
        return CE_None;
    }

    panData = (GInt16 *) pImage;
    if( !DTEDWriteProfile( poDTED_DS->psDTED, nBlockXOff, panData) )
        return CE_Failure;

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

DTEDDataset::DTEDDataset()
{
    pszFilename = CPLStrdup("unknown");
    pszProjection = CPLStrdup("");
    bVerifyChecksum = CSLTestBoolean(CPLGetConfigOption("DTED_VERIFY_CHECKSUM", "NO"));
}

/************************************************************************/
/*                            ~DTEDDataset()                            */
/************************************************************************/

DTEDDataset::~DTEDDataset()

{
    FlushCache();
    CPLFree(pszFilename);
    CPLFree( pszProjection );
    if( psDTED != NULL )
        DTEDClose( psDTED );
}

/************************************************************************/
/*                            SetFileName()                             */
/************************************************************************/

void DTEDDataset::SetFileName(const char* pszFilename)

{
    CPLFree(this->pszFilename);
    this->pszFilename = CPLStrdup(pszFilename);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DTEDDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int         i;
    DTEDInfo    *psDTED;

/* -------------------------------------------------------------------- */
/*      Does the file start with one of the possible DTED header        */
/*      record types, and do we have a UHL marker?                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 240 )
        return NULL;

    if( !EQUALN((const char *)poOpenInfo->pabyHeader,"VOL",3)
        && !EQUALN((const char *)poOpenInfo->pabyHeader,"HDR",3)
        && !EQUALN((const char *)poOpenInfo->pabyHeader,"UHL",3) )
    {
        return NULL;
    }

    int bFoundUHL = FALSE;
    for(i=0;i<poOpenInfo->nHeaderBytes-3 && !bFoundUHL ;i += DTED_UHL_SIZE)
    {
        if( EQUALN((const char *)poOpenInfo->pabyHeader + i,"UHL", 3) )
        {
            bFoundUHL = TRUE;
        }
    }
    if (!bFoundUHL)
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    psDTED = DTEDOpen( poOpenInfo->pszFilename, (poOpenInfo->eAccess == GA_Update) ? "rb+" : "rb", TRUE );

    if( psDTED == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    DTEDDataset         *poDS;

    poDS = new DTEDDataset();
    poDS->SetFileName(poOpenInfo->pszFilename);

    poDS->eAccess = poOpenInfo->eAccess;
    poDS->psDTED = psDTED;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = psDTED->nXSize;
    poDS->nRasterYSize = psDTED->nYSize;
    
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }
    
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
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_VERTACCURACY_ACC );
    poDS->SetMetadataItem( "DTED_VerticalAccuracy_ACC", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_SECURITYCODE_UHL );
    poDS->SetMetadataItem( "DTED_SecurityCode_UHL", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_SECURITYCODE_DSI );
    poDS->SetMetadataItem( "DTED_SecurityCode_DSI", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_UNIQUEREF_UHL );
    poDS->SetMetadataItem( "DTED_UniqueRef_UHL", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_UNIQUEREF_DSI );
    poDS->SetMetadataItem( "DTED_UniqueRef_DSI", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_DATA_EDITION );
    poDS->SetMetadataItem( "DTED_DataEdition", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_MATCHMERGE_VERSION );
    poDS->SetMetadataItem( "DTED_MatchMergeVersion", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_MAINT_DATE );
    poDS->SetMetadataItem( "DTED_MaintenanceDate", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_MATCHMERGE_DATE );
    poDS->SetMetadataItem( "DTED_MatchMergeDate", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_MAINT_DESCRIPTION );
    poDS->SetMetadataItem( "DTED_MaintenanceDescription", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_PRODUCER );
    poDS->SetMetadataItem( "DTED_Producer", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_VERTDATUM );
    poDS->SetMetadataItem( "DTED_VerticalDatum", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_HORIZDATUM );
    poDS->SetMetadataItem( "DTED_HorizontalDatum", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_DIGITIZING_SYS );
    poDS->SetMetadataItem( "DTED_DigitizingSystem", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_COMPILATION_DATE );
    poDS->SetMetadataItem( "DTED_CompilationDate", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_HORIZACCURACY );
    poDS->SetMetadataItem( "DTED_HorizontalAccuracy", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_REL_HORIZACCURACY );
    poDS->SetMetadataItem( "DTED_RelHorizontalAccuracy", pszValue );
    free( pszValue );

    pszValue = DTEDGetMetadata( psDTED, DTEDMD_REL_VERTACCURACY );
    poDS->SetMetadataItem( "DTED_RelVerticalAccuracy", pszValue );
    free( pszValue );
    
    pszValue = DTEDGetMetadata( psDTED, DTEDMD_ORIGINLAT );
    poDS->SetMetadataItem( "DTED_OriginLatitude", pszValue );
    free( pszValue );
    
    pszValue = DTEDGetMetadata( psDTED, DTEDMD_ORIGINLONG );
    poDS->SetMetadataItem( "DTED_OriginLongitude", pszValue );
    free( pszValue );
    
    poDS->SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    // if no SR in xml, try aux
    const char* pszPrj = poDS->GDALPamDataset::GetProjectionRef();
    if( !pszPrj || strlen(pszPrj) == 0 )
    {
        GDALDataset* poAuxDS = GDALFindAssociatedAuxFile( poOpenInfo->pszFilename, GA_ReadOnly, poDS );
        if( poAuxDS )
        {
            pszPrj = poAuxDS->GetProjectionRef();
            if( pszPrj && strlen(pszPrj) > 0 )
            {
                CPLFree( poDS->pszProjection );
                poDS->pszProjection = CPLStrdup(pszPrj);
            }

            GDALClose( poAuxDS );
        }
    }

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
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
    // get xml and aux SR first
    const char* pszPrj = GDALPamDataset::GetProjectionRef();
    if(pszPrj && strlen(pszPrj) > 0)
        return pszPrj;

    if (pszProjection && strlen(pszProjection) > 0)
        return pszProjection;

    pszPrj = GetMetadataItem( "DTED_HorizontalDatum");
    if (EQUAL(pszPrj, "WGS84"))
    {
        return( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433],AUTHORITY[\"EPSG\",\"4326\"]]" );
    }
    else if (EQUAL(pszPrj, "WGS72"))
    {
        static int bWarned = FALSE;
        if (!bWarned)
        {
            bWarned = TRUE;
            CPLError( CE_Warning, CPLE_AppDefined,
                      "The DTED file %s indicates WGS72 as horizontal datum. \n"
                      "As this is outdated nowadays, you should contact your data producer to get data georeferenced in WGS84.\n"
                      "In some cases, WGS72 is a wrong indication and the georeferencing is really WGS84. In that case\n"
                      "you might consider doing 'gdal_translate -of DTED -mo \"DTED_HorizontalDatum=WGS84\" src.dtX dst.dtX' to\n"
                      "fix the DTED file.\n"
                      "No more warnings will be issued in this session about this operation.", GetFileName() );
        }
        return "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"WGS 72\",6378135,298.26]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433],AUTHORITY[\"EPSG\",\"4322\"]]"; 
    }
    else
    {
        static int bWarned = FALSE;
        if (!bWarned)
        {
            bWarned = TRUE;
            CPLError( CE_Warning, CPLE_AppDefined,
                      "The DTED file %s indicates %s as horizontal datum, which is not recognized by the DTED driver. \n"
                      "The DTED driver is going to consider it as WGS84.\n"
                      "No more warnings will be issued in this session about this operation.", GetFileName(), pszProjection );
        }
        return( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433],AUTHORITY[\"EPSG\",\"4326\"]]" );
    }
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
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "DTED driver does not support source dataset with zero band.\n");
        return NULL;
    }
    
    if (nBands != 1)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported, 
                  "DTED driver only uses the first band of the dataset.\n");
        if (bStrict)
            return NULL;
    }

    if( pfnProgress && !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

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
/*      Checks the input SRS                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReference ogrsr_input;
    OGRSpatialReference ogrsr_wgs84;
    char* c = (char*)poSrcDS->GetProjectionRef();
    ogrsr_input.importFromWkt(&c);
    ogrsr_wgs84.SetWellKnownGeogCS( "WGS84" );
    if ( ogrsr_input.IsSameGeogCS(&ogrsr_wgs84) == FALSE)
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "The source projection coordinate system is %s. Only WGS 84 is supported.\n"
                  "The DTED driver will generate a file as if the source was WGS 84 projection coordinate system.",
                  poSrcDS->GetProjectionRef() );
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

    if (fabs(nLLOriginLat - (adfGeoTransform[3] 
              + (poSrcDS->GetRasterYSize() - 0.5) * adfGeoTransform[5])) > 1e-10 ||
        fabs(nLLOriginLong - (adfGeoTransform[0] + 0.5 * adfGeoTransform[1])) > 1e-10)
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
               "The corner coordinates of the source are not properly "
               "aligned on plain latitude/longitude boundaries.");
    }

/* -------------------------------------------------------------------- */
/*     Check horizontal source size.                                    */
/* -------------------------------------------------------------------- */
    int expectedXSize;
    if( ABS(nLLOriginLat) >= 80 )
        expectedXSize = (poSrcDS->GetRasterYSize() - 1) / 6 + 1;
    else if( ABS(nLLOriginLat) >= 75 )
        expectedXSize = (poSrcDS->GetRasterYSize() - 1) / 4 + 1;
    else if( ABS(nLLOriginLat) >= 70 )
        expectedXSize = (poSrcDS->GetRasterYSize() - 1) / 3 + 1;
    else if( ABS(nLLOriginLat) >= 50 )
        expectedXSize = (poSrcDS->GetRasterYSize() - 1) / 2 + 1;
    else
        expectedXSize = poSrcDS->GetRasterYSize();

    if (poSrcDS->GetRasterXSize() != expectedXSize)
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
               "The horizontal source size is not conformant with the one "
               "expected by DTED Level %d at this latitude (%d pixels found instead of %d).", nLevel,
                poSrcDS->GetRasterXSize(), expectedXSize);
    }

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
/*      Read all the data in a single buffer.                           */
/* -------------------------------------------------------------------- */
    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( 1 );
    GInt16      *panData;

    panData = (GInt16 *) 
        CPLMalloc(sizeof(GInt16) * psDTED->nXSize * psDTED->nYSize);

    for( int iY = 0; iY < psDTED->nYSize; iY++ )
    {
        poSrcBand->RasterIO( GF_Read, 0, iY, psDTED->nXSize, 1, 
                            (void *) (panData + iY * psDTED->nXSize), psDTED->nXSize, 1, 
                            GDT_Int16, 0, 0 );

        if( pfnProgress && !pfnProgress(0.5 * (iY+1) / (double) psDTED->nYSize, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, 
                        "User terminated CreateCopy()" );
            DTEDClose( psDTED );
            CPLFree( panData );
            return NULL;
        }
    }

    int bSrcBandHasNoData;
    double srcBandNoData = poSrcBand->GetNoDataValue(&bSrcBandHasNoData);

/* -------------------------------------------------------------------- */
/*      Write all the profiles.                                         */
/* -------------------------------------------------------------------- */
    GInt16      anProfData[3601];
    int         dfNodataCount=0;
    GByte       iPartialCell;

    for( int iProfile = 0; iProfile < psDTED->nXSize; iProfile++ )
    {
        for( int iY = 0; iY < psDTED->nYSize; iY++ )
        {
            anProfData[iY] = panData[iProfile + iY * psDTED->nXSize];
            if ( bSrcBandHasNoData && anProfData[iY] == srcBandNoData)
            {
                anProfData[iY] = DTED_NODATA_VALUE;
                dfNodataCount++;
            }
            else if ( anProfData[iY] == DTED_NODATA_VALUE )
                dfNodataCount++;
        }
        DTEDWriteProfile( psDTED, iProfile, anProfData );

        if( pfnProgress
            && !pfnProgress( 0.5 + 0.5 * (iProfile+1) / (double) psDTED->nXSize,
                             NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, 
                      "User terminated CreateCopy()" );
            DTEDClose( psDTED );
            CPLFree( panData );
            return NULL;
        }
    }
    CPLFree( panData );

/* -------------------------------------------------------------------- */
/* Partial cell indicator: 0 for complete coverage; 1-99 for incomplete */
/* -------------------------------------------------------------------- */
    char szPartialCell[3];

    if ( dfNodataCount == 0 )
        iPartialCell = 0;
    else
    {
      iPartialCell = (GByte)int(floor(100.0 - 
           (dfNodataCount*100.0/(psDTED->nXSize * psDTED->nYSize))));
        if (iPartialCell < 1)
           iPartialCell=1;
    }
    sprintf(szPartialCell,"%02d",iPartialCell);
    strncpy((char *) (psDTED->pachDSIRecord+289), szPartialCell, 2 );

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

    if( poSrcDS->GetMetadataItem( "DTED_HorizontalDatum" ) != NULL )
        DTEDSetMetadata( psDTED, DTEDMD_HORIZDATUM, 
                         poSrcDS->GetMetadataItem( "DTED_HorizontalDatum" ) );

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

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = DTEDDataset::Open;
        poDriver->pfnCreateCopy = DTEDCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

