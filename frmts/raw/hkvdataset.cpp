/******************************************************************************
 *
 * Project:  GView
 * Purpose:  Implementation of Atlantis HKV labelled blob support
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <ctype.h>

#include "atlsci_spheroid.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <cmath>

#include <algorithm>

/************************************************************************/
/* ==================================================================== */
/*                            HKVRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class HKVDataset;

class HKVRasterBand final : public RawRasterBand
{
    friend class HKVDataset;

  public:
    HKVRasterBand(HKVDataset *poDS, int nBand, VSILFILE *fpRaw,
                  unsigned int nImgOffset, int nPixelOffset, int nLineOffset,
                  GDALDataType eDataType, int bNativeOrder);

    ~HKVRasterBand() override
    {
    }
};

/************************************************************************/
/*                      HKV Spheroids                                   */
/************************************************************************/

class HKVSpheroidList : public SpheroidList
{
  public:
    HKVSpheroidList();

    ~HKVSpheroidList()
    {
    }
};

HKVSpheroidList ::HKVSpheroidList()
{
    num_spheroids = 58;
    epsilonR = 0.1;
    epsilonI = 0.000001;

    spheroids[0].SetValuesByEqRadiusAndInvFlattening("airy-1830", 6377563.396,
                                                     299.3249646);
    spheroids[1].SetValuesByEqRadiusAndInvFlattening("modified-airy",
                                                     6377340.189, 299.3249646);
    spheroids[2].SetValuesByEqRadiusAndInvFlattening("australian-national",
                                                     6378160, 298.25);
    spheroids[3].SetValuesByEqRadiusAndInvFlattening("bessel-1841-namibia",
                                                     6377483.865, 299.1528128);
    spheroids[4].SetValuesByEqRadiusAndInvFlattening("bessel-1841", 6377397.155,
                                                     299.1528128);
    spheroids[5].SetValuesByEqRadiusAndInvFlattening("clarke-1858", 6378294.0,
                                                     294.297);
    spheroids[6].SetValuesByEqRadiusAndInvFlattening("clarke-1866", 6378206.4,
                                                     294.9786982);
    spheroids[7].SetValuesByEqRadiusAndInvFlattening("clarke-1880", 6378249.145,
                                                     293.465);
    spheroids[8].SetValuesByEqRadiusAndInvFlattening("everest-india-1830",
                                                     6377276.345, 300.8017);
    spheroids[9].SetValuesByEqRadiusAndInvFlattening("everest-sabah-sarawak",
                                                     6377298.556, 300.8017);
    spheroids[10].SetValuesByEqRadiusAndInvFlattening("everest-india-1956",
                                                      6377301.243, 300.8017);
    spheroids[11].SetValuesByEqRadiusAndInvFlattening("everest-malaysia-1969",
                                                      6377295.664, 300.8017);
    spheroids[12].SetValuesByEqRadiusAndInvFlattening("everest-malay-sing",
                                                      6377304.063, 300.8017);
    spheroids[13].SetValuesByEqRadiusAndInvFlattening("everest-pakistan",
                                                      6377309.613, 300.8017);
    spheroids[14].SetValuesByEqRadiusAndInvFlattening("modified-fisher-1960",
                                                      6378155, 298.3);
    spheroids[15].SetValuesByEqRadiusAndInvFlattening("helmert-1906", 6378200,
                                                      298.3);
    spheroids[16].SetValuesByEqRadiusAndInvFlattening("hough-1960", 6378270,
                                                      297);
    spheroids[17].SetValuesByEqRadiusAndInvFlattening("hughes", 6378273.0,
                                                      298.279);
    spheroids[18].SetValuesByEqRadiusAndInvFlattening("indonesian-1974",
                                                      6378160, 298.247);
    spheroids[19].SetValuesByEqRadiusAndInvFlattening("international-1924",
                                                      6378388, 297);
    spheroids[20].SetValuesByEqRadiusAndInvFlattening("iugc-67", 6378160.0,
                                                      298.254);
    spheroids[21].SetValuesByEqRadiusAndInvFlattening("iugc-75", 6378140.0,
                                                      298.25298);
    spheroids[22].SetValuesByEqRadiusAndInvFlattening("krassovsky-1940",
                                                      6378245, 298.3);
    spheroids[23].SetValuesByEqRadiusAndInvFlattening("kaula", 6378165.0,
                                                      292.308);
    spheroids[24].SetValuesByEqRadiusAndInvFlattening("grs-80", 6378137,
                                                      298.257222101);
    spheroids[25].SetValuesByEqRadiusAndInvFlattening("south-american-1969",
                                                      6378160, 298.25);
    spheroids[26].SetValuesByEqRadiusAndInvFlattening("wgs-72", 6378135,
                                                      298.26);
    spheroids[27].SetValuesByEqRadiusAndInvFlattening("wgs-84", 6378137,
                                                      298.257223563);
    spheroids[28].SetValuesByEqRadiusAndInvFlattening("ev-wgs-84", 6378137.0,
                                                      298.252841);
    spheroids[29].SetValuesByEqRadiusAndInvFlattening("ev-bessel", 6377397.0,
                                                      299.1976073);

    spheroids[30].SetValuesByEqRadiusAndInvFlattening("airy_1830", 6377563.396,
                                                      299.3249646);
    spheroids[31].SetValuesByEqRadiusAndInvFlattening("modified_airy",
                                                      6377340.189, 299.3249646);
    spheroids[32].SetValuesByEqRadiusAndInvFlattening("australian_national",
                                                      6378160, 298.25);
    spheroids[33].SetValuesByEqRadiusAndInvFlattening("bessel_1841_namibia",
                                                      6377483.865, 299.1528128);
    spheroids[34].SetValuesByEqRadiusAndInvFlattening("bessel_1841",
                                                      6377397.155, 299.1528128);
    spheroids[35].SetValuesByEqRadiusAndInvFlattening("clarke_1858", 6378294.0,
                                                      294.297);
    spheroids[36].SetValuesByEqRadiusAndInvFlattening("clarke_1866", 6378206.4,
                                                      294.9786982);
    spheroids[37].SetValuesByEqRadiusAndInvFlattening("clarke_1880",
                                                      6378249.145, 293.465);
    spheroids[38].SetValuesByEqRadiusAndInvFlattening("everest_india_1830",
                                                      6377276.345, 300.8017);
    spheroids[39].SetValuesByEqRadiusAndInvFlattening("everest_sabah_sarawak",
                                                      6377298.556, 300.8017);
    spheroids[40].SetValuesByEqRadiusAndInvFlattening("everest_india_1956",
                                                      6377301.243, 300.8017);
    spheroids[41].SetValuesByEqRadiusAndInvFlattening("everest_malaysia_1969",
                                                      6377295.664, 300.8017);
    spheroids[42].SetValuesByEqRadiusAndInvFlattening("everest_malay_sing",
                                                      6377304.063, 300.8017);
    spheroids[43].SetValuesByEqRadiusAndInvFlattening("everest_pakistan",
                                                      6377309.613, 300.8017);
    spheroids[44].SetValuesByEqRadiusAndInvFlattening("modified_fisher_1960",
                                                      6378155, 298.3);
    spheroids[45].SetValuesByEqRadiusAndInvFlattening("helmert_1906", 6378200,
                                                      298.3);
    spheroids[46].SetValuesByEqRadiusAndInvFlattening("hough_1960", 6378270,
                                                      297);
    spheroids[47].SetValuesByEqRadiusAndInvFlattening("indonesian_1974",
                                                      6378160, 298.247);
    spheroids[48].SetValuesByEqRadiusAndInvFlattening("international_1924",
                                                      6378388, 297);
    spheroids[49].SetValuesByEqRadiusAndInvFlattening("iugc_67", 6378160.0,
                                                      298.254);
    spheroids[50].SetValuesByEqRadiusAndInvFlattening("iugc_75", 6378140.0,
                                                      298.25298);
    spheroids[51].SetValuesByEqRadiusAndInvFlattening("krassovsky_1940",
                                                      6378245, 298.3);
    spheroids[52].SetValuesByEqRadiusAndInvFlattening("grs_80", 6378137,
                                                      298.257222101);
    spheroids[53].SetValuesByEqRadiusAndInvFlattening("south_american_1969",
                                                      6378160, 298.25);
    spheroids[54].SetValuesByEqRadiusAndInvFlattening("wgs_72", 6378135,
                                                      298.26);
    spheroids[55].SetValuesByEqRadiusAndInvFlattening("wgs_84", 6378137,
                                                      298.257223563);
    spheroids[56].SetValuesByEqRadiusAndInvFlattening("ev_wgs_84", 6378137.0,
                                                      298.252841);
    spheroids[57].SetValuesByEqRadiusAndInvFlattening("ev_bessel", 6377397.0,
                                                      299.1976073);
}

/************************************************************************/
/* ==================================================================== */
/*                              HKVDataset                              */
/* ==================================================================== */
/************************************************************************/

class HKVDataset final : public RawDataset
{
    friend class HKVRasterBand;

    char *pszPath;
    VSILFILE *fpBlob;

    int nGCPCount;
    GDAL_GCP *pasGCPList;

    void ProcessGeoref(const char *);
    void ProcessGeorefGCP(char **, const char *, double, double);

    void SetVersion(float version_number)
    {
        // Update stored info.
        MFF2version = version_number;
    }

    float MFF2version;

    GDALDataType eRasterType;

    void SetNoDataValue(double);

    OGRSpatialReference m_oSRS{};
    OGRSpatialReference m_oGCPSRS{};
    double adfGeoTransform[6];

    char **papszAttrib;

    char **papszGeoref;

    // NOTE: The MFF2 format goes against GDAL's API in that nodata values are
    // set per-dataset rather than per-band.  To compromise, for writing out,
    // the dataset's nodata value will be set to the last value set on any of
    // the raster bands.

    bool bNoDataSet;
    double dfNoDataValue;

    CPL_DISALLOW_COPY_ASSIGN(HKVDataset)

    CPLErr Close() override;

  public:
    HKVDataset();
    ~HKVDataset() override;

    int GetGCPCount() override /* const */
    {
        return nGCPCount;
    }

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
/*                            HKVRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HKVRasterBand()                            */
/************************************************************************/

HKVRasterBand::HKVRasterBand(HKVDataset *poDSIn, int nBandIn, VSILFILE *fpRawIn,
                             unsigned int nImgOffsetIn, int nPixelOffsetIn,
                             int nLineOffsetIn, GDALDataType eDataTypeIn,
                             int bNativeOrderIn)
    : RawRasterBand(GDALDataset::FromHandle(poDSIn), nBandIn, fpRawIn,
                    nImgOffsetIn, nPixelOffsetIn, nLineOffsetIn, eDataTypeIn,
                    bNativeOrderIn, RawRasterBand::OwnFP::NO)

{
    poDS = poDSIn;
    nBand = nBandIn;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/* ==================================================================== */
/*                              HKVDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            HKVDataset()                             */
/************************************************************************/

HKVDataset::HKVDataset()
    : pszPath(nullptr), fpBlob(nullptr), nGCPCount(0), pasGCPList(nullptr),
      // Initialize datasets to new version; change if necessary.
      MFF2version(1.1f), eRasterType(GDT_Unknown), papszAttrib(nullptr),
      papszGeoref(nullptr), bNoDataSet(false), dfNoDataValue(0.0)
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
/*                            ~HKVDataset()                            */
/************************************************************************/

HKVDataset::~HKVDataset()

{
    HKVDataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr HKVDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (HKVDataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        if (fpBlob)
        {
            if (VSIFCloseL(fpBlob) != 0)
            {
                eErr = CE_Failure;
                CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            }
        }

        if (nGCPCount > 0)
        {
            GDALDeinitGCPs(nGCPCount, pasGCPList);
            CPLFree(pasGCPList);
        }

        CPLFree(pszPath);
        CSLDestroy(papszGeoref);
        CSLDestroy(papszAttrib);

        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HKVDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return CE_None;
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *HKVDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          ProcessGeorefGCP()                          */
/************************************************************************/

void HKVDataset::ProcessGeorefGCP(char **papszGeorefIn, const char *pszBase,
                                  double dfRasterX, double dfRasterY)

{
    /* -------------------------------------------------------------------- */
    /*      Fetch the GCP from the string list.                             */
    /* -------------------------------------------------------------------- */
    char szFieldName[128] = {'\0'};
    snprintf(szFieldName, sizeof(szFieldName), "%s.latitude", pszBase);
    double dfLat = 0.0;
    if (CSLFetchNameValue(papszGeorefIn, szFieldName) == nullptr)
        return;
    else
        dfLat = CPLAtof(CSLFetchNameValue(papszGeorefIn, szFieldName));

    snprintf(szFieldName, sizeof(szFieldName), "%s.longitude", pszBase);
    double dfLong = 0.0;
    if (CSLFetchNameValue(papszGeorefIn, szFieldName) == nullptr)
        return;
    else
        dfLong = CPLAtof(CSLFetchNameValue(papszGeorefIn, szFieldName));

    /* -------------------------------------------------------------------- */
    /*      Add the gcp to the internal list.                               */
    /* -------------------------------------------------------------------- */
    GDALInitGCPs(1, pasGCPList + nGCPCount);

    CPLFree(pasGCPList[nGCPCount].pszId);

    pasGCPList[nGCPCount].pszId = CPLStrdup(pszBase);

    pasGCPList[nGCPCount].dfGCPX = dfLong;
    pasGCPList[nGCPCount].dfGCPY = dfLat;
    pasGCPList[nGCPCount].dfGCPZ = 0.0;

    pasGCPList[nGCPCount].dfGCPPixel = dfRasterX;
    pasGCPList[nGCPCount].dfGCPLine = dfRasterY;

    nGCPCount++;
}

/************************************************************************/
/*                           ProcessGeoref()                            */
/************************************************************************/

void HKVDataset::ProcessGeoref(const char *pszFilename)

{
    /* -------------------------------------------------------------------- */
    /*      Load the georef file, and boil white space away from around     */
    /*      the equal sign.                                                 */
    /* -------------------------------------------------------------------- */
    CSLDestroy(papszGeoref);
    papszGeoref = CSLLoad(pszFilename);
    if (papszGeoref == nullptr)
        return;

    HKVSpheroidList *hkvEllipsoids = new HKVSpheroidList;

    for (int i = 0; papszGeoref[i] != nullptr; i++)
    {
        int iDst = 0;
        char *pszLine = papszGeoref[i];

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
    /*      Try to get GCPs, in lat/longs                     .             */
    /* -------------------------------------------------------------------- */
    nGCPCount = 0;
    pasGCPList = reinterpret_cast<GDAL_GCP *>(CPLCalloc(sizeof(GDAL_GCP), 5));

    if (MFF2version > 1.0)
    {
        ProcessGeorefGCP(papszGeoref, "top_left", 0, 0);
        ProcessGeorefGCP(papszGeoref, "top_right", GetRasterXSize(), 0);
        ProcessGeorefGCP(papszGeoref, "bottom_left", 0, GetRasterYSize());
        ProcessGeorefGCP(papszGeoref, "bottom_right", GetRasterXSize(),
                         GetRasterYSize());
        ProcessGeorefGCP(papszGeoref, "centre", GetRasterXSize() / 2.0,
                         GetRasterYSize() / 2.0);
    }
    else
    {
        ProcessGeorefGCP(papszGeoref, "top_left", 0.5, 0.5);
        ProcessGeorefGCP(papszGeoref, "top_right", GetRasterXSize() - 0.5, 0.5);
        ProcessGeorefGCP(papszGeoref, "bottom_left", 0.5,
                         GetRasterYSize() - 0.5);
        ProcessGeorefGCP(papszGeoref, "bottom_right", GetRasterXSize() - 0.5,
                         GetRasterYSize() - 0.5);
        ProcessGeorefGCP(papszGeoref, "centre", GetRasterXSize() / 2.0,
                         GetRasterYSize() / 2.0);
    }

    if (nGCPCount == 0)
    {
        CPLFree(pasGCPList);
        pasGCPList = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have a recognised projection?                             */
    /* -------------------------------------------------------------------- */
    const char *pszProjName = CSLFetchNameValue(papszGeoref, "projection.name");
    const char *pszOriginLong =
        CSLFetchNameValue(papszGeoref, "projection.origin_longitude");
    const char *pszSpheroidName =
        CSLFetchNameValue(papszGeoref, "spheroid.name");

    if (pszSpheroidName != nullptr &&
        hkvEllipsoids->SpheroidInList(pszSpheroidName))
    {
#if 0
      // TODO(schwehr): Enable in trunk after 2.1 branch and fix.
      // Breaks tests on some platforms.
      CPLError( CE_Failure, CPLE_AppDefined,
                "Unrecognized ellipsoid.  Not handled.  "
                "Spheroid name not in spheroid list: '%s'",
                pszSpheroidName );
#endif
        // Why were eq_radius and inv_flattening never used?
        // eq_radius = hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName);
        // inv_flattening =
        //     hkvEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName);
    }
    else if (pszProjName != nullptr)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unrecognized ellipsoid.  Not handled.");
        // TODO(schwehr): This error is was never what was happening.
        // CPLError( CE_Warning, CPLE_AppDefined,
        //           "Unrecognized ellipsoid.  Using wgs-84 parameters.");
        // eq_radius=hkvEllipsoids->GetSpheroidEqRadius("wgs-84"); */
        // inv_flattening=hkvEllipsoids->GetSpheroidInverseFlattening("wgs-84");
    }

    if (pszProjName != nullptr && EQUAL(pszProjName, "utm") && nGCPCount == 5)
    {
        // int nZone = (int)((CPLAtof(pszOriginLong)+184.5) / 6.0);
        int nZone = 31;  // TODO(schwehr): Where does 31 come from?

        if (pszOriginLong == nullptr)
        {
            // If origin not specified, assume 0.0.
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "No projection origin longitude specified.  Assuming 0.0.");
        }
        else
        {
            nZone = 31 + static_cast<int>(floor(CPLAtof(pszOriginLong) / 6.0));
        }

        OGRSpatialReference oUTM;

        if (pasGCPList[4].dfGCPY < 0)
            oUTM.SetUTM(nZone, 0);
        else
            oUTM.SetUTM(nZone, 1);

        OGRSpatialReference oLL;
        oLL.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (pszOriginLong != nullptr)
        {
            oUTM.SetProjParm(SRS_PP_CENTRAL_MERIDIAN, CPLAtof(pszOriginLong));
            oLL.SetProjParm(SRS_PP_LONGITUDE_OF_ORIGIN, CPLAtof(pszOriginLong));
        }

        if ((pszSpheroidName == nullptr) ||
            (EQUAL(pszSpheroidName, "wgs-84")) ||
            (EQUAL(pszSpheroidName, "wgs_84")))
        {
            oUTM.SetWellKnownGeogCS("WGS84");
            oLL.SetWellKnownGeogCS("WGS84");
        }
        else
        {
            if (hkvEllipsoids->SpheroidInList(pszSpheroidName))
            {
                oUTM.SetGeogCS(
                    "unknown", "unknown", pszSpheroidName,
                    hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName),
                    hkvEllipsoids->GetSpheroidInverseFlattening(
                        pszSpheroidName));
                oLL.SetGeogCS(
                    "unknown", "unknown", pszSpheroidName,
                    hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName),
                    hkvEllipsoids->GetSpheroidInverseFlattening(
                        pszSpheroidName));
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Unrecognized ellipsoid.  Using wgs-84 parameters.");
                oUTM.SetWellKnownGeogCS("WGS84");
                oLL.SetWellKnownGeogCS("WGS84");
            }
        }

        OGRCoordinateTransformation *poTransform =
            OGRCreateCoordinateTransformation(&oLL, &oUTM);

        bool bSuccess = true;
        if (poTransform == nullptr)
        {
            CPLErrorReset();
            bSuccess = false;
        }

        double dfUtmX[5] = {0.0};
        double dfUtmY[5] = {0.0};

        if (poTransform != nullptr)
        {
            for (int gcp_index = 0; gcp_index < 5; gcp_index++)
            {
                dfUtmX[gcp_index] = pasGCPList[gcp_index].dfGCPX;
                dfUtmY[gcp_index] = pasGCPList[gcp_index].dfGCPY;

                if (bSuccess && !poTransform->Transform(1, &(dfUtmX[gcp_index]),
                                                        &(dfUtmY[gcp_index])))
                    bSuccess = false;
            }
        }

        if (bSuccess)
        {
            // Update GCPS to proper projection.
            for (int gcp_index = 0; gcp_index < 5; gcp_index++)
            {
                pasGCPList[gcp_index].dfGCPX = dfUtmX[gcp_index];
                pasGCPList[gcp_index].dfGCPY = dfUtmY[gcp_index];
            }

            m_oGCPSRS = oUTM;

            bool transform_ok = CPL_TO_BOOL(
                GDALGCPsToGeoTransform(5, pasGCPList, adfGeoTransform, 0));

            if (!transform_ok)
            {
                // Transform may not be sufficient in all cases (slant range
                // projection).
                adfGeoTransform[0] = 0.0;
                adfGeoTransform[1] = 1.0;
                adfGeoTransform[2] = 0.0;
                adfGeoTransform[3] = 0.0;
                adfGeoTransform[4] = 0.0;
                adfGeoTransform[5] = 1.0;
                m_oGCPSRS.Clear();
            }
            else
            {
                m_oSRS = std::move(oUTM);
            }
        }

        if (poTransform != nullptr)
            delete poTransform;
    }
    else if (pszProjName != nullptr && nGCPCount == 5)
    {
        OGRSpatialReference oLL;
        oLL.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        if (pszOriginLong != nullptr)
        {
            oLL.SetProjParm(SRS_PP_LONGITUDE_OF_ORIGIN, CPLAtof(pszOriginLong));
        }

        if (pszSpheroidName == nullptr ||
            EQUAL(pszSpheroidName, "wgs-84") ||  // Dash.
            EQUAL(pszSpheroidName, "wgs_84"))    // Underscore.
        {
            oLL.SetWellKnownGeogCS("WGS84");
        }
        else
        {
            if (hkvEllipsoids->SpheroidInList(pszSpheroidName))
            {
                oLL.SetGeogCS(
                    "", "", pszSpheroidName,
                    hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName),
                    hkvEllipsoids->GetSpheroidInverseFlattening(
                        pszSpheroidName));
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Unrecognized ellipsoid.  "
                         "Using wgs-84 parameters.");
                oLL.SetWellKnownGeogCS("WGS84");
            }
        }

        const bool transform_ok = CPL_TO_BOOL(
            GDALGCPsToGeoTransform(5, pasGCPList, adfGeoTransform, 0));

        m_oSRS.Clear();

        if (!transform_ok)
        {
            adfGeoTransform[0] = 0.0;
            adfGeoTransform[1] = 1.0;
            adfGeoTransform[2] = 0.0;
            adfGeoTransform[3] = 0.0;
            adfGeoTransform[4] = 0.0;
            adfGeoTransform[5] = 1.0;
        }
        else
        {
            m_oSRS = oLL;
        }

        m_oGCPSRS = std::move(oLL);
    }

    delete hkvEllipsoids;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HKVDataset::Open(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      We assume the dataset is passed as a directory.  Check for      */
    /*      an attrib and blob file as a minimum.                           */
    /* -------------------------------------------------------------------- */
    if (!poOpenInfo->bIsDirectory)
        return nullptr;

    std::string osFilename =
        CPLFormFilenameSafe(poOpenInfo->pszFilename, "image_data", nullptr);
    VSIStatBuf sStat;
    if (VSIStat(osFilename.c_str(), &sStat) != 0)
        osFilename =
            CPLFormFilenameSafe(poOpenInfo->pszFilename, "blob", nullptr);
    if (VSIStat(osFilename.c_str(), &sStat) != 0)
        return nullptr;

    osFilename =
        CPLFormFilenameSafe(poOpenInfo->pszFilename, "attrib", nullptr);
    if (VSIStat(osFilename.c_str(), &sStat) != 0)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Load the attrib file, and boil white space away from around     */
    /*      the equal sign.                                                 */
    /* -------------------------------------------------------------------- */
    char **papszAttrib = CSLLoad(osFilename.c_str());
    if (papszAttrib == nullptr)
        return nullptr;

    for (int i = 0; papszAttrib[i] != nullptr; i++)
    {
        int iDst = 0;
        char *pszLine = papszAttrib[i];

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
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<HKVDataset>();

    poDS->pszPath = CPLStrdup(poOpenInfo->pszFilename);
    poDS->papszAttrib = papszAttrib;

    poDS->eAccess = poOpenInfo->eAccess;

    /* -------------------------------------------------------------------- */
    /*      Set some dataset wide information.                              */
    /* -------------------------------------------------------------------- */
    bool bNative = false;
    bool bComplex = false;
    int nRawBands = 0;

    if (CSLFetchNameValue(papszAttrib, "extent.cols") == nullptr ||
        CSLFetchNameValue(papszAttrib, "extent.rows") == nullptr)
    {
        return nullptr;
    }

    poDS->nRasterXSize = atoi(CSLFetchNameValue(papszAttrib, "extent.cols"));
    poDS->nRasterYSize = atoi(CSLFetchNameValue(papszAttrib, "extent.rows"));

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    const char *pszValue = CSLFetchNameValue(papszAttrib, "pixel.order");
    if (pszValue == nullptr)
        bNative = true;
    else
    {
#ifdef CPL_MSB
        bNative = strstr(pszValue, "*msbf") != NULL;
#else
        bNative = strstr(pszValue, "*lsbf") != nullptr;
#endif
    }

    bool bNoDataSet = false;
    double dfNoDataValue = 0.0;
    pszValue = CSLFetchNameValue(papszAttrib, "pixel.no_data");
    if (pszValue != nullptr)
    {
        bNoDataSet = true;
        dfNoDataValue = CPLAtof(pszValue);
    }

    pszValue = CSLFetchNameValue(papszAttrib, "channel.enumeration");
    if (pszValue != nullptr)
        nRawBands = atoi(pszValue);
    else
        nRawBands = 1;

    if (!GDALCheckBandCount(nRawBands, TRUE))
    {
        return nullptr;
    }

    pszValue = CSLFetchNameValue(papszAttrib, "pixel.field");
    if (pszValue != nullptr && strstr(pszValue, "*complex") != nullptr)
        bComplex = true;
    else
        bComplex = false;

    /* Get the version number, if present (if not, assume old version. */
    /* Versions differ in their interpretation of corner coordinates.  */

    if (CSLFetchNameValue(papszAttrib, "version") != nullptr)
        poDS->SetVersion(static_cast<float>(
            CPLAtof(CSLFetchNameValue(papszAttrib, "version"))));
    else
        poDS->SetVersion(1.0);

    /* -------------------------------------------------------------------- */
    /*      Figure out the datatype                                         */
    /* -------------------------------------------------------------------- */
    const char *pszEncoding = CSLFetchNameValue(papszAttrib, "pixel.encoding");
    if (pszEncoding == nullptr)
        pszEncoding = "{ *unsigned }";

    int nSize = 1;
    if (CSLFetchNameValue(papszAttrib, "pixel.size") != nullptr)
        nSize = atoi(CSLFetchNameValue(papszAttrib, "pixel.size")) / 8;
#if 0
    int nPseudoBands;
    if( bComplex )
        nPseudoBands = 2;
    else
        nPseudoBands = 1;
#endif

    GDALDataType eType;
    if (nSize == 1)
        eType = GDT_Byte;
    else if (nSize == 2 && strstr(pszEncoding, "*unsigned") != nullptr)
        eType = GDT_UInt16;
    else if (nSize == 4 && bComplex)
        eType = GDT_CInt16;
    else if (nSize == 2)
        eType = GDT_Int16;
    else if (nSize == 4 && strstr(pszEncoding, "*unsigned") != nullptr)
        eType = GDT_UInt32;
    else if (nSize == 8 && strstr(pszEncoding, "*two") != nullptr && bComplex)
        eType = GDT_CInt32;
    else if (nSize == 4 && strstr(pszEncoding, "*two") != nullptr)
        eType = GDT_Int32;
    else if (nSize == 8 && bComplex)
        eType = GDT_CFloat32;
    else if (nSize == 4)
        eType = GDT_Float32;
    else if (nSize == 16 && bComplex)
        eType = GDT_CFloat64;
    else if (nSize == 8)
        eType = GDT_Float64;
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported pixel data type in %s.\n"
                 "pixel.size=%d pixel.encoding=%s",
                 poDS->pszPath, nSize, pszEncoding);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Open the blob file.                                             */
    /* -------------------------------------------------------------------- */
    osFilename = CPLFormFilenameSafe(poDS->pszPath, "image_data", nullptr);
    if (VSIStat(osFilename.c_str(), &sStat) != 0)
        osFilename = CPLFormFilenameSafe(poDS->pszPath, "blob", nullptr);
    if (poOpenInfo->eAccess == GA_ReadOnly)
    {
        poDS->fpBlob = VSIFOpenL(osFilename.c_str(), "rb");
        if (poDS->fpBlob == nullptr)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Unable to open file %s for read access.",
                     osFilename.c_str());
            return nullptr;
        }
    }
    else
    {
        poDS->fpBlob = VSIFOpenL(osFilename.c_str(), "rb+");
        if (poDS->fpBlob == nullptr)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Unable to open file %s for update access.",
                     osFilename.c_str());
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Build the overview filename, as blob file = "_ovr".             */
    /* -------------------------------------------------------------------- */
    std::string osOvrFilename(osFilename);
    osOvrFilename += "_ovr";

    /* -------------------------------------------------------------------- */
    /*      Define the bands.                                               */
    /* -------------------------------------------------------------------- */
    const int nPixelOffset = nRawBands * nSize;
    const int nLineOffset = nPixelOffset * poDS->GetRasterXSize();
    int nOffset = 0;

    for (int iRawBand = 0; iRawBand < nRawBands; iRawBand++)
    {
        auto poBand = std::make_unique<HKVRasterBand>(
            poDS.get(), poDS->GetRasterCount() + 1, poDS->fpBlob, nOffset,
            nPixelOffset, nLineOffset, eType, bNative);
        if (!poBand->IsValid())
            return nullptr;

        if (bNoDataSet)
            poBand->SetNoDataValue(dfNoDataValue);
        poDS->SetBand(poDS->GetRasterCount() + 1, std::move(poBand));
        nOffset += GDALGetDataTypeSizeBytes(eType);
    }

    poDS->eRasterType = eType;

    /* -------------------------------------------------------------------- */
    /*      Process the georef file if there is one.                        */
    /* -------------------------------------------------------------------- */
    osFilename = CPLFormFilenameSafe(poDS->pszPath, "georef", nullptr);
    if (VSIStat(osFilename.c_str(), &sStat) == 0)
        poDS->ProcessGeoref(osFilename.c_str());

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(osOvrFilename.c_str());
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Handle overviews.                                               */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), osOvrFilename.c_str(), nullptr,
                                TRUE);

    return poDS.release();
}

/************************************************************************/
/*                         GDALRegister_HKV()                           */
/************************************************************************/

void GDALRegister_HKV()

{
    if (GDALGetDriverByName("MFF2") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MFF2");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Vexcel MFF2 (HKV) Raster");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/mff2.html");

    poDriver->pfnOpen = HKVDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
