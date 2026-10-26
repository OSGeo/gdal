/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "srsinfo" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "gdalalg_srsinfo.h"

#include "gdal_priv.h"

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                         GDALSRSInfoAlgorithm                         */
/************************************************************************/

GDALSRSInfoAlgorithm::GDALSRSInfoAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL),
      m_outputTypeStr(OUTPUT_TYPE_DEFAULT_STR),
      m_outputType(OutputType::OUTPUT_TYPE_DEFAULT)
{
    AddArg(GDAL_ARG_NAME_INPUT_CRS, 's', _("SRS"), &m_srs)
        .SetIsCRSArg()
        .SetRequired()
        .SetPositional();

    AddArg("validate", 'v', _("Validate the SRS"), &m_validate)
        .AddShortNameAlias('V');
    AddArg("epsg", 'e', _("Search for EPSG number(s) corresponding to SRS"),
           &m_listEpsgNumbersForSrs);
    AddArg("single-line", 'l', _("Print WKT on a single line"),
           &m_singleLineOutput);
    AddArg("output-type", 'o', _("Output type"), &m_outputTypeStr)
        .SetDefault(OUTPUT_TYPE_DEFAULT_STR)
        .SetChoices({OUTPUT_TYPE_DEFAULT_STR, OUTPUT_TYPE_ALL_STR,
                     OUTPUT_TYPE_WKT_ALL_STR, OUTPUT_TYPE_WKT_LATEST_STR,
                     OUTPUT_TYPE_PROJJSON_STR, OUTPUT_TYPE_PROJ4_STR,
                     OUTPUT_TYPE_WKT1_STR, OUTPUT_TYPE_WKT_SIMPLE_STR,
                     OUTPUT_TYPE_WKT_NOCT_STR, OUTPUT_TYPE_WKT_ESRI_STR,
                     OUTPUT_TYPE_WKT2_STR, OUTPUT_TYPE_WKT2_2015_STR,
                     OUTPUT_TYPE_WKT2_2019_STR, OUTPUT_TYPE_MAPINFO_STR,
                     OUTPUT_TYPE_XML_STR})
        .AddAction(
            [this]()
            {
                // Default is "proj4" and "wkt2"
                if (m_outputTypeStr == OUTPUT_TYPE_DEFAULT_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_DEFAULT;
                }
                // Latest and 2019
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT_LATEST_STR ||
                         m_outputTypeStr == OUTPUT_TYPE_WKT2_2019_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT2_2019;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_ALL_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_ALL;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT_ALL_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT_ALL;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_PROJJSON_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_PROJJSON;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_PROJ4_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_PROJ4;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT1_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT1;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT_SIMPLE_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT_SIMPLE;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT_NOCT_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT_NOCT;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT_ESRI_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT_ESRI;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT2_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT2;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT2_2015_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT2_2015;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_MAPINFO_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_MAPINFO;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_XML_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_XML;
                }
            });
}

bool GDALSRSInfoAlgorithm::RunImpl(GDALProgressFunc, void *)
{

    struct SRSMatch
    {
        SRSMatch(std::unique_ptr<OGRSpatialReference> oSRSIn, int nConfidenceIn)
            : oSRS(std::move(oSRSIn)), nConfidence(nConfidenceIn)
        {
        }

        std::unique_ptr<OGRSpatialReference> oSRS;
        int nConfidence = 0;
    };

    std::vector<SRSMatch> aoSRS;

    std::unique_ptr<OGRSpatialReference> oSRS;
    if (oSRS->SetFromUserInput(m_srs.c_str()) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse SRS: %s",
                 m_srs.c_str());
        return false;
    }

    int nEntries = 0;

    if (m_listEpsgNumbersForSrs)
    {
        int *panConfidence = nullptr;

        OGRSpatialReferenceH *pahSRS = nullptr;
        pahSRS =
            OSRFindMatches(reinterpret_cast<OGRSpatialReferenceH>(
                               const_cast<OGRSpatialReference *>(oSRS.get())),
                           nullptr, &nEntries, &panConfidence);

        for (int i = 0; i < nEntries; i++)
        {
            OGRSpatialReferenceH hSRS = pahSRS[i];
            std::unique_ptr<OGRSpatialReference> srs(
                OGRSpatialReference::FromHandle(hSRS));
            aoSRS.emplace_back(std::move(srs), panConfidence[i]);
        }
        CPLFree(panConfidence);
    }
    else
    {
        aoSRS.emplace_back(std::move(oSRS), 100);
    }

    for (const auto &srsMatch : aoSRS)
    {
        std::string osIdentifier;
        if (nEntries)
        {
            if (srsMatch.nConfidence != 100)
            {
                printf("Confidence in this match: %d %%\n",
                       srsMatch.nConfidence);
            }

            osIdentifier = srsMatch.oSRS->GetAuthorityName();
            if (const std::string osCode = srsMatch.oSRS->GetAuthorityCode();
                !osCode.empty())
            {
                if (!osIdentifier.empty())
                    osIdentifier += ":";
                osIdentifier += osCode;
            }
        }

        if (m_validate)
        {
            const OGRErr eErr = srsMatch.oSRS->Validate();
            if (eErr != OGRERR_NONE)
            {
                printf(_("\nValidate Fails"));
                if (eErr == OGRERR_CORRUPT_DATA)
                    printf(_(" - SRS is not well formed"));
                else if (eErr == OGRERR_UNSUPPORTED_SRS)
                    printf(_(" - contains non-standard PROJECTION[] values"));
                printf("\n");
            }
            else
                printf(_("\nValidate Succeeds\n"));
        }

        if (!m_singleLineOutput && m_listEpsgNumbersForSrs)
        {
            printf("\n%s\n", osIdentifier.c_str());
        }

        PrintSRSOutputTypes(srsMatch.oSRS.get(), m_outputType,
                            !m_singleLineOutput);
    }

    return true;
}

CPLErr GDALSRSInfoAlgorithm::PrintSRSInfo(const OGRSpatialReference *srs,
                                          OutputType outputType,
                                          bool prettyPrint,
                                          bool printSeparator) const
{
    char *pszOutput = nullptr;

    switch (outputType)
    {
        case OutputType::OUTPUT_TYPE_PROJ4:
        {
            // Note: this is inline (space instead of line break)
            if (printSeparator)
                printf("PROJ.4 : ");
            srs->exportToProj4(&pszOutput);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        case OutputType::OUTPUT_TYPE_PROJJSON:
        {
            if (printSeparator)
                printf("PROJJSON :\n");
            const char *const apszOptions[] = {
                prettyPrint ? "MULTILINE=YES" : "MULTILINE=NO", nullptr};
            srs->exportToPROJJSON(&pszOutput, apszOptions);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        case OutputType::OUTPUT_TYPE_WKT1:
        {
            if (printSeparator)
                printf("WKT1 :\n");
            const char *const apszOptions[] = {
                "FORMAT=WKT1_GDAL", prettyPrint ? "MULTILINE=YES" : nullptr,
                nullptr};
            srs->exportToWkt(&pszOutput, apszOptions);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        case OutputType::OUTPUT_TYPE_WKT_SIMPLE:
        {
            if (printSeparator)
                printf("OGC WKT1 (simple) :\n");
            const char *const apszOptions[] = {
                "FORMAT=WKT1_SIMPLE", prettyPrint ? "MULTILINE=YES" : nullptr,
                nullptr};
            srs->exportToWkt(&pszOutput, apszOptions);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        case OutputType::OUTPUT_TYPE_WKT_NOCT:
        {
            if (printSeparator)
                printf("OGC WKT1 (no CT) :\n");
            const char *const apszOptions[] = {
                "FORMAT=SFSQL", prettyPrint ? "MULTILINE=YES" : nullptr,
                nullptr};
            srs->exportToWkt(&pszOutput, apszOptions);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        case OutputType::OUTPUT_TYPE_WKT_ESRI:
        {
            if (printSeparator)
                printf("ESRI WKT1 :\n");
            const char *const apszOptions[] = {
                "FORMAT=WKT1_ESRI", prettyPrint ? "MULTILINE=YES" : nullptr,
                nullptr};
            srs->exportToWkt(&pszOutput, apszOptions);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        case OutputType::OUTPUT_TYPE_WKT2_2015:
        {
            if (printSeparator)
                printf("OGC WKT2:2015 :\n");
            const char *const apszOptions[] = {
                "FORMAT=WKT2_2015", prettyPrint ? "MULTILINE=YES" : nullptr,
                nullptr};
            srs->exportToWkt(&pszOutput, apszOptions);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        case OutputType::OUTPUT_TYPE_WKT2:
        case OutputType::OUTPUT_TYPE_WKT2_2019:
        {
            if (printSeparator)
                printf("OGC WKT2:2019 :\n");
            const char *const apszOptions[] = {
                "FORMAT=WKT2_2019", prettyPrint ? "MULTILINE=YES" : nullptr,
                nullptr};
            srs->exportToWkt(&pszOutput, apszOptions);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        case OutputType::OUTPUT_TYPE_MAPINFO:
        {
            if (printSeparator)
                printf("MapInfo :\n");
            srs->exportToMICoordSys(&pszOutput);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        case OutputType::OUTPUT_TYPE_XML:
        {
            if (printSeparator)
                printf("XML :\n");
            srs->exportToXML(&pszOutput);
            printf("%s\n", pszOutput ? pszOutput : "(error)");
            break;
        }
        default:
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unsupported output type");
            return CE_Failure;
        }
    }

    CPLFree(pszOutput);
    return CE_None;
}

void GDALSRSInfoAlgorithm::PrintSRSOutputTypes(const OGRSpatialReference *srs,
                                               OutputType outputTypes,
                                               bool prettyPrint) const
{
    for (uint32_t i = 0; static_cast<uint32_t>(1 << i) <
                         static_cast<uint32_t>(OutputType::OUTPUT_TYPE_LAST);
         i++)
    {
        if (static_cast<uint32_t>(outputTypes) & (1 << i))
        {
            if (prettyPrint)
                printf("\n");
            PrintSRSInfo(srs, static_cast<OutputType>(1 << i), prettyPrint,
                         prettyPrint);
        }
    }
}

//! @endcond
