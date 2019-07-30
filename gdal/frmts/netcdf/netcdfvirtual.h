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

	/* netCDFVDimension
	 * -
	 * Contains the real dim id, real dimension name, and dimension length
	 */
	class netCDFVDimension
	{
		std::string real_dim_name;
		int r_did = INVALID_DIM_ID;
		int v_did;
		size_t dim_len;

		public:
                    netCDFVDimension(const char * name, size_t len, int dimid) :
                        real_dim_name(name),
                        v_did(dimid),
                        dim_len(len)
                    {} 

		std::string& getName() { return this->real_dim_name; }
		size_t getLen() { return this->dim_len; }
		void setLen(size_t len) { this->dim_len = len; }
		int getRealID() { return this->r_did; }
		void setRealID(int realID) { this->r_did = realID; }
		int getVirtualID() { return this->v_did; }
	};

	/* netCDFVVariable
	 * -
	 * Contains the variable name, variable type, etc.
	 */
	class netCDFVVariable
	{
		std::string real_var_name;
		nc_type ntype;
                int r_vid = INVALID_VAR_ID;
		int v_vid;
		int ndimc;
		std::unique_ptr<int, std::default_delete<int[]>> dimid;

		public:
			netCDFVVariable(const char * name, nc_type xtype, int ndims, const int* dimidsp, int varid);
			std::string& getName() { return real_var_name; }
			int getRealID() { return r_vid; }
			void setRealID(int realID) { this->r_vid = realID; }
			nc_type getType() { return ntype; }
			int getDimCount() { return this->ndimc; }	
			const int* getDimIds() { return this->dimid.get(); }
	};

	/* netCDFVID
	 * -
	 * A netCDF ID that sits on top of an actual netCDF ID
	 * And manages actual interaction with the real netCDF file
	 * Some differences is that netCDFVDataset:
	 * - doesn't have fixed dim sizes, until defines are committed
	 */
	class netCDFVID
        {
		int & ncid;
		int dimTicket = 0;
		int varTicket = 0;
		std::vector<netCDFVVariable> varList; // for now: direct mapping into var and dim list, might use a more flexible mapping system later
		std::vector<netCDFVDimension> dimList;
		std::map<std::string, int> nameDimTable;
		std::map<std::string, int> nameVarTable;

                public:
			// Each of these returns an ID, NOT an error code
			int nc_def_vdim(const char * name, size_t dimlen); // for dims that don't already exist in netCDF file
			int nc_register_vdim(int realID); // for dims that DO already exist in netCDF file
			int nc_def_vvar(const char * name, nc_type xtype, int ndims, const int* dimidsp);
			int nc_register_vvar(int realID); // for vars that DO already exist in netCDF file
			void nc_resize_vdim(int dimid, size_t dimlen); // for dims that haven't been mapped to physical yet

			/* nc_vmap()
			 * Maps virtual IDs to real physical ID if that mapping doesn't already exist
			 * This is required before writing data to virtual IDs that do not exist yet in the netCDF file
			 */
			void nc_vmap();

                        // Attribute function(s)
			void nc_put_vatt_text(int varid, const char * name, const char * out);

                        // Writing Functions
			void nc_put_vvar1_text(int varid, const size_t* index, const char* out);
			void nc_put_vvara_text(int varid, const size_t* start, const size_t* index, const char* out);
			void nc_put_vvar1_short(int varid, const size_t* index, short* out);
			void nc_put_vvar1_int(int varid, const size_t* index, int* out);
			void nc_put_vvar1_schar(int varid, const size_t* index, signed char* out);
			void nc_put_vvar1_float(int varid, const size_t* index, float* out);
			void nc_put_vvar1_double(int varid, const size_t* index, double* out);

			// Equivalent "enquiry" functions
			netCDFVVariable& virtualVIDToVar(int virtualID); // converts a virtual var ID to a real ID
			netCDFVDimension& virtualDIDToDim(int virtualID); // converts a virtual dim ID to a real ID
			int nameToVirtualVID(std::string& name);
			int nameToVirtualDID(std::string& name);

			// Constructor
			netCDFVID(int & ncid_in) : ncid(ncid_in) {}
        };
}
