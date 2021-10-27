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
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>
#include "netcdf.h"
#include "netcdfdataset.h"
#include "netcdfsg.h"
namespace nccfdriver
{
    /* Attribute Fetch
     * -
     * A function which makes it a bit easier to fetch single text attribute values
     * ncid: as used in netcdf.h
     * varID: variable id in which to look for the attribute
     * attrName: name of attribute to fine
     * alloc: a reference to a string that will be filled with the attribute (i.e. truncated and filled with the return value)
     * Returns: a reference to the string to fill (a.k.a. string pointed to by alloc reference)
     */
    std::string& attrf(int ncid, int varId, const char * attrName, std::string& alloc)
    {
        size_t len = 0;
        nc_inq_attlen(ncid, varId, attrName, &len);

        if(len < 1)
        {
            alloc.clear();
            return alloc;
        }

        alloc.resize(len);
        memset(&alloc[0], 0, len);

        // Now look through this variable for the attribute
        nc_get_att_text(ncid, varId, attrName, &alloc[0]);

        return alloc;
    }


    /* SGeometry_Reader
     * (implementations)
     *
     */
    SGeometry_Reader::SGeometry_Reader(int ncId, int geoVarId)
        : gc_varId(geoVarId), touple_order(0)
    {

        char container_name[NC_MAX_NAME + 1];
        memset(container_name, 0, NC_MAX_NAME + 1);

        // Get geometry container name
        if(nc_inq_varname(ncId, geoVarId, container_name) != NC_NOERR)
        {
            throw SG_Exception_Existential("new geometry container", "the variable of the given ID");
        }

        // Establish string version of container_name
        container_name_s = std::string(container_name);

        // Find geometry type
        this->type = nccfdriver::getGeometryType(ncId, geoVarId);

        if(this->type == NONE)
        {
            throw SG_Exception_Existential(static_cast<const char*>(container_name), CF_SG_GEOMETRY_TYPE);
        }

        // Get grid mapping variable, if it exists
        this->gm_varId = INVALID_VAR_ID;
        if(attrf(ncId, geoVarId, CF_GRD_MAPPING, gm_name_s) != "")
        {
            const char * gm_name = gm_name_s.c_str();
            int gmVID;
            if(nc_inq_varid(ncId, gm_name, &gmVID) == NC_NOERR)
            {
                this->gm_varId = gmVID;
            }
        }

        // Find a list of node counts and part node count
        std::string nc_name_s;
        std::string pnc_name_s;
        std::string ir_name_s;
        int pnc_vid = INVALID_VAR_ID;
        int nc_vid = INVALID_VAR_ID;
        int ir_vid = INVALID_VAR_ID;
        int buf;
        size_t bound = 0;
        size_t total_node_count = 0; // used in error checks later
        size_t total_part_node_count = 0;
        if(attrf(ncId, geoVarId, CF_SG_NODE_COUNT, nc_name_s) != "")
        {
            const char * nc_name = nc_name_s.c_str();
            nc_inq_varid(ncId, nc_name, &nc_vid);
            while(nc_get_var1_int(ncId, nc_vid, &bound, &buf) == NC_NOERR)
            {
                if(buf < 0)
                {
                    throw SG_Exception_Value_Violation(static_cast<const char*>(container_name), CF_SG_NODE_COUNT,
                                                       "negative");
                }

                this->node_counts.push_back(buf);
                total_node_count += buf;
                bound++;
            }

        }

        if(attrf(ncId, geoVarId, CF_SG_PART_NODE_COUNT, pnc_name_s) != "")
        {
            const char * pnc_name = pnc_name_s.c_str();
            bound = 0;
            nc_inq_varid(ncId, pnc_name, &pnc_vid);
            while(nc_get_var1_int(ncId, pnc_vid, &bound, &buf) == NC_NOERR)
            {
                if(buf < 0)
                {
                    throw SG_Exception_Value_Violation(static_cast<const char*>(container_name), CF_SG_PART_NODE_COUNT,
                                                       "negative");
                }

                this->pnode_counts.push_back(buf);
                total_part_node_count += buf;
                bound++;
            }
        }

        if(attrf(ncId, geoVarId, CF_SG_INTERIOR_RING, ir_name_s) != "")
        {
            const char * ir_name = ir_name_s.c_str();
            bound = 0;
            nc_inq_varid(ncId, ir_name, &ir_vid);
            while(nc_get_var1_int(ncId, ir_vid, &bound, &buf) == NC_NOERR)
            {
                bool store = buf == 0 ? false : true;

                if(buf != 0 && buf != 1)
                {
                    throw SG_Exception_Value_Required(static_cast<const char*>(container_name), CF_SG_INTERIOR_RING,
                                                       "0 or 1");
                }

                this->int_rings.push_back(store);
                bound++;
            }
        }


        /* Enforcement of well formed CF files
         * If these are not met then the dataset is malformed and will halt further processing of
         * simple geometries.
         */

        // part node count exists only when node count exists
        if(pnode_counts.size() > 0 && node_counts.size() == 0)
        {
            throw SG_Exception_Dep(static_cast<const char *>(container_name), CF_SG_PART_NODE_COUNT, CF_SG_NODE_COUNT);
        }

        // part node counts and node counts don't match up in count
        if(pnc_vid != INVALID_VAR_ID && total_node_count != total_part_node_count)
        {
            throw SG_Exception_BadSum(static_cast<const char*>(container_name), CF_SG_PART_NODE_COUNT, CF_SG_PART_NODE_COUNT);
        }

        // interior rings only exist when part node counts exist
        if(int_rings.size() > 0 && pnode_counts.size() == 0)
        {
            throw SG_Exception_Dep(static_cast<const char *>(container_name), CF_SG_INTERIOR_RING, CF_SG_PART_NODE_COUNT);
        }


        // cardinality of part_node_counts == cardinality of interior_ring (if interior ring > 0)
        if(int_rings.size() > 0)
        {
            if(int_rings.size() != pnode_counts.size())
            {
                throw SG_Exception_Dim_MM(static_cast<const char *>(container_name), CF_SG_INTERIOR_RING, CF_SG_PART_NODE_COUNT);
            }
        }

        // lines and polygons require node_counts, multipolygons are checked with part_node_count
        if(this->type == POLYGON || this->type == LINE)
        {
            if(node_counts.size() < 1)
            {
                throw SG_Exception_Existential(static_cast<const char*>(container_name), CF_SG_NODE_COUNT);
            }
        }

        /* Basic Safety checks End
         */

        // Create bound list
        size_t rc = 0;
        bound_list.push_back(0);// start with 0

        if(node_counts.size() > 0)
        {
            for(size_t i = 0; i < node_counts.size() - 1; i++)
            {
                rc = rc + node_counts[i];
                bound_list.push_back(rc);
            }
        }


        std::string cart_s;
        // Node Coordinates
        if(attrf(ncId, geoVarId, CF_SG_NODE_COORDINATES, cart_s) == "")
        {
            throw SG_Exception_Existential(container_name, CF_SG_NODE_COORDINATES);
        }

        // Create parts count list and an offset list for parts indexing
        if(this->node_counts.size() > 0)
        {
            int ind = 0;
            int parts = 0;
            int prog = 0;
            int c = 0;

            for(size_t pcnt = 0; pcnt < pnode_counts.size() ; pcnt++)
            {
                if(prog == 0) pnc_bl.push_back(pcnt);

                if(int_rings.size() > 0 && !int_rings[pcnt])
                    c++;

                prog = prog + pnode_counts[pcnt];
                parts++;

                if(prog == node_counts[ind])
                {
                    ind++;
                    this->parts_count.push_back(parts);
                    if(int_rings.size() > 0)
                        this->poly_count.push_back(c);
                    c = 0;
                    prog = 0; parts = 0;
                }
                else if(prog > node_counts[ind])
                {
                    throw SG_Exception_BadSum(container_name, CF_SG_PART_NODE_COUNT, CF_SG_NODE_COUNT);
                }
            }
        }

        // (1) the tuple order for a single point
        // (2) the variable ids with the relevant coordinate values
        int X = INVALID_VAR_ID;
        int Y = INVALID_VAR_ID;
        int Z = INVALID_VAR_ID;

        char cart[NC_MAX_NAME + 1];
        memset(cart, 0, NC_MAX_NAME + 1);
        strncpy(cart, cart_s.c_str(), NC_MAX_NAME);

        char * dim = strtok(cart,  " ");
        int axis_id = 0;

        while(dim != nullptr)
        {
            if(nc_inq_varid(ncId, dim, &axis_id) == NC_NOERR)
            {

                // Check axis signature
                std::string a_sig;
                attrf(ncId, axis_id, CF_AXIS, a_sig);

                // If valid signify axis correctly
                if(a_sig == "X")
                {
                    X = axis_id;
                }
                else if(a_sig == "Y")
                {
                    Y = axis_id;
                }
                else if(a_sig == "Z")
                {
                    Z = axis_id;
                }
                else
                {
                    throw SG_Exception_Dep(container_name, "A node_coordinates variable", CF_AXIS);
                }

                this->touple_order++;
            }
            else
            {
                throw SG_Exception_Existential(container_name, dim);
            }

            dim = strtok(nullptr, " ");
        }

        // Write axis in X, Y, Z order

        if(X != INVALID_VAR_ID)
            this->nodec_varIds.push_back(X);
        else
        {
            throw SG_Exception_Existential(container_name, "node_coordinates: X-axis");
        }
        if(Y != INVALID_VAR_ID)
            this->nodec_varIds.push_back(Y);
        else
        {
            throw SG_Exception_Existential(container_name, "node_coordinates: Y-axis");
        }
        if(Z != INVALID_VAR_ID)
            this->nodec_varIds.push_back(Z);

        /* Final Checks for node coordinates
         * (1) Each axis has one and only one dimension, dim length of each node coordinates are all equal
         * (2) total node count == dim length of each node coordinates (if node_counts not empty)
         * (3) there are at least two node coordinate variable ids
         */

        int all_dim = INVALID_VAR_ID;
        bool dim_set = false;
        //(1) one dimension check, each node_coordinates have same dimension
        for(size_t nvitr = 0; nvitr < nodec_varIds.size(); nvitr++)
        {
            int dimC = 0;
            nc_inq_varndims(ncId, nodec_varIds[nvitr], &dimC);

            if(dimC != 1)
            {
                throw SG_Exception_Not1D();
            }

            // check that all have same dimension
            int inter_dim[1];
            if(nc_inq_vardimid(ncId, nodec_varIds[nvitr], inter_dim) != NC_NOERR)
            {
                throw SG_Exception_Existential(container_name, "one or more node_coordinate dimensions");
            }

            if(!dim_set)
            {
                all_dim = inter_dim[0];
                dim_set = true;
            }

            else
            {
                if (inter_dim[0] != all_dim)
                    throw SG_Exception_Dim_MM(container_name, "X, Y", "in general all node coordinate axes");
            }
        }

        // (2) check equality one
        if(node_counts.size() > 0)
        {
            size_t diml = 0;
            nc_inq_dimlen(ncId, all_dim, &diml);

            if(diml != total_node_count)
                throw SG_Exception_BadSum(container_name, "node_count", "node coordinate dimension length");
        }


        // (3) check tuple order
        if(this->touple_order < 2)
        {
            throw SG_Exception_Existential(container_name, "insufficient node coordinates must have at least two axis");
        }

       /* Investigate for instance dimension
        * The procedure is as follows
        *
        * (1) if there's node_count, use the dimension used to index node count
        * (2) otherwise it's point (singleton) data, in this case use the node coordinate dimension
        */
        size_t instance_dim_len = 0;

        if(node_counts.size() >= 1)
        {
            int nc_dims = 0;
            nc_inq_varndims(ncId, nc_vid, &nc_dims);

            if(nc_dims != 1) throw SG_Exception_Not1D();

            int nc_dim_id[1];

            if(nc_inq_vardimid(ncId, nc_vid, nc_dim_id) != NC_NOERR)
            {
                throw SG_Exception_Existential(container_name, "node_count dimension");
            }

            this->inst_dimId = nc_dim_id[0];
        }

        else
        {
            this->inst_dimId = all_dim;
        }

        nc_inq_dimlen(ncId, this->inst_dimId, &instance_dim_len);

        if(instance_dim_len == 0)
            throw SG_Exception_EmptyDim();

        // Set values accordingly
        this->inst_dimLen = instance_dim_len;
        this->pt_buffer = cpl::make_unique<Point>(this->touple_order);
        this->gc_varId = geoVarId;
        this->ncid = ncId;
    }

    Point& SGeometry_Reader::operator[](size_t index)
    {
        for(int order = 0; order < touple_order; order++)
        {
            Point& pt = *(this->pt_buffer);
            double data;
            size_t real_ind = index;

            // Read a single coord
            int err = nc_get_var1_double(ncid, nodec_varIds[order], &real_ind, &data);

            if(err != NC_NOERR)
            {
                throw SG_Exception_BadPoint();
            }

            pt[order] = data;
        }

        return *(this->pt_buffer);
    }

    size_t SGeometry_Reader::get_geometry_count()
    {
        if(type == POINT)
        {
            if(this->nodec_varIds.size() < 1) return 0;

            // If more than one dim, then error. Otherwise inquire its length and return that
            int dims;
            if(nc_inq_varndims(this->ncid, nodec_varIds[0], &dims) != NC_NOERR) return 0;
            if(dims != 1) return 0;

            // Find which dimension is used for x
            int index;
            if(nc_inq_vardimid(this->ncid, nodec_varIds[0], &index) != NC_NOERR)
            {
                return 0;
            }

            // Finally find the length
            size_t len;
            if(nc_inq_dimlen(this->ncid, index, &len) != NC_NOERR)
            {
                return 0;
            }
            return len;
        }

        else return this->node_counts.size();
    }

    static void add_to_buffer(std::vector<unsigned char>& buffer, uint8_t v)
    {
        buffer.push_back(v);
    }

    static void add_to_buffer(std::vector<unsigned char>& buffer, uint32_t v)
    {
        const size_t old_size = buffer.size();
        buffer.resize(old_size + sizeof(v));
        memcpy(&buffer[old_size], &v, sizeof(v));
    }

    static void add_to_buffer(std::vector<unsigned char>&, int) = delete;

    static void add_to_buffer(std::vector<unsigned char>& buffer, double v)
    {
        const size_t old_size = buffer.size();
        buffer.resize(old_size + sizeof(v));
        memcpy(&buffer[old_size], &v, sizeof(v));
    }

    /* serializeToWKB
     * Takes the geometry in SGeometry at a given index and converts it into WKB format.
     * Converting SGeometry into WKB automatically allocates the required buffer space
     * and returns a buffer that MUST be free'd
     */
    std::vector<unsigned char> SGeometry_Reader::serializeToWKB(size_t featureInd)
    {
        std::vector<unsigned char> ret;
        int nc = 0; size_t sb = 0;

        // Points don't have node_count entry... only inspect and set node_counts if not a point
        if(this->getGeometryType() != POINT)
        {
            nc = node_counts[featureInd];
            sb = bound_list[featureInd];
        }

        // Serialization occurs differently depending on geometry
        // The memory requirements also differ between geometries
        switch(this->getGeometryType())
        {
            case POINT:
                inPlaceSerialize_Point(this, featureInd, ret);
                break;

            case LINE:
                inPlaceSerialize_LineString(this, nc, sb, ret);
                break;

            case POLYGON:
                // A polygon has:
                // 1 byte header
                // 4 byte Type
                // 4 byte ring count (1 (exterior)) [assume only exterior rings, otherwise multipolygon]
                // For each ring:
                // 4 byte point count, 8 byte double x point count x # dimension
                // (This is equivalent to requesting enough for all points and a point count header for each point)
                // (Or 8 byte double x node count x # dimension + 4 byte point count x part_count)

                // if interior ring, then assume that it will be a multipolygon (maybe future work?)
                inPlaceSerialize_PolygonExtOnly(this, nc, sb, ret);
                break;

            case MULTIPOINT:
                {
                    uint8_t header = PLATFORM_HEADER;
                    uint32_t t = this->touple_order == 2 ? wkbMultiPoint :
                                 this->touple_order == 3 ? wkbMultiPoint25D : wkbNone;

                    if(t == wkbNone) throw SG_Exception_BadFeature();

                    // Add metadata
                    add_to_buffer(ret, header);
                    add_to_buffer(ret, t);
                    add_to_buffer(ret, static_cast<uint32_t>(nc));

                    // Add points
                    for(int pts = 0; pts < nc; pts++)
                    {
                        inPlaceSerialize_Point(this, static_cast<size_t>(sb + pts), ret);
                    }
                }

                break;

            case MULTILINE:
                {
                    uint8_t header = PLATFORM_HEADER;
                    uint32_t t = this->touple_order == 2 ? wkbMultiLineString :
                                this->touple_order == 3 ? wkbMultiLineString25D : wkbNone;

                    if(t == wkbNone) throw SG_Exception_BadFeature();
                    int32_t lc = parts_count[featureInd];
                    size_t seek_begin = sb;
                    size_t pc_begin = pnc_bl[featureInd]; // initialize with first part count, list of part counts is contiguous
                    std::vector<int> pnc;

                    // Build sub vector for part_node_counts
                    // + Calculate wkbSize
                    for(int itr = 0; itr < lc; itr++)
                    {
                        pnc.push_back(pnode_counts[pc_begin + itr]);
                    }

                    size_t cur_point = seek_begin;
                    size_t pcount = pnc.size();

                    // Begin Writing
                    add_to_buffer(ret, header);
                    add_to_buffer(ret, t);
                    add_to_buffer(ret, static_cast<uint32_t>(pcount));

                    for(size_t itr = 0; itr < pcount; itr++)
                    {
                            inPlaceSerialize_LineString(this, pnc[itr], cur_point, ret);
                            cur_point = pnc[itr] + cur_point;
                    }
                }

                break;

            case MULTIPOLYGON:
                {
                    uint8_t header = PLATFORM_HEADER;
                    uint32_t t = this->touple_order == 2 ? wkbMultiPolygon:
                                 this->touple_order == 3 ? wkbMultiPolygon25D: wkbNone;

                    if(t == wkbNone) throw SG_Exception_BadFeature();
                    bool noInteriors = this->int_rings.size() == 0 ? true : false;
                    int32_t rc = parts_count[featureInd];
                    size_t seek_begin = sb;
                    size_t pc_begin = pnc_bl[featureInd]; // initialize with first part count, list of part counts is contiguous
                    std::vector<int> pnc;

                    // Build sub vector for part_node_counts
                    for(int itr = 0; itr < rc; itr++)
                    {
                        pnc.push_back(pnode_counts[pc_begin + itr]);
                    }

                    // Create Multipolygon headers
                    add_to_buffer(ret, header);
                    add_to_buffer(ret, t);

                    if(noInteriors)
                    {
                        size_t cur_point = seek_begin;
                        size_t pcount = pnc.size();
                        add_to_buffer(ret, static_cast<uint32_t>(pcount));

                        for(size_t itr = 0; itr < pcount; itr++)
                        {
                            inPlaceSerialize_PolygonExtOnly(this, pnc[itr], cur_point, ret);
                            cur_point = pnc[itr] + cur_point;
                        }
                    }

                    else
                    {
                        int32_t polys = poly_count[featureInd];
                        add_to_buffer(ret, static_cast<uint32_t>(polys));

                        size_t base = pnc_bl[featureInd]; // beginning of parts_count for this multigeometry
                        size_t seek = seek_begin; // beginning of node range for this multigeometry
                        size_t ir_base = base + 1;

                        // has interior rings,
                        for(int32_t itr = 0; itr < polys; itr++)
                        {
                            int rc_m = 1;

                            // count how many parts belong to each Polygon
                            while(ir_base < int_rings.size() && int_rings[ir_base])
                            {
                                rc_m++;
                                ir_base++;
                            }

                            if(rc_m == 1)
                                ir_base++;    // single ring case

                            std::vector<int> poly_parts;

                            // Make part node count sub vector
                            for(int itr_2 = 0; itr_2 < rc_m; itr_2++)
                            {
                                poly_parts.push_back(pnode_counts[base + itr_2]);
                            }

                            inPlaceSerialize_Polygon(this, poly_parts, rc_m, seek, ret);

                            // Update seek position
                            for(size_t itr_3 = 0; itr_3 < poly_parts.size(); itr_3++)
                            {
                                seek += poly_parts[itr_3];
                            }

                            base += poly_parts.size();
                            // cppcheck-suppress redundantAssignment
                            ir_base = base + 1;
                        }
                    }
                }
                break;

                default:

                    throw SG_Exception_BadFeature();
                    break;
        }

        return ret;
    }

    void SGeometry_PropertyScanner::open(int container_id)
    {
        // First check for container_id, if variable doesn't exist error out
        if(nc_inq_var(this->nc, container_id, nullptr, nullptr, nullptr, nullptr, nullptr) != NC_NOERR)
        {
            return;    // change to exception
        }

        // Now exists, see what variables refer to this one
        // First get name of this container
        char contname[NC_MAX_NAME + 1];
        memset(contname, 0, NC_MAX_NAME + 1);
        if(nc_inq_varname(this->nc, container_id, contname) != NC_NOERR)
        {
            return;
        }

        // Then scan throughout the netcdfDataset if those variables geometry_container
        // attribute matches the container
        int varCount = 0;
        if(nc_inq_nvars(this->nc, &varCount) != NC_NOERR)
        {
            return;
        }

        for(int curr = 0; curr < varCount; curr++)
        {
            size_t contname2_len = 0;

            // First find container length, and make buf that size in chars
            if(nc_inq_attlen(this->nc, curr, CF_SG_GEOMETRY, &contname2_len) != NC_NOERR)
            {
                // not a geometry variable, continue
                continue;
            }

            // Also if present but empty, go on
            if(contname2_len == 0) continue;

            // Otherwise, geometry: see what container it has
            char buf[NC_MAX_CHAR + 1];
            memset(buf, 0, NC_MAX_CHAR + 1);

            if(nc_get_att_text(this->nc, curr, CF_SG_GEOMETRY, buf)!= NC_NOERR)
            {
                continue;
            }

            // If matches, then establish a reference by placing this variable's data in both vectors
            if(!strcmp(contname, buf))
            {
                char property_name[NC_MAX_NAME + 1] = {0};

                // look for special OGR original name field
                if(nc_get_att_text(this->nc, curr, OGR_SG_ORIGINAL_LAYERNAME, property_name) != NC_NOERR)
                {
                    // if doesn't exist, then just use the variable name
                    if(nc_inq_varname(this->nc, curr, property_name) != NC_NOERR)
                    {
                        throw SG_Exception_General_Malformed(contname);
                    }
                }

                std::string n(property_name);
                v_ids.push_back(curr);
                v_headers.push_back(n);
            }
        }
    }

    // Exception Class Implementations
    SG_Exception_Dim_MM::SG_Exception_Dim_MM(const char* container_name, const char* field_1, const char* field_2)
    {
        std::string cn_s(container_name);
        std::string field1_s(field_1);
        std::string field2_s(field_2);

        this -> err_msg = "[" + cn_s + "] One or more dimensions of "
                + field1_s
                + " and "
                + field2_s
                + " do not match but must match.";
    }

    SG_Exception_Existential::SG_Exception_Existential(const char* container_name, const char* missing_name)
    {
        std::string cn_s(container_name);
        std::string mn_s(missing_name);

        this -> err_msg = "[" + cn_s + "] The property or the variable associated with "
                + mn_s
                + " is missing.";
    }

    SG_Exception_Dep::SG_Exception_Dep(const char* container_name, const char* arg1, const char* arg2)
    {
        std::string cn_s(container_name);
        std::string arg1_s(arg1);
        std::string arg2_s(arg2);

        this -> err_msg = "[" + cn_s + "] The attribute "
                + arg1_s
                + " may not exist without the attribute "
                + arg2_s
                + " existing.";
    }

    SG_Exception_BadSum::SG_Exception_BadSum(const char* container_name, const char* arg1, const char* arg2)
    {
        std::string cn_s(container_name);
        std::string arg1_s(arg1);
        std::string arg2_s(arg2);

        this -> err_msg = "[" + cn_s + "]"
                + " The sum of all values in "
                + arg1_s
                + " and "
                + arg2_s
                + " do not match.";
    }

    SG_Exception_General_Malformed
        ::SG_Exception_General_Malformed(const char * arg)
    {
        std::string arg1_s(arg);

        this -> err_msg = "Corruption or malformed formatting has been detected in: " + arg1_s;
    }

    // to get past linker
    SG_Exception::~SG_Exception() {}

    // Helpers
    // following is a short hand for a clean up and exit, since goto isn't allowed
    double getCFVersion(int ncid)
    {
        double ver = -1.0;
        std::string attrVal;

        // Fetch the CF attribute
        if(attrf(ncid, NC_GLOBAL, NCDF_CONVENTIONS, attrVal) == "")
        {
            return ver;
        }

        if(sscanf(attrVal.c_str(), "CF-%lf", &ver) != 1)
        {
            return -1.0;
        }

        return ver;
    }

    geom_t getGeometryType(int ncid, int varid)
    {
        geom_t ret = UNSUPPORTED;
        std::string gt_name_s;
        const char * gt_name= attrf(ncid, varid, CF_SG_GEOMETRY_TYPE, gt_name_s).c_str();

        if(gt_name == nullptr)
        {
            return NONE;
        }

        // Points
        if(!strcmp(gt_name, CF_SG_TYPE_POINT))
        {
            // Node Count not present? Assume that it is a multipoint.
            if(nc_inq_att(ncid, varid, CF_SG_NODE_COUNT, nullptr, nullptr) == NC_ENOTATT)
            {
                ret = POINT;
            }
            else ret = MULTIPOINT;
        }

        // Lines
        else if(!strcmp(gt_name, CF_SG_TYPE_LINE))
        {
            // Part Node Count present? Assume multiline
            if(nc_inq_att(ncid, varid, CF_SG_PART_NODE_COUNT, nullptr, nullptr) == NC_ENOTATT)
            {
                ret = LINE;
            }
            else ret = MULTILINE;
        }

        // Polygons
        else if(!strcmp(gt_name, CF_SG_TYPE_POLY))
        {
            /* Polygons versus MultiPolygons, slightly ambiguous
             * Part Node Count & no Interior Ring - MultiPolygon
             * no Part Node Count & no Interior Ring - Polygon
             * Part Node Count & Interior Ring - assume that it is a MultiPolygon
             */
            int pnc_present = nc_inq_att(ncid, varid, CF_SG_PART_NODE_COUNT, nullptr, nullptr);
            int ir_present = nc_inq_att(ncid, varid, CF_SG_INTERIOR_RING, nullptr, nullptr);

            if(pnc_present == NC_ENOTATT && ir_present == NC_ENOTATT)
            {
                ret = POLYGON;
            }
            else ret = MULTIPOLYGON;
        }

        return ret;
    }

    void inPlaceSerialize_Point(SGeometry_Reader * ge, size_t seek_pos, std::vector<unsigned char>& buffer)
    {
        uint8_t order = PLATFORM_HEADER;
        uint32_t t = ge->get_axisCount() == 2 ? wkbPoint:
                     ge->get_axisCount() == 3 ? wkbPoint25D: wkbNone;

        if(t == wkbNone) throw SG_Exception_BadFeature();

        add_to_buffer(buffer, order);
        add_to_buffer(buffer, t);

        // Now get point data;
        Point & p = (*ge)[seek_pos];
        add_to_buffer(buffer, p[0]);
        add_to_buffer(buffer, p[1]);

        if(ge->get_axisCount() >= 3)
        {
            add_to_buffer(buffer, p[2]);
        }
    }

    void inPlaceSerialize_LineString(SGeometry_Reader * ge, int node_count, size_t seek_begin, std::vector<unsigned char>& buffer)
    {
        uint8_t order = PLATFORM_HEADER;
        uint32_t t = ge->get_axisCount() == 2 ? wkbLineString:
                     ge->get_axisCount() == 3 ? wkbLineString25D: wkbNone;

        if(t == wkbNone) throw SG_Exception_BadFeature();
        uint32_t nc = (uint32_t) node_count;

        add_to_buffer(buffer, order);
        add_to_buffer(buffer, t);
        add_to_buffer(buffer, nc);

        // Now serialize points
        for(int ind = 0; ind < node_count; ind++)
        {
            Point & p = (*ge)[seek_begin + ind];
            add_to_buffer(buffer, p[0]);
            add_to_buffer(buffer, p[1]);

            if(ge->get_axisCount() >= 3)
            {
                add_to_buffer(buffer, p[2]);
            }
        }
    }

    void inPlaceSerialize_PolygonExtOnly(SGeometry_Reader * ge, int node_count, size_t seek_begin, std::vector<unsigned char>& buffer)
    {
        uint8_t order = PLATFORM_HEADER;
        uint32_t t = ge->get_axisCount() == 2 ? wkbPolygon:
                     ge->get_axisCount() == 3 ? wkbPolygon25D: wkbNone;

        if(t == wkbNone) throw SG_Exception_BadFeature();
        uint32_t rc = 1;

        add_to_buffer(buffer, order);
        add_to_buffer(buffer, t);
        add_to_buffer(buffer, rc);
        add_to_buffer(buffer, static_cast<uint32_t>(node_count));
        for(int pind = 0; pind < node_count; pind++)
        {
            Point & pt= (*ge)[seek_begin + pind];
            add_to_buffer(buffer, pt[0]);
            add_to_buffer(buffer, pt[1]);

            if(ge->get_axisCount() >= 3)
            {
                add_to_buffer(buffer, pt[2]);
            }
        }
    }

    void inPlaceSerialize_Polygon(SGeometry_Reader * ge, std::vector<int>& pnc, int ring_count, size_t seek_begin, std::vector<unsigned char>& buffer)
    {
        uint8_t order = PLATFORM_HEADER;
        uint32_t t = ge->get_axisCount() == 2 ? wkbPolygon:
                     ge->get_axisCount() == 3 ? wkbPolygon25D: wkbNone;

        if(t == wkbNone) throw SG_Exception_BadFeature();
        uint32_t rc = static_cast<uint32_t>(ring_count);

        add_to_buffer(buffer, order);
        add_to_buffer(buffer, t);
        add_to_buffer(buffer, rc);
        int cmoffset = 0;
        for(int ring_c = 0; ring_c < ring_count; ring_c++)
        {
            uint32_t node_count = pnc[ring_c];
            add_to_buffer(buffer, node_count);

            int pind = 0;
            for(pind = 0; pind < pnc[ring_c]; pind++)
            {
                Point & pt= (*ge)[seek_begin + cmoffset + pind];
                add_to_buffer(buffer, pt[0]);
                add_to_buffer(buffer, pt[1]);

                if(ge->get_axisCount() >= 3)
                {
                     add_to_buffer(buffer, pt[2]);
                }
            }

            cmoffset += pind;
        }
    }

    int scanForGeometryContainers(int ncid, std::set<int> & r_ids)
    {
        int nvars;
        if(nc_inq_nvars(ncid, &nvars) != NC_NOERR)
        {
            return -1;
        }

        r_ids.clear();

        // For each variable check for geometry attribute
        // If has geometry attribute, then check the associated variable ID

        for(int itr = 0; itr < nvars; itr++)
        {
            char c[NC_MAX_CHAR];
            memset(c, 0, NC_MAX_CHAR);
            if(nc_get_att_text(ncid, itr, CF_SG_GEOMETRY, c) != NC_NOERR)
            {
                continue;
            }

            int varID;
            if(nc_inq_varid(ncid, c, &varID) != NC_NOERR)
            {
                continue;
            }

            // Now have variable ID. See if vector contains it, and if not
            // insert
            r_ids.insert(varID); // It's a set. No big deal sets don't allow duplicates anyways
        }

        return 0 ;
    }
}
