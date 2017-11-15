/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset class for WCS 1.0.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2017, Ari Jolma
 * Copyright (c) 2017, Finnish Environment Institute
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

#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_http.h"
#include "gmlutils.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "gmlcoverage.h"

#include <algorithm>
#include <dirent.h>

#include "wcsdataset.h"
#include "wcsutils.h"

/************************************************************************/
/*                         GetExtent()                                  */
/*                                                                      */
/************************************************************************/

std::vector<double> WCSDataset100::GetExtent(int nXOff, int nYOff,
                                             int nXSize, int nYSize,
                                             CPL_UNUSED int nBufXSize, CPL_UNUSED int nBufYSize)
{
    std::vector<double> extent;
    // WCS 1.0 extents are the outer edges of outer pixels.
    extent.push_back(adfGeoTransform[0] +
                     (nXOff) * adfGeoTransform[1]);
    extent.push_back(adfGeoTransform[3] +
                     (nYOff + nYSize) * adfGeoTransform[5]);
    extent.push_back(adfGeoTransform[0] +
                     (nXOff + nXSize) * adfGeoTransform[1]);
    extent.push_back(adfGeoTransform[3] +
                     (nYOff) * adfGeoTransform[5]);
    return extent;
}

/************************************************************************/
/*                        GetCoverageRequest()                          */
/*                                                                      */
/************************************************************************/

CPLString WCSDataset100::GetCoverageRequest( CPL_UNUSED bool scaled,
                                             int nBufXSize, int nBufYSize,
                                             std::vector<double> extent,
                                             CPLString osBandList )
{

/* -------------------------------------------------------------------- */
/*      URL encode strings that could have questionable characters.     */
/* -------------------------------------------------------------------- */
    CPLString osCoverage = CPLGetXMLValue( psService, "CoverageName", "" );

    char *pszEncoded = CPLEscapeString( osCoverage, -1, CPLES_URL );
    osCoverage = pszEncoded;
    CPLFree( pszEncoded );

    CPLString osFormat = CPLGetXMLValue( psService, "PreferredFormat", "" );

    pszEncoded = CPLEscapeString( osFormat, -1, CPLES_URL );
    osFormat = pszEncoded;
    CPLFree( pszEncoded );

/* -------------------------------------------------------------------- */
/*      Do we have a time we want to use?                               */
/* -------------------------------------------------------------------- */
    CPLString osTime;

    osTime = CSLFetchNameValueDef( papszSDSModifiers, "time", osDefaultTime );

/* -------------------------------------------------------------------- */
/*      Construct a "simple" GetCoverage request (WCS 1.0).             */
/* -------------------------------------------------------------------- */
    CPLString request = CPLGetXMLValue( psService, "ServiceURL", "" );
    request = CPLURLAddKVP(request, "SERVICE", "WCS");
    request = CPLURLAddKVP(request, "REQUEST", "GetCoverage");
    request = CPLURLAddKVP(request, "VERSION", CPLGetXMLValue( psService, "Version", "1.0.0" ));
    request = CPLURLAddKVP(request, "COVERAGE", osCoverage.c_str());
    request = CPLURLAddKVP(request, "FORMAT", osFormat.c_str());
    request += CPLString().Printf("&BBOX=%.15g,%.15g,%.15g,%.15g&WIDTH=%d&HEIGHT=%d&CRS=%s%s",
                                  extent[0], extent[1], extent[2], extent[3],
                                  nBufXSize, nBufYSize,
                                  osCRS.c_str(),
                                  CPLGetXMLValue( psService, "GetCoverageExtra", "" ) );

    if( CPLGetXMLValue( psService, "Resample", NULL ) )
    {
        request += "&INTERPOLATION=";
        request += CPLGetXMLValue( psService, "Resample", "" );
    }
    
    if( osTime != "" )
    {
        request += "&time=";
        request += osTime;
    }
    
    if( osBandList != "" )
    {
        request += CPLString().Printf( "&%s=%s",
                                       osBandIdentifier.c_str(),
                                       osBandList.c_str() );
    }
    return request;
}

/************************************************************************/
/*                      DescribeCoverageRequest()                       */
/*                                                                      */
/************************************************************************/

CPLString WCSDataset100::DescribeCoverageRequest()
{
    CPLString request = CPLGetXMLValue( psService, "ServiceURL", "" );
    request = CPLURLAddKVP(request, "SERVICE", "WCS");
    request = CPLURLAddKVP(request, "REQUEST", "DescribeCoverage");
    request = CPLURLAddKVP(request, "VERSION", CPLGetXMLValue( psService, "Version", "1.0.0" ));
    request = CPLURLAddKVP(request, "COVERAGE", CPLGetXMLValue( psService, "CoverageName", "" ));
    CPLString extra = CPLGetXMLValue(psService, "DescribeCoverageExtra", "");
    if (extra != "") {
        std::vector<CPLString> pairs = Split(extra, "&");
        for (unsigned int i = 0; i < pairs.size(); ++i) {
            std::vector<CPLString> pair = Split(pairs[i], "=");
            request = CPLURLAddKVP(request, pair[0], pair[1]);
        }
    }
    return request;
}

/************************************************************************/
/*                         CoverageOffering()                           */
/*                                                                      */
/************************************************************************/

CPLXMLNode *WCSDataset100::CoverageOffering(CPLXMLNode *psDC)
{
    return CPLGetXMLNode( psDC, "=CoverageDescription.CoverageOffering" );
}

/************************************************************************/
/*                         ExtractGridInfo()                            */
/*                                                                      */
/*      Collect info about grid from describe coverage for WCS 1.0.0    */
/*      and above.                                                      */
/************************************************************************/

bool WCSDataset100::ExtractGridInfo()

{
    CPLXMLNode * psCO = CPLGetXMLNode( psService, "CoverageOffering" );

    if( psCO == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      We need to strip off name spaces so it is easier to             */
/*      searchfor plain gml names.                                      */
/* -------------------------------------------------------------------- */
    CPLStripXMLNamespace( psCO, NULL, TRUE );

/* -------------------------------------------------------------------- */
/*      Verify we have a Rectified Grid.                                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRG =
        CPLGetXMLNode( psCO, "domainSet.spatialDomain.RectifiedGrid" );

    if( psRG == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find RectifiedGrid in CoverageOffering,\n"
                  "unable to process WCS Coverage." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract size, geotransform and coordinate system.               */
/*      Projection is, if it is, from Point.srsName                     */
/* -------------------------------------------------------------------- */
    if( WCSParseGMLCoverage( psRG, &nRasterXSize, &nRasterYSize,
                             adfGeoTransform, &pszProjection ) != CE_None )
        return FALSE;

    // Some MapServers have origin at pixel boundary
    if (CPLGetXMLBoolean(psService, "OriginNotCenter100")) {
        adfGeoTransform[0] += adfGeoTransform[1]*0.5;
        adfGeoTransform[0] += adfGeoTransform[2]*0.5;
        adfGeoTransform[3] += adfGeoTransform[4]*0.5;
        adfGeoTransform[3] += adfGeoTransform[5]*0.5;
    }

/* -------------------------------------------------------------------- */
/*      Fallback to nativeCRSs declaration.                             */
/* -------------------------------------------------------------------- */
    const char *pszNativeCRSs =
        CPLGetXMLValue( psCO, "supportedCRSs.nativeCRSs", NULL );

    if( pszNativeCRSs == NULL )
        pszNativeCRSs =
            CPLGetXMLValue( psCO, "supportedCRSs.requestResponseCRSs", NULL );

    if( pszNativeCRSs == NULL )
        pszNativeCRSs =
            CPLGetXMLValue( psCO, "supportedCRSs.requestCRSs", NULL );

    if( pszNativeCRSs == NULL )
        pszNativeCRSs =
            CPLGetXMLValue( psCO, "supportedCRSs.responseCRSs", NULL );

    if( pszNativeCRSs != NULL
        && (pszProjection == NULL || strlen(pszProjection) == 0) )
    {
        OGRSpatialReference oSRS;

        if( oSRS.SetFromUserInput( pszNativeCRSs ) == OGRERR_NONE )
        {
            CPLFree( pszProjection );
            oSRS.exportToWkt( &pszProjection );
        }
        else
            CPLDebug( "WCS",
                      "<nativeCRSs> element contents not parsable:\n%s",
                      pszNativeCRSs );
    }

    // We should try to use the services name for the CRS if possible.
    if( pszNativeCRSs != NULL
        && ( STARTS_WITH_CI(pszNativeCRSs, "EPSG:")
             || STARTS_WITH_CI(pszNativeCRSs, "AUTO:")
             || STARTS_WITH_CI(pszNativeCRSs, "Image ")
             || STARTS_WITH_CI(pszNativeCRSs, "Engineering ")
             || STARTS_WITH_CI(pszNativeCRSs, "OGC:") ) )
    {
        osCRS = pszNativeCRSs;

        size_t nDivider = osCRS.find( " " );

        if( nDivider != std::string::npos )
            osCRS.resize( nDivider-1 );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a coordinate system override?                        */
/* -------------------------------------------------------------------- */
    const char *pszProjOverride = CPLGetXMLValue( psService, "SRS", NULL );

    if( pszProjOverride )
    {
        OGRSpatialReference oSRS;

        if( oSRS.SetFromUserInput( pszProjOverride ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "<SRS> element contents not parsable:\n%s",
                      pszProjOverride );
            return FALSE;
        }

        CPLFree( pszProjection );
        oSRS.exportToWkt( &pszProjection );

        if( STARTS_WITH_CI(pszProjOverride, "EPSG:")
            || STARTS_WITH_CI(pszProjOverride, "AUTO:")
            || STARTS_WITH_CI(pszProjOverride, "OGC:")
            || STARTS_WITH_CI(pszProjOverride, "Image ")
            || STARTS_WITH_CI(pszProjOverride, "Engineering ") )
            osCRS = pszProjOverride;
    }

/* -------------------------------------------------------------------- */
/*      Build CRS name to use.                                          */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    const char *pszAuth;

    if( pszProjection && strlen(pszProjection) > 0 && osCRS == "" )
    {
        oSRS.SetFromUserInput( pszProjection );
        pszAuth = oSRS.GetAuthorityName(NULL);

        if( pszAuth != NULL && EQUAL(pszAuth,"EPSG") )
        {
            pszAuth = oSRS.GetAuthorityCode(NULL);
            if( pszAuth )
            {
                osCRS = "EPSG:";
                osCRS += pszAuth;
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to define CRS to use." );
                return FALSE;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Pick a format type if we don't already have one selected.       */
/*                                                                      */
/*      We will prefer anything that sounds like TIFF, otherwise        */
/*      falling back to the first supported format.  Should we          */
/*      consider preferring the nativeFormat if available?              */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "PreferredFormat", NULL ) == NULL )
    {
        CPLXMLNode *psSF = CPLGetXMLNode( psCO, "supportedFormats" );
        CPLXMLNode *psNode;
        char **papszFormatList = NULL;
        CPLString osPreferredFormat;
        int iFormat;

        if( psSF == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "No <PreferredFormat> tag in service definition file, and no\n"
                      "<supportedFormats> in coverageOffering." );
            return FALSE;
        }

        for( psNode = psSF->psChild; psNode != NULL; psNode = psNode->psNext )
        {
            if( psNode->eType == CXT_Element
                && EQUAL(psNode->pszValue,"formats")
                && psNode->psChild != NULL
                && psNode->psChild->eType == CXT_Text )
            {
                // This check is looking for deprecated WCS 1.0 capabilities
                // with multiple formats space delimited in a single <formats>
                // element per GDAL ticket 1748 (done by MapServer 4.10 and
                // earlier for instance).
                if( papszFormatList == NULL
                    && psNode->psNext == NULL
                    && strstr(psNode->psChild->pszValue," ") != NULL
                    && strstr(psNode->psChild->pszValue,";") == NULL )
                {
                    char **papszSubList =
                        CSLTokenizeString( psNode->psChild->pszValue );
                    papszFormatList = CSLInsertStrings( papszFormatList,
                                                        -1, papszSubList );
                    CSLDestroy( papszSubList );
                }
                else
                {
                    papszFormatList = CSLAddString( papszFormatList,
                                                    psNode->psChild->pszValue);
                }
            }
        }

        for( iFormat = 0;
             papszFormatList != NULL && papszFormatList[iFormat] != NULL;
             iFormat++ )
        {
            if( osPreferredFormat.empty() )
                osPreferredFormat = papszFormatList[iFormat];

            if( strstr(papszFormatList[iFormat],"tiff") != NULL
                    || strstr(papszFormatList[iFormat],"TIFF") != NULL
                    || strstr(papszFormatList[iFormat],"Tiff") != NULL )
            {
                osPreferredFormat = papszFormatList[iFormat];
                break;
            }
        }

        CSLDestroy( papszFormatList );

        if( !osPreferredFormat.empty() )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "PreferredFormat",
                                         osPreferredFormat );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to identify a nodata value.  For now we only support the    */
/*      singleValue mechanism.                                          */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "NoDataValue", NULL ) == NULL )
    {
        const char *pszSV = CPLGetXMLValue( psCO, "rangeSet.RangeSet.nullValues.singleValue", NULL );

        if( pszSV != NULL && (CPLAtof(pszSV) != 0.0 || *pszSV == DIGIT_ZERO) )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "NoDataValue",
                                         pszSV );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a Band range type.  For now we look for a fairly     */
/*      specific configuration.  The rangeset my have one axis named    */
/*      "Band", with a set of ascending numerical values.               */
/* -------------------------------------------------------------------- */
    osBandIdentifier = CPLGetXMLValue( psService, "BandIdentifier", "" );
    CPLXMLNode * psAD = CPLGetXMLNode( psService,
      "CoverageOffering.rangeSet.RangeSet.axisDescription.AxisDescription" );
    CPLXMLNode *psValues;

    if( osBandIdentifier.empty()
        && psAD != NULL
        && (EQUAL(CPLGetXMLValue(psAD,"name",""),"Band")
            || EQUAL(CPLGetXMLValue(psAD,"name",""),"Bands"))
        && ( (psValues = CPLGetXMLNode( psAD, "values" )) != NULL ) )
    {
        CPLXMLNode *psSV;
        int iBand;

        osBandIdentifier = CPLGetXMLValue(psAD,"name","");

        for( psSV = psValues->psChild, iBand = 1;
             psSV != NULL;
             psSV = psSV->psNext, iBand++ )
        {
            if( psSV->eType != CXT_Element
                || !EQUAL(psSV->pszValue,"singleValue")
                || psSV->psChild == NULL
                || psSV->psChild->eType != CXT_Text
                || atoi(psSV->psChild->pszValue) != iBand )
            {
                osBandIdentifier = "";
                break;
            }
        }

        if( !osBandIdentifier.empty() )
        {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "BandIdentifier",
                                         osBandIdentifier );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a temporal domain?  If so, try to identify a         */
/*      default time value.                                             */
/* -------------------------------------------------------------------- */
    osDefaultTime = CPLGetXMLValue( psService, "DefaultTime", "" );
    CPLXMLNode * psTD =
        CPLGetXMLNode( psService, "CoverageOffering.domainSet.temporalDomain" );
    CPLString osServiceURL = CPLGetXMLValue( psService, "ServiceURL", "" );
    CPLString osCoverageExtra = CPLGetXMLValue( psService, "GetCoverageExtra", "" );

    if( psTD != NULL )
    {
        CPLXMLNode *psTime;

        // collect all the allowed time positions.

        for( psTime = psTD->psChild; psTime != NULL; psTime = psTime->psNext )
        {
            if( psTime->eType == CXT_Element
                && EQUAL(psTime->pszValue,"timePosition")
                && psTime->psChild != NULL
                && psTime->psChild->eType == CXT_Text )
                aosTimePositions.push_back( psTime->psChild->pszValue );
        }

        // we will default to the last - likely the most recent - entry.

        if( !aosTimePositions.empty()
            && osDefaultTime.empty()
            && osServiceURL.ifind("time=") == std::string::npos
            && osCoverageExtra.ifind("time=") == std::string::npos )
        {
            osDefaultTime = aosTimePositions.back();
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "DefaultTime",
                                         osDefaultTime );
        }
    }

    return true;
}

/************************************************************************/
/*                      ParseCapabilities()                             */
/************************************************************************/

CPLErr WCSDataset100::ParseCapabilities( CPLXMLNode * Capabilities, CPL_UNUSED CPLString url )
{

    CPLStripXMLNamespace(Capabilities, NULL, TRUE);

    if (strcmp(Capabilities->pszValue, "WCS_Capabilities") != 0) {
        return CE_Failure;
    }

    char **metadata = NULL;
    CPLString path = "WCS_GLOBAL#";

    CPLString key = path + "version";
    metadata = CSLSetNameValue(metadata, key, Version());

    for( CPLXMLNode *node = Capabilities->psChild; node != NULL; node = node->psNext)
    {
        const char *attr = node->pszValue;
        if( node->eType == CXT_Attribute && EQUAL(attr, "updateSequence") )
        {
            key = path + "updateSequence";
            CPLString value = CPLGetXMLValue(node, NULL, "");
            metadata = CSLSetNameValue(metadata, key, value);
        }
    }

    // identification metadata
    CPLString path2 = path;
    std::vector<CPLString> keys2 = {
        "description",
        "name",
        "label",
        "fees",
        "accessConstraints"
    };
    CPLXMLNode *service = AddSimpleMetaData(&metadata, Capabilities, path2, "Service", keys2);
    if (service) {
        CPLString path3 = path2;
        std::vector<CPLString> keys3 = {
            "individualName",
            "organisationName",
            "positionName"
        };
        CPLString kw = GetKeywords(service, "keywords", "keyword");
        if (kw != "") {
            CPLString name = path + "keywords";
            metadata = CSLSetNameValue(metadata, name, kw);
        }
        CPLXMLNode *party = AddSimpleMetaData(&metadata, service, path3, "resposibleParty", keys3);
        CPLXMLNode *info;
        if (party && (info = CPLGetXMLNode(party, "contactInfo"))) {
            CPLString path4 = path3 + "contactInfo.";
            std::vector<CPLString> keys4 = {
                "deliveryPoint",
                "city",
                "administrativeArea",
                "postalCode",
                "country",
                "electronicMailAddress"
            };
            CPLString path5 = path4;
            std::vector<CPLString> keys5 = {
                "voice",
                "facsimile"
            };
            AddSimpleMetaData(&metadata, info, path4, "address", keys4);
            AddSimpleMetaData(&metadata, info, path5, "phone", keys5);
        }
    }

    // provider metadata
    // operations metadata
    CPLString DescribeCoverageURL = "";
    DescribeCoverageURL = CPLGetXMLValue(
        CPLGetXMLNode(
            CPLGetXMLNode(
                CPLSearchXMLNode(
                    CPLSearchXMLNode(Capabilities, "DescribeCoverage"),
                    "Get"),
                "OnlineResource"),
            "href"), NULL, "");
    // todo: if DescribeCoverageURL looks wrong (i.e. has localhost) should we change it?

    this->SetMetadata( metadata, "" );
    CSLDestroy( metadata );
    metadata = NULL;

    if( CPLXMLNode *contents = CPLGetXMLNode(Capabilities, "ContentMetadata") )
    {
        int index = 1;
        for( CPLXMLNode *summary = contents->psChild; summary != NULL; summary = summary->psNext)
        {
            if( summary->eType != CXT_Element
                || !EQUAL(summary->pszValue, "CoverageOfferingBrief") )
            {
                continue;
            }
            CPLString path3;
            path3.Printf( "SUBDATASET_%d_", index);

            CPLXMLNode *node;

            if ((node = CPLGetXMLNode(summary, "name"))) {
                CPLString name = path3 + "NAME";
                CPLString value = DescribeCoverageURL;
                value = CPLURLAddKVP(value, "SERVICE", "WCS");
                value = CPLURLAddKVP(value, "VERSION", this->Version());
                value = CPLURLAddKVP(value, "COVERAGE", CPLGetXMLValue(node, NULL, ""));
                // GDAL Data Model:
                // The value of the _NAME is a string that can be passed to GDALOpen() to access the file.
                metadata = CSLSetNameValue(metadata, name, value);
            }

            if ((node = CPLGetXMLNode(summary, "label"))) {
                CPLString name = path3 + "LABEL";
                metadata = CSLSetNameValue(metadata, name, CPLGetXMLValue(node, NULL, ""));
            }

            if ((node = CPLGetXMLNode(summary, "lonlatEnvelope"))) {
                CPLString name = path3 + "lonlatEnvelope";
                CPLString CRS = ParseCRS(node);
                std::vector<CPLString> bbox = ParseBoundingBox(node);
                if (bbox.size() >= 2) {
                    // lonlat => no need for axis order swap
                    std::vector<double> low = Flist(Split(bbox[0], " "), 0, 2);
                    std::vector<double> high = Flist(Split(bbox[1], " "), 0, 2);
                    CPLString str;
                    str.Printf("%.15g,%.15g,%.15g,%.15g", low[0], low[1], high[0], high[1]);
                    metadata = CSLSetNameValue(metadata, name, str);
                }
            }

            CPLString kw = GetKeywords(summary, "keywords", "keyword");
            if (kw != "") {
                CPLString name = path3 + "keywords";
                metadata = CSLSetNameValue(metadata, name, kw);
            }

            if ((node = CPLGetXMLNode(summary, "description"))) {
                CPLString name = path3 + "description";
                metadata = CSLSetNameValue(metadata, name, CPLGetXMLValue(node, NULL, ""));
            }

            index++;
        }
    }
    this->SetMetadata( metadata, "SUBDATASETS" );
    CSLDestroy( metadata );
    return CE_None;
}

/************************************************************************/
/*                          ExceptionNodeName()                         */
/*                                                                      */
/************************************************************************/

const char *WCSDataset100::ExceptionNodeName()
{
    return "=ServiceExceptionReport.ServiceException";
}
