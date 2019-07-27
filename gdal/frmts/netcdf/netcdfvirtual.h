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
		int real_dim_id;
		size_t real_dim_len;

		public:
                    netCDFVDimension(const char * name, size_t len, int dimid) :
                        real_dim_name(name),
                        real_dim_id(dimid),
                        real_dim_len(len)
                    {} 
	};

	/* netCDFVVariable
	 * -
	 * Contains the variable name, variable type, etc.
	 */
	class netCDFVVariable
	{
		std::string real_var_name;
		nc_type ntype;
                int id;
		int ndims;
		std::unique_ptr<int, std::default_delete<int[]>> dimid;

		public:

	};

	/* netCDFVID
	 * -
	 * A netCDF ID that sits on top of an actual netCDF ID
	 * And manages actual interaction with the real netCDF file
	 * Some differences is that netCDFVDataset:
	 * - doesn't have fixed dim sizes
	 * - doesn't manage data- intended to use 
	 */
	class netCDFVID
        {
		int dimTicket = 0;
		int varTicket = 0;
		std::map<int, netCDFVDimension> vToReal_dims; // maps from virtual dim ID to real dim ID
		std::map<int, netCDFVVariable> vToReal_vars; // maps from virtual var ID to real var ID

                public:
			// Each of these returns an ID, NOT an error code
			int nc_def_vdim(const char * name, size_t dimlen); // for dims that don't already exist in netCDF file
			int nc_register_vdim(int realID); // for dims that DO already exist in netCDF file
			int nc_def_vvar(const char * name, nc_type xtype, int ndims, const int* dimidsp);
			int nc_register_vvar(int realID); // for vars that DO already exist in netCDF file

                        // Attribute function(s)
			void nc_put_vatt_text(int varid, const char * name, const char * out);
        };
}
