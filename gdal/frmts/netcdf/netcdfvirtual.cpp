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
#include "netcdfvirtual.h"

// netCDF Virtual
// Provides a layer of "virtual ncID"
// that can be mapped to a real netCDF ID
namespace nccfdriver
{
	netCDFVVariable::netCDFVVariable(const char * name, nc_type xtype, int ndims, const int* dimidsp, int varid) :
		real_var_name(name),
		ntype(xtype),
		v_vid(varid),
		ndimc(ndims),
		dimid(new int[ndims])
	{
		for(int c = 0; c < ndims; c++)
		{
			dimid.get()[c] = dimidsp[c];
		}
	}

	int netCDFVID::nc_def_vdim(const char * name, size_t len)
	{
		int dimID = dimTicket;

		// Check if name is already defined
		if(nameDimTable.count(std::string(name)) > 0)
		{
			// TODO: throw exception
		}

		// Add to lookup tables
                dimList.push_back(netCDFVDimension(name, len, dimTicket++));
		nameDimTable.insert(std::pair<std::string, int>(std::string(name), dimID));

		// Return virtual dimID
		return dimID;
	}

	int netCDFVID::nc_def_vvar(const char * name, nc_type xtype, int ndims, const int* dimidsp)
	{
		int varID = varTicket;

		// Check if name is already defined
		if(nameVarTable.count(std::string(name)) > 0)
		{
			// TODO: throw exception
		}

		// Add to lookup tables
		varList.push_back(netCDFVVariable(name, xtype, ndims, dimidsp, varTicket++));
		nameVarTable.insert(std::pair<std::string, int>(std::string(name), varID));

		// Return virtual dimID
		return varID;
	}

	void netCDFVID::nc_resize_vdim(int dimid, size_t dimlen)
	{
		netCDFVDimension& dim = virtualDIDToDim(dimid);

		if(dim.getRealID() == INVALID_DIM_ID)
		{
			dim.setLen(dimlen);
		}
	}

	void netCDFVID::nc_set_define_mode()
	{
		nc_redef(ncid);
	}

	void netCDFVID::nc_set_data_mode()
	{
		nc_enddef(ncid);
	}

        void netCDFVID::nc_vmap()
	{
		nc_set_define_mode();

		for(size_t itr_d = 0; itr_d < dimList.size(); itr_d++)
		{
			int realDimID;
			netCDFVDimension& dim = dimList[itr_d];

			nc_def_dim(ncid, dim.getName().c_str(), dim.getLen(), &realDimID);
			dimList[itr_d].setRealID(realDimID);
		}

		for(size_t itr_v = 0; itr_v < varList.size(); itr_v++)
		{
			int realVarID;
			netCDFVVariable& var = varList[itr_v];

			// Convert each virtual dimID to a physical dimID:
			std::unique_ptr<int, std::default_delete<int[]>> newdims(new int[var.getDimCount()]);
			for(int dimct = 0; dimct < var.getDimCount(); dimct++)
			{
				newdims.get()[dimct] = virtualDIDToDim(var.getDimIds()[dimct]).getRealID();
			}
			
			nc_def_var(ncid, var.getName().c_str(), var.getType(), var.getDimCount(), newdims.get(), &realVarID);
			var.setRealID(realVarID);
		}

		nc_set_data_mode();
	}

	/* Enquiry Functions
	 * (For use with enquiry information about virtual entities)
	 */
	netCDFVVariable& netCDFVID::virtualVIDToVar(int virtualID)
	{
		if(virtualID >= static_cast<int>(varList.size()) || virtualID < 0)
		{
			// TODO: throw exception
		}

		return varList[virtualID];
	}

	netCDFVDimension& netCDFVID::virtualDIDToDim(int virtualID)
	{

		if(virtualID >= static_cast<int>(dimList.size()) || virtualID < 0)
		{
			// TODO: throw exception
		}

		return dimList[virtualID];
	}

	int netCDFVID::nameToVirtualVID(std::string& name)
	{
		return nameVarTable.at(name); // todo exception handling
	}

	int netCDFVID::nameToVirtualDID(std::string& name)
	{
		return nameDimTable.at(name); // todo exception handling
	}

	/* Single Datum Writing
	 * (mostly just convenience functions)
	 */        
	void netCDFVID::nc_put_vvar1_text(int varid, const size_t* index, const char* out)
	{
		nc_put_var1_text(ncid, virtualVIDToVar(varid).getRealID(), index, out);
	}

	void netCDFVID::nc_put_vvara_text(int varid, const size_t* start, const size_t* count, const char* out)
	{
		nc_put_vara_text(ncid, virtualVIDToVar(varid).getRealID(), start, count, out);
	}

	void netCDFVID::nc_put_vvar1_short(int varid, const size_t* index, short* out)
	{
		nc_put_var1_short(ncid, virtualVIDToVar(varid).getRealID(), index, out);
	}

	void netCDFVID::nc_put_vvar1_int(int varid, const size_t* index, int* out)
	{
		nc_put_var1_int(ncid, virtualVIDToVar(varid).getRealID(), index, out);
	}

	void netCDFVID::nc_put_vvar1_schar(int varid, const size_t* index, signed char* out)
	{
		nc_put_var1_schar(ncid, virtualVIDToVar(varid).getRealID(), index, out);
	}

	void netCDFVID::nc_put_vvar1_float(int varid, const size_t* index, float* out)
	{
		nc_put_var1_float(ncid, virtualVIDToVar(varid).getRealID(), index, out);
	}

	void netCDFVID::nc_put_vvar1_double(int varid, const size_t* index, double* out)
	{
		nc_put_var1_double(ncid, virtualVIDToVar(varid).getRealID(), index, out);
	}

}
