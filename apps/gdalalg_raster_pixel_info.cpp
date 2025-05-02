/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster pixelinfo" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_pixel_info.h"

#include "cpl_conv.h"
#include "cpl_json.h"
#include "cpl_minixml.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <cmath>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*       GDALRasterPixelInfoAlgorithm::GDALRasterPixelInfoAlgorithm()   */
/************************************************************************/

GDALRasterPixelInfoAlgorithm::GDALRasterPixelInfoAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddOutputFormatArg(&m_format)
        .SetDefault("geojson")
        .SetChoices("geojson", "csv")
        .SetHiddenChoices("json")
        .SetDefault(m_format);
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_dataset, GDAL_OF_RASTER).AddAlias("dataset");
    AddOutputStringArg(&m_output);

    AddBandArg(&m_band);
    AddArg("overview", 0, _("Which overview level of source file must be used"),
           &m_overview)
        .SetMinValueIncluded(0);

    AddArg("position", 'p', _("Pixel position"), &m_pos)
        .AddAlias("pos")
        .SetMetaVar("<column,line> or <X,Y>")
        .SetPositional()
        .AddValidationAction(
            [this]
            {
                if ((m_pos.size() % 2) != 0)
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "An even number of values must be specified "
                                "for 'position' argument");
                    return false;
                }
                return true;
            });
    AddArg("position-crs", 0, _("CRS of position"), &m_posCrs)
        .SetIsCRSArg(false, {"pixel", "dataset"})
        .SetDefault("pixel")
        .AddHiddenAlias("l_srs");

    AddArg("resampling", 'r', _("Resampling algorithm for interpolation"),
           &m_resampling)
        .SetDefault(m_resampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline")
        .SetHiddenChoices("near");

    AddValidationAction(
        [this]
        {
            if (auto poSrcDS = m_dataset.GetDatasetRef())
            {
                const int nOvrCount =
                    poSrcDS->GetRasterBand(1)->GetOverviewCount();
                if (m_overview >= 0 && poSrcDS->GetRasterCount() > 0 &&
                    m_overview >= nOvrCount)
                {
                    if (nOvrCount == 0)
                    {
                        ReportError(
                            CE_Failure, CPLE_IllegalArg,
                            "Source dataset has no overviews. "
                            "Argument 'overview' must not be specified.");
                    }
                    else
                    {
                        ReportError(
                            CE_Failure, CPLE_IllegalArg,
                            "Source dataset has only %d overview level%s. "
                            "'overview' "
                            "value must be strictly lower than this number.",
                            nOvrCount, nOvrCount > 1 ? "s" : "");
                    }
                    return false;
                }
            }
            return true;
        });
}

/************************************************************************/
/*              GDALRasterPixelInfoAlgorithm::PrintLine()               */
/************************************************************************/

void GDALRasterPixelInfoAlgorithm::PrintLine(const std::string &str)
{
    if (IsCalledFromCommandLine())
    {
        printf("%s\n", str.c_str());
    }
    else
    {
        m_output += str;
        m_output += '\n';
    }
}

/************************************************************************/
/*                GDALRasterPixelInfoAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALRasterPixelInfoAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    auto poSrcDS = m_dataset.GetDatasetRef();
    CPLAssert(poSrcDS);

    if (m_pos.empty() && !IsCalledFromCommandLine())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Argument 'position' must be specified.");
        return false;
    }

    const GDALRIOResampleAlg eInterpolation =
        GDALRasterIOGetResampleAlg(m_resampling.c_str());

    const auto poSrcCRS = poSrcDS->GetSpatialRef();
    double adfGeoTransform[6] = {0, 0, 0, 0, 0, 0};
    const bool bHasGT = poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None;
    double adfInvGeoTransform[6] = {0, 0, 0, 0, 0, 0};

    if (m_posCrs != "pixel")
    {
        if (!poSrcCRS)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset has no CRS. Only 'position-crs' = 'pixel' is "
                        "supported.");
            return false;
        }

        if (!bHasGT)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot get geotransform");
            return false;
        }

        if (!GDALInvGeoTransform(adfGeoTransform, adfInvGeoTransform))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot invert geotransform");
            return false;
        }
    }

    std::unique_ptr<OGRCoordinateTransformation> poCT;
    if (m_posCrs != "pixel" && m_posCrs != "dataset")
    {
        OGRSpatialReference oUserCRS;
        oUserCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        // Already validated due SetIsCRSArg()
        CPL_IGNORE_RET_VAL(oUserCRS.SetFromUserInput(m_posCrs.c_str()));
        poCT.reset(OGRCreateCoordinateTransformation(&oUserCRS, poSrcCRS));
        if (!poCT)
            return false;
    }

    if (m_band.empty())
    {
        for (int i = 1; i <= poSrcDS->GetRasterCount(); ++i)
            m_band.push_back(i);
    }

    if (m_format == "csv")
    {
        std::string line = "input_x,input_y,extra_input,column,line";
        for (int nBand : m_band)
        {
            auto hBand =
                GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(nBand));
            const bool bIsComplex = CPL_TO_BOOL(
                GDALDataTypeIsComplex(GDALGetRasterDataType(hBand)));
            if (bIsComplex)
                line +=
                    CPLSPrintf(",band_%d_real_value,band_%d_imaginary_value",
                               nBand, nBand);
            else
            {
                line += CPLSPrintf(",band_%d_raw_value,band_%d_unscaled_value",
                                   nBand, nBand);
            }
        }
        PrintLine(line);
    }

    const bool isInteractive =
        m_pos.empty() && IsCalledFromCommandLine() && CPLIsInteractive(stdin);

    CPLJSONObject oCollection;
    oCollection.Add("type", "FeatureCollection");
    std::unique_ptr<OGRCoordinateTransformation> poCTToWGS84;
    bool canOutputGeoJSONGeom = false;
    if (poSrcCRS && bHasGT)
    {
        const char *pszAuthName = poSrcCRS->GetAuthorityName(nullptr);
        const char *pszAuthCode = poSrcCRS->GetAuthorityCode(nullptr);
        if (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
        {
            canOutputGeoJSONGeom = true;
            CPLJSONObject jCRS;
            CPLJSONObject oProperties;
            if (EQUAL(pszAuthCode, "4326"))
                oProperties.Add("name", "urn:ogc:def:crs:OGC:1.3:CRS84");
            else
                oProperties.Add(
                    "name",
                    std::string("urn:ogc:def:crs:EPSG::").append(pszAuthCode));
            jCRS.Add("type", "name");
            jCRS.Add("properties", oProperties);
            oCollection.Add("crs", jCRS);
        }
        else
        {
            OGRSpatialReference oCRS;
            oCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            oCRS.importFromEPSG(4326);
            poCTToWGS84.reset(
                OGRCreateCoordinateTransformation(poSrcCRS, &oCRS));
            if (poCTToWGS84)
            {
                canOutputGeoJSONGeom = true;
                CPLJSONObject jCRS;
                CPLJSONObject oProperties;
                oProperties.Add("name", "urn:ogc:def:crs:OGC:1.3:CRS84");
                jCRS.Add("type", "name");
                jCRS.Add("properties", oProperties);
                oCollection.Add("crs", jCRS);
            }
        }
    }
    CPLJSONArray oFeatures;
    oCollection.Add("features", oFeatures);

    char szLine[1024];
    int nLine = 0;
    size_t iVal = 0;
    do
    {
        double x = 0, y = 0;
        std::string osExtraContent;
        if (iVal + 1 < m_pos.size())
        {
            x = m_pos[iVal++];
            y = m_pos[iVal++];
        }
        else
        {
            if (CPLIsInteractive(stdin))
            {
                if (m_posCrs != "pixel")
                {
                    fprintf(stderr, "Enter X Y values separated by space, and "
                                    "press Return.\n");
                }
                else
                {
                    fprintf(stderr,
                            "Enter pixel line values separated by space, "
                            "and press Return.\n");
                }
            }

            if (fgets(szLine, sizeof(szLine) - 1, stdin) && szLine[0] != '\n')
            {
                const CPLStringList aosTokens(CSLTokenizeString(szLine));
                const int nCount = aosTokens.size();

                ++nLine;
                if (nCount < 2)
                {
                    fprintf(stderr, "Not enough values at line %d\n", nLine);
                    return false;
                }
                else
                {
                    x = CPLAtof(aosTokens[0]);
                    y = CPLAtof(aosTokens[1]);

                    for (int i = 2; i < nCount; ++i)
                    {
                        if (!osExtraContent.empty())
                            osExtraContent += ' ';
                        osExtraContent += aosTokens[i];
                    }
                    while (!osExtraContent.empty() &&
                           isspace(static_cast<int>(osExtraContent.back())))
                    {
                        osExtraContent.pop_back();
                    }
                }
            }
            else
            {
                break;
            }
        }

        const double xOri = x;
        const double yOri = y;
        double dfPixel{0}, dfLine{0};

        if (poCT)
        {
            if (!poCT->Transform(1, &x, &y, nullptr))
                return false;
        }

        if (m_posCrs != "pixel")
        {
            GDALApplyGeoTransform(adfInvGeoTransform, x, y, &dfPixel, &dfLine);
        }
        else
        {
            dfPixel = x;
            dfLine = y;
        }
        const int iPixel = static_cast<int>(
            std::clamp(std::floor(dfPixel), static_cast<double>(INT_MIN),
                       static_cast<double>(INT_MAX)));
        const int iLine = static_cast<int>(
            std::clamp(std::floor(dfLine), static_cast<double>(INT_MIN),
                       static_cast<double>(INT_MAX)));

        std::string line;
        CPLJSONObject oFeature;
        CPLJSONObject oProperties;
        if (m_format == "csv")
        {
            line = CPLSPrintf("%.17g,%.17g", xOri, yOri);
            line += ",\"";
            line += CPLString(osExtraContent).replaceAll('"', "\"\"");
            line += '"';
            line += CPLSPrintf(",%.17g,%.17g", dfPixel, dfLine);
        }
        else
        {
            oFeature.Add("type", "Feature");
            oFeature.Add("properties", oProperties);
            {
                CPLJSONArray oArray;
                oArray.Add(xOri);
                oArray.Add(yOri);
                oProperties.Add("input_coordinate", oArray);
            }
            if (!osExtraContent.empty())
                oProperties.Add("extra_content", osExtraContent);
            oProperties.Add("column", dfPixel);
            oProperties.Add("line", dfLine);
        }

        CPLJSONArray oBands;

        for (int nBand : m_band)
        {
            CPLJSONObject oBand;
            oBand.Add("band_number", nBand);

            auto hBand =
                GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(nBand));

            int iPixelToQuery = iPixel;
            int iLineToQuery = iLine;

            double dfPixelToQuery = dfPixel;
            double dfLineToQuery = dfLine;

            if (m_overview >= 0 && hBand != nullptr)
            {
                GDALRasterBandH hOvrBand = GDALGetOverview(hBand, m_overview);
                if (hOvrBand != nullptr)
                {
                    int nOvrXSize = GDALGetRasterBandXSize(hOvrBand);
                    int nOvrYSize = GDALGetRasterBandYSize(hOvrBand);
                    iPixelToQuery = static_cast<int>(
                        0.5 +
                        1.0 * iPixel / poSrcDS->GetRasterXSize() * nOvrXSize);
                    iLineToQuery = static_cast<int>(
                        0.5 +
                        1.0 * iLine / poSrcDS->GetRasterYSize() * nOvrYSize);
                    if (iPixelToQuery >= nOvrXSize)
                        iPixelToQuery = nOvrXSize - 1;
                    if (iLineToQuery >= nOvrYSize)
                        iLineToQuery = nOvrYSize - 1;
                    dfPixelToQuery =
                        dfPixel / poSrcDS->GetRasterXSize() * nOvrXSize;
                    dfLineToQuery =
                        dfLine / poSrcDS->GetRasterYSize() * nOvrYSize;
                }
                else
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot get overview %d of band %d", m_overview,
                                nBand);
                    return false;
                }
                hBand = hOvrBand;
            }

            double adfPixel[2] = {0, 0};
            const bool bIsComplex = CPL_TO_BOOL(
                GDALDataTypeIsComplex(GDALGetRasterDataType(hBand)));
            int bIgnored;
            const double dfOffset = GDALGetRasterOffset(hBand, &bIgnored);
            const double dfScale = GDALGetRasterScale(hBand, &bIgnored);
            if (GDALRasterInterpolateAtPoint(
                    hBand, dfPixelToQuery, dfLineToQuery, eInterpolation,
                    &adfPixel[0], &adfPixel[1]) == CE_None)
            {
                if (!bIsComplex)
                {
                    const double dfUnscaledVal =
                        adfPixel[0] * dfScale + dfOffset;
                    if (m_format == "csv")
                    {
                        line += CPLSPrintf(",%.17g", adfPixel[0]);
                        line += CPLSPrintf(",%.17g", dfUnscaledVal);
                    }
                    else
                    {
                        if (GDALDataTypeIsInteger(GDALGetRasterDataType(hBand)))
                        {
                            oBand.Add("raw_value",
                                      static_cast<GInt64>(adfPixel[0]));
                        }
                        else
                        {
                            oBand.Add("raw_value", adfPixel[0]);
                        }

                        oBand.Add("unscaled_value", dfUnscaledVal);
                    }
                }
                else
                {
                    if (m_format == "csv")
                    {
                        line += CPLSPrintf(",%.17g,%.17g", adfPixel[0],
                                           adfPixel[1]);
                    }
                    else
                    {
                        CPLJSONObject oValue;
                        oValue.Add("real", adfPixel[0]);
                        oValue.Add("imaginary", adfPixel[1]);
                        oBand.Add("value", oValue);
                    }
                }
            }
            else if (m_format == "csv")
            {
                line += ",,";
            }

            // Request location info for this location (just a few drivers,
            // like the VRT driver actually supports this).
            CPLString osItem;
            osItem.Printf("Pixel_%d_%d", iPixelToQuery, iLineToQuery);

            if (const char *pszLI =
                    GDALGetMetadataItem(hBand, osItem, "LocationInfo"))
            {
                CPLXMLTreeCloser oTree(CPLParseXMLString(pszLI));

                if (oTree && oTree->psChild != nullptr &&
                    oTree->eType == CXT_Element &&
                    EQUAL(oTree->pszValue, "LocationInfo"))
                {
                    CPLJSONArray oFiles;

                    for (const CPLXMLNode *psNode = oTree->psChild;
                         psNode != nullptr; psNode = psNode->psNext)
                    {
                        if (psNode->eType == CXT_Element &&
                            EQUAL(psNode->pszValue, "File") &&
                            psNode->psChild != nullptr)
                        {
                            char *pszUnescaped = CPLUnescapeString(
                                psNode->psChild->pszValue, nullptr, CPLES_XML);
                            oFiles.Add(pszUnescaped);
                            CPLFree(pszUnescaped);
                        }
                    }

                    oBand.Add("files", oFiles);
                }
                else
                {
                    oBand.Add("location_info", pszLI);
                }
            }

            oBands.Add(oBand);
        }

        if (m_format == "csv")
        {
            PrintLine(line);
        }
        else
        {
            oProperties.Add("bands", oBands);

            if (canOutputGeoJSONGeom)
            {
                x = dfPixel;
                y = dfLine;

                GDALApplyGeoTransform(adfGeoTransform, x, y, &x, &y);

                if (poCTToWGS84)
                    poCTToWGS84->Transform(1, &x, &y);

                CPLJSONObject oGeometry;
                oFeature.Add("geometry", oGeometry);
                oGeometry.Add("type", "Point");
                CPLJSONArray oCoordinates;
                oCoordinates.Add(x);
                oCoordinates.Add(y);
                oGeometry.Add("coordinates", oCoordinates);
            }
            else
            {
                oFeature.AddNull("geometry");
            }

            if (isInteractive)
            {
                CPLJSONDocument oDoc;
                oDoc.SetRoot(oFeature);
                printf("%s\n", oDoc.SaveAsString().c_str());
            }
            else
            {
                oFeatures.Add(oFeature);
            }
        }

    } while (m_pos.empty() || iVal + 1 < m_pos.size());

    if (m_format != "csv" && !isInteractive)
    {
        CPLJSONDocument oDoc;
        oDoc.SetRoot(oCollection);
        m_output = oDoc.SaveAsString();
    }

    return true;
}

//! @endcond
