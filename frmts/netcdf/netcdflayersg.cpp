/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Winor Chen <wchen329 at wisc.edu>
 *
 ******************************************************************************
 * Copyright (c) 2019, Winor Chen <wchen329 at wisc.edu>
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
#include "netcdfsg.h"
#include "netcdfdataset.h"
#include "ogr_core.h"

namespace nccfdriver
{
    OGRwkbGeometryType RawToOGR(geom_t type, int axis_count)
    {
        OGRwkbGeometryType ret = wkbNone;

        switch(type)
        {
            case NONE:
                break;
            case LINE:
                ret = axis_count == 2 ? wkbLineString :
                      axis_count == 3 ? wkbLineString25D: wkbNone;
                break;
            case MULTILINE:
                ret = axis_count == 2 ? wkbMultiLineString :
                      axis_count == 3 ? wkbMultiLineString25D : wkbNone;
                break;
            case POLYGON:
                ret = axis_count == 2 ? wkbPolygon :
                      axis_count == 3 ? wkbPolygon25D : wkbNone;
                break;
            case MULTIPOLYGON:
                ret = axis_count == 2 ? wkbMultiPolygon :
                      axis_count == 3 ? wkbMultiPolygon25D : wkbNone;
                break;
            case POINT:
                ret = axis_count == 2 ? wkbPoint :
                      axis_count == 3 ? wkbPoint25D: wkbNone;
                break;
            case MULTIPOINT:
                ret = axis_count == 2 ? wkbMultiPoint :
                      axis_count == 3 ? wkbMultiPoint25D : wkbNone;
                break;
            case UNSUPPORTED:
                break;
        }

        return ret;
    }

    geom_t OGRtoRaw(OGRwkbGeometryType type)
    {
        geom_t ret = NONE;
        auto eFlatType = wkbFlatten(type);

        if (eFlatType == wkbPoint)
        {
            ret = POINT;
        }

        else if (eFlatType == wkbLineString)
        {
            ret = LINE;
        }

        else if(eFlatType == wkbPolygon)
        {
            ret = POLYGON;
        }

        else if (eFlatType == wkbMultiPoint)
        {
            ret = MULTIPOINT;
        }

        else if (eFlatType == wkbMultiLineString)
        {
            ret = MULTILINE;
        }

        else if (eFlatType == wkbMultiPolygon)
        {
            ret = MULTIPOLYGON;
        }

        // if the feature type isn't NONE potentially give a warning about measures
        if(ret != NONE && wkbHasM(type))
        {
            CPLError(CE_Warning, CPLE_NotSupported, "A partially supported measured feature type was detected. X, Y, Z Geometry will be preserved but the measure axis and related information will be removed.");
        }

        return ret;
    }

    bool OGRHasZandSupported(OGRwkbGeometryType type)
    {
        return type == wkbPoint25D || type == wkbLineString25D || type == wkbPolygon25D ||
            type == wkbMultiPoint25D || type == wkbMultiLineString25D || type == wkbMultiPolygon25D;
    }

}

CPLErr netCDFDataset::DetectAndFillSGLayers(int ncid)
{
    // Discover simple geometry variables
    int var_count;
    nc_inq_nvars(ncid, &var_count);
    std::set<int> vidList;

    nccfdriver::scanForGeometryContainers(ncid, vidList);

    if(!vidList.empty())
    {
        for(auto vid: vidList)
        {
            try
            {
                LoadSGVarIntoLayer(ncid, vid);
            }

            catch(nccfdriver::SG_Exception& e)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Translation of a simple geometry layer has been terminated prematurely due to an error.\n%s", e.get_err_msg());
            }
        }
    }

    return CE_None;
}

CPLErr netCDFDataset::LoadSGVarIntoLayer(int ncid, int nc_basevarId)
{
    std::shared_ptr<nccfdriver::SGeometry_Reader> sg (new nccfdriver::SGeometry_Reader(ncid, nc_basevarId));
    int cont_id = sg->getContainerId();
    nccfdriver::SGeometry_PropertyScanner pr(ncid, cont_id);
    OGRwkbGeometryType owgt = nccfdriver::RawToOGR(sg->getGeometryType(), sg->get_axisCount());

    std::string return_gm = "";

    if(sg->getGridMappingVarID() != nccfdriver::INVALID_VAR_ID)
        SetProjectionFromVar(ncid, nc_basevarId, true, sg->getGridMappingName().c_str(), &return_gm, sg.get());

    // Geometry Type invalid, avoid further processing
    if(owgt == wkbNone)
    {
        throw nccfdriver::SG_Exception_BadFeature();
    }

    char baseName[NC_MAX_CHAR + 1];
    memset(baseName, 0, NC_MAX_CHAR + 1);
    nc_inq_varname(ncid, nc_basevarId, baseName);

    OGRSpatialReference * poSRS = nullptr;
    if(return_gm != "")
    {
        poSRS = new OGRSpatialReference();
        if(poSRS->importFromWkt(return_gm.c_str()) != OGRERR_NONE)
        {
            delete poSRS;
            throw nccfdriver::SG_Exception_General_Malformed("SRS settings");
        }

        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    std::shared_ptr<netCDFLayer> poL(new netCDFLayer(this, ncid, baseName, owgt, poSRS));

    if(poSRS != nullptr)
    {
        poSRS -> Release();
    }

    poL->EnableSGBypass();
    OGRFeatureDefn * defn = poL->GetLayerDefn();
    defn->SetGeomType(owgt);

    // Add properties
    std::vector<int> props = pr.ids();
    for(size_t itr = 0; itr < props.size(); itr++)
    {
        poL->AddField(props[itr]);
    }

    // Set simple geometry object
    poL->SetSGeometryRepresentation(sg);

    // Create layer
    papoLayers.push_back(poL);

    return CE_None;
}

/* Creates and fills any needed variables that haven't already been created
 */
void netCDFDataset::SGCommitPendingTransaction()
{
    try
    {
        if(bSGSupport)
        {
            // Go through all the layers and resize dimensions accordingly
            for(size_t layerInd = 0; layerInd < papoLayers.size(); layerInd++)
            {
                auto poLayer = dynamic_cast<netCDFLayer*>(papoLayers[layerInd].get());
                if( !poLayer )
                    continue;
                nccfdriver::ncLayer_SG_Metadata& layerMD = poLayer->getLayerSGMetadata();
                nccfdriver::geom_t wType = layerMD.getWritableType();

                // Resize node coordinates
                int ncoord_did = layerMD.get_node_coord_dimID();
                if(ncoord_did != nccfdriver::INVALID_DIM_ID)
                {
                    vcdf.nc_resize_vdim(ncoord_did, layerMD.get_next_write_pos_node_coord());
                }

                // Resize node count (for all except POINT)
                if(wType != nccfdriver::POINT)
                {
                    int ncount_did = layerMD.get_node_count_dimID();
                    if(ncount_did != nccfdriver::INVALID_DIM_ID)
                    {
                        vcdf.nc_resize_vdim(ncount_did, layerMD.get_next_write_pos_node_count());
                    }
                }

                // Resize part node count (for MULTILINE, POLYGON, MULTIPOLYGON)
                if(wType == nccfdriver::MULTILINE || wType == nccfdriver::POLYGON || wType == nccfdriver::MULTIPOLYGON)
                {
                    int pnc_did = layerMD.get_pnc_dimID();
                    if(pnc_did != nccfdriver::INVALID_DIM_ID)
                    {
                        vcdf.nc_resize_vdim(pnc_did, layerMD.get_next_write_pos_pnc());
                    }
                }

                 nccfdriver::geom_t geometry_type = layerMD.getWritableType();

               /* Delete interior ring stuff if not detected
                */

                if (!layerMD.getInteriorRingDetected() && (geometry_type == nccfdriver::MULTIPOLYGON || geometry_type == nccfdriver::POLYGON) &&
                    layerMD.get_containerRealID() != nccfdriver::INVALID_VAR_ID)
                {
                    SetDefineMode(true);

                    int err_code = nc_del_att(cdfid, layerMD.get_containerRealID(), CF_SG_INTERIOR_RING);
                    NCDF_ERR(err_code);
                    if(err_code != NC_NOERR)
                    {
                        std::string frmt = std::string("attribute: ") + std::string(CF_SG_INTERIOR_RING);
                        throw nccfdriver::SGWriter_Exception_NCDelFailure(layerMD.get_containerName().c_str(), frmt.c_str());
                    }

                    // Invalidate variable writes as well - Interior Ring
                    vcdf.nc_del_vvar(layerMD.get_intring_varID());

                    if(geometry_type == nccfdriver::POLYGON)
                    {
                        err_code = nc_del_att(cdfid, layerMD.get_containerRealID(), CF_SG_PART_NODE_COUNT);
                        NCDF_ERR(err_code);
                        if(err_code != NC_NOERR)
                        {
                            std::string frmt = std::string("attribute: ") + std::string(CF_SG_PART_NODE_COUNT);
                            throw nccfdriver::SGWriter_Exception_NCDelFailure(layerMD.get_containerName().c_str(), frmt.c_str());
                        }

                        // Invalidate variable writes as well - Part Node Count
                        vcdf.nc_del_vvar(layerMD.get_pnc_varID());

                        // Invalidate dimension as well - Part Node Count
                        vcdf.nc_del_vdim(layerMD.get_pnc_dimID());
                    }

                    SetDefineMode(false);
                }
            }

            vcdf.nc_vmap();
            this->FieldScribe.commit_transaction();
            this->GeometryScribe.commit_transaction();
        }
    }

    catch(nccfdriver::SG_Exception& sge)
    {
        CPLError(CE_Fatal, CPLE_FileIO, "An error occurred while writing the target netCDF File. Translation will be terminated.\n%s", sge.get_err_msg());
    }
}

void netCDFDataset::SGLogPendingTransaction()
{
    GeometryScribe.log_transaction();
    FieldScribe.log_transaction();
}

/* Takes an index and using the layer geometry builds the equivalent
 * OGRFeature.
 */
OGRFeature* netCDFLayer::buildSGeometryFeature(size_t featureInd)
{
    OGRGeometry * geometry;

    switch(m_simpleGeometryReader->getGeometryType())
    {
        case nccfdriver::POINT:
            geometry = new OGRPoint;
            break;
        case nccfdriver::LINE:
            geometry = new OGRLineString;
            break;
        case nccfdriver::POLYGON:
            geometry = new OGRPolygon;
            break;
        case nccfdriver::MULTIPOINT:
            geometry = new OGRMultiPoint;
            break;
        case nccfdriver::MULTILINE:
            geometry = new OGRMultiLineString;
            break;
        case nccfdriver::MULTIPOLYGON:
            geometry = new OGRMultiPolygon;
            break;
        default:
            throw nccfdriver::SG_Exception_BadFeature();
            break;
    }

    const auto wkb = m_simpleGeometryReader->serializeToWKB(featureInd);
    geometry->importFromWkb(wkb.data(), wkb.size(), wkbVariantIso);
    geometry->assignSpatialReference(this->GetSpatialRef());

    OGRFeatureDefn* defn = this->GetLayerDefn();
    OGRFeature * feat = new OGRFeature(defn);
    feat -> SetGeometryDirectly(geometry);

    int dimId = m_simpleGeometryReader->getInstDim();

    this->FillFeatureFromVar(feat, dimId, featureInd);

    feat -> SetFID(featureInd);
    return feat;
}

std::string netCDFDataset::generateLogName()
{
    return std::string(CPLGenerateTempFilename(nullptr));
}
