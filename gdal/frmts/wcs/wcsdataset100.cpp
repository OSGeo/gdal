/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset class for WCS 1.0.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "wcsdataset.h"
#include "wcsutils.h"

using namespace WCSUtils;

/************************************************************************/
/*                         GetExtent()                                  */
/*                                                                      */
/************************************************************************/

std::vector<double> WCSDataset100::GetExtent(int nXOff, int nYOff,
                                             int nXSize, int nYSize,
                                             CPL_UNUSED int, CPL_UNUSED int)
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
                                             const std::vector<double> &extent,
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
    request += CPLString().Printf("&BBOX=%.15g,%.15g,%.15g,%.15g&WIDTH=%d&HEIGHT=%d&CRS=%s",
                                  extent[0], extent[1], extent[2], extent[3],
                                  nBufXSize, nBufYSize,
                                  osCRS.c_str() );
    CPLString extra = CPLGetXMLValue(psService, "Parameters", "");
    if (extra != "") {
        std::vector<CPLString> pairs = Split(extra, "&");
        for (unsigned int i = 0; i < pairs.size(); ++i) {
            std::vector<CPLString> pair = Split(pairs[i], "=");
            request = CPLURLAddKVP(request, pair[0], pair[1]);
        }
    }
    extra = CPLGetXMLValue(psService, "GetCoverageExtra", "");
    if (extra != "") {
        std::vector<CPLString> pairs = Split(extra, "&");
        for (unsigned int i = 0; i < pairs.size(); ++i) {
            std::vector<CPLString> pair = Split(pairs[i], "=");
            request = CPLURLAddKVP(request, pair[0], pair[1]);
        }
    }

    CPLString interpolation = CPLGetXMLValue( psService, "Interpolation", "" );
    if (interpolation == "") {
        // old undocumented key for interpolation in service
        interpolation = CPLGetXMLValue( psService, "Resample", "" );
    }
    if (interpolation != "") {
        request += "&INTERPOLATION=" + interpolation;
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
    CPLString extra = CPLGetXMLValue(psService, "Parameters", "");
    if (extra != "") {
        std::vector<CPLString> pairs = Split(extra, "&");
        for (unsigned int i = 0; i < pairs.size(); ++i) {
            std::vector<CPLString> pair = Split(pairs[i], "=");
            request = CPLURLAddKVP(request, pair[0], pair[1]);
        }
    }
    extra = CPLGetXMLValue(psService, "DescribeCoverageExtra", "");
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

    if( psCO == nullptr )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      We need to strip off name spaces so it is easier to             */
/*      searchfor plain gml names.                                      */
/* -------------------------------------------------------------------- */
    CPLStripXMLNamespace( psCO, nullptr, TRUE );

/* -------------------------------------------------------------------- */
/*      Verify we have a Rectified Grid.                                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRG =
        CPLGetXMLNode( psCO, "domainSet.spatialDomain.RectifiedGrid" );

    if( psRG == nullptr )
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

    // MapServer have origin at pixel boundary
    if (CPLGetXMLBoolean(psService, "OriginAtBoundary")) {
        adfGeoTransform[0] += adfGeoTransform[1]*0.5;
        adfGeoTransform[0] += adfGeoTransform[2]*0.5;
        adfGeoTransform[3] += adfGeoTransform[4]*0.5;
        adfGeoTransform[3] += adfGeoTransform[5]*0.5;
    }

/* -------------------------------------------------------------------- */
/*      Fallback to nativeCRSs declaration.                             */
/* -------------------------------------------------------------------- */
    const char *pszNativeCRSs =
        CPLGetXMLValue( psCO, "supportedCRSs.nativeCRSs", nullptr );

    if( pszNativeCRSs == nullptr )
        pszNativeCRSs =
            CPLGetXMLValue( psCO, "supportedCRSs.requestResponseCRSs", nullptr );

    if( pszNativeCRSs == nullptr )
        pszNativeCRSs =
            CPLGetXMLValue( psCO, "supportedCRSs.requestCRSs", nullptr );

    if( pszNativeCRSs == nullptr )
        pszNativeCRSs =
            CPLGetXMLValue( psCO, "supportedCRSs.responseCRSs", nullptr );

    if( pszNativeCRSs != nullptr
        && (pszProjection == nullptr || strlen(pszProjection) == 0) )
    {
        OGRSpatialReference oSRS;

        if( oSRS.SetFromUserInput( pszNativeCRSs, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) == OGRERR_NONE )
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
    if( pszNativeCRSs != nullptr
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
    const char *pszProjOverride = CPLGetXMLValue( psService, "SRS", nullptr );

    if( pszProjOverride )
    {
        OGRSpatialReference oSRS;

        if( oSRS.SetFromUserInput( pszProjOverride, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) != OGRERR_NONE )
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

    if( pszProjection && strlen(pszProjection) > 0 && osCRS == "" )
    {
        oSRS.SetFromUserInput( pszProjection, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get());
        const char *pszAuth = oSRS.GetAuthorityName(nullptr);

        if( pszAuth != nullptr && EQUAL(pszAuth,"EPSG") )
        {
            pszAuth = oSRS.GetAuthorityCode(nullptr);
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
    if( CPLGetXMLValue( psService, "PreferredFormat", nullptr ) == nullptr )
    {
        CPLXMLNode *psSF = CPLGetXMLNode( psCO, "supportedFormats" );
        CPLXMLNode *psNode;
        char **papszFormatList = nullptr;
        CPLString osPreferredFormat;
        int iFormat;

        if( psSF == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "No <PreferredFormat> tag in service definition file, and no\n"
                      "<supportedFormats> in coverageOffering." );
            return FALSE;
        }

        for( psNode = psSF->psChild; psNode != nullptr; psNode = psNode->psNext )
        {
            if( psNode->eType == CXT_Element
                && EQUAL(psNode->pszValue,"formats")
                && psNode->psChild != nullptr
                && psNode->psChild->eType == CXT_Text )
            {
                // This check is looking for deprecated WCS 1.0 capabilities
                // with multiple formats space delimited in a single <formats>
                // element per GDAL ticket 1748 (done by MapServer 4.10 and
                // earlier for instance).
                if( papszFormatList == nullptr
                    && psNode->psNext == nullptr
                    && strstr(psNode->psChild->pszValue," ") != nullptr
                    && strstr(psNode->psChild->pszValue,";") == nullptr )
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
             papszFormatList != nullptr && papszFormatList[iFormat] != nullptr;
             iFormat++ )
        {
            if( osPreferredFormat.empty() )
                osPreferredFormat = papszFormatList[iFormat];

            if( strstr(papszFormatList[iFormat],"tiff") != nullptr
                    || strstr(papszFormatList[iFormat],"TIFF") != nullptr
                    || strstr(papszFormatList[iFormat],"Tiff") != nullptr )
            {
                osPreferredFormat = papszFormatList[iFormat];
                break;
            }
        }

        CSLDestroy( papszFormatList );

        if( !osPreferredFormat.empty() )
        {
            bServiceDirty = true;
            CPLCreateXMLElementAndValue( psService, "PreferredFormat",
                                         osPreferredFormat );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to identify a nodata value.  For now we only support the    */
/*      singleValue mechanism.                                          */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "NoDataValue", nullptr ) == nullptr )
    {
        const char *pszSV = CPLGetXMLValue( psCO, "rangeSet.RangeSet.nullValues.singleValue", nullptr );

        if( pszSV != nullptr && (CPLAtof(pszSV) != 0.0 || *pszSV == DIGIT_ZERO) )
        {
            bServiceDirty = true;
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
        && psAD != nullptr
        && (EQUAL(CPLGetXMLValue(psAD,"name",""),"Band")
            || EQUAL(CPLGetXMLValue(psAD,"name",""),"Bands"))
        && ( (psValues = CPLGetXMLNode( psAD, "values" )) != nullptr ) )
    {
        CPLXMLNode *psSV;
        int iBand;

        osBandIdentifier = CPLGetXMLValue(psAD,"name","");

        for( psSV = psValues->psChild, iBand = 1;
             psSV != nullptr;
             psSV = psSV->psNext, iBand++ )
        {
            if( psSV->eType != CXT_Element
                || !EQUAL(psSV->pszValue,"singleValue")
                || psSV->psChild == nullptr
                || psSV->psChild->eType != CXT_Text
                || atoi(psSV->psChild->pszValue) != iBand )
            {
                osBandIdentifier = "";
                break;
            }
        }

        if( !osBandIdentifier.empty() )
        {
            bServiceDirty = true;
            CPLSetXMLValue( psService, "BandIdentifier",
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

    if( psTD != nullptr )
    {
        CPLXMLNode *psTime;

        // collect all the allowed time positions.

        for( psTime = psTD->psChild; psTime != nullptr; psTime = psTime->psNext )
        {
            if( psTime->eType == CXT_Element
                && EQUAL(psTime->pszValue,"timePosition")
                && psTime->psChild != nullptr
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
            bServiceDirty = true;
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

    CPLStripXMLNamespace(Capabilities, nullptr, TRUE);

    if (strcmp(Capabilities->pszValue, "WCS_Capabilities") != 0) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error in capabilities document.\n" );
        return CE_Failure;
    }

    char **metadata = nullptr;
    CPLString path = "WCS_GLOBAL#";

    CPLString key = path + "version";
    metadata = CSLSetNameValue(metadata, key, Version());

    for( CPLXMLNode *node = Capabilities->psChild; node != nullptr; node = node->psNext)
    {
        const char *attr = node->pszValue;
        if( node->eType == CXT_Attribute && EQUAL(attr, "updateSequence") )
        {
            key = path + "updateSequence";
            CPLString value = CPLGetXMLValue(node, nullptr, "");
            metadata = CSLSetNameValue(metadata, key, value);
        }
    }

    // identification metadata
    CPLString path2 = path;
    std::vector<CPLString> keys2;
    keys2.push_back("description");
    keys2.push_back("name");
    keys2.push_back("label");
    keys2.push_back("fees");
    keys2.push_back("accessConstraints");
    CPLXMLNode *service = AddSimpleMetaData(&metadata, Capabilities, path2, "Service", keys2);
    if (service) {
        CPLString path3 = path2;
        std::vector<CPLString> keys3;
        keys3.push_back("individualName");
        keys3.push_back("organisationName");
        keys3.push_back("positionName");
        CPLString kw = GetKeywords(service, "keywords", "keyword");
        if (kw != "") {
            CPLString name = path + "keywords";
            metadata = CSLSetNameValue(metadata, name, kw);
        }
        CPLXMLNode *party = AddSimpleMetaData(&metadata, service, path3, "responsibleParty", keys3);
        CPLXMLNode *info = CPLGetXMLNode(party, "contactInfo");
        if (party && info) {
            CPLString path4 = path3 + "contactInfo.";
            std::vector<CPLString> keys4;
            keys4.push_back("deliveryPoint");
            keys4.push_back("city");
            keys4.push_back("administrativeArea");
            keys4.push_back("postalCode");
            keys4.push_back("country");
            keys4.push_back("electronicMailAddress");
            CPLString path5 = path4;
            std::vector<CPLString> keys5;
            keys5.push_back("voice");
            keys5.push_back("facsimile");
            AddSimpleMetaData(&metadata, info, path4, "address", keys4);
            AddSimpleMetaData(&metadata, info, path5, "phone", keys5);
        }
    }

    // provider metadata
    // operations metadata
    CPLString DescribeCoverageURL;
    DescribeCoverageURL = CPLGetXMLValue(
        CPLGetXMLNode(
            CPLGetXMLNode(
                CPLSearchXMLNode(
                    CPLSearchXMLNode(Capabilities, "DescribeCoverage"),
                    "Get"),
                "OnlineResource"),
            "href"), nullptr, "");
    // if DescribeCoverageURL looks wrong (i.e. has localhost) should we change it?

    this->SetMetadata( metadata, "" );
    CSLDestroy( metadata );
    metadata = nullptr;

    if( CPLXMLNode *contents = CPLGetXMLNode(Capabilities, "ContentMetadata") )
    {
        int index = 1;
        for( CPLXMLNode *summary = contents->psChild; summary != nullptr; summary = summary->psNext)
        {
            if( summary->eType != CXT_Element
                || !EQUAL(summary->pszValue, "CoverageOfferingBrief") )
            {
                continue;
            }
            CPLString path3;
            path3.Printf( "SUBDATASET_%d_", index);
            index += 1;

            // the name and description of the subdataset:
            // GDAL Data Model:
            // The value of the _NAME is a string that can be passed to GDALOpen() to access the file.

            CPLXMLNode *node = CPLGetXMLNode(summary, "name");
            if (node) {
                CPLString key2 = path3 + "NAME";
                CPLString name = CPLGetXMLValue(node, nullptr, "");
                CPLString value = DescribeCoverageURL;
                value = CPLURLAddKVP(value, "VERSION", this->Version());
                value = CPLURLAddKVP(value, "COVERAGE", name);
                metadata = CSLSetNameValue(metadata, key2, value);
            } else {
                CSLDestroy( metadata );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Error in capabilities document.\n" );
                return CE_Failure;
            }

            node = CPLGetXMLNode(summary, "label");
            if (node) {
                CPLString key2 = path3 + "DESC";
                metadata = CSLSetNameValue(metadata, key2, CPLGetXMLValue(node, nullptr, ""));
            } else {
                CSLDestroy( metadata );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Error in capabilities document.\n" );
                return CE_Failure;
            }

            // todo: compose global bounding box from lonLatEnvelope

            // further subdataset (coverage) parameters are parsed in ParseCoverageCapabilities

        }
    }
    this->SetMetadata( metadata, "SUBDATASETS" );
    CSLDestroy( metadata );
    return CE_None;
}

void WCSDataset100::ParseCoverageCapabilities(CPLXMLNode *capabilities, const CPLString &coverage, CPLXMLNode *metadata)
{
    CPLStripXMLNamespace(capabilities, nullptr, TRUE);
    if( CPLXMLNode *contents = CPLGetXMLNode(capabilities, "ContentMetadata") )
    {
        for( CPLXMLNode *summary = contents->psChild; summary != nullptr; summary = summary->psNext)
        {
            if( summary->eType != CXT_Element
                || !EQUAL(summary->pszValue, "CoverageOfferingBrief") )
            {
                continue;
            }

            CPLXMLNode *node = CPLGetXMLNode(summary, "name");
            if (node) {
                CPLString name = CPLGetXMLValue(node, nullptr, "");
                if (name != coverage) {
                    continue;
                }
            }

            XMLCopyMetadata(summary, metadata, "label");
            XMLCopyMetadata(summary, metadata, "description");

            CPLString kw = GetKeywords(summary, "keywords", "keyword");
            CPLAddXMLAttributeAndValue(
                CPLCreateXMLElementAndValue(metadata, "MDI", kw), "key", "keywords");

            // skip metadataLink

        }
    }
}
