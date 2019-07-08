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
#include "netcdfsgwriterutil.h"
#include "netcdfdataset.h"

namespace nccfdriver
{
    SGeometry_Feature::SGeometry_Feature(OGRFeature& ft)
    {
        this->hasInteriorRing = false;
        OGRGeometry * defnGeometry = ft.GetGeometryRef();

        if (defnGeometry == nullptr)
        {
            throw SGWriter_Exception_EmptyGeometry();
        }

        OGRGeometry& r_defnGeometry = *defnGeometry;

        OGRwkbGeometryType ogwkt = defnGeometry->getGeometryType();
        this->type = OGRtoRaw(ogwkt);


        if (this->type == MULTIPOINT)
        {
            OGRMultiPoint& r_defnGeometryMP = dynamic_cast<OGRMultiPoint&>(r_defnGeometry);

            // Set total node count
            this->total_point_count = r_defnGeometryMP.getNumGeometries();

            // The amount of nodes is also the amount of parts
            for(size_t pc = 0; pc < total_point_count; pc++)
            {
                ppart_node_count.push_back(1);
            }

            // total part count ==  total node count
            this->total_part_count = this->total_point_count;

        }

        else if (this->type == LINE)
        {
            OGRLineString& r_defnGeometryLine = dynamic_cast<OGRLineString&>(r_defnGeometry);
            // to do: check for std::bad_cast somewhere?

            // Get node count
            this->total_point_count = r_defnGeometryLine.getNumPoints();

            // Single line: 1 part count == node count
            this->ppart_node_count.push_back(r_defnGeometryLine.getNumPoints());

            // One part
            this->total_part_count = 1;
        }

        else if(this->type == MULTILINE)
        {
            OGRMultiLineString& r_defnMLS = dynamic_cast<OGRMultiLineString&>(r_defnGeometry);
            this->total_point_count = 0;
            this->total_part_count = r_defnMLS.getNumGeometries();

            // Take each geometry, just add up the corresponding parts

            for(int itrLScount = 0; itrLScount < r_defnMLS.getNumGeometries(); itrLScount++)
            {
                OGRLineString & r_LS = dynamic_cast<OGRLineString&>(*r_defnMLS.getGeometryRef(itrLScount));
                int pt_count = r_LS.getNumPoints();

                this->ppart_node_count.push_back(pt_count);
                this->total_point_count += pt_count;
            }
        }

        else if(this->type == POLYGON)
        {
            OGRPolygon& r_defnPolygon = dynamic_cast<OGRPolygon&>(r_defnGeometry);

            this->total_point_count = 0;
            this->total_part_count = 0;

            // Get node count
            // First count exterior ring
            if(r_defnPolygon.getExteriorRing() == nullptr)
            {
                throw SGWriter_Exception_EmptyGeometry();
            }

            OGRLinearRing & exterior_ring = *r_defnPolygon.getExteriorRing();

            size_t outer_ring_ct = exterior_ring.getNumPoints();

            this->total_point_count += outer_ring_ct;
            this->ppart_node_count.push_back(outer_ring_ct);
            this->total_part_count++;

            // Then count all from the interior rings
            // While doing the following
            // Get per part node count (per part RING count)
            // Get part count (in this case it's the amount of RINGS)

            for(int iRingCt = 0; iRingCt < r_defnPolygon.getNumInteriorRings(); iRingCt++)
            {
                this->hasInteriorRing = true;
                if(r_defnPolygon.getInteriorRing(iRingCt) == nullptr)
                {
                    throw SGWriter_Exception_RingOOB();
                }

                OGRLinearRing & iring = *r_defnPolygon.getInteriorRing(iRingCt);
                this->total_point_count += iring.getNumPoints();
                this->ppart_node_count.push_back(iring.getNumPoints());
                this->total_part_count++;
            }
        }

        else if(this->type == MULTIPOLYGON)
        {
            OGRMultiPolygon& r_defnMPolygon = dynamic_cast<OGRMultiPolygon&>(r_defnGeometry);

            this->total_point_count = 0;
            this->total_part_count = 0;

            for(int itr = 0; itr < r_defnMPolygon.getNumGeometries(); itr++)
            {
                OGRPolygon & r_Pgon = dynamic_cast<OGRPolygon&>(*r_defnMPolygon.getGeometryRef(itr));

                if(r_Pgon.getExteriorRing() == nullptr)
                {
                    throw SGWriter_Exception_EmptyGeometry();
                }

                OGRLinearRing & exterior_ring = *r_Pgon.getExteriorRing();

                size_t outer_ring_ct = exterior_ring.getNumPoints();

                this->total_point_count += outer_ring_ct;
                this->ppart_node_count.push_back(outer_ring_ct);
                this->total_part_count++;
                this->part_at_ind_interior.push_back(false);

                // Then count all from the interior rings
                // While doing the following
                // Get per part node count (per part RING count)
                // Get part count (in this case it's the amount of RINGS)

                for(int iRingCt = 0; iRingCt < r_Pgon.getNumInteriorRings(); iRingCt++)
                {
                    if(r_Pgon.getInteriorRing(iRingCt) == nullptr)
                    {
                        throw SGWriter_Exception_RingOOB();
                    }

                    OGRLinearRing & iring = *r_Pgon.getInteriorRing(iRingCt);
                    this->hasInteriorRing = true;
                    this->total_point_count += iring.getNumPoints();
                    this->ppart_node_count.push_back(iring.getNumPoints());
                    this->total_part_count++;
                    this->part_at_ind_interior.push_back(true);
                }

            }

        }

        else
        {
            throw SG_Exception_BadFeature();
        }

        this->geometry_ref = ft.GetGeometryRef();
    }

    OGRPoint& SGeometry_Feature::getPoint(size_t part_no, int point_index)
    {
        if (this->type == MULTIPOINT)
        {
            OGRMultiPoint* as_mp_ref = dynamic_cast<OGRMultiPoint*>(this->geometry_ref);
            int part_ind = static_cast<int>(part_no);
            OGRPoint * pt = dynamic_cast<OGRPoint*>(as_mp_ref->getGeometryRef(part_ind));
            return *pt;
        }

        if (this->type == LINE)
        {
            OGRLineString* as_line_ref = dynamic_cast<OGRLineString*>(this->geometry_ref);
            as_line_ref->getPoint(point_index, &pt_buffer);
        }

        if (this->type == MULTILINE)
        {
            OGRMultiLineString* as_mline_ref = dynamic_cast<OGRMultiLineString*>(this->geometry_ref);
            int part_ind = static_cast<int>(part_no);
            OGRLineString* lstring = dynamic_cast<OGRLineString*>(as_mline_ref->getGeometryRef(part_ind));
            lstring->getPoint(point_index, &pt_buffer);
        }

        if (this->type == POLYGON)
        {
            OGRPolygon* as_polygon_ref = dynamic_cast<OGRPolygon*>(this->geometry_ref);
            int ring_ind = static_cast<int>(part_no);

            if(part_no == 0)
            {
                as_polygon_ref->getExteriorRing()->getPoint(point_index, &pt_buffer);
            }

            else
            {
                as_polygon_ref->getInteriorRing(ring_ind - 1)->getPoint(point_index, &pt_buffer);
            }
        }

        if (this->type == MULTIPOLYGON)
        {
            OGRMultiPolygon* as_mpolygon_ref = dynamic_cast<OGRMultiPolygon*>(this->geometry_ref);

            int polygon_num = 0;
            int ring_number = 0;
            int pno_itr = static_cast<int>(part_no);

            // Find the right polygon, and the right ring number
            for(int pind = 0; pind < as_mpolygon_ref->getNumGeometries(); pind++)
            {
                OGRPolygon * itr_poly = dynamic_cast<OGRPolygon*>(as_mpolygon_ref->getGeometryRef(pind));
                if(pno_itr < (itr_poly->getNumInteriorRings() + 1)) // + 1 is counting the EXTERIOR ring
                {
                    ring_number = static_cast<int>(pno_itr);
                    break;
                }

                else
                {
                    pno_itr -= (itr_poly->getNumInteriorRings() + 1);
                    polygon_num++;
                }
            }

            OGRPolygon* key_polygon = dynamic_cast<OGRPolygon*>(as_mpolygon_ref->getGeometryRef(polygon_num));

            if(ring_number == 0)
            {
                key_polygon->getExteriorRing()->getPoint(point_index, &pt_buffer);
            }

            else
            {
                key_polygon->getInteriorRing(ring_number - 1)->getPoint(point_index, &pt_buffer);
            }
        }

        return pt_buffer;
    }

    void OGR_SGeometry_Scribe::writeSGeometryFeature(SGeometry_Feature& ft)
    {
        if (ft.getType() == NONE)
        {
            throw SG_Exception_BadFeature();
        }

        if(ft.getType() != this->writableType)
        {
            CPLError(CE_Warning, CPLE_NotSupported, "An attempt was made to write a feature to a layer whose geometry types do not match. This is not supported and so the feature has been skipped.");
            return;
        }

        // Write each point from each part in node coordinates
        for(size_t part_no = 0; part_no < ft.getTotalPartCount(); part_no++)
        {
            if(this->writableType == POLYGON || this->writableType == MULTIPOLYGON)
            {
                bool interior_ring_fl = false;

                if(this->writableType == POLYGON)
                    interior_ring_fl = part_no == 0 ? false : true;
                if(this->writableType == MULTIPOLYGON)
                {
                    if(ft.IsPartAtIndInteriorRing(part_no))
                    {
                        interior_ring_fl = true;
                    }
                    else
                    {
                        interior_ring_fl = false;
                    }
                }

                if(interior_ring_fl == false)
                {
                    if(this->interiorRingDetected)
                        wbuf.addIRing(false);
                }
                else
                {
                    /* Take corrective measures if Interior Ring hasn't yet been detected.
                     * If Polygon: define interior_ring and part_node_count variables in in file
                     * If Multipolygon: define interior_ring variable, zero fill in file
                     */
                    if(!this->interiorRingDetected)
                    {
                        this->interiorRingDetected = true;
                        if(this->writableType == POLYGON)
                        {
                            redef_pnc();
                            wbuf.cpyNCOUNT_into_PNC();
                        }

                        redef_interior_ring();
                        while(wbuf.getPNCCount() != wbuf.getIRingVarEntryCount())
                        {
                            wbuf.addIRing(false);
                        }
                    }

                    wbuf.addIRing(true);
                }
            }

            if(this->writableType == POLYGON || this->writableType == MULTILINE || this->writableType == MULTIPOLYGON)
            {
                int pnc_writable = static_cast<int>(ft.getPerPartNodeCount()[part_no]);
                wbuf.addPNC(pnc_writable);
            }

            for(size_t pt_ind = 0; pt_ind < ft.getPerPartNodeCount()[part_no]; pt_ind++)
            {
                int pt_ind_int = static_cast<int>(pt_ind);
                OGRPoint& write_pt = ft.getPoint(part_no, pt_ind_int);


                // Write each node coordinate
                double x = write_pt.getX();
                wbuf.addXCoord(x);

                double y = write_pt.getY();
                wbuf.addYCoord(y);

                if(this->node_coordinates_varIDs.size() > 2)
                {
                    double z = write_pt.getZ();
                    wbuf.addZCoord(z);
                }
            }
        }

        // Polygons use a "predictive approach", i.e. predict that there will be interior rings
        // If there are after all, no interior rings then flush out the PNC buffer
        if(!this->interiorRingDetected && this->writableType == POLYGON)
        {
            wbuf.flushPNCBuffer();
        }

        // Append node counts from the end
        int ncount_add = static_cast<int>(ft.getTotalNodeCount());
        this->wbuf.addNCOUNT(ncount_add);
    }

    /* Writes buffer contents to the netCDF File
     * Flushes buffer,
     * And sets new iterators accordingly
     * If the transaction is empty, then this function does nothing
     */
    void OGR_SGeometry_Scribe::commit_transaction()
    {
        int err_code;

        // Append from the end
        while(!wbuf.NCOUNTEmpty())
        {
            int ncount = wbuf.NCOUNTDequeue();
            err_code = nc_put_var1_int(ncID, node_count_varID, &next_write_pos_node_count, &ncount);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_NODE_COUNT, "integer datum");
            }

            next_write_pos_node_count++;
        }

        while(!wbuf.PNCEmpty())
        {
            // Write each point from each part in node coordinates
            if(( this->writableType == POLYGON && this->interiorRingDetected ) || this->writableType == MULTILINE || this->writableType == MULTIPOLYGON)
            {
                // if interior rings is present, go write part node counts and interior ring info
                if( (this->writableType == POLYGON || this->writableType == MULTIPOLYGON) && this->interiorRingDetected )
                {
                    int interior_ring_w = wbuf.IRingDequeue() ? 1 : 0;

                    err_code = nc_put_var1_int(ncID, intring_varID, &next_write_pos_pnc, &interior_ring_w);
                    NCDF_ERR(err_code);
                    if (err_code != NC_NOERR)
                    {
                        throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_INTERIOR_RING, "integer datum");
                    }
                }

                int pnc_writable = wbuf.PNCDequeue();
                err_code = nc_put_var1_int(ncID, pnc_varID, &next_write_pos_pnc, &pnc_writable);
                NCDF_ERR(err_code);
                if (err_code != NC_NOERR)
                {
                    throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_PART_NODE_COUNT, "integer datum");
                }

                next_write_pos_pnc++;
            }
        }

        while(!wbuf.XCoordEmpty() && !wbuf.YCoordEmpty())
        {
            // Write each node coordinate
            double x = wbuf.XCoordDequeue();
            err_code = nc_put_var1_double(ncID, node_coordinates_varIDs[0], &next_write_pos_node_coord, &x);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "x double-precision datum");
            }

            double y = wbuf.YCoordDequeue();
            err_code = nc_put_var1_double(ncID, node_coordinates_varIDs[1], &next_write_pos_node_coord, &y);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "y double-precision datum");
            }

            if(this->node_coordinates_varIDs.size() > 2)
            {
                double z = wbuf.ZCoordDequeue();
                err_code = nc_put_var1_double(ncID, node_coordinates_varIDs[2], &next_write_pos_node_coord, &z);
                if (err_code != NC_NOERR)
                {
                    throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "z double-precision datum");
                }
            }

            // Step the position
            this->next_write_pos_node_coord++;
        }
    }

    OGR_SGeometry_Scribe::OGR_SGeometry_Scribe(int ncID_in, int container_varID_in, geom_t geot, unsigned long long bufsize)
        : ncID(ncID_in),
        writableType(geot),
        containerVarID(container_varID_in),
        interiorRingDetected(false),
        next_write_pos_node_coord(0),
        next_write_pos_node_count(0),
        next_write_pos_pnc(0)
    {
        char container_name[NC_MAX_CHAR + 1] = {0};

        // Set buffer size
        // Allow 4KB (standard page size) to be smallest buffer heuristic
        if(bufsize >= 4096)
        {
            wbuf.setBufSize(bufsize);
        }

        // Prepare variable names
        char node_coord_names[NC_MAX_CHAR + 1] = {0};
        char node_count_name[NC_MAX_CHAR + 1] = {0};

        // Set default values
        pnc_varID = INVALID_VAR_ID;
        pnc_dimID = INVALID_DIM_ID;
        intring_varID = INVALID_VAR_ID;

        int err_code;
        err_code = nc_inq_varname(ncID, containerVarID, container_name);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCInqFailure("new layer", "geometry container", "varName");
        }

        this->containerVarName = std::string(container_name);

        err_code = nc_get_att_text(ncID, containerVarID, CF_SG_NODE_COORDINATES, node_coord_names);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCInqFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "varName");
        }

        err_code = nc_get_att_text(ncID, containerVarID, CF_SG_NODE_COUNT, node_count_name);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCInqFailure(containerVarName.c_str(), CF_SG_NODE_COUNT, "varName");
        }


        // Make dimensions for each of these
        err_code = nc_def_dim(ncID_in, node_count_name, 1, &node_count_dimID);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_NODE_COUNT, "dimension for");
        }
        err_code = nc_def_dim(ncID_in, container_name, 1, &node_coordinates_dimID);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "dimension for");
        }

        // Define variables for each of those
        err_code = nc_def_var(ncID_in, node_count_name, NC_INT, 1, &node_count_dimID, &node_count_varID);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCInqFailure(containerVarName.c_str(), CF_SG_NODE_COUNT, "variable");
        }

        // Do the same for part node count, if it exists
        char pnc_name[NC_MAX_CHAR + 1] = {0};
        err_code = nc_get_att_text(ncID, containerVarID, CF_SG_PART_NODE_COUNT, pnc_name);
        if(err_code == NC_NOERR)
        {
            err_code = nc_def_dim(ncID_in, pnc_name, 1, &pnc_dimID);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_PART_NODE_COUNT, "dimension for");
            }

            err_code = nc_def_var(ncID_in, pnc_name, NC_INT, 1, &pnc_dimID, &pnc_varID);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_PART_NODE_COUNT, "variable");
            }
        }

        // Node coordinates
        int new_varID;
        CPLStringList aosNcoord(CSLTokenizeString2(node_coord_names, " ", 0));

        if(aosNcoord.size() < 2)
            throw SGWriter_Exception();

        // first it's X
        err_code = nc_def_var(ncID, aosNcoord[0], NC_DOUBLE, 1, &node_coordinates_dimID, &new_varID);

        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "variable(s)");
        }

        err_code = nc_put_att_text(ncID, new_varID, CF_AXIS, strlen(CF_SG_X_AXIS), CF_SG_X_AXIS);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "X axis attribute");
        }

        this->node_coordinates_varIDs.push_back(new_varID);

        // second it's Y
        err_code = nc_def_var(ncID, aosNcoord[1], NC_DOUBLE, 1, &node_coordinates_dimID, &new_varID);

        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "variable(s)");
        }

        err_code = nc_put_att_text(ncID, new_varID, CF_AXIS, strlen(CF_SG_Y_AXIS), CF_SG_Y_AXIS);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "Y axis attribute");
        }

        this->node_coordinates_varIDs.push_back(new_varID);

        // (and perhaps) third it's Z
        if(aosNcoord.size() > 2)
        {
            err_code = nc_def_var(ncID, aosNcoord[2], NC_DOUBLE, 1, &node_coordinates_dimID, &new_varID);

            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "variable(s)");
            }

            err_code = nc_put_att_text(ncID, new_varID, CF_AXIS, strlen(CF_SG_Z_AXIS), CF_SG_Z_AXIS);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "Z axis attribute");
            }
        
            this->node_coordinates_varIDs.push_back(new_varID);
        }
    }

    void OGR_SGeometry_Scribe::redef_interior_ring()
    {
        int err_code;
        const char * container_name = containerVarName.c_str();

        err_code = nc_redef(ncID);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure("netCDF file", "switch to define mode", "file");
        }

        std::string int_ring = std::string(container_name) + std::string("_interior_ring");

        // Put the new interior ring attribute
        err_code = nc_put_att_text(ncID, containerVarID, CF_SG_INTERIOR_RING, strlen(int_ring.c_str()), int_ring.c_str());
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_INTERIOR_RING, "attribute");
        }

        size_t pnc_dim_len = 0;

        err_code = nc_inq_dimlen(ncID, pnc_dimID, &pnc_dim_len);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCInqFailure(containerVarName.c_str(), CF_SG_PART_NODE_COUNT, "dim (part node count must be defined/redefined first)");
        }

        // Define the new variable
        err_code = nc_def_var(ncID, int_ring.c_str(), NC_INT, 1, &pnc_dimID, &intring_varID);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_INTERIOR_RING, "variable");
        }

        err_code = nc_enddef(ncID);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure("netCDF file", "switch to data mode", "file");
        }

        const int zero_fill = 0;

        // Zero fill interior ring
        for(size_t itr = 0; itr < pnc_dim_len; itr++)
        {
            err_code = nc_put_var1_int(ncID, intring_varID, &itr, &zero_fill);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_INTERIOR_RING, "integer datum");
            }
        }
    }

    void OGR_SGeometry_Scribe::redef_pnc()
    {
        int err_code;
        const char* container_name = containerVarName.c_str();

        err_code = nc_redef(ncID);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure("netCDF file", "switch to define mode", "file");
        }

        std::string pnc_name = std::string(container_name) + std::string("_part_node_count");

        // Put the new part node count attribute
        err_code = nc_put_att_text(ncID, containerVarID, CF_SG_PART_NODE_COUNT, strlen(pnc_name.c_str()), pnc_name.c_str());
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_PART_NODE_COUNT, "attribute");
        }

        // If the PNC dim doesn't exist, define it

        size_t pnc_dim_len = 0;

        if(pnc_dimID == INVALID_DIM_ID)
        {
            // Initialize it with size of node_count Dim
            size_t ncount_len;
            err_code = nc_inq_dimlen(ncID, node_count_dimID, &ncount_len);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCInqFailure(containerVarName.c_str(), CF_SG_NODE_COUNT, "dimension");
            }
            err_code = nc_def_dim(ncID, pnc_name.c_str(), ncount_len, &pnc_dimID);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_PART_NODE_COUNT, "dimension");
            }

            this->next_write_pos_pnc = this->next_write_pos_node_count;
        }

        err_code = nc_inq_dimlen(ncID, pnc_dimID, &pnc_dim_len);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCInqFailure(containerVarName.c_str(), CF_SG_PART_NODE_COUNT, "dim");
        }

        // Define the new variable
        err_code = nc_def_var(ncID, pnc_name.c_str(), NC_INT, 1, &pnc_dimID, &pnc_varID);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure(containerVarName.c_str(), CF_SG_PART_NODE_COUNT, "variable");
        }

        err_code = nc_enddef(ncID);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure("netCDF file", "switch to data mode", "file");
        }

        // Fill pnc with the current values of node counts
        for(size_t itr = 0; itr < pnc_dim_len; itr++)
        {
            int ncount_in;
            err_code = nc_get_var1_int(ncID, node_count_varID, &itr, &ncount_in);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCInqFailure(containerVarName.c_str(), CF_SG_NODE_COUNT, "integer datum");
            }

            err_code = nc_put_var1_int(ncID, pnc_varID, &itr, &ncount_in);
            NCDF_ERR(err_code);
            if (err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCWriteFailure(containerVarName.c_str(), CF_SG_PART_NODE_COUNT, "integer datum");
            }
        }
    }


    static std::string sgwe_msg_builder
        (    const char * layer_name,
            const char * failure_name,
            const char * failure_type,
            const char * special_msg
        )
    {
        return
            std::string("[") + std::string(layer_name) + std::string("] ") +
            std::string(failure_type) + std::string(" ") + std::string(failure_name) + std::string(" ") + std::string(special_msg);
    }

    // Exception related definitions
    SGWriter_Exception_NCWriteFailure::SGWriter_Exception_NCWriteFailure(const char * layer_name, const char * failure_name, const char * failure_type)
        : msg(sgwe_msg_builder(layer_name, failure_name, failure_type,
            "could not be written to (write failure)."))
    {}

    SGWriter_Exception_NCInqFailure::SGWriter_Exception_NCInqFailure (const char * layer_name, const char * failure_name, const char * failure_type)
    : msg(sgwe_msg_builder(layer_name, failure_name, failure_type,
            "could not be read from (property inquiry failure)."))
    {}

    SGWriter_Exception_NCDefFailure::SGWriter_Exception_NCDefFailure(const char * layer_name, const char * failure_name, const char * failure_type)
    : msg(sgwe_msg_builder(layer_name, failure_name, failure_type,
            "could not be defined in the dataset (definition failure)."))
    {}

    // SGeometry_Layer_WBuffer
    void SGeometry_Layer_WBuffer::cpyNCOUNT_into_PNC()
    {
        std::queue<int> refill = std::queue<int>(ncounts);
        while(!pnc.empty())
        {
            refill.push(pnc.front());
            pnc.pop();
        }
        this->pnc = refill;
    }

    // Helper function definitions
    int write_Geometry_Container
        (int ncID, const std::string& name, geom_t geometry_type, const std::vector<std::string> & node_coordinate_names)
    {

        int write_var_id;
        int err_code;

        // Define geometry container variable
        err_code = nc_def_var(ncID, name.c_str(), NC_FLOAT, 0, nullptr, &write_var_id);
        // todo: exception handling of err_code
        NCDF_ERR(err_code);
        if(err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCDefFailure(name.c_str(), "geometry_container", "variable");
        }


        /* Geometry Type Attribute
         * -
         */

        // Next, go on to add attributes needed for each geometry type
        std::string geometry_str =
            (geometry_type == POINT || geometry_type == MULTIPOINT) ? CF_SG_TYPE_POINT :
            (geometry_type == LINE || geometry_type == MULTILINE) ? CF_SG_TYPE_LINE :
            (geometry_type == POLYGON || geometry_type == MULTIPOLYGON) ? CF_SG_TYPE_POLY :
            ""; // obviously an error condition...

        if(geometry_str == "")
        {
            throw SG_Exception_BadFeature();
        }

        // Add the geometry type attribute
        err_code = nc_put_att_text(ncID, write_var_id, CF_SG_GEOMETRY_TYPE, geometry_str.size(), geometry_str.c_str());
        NCDF_ERR(err_code);
        if(err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCWriteFailure(name.c_str(), CF_SG_GEOMETRY_TYPE, "attribute in geometry_container");
        }

        /* Node Coordinates Attribute
         * -
         */
        std::string ncoords_atr_str = "";

        for(size_t itr = 0; itr < node_coordinate_names.size(); itr++)
        {
            ncoords_atr_str += node_coordinate_names[itr];
            if (itr < node_coordinate_names.size() - 1)
            {
                ncoords_atr_str += " ";
            }
        }

        err_code = nc_put_att_text(ncID, write_var_id, CF_SG_NODE_COORDINATES, ncoords_atr_str.size(), ncoords_atr_str.c_str());

        NCDF_ERR(err_code);
        if(err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCWriteFailure(name.c_str(), CF_SG_NODE_COORDINATES, "attribute in geometry_container");
        }
        // The previous two attributes are all that are required from POINT


        /* Node_Count Attribute
         * (not needed for POINT)
         */
        if (geometry_type != POINT)
        {
            std::string nodecount_atr_str = name + "_node_count";

            err_code = nc_put_att_text(ncID, write_var_id, CF_SG_NODE_COUNT, nodecount_atr_str.size(), nodecount_atr_str.c_str());
            NCDF_ERR(err_code);
            if(err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCWriteFailure(name.c_str(), CF_SG_NODE_COUNT, "attribute in geometry_container");
            }
        }

        /* Part_Node_Count Attribute
         * (only needed for MULTILINE, MULTIPOLYGON, and (potentially) POLYGON)
         */
        if (geometry_type == MULTILINE || geometry_type == MULTIPOLYGON)
        {
            std::string pnc_atr_str = name + "_part_node_count";

            err_code = nc_put_att_text(ncID, write_var_id, CF_SG_PART_NODE_COUNT, pnc_atr_str.size(), pnc_atr_str.c_str());

            NCDF_ERR(err_code);
            if(err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCWriteFailure(name.c_str(), CF_SG_PART_NODE_COUNT, "attribute in geometry_container");
            }
        }

        return write_var_id;
    }
}
