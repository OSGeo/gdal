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

#include "s100.h"
#include "hdf5dataset.h"

#include "proj.h"
#include "proj_experimental.h"
#include "proj_constants.h"
#include "ogr_proj_p.h"

#include <algorithm>
#include <cmath>

/************************************************************************/
/*                       S100BaseDataset()                              */
/************************************************************************/

S100BaseDataset::S100BaseDataset(const std::string &osFilename)
    : m_osFilename(osFilename)
{
}

/************************************************************************/
/*                              Init()                                  */
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
/*                         GetSpatialRef()                              */
/************************************************************************/

const OGRSpatialReference *S100BaseDataset::GetSpatialRef() const
{
    if (!m_oSRS.IsEmpty())
        return &m_oSRS;
    return GDALPamDataset::GetSpatialRef();
}

/************************************************************************/
/*                         GetFileList()                                */
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

bool S100ReadSRS(const GDALGroup *poRootGroup, OGRSpatialReference &oSRS)
{
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
/*               S100GetNumPointsLongitudinalLatitudinal()              */
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

        gt[0] = poOriginX->ReadAsDouble();
        gt[3] = poOriginY->ReadAsDouble() +
                (bNorthUp ? dfSpacingY * (nNumPointsLatitudinal - 1) : 0);
        gt[1] = dfSpacingX;
        gt[5] = bNorthUp ? -dfSpacingY : dfSpacingY;

        // From pixel-center convention to pixel-corner convention
        gt[0] -= gt[1] / 2;
        gt[3] -= gt[5] / 2;

        return true;
    }
    return false;
}

/************************************************************************/
/*                        S100GetDimensions()                           */
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
/*                       S100ReadVerticalDatum()                        */
/************************************************************************/

void S100ReadVerticalDatum(GDALMajorObject *poMO, const GDALGroup *poGroup)
{
    // https://iho.int/uploads/user/pubs/standards/s-100/S-100_5.2.0_Final_Clean.pdf
    // Table 10c-25 - Vertical and sounding datum, page 53
    static const struct
    {
        int nCode;
        const char *pszMeaning;
        const char *pszAbbrev;
    } asVerticalDatums[] = {
        {1, "meanLowWaterSprings", "MLWS"},
        {2, "meanLowerLowWaterSprings", nullptr},
        {3, "meanSeaLevel", "MSL"},
        {4, "lowestLowWater", nullptr},
        {5, "meanLowWater", "MLW"},
        {6, "lowestLowWaterSprings", nullptr},
        {7, "approximateMeanLowWaterSprings", nullptr},
        {8, "indianSpringLowWater", nullptr},
        {9, "lowWaterSprings", nullptr},
        {10, "approximateLowestAstronomicalTide", nullptr},
        {11, "nearlyLowestLowWater", nullptr},
        {12, "meanLowerLowWater", "MLLW"},
        {13, "lowWater", "LW"},
        {14, "approximateMeanLowWater", nullptr},
        {15, "approximateMeanLowerLowWater", nullptr},
        {16, "meanHighWater", "MHW"},
        {17, "meanHighWaterSprings", "MHWS"},
        {18, "highWater", "HW"},
        {19, "approximateMeanSeaLevel", nullptr},
        {20, "highWaterSprings", nullptr},
        {21, "meanHigherHighWater", "MHHW"},
        {22, "equinoctialSpringLowWater", nullptr},
        {23, "lowestAstronomicalTide", "LAT"},
        {24, "localDatum", nullptr},
        {25, "internationalGreatLakesDatum1985", nullptr},
        {26, "meanWaterLevel", nullptr},
        {27, "lowerLowWaterLargeTide", nullptr},
        {28, "higherHighWaterLargeTide", nullptr},
        {29, "nearlyHighestHighWater", nullptr},
        {30, "highestAstronomicalTide", "HAT"},
        {44, "balticSeaChartDatum2000", nullptr},
        {46, "internationalGreatLakesDatum2020", nullptr},
        {47, "seaFloor", nullptr},
        {48, "seaSurface", nullptr},
        {49, "hydrographicZero", nullptr},
    };

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
        if (nVerticalDatumReference == 1)
        {
            bool bFound = false;
            const auto nVal = poVerticalDatum->ReadAsInt();
            for (const auto &sVerticalDatum : asVerticalDatums)
            {
                if (sVerticalDatum.nCode == nVal)
                {
                    bFound = true;
                    poMO->GDALMajorObject::SetMetadataItem(
                        S100_VERTICAL_DATUM_MEANING, sVerticalDatum.pszMeaning);
                    if (sVerticalDatum.pszAbbrev)
                        poMO->GDALMajorObject::SetMetadataItem(
                            S100_VERTICAL_DATUM_ABBREV,
                            sVerticalDatum.pszAbbrev);
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
            poMO->GDALMajorObject::SetMetadataItem("VERTICAL_DATUM_EPSG_CODE",
                                                   CPLSPrintf("%d", nVal));
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
/*                         S100ReadMetadata()                           */
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
