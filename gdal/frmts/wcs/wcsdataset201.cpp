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

static CPLString CoverageSubtype(CPLXMLNode *coverage)
{
    CPLString subtype = CPLGetXMLValue(coverage, "ServiceParameters.CoverageSubtype", "");
    size_t pos = subtype.find("Coverage");
    if (pos != std::string::npos) {
        subtype.erase(pos);
    }
    return subtype;
}


static CPLXMLNode *GetGridNode(CPLXMLNode *coverage, CPLString subtype)
{
    CPLXMLNode *grid = NULL;
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

CPLString WCSDataset201::GetCoverageRequest(CPL_UNUSED bool scaled,
                                            int nBufXSize, int nBufYSize,
                                            std::vector<double> extent,
                                            CPL_UNUSED CPLString osBandList)
{
    CPLString request = CPLGetXMLValue(psService, "ServiceURL", ""), tmp;
    request += "SERVICE=WCS";
    request += "&REQUEST=GetCoverage";
    request += "&VERSION=" + String(CPLGetXMLValue(psService, "Version", ""));
    request += "&COVERAGEID=" + URLEncode(CPLGetXMLValue(psService, "CoverageName", ""));
    if (!native_crs) {
        request += "&OUTPUTCRS=" + URLEncode(CPLGetXMLValue(psService, "CRS", ""));
    }
    request += "&FORMAT=" + URLEncode(CPLGetXMLValue(psService, "PreferredFormat", ""));
    // todo updatesequence

    std::vector<CPLString> domain = Split(CPLGetXMLValue(psService, "Domain", ""), ",");
    tmp.Printf("&SUBSET=%s(%.15g,%.15g),%s(%.15g,%.15g)",
               domain[0].c_str(), extent[0], extent[2],
               domain[1].c_str(), extent[1], extent[3]);
    request += tmp;
    if (!native_crs) {
        request += "&SUBSETTINGCRS=" + URLEncode(CPLGetXMLValue(psService, "CRS", ""));
    }

    // todo: set subset to slice to axis on some value other than x/y domain
    // if DimensionToBand
    std::vector<CPLString> dimensions = Split(CPLGetXMLValue(psService, "Dimensions", ""), ";");
    for (unsigned int i = 0; i < dimensions.size(); ++i) {
        request += "&SUBSET=" + dimensions[i];
    }

    tmp.Printf("&SCALESIZE=%s(%i),%s(%i)", domain[0].c_str(), nBufXSize, domain[1].c_str(), nBufYSize);
    request += tmp;
    CPLString interpolation = CPLGetXMLValue(psService, "Interpolation", "");
    if (interpolation != "") {
        request += "&INTERPOLATION=" + interpolation;
    }
    
    CPLString range = CPLGetXMLValue(psService, "Range", "");
    if (range != "") {
        request += "&RANGESUBSET=" + range;
    }

    // todo other stuff, e.g., GeoTIFF encoding

    return request;

}

CPLString WCSDataset201::DescribeCoverageRequest()
{
    CPLString request;
    request.Printf(
        "%sSERVICE=WCS&REQUEST=DescribeCoverage&VERSION=%s&COVERAGEID=%s%s&FORMAT=text/xml",
        CPLGetXMLValue( psService, "ServiceURL", "" ),
        CPLGetXMLValue( psService, "Version", "1.0.0" ),
        CPLGetXMLValue( psService, "CoverageName", "" ),
        CPLGetXMLValue( psService, "DescribeCoverageExtra", "" ) );
    return request;
}

static bool GridOffsets(CPLXMLNode *grid,
                        std::vector<std::vector<double>> &offset,
                        std::vector<CPLString> labels,
                        CPLString subtype,
                        CPLString crs,
                        bool swap)
{
    // todo: use domain_index
    if (subtype == "RectifiedGrid") {

        // for rectified grid the geo transform is from origin and offsetVectors
        int i = 0;
        for (CPLXMLNode *node = grid->psChild; node != NULL; node = node->psNext) {
            if (node->eType != CXT_Element || !EQUAL(node->pszValue, "offsetVector")) {
                continue;
            }
            // check srs is the same
            CPLString crs2 = ParseCRS(node);
            if (crs2 != "" && crs2 != crs) {
                CPLError(CE_Failure, CPLE_AppDefined, "SRS mismatch between origin and offset vector.");
                return false;
            }
            offset.push_back(Flist(Split(CPLGetXMLValue(node, NULL, ""), " ", swap)));
            i++;
        }

    } else { // if (coverage_type == "ReferenceableGrid"(ByVector)) {

        // for vector referenceable grid the geo transform is from
        // offsetVector, coefficients, gridAxesSpanned, sequenceRule
        // in generalGridAxis.GeneralGridAxis
        for (CPLXMLNode *node = grid->psChild; node != NULL; node = node->psNext) {
            CPLXMLNode *axis = CPLGetXMLNode(node, "GeneralGridAxis");
            if (!axis) {
                continue;
            }
            /*
            CPLString coeffs = CPLGetXMLValue(axis, "coefficients", "");
            if (coeffs != "") {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "This is not a uniform grid, coefficients: '%s'.", coeffs.c_str());
                return false;
            }
            */
            CPLString spanned = CPLGetXMLValue(axis, "gridAxesSpanned", "");
            unsigned int i = 0;
            while (spanned != labels[i] && i < labels.size()) {
                i = i + 1;
            }
            if (i >= labels.size()) {
                CPLError(CE_Failure, CPLE_AppDefined, "This is not a rectilinear grid(?).");
                return false;
            }
            CPLString order = CPLGetXMLValue(axis, "sequenceRule.axisOrder", "");
            CPLString rule = CPLGetXMLValue(axis, "sequenceRule", "");
            if (!(order == "+1" && rule == "Linear")) {
                CPLError(CE_Failure, CPLE_AppDefined, "The grid is not linear and increasing from origo.");
                return false;
            }
            CPLXMLNode *offset_node = CPLGetXMLNode(axis, "offsetVector");
            if (offset_node) {
                CPLString crs2 = ParseCRS(node);
                if (crs2 != "" && crs2 != crs) {
                    CPLError(CE_Failure, CPLE_AppDefined, "SRS mismatch between origin and offset vector.");
                    return false;
                }
                offset.push_back(Flist(Split(CPLGetXMLValue(offset_node, NULL, ""), " ", swap)));
            } else {
                CPLError(CE_Failure, CPLE_AppDefined, "Missing offset vector in grid axis.");
                return false;
            }
        }

    }
    if (offset.size() < 2) {
        CPLError(CE_Failure, CPLE_AppDefined, "Not enough offset vectors in grid.");
        return false;
    }
    return true;
}

CPLString WCSDataset201::GetSubdataset(CPLString coverage)
{
    char **metadata = GDALPamDataset::GetMetadata("SUBDATASETS");
    CPLString subdataset;
    if (metadata != NULL) {
        for (int i = 0; metadata[i] != NULL; ++i) {
            CPLString key;
            key.Printf("SUBDATASET_%i_NAME", i);
            CPLString url = CSLFetchNameValue(metadata, key);            
            if (!url) {
                continue;
            }
            if (coverage == CPLURLGetValue(url, "coverageId")) {
                subdataset.Printf("SUBDATASET_%i", i);
                break;
            }
        }
        CSLDestroy(metadata);
    }
    return subdataset;
}

bool WCSDataset201::SetFormat(CPLXMLNode *coverage)
{
    // set the PreferredFormat value in service, unless is set
    // by the user, either through direct edit or options
    CPLString format = CPLGetXMLValue(psService, "PreferredFormat", "");
    
    // todo: check the value against list of supported formats
    if (format != "") {
        return true;
    }
    
/*      We will prefer anything that sounds like TIFF, otherwise        */
/*      falling back to the first supported format.  Should we          */
/*      consider preferring the nativeFormat if available?              */

    char **metadata = GDALPamDataset::GetMetadata(NULL);
    int i = CSLFindString(metadata, "WCS_GLOBAL#formatSupported");
    if (i == -1) {
        format = CPLGetXMLValue(coverage, "ServiceParameters.nativeFormat", "");
    } else {
        std::vector<CPLString> format_list = Split(metadata[i], ",");
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
    CSLDestroy(metadata);
    if (format != "") {
        if (!CPLSetXMLValue(psService, "PreferredFormat", format)) {
            CPLCreateXMLElementAndValue(psService, "PreferredFormat", format);
        }
        return true;
    } else {
        return false;
    }
}

bool WCSDataset201::ParseGridFunction(std::vector<CPLString> &axisOrder)
{
    CPLXMLNode *function = CPLGetXMLNode(psService, "coverageFunction.GridFunction");
    if (function) {
        CPLString path = "sequenceRule";
        CPLString sequenceRule = CPLGetXMLValue(function, path, "");
        path += ".axisOrder";
        axisOrder = Split(CPLGetXMLValue(function, path, ""), " ");
        path = "startPoint";
        std::vector<CPLString> startPoint = Split(CPLGetXMLValue(function, path, ""), " ");
        // for now require simple
        if (sequenceRule != "Linear") {
            CPLError(CE_Failure, CPLE_AppDefined, "Can't handle '%s' coverages.", sequenceRule.c_str());
            return false;
        }
    }
    return true;
}

bool WCSDataset201::ParseRange(CPLXMLNode *coverage, char ***metadata, unsigned int &bands)
{
    // The range
    // Default is to include all (types permitting?) unless dimension_to_band
    // Can also be controlled with Range parameter
       
    // assuming here that the attributes are defined by swe:DataRecord
    CPLString path = "rangeType";
    CPLXMLNode *record = CPLGetXMLNode(coverage, (path + ".DataRecord").c_str());
    if (!record) {
        CPLError(CE_Failure, CPLE_AppDefined, "Attributes are not defined in a DataRecord, giving up.");
        return false;
    }
    
    // if Range is set remove those not in it
    std::vector<CPLString> range = Split(CPLGetXMLValue(psService, "Range", ""), ",");
    // todo: add check for range subsetting profile existence in server metadata here
    std::vector<CPLString> band_name;
    unsigned int range_index = 0; // index for reading from range
    bool in_band_range = false;

    unsigned int field_index = 1;
    std::vector<CPLString> band_descr;
    std::vector<CPLString> band_nodata;
    std::vector<CPLString> band_interval;
    
    for (CPLXMLNode *field = record->psChild; field != NULL; field = field->psNext) {
        if (field->eType != CXT_Element || !EQUAL(field->pszValue, "field")) {
            continue;
        }
        CPLString fname = CPLGetXMLValue(field, "name", "");
        bool include = true;
        if (range.size() > 0) {
            CPLString current_range = range[range_index];
            if (fname == current_range) {
            } else if (current_range.find(fname + ":") != std::string::npos) {
                in_band_range = true;
            } else if (current_range.find(":" + fname) != std::string::npos) {
                in_band_range = false;
            } else {
                if (!in_band_range) {
                    include = false;
                    range_index += 1;
                    if (range_index >= range.size()) {
                        break;
                    }
                }
            }
        }
        
        CPLString key;
        key.Printf("FIELD_%i_", field_index);
        field_index += 1;
        
        *metadata = CSLSetNameValue(*metadata, (key + "NAME").c_str(), fname);
        CPLString nodata = CPLGetXMLValue(field, "Quantity.nilValues.NilValue", "");
        *metadata = CSLSetNameValue(*metadata, (key + "NODATA").c_str(), nodata);
        CPLString descr = CPLGetXMLValue(field, "Quantity.description", "");
        *metadata = CSLSetNameValue(*metadata, (key + "DESCR").c_str(), descr);
        path = "Quantity.constraint.AllowedValues.interval";
        CPLString interval = CPLGetXMLValue(field, path, "");
        *metadata = CSLSetNameValue(*metadata, (key + "INTERVAL").c_str(), interval);
            
        if (include) {
            band_name.push_back(fname);
            band_descr.push_back(descr);
            band_nodata.push_back(nodata);
            band_interval.push_back(interval);
            //std::vector<double> interval = Flist(Split(CPLGetXMLValue(field, path, ""), " "));
            bands += 1;
        }
    }
    // todo: update XML element value
    CPLCreateXMLElementAndValue(psService, "NoDataValue", Join(band_nodata, ","));
    // todo: if Range was not set and we have a usable default, set it

    // may we should set something for GetCoverage if not set
    if (range.size() == 0) {
        CPLCreateXMLElementAndValue(psService, "Range", Join(band_name, ","));
    }
    

    return true;
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

    if (coverage == NULL) {
        CPLError(CE_Failure, CPLE_AppDefined, "CoverageDescription missing from service. RECREATE_SERVICE?");
        return false;
    }

    CPLString subtype = CoverageSubtype(coverage);
    CPLXMLNode *grid = GetGridNode(coverage, subtype);
    if (!grid) {
        return false;
    }

    // GridFunction (is optional)
    // We support only linear grid functions.
    std::vector<CPLString> axisOrder;
    if (!ParseGridFunction(axisOrder)) {
        return false;
    }

    // get CRS from boundedBy.Envelope and set the native flag to true
    // below we may set the CRS again but that won't be native
    CPLString path = "boundedBy.Envelope";
    CPLXMLNode *envelope = CPLGetXMLNode(coverage, path);
    if (!SetCRS(ParseCRS(envelope), true)) {
        return false;
    }
    std::vector<CPLString> axes = Split(
        CPLGetXMLValue(coverage, (path + ".axisLabels").c_str(), ""), " ", axis_order_swap);
    std::vector<CPLString> uoms = Split(
        CPLGetXMLValue(coverage, (path + ".uomLabels").c_str(), ""), " ", axis_order_swap);

    // todo: axes are the domain dimension names
    // these need to be reported to the user
    // and the user may select mappings name => x, name => y, name => bands
    // or we'll use the first two by default, and possibly make t => bands
    // this will be managed by
    // CPLGetXMLValue(psService, "Domain", "") // first two if ""

    CPLString dimension_to_band = CPLGetXMLValue(psService, "DimensionToBand", "");
    std::vector<CPLString> dimensions = Split(CPLGetXMLValue(psService, "Dimensions", ""), ";");
    // todo: check that all non-domain and not dimension_to_band dimensions are sliced to some value
    // todo: remove x/y (domain) from dimensions

    bool dimensions_are_ok = true;

    // find the dimension of dimension_to_band and return what's inside "()"
    std::vector<CPLString> dimension_to_band_trim = ParseSubset(dimensions, dimension_to_band);
    
    std::vector<CPLString> bbox = ParseBoundingBox(envelope);
    if (axes.size() < 2 || bbox.size() < 2) {
        CPLError(CE_Failure, CPLE_AppDefined, "Less than 2 dimensions in coverage envelope or no axisLabels.");
        return false;
    }
    std::vector<double> low = Flist(Split(bbox[0], " ", axis_order_swap));
    std::vector<double> high = Flist(Split(bbox[1], " ", axis_order_swap));
    std::vector<double> env;
    env.insert(env.end(), low.begin(), low.begin() + 2);
    env.insert(env.end(), high.begin(), high.begin() + 2);
    // todo: EnvelopeWithTimePeriod

    CPLXMLNode *point = CPLGetXMLNode(grid, "origin.Point");
    CPLString crs = ParseCRS(point);
    bool swap;
    if (!CRSImpliesAxisOrderSwap(crs, swap)) {
        return false;
    }

    path = "limits.GridEnvelope";
    std::vector<std::vector<int>> size = ParseGridEnvelope(CPLGetXMLNode(grid, path)); // todo: should we swap?
    std::vector<int> grid_size;
    // we need the indexes of x and y and possibly dimension_to_band

    std::vector<int> domain_index = IndexOf(axes, Split(CPLGetXMLValue(psService, "Domain", ""), ","));
    if (Contains(domain_index, -1)) {
        CPLError(CE_Failure, CPLE_AppDefined, "Axis in given domain does not exist in coverage.");
        return false;
    }
    
    if (domain_index.size() == 0) {
        // the first two
        domain_index.push_back(0);
        domain_index.push_back(1);
    }
    
    grid_size.push_back(size[1][domain_index[0]] - size[0][domain_index[0]] + 1);
    grid_size.push_back(size[1][domain_index[1]] - size[0][domain_index[1]] + 1);

    int dimension_to_band_index = IndexOf(axes, dimension_to_band); // returns -1 if dimension_to_band is ""
    // todo: can't be domain_index
    
    std::vector<std::vector<double>> offsets;
    if (!GridOffsets(grid, offsets, axes, subtype, crs, swap)) {
        return false;
    }

    SetGeometry(env, axisOrder, grid_size, offsets);

    crs = CPLGetXMLValue(psService, "CRS", "");
    if (crs != "" && crs != osCRS) {
        // the user has requested that this dataset is in different
        // crs than the server native crs
        // thus, we need to redo the geo transform too

        // todo: warp env from osCRS to crs

        if (!SetCRS(crs, false)) {
            return false;
        }

        SetGeometry(env, axisOrder, grid_size, offsets);
    }

    // todo: ElevationDomain, DimensionDomain

    char **metadata = NULL;

    for (unsigned int i = 0; i < axes.size(); ++i) {
        CPLString key;
        key.Printf("DIMENSION_%i_", i);
        metadata = CSLSetNameValue(metadata, (key + "AXIS").c_str(), axes[i]);
        metadata = CSLSetNameValue(metadata, (key + "UOM").c_str(), uoms[i]);
    }

    unsigned int bands = 0;
    if (!ParseRange(coverage, &metadata, bands)) {
        return false;
    }
    
    if (dimension_to_band != "") {
        
        // if not sliced, must drop all but the first band_name from
        // bands and that will become the data value and the (possibly
        // trimmed) dimension becomes the new bands
        
        if (dimension_to_band_trim.size() > 0) {
            if (dimension_to_band_trim[2] == "") {
                // slice
            }
            // dimension_to_band_trim[2] may be a timestamp string ...
            //bands = dimension_to_band_trim[2] - dimension_to_band_trim[1];
            // todo: deal with non-integer dimension_to_band_trim values
            // lookup from generalGridAxis.coefficients?
            // only size is needed here, i.e, nr of bands
        } else {
            // get all
            bands = size[1][dimension_to_band_index] - size[0][dimension_to_band_index] + 1;
            // todo: remove all but first from band_* vectors
        }
    }

    // todo: if not dimensions_are_ok, bands = 0
    // add PAM metadata to domain SUBDATASET_i
    CPLString subdataset = GetSubdataset(CPLGetXMLValue(psService, "CoverageName", ""));

    CPLCreateXMLElementAndValue(psService, "BandCount", CPLString().Printf("%d",bands));
    // save band information into the meta data
    // for band type we will need to wait until we get a sample
    // note: the type is now expected to be the same for all bands in this driver
    //CPLCreateXMLElementAndValue(psService, "BandType", Join(, ","));

    // Dimensions is not needed here except for testing (todo)
    
    // set the PreferredFormat value in service, unless it is set
    // by the user, either through direct edit or options
    if (!SetFormat(coverage)) {
        // all attempts to find a format have failed...
        CPLError(CE_Failure, CPLE_AppDefined, "All attempts to find a format have failed, giving up.");
        return false;
    }

    // todo: set the Interpolation, check it against InterpolationSupported
    
    this->SetMetadata(metadata, subdataset);
    CSLDestroy(metadata);
    
    bServiceDirty = TRUE;
    return true;
}
