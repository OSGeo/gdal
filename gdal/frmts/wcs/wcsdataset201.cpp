/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset class for WCS 2.0.
 * Author:   Ari Jolma <ari dot jolma at gmail dot com>
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
#include "cpl_conv.h"
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
/*                         CoverageSubtype()                            */
/*                                                                      */
/************************************************************************/

static CPLString CoverageSubtype(CPLXMLNode *coverage)
{
    CPLString subtype = CPLGetXMLValue(coverage, "ServiceParameters.CoverageSubtype", "");
    size_t pos = subtype.find("Coverage");
    if (pos != std::string::npos) {
        subtype.erase(pos);
    }
    return subtype;
}

/************************************************************************/
/*                         GetGridNode()                                */
/*                                                                      */
/************************************************************************/

static CPLXMLNode *GetGridNode(CPLXMLNode *coverage, const CPLString &subtype)
{
    CPLXMLNode *grid = nullptr;
    // Construct the name of the node that we look under domainSet.
    // For now we can handle RectifiedGrid and ReferenceableGridByVectors.
    // Note that if this is called at GetCoverage stage, the grid should not be NULL.
    CPLString path = "domainSet";
    if (subtype == "RectifiedGrid") {
        grid = CPLGetXMLNode(coverage, (path + "." + subtype).c_str());
    } else if (subtype == "ReferenceableGrid") {
        grid = CPLGetXMLNode(coverage, (path + "." + subtype + "ByVectors").c_str());
    }
    if (!grid) {
        CPLError(CE_Failure, CPLE_AppDefined, "Can't handle coverages of type '%s'.", subtype.c_str());
    }
    return grid;
}

/************************************************************************/
/*                         ParseParameters()                            */
/*                                                                      */
/************************************************************************/

static void ParseParameters(CPLXMLNode *service,
                            std::vector<CPLString> &dimensions,
                            CPLString &range,
                            std::vector<std::vector<CPLString> > &others)
{
    std::vector<CPLString> parameters = Split(CPLGetXMLValue(service, "Parameters", ""), "&");
    for (unsigned int i = 0; i < parameters.size(); ++i) {
        std::vector<CPLString> kv = Split(parameters[i], "=");
        if (kv.size() < 2) {
            continue;
        }
        kv[0].toupper();
        if (kv[0] == "RANGESUBSET") {
            range = kv[1];
        } else if (kv[0] == "SUBSET") {
            dimensions = Split(kv[1], ";");
        } else {
            std::vector<CPLString> kv2;
            kv2.push_back(kv[0]);
            kv2.push_back(kv[1]);
            others.push_back(kv2);
        }
    }
    // fallback to service values, if any
    if (range == "") {
        range = CPLGetXMLValue(service, "RangeSubset", "");
    }
    if (dimensions.size() == 0) {
        dimensions = Split(CPLGetXMLValue(service, "Subset", ""), ";");
    }
}

/************************************************************************/
/*                         GetExtent()                                  */
/*                                                                      */
/************************************************************************/

std::vector<double> WCSDataset201::GetExtent(int nXOff, int nYOff,
                                             int nXSize, int nYSize,
                                             CPL_UNUSED int nBufXSize, CPL_UNUSED int nBufYSize)
{
    std::vector<double> extent;
    // WCS 2.0 extents are the outer edges of outer pixels.
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

CPLString WCSDataset201::GetCoverageRequest(bool scaled,
                                            int nBufXSize, int nBufYSize,
                                            const std::vector<double> &extent,
                                            CPL_UNUSED CPLString osBandList)
{
    CPLString request = CPLGetXMLValue(psService, "ServiceURL", "");
    request = CPLURLAddKVP(request, "SERVICE", "WCS");
    request += "&REQUEST=GetCoverage";
    request += "&VERSION=" + CPLString(CPLGetXMLValue(psService, "Version", ""));
    request += "&COVERAGEID=" + URLEncode(CPLGetXMLValue(psService, "CoverageName", ""));

    // note: native_crs is not really supported
    if (!native_crs) {
        CPLString crs = URLEncode(CPLGetXMLValue(psService, "SRS", ""));
        request += "&OUTPUTCRS=" + crs;
        request += "&SUBSETTINGCRS=" + crs;
    }

    std::vector<CPLString> domain = Split(CPLGetXMLValue(psService, "Domain", ""), ",");
    if (domain.size() < 2) {
        // eek!
        domain.push_back("E");
        domain.push_back("N");
    }
    const char *x = domain[0].c_str();
    const char *y = domain[1].c_str();
    if (CPLGetXMLBoolean(psService, "SubsetAxisSwap")) {
        const char *tmp = x;
        x = y;
        y = tmp;
    }

    std::vector<CPLString> low = Split(CPLGetXMLValue(psService, "Low", ""), ",");
    std::vector<CPLString> high = Split(CPLGetXMLValue(psService, "High", ""), ",");
    CPLString a = CPLString().Printf("%.17g", extent[0]);
    if (low.size() > 1 && CPLAtof(low[0].c_str()) > extent[0]) {
        a = low[0];
    }
    CPLString b = CPLString().Printf("%.17g", extent[2]);
    if (high.size() > 1 && CPLAtof(high[0].c_str()) < extent[2]) {
        b = high[0];
    }
    /*
    CPLString a = CPLString().Printf(
        "%.17g", MAX(adfGeoTransform[0], extent[0]));
    CPLString b = CPLString().Printf(
        "%.17g", MIN(adfGeoTransform[0] + nRasterXSize * adfGeoTransform[1], extent[2]));
    */

    // 09-147 KVP Protocol: subset keys must be unique
    // GeoServer: seems to require plain SUBSET for x and y

    request += CPLString().Printf("&SUBSET=%s%%28%s,%s%%29", x, a.c_str(), b.c_str());

    a = CPLString().Printf("%.17g", extent[1]);
    if (low.size() > 1 && CPLAtof(low[1].c_str()) > extent[1]) {
        a = low[1];
    }
    b = CPLString().Printf("%.17g", extent[3]);
    if (high.size() > 1 && CPLAtof(high[1].c_str()) < extent[3]) {
        b = high[1];
    }
    /*
    a = CPLString().Printf(
        "%.17g", MAX(adfGeoTransform[3] + nRasterYSize * adfGeoTransform[5], extent[1]));
    b = CPLString().Printf(
        "%.17g", MIN(adfGeoTransform[3], extent[3]));
    */

    request += CPLString().Printf("&SUBSET=%s%%28%s,%s%%29", y, a.c_str(), b.c_str());

    // Dimension and range parameters:
    std::vector<CPLString> dimensions;
    CPLString range;
    std::vector<std::vector<CPLString> > others;
    ParseParameters(psService, dimensions, range, others);

    // set subsets for axis other than x/y
    for (unsigned int i = 0; i < dimensions.size(); ++i) {
        size_t pos = dimensions[i].find("(");
        CPLString dim = dimensions[i].substr(0, pos);
        if (IndexOf(dim, domain) != -1) {
            continue;
        }
        std::vector<CPLString> params = Split(FromParenthesis(dimensions[i]), ",");
        request += "&SUBSET" + CPLString().Printf("%i", i) + "=" + dim + "%28"; // (
        for (unsigned int j = 0; j < params.size(); ++j) {
            // todo: %22 (") should be used only for non-numbers
            request += "%22" + params[j] + "%22";
        }
        request += "%29"; // )
    }

    if (scaled) {
        CPLString tmp;
        // scaling is expressed in grid axes
        if (CPLGetXMLBoolean(psService, "UseScaleFactor")) {
            double fx = fabs((extent[2] - extent[0])/adfGeoTransform[1]/((double)nBufXSize + 0.5));
            double fy = fabs((extent[3] - extent[1])/adfGeoTransform[5]/((double)nBufYSize + 0.5));
            tmp.Printf("&SCALEFACTOR=%.15g", MIN(fx,fy));
        } else {
            std::vector<CPLString> grid_axes = Split(CPLGetXMLValue(psService, "GridAxes", ""), ",");
            if (grid_axes.size() < 2) {
                // eek!
                grid_axes.push_back("E");
                grid_axes.push_back("N");
            }
            tmp.Printf("&SCALESIZE=%s%%28%i%%29,%s%%28%i%%29",
                       grid_axes[0].c_str(), nBufXSize, grid_axes[1].c_str(), nBufYSize);
        }
        request += tmp;
    }

    if (range != "" && range != "*") {
        request += "&RANGESUBSET=" + range;
    }

    // other parameters may come from
    // 1) URL (others)
    // 2) Service file
    const char *keys[] = {
        WCS_URL_PARAMETERS
    };
    for (unsigned int i = 0; i < CPL_ARRAYSIZE(keys); i++) {
        CPLString value;
        int ix = IndexOf(CPLString(keys[i]).toupper(), others);
        if (ix >= 0) {
            value = others[ix][1];
        } else {
            value = CPLGetXMLValue(psService, keys[i], "");
        }
        if (value != "") {
            request = CPLURLAddKVP(request, keys[i], value);
        }
    }
    // add extra parameters
    CPLString extra = CPLGetXMLValue(psService, "Parameters", "");
    if (extra != "") {
        std::vector<CPLString> pairs = Split(extra, "&");
        for (unsigned int i = 0; i < pairs.size(); ++i) {
            std::vector<CPLString> pair = Split(pairs[i], "=");
            request = CPLURLAddKVP(request, pair[0], pair[1]);
        }
    }
    std::vector<CPLString> pairs = Split(CPLGetXMLValue(psService, "GetCoverageExtra", ""), "&");
    for (unsigned int i = 0; i < pairs.size(); ++i) {
        std::vector<CPLString> pair = Split(pairs[i], "=");
        if (pair.size() > 1) {
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

CPLString WCSDataset201::DescribeCoverageRequest()
{
    CPLString request = CPLGetXMLValue( psService, "ServiceURL", "" );
    request = CPLURLAddKVP(request, "SERVICE", "WCS");
    request = CPLURLAddKVP(request, "REQUEST", "DescribeCoverage");
    request = CPLURLAddKVP(request, "VERSION", CPLGetXMLValue( psService, "Version", "2.0.1" ));
    request = CPLURLAddKVP(request, "COVERAGEID", CPLGetXMLValue( psService, "CoverageName", "" ));
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
    CPLDebug("WCS", "Requesting %s", request.c_str());
    return request;
}

/************************************************************************/
/*                             GridOffsets()                            */
/*                                                                      */
/************************************************************************/

bool WCSDataset201::GridOffsets(CPLXMLNode *grid,
                                CPLString subtype,
                                bool swap_grid_axis,
                                std::vector<double> &origin,
                                std::vector<std::vector<double> > &offset,
                                std::vector<CPLString> axes,
                                char ***metadata)
{
    // todo: use domain_index

    // origin position, center of cell
    CPLXMLNode *point = CPLGetXMLNode(grid, "origin.Point.pos");
    origin = Flist(Split(CPLGetXMLValue(point, nullptr, ""), " ", axis_order_swap), 0, 2);

    // offsets = coefficients of affine transformation from cell coords to
    // CRS coords, (1,2) and (4,5)

    if (subtype == "RectifiedGrid") {

        // for rectified grid the geo transform is from origin and offsetVectors
        int i = 0;
        for (CPLXMLNode *node = grid->psChild; node != nullptr; node = node->psNext) {
            if (node->eType != CXT_Element || !EQUAL(node->pszValue, "offsetVector")) {
                continue;
            }
            offset.push_back(Flist(Split(CPLGetXMLValue(node, nullptr, ""), " ", axis_order_swap), 0, 2));
            if (i == 1) {
                break;
            }
            i++;
        }
        if (offset.size() < 2) {
            // error or not?
            std::vector<double> x;
            x.push_back(1);
            x.push_back(0);
            std::vector<double> y;
            y.push_back(0);
            y.push_back(1);
            offset.push_back(x);
            offset.push_back(y);
        }
        // if axis_order_swap
        // the offset order should be swapped
        // Rasdaman does it
        // MapServer and GeoServer not
        if (swap_grid_axis) {
            std::vector<double> tmp = offset[0];
            offset[0] = offset[1];
            offset[1] = tmp;
        }

    } else { // if (coverage_type == "ReferenceableGrid"(ByVector)) {

        // for vector referenceable grid the geo transform is from
        // offsetVector, coefficients, gridAxesSpanned, sequenceRule
        // in generalGridAxis.GeneralGridAxis
        for (CPLXMLNode *node = grid->psChild; node != nullptr; node = node->psNext) {
            CPLXMLNode *axis = CPLGetXMLNode(node, "GeneralGridAxis");
            if (!axis) {
                continue;
            }
            CPLString spanned = CPLGetXMLValue(axis, "gridAxesSpanned", "");
            int index = IndexOf(spanned, axes);
            if (index == -1) {
                CPLError(CE_Failure, CPLE_AppDefined, "This is not a rectilinear grid(?).");
                return false;
            }
            CPLString coeffs = CPLGetXMLValue(axis, "coefficients", "");
            if (coeffs != "") {
                *metadata = CSLSetNameValue(*metadata, CPLString().Printf("DIMENSION_%i_COEFFS", index), coeffs);
            }
            CPLString order = CPLGetXMLValue(axis, "sequenceRule.axisOrder", "");
            CPLString rule = CPLGetXMLValue(axis, "sequenceRule", "");
            if (!(order == "+1" && rule == "Linear")) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Grids with sequence rule '%s' and axis order '%s' are not supported.",
                         rule.c_str(), order.c_str());
                return false;
            }
            CPLXMLNode *offset_node = CPLGetXMLNode(axis, "offsetVector");
            if (offset_node) {
                offset.push_back(Flist(Split(CPLGetXMLValue(offset_node, nullptr, ""), " ", axis_order_swap), 0, 2));
            } else {
                CPLError(CE_Failure, CPLE_AppDefined, "Missing offset vector in grid axis.");
                return false;
            }
        }
        // todo: make sure offset order is the same as the axes order but see above

    }
    if (origin.size() < 2 || offset.size() < 2) {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not parse origin or offset vectors from grid.");
        return false;
    }
    return true;
}

/************************************************************************/
/*                             GetSubdataset()                          */
/*                                                                      */
/************************************************************************/

CPLString WCSDataset201::GetSubdataset(const CPLString &coverage)
{
    char **metadata = GDALPamDataset::GetMetadata("SUBDATASETS");
    CPLString subdataset;
    if (metadata != nullptr) {
        for (int i = 0; metadata[i] != nullptr; ++i) {
            char *key;
            CPLString url = CPLParseNameValue(metadata[i], &key);
            if (key != nullptr && strstr(key, "SUBDATASET_") && strstr(key, "_NAME")) {
                if (coverage == CPLURLGetValue(url, "coverageId")) {
                    subdataset = key;
                    subdataset.erase(subdataset.find("_NAME"), 5);
                    CPLFree(key);
                    break;
                }
            }
            CPLFree(key);
        }
    }
    return subdataset;
}

/************************************************************************/
/*                             SetFormat()                              */
/*                                                                      */
/************************************************************************/

bool WCSDataset201::SetFormat(CPLXMLNode *coverage)
{
    // set the Format value in service,
    // unless it is set by the user
    CPLString format = CPLGetXMLValue(psService, "Format", "");

    // todo: check the value against list of supported formats?
    if (format != "") {
        return true;
    }

/*      We will prefer anything that sounds like TIFF, otherwise        */
/*      falling back to the first supported format.  Should we          */
/*      consider preferring the nativeFormat if available?              */

    char **metadata = GDALPamDataset::GetMetadata(nullptr);
    const char *value = CSLFetchNameValue(metadata, "WCS_GLOBAL#formatSupported");
    if (value == nullptr) {
        format = CPLGetXMLValue(coverage, "ServiceParameters.nativeFormat", "");
    } else {
        std::vector<CPLString> format_list = Split(value, ",");
        for (unsigned j = 0; j < format_list.size(); ++j) {
            if (format_list[j].ifind("tiff") != std::string::npos) {
                format = format_list[j];
                break;
            }
        }
        if (format == "" && format_list.size() > 0) {
            format = format_list[0];
        }
    }
    if (format != "") {
        CPLSetXMLValue(psService, "Format", format);
        bServiceDirty = true;
        return true;
    } else {
        return false;
    }
}

/************************************************************************/
/*                         ParseGridFunction()                          */
/*                                                                      */
/************************************************************************/

bool WCSDataset201::ParseGridFunction(CPLXMLNode *coverage, std::vector<int> &axisOrder)
{
    CPLXMLNode *function = CPLGetXMLNode(coverage, "coverageFunction.GridFunction");
    if (function) {
        CPLString path = "sequenceRule";
        CPLString sequenceRule = CPLGetXMLValue(function, path, "");
        path += ".axisOrder";
        axisOrder = Ilist(Split(CPLGetXMLValue(function, path, ""), " "));
        // for now require simple
        if (sequenceRule != "Linear") {
            CPLError(CE_Failure, CPLE_AppDefined, "Can't handle '%s' coverages.", sequenceRule.c_str());
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                             ParseRange()                             */
/*                                                                      */
/************************************************************************/

int WCSDataset201::ParseRange(CPLXMLNode *coverage, const CPLString &range_subset, char ***metadata)
{
    int fields = 0;
    // Default is to include all (types permitting?)
    // Can also be controlled with Range parameter

    // The contents of a rangeType is a swe:DataRecord
    CPLString path = "rangeType.DataRecord";
    CPLXMLNode *record = CPLGetXMLNode(coverage, path);
    if (!record) {
        CPLError(CE_Failure, CPLE_AppDefined, "Attributes are not defined in a DataRecord, giving up.");
        return 0;
    }

    // mapserver does not like field names, it wants indexes
    // so we should be able to give those

    // if Range is set remove those not in it
    std::vector<CPLString> range = Split(range_subset, ",");
    // todo: add check for range subsetting profile existence in server metadata here
    unsigned int range_index = 0; // index for reading from range
    bool in_band_range = false;

    unsigned int field_index = 1;
    CPLString field_name;
    std::vector<CPLString> nodata_array;

    for (CPLXMLNode *field = record->psChild; field != nullptr; field = field->psNext) {
        if (field->eType != CXT_Element || !EQUAL(field->pszValue, "field")) {
            continue;
        }
        CPLString fname = CPLGetXMLValue(field, "name", "");
        bool include = true;

        if (range.size() > 0) {
            include = false;
            if (range_index < range.size()) {
                CPLString current_range = range[range_index];
                CPLString fname_test;

                if (atoi(current_range) != 0) {
                    fname_test = CPLString().Printf("%i", field_index);
                } else {
                    fname_test = fname;
                }

                if (current_range == "*") {
                    include = true;
                } else if (current_range == fname_test) {
                    include = true;
                    range_index += 1;
                } else if (current_range.find(fname_test + ":") != std::string::npos) {
                    include = true;
                    in_band_range = true;
                } else if (current_range.find(":" + fname_test) != std::string::npos) {
                    include = true;
                    in_band_range = false;
                    range_index += 1;
                } else if (in_band_range) {
                    include = true;
                }
            }
        }

        if (include) {
            CPLString key;
            key.Printf("FIELD_%i_", field_index);
            *metadata = CSLSetNameValue(*metadata, (key + "NAME").c_str(), fname);

            CPLString nodata = CPLGetXMLValue(field, "Quantity.nilValues.NilValue", "");
            if (nodata != "") {
                *metadata = CSLSetNameValue(*metadata, (key + "NODATA").c_str(), nodata);
            }

            CPLString descr = CPLGetXMLValue(field, "Quantity.description", "");
            if (descr != "") {
                *metadata = CSLSetNameValue(*metadata, (key + "DESCR").c_str(), descr);
            }

            path = "Quantity.constraint.AllowedValues.interval";
            CPLString interval = CPLGetXMLValue(field, path, "");
            if (interval != "") {
                *metadata = CSLSetNameValue(*metadata, (key + "INTERVAL").c_str(), interval);
            }

            if (field_name == "") {
                field_name = fname;
            }
            nodata_array.push_back(nodata);
            fields += 1;
        }

        field_index += 1;
    }

    if (fields == 0) {
        CPLError(CE_Failure, CPLE_AppDefined, "No data fields found (bad Range?).");
    } else {
        // todo: default to the first one?
        bServiceDirty = CPLUpdateXML(psService, "NoDataValue", Join(nodata_array, ",")) || bServiceDirty;
    }

    return fields;
}

/************************************************************************/
/*                          ExtractGridInfo()                           */
/*                                                                      */
/*      Collect info about grid from describe coverage for WCS 2.0.     */
/*                                                                      */
/************************************************************************/

bool WCSDataset201::ExtractGridInfo()
{
    // this is for checking what's in service and for filling in empty slots in it
    // if the service file can be considered ready for use, this could be skipped

    CPLXMLNode *coverage = CPLGetXMLNode(psService, "CoverageDescription");

    if (coverage == nullptr) {
        CPLError(CE_Failure, CPLE_AppDefined, "CoverageDescription missing from service.");
        return false;
    }

    CPLString subtype = CoverageSubtype(coverage);

    // get CRS from boundedBy.Envelope and set the native flag to true
    // below we may set the CRS again but that won't be native (however, non native CRS is not yet supported)
    // also axis order swap is set
    CPLString path = "boundedBy.Envelope";
    CPLXMLNode *envelope = CPLGetXMLNode(coverage, path);
    if( envelope == nullptr )
    {
        path = "boundedBy.EnvelopeWithTimePeriod";
        envelope = CPLGetXMLNode(coverage, path);
        if( envelope == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing boundedBy.Envelope");
            return false;
        }
    }
    std::vector<CPLString> bbox = ParseBoundingBox(envelope);
    if (!SetCRS(ParseCRS(envelope), true) || bbox.size() < 2) {
        return false;
    }

    // has the user set the domain?
    std::vector<CPLString> domain = Split(CPLGetXMLValue(psService, "Domain", ""), ",");

    // names of axes
    std::vector<CPLString> axes = Split(
        CPLGetXMLValue(coverage, (path + ".axisLabels").c_str(), ""), " ", axis_order_swap);
    std::vector<CPLString> uoms = Split(
        CPLGetXMLValue(coverage, (path + ".uomLabels").c_str(), ""), " ", axis_order_swap);

    if (axes.size() < 2) {
        CPLError(CE_Failure, CPLE_AppDefined, "The coverage has less than 2 dimensions or no axisLabels.");
        return false;
    }

    std::vector<int> domain_indexes = IndexOf(domain, axes);
    if (Contains(domain_indexes, -1)) {
        CPLError(CE_Failure, CPLE_AppDefined, "Axis in given domain does not exist in coverage.");
        return false;
    }
    if (domain_indexes.size() == 0) { // default is the first two
        domain_indexes.push_back(0);
        domain_indexes.push_back(1);
    }
    if (domain.size() == 0) {
        domain.push_back(axes[0]);
        domain.push_back(axes[1]);
        CPLSetXMLValue(psService, "Domain", Join(domain, ","));
        bServiceDirty = true;
    }

    // GridFunction (is optional)
    // We support only linear grid functions.
    // axisOrder determines how data is arranged in the grid <order><axis number>
    // specifically: +2 +1 => swap grid envelope and the order of the offsets
    std::vector<int> axisOrder;
    if (!ParseGridFunction(coverage, axisOrder)) {
        return false;
    }

    const char *md_domain = "";
    char **metadata = CSLDuplicate(GetMetadata(md_domain)); // coverage metadata to be added/updated

    metadata = CSLSetNameValue(metadata, "DOMAIN", Join(domain, ","));

    // add coverage metadata: GeoServer TimeDomain

    CPLXMLNode *timedomain = CPLGetXMLNode(coverage, "metadata.Extension.TimeDomain");
    if (timedomain) {
        std::vector<CPLString> timePositions;
        // "//timePosition"
        for (CPLXMLNode *node = timedomain->psChild; node != nullptr; node = node->psNext) {
            if (node->eType != CXT_Element || strcmp(node->pszValue, "TimeInstant") != 0) {
                continue;
            }
            for (CPLXMLNode *node2 = node->psChild; node2 != nullptr; node2 = node2->psNext) {
                if (node2->eType != CXT_Element || strcmp(node2->pszValue, "timePosition") != 0) {
                    continue;
                }
                timePositions.push_back(CPLGetXMLValue(node2, "", ""));
            }
        }
        metadata = CSLSetNameValue(metadata, "TimeDomain", Join(timePositions, ","));
    }

    // dimension metadata

    std::vector<CPLString> slow = Split(bbox[0], " ", axis_order_swap);
    std::vector<CPLString> shigh = Split(bbox[1], " ", axis_order_swap);
    bServiceDirty = CPLUpdateXML(psService, "Low", Join(slow, ",")) || bServiceDirty;
    bServiceDirty = CPLUpdateXML(psService, "High", Join(shigh, ",")) || bServiceDirty;
    if (slow.size() < 2 || shigh.size() < 2) {
        CPLError(CE_Failure, CPLE_AppDefined, "The coverage has less than 2 dimensions.");
        CSLDestroy(metadata);
        return false;
    }
    // todo: if our x,y domain is not the first two? use domain_indexes?
    std::vector<double> low = Flist(slow, 0, 2);
    std::vector<double> high = Flist(shigh, 0, 2);
    std::vector<double> env;
    env.insert(env.end(), low.begin(), low.begin() + 2);
    env.insert(env.end(), high.begin(), high.begin() + 2);

    for (unsigned int i = 0; i < axes.size(); ++i) {
        CPLString key;
        key.Printf("DIMENSION_%i_", i);
        metadata = CSLSetNameValue(metadata, (key + "AXIS").c_str(), axes[i]);
        if (i < uoms.size()) {
            metadata = CSLSetNameValue(metadata, (key + "UOM").c_str(), uoms[i]);
        }
        if (i < 2) {
            metadata = CSLSetNameValue(metadata, (key + "INTERVAL").c_str(),
                                       CPLString().Printf("%.15g,%.15g", low[i], high[i]));
        } else if( i < slow.size() && i < shigh.size() ) {
            metadata = CSLSetNameValue(metadata, (key + "INTERVAL").c_str(),
                                       CPLString().Printf("%s,%s", slow[i].c_str(), shigh[i].c_str()));
        } else if (i < bbox.size()) {
            metadata = CSLSetNameValue(metadata, (key + "INTERVAL").c_str(), bbox[i].c_str());
        }
    }

    // domainSet
    // requirement 23: the srsName here _shall_ be the same as in boundedBy
    // => we ignore it
    // the CRS of this dataset is from boundedBy (unless it is overridden)
    // this is the size of this dataset
    // this gives the geotransform of this dataset (unless there is CRS override)

    CPLXMLNode *grid = GetGridNode(coverage, subtype);
    if (!grid) {
        CSLDestroy(metadata);
        return false;
    }

    //
    bool swap_grid_axis = false;
    if (axisOrder.size() >= 2
        && axisOrder[domain_indexes[0]] == 2
        && axisOrder[domain_indexes[1]] == 1)
    {
        swap_grid_axis = !CPLGetXMLBoolean(psService, "NoGridAxisSwap");
    }
    path = "limits.GridEnvelope";
    std::vector<std::vector<int> > size = ParseGridEnvelope(CPLGetXMLNode(grid, path), swap_grid_axis);
    std::vector<int> grid_size;
    if (size.size() < 2) {
        CPLError(CE_Failure, CPLE_AppDefined, "Can't parse the grid envelope.");
        CSLDestroy(metadata);
        return false;
    }

    grid_size.push_back(size[1][domain_indexes[0]] - size[0][domain_indexes[0]] + 1);
    grid_size.push_back(size[1][domain_indexes[1]] - size[0][domain_indexes[1]] + 1);

    path = "axisLabels";
    bool swap_grid_axis_labels = swap_grid_axis || CPLGetXMLBoolean(psService, "GridAxisLabelSwap");
    std::vector<CPLString> grid_axes = Split(CPLGetXMLValue(grid, path, ""), " ", swap_grid_axis_labels);
    // autocorrect MapServer thing
    if (grid_axes.size() >= 2 && grid_axes[0] == "lat" && grid_axes[1] == "long") {
        grid_axes[0] = "long";
        grid_axes[1] = "lat";
    }
    bServiceDirty = CPLUpdateXML(psService, "GridAxes", Join(grid_axes, ",")) || bServiceDirty;

    std::vector<double> origin;
    std::vector<std::vector<double> > offsets;
    if (!GridOffsets(grid, subtype, swap_grid_axis, origin, offsets, axes, &metadata)) {
        CSLDestroy(metadata);
        return false;
    }

    SetGeometry(grid_size, origin, offsets);

    // subsetting and dimension to bands
    std::vector<CPLString> dimensions;
    CPLString range;
    std::vector<std::vector<CPLString> > others;
    ParseParameters(psService, dimensions, range, others);

    // it is ok to have trimming or even slicing for x/y, it just affects our bounding box
    // but that is a todo item
    // todo: BoundGeometry(domain_trim) if domain_trim.size() > 0
    std::vector<std::vector<double> > domain_trim;

    // are all dimensions that are not x/y domain sliced?
    // if not, bands can't be defined, see below
    bool dimensions_are_ok = true;
    for (unsigned int i = 0; i < axes.size(); ++i) {
        std::vector<CPLString> params;
        for (unsigned int j = 0; j < dimensions.size(); ++j) {
            if (dimensions[j].find(axes[i] + "(") != std::string::npos) {
                params = Split(FromParenthesis(dimensions[j]), ",");
                break;
            }
        }
        int domain_index = IndexOf(axes[i], domain);
        if (domain_index != -1) {
            std::vector<double> trim = Flist(params, 0, 2);
            domain_trim.push_back(trim);
            continue;
        }
        // size == 1 => sliced
        if (params.size() != 1) {
            dimensions_are_ok = false;
        }
    }
    // todo: add metadata: note: no bands, you need to subset to get data

    // check for CRS override
    CPLString crs = CPLGetXMLValue(psService, "SRS", "");
    if (crs != "" && crs != osCRS) {
        if (!SetCRS(crs, false)) {
            CSLDestroy(metadata);
            return false;
        }
        // todo: support CRS override, it requires warping the grid to the new CRS
        // SetGeometry(grid_size, origin, offsets);
        CPLError(CE_Failure, CPLE_AppDefined, "CRS override not yet supported.");
        CSLDestroy(metadata);
        return false;
    }

    // todo: ElevationDomain, DimensionDomain

    // rangeType

    // get the field metadata
    // get the count of fields
    // if Range is set in service that may limit the fields
    int fields = ParseRange(coverage, range, &metadata);
    // if fields is 0 an error message has been emitted
    // but we let this go on since the user may be experimenting
    // and she wants to see the resulting metadata and not just an error message
    // situation is ~the same when bands == 0 when we exit here

    // todo: do this only if metadata is dirty
    this->SetMetadata(metadata, md_domain);
    CSLDestroy(metadata);
    TrySaveXML();

    // determine the band count
    int bands = 0;
    if (dimensions_are_ok) {
        bands = fields;
    }
    bServiceDirty = CPLUpdateXML(psService, "BandCount", CPLString().Printf("%d",bands))
        || bServiceDirty;

    // set the Format value in service, unless it is set
    // by the user, either through direct edit or options
    if (!SetFormat(coverage)) {
        // all attempts to find a format have failed...
        CPLError(CE_Failure, CPLE_AppDefined, "All attempts to find a format have failed, giving up.");
        return false;
    }

    return true;
}
