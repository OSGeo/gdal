/******************************************************************************
 *
 * Project:  SNODAS driver
 * Purpose:  Implementation of SNODASDataset
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_srs_api.h"
#include "rawdataset.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                            SNODASDataset                             */
/* ==================================================================== */
/************************************************************************/

class SNODASRasterBand;

class SNODASDataset : public RawDataset
{
    CPLString   osDataFilename;
    bool        bGotTransform;
    double      adfGeoTransform[6];
    bool        bHasNoData;
    double      dfNoData;
    bool        bHasMin;
    double      dfMin;
    int         bHasMax;
    double      dfMax;

    friend class SNODASRasterBand;

  public:
                    SNODASDataset();
    virtual ~SNODASDataset();

    virtual CPLErr GetGeoTransform( double * padfTransform ) override;
    virtual const char *GetProjectionRef(void) override;

    virtual char **GetFileList() override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            SNODASRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class SNODASRasterBand : public RawRasterBand
{
  public:
            SNODASRasterBand( VSILFILE* fpRaw, int nXSize, int nYSize );
    virtual ~SNODASRasterBand() {}

    virtual double GetNoDataValue( int *pbSuccess = NULL ) override;
    virtual double GetMinimum( int *pbSuccess = NULL ) override;
    virtual double GetMaximum(int *pbSuccess = NULL ) override;
};

/************************************************************************/
/*                         SNODASRasterBand()                           */
/************************************************************************/

SNODASRasterBand::SNODASRasterBand( VSILFILE* fpRawIn,
                                    int nXSize, int nYSize ) :
    RawRasterBand( fpRawIn, 0, 2,
                   nXSize * 2, GDT_Int16,
                   !CPL_IS_LSB, nXSize, nYSize, TRUE, TRUE)
{}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

double SNODASRasterBand::GetNoDataValue( int *pbSuccess )
{
    SNODASDataset* poGDS = reinterpret_cast<SNODASDataset *>( poDS );
    if( pbSuccess )
        *pbSuccess = poGDS->bHasNoData;

    if( poGDS->bHasNoData )
        return poGDS->dfNoData;

    return RawRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                            GetMinimum()                              */
/************************************************************************/

double SNODASRasterBand::GetMinimum( int *pbSuccess )
{
    SNODASDataset* poGDS = reinterpret_cast<SNODASDataset *>( poDS );
    if( pbSuccess )
        *pbSuccess = poGDS->bHasMin;

    if( poGDS->bHasMin )
        return poGDS->dfMin;

    return RawRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                            GetMaximum()                             */
/************************************************************************/

double SNODASRasterBand::GetMaximum( int *pbSuccess )
{
    SNODASDataset* poGDS = reinterpret_cast<SNODASDataset *>( poDS );
    if( pbSuccess )
        *pbSuccess = poGDS->bHasMax;

    if( poGDS->bHasMax )
        return poGDS->dfMax;

    return RawRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/* ==================================================================== */
/*                            SNODASDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SNODASDataset()                            */
/************************************************************************/

SNODASDataset::SNODASDataset() :
    bGotTransform(false),
    bHasNoData(false),
    dfNoData(0.0),
    bHasMin(false),
    dfMin(0.0),
    bHasMax(false),
    dfMax(0.0)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~SNODASDataset()                           */
/************************************************************************/

SNODASDataset::~SNODASDataset()

{
    FlushCache();
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *SNODASDataset::GetProjectionRef()

{
    return SRS_WKT_WGS84;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SNODASDataset::GetGeoTransform( double * padfTransform )

{
    if( bGotTransform )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **SNODASDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    papszFileList = CSLAddString(papszFileList, osDataFilename);

    return papszFileList;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int SNODASDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if (poOpenInfo->nHeaderBytes == 0)
        return FALSE;

    return STARTS_WITH_CI(
        reinterpret_cast<char *>(poOpenInfo->pabyHeader),
        "Format version: NOHRSC GIS/RS raster file v1.1" );}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SNODASDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify(poOpenInfo) )
        return NULL;

    VSILFILE *fp = VSIFOpenL( poOpenInfo->pszFilename, "r" );
    if( fp == NULL )
    {
        return NULL;
    }

    int nRows = -1;
    int nCols = -1;
    CPLString       osDataFilename;
    bool bIsInteger = false;
    bool bIs2Bytes = false;
    double          dfNoData = 0;
    bool bHasNoData = false;
    double          dfMin = 0;
    bool bHasMin = false;
    double          dfMax = 0;
    bool bHasMax = false;
    double dfMinX = 0.0;
    double dfMinY = 0.0;
    double dfMaxX = 0.0;
    double dfMaxY = 0.0;
    bool bHasMinX = false;
    bool bHasMinY = false;
    bool bHasMaxX = false;
    bool bHasMaxY = false;
    bool bNotProjected = false;
    bool bIsWGS84 = false;
    CPLString       osDataUnits;
    CPLString       osDescription;
    int nStartYear = -1;
    int nStartMonth = -1;
    int nStartDay = -1;
    int nStartHour = -1;
    int nStartMinute = -1;
    int nStartSecond = -1;
    int nStopYear = -1;
    int nStopMonth = -1;
    int nStopDay = -1;
    int nStopHour = -1;
    int nStopMinute = -1;
    int nStopSecond = -1;

    const char *pszLine = NULL;
    while( (pszLine = CPLReadLine2L( fp, 256, NULL )) != NULL )
    {
        char** papszTokens =
            CSLTokenizeStringComplex( pszLine, ":", TRUE, FALSE );
        if( CSLCount( papszTokens ) != 2 )
        {
            CSLDestroy( papszTokens );
            continue;
        }
        if( papszTokens[1][0] == ' ' )
            memmove( papszTokens[1], papszTokens[1] + 1,
                     strlen(papszTokens[1] + 1) + 1);

        if( EQUAL(papszTokens[0], "Data file pathname") )
        {
            osDataFilename = papszTokens[1];
        }
        else if( EQUAL(papszTokens[0], "Description") )
        {
            osDescription = papszTokens[1];
        }
        else if( EQUAL(papszTokens[0], "Data units") )
        {
            osDataUnits= papszTokens[1];
        }

        else if( EQUAL(papszTokens[0],"Start year") )
            nStartYear = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Start month") )
            nStartMonth = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Start day") )
            nStartDay = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0], "Start hour") )
            nStartHour = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0]," Start minute") )
            nStartMinute = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0], "Start second") )
            nStartSecond = atoi(papszTokens[1]);

        else if( EQUAL(papszTokens[0], "Stop year") )
            nStopYear = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0], "Stop month") )
            nStopMonth = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0], "Stop day") )
            nStopDay = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0], "Stop hour") )
            nStopHour = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0], "Stop minute") )
            nStopMinute = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0], "Stop second") )
            nStopSecond = atoi(papszTokens[1]);

        else if( EQUAL(papszTokens[0], "Number of columns") )
        {
            nCols = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "Number of rows") )
        {
            nRows = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "Data type") )
        {
            bIsInteger = EQUAL(papszTokens[1],"integer");
        }
        else if( EQUAL(papszTokens[0], "Data bytes per pixel") )
        {
            bIs2Bytes = EQUAL(papszTokens[1],"2");
        }
        else if( EQUAL(papszTokens[0], "Projected") )
        {
            bNotProjected = EQUAL(papszTokens[1],"no");
        }
        else if( EQUAL(papszTokens[0], "Horizontal datum") )
        {
            bIsWGS84 = EQUAL(papszTokens[1],"WGS84");
        }
        else if( EQUAL(papszTokens[0], "No data value") )
        {
            bHasNoData = true;
            dfNoData = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"Minimum data value") )
        {
            bHasMin = true;
            dfMin = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "Maximum data value") )
        {
            bHasMax = true;
            dfMax = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "Minimum x-axis coordinate") )
        {
            bHasMinX = true;
            dfMinX = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "Minimum y-axis coordinate") )
        {
            bHasMinY = true;
            dfMinY = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "Maximum x-axis coordinate") )
        {
            bHasMaxX = true;
            dfMaxX = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "Maximum y-axis coordinate") )
        {
            bHasMaxY = true;
            dfMaxY = CPLAtofM(papszTokens[1]);
        }

        CSLDestroy( papszTokens );
    }

    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( nRows == -1 || nCols == -1 || !bIsInteger || !bIs2Bytes )
        return NULL;

    if( !bNotProjected || !bIsWGS84 )
        return NULL;

    if( osDataFilename.empty() )
        return NULL;

    if( !GDALCheckDatasetDimensions(nCols, nRows) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open target binary file.                                        */
/* -------------------------------------------------------------------- */
    const char* pszPath = CPLGetPath(poOpenInfo->pszFilename);
    osDataFilename = CPLFormFilename(pszPath, osDataFilename, NULL);

    VSILFILE* fpRaw = VSIFOpenL( osDataFilename, "rb" );

    if( fpRaw == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    SNODASDataset *poDS = new SNODASDataset();

    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->osDataFilename = osDataFilename;
    poDS->bHasNoData = bHasNoData;
    poDS->dfNoData = dfNoData;
    poDS->bHasMin = bHasMin;
    poDS->dfMin = dfMin;
    poDS->bHasMax = bHasMax;
    poDS->dfMax = dfMax;
    if( bHasMinX && bHasMinY && bHasMaxX && bHasMaxY )
    {
        poDS->bGotTransform = true;
        poDS->adfGeoTransform[0] = dfMinX;
        poDS->adfGeoTransform[1] = (dfMaxX - dfMinX) / nCols;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = dfMaxY;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - (dfMaxY - dfMinY) / nRows;
    }

    if( !osDescription.empty() )
        poDS->SetMetadataItem("Description", osDescription);
    if( !osDataUnits.empty() )
        poDS->SetMetadataItem("Data_Units", osDataUnits);
    if( nStartYear != -1 && nStartMonth != -1 && nStartDay != -1 &&
        nStartHour != -1 && nStartMinute != -1 && nStartSecond != -1 )
        poDS->SetMetadataItem("Start_Date",
                              CPLSPrintf("%04d/%02d/%02d %02d:%02d:%02d",
                                        nStartYear, nStartMonth, nStartDay,
                                        nStartHour, nStartMinute, nStartSecond));
    if( nStopYear != -1 && nStopMonth != -1 && nStopDay != -1 &&
        nStopHour != -1 && nStopMinute != -1 && nStopSecond != -1 )
        poDS->SetMetadataItem("Stop_Date",
                              CPLSPrintf("%04d/%02d/%02d %02d:%02d:%02d",
                                        nStopYear, nStopMonth, nStopDay,
                                        nStopHour, nStopMinute, nStopSecond));

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new SNODASRasterBand( fpRaw, nCols, nRows) );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                       GDALRegister_SNODAS()                          */
/************************************************************************/

void GDALRegister_SNODAS()

{
    if( GDALGetDriverByName( "SNODAS" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SNODAS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Snow Data Assimilation System" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_various.html#SNODAS" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "hdr" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = SNODASDataset::Open;
    poDriver->pfnIdentify = SNODASDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
