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
#ifndef __NETCDFVIRTUAL_H__
#define __NETCDFVIRTUAL_H__
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "netcdfsg.h"
#include "netcdf.h"

// netCDF Virtual
// Provides a layer of "virtual ncID"
// that can be mapped to a real netCDF ID
namespace nccfdriver
{
    // Exceptions
    class SG_Exception_NVOOB : public SG_Exception
    {
        std::string err_msg;

        public:
            explicit SG_Exception_NVOOB(const char* dsname) :
                err_msg(std::string("An attempt to read an undefined ID from ") + std::string(dsname) + std::string(" was made")) {}
            const char* get_err_msg() override { return this->err_msg.c_str(); }
    };

    class SG_Exception_DupName: public SG_Exception
    {
        std::string err_msg;

        public:
            SG_Exception_DupName(const char * keyn, const char* dsname) :
                err_msg(std::string("The key ") + std::string(keyn) + std::string(" already exists in") + std::string(dsname)) {}
            const char* get_err_msg() override { return this->err_msg.c_str(); }
    };

    class SG_Exception_BadMapping : public SG_Exception
    {
        std::string err_msg;

        public:
            SG_Exception_BadMapping(const char* key, const char* where) :
                err_msg(std::string(key) + std::string(" not found in ") + std::string(where))
            {}

            const char* get_err_msg() override { return this->err_msg.c_str(); }
    };

    class SG_Exception_VWrite_Failure : public SG_Exception
    {
        std::string err_msg;

        public:
            SG_Exception_VWrite_Failure(const char* where, const char* type) :
                err_msg(std::string("Failed to write ") + std::string(type) + std::string(" to ") + std::string(where))
            {}

            const char* get_err_msg() override { return this->err_msg.c_str(); }
    };

    /* netCDFVAttribute
     * -
     * Contains attribute name and data.
     * Central to derived types are reimplementations of vsync
     */
    class netCDFVAttribute
    {
        public:
            /* vsync(...)
             * Implementation: Given the REAL ncID and REAL variable ID
             * Write the attribute to the variable
             */
            virtual void vsync(int realncid, int realvarid) = 0;

            /*  ~netCDFVAttribute()
             * Virtual destructor
             */
            virtual ~netCDFVAttribute() { }
    };

    template <class VClass, nc_type ntype> class netCDFVGeneralAttribute : public netCDFVAttribute
    {
        std::string name;
        VClass value;

        public:
            netCDFVGeneralAttribute<VClass, ntype>(const char* a_name, const VClass* a_value) :
                name(a_name),
                value(*a_value)
            {}

            void vsync(int realncid, int realvarid) override
            {
                if(nc_put_att(realncid, realvarid, name.c_str(), ntype, 1, &value) != NC_NOERR)
                {
                    throw SG_Exception_VWrite_Failure("variable", "attribute");
                }
            }
    };

    /* netCDFVTextAttribute
     * -
     * Attribute that has a text string value
     */
    class netCDFVTextAttribute : public netCDFVAttribute
    {
        std::string name;
        std::string value;

        public:
            netCDFVTextAttribute(const char* a_name, const char* a_value) :
                name(a_name),
                value(a_value)
            {}

            void vsync(int realncid, int realvarid) override;
    };

    typedef netCDFVGeneralAttribute<signed char, NC_BYTE> netCDFVByteAttribute;
    typedef netCDFVGeneralAttribute<int, NC_INT> netCDFVIntAttribute;
    typedef netCDFVGeneralAttribute<double, NC_DOUBLE> netCDFVDoubleAttribute;
    typedef netCDFVGeneralAttribute<float, NC_FLOAT> netCDFVFloatAttribute;

    /* netCDFVDimension
     * -
     * Contains the real dim id, real dimension name, and dimension length
     */
    class netCDFVDimension
    {
        friend class netCDFVID;

        std::string real_dim_name;
        int r_did = INVALID_DIM_ID;
        int v_did;
        size_t dim_len;
        bool valid = true;

        protected:
            void setRealID(int realID) { this->r_did = realID; }
            void invalidate();
            void setLen(size_t len) { this->dim_len = len; }
        public:
            netCDFVDimension(const char * name, size_t len, int dimid) :
                             real_dim_name(name),
                             v_did(dimid),
                             dim_len(len)
                             {}

            std::string& getName() { return this->real_dim_name; }
            size_t getLen() { return this->dim_len; }
            int getRealID() { return this->r_did; }
            int getVirtualID() { return this->v_did; }
            bool isValid() { return this->valid; }
    };

    /* netCDFVVariable
     * -
     * Contains the variable name, variable type, etc.
     */
    class netCDFVVariable
    {
        friend class netCDFVID;

        std::string real_var_name;
        nc_type ntype;
        int r_vid = INVALID_VAR_ID;
        int ndimc;
        std::unique_ptr<int, std::default_delete<int[]>> dimid;
        std::vector<std::shared_ptr<netCDFVAttribute>> attribs;
        bool valid = true;

        protected:
            std::vector<std::shared_ptr<netCDFVAttribute>>& getAttributes() { return attribs; }
            void invalidate();
            void setRealID(int realID) { this->r_vid = realID; }
        public:
            netCDFVVariable(const char * name, nc_type xtype, int ndims, const int* dimidsp);
            std::string& getName() { return real_var_name; }
            int getRealID() { return r_vid; }
            nc_type getType() { return ntype; }
            int getDimCount() { return this->ndimc; }
            const int* getDimIds() { return this->dimid.get(); }
            bool isValid() { return this->valid; }
    };

    /* netCDFVID
     * -
     * A netCDF ID that sits on top of an actual netCDF ID
     * And manages actual interaction with the real netCDF file
     *
     * A bit difference is that netCDFVID
     * doesn't have fixed dim sizes, until defines are committed
     *
     * Also, virtual attributes only exist until the variable is committed. Use "real" attributes and "real" IDs
     * for a variable after its been committed.
     *
     * ** Do not mix netCDF virtual dim and variable IDs with regular netCDF dim (a.k.a. "real")
     * ids and variable ids. They are NOT necessarily compatible, and must be translated first, to be
     * used in this manner **
     *
     * The netCDFVID can also be used in what is called "direct mode" and the netCDFVID will just act as a wrapper to the netCDF Library.
     * In such a case netCDFVID should take real IDs, not real ones. However, the big advantages of using netCDFVID (such as quick dim resizing)
     * are no longer are available.
     */
    class netCDFVID
    {
        int & ncid; // ncid REF. which tracks ncID changes that may be made upstream
        int dimTicket = 0;
        int varTicket = 0;
        bool directMode = true;

        std::vector<netCDFVVariable> varList;
        std::vector<netCDFVDimension> dimList;

        std::map<std::string, int> nameDimTable;
        std::map<std::string, int> nameVarTable;

        public:
            // Each of these returns an ID, NOT an error code

            /* nc_def_vdim(...)
             * Defines a virtual dim given the parameters NAME and LENGTH.
             * Returns: virtual dimID
             */
            int nc_def_vdim(const char * name, size_t dimlen); // for dims that don't already exist in netCDF file

            /* nc_def_vvar(...)
             * Defines a virtual var given the parameters NAME, NC TYPE, NUMBER OF DIMS, and DIM IDS
             * The dim IDs in dimidsp given are to be virtual dim IDs, using real dim IDs is undefined
             */
            int nc_def_vvar(const char * name, nc_type xtype, int ndims, const int* dimidsp);

            /* nc_del_vdim(...)
             * Delete a virtual dimension
             * NOTES:
             *     This doesn't work on committed IDs.
             *     Also the dimension (for now) will be only invalidated, doesn't completely *delete* it in memory.
             */
            void nc_del_vdim(int dimid);

            /* nc_del_vvar(...)
             * Delete a virtual variable
             * NOTES:
             *     This doesn't work on committed IDs.
             *     Also the variable (for now) will be only invalidated, doesn't completely *delete* it in memory.
             */
            void nc_del_vvar(int varid);

            /* nc_resize_vdim(...)
             * Change the size of a virtual dim to the given size.
             * NOTE: if the dim has committed using nc_vmap then this has no effect.
             */
            void nc_resize_vdim(int dimid, size_t dimlen); // for dims that haven't been mapped to physical yet

            /* nc_set_define_mode()
             * Convenience function for setting the ncid to define mode
             */
            void nc_set_define_mode();

            /* nc_set_data_mode()
             * Convenience function for setting the ncid to data mode
             */
            void nc_set_data_mode();

            /* nc_vmap()
             * Maps virtual IDs to real physical ID if that mapping doesn't already exist
             * This is required before writing data to virtual IDs that do not exist yet in the netCDF file
             */
            void nc_vmap();

            /* void enableFullVirtualMode
             * Enables full virtual mode (i.e. allows netCDFVID to use its full capabilities).
             */
            void enableFullVirtualMode() { this->directMode = false; }

            // Attribute function(s)
            template<class attrC, class attrT> void nc_put_vatt_generic(int varid, const char* name, const attrT* value)
            {

                if(varid >= static_cast<int>(varList.size()) || varid < 0)
                {
                    throw SG_Exception_NVOOB("virtual variable collection");
                }

                netCDFVVariable& v = virtualVIDToVar(varid);
                        v.getAttributes().push_back(std::shared_ptr<netCDFVAttribute>(new attrC(name, value)));
            }

            void nc_put_vatt_text(int varid, const char * name, const char * value);
            void nc_put_vatt_int(int varid, const char* name, const int* value);
            void nc_put_vatt_double(int varid, const char* name, const double* value);
            void nc_put_vatt_float(int varid, const char* name, const float* value);
            void nc_put_vatt_byte(int varid, const char* name, const signed char* value);

            // Writing Functions
            template<class out_T> void nc_put_vvar_generic(int varid, const size_t* index, const out_T* value)
            {
                int rvarid = !directMode ? virtualVIDToVar(varid).getRealID() : varid;

                if(rvarid == INVALID_VAR_ID)
                    return; // invalidated variable, specific condition that Scribe relies on

                if(nc_put_var1(ncid, rvarid, index, value) != NC_NOERR)
                {
                    throw SG_Exception_VWrite_Failure("variable", "datum");
                }
            }

            template<class outArr_T> void nc_put_vvara_generic(int varid, const size_t* index, const size_t* count, const outArr_T* value)
            {
                int rvarid = !directMode ? virtualVIDToVar(varid).getRealID() : varid;

                if (rvarid == INVALID_VAR_ID)
                    return; // invalidated variable, specific condition that Scribe relies on

                if (nc_put_vara(ncid, rvarid, index, count, value) != NC_NOERR)
                {
                    throw SG_Exception_VWrite_Failure("variable", "data array");
                }
            }

            void nc_put_vvar1_text(int varid, const size_t* index, const char* value);
            void nc_put_vvara_text(int varid, const size_t* start, const size_t* index, const char* value);

#ifdef NETCDF_HAS_NC4
            void nc_put_vvar1_string(int varid, const size_t* index, const char** value);
#endif

            // Equivalent "enquiry" functions
            netCDFVVariable& virtualVIDToVar(int virtualID); // converts a virtual var ID to a real ID
            netCDFVDimension& virtualDIDToDim(int virtualID); // converts a virtual dim ID to a real ID
            int nameToVirtualVID(std::string& name);
            int nameToVirtualDID(std::string& name);
            bool virtualVarNameDefined(std::string& nm) { return nameVarTable.count(nm) > 0; }

            // Constructor
            explicit netCDFVID(int & ncid_in) : ncid(ncid_in) {}
        };

}
#endif
