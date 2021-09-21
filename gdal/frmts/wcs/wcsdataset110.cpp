/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset class for WCS 1.1.
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

std::vector<double> WCSDataset110::GetExtent(int nXOff, int nYOff,
                                             int nXSize, int nYSize,
                                             CPL_UNUSED int nBufXSize, CPL_UNUSED int nBufYSize)
{
    std::vector<double> extent;

    // outer edges of outer pixels.
    extent.push_back(adfGeoTransform[0] +
                     (nXOff) * adfGeoTransform[1]);
    extent.push_back(adfGeoTransform[3] +
                     (nYOff + nYSize) * adfGeoTransform[5]);
    extent.push_back(adfGeoTransform[0] +
                     (nXOff + nXSize) * adfGeoTransform[1]);
    extent.push_back(adfGeoTransform[3] +
                     (nYOff) * adfGeoTransform[5]);

    bool no_shrink = CPLGetXMLBoolean(psService, "OuterExtents");

    // WCS 1.1 extents are centers of outer pixels.
    if (!no_shrink) {
        extent[2] -= adfGeoTransform[1] * 0.5;
        extent[0] += adfGeoTransform[1] * 0.5;
        extent[1] -= adfGeoTransform[5] * 0.5;
        extent[3] += adfGeoTransform[5] * 0.5;
    }

    double dfXStep, dfYStep;

    if (!no_shrink) {
        dfXStep = (nXSize/(double)nBufXSize) * adfGeoTransform[1];
        dfYStep = (nYSize/(double)nBufYSize) * adfGeoTransform[5];
        // Carefully adjust bounds for pixel centered values at new
        // sampling density.
        if( nBufXSize != nXSize || nBufYSize != nYSize )
        {
            dfXStep = (nXSize/(double)nBufXSize) * adfGeoTransform[1];
            dfYStep = (nYSize/(double)nBufYSize) * adfGeoTransform[5];

            extent[0]  = nXOff * adfGeoTransform[1] + adfGeoTransform[0]
                + dfXStep * 0.5;
            extent[2]  = extent[0] + (nBufXSize - 1) * dfXStep;

            extent[3]  = nYOff * adfGeoTransform[5] + adfGeoTransform[3]
                + dfYStep * 0.5;
            extent[1]  = extent[3] + (nBufYSize - 1) * dfYStep;
        }
    } else {
        double adjust = CPLAtof(CPLGetXMLValue(psService, "BufSizeAdjust", "0.0"));
        dfXStep = (nXSize/((double)nBufXSize + adjust)) * adfGeoTransform[1];
        dfYStep = (nYSize/((double)nBufYSize + adjust)) * adfGeoTransform[5];
    }

    extent.push_back(dfXStep);
    extent.push_back(dfYStep);

    return extent;
}

/************************************************************************/
/*                        GetCoverageRequest()                          */
/*                                                                      */
/************************************************************************/

CPLString WCSDataset110::GetCoverageRequest(bool scaled,
                                            CPL_UNUSED int nBufXSize, CPL_UNUSED int nBufYSize,
                                            const std::vector<double> &extent,
                                            CPLString osBandList)
{
    CPLString osRequest;

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

    CPLString osRangeSubset = CPLGetXMLValue(psService, "FieldName", "");

    // todo: MapServer seems to require interpolation

    CPLString interpolation = CPLGetXMLValue( psService, "Interpolation", "" );
    if (interpolation == "") {
        // old undocumented key for interpolation in service
        interpolation = CPLGetXMLValue( psService, "Resample", "" );
    }
    if (interpolation != "") {
        osRangeSubset += ":" + interpolation;
    }

    if( osBandList != "" )
    {
        if (osBandIdentifier != "") {
            osRangeSubset +=
                CPLString().Printf( "[%s[%s]]",
                                    osBandIdentifier.c_str(),
                                    osBandList.c_str() );
        }
    }

    osRangeSubset = "&RangeSubset=" + URLEncode(osRangeSubset);

    double
        bbox_0 = extent[0], // min X
        bbox_1 = extent[1], // min Y
        bbox_2 = extent[2], // max X
        bbox_3 = extent[3]; // max Y

    if (axis_order_swap) {
        bbox_0 = extent[1]; // min Y
        bbox_1 = extent[0]; // min X
        bbox_2 = extent[3]; // max Y
        bbox_3 = extent[2]; // max X
    }
    CPLString request = CPLGetXMLValue( psService, "ServiceURL", "" );
    request = CPLURLAddKVP(request, "SERVICE", "WCS");
    request += CPLString().Printf(
        "&VERSION=%s&REQUEST=GetCoverage&IDENTIFIER=%s"
        "&FORMAT=%s&BOUNDINGBOX=%.15g,%.15g,%.15g,%.15g,%s%s",
        CPLGetXMLValue( psService, "Version", "" ),
        osCoverage.c_str(),
        osFormat.c_str(),
        bbox_0, bbox_1, bbox_2, bbox_3,
        osCRS.c_str(),
        osRangeSubset.c_str() );
    double
        origin_1 = extent[0], // min X
        origin_2 = extent[3], // max Y
        offset_1 = extent[4], // dX
        offset_2 = extent[5]; // dY

    if (axis_order_swap) {
        origin_1 = extent[3]; // max Y
        origin_2 = extent[0]; // min X
        offset_1 = extent[5]; // dY
        offset_2 = extent[4]; // dX
    }
    CPLString offsets;
    if (CPLGetXMLBoolean(psService, "OffsetsPositive")) {
        offset_1 = fabs(offset_1);
        offset_2 = fabs(offset_2);
    }
    if (EQUAL(CPLGetXMLValue(psService, "NrOffsets", "4"), "2")) {
        offsets = CPLString().Printf("%.15g,%.15g", offset_1, offset_2);
    } else {
        if (axis_order_swap) {
            // Only tested with GeoServer but this is the correct offset(?)
            offsets = CPLString().Printf("0,%.15g,%.15g,0", offset_2, offset_1);
        } else {
            offsets = CPLString().Printf("%.15g,0,0,%.15g", offset_1, offset_2);
        }
    }
    bool do_not_include = CPLGetXMLBoolean(psService, "GridCRSOptional") && !scaled;
    if (!do_not_include) {
        request += CPLString().Printf(
            "&GridBaseCRS=%s"
            "&GridCS=urn:ogc:def:cs:OGC:0.0:Grid2dSquareCS"
            "&GridType=urn:ogc:def:method:WCS:1.1:2dGridIn2dCrs"
            "&GridOrigin=%.15g,%.15g"
            "&GridOffsets=%s",
            osCRS.c_str(),
            origin_1, origin_2,
            offsets.c_str()
            );
    }
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
    CPLDebug("WCS", "Requesting %s", request.c_str());
    return request;
}

/************************************************************************/
/*                        DescribeCoverageRequest()                     */
/*                                                                      */
/************************************************************************/

CPLString WCSDataset110::DescribeCoverageRequest()
{
    CPLString request = CPLGetXMLValue( psService, "ServiceURL", "" );
    request = CPLURLAddKVP(request, "SERVICE", "WCS");
    request = CPLURLAddKVP(request, "REQUEST", "DescribeCoverage");
    request = CPLURLAddKVP(request, "VERSION", CPLGetXMLValue( psService, "Version", "1.1.0" ));
    request = CPLURLAddKVP(request, "IDENTIFIERS", CPLGetXMLValue( psService, "CoverageName", "" ));
    request = CPLURLAddKVP(request, "FORMAT", "text/xml");
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

CPLXMLNode *WCSDataset110::CoverageOffering(CPLXMLNode *psDC)
{
    return CPLGetXMLNode( psDC,"=CoverageDescriptions.CoverageDescription");
}


/************************************************************************/
/*                          ExtractGridInfo()                           */
/*                                                                      */
/*      Collect info about grid from describe coverage for WCS 1.1.     */
/*                                                                      */
/************************************************************************/

bool WCSDataset110::ExtractGridInfo()

{
    CPLXMLNode * psCO = CPLGetXMLNode( psService, "CoverageDescription" );

    if( psCO == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      We need to strip off name spaces so it is easier to             */
/*      searchfor plain gml names.                                      */
/* -------------------------------------------------------------------- */
    CPLStripXMLNamespace( psCO, nullptr, TRUE );

/* -------------------------------------------------------------------- */
/*      Verify we have a SpatialDomain and GridCRS.                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psSD =
        CPLGetXMLNode( psCO, "Domain.SpatialDomain" );
    CPLXMLNode *psGCRS =
        CPLGetXMLNode( psSD, "GridCRS" );

    if( psSD == nullptr || psGCRS == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find GridCRS in CoverageDescription,\n"
                  "unable to process WCS Coverage." );
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Establish our coordinate system.                                */
/*   This is needed before geometry since we may have axis order swap.  */
/* -------------------------------------------------------------------- */
    CPLString crs = ParseCRS(psGCRS);

    if (crs.empty()) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find GridCRS.GridBaseCRS" );
        return false;
    }

    // SetCRS should fail only if the CRS is really unknown to GDAL
    if (!SetCRS(crs, true)) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to interpret GridBaseCRS '%s'.",
                  crs.c_str() );
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Collect size, origin, and offsets for SetGeometry()             */
/*                                                                      */
/*      Extract Geotransform from GridCRS.                              */
/*                                                                      */
/* -------------------------------------------------------------------- */
    const char *pszGridType = CPLGetXMLValue( psGCRS, "GridType",
                                              "urn:ogc:def:method:WCS::2dSimpleGrid" );
    bool swap = axis_order_swap && !CPLGetXMLBoolean(psService, "NoGridAxisSwap");
    std::vector<double> origin = Flist(Split(CPLGetXMLValue(psGCRS, "GridOrigin", ""), " ", swap));

    std::vector<CPLString> offset_1 = Split(CPLGetXMLValue(psGCRS, "GridOffsets", ""), " ");
    std::vector<CPLString> offset_2;
    size_t n = offset_1.size();
    if (n % 2 != 0) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GridOffsets has incorrect amount of coefficients.\n"
                  "Unable to process WCS coverage.");
        return false;
    }
    for (unsigned int i = 0; i < n / 2; ++i) {
        CPLString s = offset_1.back();
        offset_1.erase(offset_1.end() - 1);
        offset_2.insert(offset_2.begin(), s);
    }
    std::vector<std::vector<double> > offsets;
    if (swap) {
        offsets.push_back(Flist(offset_2));
        offsets.push_back(Flist(offset_1));
    } else {
        offsets.push_back(Flist(offset_1));
        offsets.push_back(Flist(offset_2));
    }

    if( strstr(pszGridType,":2dGridIn2dCrs")
        || strstr(pszGridType,":2dGridin2dCrs") )
    {
        if( !(offset_1.size() == 2
              && origin.size() == 2 ))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "2dGridIn2dCrs does not have expected GridOrigin or\n"
                      "GridOffsets values - unable to process WCS coverage.");
            return false;
        }
    }

    else if( strstr(pszGridType,":2dGridIn3dCrs") )
    {
        if( !(offset_1.size() == 3
              && origin.size() == 3 ))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "2dGridIn3dCrs does not have expected GridOrigin or\n"
                      "GridOffsets values - unable to process WCS coverage.");
            return false;
        }
    }

    else if( strstr(pszGridType,":2dSimpleGrid") )
    {
        if( !(offset_1.size() == 1
              && origin.size() == 2 ))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "2dSimpleGrid does not have expected GridOrigin or\n"
                      "GridOffsets values - unable to process WCS coverage.");
            return false;
        }
    }

    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unrecognized GridCRS.GridType value '%s',\n"
                  "unable to process WCS coverage.",
                  pszGridType );
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Search for an ImageCRS for raster size.                         */
/* -------------------------------------------------------------------- */
    std::vector<int> size;
    CPLXMLNode *psNode;

    for( psNode = psSD->psChild;
         psNode != nullptr && size.size() == 0;
         psNode = psNode->psNext )
    {
        if( psNode->eType != CXT_Element
            || !EQUAL(psNode->pszValue,"BoundingBox") )
            continue;

        CPLString osBBCRS = ParseCRS(psNode);
        if (strstr(osBBCRS,":imageCRS")) {
            std::vector<CPLString> bbox = ParseBoundingBox(psNode);
            if (bbox.size() >= 2) {
                std::vector<int> low = Ilist(Split(bbox[0], " "), 0, 2);
                std::vector<int> high = Ilist(Split(bbox[1], " "), 0, 2);
                if (low[0] == 0 && low[1] == 0) {
                    size.push_back(high[0]);
                    size.push_back(high[1]);
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we search for a bounding box in our coordinate        */
/*      system and derive the size from that.                           */
/* -------------------------------------------------------------------- */
    for( psNode = psSD->psChild;
         psNode != nullptr && size.size() == 0;
         psNode = psNode->psNext )
    {
        if( psNode->eType != CXT_Element
            || !EQUAL(psNode->pszValue,"BoundingBox") )
            continue;

        CPLString osBBCRS = ParseCRS(psNode);
        if (osBBCRS == osCRS) {
            std::vector<CPLString> bbox = ParseBoundingBox(psNode);
            bool not_rot = (offsets[0].size() == 1 && offsets[1].size() == 1)
                || ((swap && offsets[0][0] == 0.0 && offsets[1][1] == 0.0)
                    || (!swap && offsets[0][1] == 0.0 && offsets[1][0] == 0.0));
            if (bbox.size() >= 2 && not_rot)
            {
                std::vector<double> low = Flist(Split(bbox[0], " ", axis_order_swap), 0, 2);
                std::vector<double> high = Flist(Split(bbox[1], " ", axis_order_swap), 0, 2);
                double c1 = offsets[0][0];
                double c2 = offsets[1].size() == 1 ? offsets[1][0] : offsets[1][1];
                size.push_back((int)((high[0] - low[0]) / c1 + 1.01));
                size.push_back((int)((high[1] - low[1]) / fabs(c2) + 1.01));
            }
        }
    }

    if( size.size() < 2 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Could not determine the size of the grid." );
        return false;
    }

    SetGeometry(size, origin, offsets);

/* -------------------------------------------------------------------- */
/*      Do we have a coordinate system override?                        */
/* -------------------------------------------------------------------- */
    const char *pszProjOverride = CPLGetXMLValue( psService, "SRS", nullptr );

    if( pszProjOverride )
    {
        OGRSpatialReference oSRS;

        if( oSRS.SetFromUserInput( pszProjOverride, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "<SRS> element contents not parsable:\n%s",
                      pszProjOverride );
            return false;
        }

        CPLFree( pszProjection );
        oSRS.exportToWkt( &pszProjection );
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
        CPLString osPreferredFormat;

        for( psNode = psCO->psChild; psNode != nullptr; psNode = psNode->psNext )
        {
            if( psNode->eType == CXT_Element
                && EQUAL(psNode->pszValue,"SupportedFormat")
                && psNode->psChild
                && psNode->psChild->eType == CXT_Text )
            {
                if( osPreferredFormat.empty() )
                    osPreferredFormat = psNode->psChild->pszValue;

                if( strstr(psNode->psChild->pszValue,"tiff") != nullptr
                    || strstr(psNode->psChild->pszValue,"TIFF") != nullptr
                    || strstr(psNode->psChild->pszValue,"Tiff") != nullptr )
                {
                    osPreferredFormat = psNode->psChild->pszValue;
                    break;
                }
            }
        }

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
        const char *pszSV =
            CPLGetXMLValue( psCO, "Range.Field.NullValue", nullptr );

        if( pszSV != nullptr && (CPLAtof(pszSV) != 0.0 || *pszSV == DIGIT_ZERO) )
        {
            bServiceDirty = true;
            CPLCreateXMLElementAndValue( psService, "NoDataValue",
                                         pszSV );
        }
    }

/* -------------------------------------------------------------------- */
/*      Grab the field name, if possible.                               */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psService, "FieldName", nullptr ) == nullptr )
    {
        CPLString osFieldName =
            CPLGetXMLValue( psCO, "Range.Field.Identifier", "" );

        if( !osFieldName.empty() )
        {
            bServiceDirty = true;
            CPLCreateXMLElementAndValue( psService, "FieldName",
                                         osFieldName );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to find required Identifier name %s for Range Field.",
                      osCRS.c_str() );
            return false;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a "Band" axis?  If so try to grab the bandcount      */
/*      and data type from it.                                          */
/* -------------------------------------------------------------------- */
    osBandIdentifier = CPLGetXMLValue( psService, "BandIdentifier", "" );
    CPLXMLNode * psAxis = CPLGetXMLNode(
        psService, "CoverageDescription.Range.Field.Axis" );

    if( osBandIdentifier.empty()
        && (EQUAL(CPLGetXMLValue(psAxis,"Identifier",""),"Band")
            || EQUAL(CPLGetXMLValue(psAxis,"Identifier",""),"Bands"))
        && CPLGetXMLNode(psAxis,"AvailableKeys") != nullptr )
    {
        osBandIdentifier = CPLGetXMLValue(psAxis,"Identifier","");

        // verify keys are ascending starting at 1
        CPLXMLNode *psValues = CPLGetXMLNode(psAxis,"AvailableKeys");
        CPLXMLNode *psSV;
        int iBand;

        for( psSV = psValues->psChild, iBand = 1;
             psSV != nullptr;
             psSV = psSV->psNext, iBand++ )
        {
            if( psSV->eType != CXT_Element
                || !EQUAL(psSV->pszValue,"Key")
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
            if( CPLGetXMLValue(psService,"BandIdentifier",nullptr) == nullptr ) {
                bServiceDirty = true;
                CPLSetXMLValue( psService, "BandIdentifier",
                                osBandIdentifier );
            }

            if( CPLGetXMLValue(psService,"BandCount",nullptr) == nullptr ) {
                bServiceDirty = true;
                CPLSetXMLValue( psService, "BandCount",
                                CPLString().Printf("%d",iBand-1));
            }
        }

        // Is this an ESRI server returning a GDAL recognised data type?
        CPLString osDataType = CPLGetXMLValue( psAxis, "DataType", "" );
        if( GDALGetDataTypeByName(osDataType) != GDT_Unknown
            && CPLGetXMLValue(psService,"BandType",nullptr) == nullptr )
        {
            bServiceDirty = true;
            CPLCreateXMLElementAndValue( psService, "BandType", osDataType );
        }
    }

    return true;
}

/************************************************************************/
/*                      ParseCapabilities()                             */
/************************************************************************/

CPLErr WCSDataset110::ParseCapabilities( CPLXMLNode * Capabilities, CPLString url )
{
    CPLStripXMLNamespace(Capabilities, nullptr, TRUE);

    // make sure this is a capabilities document
    if( strcmp(Capabilities->pszValue, "Capabilities") != 0 )
    {
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
    keys2.push_back("Title");
    keys2.push_back("Abstract");
    keys2.push_back("Fees");
    keys2.push_back("AccessConstraints");
    CPLXMLNode *service = AddSimpleMetaData(&metadata, Capabilities, path2, "ServiceIdentification", keys2);
    CPLString kw = GetKeywords(service, "Keywords", "Keyword");
    if (kw != "") {
        CPLString name = path + "Keywords";
        metadata = CSLSetNameValue(metadata, name, kw);
    }
    CPLString profiles = GetKeywords(service, "", "Profile");
    if (profiles != "") {
        CPLString name = path + "Profiles";
        metadata = CSLSetNameValue(metadata, name, profiles);
    }

    // provider metadata
    path2 = path;
    keys2.clear();
    keys2.push_back("ProviderName");
    CPLXMLNode *provider = AddSimpleMetaData(&metadata, Capabilities, path2, "ServiceProvider", keys2);
    if (provider) {
        CPLXMLNode *site = CPLGetXMLNode(provider, "ProviderSite");
        if( site )
        {
            CPLString path3 = path2 + "ProviderSite";
            CPLString value = CPLGetXMLValue(CPLGetXMLNode(site, "href"), nullptr, "");
            metadata = CSLSetNameValue( metadata, path3, value );
        }
        CPLString path3 = path2;
        std::vector<CPLString> keys3;
        keys3.push_back("IndividualName");
        keys3.push_back("PositionName");
        keys3.push_back("Role");
        CPLXMLNode *contact = AddSimpleMetaData(&metadata, provider, path3, "ServiceContact", keys3);
        if (contact) {
            CPLString path4 = path3;
            std::vector<CPLString> keys4;
            keys4.push_back("HoursOfService");
            keys4.push_back("ContactInstructions");
            CPLXMLNode *info = AddSimpleMetaData(&metadata, contact, path4, "ContactInfo", keys4);
            if (info) {
                CPLString path5 = path4;
                std::vector<CPLString> keys5;
                keys5.push_back("DeliveryPoint");
                keys5.push_back("City");
                keys5.push_back("AdministrativeArea");
                keys5.push_back("PostalCode");
                keys5.push_back("Country");
                keys5.push_back("ElectronicMailAddress");
                CPLString path6 = path4;
                std::vector<CPLString> keys6;
                keys6.push_back("Voice");
                keys6.push_back("Facsimile");
                AddSimpleMetaData(&metadata, info, path5, "Address", keys5);
                AddSimpleMetaData(&metadata, info, path6, "Phone", keys6);
            }
        }
    }

    // operations metadata
    CPLString DescribeCoverageURL = "";
    CPLXMLNode *service2 = CPLGetXMLNode(Capabilities, "OperationsMetadata");
    if( service2 )
    {
        for( CPLXMLNode *operation = service2->psChild; operation != nullptr; operation = operation->psNext)
        {
            if( operation->eType != CXT_Element
                || !EQUAL(operation->pszValue, "Operation") )
            {
                continue;
            }
            if( EQUAL(CPLGetXMLValue(CPLGetXMLNode(operation, "name"), nullptr, ""), "DescribeCoverage") )
            {
                DescribeCoverageURL = CPLGetXMLValue(
                    CPLGetXMLNode(CPLSearchXMLNode(operation, "Get"),
                                  "href"), nullptr, "");
            }
        }
    }
    // if DescribeCoverageURL looks wrong, we change it
    if (DescribeCoverageURL.find("localhost") != std::string::npos) {
        DescribeCoverageURL = URLRemoveKey(url, "request");
    }

    // service metadata (in 2.0)
    CPLString ext = "ServiceMetadata";
    CPLString formats = GetKeywords(Capabilities, ext, "formatSupported");
    if (formats != "") {
        CPLString name = path + "formatSupported";
        metadata = CSLSetNameValue(metadata, name, formats);
    }
    // wcs:Extensions: interpolation, CRS, others?
    ext += ".Extension";
    CPLString interpolation = GetKeywords(Capabilities, ext, "interpolationSupported");
    if (interpolation == "") {
        interpolation = GetKeywords(Capabilities, ext + ".InterpolationMetadata", "InterpolationSupported");
    }
    if (interpolation != "") {
        CPLString name = path + "InterpolationSupported";
        metadata = CSLSetNameValue(metadata, name, interpolation);
    }
    CPLString crs = GetKeywords(Capabilities, ext, "crsSupported");
    if (crs == "") {
        crs = GetKeywords(Capabilities, ext + ".CrsMetadata", "crsSupported");
    }
    if (crs != "") {
        CPLString name = path + "crsSupported";
        metadata = CSLSetNameValue(metadata, name, crs);
    }

    this->SetMetadata( metadata, "" );
    CSLDestroy( metadata );
    metadata = nullptr;

    // contents metadata
    CPLXMLNode *contents = CPLGetXMLNode(Capabilities, "Contents");
    if( contents )
    {
        int index = 1;
        for( CPLXMLNode *summary = contents->psChild; summary != nullptr; summary = summary->psNext)
        {
            if( summary->eType != CXT_Element
                || !EQUAL(summary->pszValue, "CoverageSummary") )
            {
                continue;
            }
            CPLString path3;
            path3.Printf( "SUBDATASET_%d_", index);
            index += 1;

            // the name and description of the subdataset:
            // GDAL Data Model:
            // The value of the _NAME is a string that can be passed to GDALOpen() to access the file.

            CPLString key2 = path3 + "NAME";

            CPLString name = DescribeCoverageURL;
            name = CPLURLAddKVP(name, "version", this->Version());

            CPLXMLNode *node = CPLGetXMLNode(summary, "CoverageId");
            CPLString id;
            if (node) {
                id = CPLGetXMLValue(node, nullptr, "");
            } else {
                node = CPLGetXMLNode(summary, "Identifier");
                if (node) {
                    id = CPLGetXMLValue(node, nullptr, "");
                } else {
                    // todo: maybe not an error since CoverageSummary may be within CoverageSummary (07-067r5 Fig4)
                    CSLDestroy( metadata );
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Error in capabilities document.\n" );
                    return CE_Failure;
                }
            }
            name = CPLURLAddKVP(name, "coverage", id);
            name = "WCS:" + name;
            metadata = CSLSetNameValue(metadata, key2, name);

            key2 = path3 + "DESC";
            CPLString desc;

            node = CPLGetXMLNode(summary, "Title");
            if (node) {
                desc = CPLGetXMLValue(node, nullptr, "");
            } else {
                desc = id;
            }

            metadata = CSLSetNameValue(metadata, key2, desc);

            // todo: compose global bounding box from WGS84BoundingBox and BoundingBox

            // further subdataset (coverage) parameters are parsed in ParseCoverageCapabilities

        }
    }
    this->SetMetadata( metadata, "SUBDATASETS" );
    CSLDestroy( metadata );
    return CE_None;
}

void WCSDataset110::ParseCoverageCapabilities(CPLXMLNode *capabilities, const CPLString &coverage, CPLXMLNode *metadata)
{
    CPLStripXMLNamespace(capabilities, nullptr, TRUE);
    CPLXMLNode *contents = CPLGetXMLNode(capabilities, "Contents");
    if( contents )
    {
        for( CPLXMLNode *summary = contents->psChild; summary != nullptr; summary = summary->psNext)
        {
            if( summary->eType != CXT_Element
                || !EQUAL(summary->pszValue, "CoverageSummary") )
            {
                continue;
            }
            CPLXMLNode *node = CPLGetXMLNode(summary, "CoverageId");
            CPLString id;
            if (node) {
                id = CPLGetXMLValue(node, nullptr, "");
            } else {
                node = CPLGetXMLNode(summary, "Identifier");
                if (node) {
                    id = CPLGetXMLValue(node, nullptr, "");
                } else {
                    id = "";
                }
            }
            if (id != coverage) {
                continue;
            }

            // Description
            // todo: there could be Title and Abstract for each supported language
            XMLCopyMetadata(summary, metadata, "Title");
            XMLCopyMetadata(summary, metadata, "Abstract");

            // 2.0.1 stuff
            XMLCopyMetadata(summary, metadata, "CoverageSubtype");

            // Keywords
            CPLString kw = GetKeywords(summary, "Keywords", "Keyword");
            CPLAddXMLAttributeAndValue(
                CPLCreateXMLElementAndValue(metadata, "MDI", kw), "key", "Keywords");

            // WCSContents
            const char *tags[] = {
                "SupportedCRS",
                "SupportedFormat",
                "OtherSource"
            };
            for (unsigned int i = 0; i < CPL_ARRAYSIZE(tags); i++) {
                kw = GetKeywords(summary, "", tags[i]);
                CPLAddXMLAttributeAndValue(
                    CPLCreateXMLElementAndValue(metadata, "MDI", kw), "key", tags[i]);
            }

            // skipping WGS84BoundingBox, BoundingBox, Metadata, Extension
            // since those we'll get from coverage description

        }
    }
}
