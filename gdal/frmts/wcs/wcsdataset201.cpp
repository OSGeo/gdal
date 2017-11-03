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

CPLString WCSDataset201::GetCoverageRequest(CPL_UNUSED int nXOff, CPL_UNUSED int nYOff,
                                            CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                                            CPL_UNUSED int nBufXSize, CPL_UNUSED int nBufYSize,
                                            CPL_UNUSED std::vector<double> extent,
                                            CPLString osBandList)
{
    CPLString osRequest, osRangeSubset;

/* -------------------------------------------------------------------- */
/*      URL encode strings that could have questionable characters.     */
/* -------------------------------------------------------------------- */
    CPLString osCoverage = URLEncode(CPLGetXMLValue(psService, "CoverageName", ""));
    CPLString osFormat = URLEncode(CPLGetXMLValue(psService, "PreferredFormat", ""));

    osRangeSubset.Printf("&RangeSubset=%s",
                         CPLGetXMLValue(psService,"FieldName",""));

    if( CPLGetXMLValue( psService, "Resample", NULL ) )
    {
        osRangeSubset += ":";
        osRangeSubset += CPLGetXMLValue( psService, "Resample", "");
    }

    if( osBandList != "" )
    {
        osRangeSubset +=
            CPLString().Printf( "[%s[%s]]",
                                osBandIdentifier.c_str(),
                                osBandList.c_str() );
    }


    if (GML_IsSRSLatLongOrder(osCRS.c_str())) {
        double tmp = extent[0];
        extent[0] = extent[1];
        extent[1] = tmp;
        tmp = extent[2];
        extent[2] = extent[3];
        extent[3] = tmp;
    }

    // no bbox, only subset parameters, one per label, trim or slice
    // Note: below we accept only non-rotated grids.
    // interpolation from extension
    // http://mapserver.org/ogc/wcs_server.html

    CPLXMLNode *coverage = CPLGetXMLNode(psService, "CoverageDescription");
    CPLString path = "boundedBy.Envelope.axisLabels";
    std::vector<CPLString> labels = Split(CPLGetXMLValue(coverage, path, ""), " ");

    // construct subsets, assuming X and Y are the first two and that they are trimmed
    CPLString subsets, tmp;
    // X
    tmp.Printf("SUBSET=%s(%.15g,%.15g)", labels[0].c_str(), extent[0], extent[2]);
    subsets += tmp;
    // Y
    tmp.Printf("SUBSET=%s(%.15g,%.15g)", labels[1].c_str(), extent[1], extent[3]);
    subsets += tmp;
    // todo: the rest: slice? into bands?

    osRequest.Printf(
        "%s"
        "SERVICE=WCS&VERSION=%s"
        "&REQUEST=GetCoverage&COVERAGEID=%s"
        "&FORMAT=%s"
        "&%s",
        CPLGetXMLValue( psService, "ServiceURL", "" ),
        CPLGetXMLValue( psService, "Version", "" ),
        osCoverage.c_str(),
        osFormat.c_str(),
        subsets.c_str() );

    return osRequest;

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


/************************************************************************/
/*                          ExtractGridInfo()                           */
/*                                                                      */
/*      Collect info about grid from describe coverage for WCS 2.0.     */
/*                                                                      */
/************************************************************************/

bool WCSDataset201::ExtractGridInfo()
{
    // todo: skip this if required information is already in the service XML

    CPLXMLNode *coverage = CPLGetXMLNode(psService, "CoverageDescription");

    if( coverage == NULL )
        return false;

    CPLString subtype = CoverageSubtype(coverage);
    CPLXMLNode *grid = GetGridNode(coverage, subtype);
    if (!grid) {
        return false;
    }

    // GridFunction (is optional)
    // We support only linear grid functions.
    std::vector<CPLString> axisOrder;
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

    // get CRS from boundedBy and set the native flag to true
    // below we may set the CRS again but that won't be native
    CPLString path = "boundedBy.Envelope";
    CPLXMLNode *envelope = CPLGetXMLNode(coverage, path);
    if (!SetCRS(ParseCRS(envelope), true)) {
        return false;
    }
    path += ".axisLabels";
    std::vector<CPLString> labels = Split(CPLGetXMLValue(coverage, path, ""), " ", axis_order_swap);

    // todo: labels are the domain dimension names
    // these need to be reported to the user
    // and the user may select mappings name => x, name => y, name => bands
    // or we'll use the first two by default, and possibly make t => bands
    
    std::vector<CPLString> bbox = ParseBoundingBox(envelope);
    if (labels.size() < 2 || bbox.size() < 2) {
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
    grid_size.push_back(size[1][0] - size[0][0] + 1);
    grid_size.push_back(size[1][1] - size[0][1] + 1);
    std::vector<std::vector<double>> offsets;
    if (!GridOffsets(grid, offsets, labels, subtype, crs, swap)) {
        return false;
    }

    SetGeometry(env, axisOrder, grid_size, offsets);

    crs = CPLGetXMLValue(psService, "SRS", "");
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

    // assuming here that the attributes are defined by swe:DataRecord
    path = "rangeType";
    CPLXMLNode *record = CPLGetXMLNode(coverage, (path + ".DataRecord").c_str());
    if (!record) {
        CPLError(CE_Failure, CPLE_AppDefined, "Attributes are not defined in a DataRecord, giving up.");
        return false;
    }
    unsigned int bands = 0;
    for (CPLXMLNode *field = record->psChild; field != NULL; field = field->psNext) {
        if (field->eType != CXT_Element || !EQUAL(field->pszValue, "field")) {
            continue;
        }
        CPLString name = CPLGetXMLValue(field, "name", "");
        CPLString descr = CPLGetXMLValue(field, "Quantity.description", "");
        CPLString nodata = CPLGetXMLValue(field, "Quantity.nilValues.NilValue", "");
        if (nodata != "") {
            bServiceDirty = TRUE;
            CPLCreateXMLElementAndValue( psService, "NoDataValue", nodata.c_str());
        }
        path = "Quantity.constraint.AllowedValues.interval";
        std::vector<double> interval = Flist(Split(CPLGetXMLValue(field, path, ""), " "));
        bands += 1;
    }

    /*
    bServiceDirty = TRUE;
    if( CPLGetXMLValue(psService,"BandIdentifier",NULL) == NULL )
        CPLCreateXMLElementAndValue( psService, "BandIdentifier",
                                     osBandIdentifier );
    */

    if (bands) {
        CPLCreateXMLElementAndValue(psService, "BandCount", CPLString().Printf("%d",bands));
    }

/* -------------------------------------------------------------------- */
/*      Pick a format type if we don't already have one selected.       */
/*                                                                      */
/*      We will prefer anything that sounds like TIFF, otherwise        */
/*      falling back to the first supported format.  Should we          */
/*      consider preferring the nativeFormat if available?              */
/* -------------------------------------------------------------------- */
    CPLString native_format = CPLGetXMLValue(coverage, "ServiceParameters.nativeFormat", "");
    // format may be native or one the supported
    // the user may have set it as an option, so test it
    if (CPLGetXMLValue(psService, "PreferredFormat", NULL ) == NULL) {
        CPLCreateXMLElementAndValue(psService, "PreferredFormat", native_format);
    }

    return true;
}
