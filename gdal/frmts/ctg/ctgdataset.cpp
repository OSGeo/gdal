/******************************************************************************
 *
 * Project:  CTG driver
 * Purpose:  GDALDataset driver for CTG dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

constexpr int HEADER_LINE_COUNT = 5;

typedef struct
{
    int nCode;
    const char* pszDesc;
} LULCDescStruct;

static const LULCDescStruct asLULCDesc[] =
{
    {1, "Urban or Built-Up Land" },
    {2, "Agricultural Land" },
    {3, "Rangeland" },
    {4, "Forest Land" },
    {5, "Water" },
    {6, "Wetland" },
    {7, "Barren Land" },
    {8, "Tundra" },
    {9, "Perennial Snow and Ice" },
    {11, "Residential" },
    {12, "Commercial Services" },
    {13, "Industrial" },
    {14, "Transportation, Communications" },
    {15, "Industrial and Commercial" },
    {16, "Mixed Urban or Built-Up Land" },
    {17, "Other Urban or Built-Up Land" },
    {21, "Cropland and Pasture" },
    {22, "Orchards, Groves, Vineyards, Nurseries" },
    {23, "Confined Feeding Operations" },
    {24, "Other Agricultural Land" },
    {31, "Herbaceous Rangeland" },
    {32, "Shrub and Brush Rangeland" },
    {33, "Mixed Rangeland" },
    {41, "Deciduous Forest Land" },
    {42, "Evergreen Forest Land" },
    {43, "Mixed Forest Land" },
    {51, "Streams and Canals" },
    {52, "Lakes" },
    {53, "Reservoirs" },
    {54, "Bays and Estuaries" },
    {61, "Forested Wetlands" },
    {62, "Nonforested Wetlands" },
    {71, "Dry Salt Flats" },
    {72, "Beaches" },
    {73, "Sandy Areas Other than Beaches" },
    {74, "Bare Exposed Rock" },
    {75, "Strip Mines, Quarries, and Gravel Pits" },
    {76, "Transitional Areas" },
    {77, "Mixed Barren Land" },
    {81, "Shrub and Brush Tundra" },
    {82, "Herbaceous Tundra" },
    {83, "Bare Ground" },
    {84, "Wet Tundra" },
    {85, "Mixed Tundra" },
    {91, "Perennial Snowfields" },
    {92, "Glaciers" }
};

static const char* const apszBandDescription[] =
{
    "Land Use and Land Cover",
    "Political units",
    "Census county subdivisions and SMSA tracts",
    "Hydrologic units",
    "Federal land ownership",
    "State land ownership"
};

/************************************************************************/
/* ==================================================================== */
/*                              CTGDataset                              */
/* ==================================================================== */
/************************************************************************/

class CTGRasterBand;

class CTGDataset final: public GDALPamDataset
{
    friend class CTGRasterBand;

    VSILFILE   *fp;

    int         nNWEasting, nNWNorthing, nCellSize, nUTMZone;
    char       *pszProjection;

    int         bHasReadImagery;
    GByte      *pabyImage;

    int         ReadImagery();

    static const char* ExtractField(char* szOutput, const char* pszBuffer,
                                       int nOffset, int nLength);

  public:
    CTGDataset();
    ~CTGDataset() override;

    CPLErr GetGeoTransform( double * ) override;
    const char* _GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            CTGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class CTGRasterBand final: public GDALPamRasterBand
{
    friend class CTGDataset;

    char** papszCategories;

  public:

    CTGRasterBand( CTGDataset *, int );
    ~CTGRasterBand() override;

    CPLErr IReadBlock( int, int, void * ) override;
    double GetNoDataValue( int *pbSuccess = nullptr ) override;
    char **GetCategoryNames() override;
};

/************************************************************************/
/*                           CTGRasterBand()                            */
/************************************************************************/

CTGRasterBand::CTGRasterBand( CTGDataset *poDSIn, int nBandIn ) :
    papszCategories(nullptr)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Int32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = poDS->GetRasterYSize();
}

/************************************************************************/
/*                          ~CTGRasterBand()                            */
/************************************************************************/

CTGRasterBand::~CTGRasterBand()

{
    CSLDestroy(papszCategories);
}
/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr CTGRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                  CPL_UNUSED int nBlockYOff,
                                  void * pImage )
{
    CTGDataset* poGDS = (CTGDataset* ) poDS;

    poGDS->ReadImagery();
    memcpy(pImage,
           poGDS->pabyImage + (nBand - 1) * nBlockXSize * nBlockYSize * sizeof(int),
           nBlockXSize * nBlockYSize * sizeof(int));

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double CTGRasterBand::GetNoDataValue( int *pbSuccess )
{
    if (pbSuccess)
        *pbSuccess = TRUE;

    return 0.0;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **CTGRasterBand::GetCategoryNames()
{
    if (nBand != 1)
        return nullptr;

    if (papszCategories != nullptr)
        return papszCategories;

    int nasLULCDescSize = (int)(sizeof(asLULCDesc) / sizeof(asLULCDesc[0]));
    int nCategoriesSize = asLULCDesc[nasLULCDescSize - 1].nCode;
    papszCategories = (char**)CPLCalloc(nCategoriesSize + 2, sizeof(char*));
    for(int i=0;i<nasLULCDescSize;i++)
    {
        papszCategories[asLULCDesc[i].nCode] = CPLStrdup(asLULCDesc[i].pszDesc);
    }
    for(int i=0;i<nCategoriesSize;i++)
    {
        if (papszCategories[i] == nullptr)
            papszCategories[i] = CPLStrdup("");
    }
    papszCategories[nCategoriesSize + 1] = nullptr;

    return papszCategories;
}

/************************************************************************/
/*                            ~CTGDataset()                            */
/************************************************************************/

CTGDataset::CTGDataset() :
    fp(nullptr),
    nNWEasting(0),
    nNWNorthing(0),
    nCellSize(0),
    nUTMZone(0),
    pszProjection(nullptr),
    bHasReadImagery(FALSE),
    pabyImage(nullptr)
{}

/************************************************************************/
/*                            ~CTGDataset()                            */
/************************************************************************/

CTGDataset::~CTGDataset()

{
    CPLFree(pszProjection);
    CPLFree(pabyImage);
    if( fp != nullptr )
        VSIFCloseL(fp);
}

/************************************************************************/
/*                              ExtractField()                          */
/************************************************************************/

const char* CTGDataset::ExtractField(char* szField, const char* pszBuffer,
                                     int nOffset, int nLength)
{
    CPLAssert(nLength <= 10);
    memcpy(szField, pszBuffer + nOffset, nLength);
    szField[nLength] = 0;
    return szField;
}

/************************************************************************/
/*                            ReadImagery()                             */
/************************************************************************/

int CTGDataset::ReadImagery()
{
    if (bHasReadImagery)
        return TRUE;

    bHasReadImagery = TRUE;

    char szLine[81];
    char szField[11];
    szLine[80] = 0;
    int nLine = HEADER_LINE_COUNT;
    VSIFSeekL(fp, nLine * 80, SEEK_SET);
    int nCells = nRasterXSize * nRasterYSize;
    while(VSIFReadL(szLine, 1, 80, fp) == 80)
    {
        int nZone = atoi(ExtractField(szField, szLine, 0, 3));
        if (nZone != nUTMZone)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Read error at line %d, %s. Did not expected UTM zone %d",
                     nLine, szLine, nZone);
            return FALSE;
        }
        int nX = atoi(ExtractField(szField, szLine, 3, 8)) - nCellSize / 2;
        int nY = atoi(ExtractField(szField, szLine, 11, 8)) + nCellSize / 2;
        GIntBig nDiffX = static_cast<GIntBig>(nX) - nNWEasting;
        GIntBig nDiffY = static_cast<GIntBig>(nNWNorthing) - nY;
        if (nDiffX < 0 || (nDiffX % nCellSize) != 0 ||
            nDiffY < 0 || (nDiffY % nCellSize) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Read error at line %d, %s. Unexpected cell coordinates",
                     nLine, szLine);
            return FALSE;
        }
        GIntBig nCellX = nDiffX / nCellSize;
        GIntBig nCellY = nDiffY / nCellSize;
        if (nCellX >= nRasterXSize || nCellY >= nRasterYSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Read error at line %d, %s. Unexpected cell coordinates",
                     nLine, szLine);
            return FALSE;
        }
        for(int i=0;i<6;i++)
        {
            int nVal = atoi(ExtractField(szField, szLine, 20 + 10*i, 10));
            if (nVal >= 2000000000)
                nVal = 0;
            ((int*)pabyImage)[i * nCells + nCellY * nRasterXSize + nCellX] = nVal;
        }

        nLine ++;
    }

    return TRUE;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int CTGDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    CPLString osFilename; // let in that scope

    GDALOpenInfo* poOpenInfoToDelete = nullptr;
    /*  GZipped grid_cell.gz files are common, so automagically open them */
    /*  if the /vsigzip/ has not been explicitly passed */
    const char* pszFilename = CPLGetFilename(poOpenInfo->pszFilename);
    if ((EQUAL(pszFilename, "grid_cell.gz") ||
         EQUAL(pszFilename, "grid_cell1.gz") ||
         EQUAL(pszFilename, "grid_cell2.gz")) &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsigzip/"))
    {
        osFilename = "/vsigzip/";
        osFilename += poOpenInfo->pszFilename;
        poOpenInfo = poOpenInfoToDelete =
                new GDALOpenInfo(osFilename.c_str(), GA_ReadOnly,
                                 poOpenInfo->GetSiblingFiles());
    }

    if (poOpenInfo->nHeaderBytes < HEADER_LINE_COUNT * 80)
    {
        delete poOpenInfoToDelete;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Check that it looks roughly as a CTG dataset                    */
/* -------------------------------------------------------------------- */
    const char* pszData = (const char*)poOpenInfo->pabyHeader;
    for(int i=0;i<4 * 80;i++)
    {
        if (!((pszData[i] >= '0' && pszData[i] <= '9') ||
              pszData[i] == ' ' || pszData[i] == '-'))
        {
            delete poOpenInfoToDelete;
            return FALSE;
        }
    }

    char szField[11];
    int nRows = atoi(ExtractField(szField, pszData, 0, 10));
    int nCols = atoi(ExtractField(szField, pszData, 20, 10));
    int nMinColIndex = atoi(ExtractField(szField, pszData+80, 0, 5));
    int nMinRowIndex = atoi(ExtractField(szField, pszData+80, 5, 5));
    int nMaxColIndex = atoi(ExtractField(szField, pszData+80, 10, 5));
    int nMaxRowIndex = atoi(ExtractField(szField, pszData+80, 15, 5));

    if (nRows <= 0 || nCols <= 0 ||
        nMinColIndex != 1 || nMinRowIndex != 1 ||
        nMaxRowIndex != nRows || nMaxColIndex != nCols)
    {
        delete poOpenInfoToDelete;
        return FALSE;
    }

    delete poOpenInfoToDelete;
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *CTGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo))
        return nullptr;

    CPLString osFilename(poOpenInfo->pszFilename);

    /*  GZipped grid_cell.gz files are common, so automagically open them */
    /*  if the /vsigzip/ has not been explicitly passed */
    const char* pszFilename = CPLGetFilename(poOpenInfo->pszFilename);
    if ((EQUAL(pszFilename, "grid_cell.gz") ||
         EQUAL(pszFilename, "grid_cell1.gz") ||
         EQUAL(pszFilename, "grid_cell2.gz")) &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsigzip/"))
    {
        osFilename = "/vsigzip/";
        osFilename += poOpenInfo->pszFilename;
    }

    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The CTG driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Find dataset characteristics                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "rb");
    if (fp == nullptr)
        return nullptr;

    char szHeader[HEADER_LINE_COUNT * 80+1];
    szHeader[HEADER_LINE_COUNT * 80] = 0;
    if (VSIFReadL(szHeader, 1, HEADER_LINE_COUNT * 80, fp) != HEADER_LINE_COUNT * 80)
    {
        VSIFCloseL(fp);
        return nullptr;
    }

    for(int i=HEADER_LINE_COUNT * 80 - 1;i>=0;i--)
    {
        if (szHeader[i] == ' ')
            szHeader[i] = 0;
        else
            break;
    }

    char szField[11];
    int nRows = atoi(ExtractField(szField, szHeader, 0, 10));
    int nCols = atoi(ExtractField(szField, szHeader, 20, 10));

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    CTGDataset *poDS = new CTGDataset();
    poDS->fp = fp;
    fp = nullptr;
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

    poDS->SetMetadataItem("TITLE", szHeader + 4 * 80);

    poDS->nCellSize = atoi(ExtractField(szField, szHeader, 35, 5));
    if (poDS->nCellSize <= 0 || poDS->nCellSize >= 10000)
    {
        delete poDS;
        return nullptr;
    }
    poDS->nNWEasting = atoi(ExtractField(szField, szHeader + 3*80, 40, 10));
    poDS->nNWNorthing = atoi(ExtractField(szField, szHeader + 3*80, 50, 10));
    poDS->nUTMZone = atoi(ExtractField(szField, szHeader, 50, 5));
    if (poDS->nUTMZone <= 0 || poDS->nUTMZone > 60)
    {
        delete poDS;
        return nullptr;
    }

    OGRSpatialReference oSRS;
    oSRS.importFromEPSG(32600 + poDS->nUTMZone);
    oSRS.exportToWkt(&poDS->pszProjection);

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Read the imagery                                                */
/* -------------------------------------------------------------------- */
    GByte* pabyImage = (GByte*)VSICalloc(nCols * nRows, 6 * sizeof(int));
    if (pabyImage == nullptr)
    {
        delete poDS;
        return nullptr;
    }
    poDS->pabyImage = pabyImage;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 6;
    for( int i = 0; i < poDS->nBands; i++ )
    {
        poDS->SetBand( i+1, new CTGRasterBand( poDS, i+1 ) );
        poDS->GetRasterBand(i+1)->SetDescription(apszBandDescription[i]);
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr CTGDataset::GetGeoTransform( double * padfTransform )

{
    padfTransform[0] = static_cast<double>(nNWEasting) - nCellSize / 2;
    padfTransform[1] = nCellSize;
    padfTransform[2] = 0;
    padfTransform[3] = static_cast<double>(nNWNorthing) + nCellSize / 2;
    padfTransform[4] = 0.;
    padfTransform[5] = -nCellSize;

    return CE_None;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* CTGDataset::_GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                         GDALRegister_CTG()                           */
/************************************************************************/

void GDALRegister_CTG()

{
    if( GDALGetDriverByName( "CTG" ) != nullptr )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "CTG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "USGS LULC Composite Theme Grid" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/ctg.html" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = CTGDataset::Open;
    poDriver->pfnIdentify = CTGDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
