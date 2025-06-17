/******************************************************************************
 *
 * Project:  GView
 * Purpose:  Implementation of Atlantis MFF Support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "atlsci_spheroid.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <cctype>
#include <cmath>
#include <algorithm>

enum
{
    MFFPRJ_NONE,
    MFFPRJ_LL,
    MFFPRJ_UTM,
    MFFPRJ_UNRECOGNIZED
};

/************************************************************************/
/* ==================================================================== */
/*                              MFFDataset                              */
/* ==================================================================== */
/************************************************************************/

class MFFDataset final : public RawDataset
{
    int nGCPCount;
    GDAL_GCP *pasGCPList;

    OGRSpatialReference m_oSRS{};
    OGRSpatialReference m_oGCPSRS{};
    double adfGeoTransform[6];
    char **m_papszFileList;

    void ScanForGCPs();
    void ScanForProjectionInfo();

    CPL_DISALLOW_COPY_ASSIGN(MFFDataset)

    CPLErr Close() override;

  public:
    MFFDataset();
    ~MFFDataset() override;

    char **papszHdrLines;

    VSILFILE **pafpBandFiles;

    char **GetFileList() override;

    int GetGCPCount() override;

    const OGRSpatialReference *GetGCPSpatialRef() const override
    {
        return m_oGCPSRS.IsEmpty() ? nullptr : &m_oGCPSRS;
    }

    const GDAL_GCP *GetGCPs() override;

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    CPLErr GetGeoTransform(double *) override;

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                            MFFTiledBand                              */
/* ==================================================================== */
/************************************************************************/

class MFFTiledBand final : public GDALRasterBand
{
    friend class MFFDataset;

    VSILFILE *fpRaw;
    RawRasterBand::ByteOrder eByteOrder;

    CPL_DISALLOW_COPY_ASSIGN(MFFTiledBand)

  public:
    MFFTiledBand(MFFDataset *, int, VSILFILE *, int, int, GDALDataType,
                 RawRasterBand::ByteOrder);
    ~MFFTiledBand() override;

    CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/*                            MFFTiledBand()                            */
/************************************************************************/

MFFTiledBand::MFFTiledBand(MFFDataset *poDSIn, int nBandIn, VSILFILE *fp,
                           int nTileXSize, int nTileYSize,
                           GDALDataType eDataTypeIn,
                           RawRasterBand::ByteOrder eByteOrderIn)
    : fpRaw(fp), eByteOrder(eByteOrderIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = eDataTypeIn;

    nBlockXSize = nTileXSize;
    nBlockYSize = nTileYSize;
}

/************************************************************************/
/*                           ~MFFTiledBand()                            */
/************************************************************************/

MFFTiledBand::~MFFTiledBand()

{
    if (VSIFCloseL(fpRaw) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "I/O error");
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MFFTiledBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    const int nTilesPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    const int nWordSize = GDALGetDataTypeSizeBytes(eDataType);
    const int nBlockSize = nWordSize * nBlockXSize * nBlockYSize;

    const vsi_l_offset nOffset =
        nBlockSize *
        (nBlockXOff + static_cast<vsi_l_offset>(nBlockYOff) * nTilesPerRow);

    if (VSIFSeekL(fpRaw, nOffset, SEEK_SET) == -1 ||
        VSIFReadL(pImage, 1, nBlockSize, fpRaw) < 1)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Read of tile %d/%d failed with fseek or fread error.",
                 nBlockXOff, nBlockYOff);
        return CE_Failure;
    }

    if (eByteOrder != RawRasterBand::NATIVE_BYTE_ORDER && nWordSize > 1)
    {
        if (GDALDataTypeIsComplex(eDataType))
        {
            GDALSwapWords(pImage, nWordSize / 2, nBlockXSize * nBlockYSize,
                          nWordSize);
            GDALSwapWords(reinterpret_cast<GByte *>(pImage) + nWordSize / 2,
                          nWordSize / 2, nBlockXSize * nBlockYSize, nWordSize);
        }
        else
            GDALSwapWords(pImage, nWordSize, nBlockXSize * nBlockYSize,
                          nWordSize);
    }

    return CE_None;
}

/************************************************************************/
/*                      MFF Spheroids                                   */
/************************************************************************/

class MFFSpheroidList : public SpheroidList
{
  public:
    MFFSpheroidList();

    ~MFFSpheroidList()
    {
    }
};

MFFSpheroidList ::MFFSpheroidList()
{
    num_spheroids = 18;

    epsilonR = 0.1;
    epsilonI = 0.000001;

    spheroids[0].SetValuesByRadii("SPHERE", 6371007.0, 6371007.0);
    spheroids[1].SetValuesByRadii("EVEREST", 6377304.0, 6356103.0);
    spheroids[2].SetValuesByRadii("BESSEL", 6377397.0, 6356082.0);
    spheroids[3].SetValuesByRadii("AIRY", 6377563.0, 6356300.0);
    spheroids[4].SetValuesByRadii("CLARKE_1858", 6378294.0, 6356621.0);
    spheroids[5].SetValuesByRadii("CLARKE_1866", 6378206.4, 6356583.8);
    spheroids[6].SetValuesByRadii("CLARKE_1880", 6378249.0, 6356517.0);
    spheroids[7].SetValuesByRadii("HAYFORD", 6378388.0, 6356915.0);
    spheroids[8].SetValuesByRadii("KRASOVSKI", 6378245.0, 6356863.0);
    spheroids[9].SetValuesByRadii("HOUGH", 6378270.0, 6356794.0);
    spheroids[10].SetValuesByRadii("FISHER_60", 6378166.0, 6356784.0);
    spheroids[11].SetValuesByRadii("KAULA", 6378165.0, 6356345.0);
    spheroids[12].SetValuesByRadii("IUGG_67", 6378160.0, 6356775.0);
    spheroids[13].SetValuesByRadii("FISHER_68", 6378150.0, 6356330.0);
    spheroids[14].SetValuesByRadii("WGS_72", 6378135.0, 6356751.0);
    spheroids[15].SetValuesByRadii("IUGG_75", 6378140.0, 6356755.0);
    spheroids[16].SetValuesByRadii("WGS_84", 6378137.0, 6356752.0);
    spheroids[17].SetValuesByRadii("HUGHES", 6378273.0, 6356889.4);
}

/************************************************************************/
/* ==================================================================== */
/*                              MFFDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            MFFDataset()                             */
/************************************************************************/

MFFDataset::MFFDataset()
    : nGCPCount(0), pasGCPList(nullptr), m_papszFileList(nullptr),
      papszHdrLines(nullptr), pafpBandFiles(nullptr)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_oGCPSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~MFFDataset()                            */
/************************************************************************/

MFFDataset::~MFFDataset()

{
    MFFDataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr MFFDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (MFFDataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        CSLDestroy(papszHdrLines);
        if (pafpBandFiles)
        {
            for (int i = 0; i < GetRasterCount(); i++)
            {
                if (pafpBandFiles[i])
                {
                    if (VSIFCloseL(pafpBandFiles[i]) != 0)
                    {
                        eErr = CE_Failure;
                        CPLError(CE_Failure, CPLE_FileIO, "I/O error");
                    }
                }
            }
            CPLFree(pafpBandFiles);
        }

        if (nGCPCount > 0)
        {
            GDALDeinitGCPs(nGCPCount, pasGCPList);
        }
        CPLFree(pasGCPList);
        CSLDestroy(m_papszFileList);

        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **MFFDataset::GetFileList()
{
    char **papszFileList = RawDataset::GetFileList();
    papszFileList = CSLInsertStrings(papszFileList, -1, m_papszFileList);
    return papszFileList;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int MFFDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MFFDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return CE_None;
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *MFFDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void MFFDataset::ScanForGCPs()

{
    int NUM_GCPS = 0;

    if (CSLFetchNameValue(papszHdrLines, "NUM_GCPS") != nullptr)
        NUM_GCPS = atoi(CSLFetchNameValue(papszHdrLines, "NUM_GCPS"));
    if (NUM_GCPS < 0)
        return;

    nGCPCount = 0;
    pasGCPList =
        static_cast<GDAL_GCP *>(VSICalloc(sizeof(GDAL_GCP), 5 + NUM_GCPS));
    if (pasGCPList == nullptr)
        return;

    for (int nCorner = 0; nCorner < 5; nCorner++)
    {
        const char *pszBase = nullptr;
        double dfRasterX = 0.0;
        double dfRasterY = 0.0;

        if (nCorner == 0)
        {
            dfRasterX = 0.5;
            dfRasterY = 0.5;
            pszBase = "TOP_LEFT_CORNER";
        }
        else if (nCorner == 1)
        {
            dfRasterX = GetRasterXSize() - 0.5;
            dfRasterY = 0.5;
            pszBase = "TOP_RIGHT_CORNER";
        }
        else if (nCorner == 2)
        {
            dfRasterX = GetRasterXSize() - 0.5;
            dfRasterY = GetRasterYSize() - 0.5;
            pszBase = "BOTTOM_RIGHT_CORNER";
        }
        else if (nCorner == 3)
        {
            dfRasterX = 0.5;
            dfRasterY = GetRasterYSize() - 0.5;
            pszBase = "BOTTOM_LEFT_CORNER";
        }
        else /* if( nCorner == 4 ) */
        {
            dfRasterX = GetRasterXSize() / 2.0;
            dfRasterY = GetRasterYSize() / 2.0;
            pszBase = "CENTRE";
        }

        char szLatName[40] = {'\0'};
        char szLongName[40] = {'\0'};
        snprintf(szLatName, sizeof(szLatName), "%s_LATITUDE", pszBase);
        snprintf(szLongName, sizeof(szLongName), "%s_LONGITUDE", pszBase);

        if (CSLFetchNameValue(papszHdrLines, szLatName) != nullptr &&
            CSLFetchNameValue(papszHdrLines, szLongName) != nullptr)
        {
            GDALInitGCPs(1, pasGCPList + nGCPCount);

            CPLFree(pasGCPList[nGCPCount].pszId);

            pasGCPList[nGCPCount].pszId = CPLStrdup(pszBase);

            pasGCPList[nGCPCount].dfGCPX =
                CPLAtof(CSLFetchNameValue(papszHdrLines, szLongName));
            pasGCPList[nGCPCount].dfGCPY =
                CPLAtof(CSLFetchNameValue(papszHdrLines, szLatName));
            pasGCPList[nGCPCount].dfGCPZ = 0.0;

            pasGCPList[nGCPCount].dfGCPPixel = dfRasterX;
            pasGCPList[nGCPCount].dfGCPLine = dfRasterY;

            nGCPCount++;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Collect standalone GCPs.  They look like:                       */
    /*                                                                      */
    /*      GCPn = row, col, lat, long                                      */
    /*      GCP1 = 1, 1, 45.0, -75.0                                        */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < NUM_GCPS; i++)
    {
        char szName[25] = {'\0'};
        snprintf(szName, sizeof(szName), "GCP%d", i + 1);
        if (CSLFetchNameValue(papszHdrLines, szName) == nullptr)
            continue;

        char **papszTokens = CSLTokenizeStringComplex(
            CSLFetchNameValue(papszHdrLines, szName), ",", FALSE, FALSE);
        if (CSLCount(papszTokens) == 4)
        {
            GDALInitGCPs(1, pasGCPList + nGCPCount);

            CPLFree(pasGCPList[nGCPCount].pszId);
            pasGCPList[nGCPCount].pszId = CPLStrdup(szName);

            pasGCPList[nGCPCount].dfGCPX = CPLAtof(papszTokens[3]);
            pasGCPList[nGCPCount].dfGCPY = CPLAtof(papszTokens[2]);
            pasGCPList[nGCPCount].dfGCPZ = 0.0;
            pasGCPList[nGCPCount].dfGCPPixel = CPLAtof(papszTokens[1]) + 0.5;
            pasGCPList[nGCPCount].dfGCPLine = CPLAtof(papszTokens[0]) + 0.5;

            nGCPCount++;
        }

        CSLDestroy(papszTokens);
    }
}

/************************************************************************/
/*                        ScanForProjectionInfo                         */
/************************************************************************/

void MFFDataset::ScanForProjectionInfo()
{
    const char *pszProjName =
        CSLFetchNameValue(papszHdrLines, "PROJECTION_NAME");
    const char *pszOriginLong =
        CSLFetchNameValue(papszHdrLines, "PROJECTION_ORIGIN_LONGITUDE");
    const char *pszSpheroidName =
        CSLFetchNameValue(papszHdrLines, "SPHEROID_NAME");

    if (pszProjName == nullptr)
    {
        m_oSRS.Clear();
        m_oGCPSRS.Clear();
        return;
    }
    else if ((!EQUAL(pszProjName, "utm")) && (!EQUAL(pszProjName, "ll")))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Only utm and lat/long projections are currently supported.");
        m_oSRS.Clear();
        m_oGCPSRS.Clear();
        return;
    }
    MFFSpheroidList *mffEllipsoids = new MFFSpheroidList;

    OGRSpatialReference oProj;
    oProj.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (EQUAL(pszProjName, "utm"))
    {
        int nZone;

        if (pszOriginLong == nullptr)
        {
            // If origin not specified, assume 0.0.
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "No projection origin longitude specified.  Assuming 0.0.");
            nZone = 31;
        }
        else
            nZone = 31 + static_cast<int>(floor(CPLAtof(pszOriginLong) / 6.0));

        if (nGCPCount >= 5 && pasGCPList[4].dfGCPY < 0)
            oProj.SetUTM(nZone, 0);
        else
            oProj.SetUTM(nZone, 1);

        if (pszOriginLong != nullptr)
            oProj.SetProjParm(SRS_PP_CENTRAL_MERIDIAN, CPLAtof(pszOriginLong));
    }

    OGRSpatialReference oLL;
    oLL.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (pszOriginLong != nullptr)
        oLL.SetProjParm(SRS_PP_LONGITUDE_OF_ORIGIN, CPLAtof(pszOriginLong));

    if (pszSpheroidName == nullptr)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unspecified ellipsoid.  Using wgs-84 parameters.\n");

        oProj.SetWellKnownGeogCS("WGS84");
        oLL.SetWellKnownGeogCS("WGS84");
    }
    else
    {
        if (mffEllipsoids->SpheroidInList(pszSpheroidName))
        {
            oProj.SetGeogCS(
                "unknown", "unknown", pszSpheroidName,
                mffEllipsoids->GetSpheroidEqRadius(pszSpheroidName),
                mffEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName));
            oLL.SetGeogCS(
                "unknown", "unknown", pszSpheroidName,
                mffEllipsoids->GetSpheroidEqRadius(pszSpheroidName),
                mffEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName));
        }
        else if (EQUAL(pszSpheroidName, "USER_DEFINED"))
        {
            const char *pszSpheroidEqRadius =
                CSLFetchNameValue(papszHdrLines, "SPHEROID_EQUATORIAL_RADIUS");
            const char *pszSpheroidPolarRadius =
                CSLFetchNameValue(papszHdrLines, "SPHEROID_POLAR_RADIUS");
            if ((pszSpheroidEqRadius != nullptr) &&
                (pszSpheroidPolarRadius != nullptr))
            {
                const double eq_radius = CPLAtof(pszSpheroidEqRadius);
                const double polar_radius = CPLAtof(pszSpheroidPolarRadius);
                oProj.SetGeogCS("unknown", "unknown", "unknown", eq_radius,
                                eq_radius / (eq_radius - polar_radius));
                oLL.SetGeogCS("unknown", "unknown", "unknown", eq_radius,
                              eq_radius / (eq_radius - polar_radius));
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Radii not specified for user-defined ellipsoid. "
                         "Using wgs-84 parameters.");
                oProj.SetWellKnownGeogCS("WGS84");
                oLL.SetWellKnownGeogCS("WGS84");
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unrecognized ellipsoid.  Using wgs-84 parameters.");
            oProj.SetWellKnownGeogCS("WGS84");
            oLL.SetWellKnownGeogCS("WGS84");
        }
    }

    /* If a geotransform is sufficient to represent the GCP's (i.e. each */
    /* estimated gcp is within 0.25*pixel size of the actual value- this */
    /* is the test applied by GDALGCPsToGeoTransform), store the         */
    /* geotransform.                                                     */
    bool transform_ok = false;

    if (EQUAL(pszProjName, "LL"))
    {
        transform_ok = CPL_TO_BOOL(
            GDALGCPsToGeoTransform(nGCPCount, pasGCPList, adfGeoTransform, 0));
    }
    else
    {
        OGRCoordinateTransformation *poTransform =
            OGRCreateCoordinateTransformation(&oLL, &oProj);
        bool bSuccess = true;
        if (poTransform == nullptr)
        {
            CPLErrorReset();
            bSuccess = FALSE;
        }

        double *dfPrjX =
            static_cast<double *>(CPLMalloc(nGCPCount * sizeof(double)));
        double *dfPrjY =
            static_cast<double *>(CPLMalloc(nGCPCount * sizeof(double)));

        for (int gcp_index = 0; gcp_index < nGCPCount; gcp_index++)
        {
            dfPrjX[gcp_index] = pasGCPList[gcp_index].dfGCPX;
            dfPrjY[gcp_index] = pasGCPList[gcp_index].dfGCPY;

            if (bSuccess && !poTransform->Transform(1, &(dfPrjX[gcp_index]),
                                                    &(dfPrjY[gcp_index])))
                bSuccess = FALSE;
        }

        if (bSuccess)
        {
            for (int gcp_index = 0; gcp_index < nGCPCount; gcp_index++)
            {
                pasGCPList[gcp_index].dfGCPX = dfPrjX[gcp_index];
                pasGCPList[gcp_index].dfGCPY = dfPrjY[gcp_index];
            }
            transform_ok = CPL_TO_BOOL(GDALGCPsToGeoTransform(
                nGCPCount, pasGCPList, adfGeoTransform, 0));
        }

        if (poTransform)
            delete poTransform;

        CPLFree(dfPrjX);
        CPLFree(dfPrjY);
    }

    m_oSRS = oProj;
    m_oGCPSRS = std::move(oProj);

    if (!transform_ok)
    {
        /* transform is sufficient in some cases (slant range, standalone gcps)
         */
        adfGeoTransform[0] = 0.0;
        adfGeoTransform[1] = 1.0;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = 0.0;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = 1.0;
        m_oSRS.Clear();
    }

    delete mffEllipsoids;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MFFDataset::Open(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      We assume the user is pointing to the header file.              */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 17 || poOpenInfo->fpL == nullptr)
        return nullptr;

    if (!poOpenInfo->IsExtensionEqualToCI("hdr"))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Load the .hdr file, and compress white space out around the     */
    /*      equal sign.                                                     */
    /* -------------------------------------------------------------------- */
    char **papszHdrLines = CSLLoad(poOpenInfo->pszFilename);
    if (papszHdrLines == nullptr)
        return nullptr;

    // Remove spaces.  e.g.
    // SPHEROID_NAME = CLARKE_1866 -> SPHEROID_NAME=CLARKE_1866
    for (int i = 0; papszHdrLines[i] != nullptr; i++)
    {
        int iDst = 0;
        char *pszLine = papszHdrLines[i];

        for (int iSrc = 0; pszLine[iSrc] != '\0'; iSrc++)
        {
            if (pszLine[iSrc] != ' ')
            {
                pszLine[iDst++] = pszLine[iSrc];
            }
        }
        pszLine[iDst] = '\0';
    }

    /* -------------------------------------------------------------------- */
    /*      Verify it is an MFF file.                                       */
    /* -------------------------------------------------------------------- */
    if (CSLFetchNameValue(papszHdrLines, "IMAGE_FILE_FORMAT") != nullptr &&
        !EQUAL(CSLFetchNameValue(papszHdrLines, "IMAGE_FILE_FORMAT"), "MFF"))
    {
        CSLDestroy(papszHdrLines);
        return nullptr;
    }

    if ((CSLFetchNameValue(papszHdrLines, "IMAGE_LINES") == nullptr ||
         CSLFetchNameValue(papszHdrLines, "LINE_SAMPLES") == nullptr) &&
        (CSLFetchNameValue(papszHdrLines, "no_rows") == nullptr ||
         CSLFetchNameValue(papszHdrLines, "no_columns") == nullptr))
    {
        CSLDestroy(papszHdrLines);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<MFFDataset>();

    poDS->papszHdrLines = papszHdrLines;

    poDS->eAccess = poOpenInfo->eAccess;

    /* -------------------------------------------------------------------- */
    /*      Set some dataset wide information.                              */
    /* -------------------------------------------------------------------- */
    if (CSLFetchNameValue(papszHdrLines, "no_rows") != nullptr &&
        CSLFetchNameValue(papszHdrLines, "no_columns") != nullptr)
    {
        poDS->nRasterXSize =
            atoi(CSLFetchNameValue(papszHdrLines, "no_columns"));
        poDS->nRasterYSize = atoi(CSLFetchNameValue(papszHdrLines, "no_rows"));
    }
    else
    {
        poDS->nRasterXSize =
            atoi(CSLFetchNameValue(papszHdrLines, "LINE_SAMPLES"));
        poDS->nRasterYSize =
            atoi(CSLFetchNameValue(papszHdrLines, "IMAGE_LINES"));
    }

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    RawRasterBand::ByteOrder eByteOrder = RawRasterBand::NATIVE_BYTE_ORDER;

    const char *pszByteOrder = CSLFetchNameValue(papszHdrLines, "BYTE_ORDER");
    if (pszByteOrder)
    {
        eByteOrder = EQUAL(pszByteOrder, "LSB")
                         ? RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN
                         : RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN;
    }

    /* -------------------------------------------------------------------- */
    /*      Get some information specific to APP tiled files.               */
    /* -------------------------------------------------------------------- */
    int nTileXSize = 0;
    int nTileYSize = 0;
    const char *pszRefinedType = CSLFetchNameValue(papszHdrLines, "type");
    const bool bTiled = CSLFetchNameValue(papszHdrLines, "no_rows") != nullptr;

    if (bTiled)
    {
        if (CSLFetchNameValue(papszHdrLines, "tile_size_rows"))
            nTileYSize =
                atoi(CSLFetchNameValue(papszHdrLines, "tile_size_rows"));
        if (CSLFetchNameValue(papszHdrLines, "tile_size_columns"))
            nTileXSize =
                atoi(CSLFetchNameValue(papszHdrLines, "tile_size_columns"));

        if (nTileXSize <= 0 || nTileYSize <= 0 ||
            poDS->nRasterXSize - 1 > INT_MAX - nTileXSize ||
            poDS->nRasterYSize - 1 > INT_MAX - nTileYSize)
        {
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Read the directory to find matching band files.                 */
    /* -------------------------------------------------------------------- */
    const std::string osTargetPath = CPLGetPathSafe(poOpenInfo->pszFilename);
    const std::string osTargetBase =
        CPLGetBasenameSafe(poOpenInfo->pszFilename);
    const CPLStringList aosDirFiles(
        VSIReadDir(CPLGetPathSafe(poOpenInfo->pszFilename).c_str()));
    if (aosDirFiles.empty())
    {
        return nullptr;
    }

    int nSkipped = 0;
    for (int nRawBand = 0; true; nRawBand++)
    {
        /* Find the next raw band file. */

        int i = 0;  // Used after for.
        for (; i < aosDirFiles.size(); i++)
        {
            if (!EQUAL(CPLGetBasenameSafe(aosDirFiles[i]).c_str(),
                       osTargetBase.c_str()))
                continue;

            const std::string osExtension = CPLGetExtensionSafe(aosDirFiles[i]);
            if (osExtension.size() >= 2 &&
                isdigit(static_cast<unsigned char>(osExtension[1])) &&
                atoi(osExtension.c_str() + 1) == nRawBand &&
                strchr("bBcCiIjJrRxXzZ", osExtension[0]) != nullptr)
                break;
        }

        if (i == aosDirFiles.size())
            break;

        /* open the file for required level of access */
        const std::string osRawFilename =
            CPLFormFilenameSafe(osTargetPath.c_str(), aosDirFiles[i], nullptr);

        VSILFILE *fpRaw = nullptr;
        if (poOpenInfo->eAccess == GA_Update)
            fpRaw = VSIFOpenL(osRawFilename.c_str(), "rb+");
        else
            fpRaw = VSIFOpenL(osRawFilename.c_str(), "rb");

        if (fpRaw == nullptr)
        {
            CPLError(CE_Warning, CPLE_OpenFailed,
                     "Unable to open %s ... skipping.", osRawFilename.c_str());
            nSkipped++;
            continue;
        }
        poDS->m_papszFileList =
            CSLAddString(poDS->m_papszFileList, osRawFilename.c_str());

        GDALDataType eDataType = GDT_Unknown;
        const std::string osExt = CPLGetExtensionSafe(aosDirFiles[i]);
        if (pszRefinedType != nullptr)
        {
            if (EQUAL(pszRefinedType, "C*4"))
                eDataType = GDT_CFloat32;
            else if (EQUAL(pszRefinedType, "C*8"))
                eDataType = GDT_CFloat64;
            else if (EQUAL(pszRefinedType, "R*4"))
                eDataType = GDT_Float32;
            else if (EQUAL(pszRefinedType, "R*8"))
                eDataType = GDT_Float64;
            else if (EQUAL(pszRefinedType, "I*1"))
                eDataType = GDT_Byte;
            else if (EQUAL(pszRefinedType, "I*2"))
                eDataType = GDT_Int16;
            else if (EQUAL(pszRefinedType, "I*4"))
                eDataType = GDT_Int32;
            else if (EQUAL(pszRefinedType, "U*2"))
                eDataType = GDT_UInt16;
            else if (EQUAL(pszRefinedType, "U*4"))
                eDataType = GDT_UInt32;
            else if (EQUAL(pszRefinedType, "J*1"))
            {
                CPLError(
                    CE_Warning, CPLE_OpenFailed,
                    "Unable to open band %d because type J*1 is not handled. "
                    "Skipping.",
                    nRawBand + 1);
                nSkipped++;
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpRaw));
                continue;  // Does not support 1 byte complex.
            }
            else if (EQUAL(pszRefinedType, "J*2"))
                eDataType = GDT_CInt16;
            else if (EQUAL(pszRefinedType, "K*4"))
                eDataType = GDT_CInt32;
            else
            {
                CPLError(
                    CE_Warning, CPLE_OpenFailed,
                    "Unable to open band %d because type %s is not handled. "
                    "Skipping.\n",
                    nRawBand + 1, pszRefinedType);
                nSkipped++;
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpRaw));
                continue;
            }
        }
        else if (STARTS_WITH_CI(osExt.c_str(), "b"))
        {
            eDataType = GDT_Byte;
        }
        else if (STARTS_WITH_CI(osExt.c_str(), "i"))
        {
            eDataType = GDT_UInt16;
        }
        else if (STARTS_WITH_CI(osExt.c_str(), "j"))
        {
            eDataType = GDT_CInt16;
        }
        else if (STARTS_WITH_CI(osExt.c_str(), "r"))
        {
            eDataType = GDT_Float32;
        }
        else if (STARTS_WITH_CI(osExt.c_str(), "x"))
        {
            eDataType = GDT_CFloat32;
        }
        else
        {
            CPLError(CE_Warning, CPLE_OpenFailed,
                     "Unable to open band %d because extension %s is not "
                     "handled.  Skipping.",
                     nRawBand + 1, osExt.c_str());
            nSkipped++;
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpRaw));
            continue;
        }

        const int nBand = poDS->GetRasterCount() + 1;

        const int nPixelOffset = GDALGetDataTypeSizeBytes(eDataType);
        std::unique_ptr<GDALRasterBand> poBand;

        if (bTiled)
        {
            if (nTileXSize > INT_MAX / nTileYSize / nPixelOffset)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too large tile");
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpRaw));
                return nullptr;
            }

            poBand = std::make_unique<MFFTiledBand>(poDS.get(), nBand, fpRaw,
                                                    nTileXSize, nTileYSize,
                                                    eDataType, eByteOrder);
        }
        else
        {
            if (nPixelOffset != 0 &&
                poDS->GetRasterXSize() > INT_MAX / nPixelOffset)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Int overflow occurred... skipping");
                nSkipped++;
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpRaw));
                continue;
            }

            poBand = RawRasterBand::Create(
                poDS.get(), nBand, fpRaw, 0, nPixelOffset,
                nPixelOffset * poDS->GetRasterXSize(), eDataType, eByteOrder,
                RawRasterBand::OwnFP::YES);
        }

        poDS->SetBand(nBand, std::move(poBand));
    }

    /* -------------------------------------------------------------------- */
    /*      Check if we have bands.                                         */
    /* -------------------------------------------------------------------- */
    if (poDS->GetRasterCount() == 0)
    {
        if (nSkipped > 0 && poOpenInfo->eAccess)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to open %d files that were apparently bands.  "
                     "Perhaps this dataset is readonly?",
                     nSkipped);
            return nullptr;
        }
        else
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "MFF header file read successfully, but no bands "
                     "were successfully found and opened.");
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Set all information from the .hdr that isn't well know to be    */
    /*      metadata.                                                       */
    /* -------------------------------------------------------------------- */
    for (int i = 0; papszHdrLines[i] != nullptr; i++)
    {
        char *pszName = nullptr;

        const char *pszValue = CPLParseNameValue(papszHdrLines[i], &pszName);
        if (pszName == nullptr || pszValue == nullptr)
            continue;

        if (!EQUAL(pszName, "END") && !EQUAL(pszName, "FILE_TYPE") &&
            !EQUAL(pszName, "BYTE_ORDER") && !EQUAL(pszName, "no_columns") &&
            !EQUAL(pszName, "no_rows") && !EQUAL(pszName, "type") &&
            !EQUAL(pszName, "tile_size_rows") &&
            !EQUAL(pszName, "tile_size_columns") &&
            !EQUAL(pszName, "IMAGE_FILE_FORMAT") &&
            !EQUAL(pszName, "IMAGE_LINES") && !EQUAL(pszName, "LINE_SAMPLES"))
        {
            poDS->SetMetadataItem(pszName, pszValue);
        }

        CPLFree(pszName);
    }

    /* -------------------------------------------------------------------- */
    /*      Any GCPs in header file?                                        */
    /* -------------------------------------------------------------------- */
    poDS->ScanForGCPs();
    poDS->ScanForProjectionInfo();
    if (poDS->nGCPCount == 0)
        poDS->m_oGCPSRS.Clear();

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
/*                         GDALRegister_MFF()                           */
/************************************************************************/

void GDALRegister_MFF()

{
    if (GDALGetDriverByName("MFF") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MFF");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Vexcel MFF Raster");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/mff.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "hdr");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = MFFDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
