/******************************************************************************
 *
 * Project:  Natural Resources Canada's Geoid BYN file format
 * Purpose:  Implementation of BYN format
 * Author:   Ivan Lucena, ivan.lucena@outlook.com
 *
 ******************************************************************************
 * Copyright (c) 2018, Ivan Lucena
 * Copyright (c) 2018, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "byndataset.h"
#include "rawdataset.h"

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

// Specification at
// https://www.nrcan.gc.ca/sites/www.nrcan.gc.ca/files/earthsciences/pdf/gpshgrid_e.pdf

const static BYNEllipsoids EllipsoidTable[] = {
    {"GRS80", 6378137.0, 298.257222101},
    {"WGS84", 6378137.0, 298.257223564},
    {"ALT1", 6378136.3, 298.256415099},
    {"GRS67", 6378160.0, 298.247167427},
    {"ELLIP1", 6378136.46, 298.256415099},
    {"ALT2", 6378136.3, 298.257},
    {"ELLIP2", 6378136.0, 298.257},
    {"CLARKE 1866", 6378206.4, 294.9786982}};

/************************************************************************/
/*                            BYNRasterBand()                           */
/************************************************************************/

BYNRasterBand::BYNRasterBand(GDALDataset *poDSIn, int nBandIn,
                             VSILFILE *fpRawIn, vsi_l_offset nImgOffsetIn,
                             int nPixelOffsetIn, int nLineOffsetIn,
                             GDALDataType eDataTypeIn, int bNativeOrderIn)
    : RawRasterBand(poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn,
                    nLineOffsetIn, eDataTypeIn, bNativeOrderIn,
                    RawRasterBand::OwnFP::NO)
{
}

/************************************************************************/
/*                           ~BYNRasterBand()                           */
/************************************************************************/

BYNRasterBand::~BYNRasterBand()
{
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double BYNRasterBand::GetNoDataValue(int *pbSuccess)
{
    if (pbSuccess)
        *pbSuccess = TRUE;
    int bSuccess = FALSE;
    double dfNoData = GDALPamRasterBand::GetNoDataValue(&bSuccess);
    if (bSuccess)
    {
        return dfNoData;
    }
    const double dfFactor =
        reinterpret_cast<BYNDataset *>(poDS)->hHeader.dfFactor;
    return eDataType == GDT_Int16 ? 32767.0 : 9999.0 * dfFactor;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double BYNRasterBand::GetScale(int *pbSuccess)
{
    if (pbSuccess != nullptr)
        *pbSuccess = TRUE;
    const double dfFactor =
        reinterpret_cast<BYNDataset *>(poDS)->hHeader.dfFactor;
    return (dfFactor != 0.0) ? 1.0 / dfFactor : 0.0;
}

/************************************************************************/
/* ==================================================================== */
/*                              BYNDataset                              */
/* ==================================================================== */
/************************************************************************/

BYNDataset::BYNDataset()
    : fpImage(nullptr), hHeader{0, 0, 0, 0, 0, 0,   0,   0, 0.0, 0,   0, 0,
                                0, 0, 0, 0, 0, 0.0, 0.0, 0, 0,   0.0, 0}
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~BYNDataset()                             */
/************************************************************************/

BYNDataset::~BYNDataset()

{
    BYNDataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr BYNDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (BYNDataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        if (fpImage != nullptr)
        {
            if (VSIFCloseL(fpImage) != 0)
            {
                eErr = CE_Failure;
                CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            }
        }

        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int BYNDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes < BYN_HDR_SZ)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Check file extension (.byn/.err)                                */
/* -------------------------------------------------------------------- */
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    const char *pszFileExtension = poOpenInfo->osExtension.c_str();

    if (!EQUAL(pszFileExtension, "byn") && !EQUAL(pszFileExtension, "err"))
    {
        return FALSE;
    }
#endif

    /* -------------------------------------------------------------------- */
    /*      Check some value's ranges on header                             */
    /* -------------------------------------------------------------------- */

    BYNHeader hHeader = {0, 0, 0, 0, 0, 0,   0,   0, 0.0, 0,   0, 0,
                         0, 0, 0, 0, 0, 0.0, 0.0, 0, 0,   0.0, 0};

    buffer2header(poOpenInfo->pabyHeader, &hHeader);

    if (hHeader.nGlobal < 0 || hHeader.nGlobal > 1 || hHeader.nType < 0 ||
        hHeader.nType > 9 || (hHeader.nSizeOf != 2 && hHeader.nSizeOf != 4) ||
        hHeader.nVDatum < 0 || hHeader.nVDatum > 3 || hHeader.nDescrip < 0 ||
        hHeader.nDescrip > 3 || hHeader.nSubType < 0 || hHeader.nSubType > 9 ||
        hHeader.nDatum < 0 || hHeader.nDatum > 1 || hHeader.nEllipsoid < 0 ||
        hHeader.nEllipsoid > 7 || hHeader.nByteOrder < 0 ||
        hHeader.nByteOrder > 1 || hHeader.nScale < 0 || hHeader.nScale > 1)
        return FALSE;

#if 0
    // We have disabled those checks as invalid values are often found in some
    // datasets, such as http://s3.microsurvey.com/os/fieldgenius/geoids/Lithuania.zip
    // We don't use those fields, so we may just ignore them.
    if((hHeader.nTideSys   < 0 || hHeader.nTideSys   > 2 ||
        hHeader.nPtType    < 0 || hHeader.nPtType    > 1 ))
    {
        // Some datasets use 0xCC as a marker for invalidity for
        // records starting from Geopotential Wo
        for( int i = 52; i < 78; i++ )
        {
            if( poOpenInfo->pabyHeader[i] != 0xCC )
                return FALSE;
        }
    }
#endif

    if (hHeader.nScale == 0)
    {
        if ((std::abs(static_cast<GIntBig>(hHeader.nSouth) -
                      (hHeader.nDLat / 2)) > BYN_MAX_LAT) ||
            (std::abs(static_cast<GIntBig>(hHeader.nNorth) +
                      (hHeader.nDLat / 2)) > BYN_MAX_LAT) ||
            (std::abs(static_cast<GIntBig>(hHeader.nWest) -
                      (hHeader.nDLon / 2)) > BYN_MAX_LON) ||
            (std::abs(static_cast<GIntBig>(hHeader.nEast) +
                      (hHeader.nDLon / 2)) > BYN_MAX_LON))
            return FALSE;
    }
    else
    {
        if ((std::abs(static_cast<GIntBig>(hHeader.nSouth) -
                      (hHeader.nDLat / 2)) > BYN_MAX_LAT_SCL) ||
            (std::abs(static_cast<GIntBig>(hHeader.nNorth) +
                      (hHeader.nDLat / 2)) > BYN_MAX_LAT_SCL) ||
            (std::abs(static_cast<GIntBig>(hHeader.nWest) -
                      (hHeader.nDLon / 2)) > BYN_MAX_LON_SCL) ||
            (std::abs(static_cast<GIntBig>(hHeader.nEast) +
                      (hHeader.nDLon / 2)) > BYN_MAX_LON_SCL))
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BYNDataset::Open(GDALOpenInfo *poOpenInfo)

{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == nullptr ||
        poOpenInfo->eAccess == GA_Update)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */

    auto poDS = std::make_unique<BYNDataset>();

    poDS->eAccess = poOpenInfo->eAccess;
    std::swap(poDS->fpImage, poOpenInfo->fpL);

    /* -------------------------------------------------------------------- */
    /*      Read the header.                                                */
    /* -------------------------------------------------------------------- */

    buffer2header(poOpenInfo->pabyHeader, &poDS->hHeader);

    /********************************/
    /* Scale boundaries and spacing */
    /********************************/

    double dfSouth = poDS->hHeader.nSouth;
    double dfNorth = poDS->hHeader.nNorth;
    double dfWest = poDS->hHeader.nWest;
    double dfEast = poDS->hHeader.nEast;
    double dfDLat = poDS->hHeader.nDLat;
    double dfDLon = poDS->hHeader.nDLon;

    if (poDS->hHeader.nScale == 1)
    {
        dfSouth *= BYN_SCALE;
        dfNorth *= BYN_SCALE;
        dfWest *= BYN_SCALE;
        dfEast *= BYN_SCALE;
        dfDLat *= BYN_SCALE;
        dfDLon *= BYN_SCALE;
    }

    /******************************/
    /* Calculate rows and columns */
    /******************************/

    double dfXSize = -1;
    double dfYSize = -1;

    poDS->nRasterXSize = -1;
    poDS->nRasterYSize = -1;

    if (dfDLat != 0.0 && dfDLon != 0.0)
    {
        dfXSize = ((dfEast - dfWest + 1.0) / dfDLon) + 1.0;
        dfYSize = ((dfNorth - dfSouth + 1.0) / dfDLat) + 1.0;
    }

    if (dfXSize > 0.0 && dfXSize < std::numeric_limits<double>::max() &&
        dfYSize > 0.0 && dfYSize < std::numeric_limits<double>::max())
    {
        poDS->nRasterXSize = static_cast<GInt32>(dfXSize);
        poDS->nRasterYSize = static_cast<GInt32>(dfYSize);
    }

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    /*****************************/
    /* Build GeoTransform matrix */
    /*****************************/

    poDS->adfGeoTransform[0] = (dfWest - (dfDLon / 2.0)) / 3600.0;
    poDS->adfGeoTransform[1] = dfDLon / 3600.0;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = (dfNorth + (dfDLat / 2.0)) / 3600.0;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -1 * dfDLat / 3600.0;

    /*********************/
    /* Set data type     */
    /*********************/

    GDALDataType eDT = GDT_Unknown;

    if (poDS->hHeader.nSizeOf == 2)
        eDT = GDT_Int16;
    else if (poDS->hHeader.nSizeOf == 4)
        eDT = GDT_Int32;
    else
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information object.                                 */
    /* -------------------------------------------------------------------- */

    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);

    int bIsLSB = poDS->hHeader.nByteOrder == 1 ? 1 : 0;

    auto poBand = std::make_unique<BYNRasterBand>(
        poDS.get(), 1, poDS->fpImage, BYN_HDR_SZ, nDTSize,
        poDS->nRasterXSize * nDTSize, eDT, CPL_IS_LSB == bIsLSB);
    if (!poBand->IsValid())
        return nullptr;
    poDS->SetBand(1, std::move(poBand));

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */

    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */

    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BYNDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return CE_None;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *BYNDataset::GetSpatialRef() const

{
    if (!m_oSRS.IsEmpty())
        return &m_oSRS;

    /* Try to use a prefefined EPSG compound CS */

    if (hHeader.nDatum == 1 && hHeader.nVDatum == 2)
    {
        m_oSRS.importFromEPSG(BYN_DATUM_1_VDATUM_2);
        return &m_oSRS;
    }

    /* Build the GEOGCS based on Datum ( or Ellipsoid )*/

    bool bNoGeogCS = false;

    if (hHeader.nDatum == 0)
        m_oSRS.importFromEPSG(BYN_DATUM_0);
    else if (hHeader.nDatum == 1)
        m_oSRS.importFromEPSG(BYN_DATUM_1);
    else
    {
        /* Build GEOGCS based on Ellipsoid (Table 3) */

        if (hHeader.nEllipsoid > -1 &&
            hHeader.nEllipsoid <
                static_cast<GInt16>(CPL_ARRAYSIZE(EllipsoidTable)))
            m_oSRS.SetGeogCS(
                CPLSPrintf("BYN Ellipsoid(%d)", hHeader.nEllipsoid),
                "Unspecified", EllipsoidTable[hHeader.nEllipsoid].pszName,
                EllipsoidTable[hHeader.nEllipsoid].dfSemiMajor,
                EllipsoidTable[hHeader.nEllipsoid].dfInvFlattening);
        else
            bNoGeogCS = true;
    }

    /* Build the VERT_CS based on VDatum */

    OGRSpatialReference oSRSComp;
    OGRSpatialReference oSRSVert;

    int nVertCS = 0;

    if (hHeader.nVDatum == 1)
        nVertCS = BYN_VDATUM_1;
    else if (hHeader.nVDatum == 2)
        nVertCS = BYN_VDATUM_2;
    else if (hHeader.nVDatum == 3)
        nVertCS = BYN_VDATUM_3;
    else
    {
        /* Return GEOGCS ( .err files ) */

        if (bNoGeogCS)
            return nullptr;

        return &m_oSRS;
    }

    oSRSVert.importFromEPSG(nVertCS);

    /* Create CPMPD_CS with GEOGCS and VERT_CS */

    if (oSRSComp.SetCompoundCS(CPLSPrintf("BYN Datum(%d) & VDatum(%d)",
                                          hHeader.nDatum, hHeader.nDatum),
                               &m_oSRS, &oSRSVert) == CE_None)
    {
        /* Return COMPD_CS with GEOGCS and VERT_CS */

        m_oSRS = std::move(oSRSComp);
        m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        return &m_oSRS;
    }

    return nullptr;
}

/*----------------------------------------------------------------------*/
/*                           buffer2header()                            */
/*----------------------------------------------------------------------*/

void BYNDataset::buffer2header(const GByte *pabyBuf, BYNHeader *pohHeader)

{
    memcpy(&pohHeader->nSouth, pabyBuf, 4);
    memcpy(&pohHeader->nNorth, pabyBuf + 4, 4);
    memcpy(&pohHeader->nWest, pabyBuf + 8, 4);
    memcpy(&pohHeader->nEast, pabyBuf + 12, 4);
    memcpy(&pohHeader->nDLat, pabyBuf + 16, 2);
    memcpy(&pohHeader->nDLon, pabyBuf + 18, 2);
    memcpy(&pohHeader->nGlobal, pabyBuf + 20, 2);
    memcpy(&pohHeader->nType, pabyBuf + 22, 2);
    memcpy(&pohHeader->dfFactor, pabyBuf + 24, 8);
    memcpy(&pohHeader->nSizeOf, pabyBuf + 32, 2);
    memcpy(&pohHeader->nVDatum, pabyBuf + 34, 2);
    memcpy(&pohHeader->nDescrip, pabyBuf + 40, 2);
    memcpy(&pohHeader->nSubType, pabyBuf + 42, 2);
    memcpy(&pohHeader->nDatum, pabyBuf + 44, 2);
    memcpy(&pohHeader->nEllipsoid, pabyBuf + 46, 2);
    memcpy(&pohHeader->nByteOrder, pabyBuf + 48, 2);
    memcpy(&pohHeader->nScale, pabyBuf + 50, 2);
    memcpy(&pohHeader->dfWo, pabyBuf + 52, 8);
    memcpy(&pohHeader->dfGM, pabyBuf + 60, 8);
    memcpy(&pohHeader->nTideSys, pabyBuf + 68, 2);
    memcpy(&pohHeader->nRealiz, pabyBuf + 70, 2);
    memcpy(&pohHeader->dEpoch, pabyBuf + 72, 4);
    memcpy(&pohHeader->nPtType, pabyBuf + 76, 2);

#if defined(CPL_MSB)
    CPL_LSBPTR32(&pohHeader->nSouth);
    CPL_LSBPTR32(&pohHeader->nNorth);
    CPL_LSBPTR32(&pohHeader->nWest);
    CPL_LSBPTR32(&pohHeader->nEast);
    CPL_LSBPTR16(&pohHeader->nDLat);
    CPL_LSBPTR16(&pohHeader->nDLon);
    CPL_LSBPTR16(&pohHeader->nGlobal);
    CPL_LSBPTR16(&pohHeader->nType);
    CPL_LSBPTR64(&pohHeader->dfFactor);
    CPL_LSBPTR16(&pohHeader->nSizeOf);
    CPL_LSBPTR16(&pohHeader->nVDatum);
    CPL_LSBPTR16(&pohHeader->nDescrip);
    CPL_LSBPTR16(&pohHeader->nSubType);
    CPL_LSBPTR16(&pohHeader->nDatum);
    CPL_LSBPTR16(&pohHeader->nEllipsoid);
    CPL_LSBPTR16(&pohHeader->nByteOrder);
    CPL_LSBPTR16(&pohHeader->nScale);
    CPL_LSBPTR64(&pohHeader->dfWo);
    CPL_LSBPTR64(&pohHeader->dfGM);
    CPL_LSBPTR16(&pohHeader->nTideSys);
    CPL_LSBPTR16(&pohHeader->nRealiz);
    CPL_LSBPTR32(&pohHeader->dEpoch);
    CPL_LSBPTR16(&pohHeader->nPtType);
#endif

#if DEBUG
    CPLDebug("BYN", "South         = %d", pohHeader->nSouth);
    CPLDebug("BYN", "North         = %d", pohHeader->nNorth);
    CPLDebug("BYN", "West          = %d", pohHeader->nWest);
    CPLDebug("BYN", "East          = %d", pohHeader->nEast);
    CPLDebug("BYN", "DLat          = %d", pohHeader->nDLat);
    CPLDebug("BYN", "DLon          = %d", pohHeader->nDLon);
    CPLDebug("BYN", "DGlobal       = %d", pohHeader->nGlobal);
    CPLDebug("BYN", "DType         = %d", pohHeader->nType);
    CPLDebug("BYN", "Factor        = %f", pohHeader->dfFactor);
    CPLDebug("BYN", "SizeOf        = %d", pohHeader->nSizeOf);
    CPLDebug("BYN", "VDatum        = %d", pohHeader->nVDatum);
    CPLDebug("BYN", "Data          = %d", pohHeader->nDescrip);
    CPLDebug("BYN", "SubType       = %d", pohHeader->nSubType);
    CPLDebug("BYN", "Datum         = %d", pohHeader->nDatum);
    CPLDebug("BYN", "Ellipsoid     = %d", pohHeader->nEllipsoid);
    CPLDebug("BYN", "ByteOrder     = %d", pohHeader->nByteOrder);
    CPLDebug("BYN", "Scale         = %d", pohHeader->nScale);
    CPLDebug("BYN", "Wo            = %f", pohHeader->dfWo);
    CPLDebug("BYN", "GM            = %f", pohHeader->dfGM);
    CPLDebug("BYN", "TideSystem    = %d", pohHeader->nTideSys);
    CPLDebug("BYN", "RefRealzation = %d", pohHeader->nRealiz);
    CPLDebug("BYN", "Epoch         = %f", pohHeader->dEpoch);
    CPLDebug("BYN", "PtType        = %d", pohHeader->nPtType);
#endif
}

/************************************************************************/
/*                          GDALRegister_BYN()                          */
/************************************************************************/

void GDALRegister_BYN()

{
    if (GDALGetDriverByName("BYN") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("BYN");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Natural Resources Canada's Geoid");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "byn err");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/byn.html");

    poDriver->pfnOpen = BYNDataset::Open;
    poDriver->pfnIdentify = BYNDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
