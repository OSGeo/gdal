/******************************************************************************
 * $Id$
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

#include "rawdataset.h"
#include "ogr_srs_api.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

// g++ -g -Wall -fPIC frmts/raw/snodasdataset.cpp -shared -o gdal_SNODAS.so -Iport -Igcore -Ifrmts/raw -Iogr -L. -lgdal

CPL_C_START
void    GDALRegister_SNODAS(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                            SNODASDataset                             */
/* ==================================================================== */
/************************************************************************/

class SNODASRasterBand;

class SNODASDataset : public RawDataset
{
    CPLString   osDataFilename;
    int         bGotTransform;
    double      adfGeoTransform[6];
    int         bHasNoData;
    double      dfNoData;
    int         bHasMin;
    double      dfMin;
    int         bHasMax;
    double      dfMax;

    friend class SNODASRasterBand;

  public:
                    SNODASDataset();
                    ~SNODASDataset();

    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef(void);

    virtual char **GetFileList();

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
            SNODASRasterBand(VSILFILE* fpRaw, int nXSize, int nYSize);

        virtual double GetNoDataValue( int *pbSuccess = NULL );
        virtual double GetMinimum( int *pbSuccess = NULL );
        virtual double GetMaximum(int *pbSuccess = NULL );
};


/************************************************************************/
/*                         SNODASRasterBand()                           */
/************************************************************************/

SNODASRasterBand::SNODASRasterBand(VSILFILE* fpRaw,
                                   int nXSize, int nYSize) :
    RawRasterBand( fpRaw, 0, 2,
                   nXSize * 2, GDT_Int16,
                   !CPL_IS_LSB, nXSize, nYSize, TRUE, TRUE)
{
}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

double SNODASRasterBand::GetNoDataValue( int *pbSuccess )
{
    SNODASDataset* poGDS = (SNODASDataset*) poDS;
    if (pbSuccess)
        *pbSuccess = poGDS->bHasNoData;
    if (poGDS->bHasNoData)
        return poGDS->dfNoData;
    else
        return RawRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                            GetMinimum()                              */
/************************************************************************/

double SNODASRasterBand::GetMinimum( int *pbSuccess )
{
    SNODASDataset* poGDS = (SNODASDataset*) poDS;
    if (pbSuccess)
        *pbSuccess = poGDS->bHasMin;
    if (poGDS->bHasMin)
        return poGDS->dfMin;
    else
        return RawRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                            GetMaximum()                             */
/************************************************************************/

double SNODASRasterBand::GetMaximum( int *pbSuccess )
{
    SNODASDataset* poGDS = (SNODASDataset*) poDS;
    if (pbSuccess)
        *pbSuccess = poGDS->bHasMax;
    if (poGDS->bHasMax)
        return poGDS->dfMax;
    else
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

SNODASDataset::SNODASDataset()
{
    bGotTransform = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    bHasNoData = FALSE;
    dfNoData = 0.0;
    bHasMin = FALSE;
    dfMin = 0.0;
    bHasMax = FALSE;
    dfMax = 0.0;
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
    else
    {
        return GDALPamDataset::GetGeoTransform( padfTransform );
    }
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

    return EQUALN((const char*)poOpenInfo->pabyHeader,
                  "Format version: NOHRSC GIS/RS raster file v1.1",
                  strlen("Format version: NOHRSC GIS/RS raster file v1.1"));
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SNODASDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify(poOpenInfo) )
        return NULL;

    VSILFILE    *fp;

    fp = VSIFOpenL( poOpenInfo->pszFilename, "r" );

    if( fp == NULL )
    {
        return NULL;
    }

    const char *    pszLine;
    int             nRows = -1, nCols = -1;
    CPLString       osDataFilename;
    int             bIsInteger = FALSE, bIs2Bytes = FALSE;
    double          dfNoData = 0;
    int             bHasNoData = FALSE;
    double          dfMin = 0;
    int             bHasMin = FALSE;
    double          dfMax = 0;
    int             bHasMax = FALSE;
    double          dfMinX = 0.0, dfMinY = 0.0, dfMaxX = 0.0, dfMaxY = 0.0;
    int             bHasMinX = FALSE, bHasMinY = FALSE, bHasMaxX = FALSE, bHasMaxY = FALSE;
    int             bNotProjected = FALSE, bIsWGS84 = FALSE;
    CPLString       osDescription, osDataUnits;
    int             nStartYear = -1, nStartMonth = -1, nStartDay = -1,
                    nStartHour = -1, nStartMinute = -1, nStartSecond = -1;
    int             nStopYear = -1, nStopMonth = -1, nStopDay = -1,
                    nStopHour = -1, nStopMinute = -1, nStopSecond = -1;

    while( (pszLine = CPLReadLine2L( fp, 256, NULL )) != NULL )
    {
        char** papszTokens = CSLTokenizeStringComplex( pszLine, ":", TRUE, FALSE );
        if( CSLCount( papszTokens ) != 2 )
        {
            CSLDestroy( papszTokens );
            continue;
        }
        if( papszTokens[1][0] == ' ' )
            memmove(papszTokens[1], papszTokens[1] + 1, strlen(papszTokens[1] + 1) + 1);

        if( EQUAL(papszTokens[0],"Data file pathname") )
        {
            osDataFilename = papszTokens[1];
        }
        else if( EQUAL(papszTokens[0],"Description") )
        {
            osDescription = papszTokens[1];
        }
        else if( EQUAL(papszTokens[0],"Data units") )
        {
            osDataUnits= papszTokens[1];
        }

        else if( EQUAL(papszTokens[0],"Start year") )
            nStartYear = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Start month") )
            nStartMonth = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Start day") )
            nStartDay = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Start hour") )
            nStartHour = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Start minute") )
            nStartMinute = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Start second") )
            nStartSecond = atoi(papszTokens[1]);

        else if( EQUAL(papszTokens[0],"Stop year") )
            nStopYear = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Stop month") )
            nStopMonth = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Stop day") )
            nStopDay = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Stop hour") )
            nStopHour = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Stop minute") )
            nStopMinute = atoi(papszTokens[1]);
        else if( EQUAL(papszTokens[0],"Stop second") )
            nStopSecond = atoi(papszTokens[1]);

        else if( EQUAL(papszTokens[0],"Number of columns") )
        {
            nCols = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"Number of rows") )
        {
            nRows = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"Data type"))
        {
            bIsInteger = EQUAL(papszTokens[1],"integer");
        }
        else if( EQUAL(papszTokens[0],"Data bytes per pixel"))
        {
            bIs2Bytes = EQUAL(papszTokens[1],"2");
        }
        else if( EQUAL(papszTokens[0],"Projected"))
        {
            bNotProjected = EQUAL(papszTokens[1],"no");
        }
        else if( EQUAL(papszTokens[0],"Horizontal datum"))
        {
            bIsWGS84 = EQUAL(papszTokens[1],"WGS84");
        }
        else if( EQUAL(papszTokens[0],"No data value"))
        {
            bHasNoData = TRUE;
            dfNoData = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"Minimum data value"))
        {
            bHasMin = TRUE;
            dfMin = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"Maximum data value"))
        {
            bHasMax = TRUE;
            dfMax = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"Minimum x-axis coordinate") )
        {
            bHasMinX = TRUE;
            dfMinX = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"Minimum y-axis coordinate") )
        {
            bHasMinY = TRUE;
            dfMinY = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"Maximum x-axis coordinate") )
        {
            bHasMaxX = TRUE;
            dfMaxX = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"Maximum y-axis coordinate") )
        {
            bHasMaxY = TRUE;
            dfMaxY = CPLAtofM(papszTokens[1]);
        }

        CSLDestroy( papszTokens );
    }

    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( nRows == -1 || nCols == -1 || !bIsInteger || !bIs2Bytes )
        return NULL;

    if( !bNotProjected || !bIsWGS84 )
        return NULL;

    if( osDataFilename.size() == 0 )
        return NULL;

    if (!GDALCheckDatasetDimensions(nCols, nRows))
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
    SNODASDataset     *poDS;

    poDS = new SNODASDataset();

    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->osDataFilename = osDataFilename;
    poDS->bHasNoData = bHasNoData;
    poDS->dfNoData = dfNoData;
    poDS->bHasMin = bHasMin;
    poDS->dfMin = dfMin;
    poDS->bHasMax = bHasMax;
    poDS->dfMax = dfMax;
    if (bHasMinX && bHasMinY && bHasMaxX && bHasMaxY)
    {
        poDS->bGotTransform = TRUE;
        poDS->adfGeoTransform[0] = dfMinX;
        poDS->adfGeoTransform[1] = (dfMaxX - dfMinX) / nCols;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = dfMaxY;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - (dfMaxY - dfMinY) / nRows;
    }

    if (osDescription.size())
        poDS->SetMetadataItem("Description", osDescription);
    if (osDataUnits.size())
        poDS->SetMetadataItem("Data_Units", osDataUnits);
    if (nStartYear != -1 && nStartMonth != -1 && nStartDay != -1 &&
        nStartHour != -1 && nStartMinute != -1 && nStartSecond != -1)
        poDS->SetMetadataItem("Start_Date",
                              CPLSPrintf("%04d/%02d/%02d %02d:%02d:%02d",
                                        nStartYear, nStartMonth, nStartDay,
                                        nStartHour, nStartMinute, nStartSecond));
    if (nStopYear != -1 && nStopMonth != -1 && nStopDay != -1 &&
        nStopHour != -1 && nStopMinute != -1 && nStopSecond != -1)
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
   
    return( poDS );
}

/************************************************************************/
/*                       GDALRegister_SNODAS()                          */
/************************************************************************/

void GDALRegister_SNODAS()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "SNODAS" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "SNODAS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Snow Data Assimilation System" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#SNODAS" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "hdr" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->pfnOpen = SNODASDataset::Open;
        poDriver->pfnIdentify = SNODASDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
