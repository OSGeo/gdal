/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S100 bathymetric datasets.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_time.h"

#include "s100.h"
#include "hdf5dataset.h"
#include "gh5_convenience.h"

#include "proj.h"
#include "proj_experimental.h"
#include "proj_constants.h"
#include "ogr_proj_p.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

/************************************************************************/
/*                          S100BaseDataset()                           */
/************************************************************************/

S100BaseDataset::S100BaseDataset(const std::string &osFilename)
    : m_osFilename(osFilename)
{
}

/************************************************************************/
/*                                Init()                                */
/************************************************************************/

bool S100BaseDataset::Init()
{
    // Open the file as an HDF5 file.
    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_driver(fapl, HDF5GetFileDriver(), nullptr);
    hid_t hHDF5 = H5Fopen(m_osFilename.c_str(), H5F_ACC_RDONLY, fapl);
    H5Pclose(fapl);
    if (hHDF5 < 0)
        return false;

    auto poSharedResources = GDAL::HDF5SharedResources::Create(m_osFilename);
    poSharedResources->m_hHDF5 = hHDF5;

    m_poRootGroup = HDF5Dataset::OpenGroup(poSharedResources);
    if (m_poRootGroup == nullptr)
        return false;

    S100ReadSRS(m_poRootGroup.get(), m_oSRS);

    S100ReadVerticalDatum(this, m_poRootGroup.get());

    m_osMetadataFile =
        S100ReadMetadata(this, m_osFilename, m_poRootGroup.get());

    return true;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr S100BaseDataset::GetGeoTransform(GDALGeoTransform &gt) const

{
    if (m_bHasGT)
    {
        gt = m_gt;
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(gt);
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *S100BaseDataset::GetSpatialRef() const
{
    if (!m_oSRS.IsEmpty())
        return &m_oSRS;
    return GDALPamDataset::GetSpatialRef();
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **S100BaseDataset::GetFileList()
{
    char **papszFileList = GDALPamDataset::GetFileList();
    if (!m_osMetadataFile.empty())
        papszFileList = CSLAddString(papszFileList, m_osMetadataFile.c_str());
    return papszFileList;
}

/************************************************************************/
/*                            S100ReadSRS()                             */
/************************************************************************/

constexpr int PROJECTION_METHOD_MERCATOR = 9805;
static_assert(PROJECTION_METHOD_MERCATOR ==
              EPSG_CODE_METHOD_MERCATOR_VARIANT_B);
constexpr int PROJECTION_METHOD_TRANSVERSE_MERCATOR = 9807;
static_assert(PROJECTION_METHOD_TRANSVERSE_MERCATOR ==
              EPSG_CODE_METHOD_TRANSVERSE_MERCATOR);
constexpr int PROJECTION_METHOD_OBLIQUE_MERCATOR = 9815;
static_assert(PROJECTION_METHOD_OBLIQUE_MERCATOR ==
              EPSG_CODE_METHOD_HOTINE_OBLIQUE_MERCATOR_VARIANT_B);
constexpr int PROJECTION_METHOD_HOTINE_OBLIQUE_MERCATOR = 9812;
static_assert(PROJECTION_METHOD_HOTINE_OBLIQUE_MERCATOR ==
              EPSG_CODE_METHOD_HOTINE_OBLIQUE_MERCATOR_VARIANT_A);
constexpr int PROJECTION_METHOD_LCC_1SP = 9801;
static_assert(PROJECTION_METHOD_LCC_1SP ==
              EPSG_CODE_METHOD_LAMBERT_CONIC_CONFORMAL_1SP);
constexpr int PROJECTION_METHOD_LCC_2SP = 9802;
static_assert(PROJECTION_METHOD_LCC_2SP ==
              EPSG_CODE_METHOD_LAMBERT_CONIC_CONFORMAL_2SP);
constexpr int PROJECTION_METHOD_OBLIQUE_STEREOGRAPHIC = 9809;
static_assert(PROJECTION_METHOD_OBLIQUE_STEREOGRAPHIC ==
              EPSG_CODE_METHOD_OBLIQUE_STEREOGRAPHIC);
constexpr int PROJECTION_METHOD_POLAR_STEREOGRAPHIC = 9810;
static_assert(PROJECTION_METHOD_POLAR_STEREOGRAPHIC ==
              EPSG_CODE_METHOD_POLAR_STEREOGRAPHIC_VARIANT_A);
constexpr int PROJECTION_METHOD_KROVAK_OBLIQUE_CONIC_CONFORMAL = 9819;
static_assert(PROJECTION_METHOD_KROVAK_OBLIQUE_CONIC_CONFORMAL ==
              EPSG_CODE_METHOD_KROVAK);
constexpr int PROJECTION_METHOD_AMERICAN_POLYCONIC = 9818;
static_assert(PROJECTION_METHOD_AMERICAN_POLYCONIC ==
              EPSG_CODE_METHOD_AMERICAN_POLYCONIC);
constexpr int PROJECTION_METHOD_ALBERS_EQUAL_AREA = 9822;
static_assert(PROJECTION_METHOD_ALBERS_EQUAL_AREA ==
              EPSG_CODE_METHOD_ALBERS_EQUAL_AREA);
constexpr int PROJECTION_METHOD_LAMBERT_AZIMUTHAL_EQUAL_AREA = 9820;
static_assert(PROJECTION_METHOD_LAMBERT_AZIMUTHAL_EQUAL_AREA ==
              EPSG_CODE_METHOD_LAMBERT_AZIMUTHAL_EQUAL_AREA);

bool S100ReadSRS(const GDALGroup *poRootGroup, OGRSpatialReference &oSRS)
{
    // Get SRS
    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    auto poHorizontalCRS = poRootGroup->GetAttribute("horizontalCRS");
    if (poHorizontalCRS &&
        poHorizontalCRS->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        // horizontalCRS is v2.2
        const int nHorizontalCRS = poHorizontalCRS->ReadAsInt();
        if (nHorizontalCRS > 0)
        {
            if (oSRS.importFromEPSG(nHorizontalCRS) != OGRERR_NONE)
            {
                oSRS.Clear();
            }
        }
        else
        {
            auto poNameOfHorizontalCRS =
                poRootGroup->GetAttribute("nameOfHorizontalCRS");
            auto poTypeOfHorizontalCRS =
                poRootGroup->GetAttribute("typeOfHorizontalCRS");
            auto poHorizontalCS = poRootGroup->GetAttribute("horizontalCS");
            auto poHorizontalDatum =
                poRootGroup->GetAttribute("horizontalDatum");
            if (poNameOfHorizontalCRS &&
                poNameOfHorizontalCRS->GetDataType().GetClass() ==
                    GEDTC_STRING &&
                poTypeOfHorizontalCRS &&
                poTypeOfHorizontalCRS->GetDataType().GetClass() ==
                    GEDTC_NUMERIC &&
                poHorizontalCS &&
                poHorizontalCS->GetDataType().GetClass() == GEDTC_NUMERIC &&
                poHorizontalDatum &&
                poHorizontalDatum->GetDataType().GetClass() == GEDTC_NUMERIC)
            {
                const int nCRSType = poTypeOfHorizontalCRS->ReadAsInt();
                constexpr int CRS_TYPE_GEOGRAPHIC = 1;
                constexpr int CRS_TYPE_PROJECTED = 2;

                PJ *crs = nullptr;
                const char *pszCRSName = poNameOfHorizontalCRS->ReadAsString();

                const int nHorizontalCS = poHorizontalCS->ReadAsInt();
                const int nHorizontalDatum = poHorizontalDatum->ReadAsInt();

                auto pjCtxt = OSRGetProjTLSContext();
                if (nHorizontalDatum < 0)
                {
                    auto poNameOfHorizontalDatum =
                        poRootGroup->GetAttribute("nameOfHorizontalDatum");
                    auto poPrimeMeridian =
                        poRootGroup->GetAttribute("primeMeridian");
                    auto poSpheroid = poRootGroup->GetAttribute("spheroid");
                    if (poNameOfHorizontalDatum &&
                        poNameOfHorizontalDatum->GetDataType().GetClass() ==
                            GEDTC_STRING &&
                        poPrimeMeridian &&
                        poPrimeMeridian->GetDataType().GetClass() ==
                            GEDTC_NUMERIC &&
                        poSpheroid &&
                        poSpheroid->GetDataType().GetClass() == GEDTC_NUMERIC)
                    {
                        const char *pszDatumName =
                            poNameOfHorizontalDatum->ReadAsString();
                        const char *pszGeogCRSName =
                            nCRSType == CRS_TYPE_PROJECTED
                                ? (pszDatumName ? pszDatumName : "unknown")
                                : (pszCRSName ? pszCRSName : "unknown");

                        const int nPrimeMeridian = poPrimeMeridian->ReadAsInt();
                        const int nSpheroid = poSpheroid->ReadAsInt();
                        PJ *prime_meridian = proj_create_from_database(
                            pjCtxt, "EPSG", CPLSPrintf("%d", nPrimeMeridian),
                            PJ_CATEGORY_PRIME_MERIDIAN, false, nullptr);
                        if (!prime_meridian)
                            return false;
                        PJ *spheroid = proj_create_from_database(
                            pjCtxt, "EPSG", CPLSPrintf("%d", nSpheroid),
                            PJ_CATEGORY_ELLIPSOID, false, nullptr);
                        if (!spheroid)
                        {
                            proj_destroy(prime_meridian);
                            return false;
                        }

                        double semiMajorMetre = 0;
                        double invFlattening = 0;
                        proj_ellipsoid_get_parameters(pjCtxt, spheroid,
                                                      &semiMajorMetre, nullptr,
                                                      nullptr, &invFlattening);

                        double primeMeridianOffset = 0;
                        double primeMeridianUnitConv = 1;
                        const char *primeMeridianUnitName = nullptr;
                        proj_prime_meridian_get_parameters(
                            pjCtxt, prime_meridian, &primeMeridianOffset,
                            &primeMeridianUnitConv, &primeMeridianUnitName);

                        PJ *ellipsoidalCS = proj_create_ellipsoidal_2D_cs(
                            pjCtxt, PJ_ELLPS2D_LATITUDE_LONGITUDE, nullptr,
                            0.0);

                        crs = proj_create_geographic_crs(
                            pjCtxt, pszGeogCRSName,
                            pszDatumName ? pszDatumName : "unknown",
                            proj_get_name(spheroid), semiMajorMetre,
                            invFlattening, proj_get_name(prime_meridian),
                            primeMeridianOffset, primeMeridianUnitName,
                            primeMeridianUnitConv, ellipsoidalCS);

                        proj_destroy(ellipsoidalCS);
                        proj_destroy(spheroid);
                        proj_destroy(prime_meridian);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot establish CRS because "
                                 "nameOfHorizontalDatum, primeMeridian and/or "
                                 "spheroid are missing");
                        return false;
                    }
                }
                else
                {
                    PJ *datum = proj_create_from_database(
                        pjCtxt, "EPSG", CPLSPrintf("%d", nHorizontalDatum),
                        PJ_CATEGORY_DATUM, false, nullptr);
                    if (!datum)
                        return false;
                    const char *pszDatumName = proj_get_name(datum);
                    const char *pszGeogCRSName =
                        nCRSType == CRS_TYPE_PROJECTED
                            ? pszDatumName
                            : (pszCRSName ? pszCRSName : "unknown");

                    PJ *ellipsoidalCS = proj_create_ellipsoidal_2D_cs(
                        pjCtxt, PJ_ELLPS2D_LATITUDE_LONGITUDE, nullptr, 0.0);

                    crs = proj_create_geographic_crs_from_datum(
                        pjCtxt, pszGeogCRSName, datum, ellipsoidalCS);

                    proj_destroy(ellipsoidalCS);
                    proj_destroy(datum);
                }

                if (nCRSType == CRS_TYPE_PROJECTED)
                {
                    auto poProjectionMethod =
                        poRootGroup->GetAttribute("projectionMethod");
                    if (poProjectionMethod &&
                        poProjectionMethod->GetDataType().GetClass() ==
                            GEDTC_NUMERIC)
                    {
                        const int nProjectionMethod =
                            poProjectionMethod->ReadAsInt();

                        auto poFalseEasting =
                            poRootGroup->GetAttribute("falseEasting");
                        const double falseEasting =
                            poFalseEasting &&
                                    poFalseEasting->GetDataType().GetClass() ==
                                        GEDTC_NUMERIC
                                ? poFalseEasting->ReadAsDouble()
                                : 0.0;

                        auto poFalseNorthing =
                            poRootGroup->GetAttribute("falseNorthing");
                        const double falseNorthing =
                            poFalseNorthing &&
                                    poFalseNorthing->GetDataType().GetClass() ==
                                        GEDTC_NUMERIC
                                ? poFalseNorthing->ReadAsDouble()
                                : 0.0;

                        const auto getParameter = [poRootGroup](int nParam)
                        {
                            auto poParam = poRootGroup->GetAttribute(
                                "projectionParameter" + std::to_string(nParam));
                            if (poParam && poParam->GetDataType().GetClass() ==
                                               GEDTC_NUMERIC)
                                return poParam->ReadAsDouble();
                            CPLError(
                                CE_Warning, CPLE_AppDefined, "%s",
                                std::string(
                                    "Missing attribute projectionParameter")
                                    .append(std::to_string(nParam))
                                    .c_str());
                            return 0.0;
                        };

                        PJ *conv = nullptr;
                        switch (nProjectionMethod)
                        {
                            case PROJECTION_METHOD_MERCATOR:
                            {
                                conv =
                                    proj_create_conversion_mercator_variant_b(
                                        pjCtxt,
                                        /* latitude_first_parallel = */
                                        getParameter(1),
                                        /* center_long = */ getParameter(2),
                                        falseEasting, falseNorthing, nullptr,
                                        0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_TRANSVERSE_MERCATOR:
                            {
                                conv =
                                    proj_create_conversion_transverse_mercator(
                                        pjCtxt,
                                        /* center_lat = */ getParameter(1),
                                        /* center_long = */ getParameter(2),
                                        /* scale = */ getParameter(3),
                                        falseEasting, falseNorthing, nullptr,
                                        0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_OBLIQUE_MERCATOR:
                            {
                                conv =
                                    proj_create_conversion_hotine_oblique_mercator_variant_b(
                                        pjCtxt,
                                        /* latitude_projection_centre = */
                                        getParameter(1),
                                        /* longitude_projection_centre = */
                                        getParameter(2),
                                        /* azimuth_initial_line = */
                                        getParameter(3),
                                        /* angle_from_rectified_to_skrew_grid = */
                                        getParameter(4),
                                        /* scale = */ getParameter(5),
                                        falseEasting, falseNorthing, nullptr,
                                        0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_HOTINE_OBLIQUE_MERCATOR:
                            {
                                conv =
                                    proj_create_conversion_hotine_oblique_mercator_variant_a(
                                        pjCtxt,
                                        /* latitude_projection_centre = */
                                        getParameter(1),
                                        /* longitude_projection_centre = */
                                        getParameter(2),
                                        /* azimuth_initial_line = */
                                        getParameter(3),
                                        /* angle_from_rectified_to_skrew_grid = */
                                        getParameter(4),
                                        /* scale = */ getParameter(5),
                                        falseEasting, falseNorthing, nullptr,
                                        0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_LCC_1SP:
                            {
                                conv =
                                    proj_create_conversion_lambert_conic_conformal_1sp(
                                        pjCtxt,
                                        /* center_lat = */ getParameter(1),
                                        /* center_long = */ getParameter(2),
                                        /* scale = */ getParameter(3),
                                        falseEasting, falseNorthing, nullptr,
                                        0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_LCC_2SP:
                            {
                                conv =
                                    proj_create_conversion_lambert_conic_conformal_2sp(
                                        pjCtxt,
                                        /* latitude_false_origin = */
                                        getParameter(1),
                                        /* longitude_false_origin = */
                                        getParameter(2),
                                        /* latitude_first_parallel = */
                                        getParameter(3),
                                        /* latitude_second_parallel = */
                                        getParameter(4), falseEasting,
                                        falseNorthing, nullptr, 0.0, nullptr,
                                        0.0);
                                break;
                            }

                            case PROJECTION_METHOD_OBLIQUE_STEREOGRAPHIC:
                            {
                                conv =
                                    proj_create_conversion_oblique_stereographic(
                                        pjCtxt,
                                        /* center_lat = */ getParameter(1),
                                        /* center_long = */ getParameter(2),
                                        /* scale = */ getParameter(3),
                                        falseEasting, falseNorthing, nullptr,
                                        0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_POLAR_STEREOGRAPHIC:
                            {
                                conv =
                                    proj_create_conversion_polar_stereographic_variant_a(
                                        pjCtxt,
                                        /* center_lat = */ getParameter(1),
                                        /* center_long = */ getParameter(2),
                                        /* scale = */ getParameter(3),
                                        falseEasting, falseNorthing, nullptr,
                                        0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_KROVAK_OBLIQUE_CONIC_CONFORMAL:
                            {
                                conv = proj_create_conversion_krovak(
                                    pjCtxt,
                                    /* latitude_projection_centre = */
                                    getParameter(1),
                                    /* longitude_of_origin = */ getParameter(2),
                                    /* colatitude_cone_axis = */
                                    getParameter(3),
                                    /* latitude_pseudo_standard_parallel = */
                                    getParameter(4),
                                    /* scale_factor_pseudo_standard_parallel = */
                                    getParameter(5), falseEasting,
                                    falseNorthing, nullptr, 0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_AMERICAN_POLYCONIC:
                            {
                                conv =
                                    proj_create_conversion_american_polyconic(
                                        pjCtxt,
                                        /* center_lat = */ getParameter(1),
                                        /* center_long = */ getParameter(2),
                                        falseEasting, falseNorthing, nullptr,
                                        0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_ALBERS_EQUAL_AREA:
                            {
                                conv = proj_create_conversion_albers_equal_area(
                                    pjCtxt,
                                    /* latitude_false_origin = */
                                    getParameter(1),
                                    /* longitude_false_origin = */
                                    getParameter(2),
                                    /* latitude_first_parallel = */
                                    getParameter(3),
                                    /* latitude_second_parallel = */
                                    getParameter(4), falseEasting,
                                    falseNorthing, nullptr, 0.0, nullptr, 0.0);
                                break;
                            }

                            case PROJECTION_METHOD_LAMBERT_AZIMUTHAL_EQUAL_AREA:
                            {
                                conv =
                                    proj_create_conversion_lambert_azimuthal_equal_area(
                                        pjCtxt,
                                        /* latitude_nat_origin = */
                                        getParameter(1),
                                        /* longitude_nat_origin = */
                                        getParameter(2), falseEasting,
                                        falseNorthing, nullptr, 0.0, nullptr,
                                        0.0);
                                break;
                            }

                            default:
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Cannot establish CRS because "
                                         "projectionMethod = %d is unhandled",
                                         nProjectionMethod);
                                proj_destroy(crs);
                                return false;
                            }
                        }

                        constexpr int HORIZONTAL_CS_EASTING_NORTHING_METRE =
                            4400;
                        // constexpr int HORIZONTAL_CS_NORTHING_EASTING_METRE = 4500;
                        PJ *cartesianCS = proj_create_cartesian_2D_cs(
                            pjCtxt,
                            nHorizontalCS ==
                                    HORIZONTAL_CS_EASTING_NORTHING_METRE
                                ? PJ_CART2D_EASTING_NORTHING
                                : PJ_CART2D_NORTHING_EASTING,
                            nullptr, 0.0);

                        PJ *projCRS = proj_create_projected_crs(
                            pjCtxt, pszCRSName ? pszCRSName : "unknown", crs,
                            conv, cartesianCS);
                        proj_destroy(crs);
                        crs = projCRS;
                        proj_destroy(conv);
                        proj_destroy(cartesianCS);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot establish CRS because "
                                 "projectionMethod is missing");
                        proj_destroy(crs);
                        return false;
                    }
                }
                else if (nCRSType != CRS_TYPE_GEOGRAPHIC)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot establish CRS because of invalid value "
                             "for typeOfHorizontalCRS: %d",
                             nCRSType);
                    proj_destroy(crs);
                    return false;
                }

                const char *pszPROJJSON =
                    proj_as_projjson(pjCtxt, crs, nullptr);
                oSRS.SetFromUserInput(pszPROJJSON);

                proj_destroy(crs);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot establish CRS because nameOfHorizontalCRS, "
                         "typeOfHorizontalCRS, horizontalCS and/or "
                         "horizontalDatum are missing");
                return false;
            }
        }
    }
    else
    {
        auto poHorizontalDatumReference =
            poRootGroup->GetAttribute("horizontalDatumReference");
        auto poHorizontalDatumValue =
            poRootGroup->GetAttribute("horizontalDatumValue");
        if (poHorizontalDatumReference && poHorizontalDatumValue)
        {
            const char *pszAuthName =
                poHorizontalDatumReference->ReadAsString();
            const char *pszAuthCode = poHorizontalDatumValue->ReadAsString();
            if (pszAuthName && pszAuthCode)
            {
                if (oSRS.SetFromUserInput(
                        (std::string(pszAuthName) + ':' + pszAuthCode).c_str(),
                        OGRSpatialReference::
                            SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
                    OGRERR_NONE)
                {
                    oSRS.Clear();
                }
            }
        }
    }
    return !oSRS.IsEmpty();
}

/************************************************************************/
/*              S100GetNumPointsLongitudinalLatitudinal()               */
/************************************************************************/

bool S100GetNumPointsLongitudinalLatitudinal(const GDALGroup *poGroup,
                                             int &nNumPointsLongitudinal,
                                             int &nNumPointsLatitudinal)
{
    auto poSpacingX = poGroup->GetAttribute("gridSpacingLongitudinal");
    auto poSpacingY = poGroup->GetAttribute("gridSpacingLatitudinal");
    auto poNumPointsLongitudinal =
        poGroup->GetAttribute("numPointsLongitudinal");
    auto poNumPointsLatitudinal = poGroup->GetAttribute("numPointsLatitudinal");
    if (poSpacingX &&
        poSpacingX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingY &&
        poSpacingY->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poNumPointsLongitudinal &&
        GDALDataTypeIsInteger(
            poNumPointsLongitudinal->GetDataType().GetNumericDataType()) &&
        poNumPointsLatitudinal &&
        GDALDataTypeIsInteger(
            poNumPointsLatitudinal->GetDataType().GetNumericDataType()))
    {
        nNumPointsLongitudinal = poNumPointsLongitudinal->ReadAsInt();
        nNumPointsLatitudinal = poNumPointsLatitudinal->ReadAsInt();

        // Those are optional, but use them when available, to detect
        // potential inconsistency
        auto poEastBoundLongitude = poGroup->GetAttribute("eastBoundLongitude");
        auto poWestBoundLongitude = poGroup->GetAttribute("westBoundLongitude");
        auto poSouthBoundLongitude =
            poGroup->GetAttribute("southBoundLatitude");
        auto poNorthBoundLatitude = poGroup->GetAttribute("northBoundLatitude");
        if (poEastBoundLongitude &&
            GDALDataTypeIsFloating(
                poEastBoundLongitude->GetDataType().GetNumericDataType()) &&
            poWestBoundLongitude &&
            GDALDataTypeIsFloating(
                poWestBoundLongitude->GetDataType().GetNumericDataType()) &&
            poSouthBoundLongitude &&
            GDALDataTypeIsFloating(
                poSouthBoundLongitude->GetDataType().GetNumericDataType()) &&
            poNorthBoundLatitude &&
            GDALDataTypeIsFloating(
                poNorthBoundLatitude->GetDataType().GetNumericDataType()))
        {
            const double dfSpacingX = poSpacingX->ReadAsDouble();
            const double dfSpacingY = poSpacingY->ReadAsDouble();

            const double dfEast = poEastBoundLongitude->ReadAsDouble();
            const double dfWest = poWestBoundLongitude->ReadAsDouble();
            const double dfSouth = poSouthBoundLongitude->ReadAsDouble();
            const double dfNorth = poNorthBoundLatitude->ReadAsDouble();
            if (std::fabs((dfWest + nNumPointsLongitudinal * dfSpacingX) -
                          dfEast) < 5 * dfSpacingX &&
                std::fabs((dfSouth + nNumPointsLatitudinal * dfSpacingY) -
                          dfNorth) < 5 * dfSpacingY)
            {
                // We need up to 5 spacings for product
                // S-111 Trial Data Set Release 1.1/111UK_20210401T000000Z_SolentAndAppr_dcf2.h5
            }
            else
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Caution: "
                    "eastBoundLongitude/westBoundLongitude/southBoundLatitude/"
                    "northBoundLatitude/gridSpacingLongitudinal/"
                    "gridSpacingLatitudinal/numPointsLongitudinal/"
                    "numPointsLatitudinal do not seem to be consistent");
                CPLDebug("S100", "Computed east = %f. Actual = %f",
                         dfWest + nNumPointsLongitudinal * dfSpacingX, dfEast);
                CPLDebug("S100", "Computed north = %f. Actual = %f",
                         dfSouth + nNumPointsLatitudinal * dfSpacingY, dfNorth);
            }
        }

        return true;
    }
    return false;
}

/************************************************************************/
/*                        S100GetGeoTransform()                         */
/************************************************************************/

bool S100GetGeoTransform(const GDALGroup *poGroup, GDALGeoTransform &gt,
                         bool bNorthUp)
{
    auto poOriginX = poGroup->GetAttribute("gridOriginLongitude");
    auto poOriginY = poGroup->GetAttribute("gridOriginLatitude");
    auto poSpacingX = poGroup->GetAttribute("gridSpacingLongitudinal");
    auto poSpacingY = poGroup->GetAttribute("gridSpacingLatitudinal");
    if (poOriginX &&
        poOriginX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poOriginY &&
        poOriginY->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingX &&
        poSpacingX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingY &&
        poSpacingY->GetDataType().GetNumericDataType() == GDT_Float64)
    {
        int nNumPointsLongitudinal = 0;
        int nNumPointsLatitudinal = 0;
        if (!S100GetNumPointsLongitudinalLatitudinal(
                poGroup, nNumPointsLongitudinal, nNumPointsLatitudinal))
            return false;

        const double dfSpacingX = poSpacingX->ReadAsDouble();
        const double dfSpacingY = poSpacingY->ReadAsDouble();

        gt.xorig = poOriginX->ReadAsDouble();
        gt.yorig = poOriginY->ReadAsDouble() +
                   (bNorthUp ? dfSpacingY * (nNumPointsLatitudinal - 1) : 0);
        gt.xscale = dfSpacingX;
        gt.yscale = bNorthUp ? -dfSpacingY : dfSpacingY;

        // From pixel-center convention to pixel-corner convention
        gt.xorig -= gt.xscale / 2;
        gt.yorig -= gt.yscale / 2;

        return true;
    }
    return false;
}

/************************************************************************/
/*                         S100GetDimensions()                          */
/************************************************************************/

bool S100GetDimensions(
    const GDALGroup *poGroup,
    std::vector<std::shared_ptr<GDALDimension>> &apoDims,
    std::vector<std::shared_ptr<GDALMDArray>> &apoIndexingVars)
{
    auto poOriginX = poGroup->GetAttribute("gridOriginLongitude");
    auto poOriginY = poGroup->GetAttribute("gridOriginLatitude");
    auto poSpacingX = poGroup->GetAttribute("gridSpacingLongitudinal");
    auto poSpacingY = poGroup->GetAttribute("gridSpacingLatitudinal");
    if (poOriginX &&
        poOriginX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poOriginY &&
        poOriginY->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingX &&
        poSpacingX->GetDataType().GetNumericDataType() == GDT_Float64 &&
        poSpacingY &&
        poSpacingY->GetDataType().GetNumericDataType() == GDT_Float64)
    {
        int nNumPointsLongitudinal = 0;
        int nNumPointsLatitudinal = 0;
        if (!S100GetNumPointsLongitudinalLatitudinal(
                poGroup, nNumPointsLongitudinal, nNumPointsLatitudinal))
            return false;

        {
            auto poDim = std::make_shared<GDALDimensionWeakIndexingVar>(
                std::string(), "Y", GDAL_DIM_TYPE_HORIZONTAL_Y, std::string(),
                nNumPointsLatitudinal);
            auto poIndexingVar = GDALMDArrayRegularlySpaced::Create(
                std::string(), poDim->GetName(), poDim,
                poOriginY->ReadAsDouble(), poSpacingY->ReadAsDouble(), 0);
            poDim->SetIndexingVariable(poIndexingVar);
            apoDims.emplace_back(poDim);
            apoIndexingVars.emplace_back(poIndexingVar);
        }

        {
            auto poDim = std::make_shared<GDALDimensionWeakIndexingVar>(
                std::string(), "X", GDAL_DIM_TYPE_HORIZONTAL_X, std::string(),
                nNumPointsLongitudinal);
            auto poIndexingVar = GDALMDArrayRegularlySpaced::Create(
                std::string(), poDim->GetName(), poDim,
                poOriginX->ReadAsDouble(), poSpacingX->ReadAsDouble(), 0);
            poDim->SetIndexingVariable(poIndexingVar);
            apoDims.emplace_back(poDim);
            apoIndexingVars.emplace_back(poIndexingVar);
        }

        return true;
    }
    return false;
}

/************************************************************************/
/*                    SetMetadataForDataDynamicity()                    */
/************************************************************************/

void S100BaseDataset::SetMetadataForDataDynamicity(const GDALAttribute *poAttr)
{
    if (poAttr && poAttr->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        const int nVal = poAttr->ReadAsInt();
        const std::map<int, std::pair<const char *, const char *>> map = {
            {1,
             {"observation", "Values from in-situ sensor(s); may be quality "
                             "controlled and stored after collection"}},
            {2,
             {"astronomicalPrediction",
              "Values computed using harmonic analysis or other proven method "
              "of tidal analysis"}},
            {3,
             {"analysisOrHybrid",
              "Values calculated by statistical or other indirect methods, or "
              "a combination of methods"}},
            {3,
             {"hydrodynamicHindcast",
              "Values calculated from a two- or three-dimensional dynamic "
              "simulation of past conditions using only observed data for "
              "boundary forcing, via statistical method or combination"}},
            {5,
             {"hydrodynamicForecast",
              "Values calculated from a two- or three-dimensional dynamic "
              "simulation of future conditions using predicted data for "
              "boundary forcing, via statistical method or combination"}},
            {6,
             {"observedMinusPredicted",
              "Values computed as observed minus predicted values"}},
            {7,
             {"observedMinusAnalysis",
              "Values computed as observed minus analysis values"}},
            {8,
             {"observedMinusHindcast",
              "Values computed as observed minus hindcast values"}},
            {9,
             {"observedMinusForecast",
              "Values computed as observed minus forecast values"}},
            {10,
             {"forecastMinusPredicted",
              "Values computed as forecast minus predicted values"}},
        };
        const auto oIter = map.find(nVal);
        if (oIter != map.end())
        {
            GDALDataset::SetMetadataItem("DATA_DYNAMICITY_NAME",
                                         oIter->second.first);
            GDALDataset::SetMetadataItem("DATA_DYNAMICITY_DEFINITION",
                                         oIter->second.second);
        }
    }
}

/************************************************************************/
/*                   SetMetadataForCommonPointRule()                    */
/************************************************************************/

void S100BaseDataset::SetMetadataForCommonPointRule(const GDALAttribute *poAttr)
{
    if (poAttr && poAttr->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        const int nVal = poAttr->ReadAsInt();
        const std::map<int, std::pair<const char *, const char *>> map = {
            {1, {"average", "return the mean of the attribute values"}},
            {2, {"low", "use the least of the attribute values"}},
            {3, {"high", "use the greatest of the attribute values"}},
            {4,
             {"all", "return all the attribute values that can be determined "
                     "for the position"}},
        };
        const auto oIter = map.find(nVal);
        if (oIter != map.end())
        {
            GDALDataset::SetMetadataItem("COMMON_POINT_RULE_NAME",
                                         oIter->second.first);
            GDALDataset::SetMetadataItem("COMMON_POINT_RULE_DEFINITION",
                                         oIter->second.second);
        }
    }
}

/************************************************************************/
/*                  SetMetadataForInterpolationType()                   */
/************************************************************************/

void S100BaseDataset::SetMetadataForInterpolationType(
    const GDALAttribute *poAttr)
{
    if (poAttr && poAttr->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        const int nVal = poAttr->ReadAsInt();
        const std::map<int, std::pair<const char *, const char *>> map = {
            {1,
             {"nearestneighbor",
              "Assign the feature attribute value associated with the nearest "
              "domain object in the domain of the coverage"}},
            {5,
             {"bilinear", "Assign a value computed by using a bilinear "
                          "function of position within the grid cell"}},
            {6,
             {"biquadratic", "Assign a value computed by using a biquadratic "
                             "function of position within the grid cell"}},
            {7,
             {"bicubic", "Assign a value computed by using a bicubic function "
                         "of position within the grid cell"}},
            {8,
             {"lostarea", "Assign a value computed by using the lost area "
                          "method described in ISO 19123"}},
            {9,
             {"barycentric", "Assign a value computed by using the barycentric "
                             "method described in ISO 19123"}},
            {10,
             {"discrete", "No interpolation method applies to the coverage"}},
        };
        const auto oIter = map.find(nVal);
        if (oIter != map.end())
        {
            GDALDataset::SetMetadataItem("INTERPOLATION_TYPE_NAME",
                                         oIter->second.first);
            GDALDataset::SetMetadataItem("INTERPOLATION_TYPE_DEFINITION",
                                         oIter->second.second);
        }
    }
}

/************************************************************************/
/*                          gasVerticalDatums                           */
/************************************************************************/

// https://iho.int/uploads/user/pubs/standards/s-100/S-100_5.2.0_Final_Clean.pdf
// Table 10c-25 - Vertical and sounding datum, page 53
static const struct
{
    int nCode;
    const char *pszName;
    const char *pszAbbrev;
    const char *pszDefinition;
} gasVerticalDatums[] = {
    {1, "meanLowWaterSprings", "MLWS",
     "The average height of the low waters of spring tides. This level is used "
     "as a tidal datum in some areas. Also called spring low water."},
    {2, "meanLowerLowWaterSprings", nullptr,
     "The average height of lower low water springs at a place"},
    {3, "meanSeaLevel", "MSL",
     "The average height of the surface of the sea at a tide station for all "
     "stages of the tide over a 19-year period, usually determined from hourly "
     "height readings measured from a fixed predetermined reference level."},
    {4, "lowestLowWater", nullptr,
     "An arbitrary level conforming to the lowest tide observed at a place, or "
     "some what lower."},
    {5, "meanLowWater", "MLW",
     "The average height of all low waters at a place over a 19-year period."},
    {6, "lowestLowWaterSprings", nullptr,
     "An arbitrary level conforming to the lowest water level observed at a "
     "place at spring tides during a period of time shorter than 19 years."},
    {7, "approximateMeanLowWaterSprings", nullptr,
     "An arbitrary level, usually within 0.3m from that of Mean Low Water "
     "Springs (MLWS)."},
    {8, "indianSpringLowWater", "ISLW",
     "An arbitrary tidal datum approximating the level of the mean of the "
     "lower low water at spring tides. It was first used in waters surrounding "
     "India."},
    {9, "lowWaterSprings", nullptr,
     "An arbitrary level, approximating that of mean low water springs "
     "(MLWS)."},
    {10, "approximateLowestAstronomicalTide", nullptr,
     "An arbitrary level, usually within 0.3m from that of Lowest Astronomical "
     "Tide (LAT)."},
    {11, "nearlyLowestLowWater", nullptr,
     "An arbitrary level approximating the lowest water level observed at a "
     "place, usually equivalent to the Indian Spring Low Water (ISLW)."},
    {12, "meanLowerLowWater", "MLLW",
     "The average height of the lower low waters at a place over a 19-year "
     "period."},
    {13, "lowWater", "LW",
     "The lowest level reached at a place by the water surface in one "
     "oscillation. Also called low tide."},
    {14, "approximateMeanLowWater", nullptr,
     "An arbitrary level, usually within 0.3m from that of Mean Low Water "
     "(MLW)."},
    {15, "approximateMeanLowerLowWater", nullptr,
     "An arbitrary level, usually within 0.3m from that of Mean Lower Low "
     "Water (MLLW)."},
    {16, "meanHighWater", "MHW",
     "The average height of all high waters at a place over a 19-year period."},
    {17, "meanHighWaterSprings", "MHWS",
     "The average height of the high waters of spring tides. Also called "
     "spring high water."},
    {18, "highWater", "HW",
     "The highest level reached at a place by the water surface in one "
     "oscillation."},
    {19, "approximateMeanSeaLevel", nullptr,
     "An arbitrary level, usually within 0.3m from that of Mean Sea Level "
     "(MSL)."},
    {20, "highWaterSprings", nullptr,
     "An arbitrary level, approximating that of mean high water springs "
     "(MHWS)."},
    {21, "meanHigherHighWater", "MHHW",
     "The average height of higher high waters at a place over a 19-year "
     "period."},
    {22, "equinoctialSpringLowWater", nullptr,
     "The level of low water springs near the time of an equinox."},
    {23, "lowestAstronomicalTide", "LAT",
     "The lowest tide level which can be predicted to occur under average "
     "meteorological conditions and under any combination of astronomical "
     "conditions."},
    {24, "localDatum", nullptr,
     "An arbitrary datum defined by a local harbour authority, from which "
     "levels and tidal heights are measured by this authority."},
    {25, "internationalGreatLakesDatum1985", nullptr,
     "A vertical reference system with its zero based on the mean water level "
     "at Rimouski/Pointe-au-Pere, Quebec, over the period 1970 to 1988."},
    {26, "meanWaterLevel", nullptr,
     "The average of all hourly water levels over the available period of "
     "record."},
    {27, "lowerLowWaterLargeTide", nullptr,
     "The average of the lowest low waters, one from each of 19 years of "
     "observations."},
    {28, "higherHighWaterLargeTide", nullptr,
     "The average of the highest high waters, one from each of 19 years of "
     "observations."},
    {29, "nearlyHighestHighWater", nullptr,
     "An arbitrary level approximating the highest water level observed at a "
     "place, usually equivalent to the high water springs."},
    {30, "highestAstronomicalTide", "HAT",
     "The highest tidal level which can be predicted to occur under average "
     "meteorological conditions and under any combination of astronomical "
     "conditions."},
    {44, "balticSeaChartDatum2000", nullptr,
     "The datum refers to each Baltic country's realization of the European "
     "Vertical Reference System (EVRS) with land-uplift epoch 2000, which is "
     "connected to the Normaal Amsterdams Peil (NAP)"},
    {46, "internationalGreatLakesDatum2020", nullptr,
     "The 2020 update to the International Great Lakes Datum, the official "
     "reference system used to measure water level heights in the Great Lakes, "
     "connecting channels, and the St. Lawrence River system."},
    {47, "seaFloor", nullptr,
     "The bottom of the ocean and seas where there is a generally smooth "
     "gentle gradient. Also referred to as sea bed (sometimes seabed or "
     "sea-bed), and sea bottom."},
    {48, "seaSurface", nullptr,
     "A two-dimensional (in the horizontal plane) field representing the "
     "air-sea interface, with high-frequency fluctuations such as wind waves "
     "and swell, but not astronomical tides, filtered out"},
    {49, "hydrographicZero", nullptr,
     "A vertical reference near the lowest astronomical tide (LAT) below which "
     "the sea level falls only very exceptionally"},
};

/************************************************************************/
/*              S100GetVerticalDatumCodeFromNameOrAbbrev()              */
/************************************************************************/

int S100GetVerticalDatumCodeFromNameOrAbbrev(const char *pszStr)
{
    const int nCode = atoi(pszStr);
    for (const auto &sEntry : gasVerticalDatums)
    {
        if (sEntry.nCode == nCode || EQUAL(pszStr, sEntry.pszName) ||
            (sEntry.pszAbbrev && EQUAL(pszStr, sEntry.pszAbbrev)))
        {
            return sEntry.nCode;
        }
    }
    return -1;
}

/************************************************************************/
/*                       S100ReadVerticalDatum()                        */
/************************************************************************/

void S100ReadVerticalDatum(GDALMajorObject *poMO, const GDALGroup *poGroup)
{

    int nVerticalDatumReference = 1;
    auto poVerticalDatumReference =
        poGroup->GetAttribute("verticalDatumReference");
    if (poVerticalDatumReference &&
        poVerticalDatumReference->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        nVerticalDatumReference = poVerticalDatumReference->ReadAsInt();
    }

    auto poVerticalDatum = poGroup->GetAttribute("verticalDatum");
    if (poVerticalDatum &&
        poVerticalDatum->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        poMO->GDALMajorObject::SetMetadataItem(S100_VERTICAL_DATUM_ABBREV,
                                               nullptr);
        poMO->GDALMajorObject::SetMetadataItem(S100_VERTICAL_DATUM_EPSG_CODE,
                                               nullptr);
        poMO->GDALMajorObject::SetMetadataItem(S100_VERTICAL_DATUM_NAME,
                                               nullptr);
        poMO->GDALMajorObject::SetMetadataItem("verticalDatum", nullptr);
        if (nVerticalDatumReference == 1)
        {
            bool bFound = false;
            const auto nVal = poVerticalDatum->ReadAsInt();
            for (const auto &sVerticalDatum : gasVerticalDatums)
            {
                if (sVerticalDatum.nCode == nVal)
                {
                    bFound = true;
                    poMO->GDALMajorObject::SetMetadataItem(
                        S100_VERTICAL_DATUM_NAME, sVerticalDatum.pszName);
                    if (sVerticalDatum.pszAbbrev)
                    {
                        poMO->GDALMajorObject::SetMetadataItem(
                            S100_VERTICAL_DATUM_ABBREV,
                            sVerticalDatum.pszAbbrev);
                    }
                    poMO->GDALMajorObject::SetMetadataItem(
                        "VERTICAL_DATUM_DEFINITION",
                        sVerticalDatum.pszDefinition);
                    break;
                }
            }
            if (!bFound)
            {
                poMO->GDALMajorObject::SetMetadataItem("verticalDatum",
                                                       CPLSPrintf("%d", nVal));
            }
        }
        else if (nVerticalDatumReference == 2)
        {
            const auto nVal = poVerticalDatum->ReadAsInt();
            PJ *datum = proj_create_from_database(
                OSRGetProjTLSContext(), "EPSG", CPLSPrintf("%d", nVal),
                PJ_CATEGORY_DATUM, false, nullptr);
            poMO->GDALMajorObject::SetMetadataItem(
                S100_VERTICAL_DATUM_EPSG_CODE, CPLSPrintf("%d", nVal));
            if (datum)
            {
                poMO->GDALMajorObject::SetMetadataItem(S100_VERTICAL_DATUM_NAME,
                                                       proj_get_name(datum));
                proj_destroy(datum);
            }
        }
        else
        {
            CPLDebug("S100", "Unhandled verticalDatumReference = %d",
                     nVerticalDatumReference);
        }
    }
}

/************************************************************************/
/*                          S100ReadMetadata()                          */
/************************************************************************/

std::string S100ReadMetadata(GDALDataset *poDS, const std::string &osFilename,
                             const GDALGroup *poRootGroup)
{
    std::string osMetadataFile;
    for (const auto &poAttr : poRootGroup->GetAttributes())
    {
        const auto &osName = poAttr->GetName();
        if (osName == "metadata")
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal && pszVal[0])
            {
                if (CPLHasPathTraversal(pszVal))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Path traversal detected in %s", pszVal);
                    continue;
                }
                osMetadataFile = CPLFormFilenameSafe(
                    CPLGetPathSafe(osFilename.c_str()).c_str(), pszVal,
                    nullptr);
                VSIStatBufL sStat;
                if (VSIStatL(osMetadataFile.c_str(), &sStat) != 0)
                {
                    // Test products from https://data.admiralty.co.uk/portal/apps/sites/#/marine-data-portal/pages/s-100
                    // advertise a metadata filename starting with "MD_", per the spec,
                    // but the actual filename does not start with "MD_"...
                    if (STARTS_WITH(pszVal, "MD_"))
                    {
                        osMetadataFile = CPLFormFilenameSafe(
                            CPLGetPathSafe(osFilename.c_str()).c_str(),
                            pszVal + strlen("MD_"), nullptr);
                        if (VSIStatL(osMetadataFile.c_str(), &sStat) != 0)
                        {
                            osMetadataFile.clear();
                        }
                    }
                    else
                    {
                        osMetadataFile.clear();
                    }
                }
            }
        }
        else if (osName != "horizontalCRS" &&
                 osName != "horizontalDatumReference" &&
                 osName != "horizontalDatumValue" &&
                 osName != "productSpecification" &&
                 osName != "eastBoundLongitude" &&
                 osName != "northBoundLatitude" &&
                 osName != "southBoundLatitude" &&
                 osName != "westBoundLongitude" && osName != "extentTypeCode" &&
                 osName != "verticalCS" && osName != "verticalCoordinateBase" &&
                 osName != "verticalDatumReference" &&
                 osName != "verticalDatum" && osName != "nameOfHorizontalCRS" &&
                 osName != "typeOfHorizontalCRS" && osName != "horizontalCS" &&
                 osName != "horizontalDatum" &&
                 osName != "nameOfHorizontalDatum" &&
                 osName != "primeMeridian" && osName != "spheroid" &&
                 osName != "projectionMethod" &&
                 osName != "projectionParameter1" &&
                 osName != "projectionParameter2" &&
                 osName != "projectionParameter3" &&
                 osName != "projectionParameter4" &&
                 osName != "projectionParameter5" &&
                 osName != "falseNorthing" && osName != "falseEasting")
        {
            const char *pszVal = poAttr->ReadAsString();
            if (pszVal && pszVal[0])
                poDS->GDALDataset::SetMetadataItem(osName.c_str(), pszVal);
        }
    }
    return osMetadataFile;
}

/************************************************************************/
/*                   S100BaseWriter::S100BaseWriter()                   */
/************************************************************************/

S100BaseWriter::S100BaseWriter(const char *pszDestFilename,
                               GDALDataset *poSrcDS, CSLConstList papszOptions)
    : m_osDestFilename(pszDestFilename), m_poSrcDS(poSrcDS),
      m_aosOptions(papszOptions)
{
}

/************************************************************************/
/*                  S100BaseWriter::~S100BaseWriter()                   */
/************************************************************************/

S100BaseWriter::~S100BaseWriter()
{
    // Check that destructors of derived classes have called themselves their
    // Close implementation
    CPLAssert(!m_hdf5);
}

/************************************************************************/
/*                     S100BaseWriter::BaseClose()                      */
/************************************************************************/

bool S100BaseWriter::BaseClose()
{
    bool ret = m_GroupF.clear();
    ret = m_valuesGroup.clear() && ret;
    ret = m_featureInstanceGroup.clear() && ret;
    ret = m_featureGroup.clear() && ret;
    ret = m_hdf5.clear() && ret;
    return ret;
}

/************************************************************************/
/*                     S100BaseWriter::BaseChecks()                     */
/************************************************************************/

bool S100BaseWriter::BaseChecks(const char *pszDriverName, bool crsMustBeEPSG,
                                bool verticalDatumRequired)
{
    if (m_poSrcDS->GetRasterXSize() < 1 || m_poSrcDS->GetRasterYSize() < 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source dataset dimension must be at least 1x1 pixel");
        return false;
    }

    if (m_poSrcDS->GetGeoTransform(m_gt) != CE_None)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s driver requires a source dataset with a geotransform",
                 pszDriverName);
        return false;
    }
    if (m_gt.xrot != 0 || m_gt.yrot != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s driver requires a source dataset with a non-rotated "
                 "geotransform",
                 pszDriverName);
        return false;
    }

    m_poSRS = m_poSrcDS->GetSpatialRef();
    if (!m_poSRS)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s driver requires a source dataset with a CRS",
                 pszDriverName);
        return false;
    }

    const char *pszAuthName = m_poSRS->GetAuthorityName(nullptr);
    const char *pszAuthCode = m_poSRS->GetAuthorityCode(nullptr);
    if (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
    {
        m_nEPSGCode = atoi(pszAuthCode);
    }
    if (crsMustBeEPSG && m_nEPSGCode == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s driver requires a source dataset whose CRS has an EPSG "
                 "identifier",
                 pszDriverName);
        return false;
    }

    const char *pszVerticalDatum =
        m_aosOptions.FetchNameValue("VERTICAL_DATUM");
    if (!pszVerticalDatum)
        pszVerticalDatum =
            m_poSrcDS->GetMetadataItem(S100_VERTICAL_DATUM_EPSG_CODE);
    if (!pszVerticalDatum)
        pszVerticalDatum = m_poSrcDS->GetMetadataItem(S100_VERTICAL_DATUM_NAME);
    if (!pszVerticalDatum)
    {
        if (verticalDatumRequired)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VERTICAL_DATUM creation option must be specified");
            return false;
        }
    }
    else
    {
        m_nVerticalDatum =
            S100GetVerticalDatumCodeFromNameOrAbbrev(pszVerticalDatum);
        if (m_nVerticalDatum <= 0)
        {
            auto pjCtxt = OSRGetProjTLSContext();
            PJ *vertical_datum =
                proj_create_from_database(pjCtxt, "EPSG", pszVerticalDatum,
                                          PJ_CATEGORY_DATUM, false, nullptr);
            const bool bIsValid = vertical_datum != nullptr &&
                                  proj_get_type(vertical_datum) ==
                                      PJ_TYPE_VERTICAL_REFERENCE_FRAME;
            proj_destroy(vertical_datum);
            if (bIsValid)
            {
                m_nVerticalDatum = atoi(pszVerticalDatum);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "VERTICAL_DATUM value is invalid");
                return false;
            }
        }
    }

    const std::string osFilename = CPLGetFilename(m_osDestFilename.c_str());
    CPLAssert(pszDriverName[0] == 'S');
    const char *pszExpectedFilenamePrefix = pszDriverName + 1;
    if (!cpl::starts_with(osFilename, pszExpectedFilenamePrefix))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "%s dataset filenames should start with '%s'", pszDriverName,
                 pszExpectedFilenamePrefix);
    }
    if (!cpl::ends_with(osFilename, ".h5") &&
        !cpl::ends_with(osFilename, ".H5"))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "%s dataset filenames should have a '.H5' extension",
                 pszDriverName);
    }

    return true;
}

/************************************************************************/
/*                 S100BaseWriter::OpenFileUpdateMode()                 */
/************************************************************************/

bool S100BaseWriter::OpenFileUpdateMode()
{
    hid_t fapl = H5_CHECK(H5Pcreate(H5P_FILE_ACCESS));
    H5Pset_driver(fapl, HDF5GetFileDriver(), nullptr);
    m_hdf5.reset(H5Fopen(m_osDestFilename.c_str(), H5F_ACC_RDWR, fapl));
    H5Pclose(fapl);
    if (!m_hdf5)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot open file %s in update mode",
                 m_osDestFilename.c_str());
        return false;
    }
    return true;
}

/************************************************************************/
/*                     S100BaseWriter::CreateFile()                     */
/************************************************************************/

bool S100BaseWriter::CreateFile()
{
    hid_t fapl = H5_CHECK(H5Pcreate(H5P_FILE_ACCESS));
    H5Pset_driver(fapl, HDF5GetFileDriver(), nullptr);
    {
        GH5_libhdf5_error_silencer oErrorSilencer;
        m_hdf5.reset(H5Fcreate(m_osDestFilename.c_str(), H5F_ACC_TRUNC,
                               H5P_DEFAULT, fapl));
    }
    H5Pclose(fapl);
    if (!m_hdf5)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create file %s",
                 m_osDestFilename.c_str());
        return false;
    }
    return true;
}

/************************************************************************/
/*                  S100BaseWriter::WriteUInt8Value()                   */
/************************************************************************/

bool S100BaseWriter::WriteUInt8Value(hid_t hGroup, const char *pszName,
                                     int value)
{
    return GH5_CreateAttribute(hGroup, pszName, H5T_STD_U8LE) &&
           GH5_WriteAttribute(hGroup, pszName, value);
}

/************************************************************************/
/*                  S100BaseWriter::WriteUInt16Value()                  */
/************************************************************************/

bool S100BaseWriter::WriteUInt16Value(hid_t hGroup, const char *pszName,
                                      int value)
{
    return GH5_CreateAttribute(hGroup, pszName, H5T_STD_U16LE) &&
           GH5_WriteAttribute(hGroup, pszName, value);
}

/************************************************************************/
/*                  S100BaseWriter::WriteUInt32Value()                  */
/************************************************************************/

bool S100BaseWriter::WriteUInt32Value(hid_t hGroup, const char *pszName,
                                      unsigned value)
{
    return GH5_CreateAttribute(hGroup, pszName, H5T_STD_U32LE) &&
           GH5_WriteAttribute(hGroup, pszName, value);
}

/************************************************************************/
/*                  S100BaseWriter::WriteInt32Value()                   */
/************************************************************************/

bool S100BaseWriter::WriteInt32Value(hid_t hGroup, const char *pszName,
                                     int value)
{
    return GH5_CreateAttribute(hGroup, pszName, H5T_STD_I32LE) &&
           GH5_WriteAttribute(hGroup, pszName, value);
}

/************************************************************************/
/*                 S100BaseWriter::WriteFloat32Value()                  */
/************************************************************************/

bool S100BaseWriter::WriteFloat32Value(hid_t hGroup, const char *pszName,
                                       double value)
{
    return GH5_CreateAttribute(hGroup, pszName, H5T_IEEE_F32LE) &&
           GH5_WriteAttribute(hGroup, pszName, value);
}

/************************************************************************/
/*                 S100BaseWriter::WriteFloat64Value()                  */
/************************************************************************/

bool S100BaseWriter::WriteFloat64Value(hid_t hGroup, const char *pszName,
                                       double value)
{
    return GH5_CreateAttribute(hGroup, pszName, H5T_IEEE_F64LE) &&
           GH5_WriteAttribute(hGroup, pszName, value);
}

/************************************************************************/
/*             S100BaseWriter::WriteVarLengthStringValue()              */
/************************************************************************/

bool S100BaseWriter::WriteVarLengthStringValue(hid_t hGroup,
                                               const char *pszName,
                                               const char *pszValue)
{
    return GH5_CreateAttribute(hGroup, pszName, H5T_C_S1, VARIABLE_LENGTH) &&
           GH5_WriteAttribute(hGroup, pszName, pszValue);
}

/************************************************************************/
/*            S100BaseWriter::WriteFixedLengthStringValue()             */
/************************************************************************/

bool S100BaseWriter::WriteFixedLengthStringValue(hid_t hGroup,
                                                 const char *pszName,
                                                 const char *pszValue)
{
    return GH5_CreateAttribute(hGroup, pszName, H5T_C_S1,
                               static_cast<unsigned>(strlen(pszValue))) &&
           GH5_WriteAttribute(hGroup, pszName, pszValue);
}

/************************************************************************/
/*             S100BaseWriter::WriteProductSpecification()              */
/************************************************************************/

bool S100BaseWriter::WriteProductSpecification(
    const char *pszProductSpecification)
{
    return WriteVarLengthStringValue(m_hdf5, "productSpecification",
                                     pszProductSpecification);
}

/************************************************************************/
/*                   S100BaseWriter::WriteIssueDate()                   */
/************************************************************************/

bool S100BaseWriter::WriteIssueDate()
{
    const char *pszIssueDate = m_aosOptions.FetchNameValue("ISSUE_DATE");
    if (!pszIssueDate)
    {
        const char *pszTmp = m_poSrcDS->GetMetadataItem("issueDate");
        if (pszTmp && strlen(pszTmp) == 8)
            pszIssueDate = pszTmp;
    }

    std::string osIssueDate;  // keep in that scope
    if (pszIssueDate)
    {
        if (strlen(pszIssueDate) != 8)
            CPLError(CE_Warning, CPLE_AppDefined,
                     "ISSUE_DATE should be 8 digits: YYYYMMDD");
    }
    else
    {
        time_t now;
        time(&now);
        struct tm brokenDown;
        CPLUnixTimeToYMDHMS(now, &brokenDown);
        osIssueDate = CPLSPrintf("%04d%02d%02d", brokenDown.tm_year + 1900,
                                 brokenDown.tm_mon + 1, brokenDown.tm_mday);
        pszIssueDate = osIssueDate.c_str();
    }

    return WriteVarLengthStringValue(m_hdf5, "issueDate", pszIssueDate);
}

/************************************************************************/
/*                   S100BaseWriter::WriteIssueTime()                   */
/************************************************************************/

bool S100BaseWriter::WriteIssueTime(bool bAutogenerateFromCurrent)
{
    const char *pszIssueTime = m_aosOptions.FetchNameValue("ISSUE_TIME");
    if (!pszIssueTime)
    {
        const char *pszTmp = m_poSrcDS->GetMetadataItem("issueTime");
        if (pszTmp && strlen(pszTmp) == 7 && pszTmp[6] == 'Z')
            pszIssueTime = pszTmp;
    }
    std::string osIssueTime;  // keep in that scope
    if (!pszIssueTime && bAutogenerateFromCurrent)
    {
        time_t now;
        time(&now);
        struct tm brokenDown;
        CPLUnixTimeToYMDHMS(now, &brokenDown);
        osIssueTime = CPLSPrintf("%02d%02d%02dZ", brokenDown.tm_hour,
                                 brokenDown.tm_min, brokenDown.tm_sec);
        pszIssueTime = osIssueTime.c_str();
    }
    return !pszIssueTime || pszIssueTime[0] == 0 ||
           WriteVarLengthStringValue(m_hdf5, "issueTime", pszIssueTime);
}

/************************************************************************/
/*              S100BaseWriter::WriteTopLevelBoundingBox()              */
/************************************************************************/

bool S100BaseWriter::WriteTopLevelBoundingBox()
{

    OGREnvelope sExtent;
    if (m_poSrcDS->GetExtentWGS84LongLat(&sExtent) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot get dataset extent in WGS84 longitude/latitude");
        return false;
    }

    return WriteFloat32Value(m_hdf5, "westBoundLongitude", sExtent.MinX) &&
           WriteFloat32Value(m_hdf5, "southBoundLatitude", sExtent.MinY) &&
           WriteFloat32Value(m_hdf5, "eastBoundLongitude", sExtent.MaxX) &&
           WriteFloat32Value(m_hdf5, "northBoundLatitude", sExtent.MaxY);
}

/************************************************************************/
/*                 S100BaseWriter::WriteHorizontalCRS()                 */
/************************************************************************/

bool S100BaseWriter::WriteHorizontalCRS()
{
    bool ret = WriteInt32Value(m_hdf5, "horizontalCRS",
                               m_nEPSGCode > 0 ? m_nEPSGCode : -1);
    if (ret && m_nEPSGCode <= 0)
    {
        ret = WriteVarLengthStringValue(m_hdf5, "nameOfHorizontalCRS",
                                        m_poSRS->GetName());
        {
            GH5_HIDTypeHolder hEnumType(H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
            ret = ret && hEnumType;
            if (ret)
            {
                uint8_t val;
                val = 1;
                ret = ret && H5_CHECK(H5Tenum_insert(hEnumType, "geodeticCRS2D",
                                                     &val)) >= 0;
                val = 2;
                ret = ret && H5_CHECK(H5Tenum_insert(hEnumType, "projectedCRS",
                                                     &val)) >= 0;
                ret = ret &&
                      GH5_CreateAttribute(m_hdf5, "typeOfHorizontalCRS",
                                          hEnumType) &&
                      GH5_WriteAttribute(m_hdf5, "typeOfHorizontalCRS",
                                         m_poSRS->IsGeographic() ? 1 : 2);
            }
        }

        const int nHorizontalCS = m_poSRS->IsGeographic() ? 6422
                                  : m_poSRS->EPSGTreatsAsNorthingEasting()
                                      ? 4500
                                      : 4400;
        ret = ret && WriteInt32Value(m_hdf5, "horizontalCS", nHorizontalCS);

        const char *pszDatumKey =
            m_poSRS->IsGeographic() ? "GEOGCS|DATUM" : "PROJCS|GEOGCS|DATUM";
        const char *pszDatumAuthName = m_poSRS->GetAuthorityName(pszDatumKey);
        const char *pszDatumCode = m_poSRS->GetAuthorityCode(pszDatumKey);
        const int nDatum = (pszDatumAuthName && pszDatumCode &&
                            EQUAL(pszDatumAuthName, "EPSG"))
                               ? atoi(pszDatumCode)
                               : -1;
        ret = ret && WriteInt32Value(m_hdf5, "horizontalDatum", nDatum);
        if (ret && nDatum < 0)
        {
            const char *pszDatum = m_poSRS->GetAttrValue(pszDatumKey);
            if (!pszDatum)
                pszDatum = "unknown";
            ret = WriteVarLengthStringValue(m_hdf5, "nameOfHorizontalDatum",
                                            pszDatum);

            const char *pszSpheroidKey = m_poSRS->IsGeographic()
                                             ? "GEOGCS|DATUM|SPHEROID"
                                             : "PROJCS|GEOGCS|DATUM|SPHEROID";
            const char *pszSpheroidAuthName =
                m_poSRS->GetAuthorityName(pszSpheroidKey);
            const char *pszSpheroidCode =
                m_poSRS->GetAuthorityCode(pszSpheroidKey);
            const char *pszSpheroidName = m_poSRS->GetAttrValue(pszSpheroidKey);
            const int nSpheroid =
                (pszSpheroidAuthName && pszSpheroidCode &&
                 EQUAL(pszSpheroidAuthName, "EPSG"))
                    ? atoi(pszSpheroidCode)
                : (pszSpheroidName && EQUAL(pszSpheroidName, "Bessel 1841"))
                    ? 7004
                    : -1;
            if (nSpheroid <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unknown code for ellipsoid of CRS");
                return false;
            }
            ret = ret && WriteInt32Value(m_hdf5, "spheroid", nSpheroid);

            const char *pszPrimeMeridianKey = m_poSRS->IsGeographic()
                                                  ? "GEOGCS|PRIMEM"
                                                  : "PROJCS|GEOGCS|PRIMEM";
            const char *pszPrimeMeridianAuthName =
                m_poSRS->GetAuthorityName(pszPrimeMeridianKey);
            const char *pszPrimeMeridianCode =
                m_poSRS->GetAuthorityCode(pszPrimeMeridianKey);
            const char *pszPrimeMeridianName =
                m_poSRS->GetAttrValue(pszPrimeMeridianKey);
            const int nPrimeMeridian =
                (pszPrimeMeridianAuthName && pszPrimeMeridianCode &&
                 EQUAL(pszPrimeMeridianAuthName, "EPSG"))
                    ? atoi(pszPrimeMeridianCode)
                : (pszPrimeMeridianName && EQUAL(pszPrimeMeridianName, "Ferro"))
                    ? 8909
                    : -1;
            if (nPrimeMeridian <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unknown code for prime meridian of CRS");
                return false;
            }
            ret =
                ret && WriteInt32Value(m_hdf5, "primeMeridian", nPrimeMeridian);
        }

        const char *pszProjection = m_poSRS->IsProjected()
                                        ? m_poSRS->GetAttrValue("PROJECTION")
                                        : nullptr;
        if (pszProjection)
        {
            int nProjectionMethod = 0;
            double adfParams[] = {std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::quiet_NaN()};
            if (EQUAL(pszProjection, SRS_PT_MERCATOR_2SP))
            {
                nProjectionMethod = PROJECTION_METHOD_MERCATOR;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_MERCATOR_1SP))
            {
                auto poTmpSRS = std::unique_ptr<OGRSpatialReference>(
                    m_poSRS->convertToOtherProjection(SRS_PT_MERCATOR_2SP));
                nProjectionMethod = PROJECTION_METHOD_MERCATOR;
                adfParams[0] =
                    poTmpSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
                adfParams[1] =
                    poTmpSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR))
            {
                nProjectionMethod = PROJECTION_METHOD_TRANSVERSE_MERCATOR;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                adfParams[2] =
                    m_poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }
            else if (EQUAL(pszProjection,
                           SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER))
            {
                nProjectionMethod = PROJECTION_METHOD_OBLIQUE_MERCATOR;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_CENTER, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER, 0.0);
                adfParams[2] = m_poSRS->GetNormProjParm(SRS_PP_AZIMUTH, 0.0);
                adfParams[3] =
                    m_poSRS->GetNormProjParm(SRS_PP_RECTIFIED_GRID_ANGLE, 0.0);
                adfParams[4] =
                    m_poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_HOTINE_OBLIQUE_MERCATOR))
            {
                nProjectionMethod = PROJECTION_METHOD_HOTINE_OBLIQUE_MERCATOR;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_CENTER, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER, 0.0);
                adfParams[2] = m_poSRS->GetNormProjParm(SRS_PP_AZIMUTH, 0.0);
                adfParams[3] =
                    m_poSRS->GetNormProjParm(SRS_PP_RECTIFIED_GRID_ANGLE, 0.0);
                adfParams[4] =
                    m_poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP))
            {
                nProjectionMethod = PROJECTION_METHOD_LCC_1SP;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                adfParams[2] =
                    m_poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP))
            {
                nProjectionMethod = PROJECTION_METHOD_LCC_2SP;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                adfParams[2] =
                    m_poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
                adfParams[3] =
                    m_poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_OBLIQUE_STEREOGRAPHIC))
            {
                nProjectionMethod = PROJECTION_METHOD_OBLIQUE_STEREOGRAPHIC;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                adfParams[2] =
                    m_poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC))
            {
                nProjectionMethod = PROJECTION_METHOD_POLAR_STEREOGRAPHIC;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                adfParams[2] =
                    m_poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_KROVAK))
            {
                nProjectionMethod =
                    PROJECTION_METHOD_KROVAK_OBLIQUE_CONIC_CONFORMAL;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                adfParams[2] = m_poSRS->GetNormProjParm(SRS_PP_AZIMUTH, 0.0);
                adfParams[3] =
                    m_poSRS->GetNormProjParm(SRS_PP_PSEUDO_STD_PARALLEL_1, 0.0);
                adfParams[4] =
                    m_poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_POLYCONIC))
            {
                nProjectionMethod = PROJECTION_METHOD_AMERICAN_POLYCONIC;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_ALBERS_CONIC_EQUAL_AREA))
            {
                nProjectionMethod = PROJECTION_METHOD_ALBERS_EQUAL_AREA;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_CENTER, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER, 0.0);
                adfParams[2] =
                    m_poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
                adfParams[3] =
                    m_poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0);
            }
            else if (EQUAL(pszProjection, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA))
            {
                nProjectionMethod =
                    PROJECTION_METHOD_LAMBERT_AZIMUTHAL_EQUAL_AREA;
                adfParams[0] =
                    m_poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_CENTER, 0.0);
                adfParams[1] =
                    m_poSRS->GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER, 0.0);
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Projection method %s is not supported by S100",
                         pszProjection);
                return false;
            }

            ret = ret && WriteInt32Value(m_hdf5, "projectionMethod",
                                         nProjectionMethod);
            for (int i = 0; i < 5 && !std::isnan(adfParams[i]); ++i)
            {
                const std::string osAttrName =
                    "projectionParameter" + std::to_string(i + 1);
                ret = ret && WriteFloat64Value(m_hdf5, osAttrName.c_str(),
                                               adfParams[i]);
            }

            ret = ret && WriteFloat64Value(m_hdf5, "falseNorthing",
                                           m_poSRS->GetNormProjParm(
                                               SRS_PP_FALSE_NORTHING, 0.0));
            ret = ret && WriteFloat64Value(m_hdf5, "falseEasting",
                                           m_poSRS->GetNormProjParm(
                                               SRS_PP_FALSE_EASTING, 0.0));
        }
    }
    return ret;
}

/************************************************************************/
/*            S100BaseWriter::WriteVerticalCoordinateBase()             */
/************************************************************************/

bool S100BaseWriter::WriteVerticalCoordinateBase(int nCode)
{
    GH5_HIDTypeHolder hEnumType(H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
    bool ret = hEnumType;
    if (hEnumType)
    {
        uint8_t val;
        val = 1;
        ret =
            ret && H5_CHECK(H5Tenum_insert(hEnumType, "seaSurface", &val)) >= 0;
        val = 2;
        ret = ret &&
              H5_CHECK(H5Tenum_insert(hEnumType, "verticalDatum", &val)) >= 0;
        val = 3;
        ret =
            ret && H5_CHECK(H5Tenum_insert(hEnumType, "seaBottom", &val)) >= 0;

        ret =
            ret &&
            GH5_CreateAttribute(m_hdf5, "verticalCoordinateBase", hEnumType) &&
            GH5_WriteAttribute(m_hdf5, "verticalCoordinateBase", nCode);
    }
    return ret;
}

/************************************************************************/
/*            S100BaseWriter::WriteVerticalDatumReference()             */
/************************************************************************/

bool S100BaseWriter::WriteVerticalDatumReference(hid_t hGroup, int nCode)
{
    GH5_HIDTypeHolder hEnumType(H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
    bool ret = hEnumType;
    if (hEnumType)
    {
        uint8_t val;
        val = 1;
        ret = ret && H5_CHECK(H5Tenum_insert(hEnumType, "s100VerticalDatum",
                                             &val)) >= 0;
        val = 2;
        ret = ret && H5_CHECK(H5Tenum_insert(hEnumType, "EPSG", &val)) >= 0;

        ret =
            ret &&
            GH5_CreateAttribute(hGroup, "verticalDatumReference", hEnumType) &&
            GH5_WriteAttribute(hGroup, "verticalDatumReference", nCode);
    }
    return ret;
}

/************************************************************************/
/*                  S100BaseWriter::WriteVerticalCS()                   */
/************************************************************************/

bool S100BaseWriter::WriteVerticalCS(int nCode)
{
    return GH5_CreateAttribute(m_hdf5, "verticalCS", H5T_STD_I32LE) &&
           GH5_WriteAttribute(m_hdf5, "verticalCS", nCode);
}

/************************************************************************/
/*                 S100BaseWriter::WriteVerticalDatum()                 */
/************************************************************************/

bool S100BaseWriter::WriteVerticalDatum(hid_t hGroup, hid_t hType, int nCode)
{
    return GH5_CreateAttribute(hGroup, "verticalDatum", hType) &&
           GH5_WriteAttribute(hGroup, "verticalDatum", nCode);
}

/************************************************************************/
/*                    S100BaseWriter::CreateGroupF()                    */
/************************************************************************/

bool S100BaseWriter::CreateGroupF()
{
    m_GroupF.reset(H5_CHECK(H5Gcreate(m_hdf5, "Group_F", 0)));
    return m_GroupF;
}

/************************************************************************/
/*                 S100BaseWriter::CreateFeatureGroup()                 */
/************************************************************************/

bool S100BaseWriter::CreateFeatureGroup(const char *name)
{
    m_featureGroup.reset(H5_CHECK(H5Gcreate(m_hdf5, name, 0)));
    return m_featureGroup;
}

/************************************************************************/
/*               S100BaseWriter::WriteDataCodingFormat()                */
/************************************************************************/

bool S100BaseWriter::WriteDataCodingFormat(hid_t hGroup, int nCode)
{
    GH5_HIDTypeHolder hEnumType(H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
    bool ret = hEnumType;
    if (hEnumType)
    {
        uint8_t val = 0;
        for (const char *pszEnumName :
             {"Fixed Stations", "Regular Grid", "Ungeorectified Grid",
              "Moving Platform", "Irregular Grid", "Variable cell size", "TIN",
              "Fixed Stations (Stationwise)", "Feature oriented Regular Grid"})
        {
            ++val;
            ret = ret &&
                  H5_CHECK(H5Tenum_insert(hEnumType, pszEnumName, &val)) >= 0;
        }

        ret = ret &&
              GH5_CreateAttribute(hGroup, "dataCodingFormat", hEnumType) &&
              GH5_WriteAttribute(hGroup, "dataCodingFormat", nCode);
    }
    return ret;
}

/************************************************************************/
/*                S100BaseWriter::WriteCommonPointRule()                */
/************************************************************************/

bool S100BaseWriter::WriteCommonPointRule(hid_t hGroup, int nCode)
{
    GH5_HIDTypeHolder hEnumType(H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
    bool ret = hEnumType;
    if (hEnumType)
    {
        uint8_t val = 0;
        for (const char *pszEnumName : {"average", "low", "high", "all"})
        {
            ++val;
            ret = ret &&
                  H5_CHECK(H5Tenum_insert(hEnumType, pszEnumName, &val)) >= 0;
        }

        ret = ret &&
              GH5_CreateAttribute(hGroup, "commonPointRule", hEnumType) &&
              GH5_WriteAttribute(hGroup, "commonPointRule", nCode);
    }
    return ret;
}

/************************************************************************/
/*                S100BaseWriter::WriteDataOffsetCode()                 */
/************************************************************************/

bool S100BaseWriter::WriteDataOffsetCode(hid_t hGroup, int nCode)
{
    GH5_HIDTypeHolder hEnumType(H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
    bool ret = hEnumType;
    if (hEnumType)
    {
        uint8_t val = 0;
        for (const char *pszEnumName :
             {"XMin, YMin (\"Lower left\") corner (\"Cell origin\")",
              "XMax, YMax (\"Upper right\") corner",
              "XMax, YMin (\"Lower right\") corner",
              "XMin, YMax (\"Upper left\") corner",
              "Barycenter (centroid) of cell"})
        {
            ++val;
            ret = ret &&
                  H5_CHECK(H5Tenum_insert(hEnumType, pszEnumName, &val)) >= 0;
        }

        ret = ret && GH5_CreateAttribute(hGroup, "dataOffsetCode", hEnumType) &&
              GH5_WriteAttribute(hGroup, "dataOffsetCode", nCode);
    }

    return ret;
}

/************************************************************************/
/*                   S100BaseWriter::WriteDimension()                   */
/************************************************************************/

bool S100BaseWriter::WriteDimension(hid_t hGroup, int nCode)
{
    return WriteUInt8Value(hGroup, "dimension", nCode);
}

/************************************************************************/
/*         S100BaseWriter::WriteHorizontalPositionUncertainty()         */
/************************************************************************/

bool S100BaseWriter::WriteHorizontalPositionUncertainty(hid_t hGroup,
                                                        float fValue)
{
    return WriteFloat32Value(hGroup, "horizontalPositionUncertainty", fValue);
}

/************************************************************************/
/*               S100BaseWriter::WriteInterpolationType()               */
/************************************************************************/

bool S100BaseWriter::WriteInterpolationType(hid_t hGroup, int nCode)
{
    GH5_HIDTypeHolder hEnumType(H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
    bool ret = hEnumType;
    if (hEnumType)
    {
        uint8_t val = 0;
        constexpr const char *NULL_STRING = nullptr;
        for (const char *pszEnumName : {
                 "nearestneighbor",  // 1
                 NULL_STRING,        // 2
                 NULL_STRING,        // 3
                 NULL_STRING,        // 4
                 "bilinear",         // 5
                 "biquadratic",      // 6
                 "bicubic",          // 7
                 NULL_STRING,        // 8
                 "barycentric",      // 9
                 "discrete"          // 10
             })
        {
            ++val;
            if (pszEnumName)
            {
                ret = ret && H5_CHECK(H5Tenum_insert(hEnumType, pszEnumName,
                                                     &val)) >= 0;
            }
        }

        ret = ret &&
              GH5_CreateAttribute(hGroup, "interpolationType", hEnumType) &&
              GH5_WriteAttribute(hGroup, "interpolationType", nCode);
    }
    return ret;
}

/************************************************************************/
/*                 S100BaseWriter::WriteNumInstances()                  */
/************************************************************************/

bool S100BaseWriter::WriteNumInstances(hid_t hGroup, hid_t hType,
                                       int numInstances)
{
    return GH5_CreateAttribute(hGroup, "numInstances", hType) &&
           GH5_WriteAttribute(hGroup, "numInstances", numInstances);
}

/************************************************************************/
/*          S100BaseWriter::WriteSequencingRuleScanDirection()          */
/************************************************************************/

bool S100BaseWriter::WriteSequencingRuleScanDirection(hid_t hGroup,
                                                      const char *pszValue)
{
    return WriteVarLengthStringValue(hGroup, "sequencingRule.scanDirection",
                                     pszValue);
}

/************************************************************************/
/*              S100BaseWriter::WriteSequencingRuleType()               */
/************************************************************************/

bool S100BaseWriter::WriteSequencingRuleType(hid_t hGroup, int nCode)
{
    GH5_HIDTypeHolder hEnumType(H5_CHECK(H5Tenum_create(H5T_STD_U8LE)));
    bool ret = hEnumType;
    if (hEnumType)
    {
        uint8_t val = 0;
        for (const char *pszEnumName :
             {"linear", "boustrophedonic", "CantorDiagonal", "spiral", "Morton",
              "Hilbert"})
        {
            ++val;
            ret = ret &&
                  H5_CHECK(H5Tenum_insert(hEnumType, pszEnumName, &val)) >= 0;
        }

        ret = ret &&
              GH5_CreateAttribute(hGroup, "sequencingRule.type", hEnumType) &&
              GH5_WriteAttribute(hGroup, "sequencingRule.type", nCode);
    }
    return ret;
}

/************************************************************************/
/*              S100BaseWriter::WriteVerticalUncertainty()              */
/************************************************************************/

bool S100BaseWriter::WriteVerticalUncertainty(hid_t hGroup, float fValue)
{
    return WriteFloat32Value(hGroup, "verticalUncertainty", fValue);
}

/************************************************************************/
/*      S100BaseWriter::WriteOneDimensionalVarLengthStringArray()       */
/************************************************************************/

bool S100BaseWriter::WriteOneDimensionalVarLengthStringArray(
    hid_t hGroup, const char *name, CSLConstList values)
{
    bool ret = false;
    hsize_t dims[1] = {static_cast<hsize_t>(CSLCount(values))};
    GH5_HIDSpaceHolder hSpaceId(H5_CHECK(H5Screate_simple(1, dims, NULL)));
    GH5_HIDTypeHolder hTypeId(H5_CHECK(H5Tcopy(H5T_C_S1)));
    if (hSpaceId && hTypeId)
    {
        ret = H5_CHECK(H5Tset_size(hTypeId, H5T_VARIABLE)) >= 0 &&
              H5_CHECK(H5Tset_strpad(hTypeId, H5T_STR_NULLTERM)) >= 0;
        GH5_HIDDatasetHolder hDSId;
        if (ret)
        {
            hDSId.reset(H5_CHECK(
                H5Dcreate(hGroup, name, hTypeId, hSpaceId, H5P_DEFAULT)));
            if (hDSId)
                ret = H5Dwrite(hDSId, hTypeId, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                               values) >= 0;
        }
    }
    return ret;
}

/************************************************************************/
/*                   S100BaseWriter::WriteAxisNames()                   */
/************************************************************************/

bool S100BaseWriter::WriteAxisNames(hid_t hGroup)
{
    const char *axisProjected[] = {"Easting", "Northing", nullptr};
    const char *axisGeographic[] = {"Latitude", "Longitude", nullptr};
    return WriteOneDimensionalVarLengthStringArray(
        hGroup, "axisNames",
        m_poSRS->IsProjected() ? axisProjected : axisGeographic);
}

/************************************************************************/
/*             S100BaseWriter::CreateFeatureInstanceGroup()             */
/************************************************************************/

bool S100BaseWriter::CreateFeatureInstanceGroup(const char *name)
{
    CPLAssert(m_featureGroup);
    m_featureInstanceGroup.reset(H5_CHECK(H5Gcreate(m_featureGroup, name, 0)));
    return m_featureInstanceGroup;
}

/************************************************************************/
/*           S100BaseWriter::WriteFIGGridRelatedParameters()            */
/************************************************************************/

bool S100BaseWriter::WriteFIGGridRelatedParameters(hid_t hGroup)
{
    // From pixel-corner convention to pixel-center convention
    const double dfMinX = m_gt.xorig + m_gt.xscale / 2;
    const double dfMinY = m_gt.yscale < 0
                              ? m_gt.yorig +
                                    m_gt.yscale * m_poSrcDS->GetRasterYSize() -
                                    m_gt.yscale / 2
                              : m_gt.yorig + m_gt.yscale / 2;
    const double dfMaxX =
        dfMinX + (m_poSrcDS->GetRasterXSize() - 1) * m_gt.xscale;
    const double dfMaxY =
        dfMinY + (m_poSrcDS->GetRasterYSize() - 1) * std::fabs(m_gt.yscale);

    return WriteFloat32Value(hGroup, "westBoundLongitude", dfMinX) &&
           WriteFloat32Value(hGroup, "southBoundLatitude", dfMinY) &&
           WriteFloat32Value(hGroup, "eastBoundLongitude", dfMaxX) &&
           WriteFloat32Value(hGroup, "northBoundLatitude", dfMaxY) &&
           WriteFloat64Value(hGroup, "gridOriginLongitude", dfMinX) &&
           WriteFloat64Value(hGroup, "gridOriginLatitude", dfMinY) &&
           WriteFloat64Value(hGroup, "gridSpacingLongitudinal", m_gt.xscale) &&
           WriteFloat64Value(hGroup, "gridSpacingLatitudinal",
                             std::fabs(m_gt.yscale)) &&
           WriteUInt32Value(hGroup, "numPointsLongitudinal",
                            m_poSrcDS->GetRasterXSize()) &&
           WriteUInt32Value(hGroup, "numPointsLatitudinal",
                            m_poSrcDS->GetRasterYSize()) &&
           WriteVarLengthStringValue(hGroup, "startSequence", "0,0");
}

/************************************************************************/
/*                    S100BaseWriter::WriteNumGRP()                     */
/************************************************************************/

bool S100BaseWriter::WriteNumGRP(hid_t hGroup, hid_t hType, int numGRP)
{
    return GH5_CreateAttribute(hGroup, "numGRP", hType) &&
           GH5_WriteAttribute(hGroup, "numGRP", numGRP);
}

/************************************************************************/
/*                 S100BaseWriter::CreateValuesGroup()                  */
/************************************************************************/

bool S100BaseWriter::CreateValuesGroup(const char *name)
{
    CPLAssert(m_featureInstanceGroup);
    m_valuesGroup.reset(H5_CHECK(H5Gcreate(m_featureInstanceGroup, name, 0)));
    return m_valuesGroup;
}

/************************************************************************/
/*                 S100BaseWriter::WriteGroupFDataset()                 */
/************************************************************************/

bool S100BaseWriter::WriteGroupFDataset(
    const char *name,
    const std::vector<std::array<const char *, GROUP_F_DATASET_FIELD_COUNT>>
        &rows)
{
    GH5_HIDTypeHolder hDataType(H5_CHECK(
        H5Tcreate(H5T_COMPOUND, GROUP_F_DATASET_FIELD_COUNT * sizeof(char *))));
    GH5_HIDTypeHolder hVarLengthType(H5_CHECK(H5Tcopy(H5T_C_S1)));
    bool bRet =
        hDataType && hVarLengthType &&
        H5_CHECK(H5Tset_size(hVarLengthType, H5T_VARIABLE)) >= 0 &&
        H5_CHECK(H5Tset_strpad(hVarLengthType, H5T_STR_NULLTERM)) >= 0 &&
        H5_CHECK(H5Tinsert(hDataType, "code", 0 * sizeof(char *),
                           hVarLengthType)) >= 0 &&
        H5_CHECK(H5Tinsert(hDataType, "name", 1 * sizeof(char *),
                           hVarLengthType)) >= 0 &&
        H5_CHECK(H5Tinsert(hDataType, "uom.name", 2 * sizeof(char *),
                           hVarLengthType)) >= 0 &&
        H5_CHECK(H5Tinsert(hDataType, "fillValue", 3 * sizeof(char *),
                           hVarLengthType)) >= 0 &&
        H5_CHECK(H5Tinsert(hDataType, "datatype", 4 * sizeof(char *),
                           hVarLengthType)) >= 0 &&
        H5_CHECK(H5Tinsert(hDataType, "lower", 5 * sizeof(char *),
                           hVarLengthType)) >= 0 &&
        H5_CHECK(H5Tinsert(hDataType, "upper", 6 * sizeof(char *),
                           hVarLengthType)) >= 0 &&
        H5_CHECK(H5Tinsert(hDataType, "closure", 7 * sizeof(char *),
                           hVarLengthType)) >= 0;

    hsize_t dims[] = {static_cast<hsize_t>(rows.size())};
    GH5_HIDSpaceHolder hDataSpace(H5_CHECK(H5Screate_simple(1, dims, nullptr)));
    bRet = bRet && hDataSpace;
    GH5_HIDDatasetHolder hDatasetID;
    if (bRet)
    {
        hDatasetID.reset(H5_CHECK(
            H5Dcreate(m_GroupF, name, hDataType, hDataSpace, H5P_DEFAULT)));
        bRet = hDatasetID;
    }
    GH5_HIDSpaceHolder hFileSpace;
    if (bRet)
    {
        hFileSpace.reset(H5_CHECK(H5Dget_space(hDatasetID)));
        bRet = hFileSpace;
    }

    hsize_t count[] = {1};
    GH5_HIDSpaceHolder hMemSpace(H5_CHECK(H5Screate_simple(1, count, nullptr)));
    bRet = bRet && hMemSpace;

    H5OFFSET_TYPE nOffset = 0;
    for (const auto &row : rows)
    {
        H5OFFSET_TYPE offset[] = {nOffset};
        bRet = bRet &&
               H5_CHECK(H5Sselect_hyperslab(hFileSpace, H5S_SELECT_SET, offset,
                                            nullptr, count, nullptr)) >= 0 &&
               H5_CHECK(H5Dwrite(hDatasetID, hDataType, hMemSpace, hFileSpace,
                                 H5P_DEFAULT, row.data())) >= 0;
        ++nOffset;
    }

    return bRet;
}
