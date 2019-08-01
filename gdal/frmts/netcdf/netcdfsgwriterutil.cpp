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

        if (this->type == POINT)
        {
            // Set total node count (1)
            this->total_point_count = 1;

            // Also single part geometry (1)
            this->total_part_count = 1;

            // One part per part
            ppart_node_count.push_back(1);
        }

        else if (this->type == MULTIPOINT)
        {
            OGRMultiPoint& r_defnGeometryMP = dynamic_cast<OGRMultiPoint&>(r_defnGeometry);

            // Set total node count
            this->total_point_count = r_defnGeometryMP.getNumGeometries();

            // The amount of nodes is also the amount of parts
            for(size_t pc = 0; pc < total_point_count; pc++)
            {
                ppart_node_count.push_back(1);
            }

            // total part count == total node count
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

    void ncLayer_SG_Metadata::initializeNewContainer(int containerVID)
    {
        this->containerVar_realID = containerVID;

        netCDFVID& ncdf = this->vDataset;
        geom_t geo = this->writableType;

         // Define some virtual dimensions, and some virtual variables
        char container_name[NC_MAX_CHAR + 1] = {0};
        char node_coord_names[NC_MAX_CHAR + 1] = {0};

        // Set default values
        pnc_varID = INVALID_VAR_ID;
        pnc_dimID = INVALID_DIM_ID;
        intring_varID = INVALID_VAR_ID;

        int err_code;
        err_code = nc_inq_varname(ncID, containerVar_realID, container_name);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCInqFailure("new layer", "geometry container", "var name of");
        }

        this->containerVarName = std::string(container_name);

        // Node Coordinates - Dim
        std::string nodecoord_name = containerVarName + "_" + std::string(CF_SG_NODE_COORDINATES);

        node_coordinates_dimID = ncdf.nc_def_vdim(nodecoord_name.c_str(), 1);

        // Node Coordinates - Variable Names
        err_code = nc_get_att_text(ncID, containerVar_realID, CF_SG_NODE_COORDINATES, node_coord_names);
        NCDF_ERR(err_code);
        if (err_code != NC_NOERR)
        {
            throw SGWriter_Exception_NCInqFailure(containerVarName.c_str(), CF_SG_NODE_COORDINATES, "varName");
        }

        // Node Count
        if(geo != POINT)
        {
            std::string nodecount_name = containerVarName + "_" + std::string(CF_SG_NODE_COUNT);
            node_count_dimID = ncdf.nc_def_vdim(nodecount_name.c_str(), 1);
            node_count_varID = ncdf.nc_def_vvar(nodecount_name.c_str(), NC_INT, 1, &node_count_dimID);
        }

        // Do the same for part node count, if it exists
        char pnc_name[NC_MAX_CHAR + 1] = {0};
        err_code = nc_get_att_text(ncID, containerVar_realID, CF_SG_PART_NODE_COUNT, pnc_name);


        if(err_code == NC_NOERR)
        {
            pnc_dimID = ncdf.nc_def_vdim(pnc_name, 1);
            pnc_varID = ncdf.nc_def_vvar(pnc_name, NC_INT, 1, &pnc_dimID);

            char ir_name[NC_MAX_CHAR + 1] = {0}; 
            err_code = nc_get_att_text(ncID, containerVar_realID, CF_SG_INTERIOR_RING, ir_name);

            // For interior ring too (for POLYGON and MULTIPOLYGON)
            if(this->writableType == POLYGON || this->writableType == MULTIPOLYGON)
            {
                intring_varID = ncdf.nc_def_vvar(ir_name, NC_INT, 1, &pnc_dimID);
            }
        }

        // Node coordinates Var Definitions
        int new_varID;
        CPLStringList aosNcoord(CSLTokenizeString2(node_coord_names, " ", 0));

        if(aosNcoord.size() < 2)
            throw SGWriter_Exception();

        // first it's X
        new_varID = ncdf.nc_def_vvar(aosNcoord[0], NC_DOUBLE, 1, &node_coordinates_dimID);
        ncdf.nc_put_vatt_text(new_varID, CF_AXIS, CF_SG_X_AXIS);

        this->node_coordinates_varIDs.push_back(new_varID);

        // second it's Y
        new_varID = ncdf.nc_def_vvar(aosNcoord[1], NC_DOUBLE, 1, &node_coordinates_dimID);
        ncdf.nc_put_vatt_text(new_varID, CF_AXIS, CF_SG_Y_AXIS);

        this->node_coordinates_varIDs.push_back(new_varID);

        // (and perhaps) third it's Z
        if(aosNcoord.size() > 2)
        {
            new_varID = ncdf.nc_def_vvar(aosNcoord[2], NC_DOUBLE, 1, &node_coordinates_dimID);
            ncdf.nc_put_vatt_text(new_varID, CF_AXIS, CF_SG_Z_AXIS);

            this->node_coordinates_varIDs.push_back(new_varID);
        }
    }

/*    void ncLayer_SG_Metadata::scanExistingContainer(int containerVID)
    {
        // todo:
    }
*/
    ncLayer_SG_Metadata::ncLayer_SG_Metadata(int & i_ncID, geom_t geo, netCDFVID& ncdf, OGR_NCScribe& ncs) :
        ncID(i_ncID),
        vDataset(ncdf),
        ncb(ncs),
        writableType(geo)
    {
    }

    OGRPoint& SGeometry_Feature::getPoint(size_t part_no, int point_index)
    {
        if (this->type == POINT)
        {
            // Point case: always return the single point regardless of any thing

            OGRPoint* as_p_ref = dynamic_cast<OGRPoint*>(this->geometry_ref);
            return  *as_p_ref;
        }

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

    void ncLayer_SG_Metadata::writeSGeometryFeature(SGeometry_Feature& ft)
    {
        if (ft.getType() == NONE)
        {
            throw SG_Exception_BadFeature();
        }

        // Write each point from each part in node coordinates
        for(size_t part_no = 0; part_no < ft.getTotalPartCount(); part_no++)
        {
            if(this->writableType == POLYGON || this->writableType == MULTIPOLYGON)
            {
                int interior_ring_fl = 1;

                if(this->writableType == POLYGON)
                {
                    interior_ring_fl = part_no == 0 ? 0 : 1;
                }

                else if(this->writableType == MULTIPOLYGON)
                {
                    if(ft.IsPartAtIndInteriorRing(part_no))
                    {
                        interior_ring_fl = 1;
                    }

                    else
                    {
                        interior_ring_fl = 0;
                    }
                }

                if(interior_ring_fl)
                {
                    this->interiorRingDetected = true;
                }

                ncb.enqueue_transaction(MTPtr(new OGR_SGFS_NC_Int_Transaction(intring_varID, interior_ring_fl)));
            }

            if(this->writableType == POLYGON || this->writableType == MULTILINE || this->writableType == MULTIPOLYGON)
            {
                int pnc_writable = static_cast<int>(ft.getPerPartNodeCount()[part_no]);
                ncb.enqueue_transaction(MTPtr(new OGR_SGFS_NC_Int_Transaction(pnc_varID, pnc_writable)));
                this->next_write_pos_pnc++;
            }

            for(size_t pt_ind = 0; pt_ind < ft.getPerPartNodeCount()[part_no]; pt_ind++)
            {
                int pt_ind_int = static_cast<int>(pt_ind);
                OGRPoint& write_pt = ft.getPoint(part_no, pt_ind_int);

                // Write each node coordinate
                double x = write_pt.getX();
                ncb.enqueue_transaction(MTPtr(new OGR_SGFS_NC_Double_Transaction(node_coordinates_varIDs[0], x)));

                double y = write_pt.getY();
                ncb.enqueue_transaction(MTPtr(new OGR_SGFS_NC_Double_Transaction(node_coordinates_varIDs[1], y)));

                if(this->node_coordinates_varIDs.size() > 2)
                {
                    double z = write_pt.getZ();
                    ncb.enqueue_transaction(MTPtr(new OGR_SGFS_NC_Double_Transaction(node_coordinates_varIDs[2], z)));
                }
            }

            this->next_write_pos_node_coord += ft.getPerPartNodeCount()[part_no];
        }

        // Append node counts from the end, if not a POINT
        if(this->writableType != POINT)
        {
            int ncount_add = static_cast<int>(ft.getTotalNodeCount());
            ncb.enqueue_transaction(MTPtr(new OGR_SGFS_NC_Int_Transaction(node_count_varID, ncount_add)));
            this->next_write_pos_node_count++;
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

    // OGR_NCScribe
    void OGR_NCScribe::enqueue_transaction(std::shared_ptr<OGR_SGFS_Transaction> transactionAdd)
    {
        if(transactionAdd.get() == nullptr)
        {
            return;
        }

        // See if the variable name is already being written to
        if(this->varMaxInds.count(transactionAdd->getVarId()) > 0)
        {
            size_t varWriteLength = this->varMaxInds[transactionAdd->getVarId()];
            varWriteLength++;
            this->varMaxInds[transactionAdd->getVarId()] = varWriteLength;
        }

        else
        {
            // Otherwise, just add it to the list of variable names being written to
            std::pair<int, size_t> entry(transactionAdd->getVarId(), 1);
            this->varMaxInds.insert(entry);
        }

        // Add sizes to memory count
        this->buf.addCount(sizeof(transactionAdd)); // account for pointer
        this->buf.addCount(transactionAdd->count()); // account for pointee

        // Finally push the transaction in
        this->transactionQueue.push(transactionAdd);
    }

    void OGR_NCScribe::commit_transaction()
    {
        wl.startRead();

        // Buffered changes are the earliest, so commit those first
        MTPtr m = this->wl.pop();
        while(m.get() != nullptr)
        {
            int varId = m->getVarId();
            size_t writeInd;

            // First, find where to write. If doesn't exist, write to index 0
            if(this->varWriteInds.count(varId) > 0)
            {
                writeInd = this->varWriteInds[varId];
            }

            else
            {
                std::pair<int, size_t> insertable(varId, 0);
                this->varWriteInds.insert(insertable);
                writeInd = 0;
            }

            // Then write
            // Subtract sizes from memory count
            this->buf.subCount(sizeof(m)); // account for pointer
            this->buf.subCount(m->count()); // account for pointee

            // todo: check return value
            m->commit(ncvd, writeInd);

            // increment index
            this->varWriteInds[varId]++;

            m = this->wl.pop();
        }

        while(!transactionQueue.empty())
        {
            std::shared_ptr<OGR_SGFS_Transaction> t = this->transactionQueue.front();

            int varId = t->getVarId();
            size_t writeInd;

            // First, find where to write. If doesn't exist, write to index 0
            if(this->varWriteInds.count(varId) > 0)
            {
                writeInd = this->varWriteInds[varId];
            }

            else
            {
                std::pair<int, size_t> insertable(varId, 0);
                this->varWriteInds.insert(insertable);
                writeInd = 0;
            }

            // Then write
            // Subtract sizes from memory count
            this->buf.subCount(sizeof(t)); // account for pointer
            this->buf.subCount(t->count()); // account for pointee

            // todo: check return value
            t->commit(ncvd, writeInd);

            // increment index
            this->varWriteInds[varId]++;

            this->transactionQueue.pop();
        }
    }

    void OGR_NCScribe::log_transaction()
    {
        if(wl.logIsNull())
            wl.startLog();

        while(!transactionQueue.empty())
        {
            wl.push(transactionQueue.front());
            this->transactionQueue.pop();
        }
    }

    // WBufferManager
    bool WBufferManager::isOverQuota()
    {
        unsigned long long sum = 0;
        for(size_t s = 0; s < bufs.size(); s++)
        {
           WBuffer& b = *(bufs[s]);
           sum += b.getUsage();
        }

        return sum > this->buffer_soft_limit;
    }

    // Transactions
    void OGR_SGFS_NC_Char_Transaction::appendToLog(FILE * f)
    {
        int vid = OGR_SGFS_Transaction::getVarId();
        int type = NC_CHAR;
        int8_t OP = 0;
        size_t DATA_SIZE = char_rep.length(); 

        fwrite(&vid, sizeof(int), 1, f); // write varID data
        fwrite(&type, sizeof(int), 1, f); // write NC type
        fwrite(&OP, sizeof(int8_t), 1, f); // write "OP" flag
        fwrite(&DATA_SIZE, sizeof(size_t), 1, f); // write length
        fwrite(char_rep.c_str(), sizeof(char), DATA_SIZE, f); // write data
    }

#ifdef NETCDF_HAS_NC4
    void OGR_SGFS_NC_String_Transaction::appendToLog(FILE * f)
    {
        int vid = OGR_SGFS_Transaction::getVarId();
        int type = NC_STRING;
        size_t DATA_SIZE = char_rep.length(); 

        fwrite(&vid, sizeof(int), 1, f); // write varID data
        fwrite(&type, sizeof(int), 1, f); // write NC type
        fwrite(&DATA_SIZE, sizeof(size_t), 1, f); // write length
        fwrite(char_rep.c_str(), sizeof(char), DATA_SIZE, f); // write data
    }
#endif

    void OGR_SGFS_NC_CharA_Transaction::appendToLog(FILE * f)
    {
        int vid = OGR_SGFS_Transaction::getVarId();
        int type = NC_CHAR;
        int8_t OP = 1;
        size_t DATA_SIZE = char_rep.length(); 

        fwrite(&vid, sizeof(int), 1, f); // write varID data
        fwrite(&type, sizeof(int), 1, f); // write NC type
        fwrite(&OP, sizeof(int8_t), 1, f); // write "OP" flag
        fwrite(&DATA_SIZE, sizeof(size_t), 1, f); // write length
        fwrite((this->counts) + 1, sizeof(size_t), 1, f);
        fwrite(char_rep.c_str(), sizeof(char), DATA_SIZE, f); // write data
    }

    // WTransactionLog
    WTransactionLog::WTransactionLog(std::shared_ptr<std::string> logName) :
        wlogName(*logName),
        log(nullptr)
    {
    }

    void WTransactionLog::startLog()
    {
        log = fopen(wlogName.c_str(), "w");
    }

    void WTransactionLog::startRead()
    {
        if(log == nullptr)
            return;

        fclose(this->log);
        this->log = fopen(wlogName.c_str(), "r");
    }

    void WTransactionLog::push(std::shared_ptr<OGR_SGFS_Transaction> t)
    {
        t->appendToLog(this->log); 
    }

    std::shared_ptr<OGR_SGFS_Transaction> WTransactionLog::pop()
    {
         if(log == nullptr)
             return std::shared_ptr<OGR_SGFS_Transaction>(nullptr);

         int varId;
         nc_type ntype;
         int itemsread; 
         itemsread = fread(&varId, sizeof(int), 1, log);
         itemsread &= fread(&ntype, sizeof(nc_type), 1, log);

         // If one of the two reads failed, then return nullptr
         if(!itemsread) return std::shared_ptr<OGR_SGFS_Transaction>(nullptr);

         // If not, continue on and parse additional fields
         switch(ntype)
         {
             // NC-3 Primitives
             case NC_BYTE:
                 return genericLogDataRead<OGR_SGFS_NC_Byte_Transaction, signed char>(varId, log);
             case NC_SHORT:
                 return genericLogDataRead<OGR_SGFS_NC_Short_Transaction, short>(varId, log);
             case NC_INT:
                 return genericLogDataRead<OGR_SGFS_NC_Int_Transaction, int>(varId, log);
             case NC_FLOAT:
                 return genericLogDataRead<OGR_SGFS_NC_Float_Transaction, float>(varId, log);
             case NC_DOUBLE:
                 return genericLogDataRead<OGR_SGFS_NC_Double_Transaction, double>(varId, log);
#ifndef NETCDF_HAS_NC4
             case NC_UBYTE:
                 return genericLogDataRead<OGR_SGFS_NC_UByte_Transaction, unsigned char>(varId, log);
             case NC_USHORT:
                 return genericLogDataRead<OGR_SGFS_NC_UShort_Transaction, unsigned short>(varId, log);
             case NC_UINT:
                 return genericLogDataRead<OGR_SGFS_NC_UInt_Transaction, unsigned int>(varId, log);
             case NC_INT64:
                 return genericLogDataRead<OGR_SGFS_NC_Int64_Transaction, long long>(varId, log);
             case NC_UINT64:
                 return genericLogDataRead<OGR_SGFS_NC_UInt64_Transaction, unsigned long long>(varId, log);
#endif
             case NC_CHAR:
             {
                 int readcheck; // 0 means at least one read 0 bytes

                 // Check what type of OP is requested 
                 int8_t op = 0;
                 readcheck = fread(&op, sizeof(int8_t), 1, log);
                 if(!readcheck) return MTPtr(); // read failure

                 size_t strsize;

                 // get how much data to read
                 readcheck = fread(&strsize, sizeof(size_t), 1, log);
                 if(!readcheck) return MTPtr(); // read failure

                 std::unique_ptr<char, std::default_delete<char[]>> data(new char[strsize + 1]);
                 memset(data.get(), 0, strsize + 1);

                 // read that data and return it
                 readcheck = fread(data.get(), sizeof(char), strsize, log);
                 if(!readcheck) return MTPtr(); // read failure

                 // case: its a standard CHAR op
                 if(!op)
                 {
                     return MTPtr(new OGR_SGFS_NC_Char_Transaction(varId, data.get()));  // data is copied so okay!
                 }

                 // case: its a CHARA op, additional processing
                 else
                 {
                     size_t fullSizeCount;
                     if(!fread(&fullSizeCount, sizeof(size_t), 1, log))
                     {
                         return MTPtr();
                     }

                     return MTPtr(new OGR_SGFS_NC_CharA_Transaction(varId, data.get(), fullSizeCount));
                 } 

             }

#ifdef NETCDF_HAS_NC4
             case NC_STRING:
             {
                 int readcheck; // 0 means at least one read 0 bytes

                 size_t strsize;

                 // get how much data to read
                 readcheck = fread(&strsize, sizeof(size_t), 1, log);

                 if(!readcheck) return MTPtr(); // read failure

                 std::unique_ptr<char, std::default_delete<char[]>> data(new char[strsize + 1]);
                 memset(data.get(), 0, strsize + 1);

                 // read that data and return it
                 readcheck = fread(data.get(), sizeof(char), strsize, log);

                 if(!readcheck)
                     return MTPtr(); // read failure

                 return MTPtr(new OGR_SGFS_NC_String_Transaction(varId, data.get()));  // data is copied so okay!
             }
#endif

             default:
                 // Unsupported type
                 return MTPtr(); 
         } 
    }

    WTransactionLog::~WTransactionLog()
    {
        if(log != nullptr)
        {
            fclose(log);
            VSIUnlink(this->wlogName.c_str());
        }
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
        if (geometry_type == MULTILINE || geometry_type == MULTIPOLYGON || geometry_type == POLYGON)
        {
            std::string pnc_atr_str = name + "_part_node_count";

            err_code = nc_put_att_text(ncID, write_var_id, CF_SG_PART_NODE_COUNT, pnc_atr_str.size(), pnc_atr_str.c_str());

            NCDF_ERR(err_code);
            if(err_code != NC_NOERR)
            {
                throw SGWriter_Exception_NCWriteFailure(name.c_str(), CF_SG_PART_NODE_COUNT, "attribute in geometry_container");
            }
        }

        /* Interior Ring Attribute
         * (only needed potentially for MULTIPOLYGON and POLYGON)
         */

        if (geometry_type == MULTIPOLYGON || geometry_type == POLYGON)
        {
            std::string ir_atr_str = name + "_interior_ring";

            err_code = nc_put_att_text(ncID, write_var_id, CF_SG_INTERIOR_RING, ir_atr_str.size(), ir_atr_str.c_str());
            NCDF_ERR(err_code);
            if(err_code != NC_NOERR)
            {
                throw nccfdriver::SGWriter_Exception_NCWriteFailure(name.c_str(), CF_SG_INTERIOR_RING, "attribute in geometry_container");
            }
         }

        return write_var_id;
    }
}
