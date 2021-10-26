/******************************************************************************
 *
 * Project:  NGSGEOID driver
 * Purpose:  GDALDataset driver for NGSGEOID dataset.
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

#include "cpl_string.h"
#include "cpl_vsi_virtual.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

#define HEADER_SIZE (4 * 8 + 3 * 4)

/************************************************************************/
/* ==================================================================== */
/*                            NGSGEOIDDataset                           */
/* ==================================================================== */
/************************************************************************/

class NGSGEOIDRasterBand;

class NGSGEOIDDataset final: public GDALPamDataset
{
    friend class NGSGEOIDRasterBand;

    VSILFILE   *fp;
    double      adfGeoTransform[6];
    int         bIsLittleEndian;
    CPLString   osProjection{};

    static int   GetHeaderInfo( const GByte* pBuffer,
                                double* padfGeoTransform,
                                int* pnRows,
                                int* pnCols,
                                int* pbIsLittleEndian );

  public:
                 NGSGEOIDDataset();
    virtual     ~NGSGEOIDDataset();

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual const char* _GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                          NGSGEOIDRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class NGSGEOIDRasterBand final: public GDALPamRasterBand
{
    friend class NGSGEOIDDataset;

  public:
                explicit NGSGEOIDRasterBand( NGSGEOIDDataset * );

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual const char* GetUnitType() override { return "m"; }
};

/************************************************************************/
/*                        NGSGEOIDRasterBand()                          */
/************************************************************************/

NGSGEOIDRasterBand::NGSGEOIDRasterBand( NGSGEOIDDataset *poDSIn )

{
    poDS = poDSIn;
    nBand = 1;

    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr NGSGEOIDRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                       int nBlockYOff,
                                       void * pImage )

{
    NGSGEOIDDataset *poGDS = reinterpret_cast<NGSGEOIDDataset *>( poDS );

    /* First values in the file corresponds to the south-most line of the imagery */
    VSIFSeekL(poGDS->fp,
              HEADER_SIZE + static_cast<vsi_l_offset>(nRasterYSize - 1 - nBlockYOff) * nRasterXSize * 4,
              SEEK_SET);

    if (static_cast<int>(
            VSIFReadL(pImage, 4, nRasterXSize, poGDS->fp)) != nRasterXSize )
        return CE_Failure;

#ifdef CPL_MSB
    if (poGDS->bIsLittleEndian)
    {
        GDALSwapWords( pImage, 4, nRasterXSize, 4 );
    }
#endif

#ifdef CPL_LSB
    if ( !poGDS->bIsLittleEndian )
    {
        GDALSwapWords( pImage, 4, nRasterXSize, 4 );
    }
#endif

    return CE_None;
}

/************************************************************************/
/*                          ~NGSGEOIDDataset()                          */
/************************************************************************/

NGSGEOIDDataset::NGSGEOIDDataset() :
    fp(nullptr),
    bIsLittleEndian(TRUE)
{
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;
}

/************************************************************************/
/*                           ~NGSGEOIDDataset()                         */
/************************************************************************/

NGSGEOIDDataset::~NGSGEOIDDataset()

{
    FlushCache(true);
    if (fp)
        VSIFCloseL(fp);
}

/************************************************************************/
/*                            GetHeaderInfo()                           */
/************************************************************************/

int NGSGEOIDDataset::GetHeaderInfo( const GByte* pBuffer,
                                    double* padfGeoTransform,
                                    int* pnRows,
                                    int* pnCols,
                                    int* pbIsLittleEndian )
{
    /* First check IKIND marker to determine if the file */
    /* is in little or big-endian order, and if it is a valid */
    /* NGSGEOID dataset */
    int nIKIND;
    memcpy(&nIKIND, pBuffer + HEADER_SIZE - 4, 4);
    CPL_LSBPTR32(&nIKIND);
    if (nIKIND == 1)
    {
        *pbIsLittleEndian = TRUE;
    }
    else
    {
        memcpy(&nIKIND, pBuffer + HEADER_SIZE - 4, 4);
        CPL_MSBPTR32(&nIKIND);
        if (nIKIND == 1)
        {
            *pbIsLittleEndian = FALSE;
        }
        else
        {
            return FALSE;
        }
    }

    double dfSLAT;
    memcpy(&dfSLAT, pBuffer, 8);
    if (*pbIsLittleEndian)
    {
        CPL_LSBPTR64(&dfSLAT);
    }
    else
    {
        CPL_MSBPTR64(&dfSLAT);
    }
    pBuffer += 8;

    double dfWLON;
    memcpy(&dfWLON, pBuffer, 8);
    if (*pbIsLittleEndian)
    {
        CPL_LSBPTR64(&dfWLON);
    }
    else
    {
        CPL_MSBPTR64(&dfWLON);
    }
    pBuffer += 8;

    double dfDLAT;
    memcpy(&dfDLAT, pBuffer, 8);
    if (*pbIsLittleEndian)
    {
        CPL_LSBPTR64(&dfDLAT);
    }
    else
    {
        CPL_MSBPTR64(&dfDLAT);
    }
    pBuffer += 8;

    double dfDLON;
    memcpy(&dfDLON, pBuffer, 8);
    if (*pbIsLittleEndian)
    {
        CPL_LSBPTR64(&dfDLON);
    }
    else
    {
        CPL_MSBPTR64(&dfDLON);
    }
    pBuffer += 8;

    int nNLAT;
    memcpy(&nNLAT, pBuffer, 4);
    if (*pbIsLittleEndian)
    {
        CPL_LSBPTR32(&nNLAT);
    }
    else
    {
        CPL_MSBPTR32(&nNLAT);
    }
    pBuffer += 4;

    int nNLON;
    memcpy(&nNLON, pBuffer, 4);
    if (*pbIsLittleEndian)
    {
        CPL_LSBPTR32(&nNLON);
    }
    else
    {
        CPL_MSBPTR32(&nNLON);
    }
    /*pBuffer += 4;*/

    /*CPLDebug("NGSGEOID", "SLAT=%f, WLON=%f, DLAT=%f, DLON=%f, NLAT=%d, NLON=%d, IKIND=%d",
             dfSLAT, dfWLON, dfDLAT, dfDLON, nNLAT, nNLON, nIKIND);*/

    if (nNLAT <= 0 || nNLON <= 0 || dfDLAT <= 1e-15 || dfDLON <= 1e-15)
        return FALSE;

    /* Grids go over +180 in longitude */
    if (dfSLAT < -90.0 || dfSLAT + nNLAT * dfDLAT > 90.0 ||
        dfWLON < -180.0 || dfWLON + nNLON * dfDLON > 360.0)
        return FALSE;

    padfGeoTransform[0] = dfWLON - dfDLON / 2;
    padfGeoTransform[1] = dfDLON;
    padfGeoTransform[2] = 0.0;
    padfGeoTransform[3] = dfSLAT + nNLAT * dfDLAT - dfDLAT / 2;
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[5] = -dfDLAT;

    *pnRows = nNLAT;
    *pnCols = nNLON;

    return TRUE;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int NGSGEOIDDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if (poOpenInfo->nHeaderBytes < HEADER_SIZE)
        return FALSE;

    double adfGeoTransform[6];
    int nRows, nCols;
    int bIsLittleEndian;
    if ( !GetHeaderInfo( poOpenInfo->pabyHeader,
                         adfGeoTransform,
                         &nRows, &nCols, &bIsLittleEndian ) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NGSGEOIDDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == nullptr)
        return nullptr;

    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The NGSGEOID driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NGSGEOIDDataset *poDS = new NGSGEOIDDataset();
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    int nRows = 0, nCols = 0;
    GetHeaderInfo( poOpenInfo->pabyHeader,
                   poDS->adfGeoTransform,
                   &nRows,
                   &nCols,
                   &poDS->bIsLittleEndian );
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->SetBand( 1, new NGSGEOIDRasterBand( poDS ) );

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

CPLErr NGSGEOIDDataset::GetGeoTransform( double * padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, 6 * sizeof(double));

    return CE_None;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* NGSGEOIDDataset::_GetProjectionRef()
{
    if( !osProjection.empty() )
    {
        return osProjection.c_str();
    }

    CPLString osFilename(CPLGetBasename(GetDescription()));
    osFilename.tolower();

    // See https://www.ngs.noaa.gov/GEOID/GEOID12B/faq_2012B.shtml

    // GEOID2012 files ?
    if( STARTS_WITH(osFilename, "g2012") && osFilename.size() >= 7 )
    {
        OGRSpatialReference oSRS;
        if( osFilename[6] == 'h' /* Hawai */ ||
            osFilename[6] == 's' /* Samoa */ )
        {
            // NAD83 (PA11)
            oSRS.importFromEPSG(6322);
        }
        else if( osFilename[6] == 'g' /* Guam */ )
        {
            // NAD83 (MA11)
            oSRS.importFromEPSG(6325);
        }
        else
        {
            // NAD83 (2011)
            oSRS.importFromEPSG(6318);
        }

        char* pszProjection = nullptr;
        oSRS.exportToWkt(&pszProjection);
        if( pszProjection )
            osProjection = pszProjection;
        CPLFree(pszProjection);
        return osProjection.c_str();
    }

    // USGG2012 files ? We should return IGS08, but there is only a
    // geocentric CRS in EPSG, so manually forge a geographic one from it
    if(  STARTS_WITH(osFilename, "s2012") )
    {
        osProjection =
"GEOGCS[\"IGS08\",\n"
"    DATUM[\"IGS08\",\n"
"        SPHEROID[\"GRS 1980\",6378137,298.257222101,\n"
"            AUTHORITY[\"EPSG\",\"7019\"]],\n"
"        AUTHORITY[\"EPSG\",\"1141\"]],\n"
"    PRIMEM[\"Greenwich\",0,\n"
"        AUTHORITY[\"EPSG\",\"8901\"]],\n"
"    UNIT[\"degree\",0.0174532925199433,\n"
"        AUTHORITY[\"EPSG\",\"9122\"]]]";
        return osProjection.c_str();
    }

    return SRS_WKT_WGS84_LAT_LONG;
}

/************************************************************************/
/*                       GDALRegister_NGSGEOID()                        */
/************************************************************************/

void GDALRegister_NGSGEOID()

{
    if( GDALGetDriverByName( "NGSGEOID" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "NGSGEOID" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "NOAA NGS Geoid Height Grids" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/ngsgeoid.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "bin" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = NGSGEOIDDataset::Open;
    poDriver->pfnIdentify = NGSGEOIDDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
