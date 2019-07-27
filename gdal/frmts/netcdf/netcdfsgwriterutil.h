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
#include <cstdio>
#include <queue>
#include <vector>
#include "ogr_core.h"
#include "ogrsf_frmts.h"
#include "netcdflayersg.h"
#include "netcdfsg.h"
#include "netcdfvirtual.h"

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

    /* A memory buffer with a soft limit
     * Has basic capability of over quota checking, and memory counting
     */
    class WBuffer
    {
        unsigned long long used_mem = 0;

        public: 
            /* addCount(...)
             * Takes in a size, and directly adds that size to memory count
             */
             void addCount(unsigned long long memuse) { this->used_mem += memuse; }

            /* subCount(...)
             * Directly subtracts the specified size from used_mem
             */
             void subCount(unsigned long long memfree) { this->used_mem -= memfree; }
             unsigned long long& getUsage() { return used_mem; }

             void reset() { this->used_mem = 0; }

             WBuffer() {}
    };

    class ncLayer_SG_Metadata
    {
        int & ncID;
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
            int get_containerID() { return this->containerVarID; }
            int get_node_count_dimID() { return this->node_count_dimID; }
            int get_node_coord_dimID() { return this->node_coordinates_dimID; }
            int get_pnc_dimID() { return this->pnc_dimID; }
            std::vector<int>& get_nodeCoordVarIDs() { return this->node_coordinates_varIDs; }
            size_t get_next_write_pos_node_coord() { return this->next_write_pos_node_coord; }
            size_t get_next_write_pos_node_count() { return this->next_write_pos_node_count; }
            size_t get_next_write_pos_pnc() { return this->next_write_pos_pnc; }
            bool getInteriorRingDetected() { return this->interiorRingDetected; }

            ncLayer_SG_Metadata(int & i_ncID, int containerVID, geom_t geo, netCDFVID& ncdf);
    };

    /* OGR_SGeometry_Scribe
     * Takes a SGeometry_Feature and given a target geometry container ID it will write the feature
     * to a given netCDF dataset in CF-1.8 compliant formatting.
     * Any needed variables will automatically be defined, and dimensions will be automatically grown corresponding with need
     *
     */
    class OGR_SGeometry_Scribe : public WBuffer
    {
        bool writing_to_NC4 = false;
        int ncID = 0;
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

        std::queue<int> pnc; // part node counts
        std::queue<int> ncounts; // node counts
        std::queue<bool> interior_rings; // interior rings
        std::queue<double> xC; // x coords
        std::queue<double> yC; // y coords
        std::queue<double> zC; // z coords

        void addXCoord(double xp) { this->xC.push(xp); WBuffer::addCount(sizeof(xp)); }
        void addYCoord(double yp) { this->yC.push(yp); WBuffer::addCount(sizeof(yp)); }
        void addZCoord(double zp) { this->zC.push(zp); WBuffer::addCount(sizeof(zp)); }
        void addPNC(int pncp) { this->pnc.push(pncp); WBuffer::addCount(sizeof(pncp)); }
        void addNCOUNT(int ncount) { this->ncounts.push(ncount); WBuffer::addCount(sizeof(ncount)); }
        void addIRing(bool iring) { this->interior_rings.push(iring); WBuffer::addCount(sizeof(iring)); }
        double XCoordDequeue() { double ret = xC.front(); xC.pop(); WBuffer::subCount(sizeof(ret)); return(ret); }
        double YCoordDequeue() { double ret = yC.front(); yC.pop(); WBuffer::subCount(sizeof(ret)); return(ret); }
        double ZCoordDequeue() { double ret = zC.front(); zC.pop(); WBuffer::subCount(sizeof(ret)); return(ret); }
        int PNCDequeue() { int ret = pnc.front(); pnc.pop(); WBuffer::subCount(sizeof(ret)); return(ret); }
        int NCOUNTDequeue() { int ret = ncounts.front(); ncounts.pop(); WBuffer::subCount(sizeof(ret)); return(ret); }
        bool IRingDequeue() { bool ret = interior_rings.front(); interior_rings.pop(); WBuffer::subCount(sizeof(ret)); return(ret); }
        void cpyNCOUNT_into_PNC();
        void flushPNCBuffer() { this->pnc = std::queue<int>(); }
        size_t getIRingVarEntryCount() { return this-> interior_rings.size(); } // DOESN'T get the amount of "interior_rings" gets the amount of true / false interior ring entries

        public:
            geom_t getWritableType() { return this->writableType; }
            void writeSGeometryFeature(SGeometry_Feature& ft);
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

            size_t getXCBufLength() { return this->xC.size(); }
            size_t getYCBufLength() { return this->yC.size(); }
            size_t getZCBufLength() { return this->zC.size(); }
            size_t getNCOUNTBufLength() { return this->ncounts.size(); }
            size_t getPNCBufLength() { return this->pnc.size(); }

            OGR_SGeometry_Scribe() {}
            OGR_SGeometry_Scribe(int ncID, int containerVarID, geom_t geo_t, bool isTrueNC4);
            ~OGR_SGeometry_Scribe() { this->commit_transaction(); }
    };

    /* OGR_SGFS_Transaction
     * Abstract class for a commitable transaction
     *
     */
    class OGR_SGFS_Transaction
    {
        int varId = INVALID_VAR_ID;

        public:
            /* int commit(...);
             * Arguments: int ncid, the dataset to write to
             *            int write_loc, the index in which to write to
             * Implementation: should write the transaction to netCDF file
             *
             * Returns: error code from transaction
             */
            virtual int commit(int ncid, size_t write_loc) = 0;

            /* unsigned long long count(...)
             * Implementation: supposed to return an approximate count of memory usage
             * Most classes will implement with sizeof(*this), except if otherwise uncounted for dynamic allocation is involved.
             */
            virtual unsigned long long count() = 0;

            /* appendToLog
             * Implementation - given a file pointer, a transaction will be written to that log file in the format:
             * -
             * transactionVarId - sizeof(int) bytes
             * NC_TYPE - sizeof(int) bytes
             * (nc_char only) OP - 1 byte (0 if does not require COUNT or non-zero i.e. 1 if does)
             * (nc_char only): SIZE of data - sizeof(size_t) bytes
             * (nc_char && OP != 0 only) COUNT - sizeof(size_t) bytes the second dimension of the "count" arg
             * DATA - size depends on NC_TYPE
             */
            virtual void appendToLog(FILE*) = 0;

            /* ~OGR_SGFS_Transaction()
             * Empty. Simply here to stop the compiler from complaining...
             */
            virtual ~OGR_SGFS_Transaction() {}


            /* OGR_SGFS_Transaction()
             * Empty. Simply here to stop one of the CI machines from complaining...
             */
            OGR_SGFS_Transaction() {}

            /* void getVarId(...);
             * Gets the var in which to commit the transaction to.
             */
            int getVarId() { return this->varId; }

            /* void setVarId(...);
             * Sets the var in which to commit the transaction to.
             */
            void setVarId(int vId) { this->varId = vId; }

    };

    typedef std::shared_ptr<OGR_SGFS_Transaction> MTPtr; // a.k.a Managed Transaction Ptr

    template<class T_c_type, nc_type T_nc_type> void genericLogAppend(T_c_type r, int vId, FILE * f)
    {
        T_c_type rep = r;
        int varId = vId;
        int type = T_nc_type;
        fwrite(&varId, sizeof(int), 1, f); // write varID data
        fwrite(&type, sizeof(int), 1, f); // write NC type
        fwrite(&rep, sizeof(T_c_type), 1, f); // write data
    }

    template<class T_c_type, class T_r_type> std::shared_ptr<OGR_SGFS_Transaction> genericLogDataRead(int varId, FILE* f)
    {
        T_r_type data;
        if(!fread(&data, sizeof(data), 1, f))
        {
             return std::shared_ptr<OGR_SGFS_Transaction>(nullptr); // invalid read case
        }
        return std::shared_ptr<OGR_SGFS_Transaction>(new T_c_type(varId, data));
    }

    /* OGR_SGFS_NC_Char_Transaction 
     * Writes to an NC_CHAR variable
     */
    class OGR_SGFS_NC_Char_Transaction : public OGR_SGFS_Transaction
    {
        std::string char_rep;

        public:
            int commit(int ncid, size_t write_loc) override { return nc_put_var1_text(ncid, OGR_SGFS_Transaction::getVarId(), &write_loc, char_rep.c_str()); }
            unsigned long long count() override { return char_rep.size() + sizeof(*this); } // account for actual character representation, this class
            void appendToLog(FILE* f) override;
            OGR_SGFS_NC_Char_Transaction(int i_varId, const char* pszVal) : 
               char_rep(pszVal)
            {
                OGR_SGFS_Transaction::setVarId(i_varId);
            }
    };

    /* OGR_SGFS_NC_CharA_Transaction
     * Writes to an NC_CHAR variable, using vara instead of var1
     * Used to store 2D character array values, specifically
     */
    class OGR_SGFS_NC_CharA_Transaction : public OGR_SGFS_Transaction
    {
        std::string char_rep;
        size_t counts[2];

        public:
            int commit(int ncid, size_t write_loc) override { size_t ind[2] = {write_loc, 0}; return nc_put_vara_text(ncid, OGR_SGFS_Transaction::getVarId(), ind, counts, char_rep.c_str()); }
            unsigned long long count() override { return char_rep.size() + sizeof(*this); } // account for actual character representation, this class
            void appendToLog(FILE* f) override;
            OGR_SGFS_NC_CharA_Transaction(int i_varId, const char* pszVal, size_t str_width) : 
               char_rep(pszVal),
               counts{1, str_width}
            { 
                OGR_SGFS_Transaction::setVarId(i_varId);
            }
    };

    class OGR_SGFS_NC_Byte_Transaction: public OGR_SGFS_Transaction
    {
        signed char schar_rep;

        public:
            int commit(int ncid, size_t write_loc) override { return nc_put_var1_schar(ncid, OGR_SGFS_Transaction::getVarId(), &write_loc, &schar_rep); }
            unsigned long long count() override { return sizeof(*this); }
            void appendToLog(FILE* f) override { genericLogAppend<signed char, NC_BYTE>(schar_rep, OGR_SGFS_Transaction::getVarId(), f); }
            OGR_SGFS_NC_Byte_Transaction(int i_varId, signed char scin) : 
               schar_rep(scin)
            {
                OGR_SGFS_Transaction::setVarId(i_varId);
            }
    };

    /* OGR_SGFS_NC_Short_Transaction 
     * Writes to an NC_SHORT variable
     */
    class OGR_SGFS_NC_Short_Transaction: public OGR_SGFS_Transaction
    {
        short rep;

        public:
            int commit(int ncid, size_t write_loc) override { return nc_put_var1_short(ncid, OGR_SGFS_Transaction::getVarId(), &write_loc, &rep); }
            unsigned long long count() override { return sizeof(*this); }
            void appendToLog(FILE* f) override { genericLogAppend<short, NC_SHORT>(rep, OGR_SGFS_Transaction::getVarId(), f); }
            OGR_SGFS_NC_Short_Transaction(int i_varId, short shin) : 
               rep(shin)
            {
                OGR_SGFS_Transaction::setVarId(i_varId);
            }
    };

    /* OGR_SGFS_NC_Int_Transaction
     * Writes to an NC_INT variable
     */
    class OGR_SGFS_NC_Int_Transaction: public OGR_SGFS_Transaction
    {
        int rep;

        public:
            int commit(int ncid, size_t write_loc) override { return nc_put_var1_int(ncid, OGR_SGFS_Transaction::getVarId(), &write_loc, &rep); }
            unsigned long long count() override { return sizeof(*this); }
            void appendToLog(FILE* f) override { genericLogAppend<int, NC_INT>(rep, OGR_SGFS_Transaction::getVarId(), f); }
            OGR_SGFS_NC_Int_Transaction(int i_varId, int iin) : 
               rep(iin)
            {
                OGR_SGFS_Transaction::setVarId(i_varId);
            }
    };

    
    /* OGR_SGFS_NC_Float_Transaction
     * Writes to an NC_FLOAT variable
     */
    class OGR_SGFS_NC_Float_Transaction: public OGR_SGFS_Transaction
    {
        float rep;

        public:
            int commit(int ncid, size_t write_loc) override { return nc_put_var1_float(ncid, OGR_SGFS_Transaction::getVarId(), &write_loc, &rep); }
            unsigned long long count() override { return sizeof(*this); }
            void appendToLog(FILE* f) override { genericLogAppend<float, NC_FLOAT>(rep, OGR_SGFS_Transaction::getVarId(), f); }
            OGR_SGFS_NC_Float_Transaction(int i_varId, float fin) : 
               rep(fin)
            {
                OGR_SGFS_Transaction::setVarId(i_varId);
            }
    };

    /* OGR_SGFS_NC_Double_Transaction
     * Writes to an NC_DOUBLE variable
     */
    class OGR_SGFS_NC_Double_Transaction: public OGR_SGFS_Transaction
    {
        double rep;

        public:
            int commit(int ncid, size_t write_loc) override { return nc_put_var1_double(ncid, OGR_SGFS_Transaction::getVarId(), &write_loc, &rep); }
            unsigned long long count() override { return sizeof(*this); }
            void appendToLog(FILE* f) override { genericLogAppend<double, NC_DOUBLE>(rep, OGR_SGFS_Transaction::getVarId(), f); }
            OGR_SGFS_NC_Double_Transaction(int i_varId, double fin) : 
               rep(fin)
            {
                OGR_SGFS_Transaction::setVarId(i_varId);
            }
    };

    /* OGR_SGeometry_Field_Scribe
     * Buffers several netCDF transactions in memory to reduce the need of dimension resizing
     *
     */
    class OGR_SGeometry_Field_Scribe
    {
        const int & ncid;
        int ncID() { return this->ncid; } // creates copy of const reference ncid
        int recordDimID = INVALID_DIM_ID;
        WBuffer buf;

        std::queue<std::shared_ptr<OGR_SGFS_Transaction>> transactionQueue;
        std::map<int, size_t> varWriteInds;
        std::map<int, size_t> varMaxInds;
        size_t recordLength = 1;

        public:
           /* size_t get_Record_Length()
            * Return the current length of the record dimension
            */
           size_t getRecordLength() { return this->recordLength; }

            /* void commit_transaction()
             * Replays all transactions to disk (according to fs stipulations)
             */
           void commit_transaction();

           /* void enqueue_transaction()
            * Add a transaction to perform
            * Once a transaction is enqueued, it will only be dequeued on flush
            */
           void enqueue_transaction(std::shared_ptr<OGR_SGFS_Transaction> transactionAdd);

           WBuffer& getMemBuffer() { return buf; }

           /* int RecordDimID()
            * Return record dimension ID that this variable writes over
            */
           int RecordDimID() { return this->recordDimID; }

           /* void setLayerRecord()
            * Sets the record of a new layer. If there are pending writes for a previous layer, they will be commited before the new record is set
            * The new record passed in is simply the record dim ID.
            */ 
           void setLayerRecord(int dimid) { this->commit_transaction(); this->recordDimID = dimid; }

           /* OGR_SGeometry_Field_Scribe()
            * Constructs a Field Scribe over a dataset
            */
           explicit OGR_SGeometry_Field_Scribe(const int& in_ncid) :
               ncid(in_ncid),
               buf()
           {}

    };

    /* WBufferManager
     * -
     * Simply takes a collection of buffers in and a quota limit and sums all the usages up
     * to establish if buffers are over the soft limit (collectively)
     *
     * The buffers added, however, are not memory managed by WBufferManager
     */
    class WBufferManager
    {
        unsigned long long buffer_soft_limit = 0;
        std::vector<WBuffer*> bufs;

        public:
            bool isOverQuota();
            void adjustLimit(unsigned long long lim) { this->buffer_soft_limit = lim; }
            void addBuffer(WBuffer* b) { this->bufs.push_back(b); }
            explicit WBufferManager(unsigned long long lim) : buffer_soft_limit(lim){ }
    };

    /* WTransactionLog
     * -
     * A temporary file which contains transactions to be written to a netCDF file.
     * Once the transaction log is created it is set on write mode, it can only be read to after startRead() is called
     */
    class WTransactionLog
    {
        bool readMode;
        std::string wlogName; // name of the temporary file, should be unique
        FILE* log;

        WTransactionLog(WTransactionLog&); // avoid possible undefined behavior
        WTransactionLog operator=(const WTransactionLog&);

        public:
            // write mode
            void startRead();
            void push(std::shared_ptr<OGR_SGFS_Transaction>);

            // read mode
            std::shared_ptr<OGR_SGFS_Transaction> pop();  // to test for EOF, test to see if pointer returned is null ptr

            // construction, destruction
            WTransactionLog(std::string& logName);
            ~WTransactionLog();
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
