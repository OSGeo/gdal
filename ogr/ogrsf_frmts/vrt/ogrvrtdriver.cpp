/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVRTDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_vrt.h"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

/************************************************************************/
/*                           OGRVRTErrorHandler()                       */
/************************************************************************/

static void CPL_STDCALL OGRVRTErrorHandler(CPL_UNUSED CPLErr eErr,
                                           CPL_UNUSED CPLErrorNum nType,
                                           const char *pszMsg)
{
    std::vector<CPLString> *paosErrors =
        static_cast<std::vector<CPLString> *>(CPLGetErrorHandlerUserData());
    paosErrors->push_back(pszMsg);
}

/************************************************************************/
/*                         OGRVRTDriverIdentify()                       */
/************************************************************************/

static int OGRVRTDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (!poOpenInfo->bStatOK)
    {
        // Are we being passed the XML definition directly?
        // Skip any leading spaces/blanks.
        const char *pszTestXML = poOpenInfo->pszFilename;
        while (*pszTestXML != '\0' &&
               isspace(static_cast<unsigned char>(*pszTestXML)))
            pszTestXML++;
        if (STARTS_WITH_CI(pszTestXML, "<OGRVRTDataSource>"))
        {
            return TRUE;
        }
        return FALSE;
    }

    return poOpenInfo->fpL != nullptr &&
           strstr(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                  "<OGRVRTDataSource") != nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRVRTDriverOpen(GDALOpenInfo *poOpenInfo)

{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (!OGRVRTDriverIdentify(poOpenInfo))
        return nullptr;
#endif

    // Are we being passed the XML definition directly?
    // Skip any leading spaces/blanks.
    const char *pszTestXML = poOpenInfo->pszFilename;
    while (*pszTestXML != '\0' &&
           isspace(static_cast<unsigned char>(*pszTestXML)))
        pszTestXML++;

    char *pszXML = nullptr;
    if (STARTS_WITH_CI(pszTestXML, "<OGRVRTDataSource>"))
    {
        pszXML = CPLStrdup(pszTestXML);
    }

    // Open file and check if it contains appropriate XML.
    else
    {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        if (poOpenInfo->fpL == nullptr)
            return nullptr;
#endif
        VSIStatBufL sStatBuf;
        if (VSIStatL(poOpenInfo->pszFilename, &sStatBuf) != 0)
        {
            return nullptr;
        }
        if (sStatBuf.st_size > 10 * 1024 * 1024 &&
            !CPLTestBool(CPLGetConfigOption("OGR_VRT_FORCE_LOADING", "NO")))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Suscipicously long VRT file. If you really want to "
                     "open it, define OGR_VRT_FORCE_LOADING=YES as "
                     "configuration option");
            return nullptr;
        }
        if (static_cast<uint64_t>(sStatBuf.st_size) >
            std::numeric_limits<size_t>::max() - 1)
        {
            return nullptr;
        }

        // It is the right file, now load the full XML definition.
        const size_t nLen = static_cast<size_t>(sStatBuf.st_size);

        pszXML = static_cast<char *>(VSI_MALLOC_VERBOSE(nLen + 1));
        if (pszXML == nullptr)
            return nullptr;

        pszXML[nLen] = '\0';
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
        if (VSIFReadL(pszXML, 1, nLen, poOpenInfo->fpL) != nLen)
        {
            CPLFree(pszXML);
            return nullptr;
        }
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
    }

    // Parse the XML.
    CPLXMLNode *psTree = CPLParseXMLString(pszXML);

    if (psTree == nullptr)
    {
        CPLFree(pszXML);
        return nullptr;
    }

    // XML Validation.
    if (CPLTestBool(CPLGetConfigOption("GDAL_XML_VALIDATION", "YES")))
    {
        const char *pszXSD = CPLFindFile("gdal", "ogrvrt.xsd");
        if (pszXSD != nullptr)
        {
            std::vector<CPLString> aosErrors;
            CPLPushErrorHandlerEx(OGRVRTErrorHandler, &aosErrors);
            const int bRet = CPLValidateXML(pszXML, pszXSD, nullptr);
            CPLPopErrorHandler();
            if (!bRet)
            {
                if (!aosErrors.empty() &&
                    strstr(aosErrors[0].c_str(), "missing libxml2 support") ==
                        nullptr)
                {
                    for (size_t i = 0; i < aosErrors.size(); i++)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined, "%s",
                                 aosErrors[i].c_str());
                    }
                }
            }
            CPLErrorReset();
        }
    }
    CPLFree(pszXML);

    // Create a virtual datasource configured based on this XML input.
    OGRVRTDataSource *poDS = new OGRVRTDataSource(
        static_cast<GDALDriver *>(GDALGetDriverByName("OGR_VRT")));

    // psTree is owned by poDS.
    if (!poDS->Initialize(psTree, poOpenInfo->pszFilename,
                          poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRVRT()                           */
/************************************************************************/

void RegisterOGRVRT()

{
    if (GDALGetDriverByName("OGR_VRT") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("OGR_VRT");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "VRT - Virtual Datasource");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "vrt");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/vrt.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_READ, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "WidthPrecision Nullable Unique Default "
                              "Comment AlternativeName");

    poDriver->pfnOpen = OGRVRTDriverOpen;
    poDriver->pfnIdentify = OGRVRTDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
