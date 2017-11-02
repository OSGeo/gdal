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
    std::vector<CPLString> labels;
    bool swap = false; // todo
    ParseList(coverage, "boundedBy.Envelope.axisLabels", labels, swap);

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
            std::vector<double> tmp_offset;
            if (!ParseDoubleList(node, "", tmp_offset, swap)) {
                return false;
            }
            offset.push_back(tmp_offset);
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
            CPLString coeffs = CPLGetXMLValue(axis, "coefficients", "");
            if (coeffs != "") {
                CPLError(CE_Failure, CPLE_AppDefined, "This is not a uniform grid.");
                return false;
            }
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
            CPLXMLNode *offset_node = CPLGetXMLNode(node, "offsetVector");
            if (offset_node) {
                CPLString crs2 = ParseCRS(node);
                if (crs2 != "" && crs2 != crs) {
                    CPLError(CE_Failure, CPLE_AppDefined, "SRS mismatch between origin and offset vector.");
                    return false;
                }
                std::vector<double> tmp_offset;
                if (!ParseDoubleList(offset_node, "", tmp_offset, swap)) {
                    return false;
                }
                offset.push_back(tmp_offset);
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

bool WCSDataset201::Offset2GeoTransform(std::vector<double> origin,
                                        std::vector<std::vector<double>> offset)
{
    for (unsigned int i = 0; i < offset.size(); i++) {
        adfGeoTransform[i*3 + 0] = origin[i];
        adfGeoTransform[i*3 + 1] = offset[i][0];
        adfGeoTransform[i*3 + 2] = offset[i][1];
    }

    // For now(?) do not accept rotated grids,
    // since we don't know how to request their subsets.
    // That makes also coverage envelope the grid envelope.
    if (adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0) {
        CPLError(CE_Failure, CPLE_AppDefined, "Can't handle rotated grids.");
        return false;
    }

    // right now adfGeoTransform[0,3] is at grid origo (center of origo cell)
    // and offset[0] is unit vector along grid x (i) axis
    // and offset[1] is unit vector along grid y (j) axis
    // if offset[0][0] > 0 and offset[1][1] > 0 then origo is left bottom
    // set adfGeoTransform[0,3] to left top corner of left top cell
    // set adfGeoTransform[1,2] to unit vector i right
    // set adfGeoTransform[4,5] to unit vector j down
    double dx = fabs((offset[0][0] - offset[1][0])/2.0);
    double dy = fabs((offset[0][1] - offset[1][1])/2.0);
    if (offset[0][0] >= 0) {
        if (offset[1][1] >= 0) { // left bottom
            adfGeoTransform[0] -= dx;
            adfGeoTransform[3] += ((double)nRasterYSize - 0.5) * dy;
            // i vector is ok, invert j
            adfGeoTransform[3 + 1] = -offset[1][0];
            adfGeoTransform[3 + 2] = -offset[1][1];
        } else { // left top
            adfGeoTransform[0] -= dx;
            adfGeoTransform[3] += dy;
            // i and j are ok
        }
    } else {
        if (offset[1][1] >= 0) { // right bottom
            adfGeoTransform[0] -= ((double)nRasterXSize - 0.5) * dx;
            adfGeoTransform[3] += ((double)nRasterYSize - 0.5) * dy;
            // invert i and j
            adfGeoTransform[1] = -offset[0][0];
            adfGeoTransform[2] = -offset[0][1];
            adfGeoTransform[3 + 1] = -offset[1][0];
            adfGeoTransform[3 + 2] = -offset[1][1];
        } else { // right top
            adfGeoTransform[0] -= ((double)nRasterXSize - 0.5) * dx;
            adfGeoTransform[3] += dy;
            // invert i, j is ok
            adfGeoTransform[1] = -offset[0][0];
            adfGeoTransform[2] = -offset[0][1];
        }
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
    CPLXMLNode *coverage = CPLGetXMLNode(psService, "CoverageDescription");

    if( coverage == NULL )
        return false;

    CPLString path = "domainSet";
    CPLString subtype = CoverageSubtype(coverage);
    CPLXMLNode *grid = GetGridNode(coverage, subtype);
    if (!grid) {
        return false;
    }

    // GridFunction (is optional)
    CPLXMLNode *function = CPLGetXMLNode(psService, "coverageFunction.GridFunction");
    if (function) {
        std::vector<CPLString> axisOrder;
        std::vector<CPLString> startPoint;
        path = "sequenceRule.axisOrder";
        if (ParseList(function, path, axisOrder)) {
            CPLString sequenceRule = CPLGetXMLValue(coverage, (path + ".sequenceRule").c_str(), "");
            ParseList(coverage, path + ".startPoint", startPoint);
            // for now require simple
            if (!(sequenceRule == "Linear"
                  && axisOrder[0] == "+1" && axisOrder[1] == "+2"
                  && startPoint[0] == "0" && startPoint[1] == "0"))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "The grid is not linear and increasing from origo.");
                return false;
            }
        }
    }

    // get native CRS from boundedBy, geo transform will be from domainSet

    osCRS = ParseCRS(CPLGetXMLNode(coverage, "boundedBy.Envelope")); // todo: EnvelopeWithTimePeriod
    std::vector<CPLString> labels;
    if (!ParseList(coverage, "boundedBy.Envelope.axisLabels", labels)) {
        return false;
    }

    // work on grid, raster size first
    // could there be a case of domainSet given in different CRS than boundedBy?
    // or, worse, mixed CRS in domainSet?

    CPLXMLNode *point = CPLGetXMLNode(grid, "origin.Point");
    CPLString crs = ParseCRS(point);

    // todo: determine whether axis order need to be swapped
    bool swap = false;

    std::vector<int> sizes;

    std::vector<double> origin;

    if (!ParseGridEnvelope(grid, "limits.GridEnvelope", sizes)
        || !ParseDoubleList(point, "pos", origin, swap))
    {
        return false;
    }
    unsigned int dim = sizes.size() / 2;
    if (labels.size() != dim || origin.size() != dim) {
        CPLError(CE_Failure, CPLE_AppDefined, "Dimension mismatch in grid labels or origin.");
        return false;
    }

    // for now handle only grids with origin 0,0
    if (sizes[0] != 0 || sizes[1] != 0) {
        CPLError(CE_Failure, CPLE_AppDefined, "Can't handle GridEnvelope having low not at origo.");
        return false;
    }
    nRasterXSize = sizes[dim] - sizes[0] + 1;
    nRasterYSize = sizes[dim+1] - sizes[1] + 1;

    // the geo transform
    // only rectilinear uniform grids are ok

    std::vector<std::vector<double>> offset; // grid unit vectors
    if (!GridOffsets(grid, offset, labels, subtype, crs, swap)) {
        return false;
    }
    if (!Offset2GeoTransform(origin, offset)) {
        return false;
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
        CPLXMLNode *interval_node = CPLGetXMLNode(field, "Quantity.constraint.AllowedValues.interval");
        std::vector<double> interval;
        if (interval_node) {
            ParseDoubleList(interval_node, "", interval, false);
        }
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
/*      Do we have a coordinate system override?                        */
/* -------------------------------------------------------------------- */
    // SRS may be one of the crsSupported
    // the user may have set it as an option, so test it
    //const char *pszProjOverride = CPLGetXMLValue( psService, "SRS", NULL );
    // if it is, set osCRS to it

    // if osCRS is empty, use the one from domainSet
    if (osCRS.empty()) {
        osCRS = crs;
    }

    // set projection from osCRS
    if (!CRS2Projection(osCRS, &pszProjection)) {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not interpret '%s' as CRS.", osCRS.c_str());
        return false;
    }

    // note, projection may be NULL for raw images

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
