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
#include <typeinfo>
#include <vector>
#include "cpl_vsi.h"
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
             void addCount(unsigned long long memuse);

            /* subCount(...)
             * Directly subtracts the specified size from used_mem
             */
             void subCount(unsigned long long memfree);
             unsigned long long& getUsage() { return used_mem; }

             void reset() { this->used_mem = 0; }

             WBuffer() {}
    };



    /* OGR_SGFS_Transaction
     * Abstract class for a committable transaction
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
             */
            virtual void commit(netCDFVID& n, size_t write_loc) = 0;

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
             * DATA - size depends on NC_TYPE
             */
            virtual void appendToLog(VSILFILE*) = 0;

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

            /* nc_type getType
             * Returns the type of transaction being saved
             */
            virtual nc_type getType() = 0;

            /* void setVarId(...);
             * Sets the var in which to commit the transaction to.
             */
            void setVarId(int vId) { this->varId = vId; }

    };

    typedef std::map<int, void*> NCWMap;
    typedef std::pair<int, void*> NCWEntry; // NC Writer Entry
    typedef std::unique_ptr<OGR_SGFS_Transaction> MTPtr; // a.k.a Managed Transaction Ptr

    template<class T_c_type, nc_type T_nc_type> void genericLogAppend(T_c_type r, int vId, VSILFILE* f)
    {
        T_c_type rep = r;
        int varId = vId;
        int type = T_nc_type;
        VSIFWriteL(&varId, sizeof(int), 1, f); // write varID data
        VSIFWriteL(&type, sizeof(int), 1, f); // write NC type
        VSIFWriteL(&rep, sizeof(T_c_type), 1, f); // write data
    }

    template<class T_c_type, class T_r_type> MTPtr genericLogDataRead(int varId, VSILFILE* f)
    {
        T_r_type data;
        if(!VSIFReadL(&data, sizeof(T_r_type), 1, f))
        {
             return MTPtr(nullptr); // invalid read case
        }
        return MTPtr(new T_c_type(varId, data));
    }

    /* OGR_SGFS_NC_Char_Transaction
     * Writes to an NC_CHAR variable
     */
    class OGR_SGFS_NC_Char_Transaction : public OGR_SGFS_Transaction
    {
        std::string char_rep;

        public:
            void commit(netCDFVID& n, size_t write_loc) override { n.nc_put_vvar1_text(OGR_SGFS_Transaction::getVarId(), &write_loc, char_rep.c_str()); }
            unsigned long long count() override { return char_rep.size() + sizeof(*this); } // account for actual character representation, this class
            void appendToLog(VSILFILE* f) override;
            nc_type getType() override { return NC_CHAR; }
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
            void commit(netCDFVID& n, size_t write_loc) override { size_t ind[2] = {write_loc, 0}; n.nc_put_vvara_text(OGR_SGFS_Transaction::getVarId(), ind, counts, char_rep.c_str()); }
            unsigned long long count() override { return char_rep.size() + sizeof(*this); } // account for actual character representation, this class
            void appendToLog(VSILFILE* f) override;
            nc_type getType() override { return NC_CHAR; }
            OGR_SGFS_NC_CharA_Transaction(int i_varId, const char* pszVal) :
               char_rep(pszVal),
               counts{1, char_rep.length()}
            {
                OGR_SGFS_Transaction::setVarId(i_varId);
            }
    };

    template <class VClass, nc_type ntype> class OGR_SGFS_NC_Transaction_Generic : public OGR_SGFS_Transaction
    {
        VClass rep;

        public:
            void commit(netCDFVID& n, size_t write_loc) override
            {
                n.nc_put_vvar_generic<VClass>(OGR_SGFS_Transaction::getVarId(), &write_loc, &rep);
            }

            unsigned long long count() override { return sizeof(*this); }
            void appendToLog(VSILFILE* f) override
            {
                genericLogAppend<VClass, ntype>(rep, OGR_SGFS_Transaction::getVarId(), f);
            }

            OGR_SGFS_NC_Transaction_Generic(int i_varId, VClass in) :
               rep(in)
            {
                OGR_SGFS_Transaction::setVarId(i_varId);
            }

            VClass getData()
            {
                return rep;
            }

            nc_type getType() override { return ntype; }
    };

    typedef OGR_SGFS_NC_Transaction_Generic<signed char, NC_BYTE> OGR_SGFS_NC_Byte_Transaction;
    typedef OGR_SGFS_NC_Transaction_Generic<short, NC_SHORT> OGR_SGFS_NC_Short_Transaction;
    typedef OGR_SGFS_NC_Transaction_Generic<int, NC_INT> OGR_SGFS_NC_Int_Transaction;
    typedef OGR_SGFS_NC_Transaction_Generic<float, NC_FLOAT> OGR_SGFS_NC_Float_Transaction;
    typedef OGR_SGFS_NC_Transaction_Generic<double, NC_DOUBLE> OGR_SGFS_NC_Double_Transaction;

#ifdef NETCDF_HAS_NC4
    typedef OGR_SGFS_NC_Transaction_Generic<unsigned, NC_UINT> OGR_SGFS_NC_UInt_Transaction;
    typedef OGR_SGFS_NC_Transaction_Generic<unsigned long long, NC_UINT64> OGR_SGFS_NC_UInt64_Transaction;
    typedef OGR_SGFS_NC_Transaction_Generic<long long, NC_INT64> OGR_SGFS_NC_Int64_Transaction;
    typedef OGR_SGFS_NC_Transaction_Generic<unsigned char, NC_UBYTE> OGR_SGFS_NC_UByte_Transaction;
    typedef OGR_SGFS_NC_Transaction_Generic<unsigned short, NC_USHORT> OGR_SGFS_NC_UShort_Transaction;

    /* OGR_SGFS_NC_String_Transaction
     * Writes to an NC_STRING variable, in a similar manner as NC_Char
     */
    class OGR_SGFS_NC_String_Transaction : public OGR_SGFS_Transaction
    {
        std::string char_rep;

        public:
            void commit(netCDFVID& n, size_t write_loc) override
            {
                const char * writable = char_rep.c_str();
                n.nc_put_vvar1_string(OGR_SGFS_Transaction::getVarId(), &write_loc, &(writable));
            }

            unsigned long long count() override { return char_rep.size() + sizeof(*this); } // account for actual character representation, this class

            nc_type getType() override { return NC_STRING; }

            void appendToLog(VSILFILE* f) override;

            OGR_SGFS_NC_String_Transaction(int i_varId, const char* pszVal) :
                char_rep(pszVal)
            {
                OGR_SGFS_Transaction::setVarId(i_varId);
            }
    };

#endif

    /* WTransactionLog
     * -
     * A temporary file which contains transactions to be written to a netCDF file.
     * Once the transaction log is created it is set on write mode, it can only be read to after startRead() is called
     */
    class WTransactionLog
    {
        bool readMode = false;
        std::string wlogName; // name of the temporary file, should be unique
        VSILFILE* log = nullptr;


        WTransactionLog(WTransactionLog&); // avoid possible undefined behavior
        WTransactionLog operator=(const WTransactionLog&);

        public:
            bool logIsNull() { return log == nullptr; }
            void startLog(); // always call this first to open the file
            void startRead(); // then call this before reading it
            void push(MTPtr);

            // read mode
            MTPtr pop();  // to test for EOF, test to see if pointer returned is null ptr

            // construction, destruction
            explicit WTransactionLog(const std::string& logName);
            ~WTransactionLog();
    };

    /* OGR_NCScribe
     * Buffers several netCDF transactions in memory or in a log.
     * General scribe class
     */
    class OGR_NCScribe
    {
        netCDFVID & ncvd;
        WBuffer buf;
        WTransactionLog wl;
        bool singleDatumMode = false;

        std::queue<MTPtr> transactionQueue;
        std::map<int, size_t> varWriteInds;
        std::map<int, size_t> varMaxInds;

        public:
           /* size_t getWriteCount()
            * Return the total write count (happened + pending) of certain variable
            */
           size_t getWriteCount(int varId) { return this->varMaxInds.at(varId); }

            /* void commit_transaction()
             * Replays all transactions to disk (according to fs stipulations)
             */
           void commit_transaction();

           /* MTPtr pop()
            * Get the next transaction, if it exists.
            * If not, it will just return a shared_ptr with nullptr inside
            */
           MTPtr pop();

           /* void log_transacion()
            * Saves the current queued transactions to a log.
            */
           void log_transaction();

           /* void enqueue_transaction()
            * Add a transaction to perform
            * Once a transaction is enqueued, it will only be dequeued on commit
            */
           void enqueue_transaction(MTPtr transactionAdd);

           WBuffer& getMemBuffer() { return buf; }

           /* OGR_SGeometry_Field_Scribe()
            * Constructs a Field Scribe over a dataset
            */
           OGR_NCScribe(netCDFVID& ncd, const std::string& name) :
               ncvd(ncd),
               wl(name)
           {}

           /* setSingleDatumMode(...)
            * Enables or disables single datum mode
            * DO NOT use this when a commit is taking place, otherwise 
            * corruption may occur...
            */
           void setSingleDatumMode(bool sdm) { this->singleDatumMode = sdm; }

    };

    class ncLayer_SG_Metadata
    {
        int & ncID; // ncid REF. which tracks ncID changes that may be made upstream

        netCDFVID& vDataset;
        OGR_NCScribe & ncb;
        geom_t writableType = NONE;
        std::string containerVarName;
        int containerVar_realID = INVALID_VAR_ID;
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
            int get_containerRealID() { return this->containerVar_realID; }
            std::string get_containerName() { return this->containerVarName; }
            int get_node_count_dimID() { return this->node_count_dimID; }
            int get_node_coord_dimID() { return this->node_coordinates_dimID; }
            int get_pnc_dimID() { return this->pnc_dimID; }
            int get_pnc_varID() { return this->pnc_varID; }
            int get_intring_varID() { return this->intring_varID; }
            std::vector<int>& get_nodeCoordVarIDs() { return this->node_coordinates_varIDs; }
            size_t get_next_write_pos_node_coord() { return this->next_write_pos_node_coord; }
            size_t get_next_write_pos_node_count() { return this->next_write_pos_node_count; }
            size_t get_next_write_pos_pnc() { return this->next_write_pos_pnc; }
            bool getInteriorRingDetected() { return this->interiorRingDetected; }
            void initializeNewContainer(int containerVID);
            ncLayer_SG_Metadata(int & i_ncID, geom_t geo, netCDFVID& ncdf, OGR_NCScribe& scribe);
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

    class SGWriter_Exception_NCDelFailure : public SGWriter_Exception
    {
        std::string msg;

        public:
            const char * get_err_msg() override { return this->msg.c_str(); }
            SGWriter_Exception_NCDelFailure(const char* layer, const char* what)
                : msg("[" + std::string(layer) + "] Failed to delete: " + std::string(what))
            {}
    };

    // Helper Functions that for writing

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

    template<class W_type> inline void NCWMapAllocIfNeeded(int varid, NCWMap& mapAdd, size_t numEntries, std::vector<int> & v)
    {
        if(mapAdd.count(varid) < 1)
        {
            mapAdd.insert(NCWEntry(varid,  CPLMalloc(sizeof(W_type) * numEntries)));
            v.push_back(varid);
        }
    }

    template<class W_type> inline void NCWMapWriteAndCommit(int varid, NCWMap& mapAdd, size_t currentEntry, size_t numEntries, W_type data, netCDFVID& vcdf)
    {
        W_type* ptr = static_cast<W_type*>(mapAdd.at(varid));
        ptr[currentEntry] = data;
        static const size_t BEGIN = 0;

        // If all items are ready, write the array, and free it, delete the pointer
        if (currentEntry == (numEntries - 1))
        {
            try
            {
                // Write the whole array at once
                vcdf.nc_put_vvara_generic<W_type>(varid, &BEGIN, &numEntries, ptr);
            }
            catch (SG_Exception_VWrite_Failure& e)
            {
                CPLError(CE_Warning, CPLE_FileIO, "%s", e.get_err_msg());
            }

            CPLFree(mapAdd.at(varid));
            mapAdd.erase(varid);
        }
    }
}

#endif
