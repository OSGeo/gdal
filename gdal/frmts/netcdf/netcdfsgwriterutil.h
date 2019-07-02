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
#ifndef __NETCDFSGWRITERUTIL_H__
#define __NETCDFSGWRITERUTIL_H__
#include <queue>
#include <vector>
#include "ogr_core.h"
#include "ogrsf_frmts.h"
#include "netcdfsg.h"
#include "netcdflayersg.h"

namespace nccfdriver
{

    /* OGR_SGeometry_Feature
     * Constructs over a OGRFeature
     * gives some basic information about that SGeometry Feature such as
     * Hold references... limited to scope of its references
     * - what's its geometry type
     * - how much total points it has
     * - how many parts it has
     * - a vector of counts of points for each part
     */
    class SGeometry_Feature
    {
        bool hasInteriorRing;
        OGRGeometry * geometry_ref;
        geom_t type;
        size_t total_point_count;
        size_t total_part_count;
        std::vector<size_t> ppart_node_count;
        std::vector<bool> part_at_ind_interior; // for use with Multipolygons ONLY
        OGRPoint pt_buffer;

        public:
            geom_t getType() { return this->type; }
            size_t getTotalNodeCount() { return this->total_point_count; }
            size_t getTotalPartCount() { return this->total_part_count;  }
            std::vector<size_t> & getPerPartNodeCount() { return this->ppart_node_count; }
            OGRPoint& getPoint(size_t part_no, int point_index);
            explicit SGeometry_Feature(OGRFeature&);
            bool getHasInteriorRing() { return this->hasInteriorRing; }
            bool IsPartAtIndInteriorRing(size_t ind) { return this->part_at_ind_interior[ind]; } // ONLY used for Multipolygon
    };

    /* A buffer with **approximate** maximum size of 1 / 10th the size of available memory
     * Assumes small features. A single feature would not be subject to this limitation.
     * Attempts to perform memory blocking to prevent unneccessary I/O from continuous dimension deformation
     */
    class SGeometry_Layer_WBuffer
    {
        unsigned long long used_mem;
        unsigned long long mem_limit;
        std::queue<int> pnc; // part node counts
        std::queue<int> ncounts; // node counts
        std::queue<bool> interior_rings; // interior rings
        std::queue<double> x; // x coords
        std::queue<double> y; // y coords
        std::queue<double> z; // z coords

        public:
            bool isOverQuota() { return used_mem > mem_limit; }
            void addXCoord(double xp) { this->x.push(xp); used_mem += sizeof(xp); }
            void addYCoord(double yp) { this->y.push(yp); used_mem += sizeof(yp); }
            void addZCoord(double zp) { this->z.push(zp); used_mem += sizeof(zp); }
            void addPNC(int pncp) { this->pnc.push(pncp); used_mem += sizeof(pncp); }
            void addNCOUNT(int ncount) { this->ncounts.push(ncount); used_mem += sizeof(ncount); }
            void addIRing(bool iring) { this->interior_rings.push(iring); used_mem += sizeof(iring); }
            SGeometry_Layer_WBuffer() : used_mem(0), mem_limit(CPLGetUsablePhysicalRAM() / 10) {}
            bool XCoordEmpty() { return x.empty(); }
            bool YCoordEmpty() { return y.empty(); }
            bool ZCoordEmpty() { return z.empty(); }
            bool PNCEmpty() { return pnc.empty(); }
            bool NCOUNTEmpty() { return ncounts.empty(); }
            bool IRingEmpty() { return interior_rings.empty(); }
            double XCoordDequeue() { double ret = x.front(); x.pop(); used_mem -= sizeof(ret); return(ret); }
            double YCoordDequeue() { double ret = y.front(); y.pop(); used_mem -= sizeof(ret); return(ret); }
            double ZCoordDequeue() { double ret = z.front(); z.pop(); used_mem -= sizeof(ret); return(ret); }
            int PNCDequeue() { int ret = pnc.front(); pnc.pop(); used_mem -= sizeof(ret); return(ret); }
            int NCOUNTDequeue() { int ret = ncounts.front(); ncounts.pop(); used_mem -= sizeof(ret); return(ret); }
            bool IRingDequeue() { bool ret = interior_rings.front(); interior_rings.pop(); used_mem -= sizeof(ret); return(ret); }
            size_t getXCoordCount() { return this->x.size(); }
            size_t getYCoordCount() { return this->y.size(); }
            size_t getZCoordCount() { return this->z.size(); }
            size_t getNCOUNTCount() { return this->ncounts.size(); }
            size_t getPNCCount() { return this->pnc.size(); }
            size_t getIRingVarEntryCount() { return this-> interior_rings.size(); } // DOESN'T get the amount of "interior_rings" gets the amount of true / false interior ring entries
            void cpyNCOUNT_into_PNC();
            void flushPNCBuffer() { this->pnc = std::queue<int>(); }
            void setBufSize(unsigned long long new_limit) { this->mem_limit = new_limit; }
    };

    /* OGR_SGeometry_Scribe
     * Takes a SGeometry_Feature and given a target geometry container ID it will write the feature
     * to a given netCDF dataset in CF-1.8 compliant formatting.
     * Any needed variables will automatically be defined, and dimensions will be automatically grown corresponding with need
     *
     */
    class OGR_SGeometry_Scribe
    {
        int ncID = 0;
        SGeometry_Layer_WBuffer wbuf;
        geom_t writableType = NONE;
        std::string containerVarName;
        int containerVarID = INVALID_VAR_ID;
        bool interiorRingDetected = false; // flips on when an interior ring polygon has been detected
        std::vector<int> node_coordinates_varIDs;// ids in X, Y (and then possibly Z) order
        int node_coordinates_dimID = INVALID_DIM_ID; // dim of all node_coordinates
        int node_count_dimID = INVALID_DIM_ID; // node count dim
        int node_count_varID = INVALID_DIM_ID;
        int pnc_dimID = INVALID_DIM_ID; // part node count dim AND interior ring dim
        int pnc_varID = INVALID_VAR_ID;
        int intring_varID = INVALID_VAR_ID;
        size_t next_write_pos_node_coord = 0;
        size_t next_write_pos_node_count = 0;
        size_t next_write_pos_pnc = 0;

        public:
            geom_t getWritableType() { return this->writableType; }
            void writeSGeometryFeature(SGeometry_Feature& ft);
            OGR_SGeometry_Scribe() {}
            OGR_SGeometry_Scribe(int ncID, int containerVarID, geom_t geo_t, unsigned long long bufsize);
            int get_containerID() { return this->containerVarID; }
            void update_ncID(int newID) { this->ncID = newID; }
            int get_node_count_dimID() { return this->node_count_dimID; }
            int get_node_coord_dimID() { return this->node_coordinates_dimID; }
            int get_pnc_dimID() { return this->pnc_dimID; }
            std::vector<int>& get_nodeCoordVarIDs() { return this->node_coordinates_varIDs; }
            size_t get_next_write_pos_node_coord() { return this->next_write_pos_node_coord; }
            size_t get_next_write_pos_node_count() { return this->next_write_pos_node_count; }
            size_t get_next_write_pos_pnc() { return this->next_write_pos_pnc; }
            void redef_interior_ring(); // adds an interior ring attribute and to the target geometry container and corresponding variable
            void redef_pnc(); // adds a part node count attribute to the target geometry container and corresponding variable
            bool getInteriorRingDetected() { return this->interiorRingDetected; }
            void commit_transaction(); // commit all writes to the netCDF (subject to fs stipulations)
            bool bufferQuotaReached() { return wbuf.isOverQuota(); } // is the wbuf over the quota and ready to write?
            size_t getXCBufLength() { return wbuf.getXCoordCount(); }
            size_t getYCBufLength() { return wbuf.getYCoordCount(); }
            size_t getZCBufLength() { return wbuf.getZCoordCount(); }
            size_t getNCOUNTBufLength() { return wbuf.getNCOUNTCount(); }
            size_t getPNCBufLength() { return wbuf.getPNCCount(); }
            ~OGR_SGeometry_Scribe() { this->commit_transaction(); }
    };

    // Exception Classes
    class SGWriter_Exception : public SG_Exception
    {
        public:
            const char * get_err_msg() override { return "A general error occurred when writing a netCDF dataset"; }
    };

    class SGWriter_Exception_NCWriteFailure : public SGWriter_Exception
    {
        std::string msg;

        public:
            const char * get_err_msg() override { return this->msg.c_str(); }
            SGWriter_Exception_NCWriteFailure(const char * layer_name, const char * failure_name,
                const char * failure_type);
    };

    class SGWriter_Exception_NCInqFailure : public SGWriter_Exception
    {
        std::string msg;

        public:
            const char * get_err_msg() override { return this->msg.c_str(); }
            SGWriter_Exception_NCInqFailure(const char * layer_name, const char * failure_name,
                const char * failure_type);
    };

    class SGWriter_Exception_NCDefFailure : public SGWriter_Exception
    {
        std::string msg;

        public:
            const char * get_err_msg() override { return this->msg.c_str(); }
            SGWriter_Exception_NCDefFailure(const char * layer_name, const char * failure_name,
                const char * failure_type);
    };

    class SGWriter_Exception_EmptyGeometry : public SGWriter_Exception
    {
        std::string msg;

        public:
            const char * get_err_msg() override { return this->msg.c_str(); }
            SGWriter_Exception_EmptyGeometry() : msg("An empty geometry was detected when writing a netCDF file. Empty geometries are not allowed.") {}
    };

    class SGWriter_Exception_RingOOB: public SGWriter_Exception
    {
        std::string msg;

        public:
            const char * get_err_msg() override { return this->msg.c_str(); }
            SGWriter_Exception_RingOOB() : msg("An attempt was made to read a polygon ring that does not exist.") {}
    };

    // Functions that interface with netCDF, for writing

    /* std::vector<..> writeGeometryContainer(...)
     * Writes a geometry container of a given geometry type given the following arguments:
     * int ncID - ncid as used in netcdf.h, group or file id
     * std::string name - what to name this container
     * geom_t geometry_type - the geometry type of the container
     * std::vector<..> node_coordinate_names - variable names corresponding to each axis
     * Only writes attributes that are for sure required. i.e. does NOT required interior ring for anything or part node count for Polygons
     *
     * Returns: geometry container variable ID
     */
    int write_Geometry_Container
        (int ncID, const std::string& name, geom_t geometry_type, const std::vector<std::string> & node_coordinate_names);
}

#endif
