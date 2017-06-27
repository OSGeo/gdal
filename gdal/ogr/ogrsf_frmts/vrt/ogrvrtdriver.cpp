/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVRTDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_vrt.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

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

static int OGRVRTDriverIdentify( GDALOpenInfo *poOpenInfo )
{
    if( !poOpenInfo->bStatOK )
    {
        // Are we being passed the XML definition directly?
        // Skip any leading spaces/blanks.
        const char *pszTestXML = poOpenInfo->pszFilename;
        while( *pszTestXML != '\0' &&
               isspace(static_cast<unsigned char>(*pszTestXML)) )
            pszTestXML++;
        if( STARTS_WITH_CI(pszTestXML, "<OGRVRTDataSource>") )
        {
            return TRUE;
        }
        return FALSE;
    }

    return poOpenInfo->fpL != NULL &&
        strstr(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
               "<OGRVRTDataSource") != NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRVRTDriverOpen( GDALOpenInfo *poOpenInfo )

{
    if( !OGRVRTDriverIdentify(poOpenInfo) )
        return NULL;

    // Are we being passed the XML definition directly?
    // Skip any leading spaces/blanks.
    const char *pszTestXML = poOpenInfo->pszFilename;
    while( *pszTestXML != '\0' &&
           isspace(static_cast<unsigned char>(*pszTestXML)) )
        pszTestXML++;

    char *pszXML = NULL;
    if( STARTS_WITH_CI(pszTestXML, "<OGRVRTDataSource>") )
    {
        pszXML = CPLStrdup(pszTestXML);
    }

    // Open file and check if it contains appropriate XML.
    else
    {
        VSIStatBufL sStatBuf;
        if( VSIStatL(poOpenInfo->pszFilename, &sStatBuf) != 0 ||
            sStatBuf.st_size > 1024 * 1024 )
        {
            CPLDebug("VRT", "Unreasonable long file, not likely really VRT");
            return NULL;
        }

        // It is the right file, now load the full XML definition.
        const int nLen = static_cast<int>(sStatBuf.st_size);

        pszXML = static_cast<char *>(VSI_MALLOC_VERBOSE(nLen + 1));
        if( pszXML == NULL )
            return NULL;

        pszXML[nLen] = '\0';
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
        if( static_cast<int>(
                VSIFReadL(pszXML, 1, nLen, poOpenInfo->fpL)) != nLen )
        {
            CPLFree(pszXML);
            return NULL;
        }
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = NULL;
    }

    // Parse the XML.
    CPLXMLNode *psTree = CPLParseXMLString(pszXML);

    if( psTree == NULL )
    {
        CPLFree(pszXML);
        return NULL;
    }

    // XML Validation.
    if( CPLTestBool(CPLGetConfigOption("GDAL_XML_VALIDATION", "YES")) )
    {
        const char *pszXSD = CPLFindFile("gdal", "ogrvrt.xsd");
        if( pszXSD != NULL )
        {
            std::vector<CPLString> aosErrors;
            CPLPushErrorHandlerEx(OGRVRTErrorHandler, &aosErrors);
            const int bRet = CPLValidateXML(pszXML, pszXSD, NULL);
            CPLPopErrorHandler();
            if( !bRet )
            {
                if( !aosErrors.empty() &&
                    strstr(aosErrors[0].c_str(), "missing libxml2 support") ==
                        NULL )
                {
                    for( size_t i = 0; i < aosErrors.size(); i++ )
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
    if( !poDS->Initialize(psTree, poOpenInfo->pszFilename,
                          poOpenInfo->eAccess == GA_Update) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRVRT()                           */
/************************************************************************/

void RegisterOGRVRT()

{
    if( GDALGetDriverByName("OGR_VRT") != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("OGR_VRT");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "VRT - Virtual Datasource");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "vrt");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drv_vrt.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = OGRVRTDriverOpen;
    poDriver->pfnIdentify = OGRVRTDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
