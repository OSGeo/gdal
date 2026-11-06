/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "crsinfo" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2024, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "gdalalg_crs_info.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "cpl_port.h"

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                         GDALCRSInfoAlgorithm                         */
/************************************************************************/

GDALCRSInfoAlgorithm::GDALCRSInfoAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL),
      m_outputTypeStr(OUTPUT_TYPE_DEFAULT_STR),
      m_outputType(OutputType::OUTPUT_TYPE_DEFAULT)
{

    AddProgressArg(/* hidden */ true);
    AddOutputStringArg(&m_output);
    AddStdoutArg(&m_stdout);
    AddArg(GDAL_ARG_NAME_INPUT_CRS, 's', _("Input CRS"), &m_crs)
        .SetIsCRSArg()
        .SetRequired()
        .SetPositional();

    AddArg("validate", 0, _("Validate the CRS"), &m_validate);
    AddArg("identify", 'i', _("Search for EPSG number(s) corresponding to CRS"),
           &m_identifyCRS);
    AddArg("single-line", 0, _("Print WKT or PROJJSON on a single line"),
           &m_singleLineOutput);
    AddArg("output-type", 'o', _("Output type"), &m_outputTypeStr)
        .SetDefault(OUTPUT_TYPE_DEFAULT_STR)
        .SetChoices({OUTPUT_TYPE_DEFAULT_STR, OUTPUT_TYPE_ALL_STR,
                     OUTPUT_TYPE_WKT_ALL_STR, OUTPUT_TYPE_WKT_LATEST_STR,
                     OUTPUT_TYPE_PROJJSON_STR, OUTPUT_TYPE_PROJ4_STR,
                     OUTPUT_TYPE_WKT1_STR, OUTPUT_TYPE_WKT_ESRI_STR,
                     OUTPUT_TYPE_WKT2_STR, OUTPUT_TYPE_MAPINFO_STR,
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
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT_LATEST_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT2;
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
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT_ESRI_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT_ESRI;
                }
                else if (m_outputTypeStr == OUTPUT_TYPE_WKT2_STR)
                {
                    m_outputType = OutputType::OUTPUT_TYPE_WKT2;
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

void GDALCRSInfoAlgorithm::Print(const char *str) const
{
    if (m_stdout)
        fwrite(str, 1, strlen(str), stdout);
    else
        m_output += str;
}

bool GDALCRSInfoAlgorithm::RunImpl(CPL_UNUSED GDALProgressFunc pfnProgress,
                                   CPL_UNUSED void *pProgressData)
{

    struct CRSMatch
    {
        CRSMatch(std::unique_ptr<OGRSpatialReference> oCRSIn, int nConfidenceIn)
            : oCRS(std::move(oCRSIn)), nConfidence(nConfidenceIn)
        {
        }

        std::unique_ptr<OGRSpatialReference> oCRS;
        int nConfidence = 0;
    };

    std::vector<CRSMatch> aoCRS;

    std::unique_ptr<OGRSpatialReference> oCRS =
        std::make_unique<OGRSpatialReference>();

    if (oCRS->SetFromUserInput(m_crs.c_str()) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse CRS: %s",
                 m_crs.c_str());
        return false;
    }

    int nEntries = 0;
    CPLErr eErr = CE_None;

    if (m_identifyCRS)
    {
        int *panConfidence = nullptr;

        OGRSpatialReferenceH *pahCRS = nullptr;
        pahCRS =
            OSRFindMatches(reinterpret_cast<OGRSpatialReferenceH>(
                               const_cast<OGRSpatialReference *>(oCRS.get())),
                           nullptr, &nEntries, &panConfidence);

        for (int i = 0; i < nEntries; i++)
        {
            OGRSpatialReferenceH hCRS = pahCRS[i];
            std::unique_ptr<OGRSpatialReference> crs(
                OGRSpatialReference::FromHandle(hCRS));
            aoCRS.emplace_back(std::move(crs), panConfidence[i]);
            pahCRS[i] = nullptr;  // Ownership transferred to unique_ptr
        }
        CPLFree(panConfidence);
        CPLFree(pahCRS);
    }
    else
    {
        aoCRS.emplace_back(std::move(oCRS), 100);
    }

    for (const auto &crsMatch : aoCRS)
    {
        std::string osIdentifier;
        if (nEntries)
        {
            if (crsMatch.nConfidence != 100)
            {
                Print(_("Confidence in this match: "));
                Print(std::to_string(crsMatch.nConfidence).c_str());
                Print("\n");
            }

            osIdentifier = crsMatch.oCRS->GetAuthorityName();
            if (const std::string osCode = crsMatch.oCRS->GetAuthorityCode();
                !osCode.empty())
            {
                if (!osIdentifier.empty())
                    osIdentifier += ":";
                osIdentifier += osCode;
            }
        }

        if (m_validate)
        {
            const OGRErr retErr = crsMatch.oCRS->Validate();
            if (retErr != OGRERR_NONE)
            {
                Print(_("\nValidate Fails"));
                if (retErr == OGRERR_CORRUPT_DATA)
                    Print(_(" - CRS is not well formed"));
                else if (retErr == OGRERR_UNSUPPORTED_SRS)
                    Print(_(" - contains non-standard PROJECTION[] values"));
                Print("\n");
                eErr = CE_Failure;
            }
            else
            {
                Print(_("\nValidate Succeeds\n"));
            }
        }

        if (!m_singleLineOutput && m_identifyCRS)
        {
            Print("\n");
            Print(osIdentifier.c_str());
            Print("\n");
        }

        if (const auto retErr = PrintCRSOutputTypes(
                crsMatch.oCRS.get(), m_outputType, !m_singleLineOutput);
            retErr != CE_None)
        {
            eErr = retErr;
        }
    }

    return eErr == CE_None;
}

CPLErr GDALCRSInfoAlgorithm::PrintCRSInfo(const OGRSpatialReference *crs,
                                          OutputType outputType,
                                          bool prettyPrint,
                                          bool printSeparator) const
{
    char *pszOutput = nullptr;

    CPLErr eErr = CE_None;
    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);

    const auto PrintError = [&]()
    {
        eErr = CE_Failure;
        Print("(error) ");
        Print(CPLGetLastErrorMsg());
    };

    switch (outputType)
    {
        case OutputType::OUTPUT_TYPE_PROJ4:
        {
            // Note: this is inline (space instead of line break)
            if (printSeparator)
                Print("PROJ.4 : ");
            crs->exportToProj4(&pszOutput);
            if (pszOutput)
            {
                Print(pszOutput);
            }
            else
            {
                PrintError();
            }
            Print("\n");
            break;
        }
        case OutputType::OUTPUT_TYPE_PROJJSON:
        {
            if (printSeparator)
                Print("PROJJSON :\n");
            const char *const apszOptions[] = {
                prettyPrint ? "MULTILINE=YES" : "MULTILINE=NO", nullptr};
            crs->exportToPROJJSON(&pszOutput, apszOptions);
            if (pszOutput)
            {
                Print(pszOutput);
            }
            else
            {
                PrintError();
            }
            Print("\n");
            break;
        }
        case OutputType::OUTPUT_TYPE_WKT1:
        {
            if (printSeparator)
                Print("WKT1 :\n");
            const char *const apszOptions[] = {
                "FORMAT=WKT1_GDAL", prettyPrint ? "MULTILINE=YES" : nullptr,
                nullptr};
            crs->exportToWkt(&pszOutput, apszOptions);
            if (pszOutput)
            {
                Print(pszOutput);
            }
            else
            {
                PrintError();
            }
            Print("\n");
            break;
        }
        case OutputType::OUTPUT_TYPE_WKT_ESRI:
        {
            if (printSeparator)
                Print("ESRI WKT1 :\n");
            const char *const apszOptions[] = {
                "FORMAT=WKT1_ESRI", prettyPrint ? "MULTILINE=YES" : nullptr,
                nullptr};
            crs->exportToWkt(&pszOutput, apszOptions);
            if (pszOutput)
            {
                Print(pszOutput);
            }
            else
            {
                PrintError();
            }
            break;
        }
        case OutputType::OUTPUT_TYPE_WKT2:
        {
            if (printSeparator)
                Print("OGC WKT2:2019 :\n");
            const char *const apszOptions[] = {
                "FORMAT=WKT2_2019", prettyPrint ? "MULTILINE=YES" : nullptr,
                nullptr};
            crs->exportToWkt(&pszOutput, apszOptions);
            if (pszOutput)
            {
                Print(pszOutput);
            }
            else
            {
                PrintError();
            }
            Print("\n");
            break;
        }
        case OutputType::OUTPUT_TYPE_MAPINFO:
        {
            if (printSeparator)
                Print("MapInfo :\n");
            crs->exportToMICoordSys(&pszOutput);
            if (pszOutput)
            {
                Print(pszOutput);
            }
            else
            {
                PrintError();
            }
            Print("\n");
            break;
        }
        case OutputType::OUTPUT_TYPE_XML:
        {
            if (printSeparator)
                Print("XML :\n");
            crs->exportToXML(&pszOutput);
            if (pszOutput)
            {
                Print(pszOutput);
            }
            else
            {
                PrintError();
            }
            Print("\n");
            break;
        }
        default:
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unsupported output type");
            return CE_Failure;
        }
    }

    CPLFree(pszOutput);
    return eErr;
}

CPLErr GDALCRSInfoAlgorithm::PrintCRSOutputTypes(const OGRSpatialReference *crs,
                                                 OutputType outputTypes,
                                                 bool prettyPrint) const
{

    CPLErr eErr = CE_None;
    // Count requested formats
    std::vector<OutputType> requestedFormats;
    for (uint32_t i = 0; static_cast<uint32_t>(1 << i) <=
                         static_cast<uint32_t>(OutputType::OUTPUT_TYPE_LAST);
         i++)
    {
        if (static_cast<uint32_t>(outputTypes) & (1 << i))
        {
            requestedFormats.push_back(static_cast<OutputType>(1 << i));
        }
    }

    const size_t numFormats = requestedFormats.size();

    for (const auto &requestedFormat : requestedFormats)
    {
        if (numFormats > 1 && prettyPrint)
            Print("\n");
        if (const auto retErr = PrintCRSInfo(crs, requestedFormat, prettyPrint,
                                             numFormats > 1 && prettyPrint);
            retErr != CE_None)
        {
            eErr = retErr;
        }
    }
    return eErr;
}

//! @endcond
