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
#include "netcdfdataset.h"
#include "netcdfvirtual.h"

// netCDF Virtual
// Provides a layer of "virtual ncID"
// that can be mapped to a real netCDF ID
namespace nccfdriver
{
    void netCDFVDimension::invalidate()
    {
        this->valid = false;
        real_dim_name.clear();
    }

    netCDFVVariable::netCDFVVariable(const char * name, nc_type xtype, int ndims, const int* dimidsp) :
        real_var_name(name),
        ntype(xtype),
        ndimc(ndims),
        dimid(new int[ndims])
    {
        for(int c = 0; c < ndims; c++)
        {
            dimid.get()[c] = dimidsp[c];
        }
    }

    void netCDFVVariable::invalidate()
    {
        this->valid = false;
        real_var_name.clear();
        attribs.clear();
    }

    int netCDFVID::nc_def_vdim(const char * name, size_t len)
    {
        if(directMode)
        {
            int ddim;
            int err;
            if((err = nc_def_dim(ncid, name, len, &ddim)) != NC_NOERR)
            {
                NCDF_ERR(err);
                throw SG_Exception_VWrite_Failure("netCDF file", "a dimension definition");
            }

            return ddim;
        }

        int dimID = dimTicket;

        // Check if name is already defined
        if(nameDimTable.count(std::string(name)) > 0)
        {
            throw SG_Exception_DupName(name, "virtual dimension collection");
        }

        // Add to lookup tables
        dimList.push_back(netCDFVDimension(name, len, dimTicket));
        dimTicket++;
        nameDimTable.insert(std::pair<std::string, int>(std::string(name), dimID));

        // Return virtual dimID
        return dimID;
    }

    int netCDFVID::nc_def_vvar(const char * name, nc_type xtype, int ndims, const int* dimidsp)
    {
        if(directMode)
        {
            int dvar;
            int err;
            if((err = nc_def_var(ncid, name, xtype, ndims, dimidsp, &dvar)) != NC_NOERR)
            {
                NCDF_ERR(err);
                throw SG_Exception_VWrite_Failure("netCDF file", "a dimension definition");
            }

            return dvar;
        }

        int varID = varTicket;

        // Check if name is already defined
        if(nameVarTable.count(std::string(name)) > 0)
        {
            throw SG_Exception_DupName(name, "virtual variable collection");
        }

        // Add to lookup tables
        varList.push_back(netCDFVVariable(name, xtype, ndims, dimidsp));
        varTicket++;
        nameVarTable.insert(std::pair<std::string, int>(std::string(name), varID));

        // Return virtual dimID
        return varID;
    }

    void netCDFVID::nc_del_vdim(int dimid)
    {
        // First remove from name map
        nameDimTable.erase(this->dimList[dimid].getName());

        // Then clear actual dim
        this->dimList[dimid].invalidate();
    }

    void netCDFVID::nc_del_vvar(int varid)
    {
        // First remove from name map
        nameVarTable.erase(this->varList[varid].getName());

        // Then clear actual variable
        this->varList[varid].invalidate();
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

            if(!dim.isValid())
            {
                continue; // don't do anywork if variable is invalid
            }

            nc_def_dim(ncid, dim.getName().c_str(), dim.getLen(), &realDimID);
            dimList[itr_d].setRealID(realDimID);
        }

        for(size_t itr_v = 0; itr_v < varList.size(); itr_v++)
        {
            int realVarID;
            netCDFVVariable& var = varList[itr_v];

            if(!var.isValid())
            {
                continue; // don't do anywork if variable is invalid
            }

            // Convert each virtual dimID to a physical dimID:
            std::unique_ptr<int, std::default_delete<int[]>> newdims(new int[var.getDimCount()]);
            for(int dimct = 0; dimct < var.getDimCount(); dimct++)
            {
                newdims.get()[dimct] = virtualDIDToDim(var.getDimIds()[dimct]).getRealID();
            }

            nc_def_var(ncid, var.getName().c_str(), var.getType(), var.getDimCount(), newdims.get(), &realVarID);
            var.setRealID(realVarID);

            // Now write each of its attributes
            for(size_t attrct = 0; attrct < var.getAttributes().size(); attrct++)
            {
                var.getAttributes()[attrct]->vsync(ncid, realVarID);
            }

            var.getAttributes().clear();
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
            throw SG_Exception_NVOOB("virtual variable collection");
        }

        return varList[virtualID];
    }

    netCDFVDimension& netCDFVID::virtualDIDToDim(int virtualID)
    {
        if(virtualID >= static_cast<int>(dimList.size()) || virtualID < 0)
        {
            throw SG_Exception_NVOOB("virtual dimension collection");
        }

        return dimList[virtualID];
    }

    int netCDFVID::nameToVirtualVID(std::string& name)
    {
        if(nameVarTable.count(name) < 1)
        {
            throw SG_Exception_BadMapping(name.c_str(), "variable ID lookup");
        }

        return nameVarTable.at(name);
    }

    int netCDFVID::nameToVirtualDID(std::string& name)
    {
        if(nameDimTable.count(name) < 1)
        {
            throw SG_Exception_BadMapping(name.c_str(), "dimension ID lookup");
        }
        return nameDimTable.at(name);
    }

    /* Attribute writing
     *
     */

    void netCDFVID::nc_put_vatt_text(int varid, const char* name, const char* value)
    {
        if(directMode)
        {
            int err;
            if((err = nc_put_att_text(ncid, varid, name, strlen(value), value)) != NC_NOERR)
            {
                NCDF_ERR(err);
                throw SG_Exception_VWrite_Failure("variable", "text attribute");
            }
            return;
        }

        nc_put_vatt_generic<netCDFVTextAttribute, char>(varid, name, value);
    }

    void netCDFVID::nc_put_vatt_int(int varid, const char* name, const int* value)
    {
        if(directMode)
        {
            int err;
            if((err = nc_put_att_int(ncid, varid, name, NC_INT, 1, value)) != NC_NOERR)
            {
                NCDF_ERR(err);
                throw SG_Exception_VWrite_Failure("variable", "int attribute");
            }
            return;
        }

        nc_put_vatt_generic<netCDFVIntAttribute, int>(varid, name, value);
    }

    void netCDFVID::nc_put_vatt_double(int varid, const char* name, const double* value)
    {
        if(directMode)
        {
            int err;
            if((err = nc_put_att_double(ncid, varid, name, NC_DOUBLE, 1, value)) != NC_NOERR)
            {
                NCDF_ERR(err);
                throw SG_Exception_VWrite_Failure("variable", "double attribute");
            }
            return;
        }

        nc_put_vatt_generic<netCDFVDoubleAttribute, double>(varid, name, value);
    }

    void netCDFVID::nc_put_vatt_float(int varid, const char* name, const float* value)
    {
        if(directMode)
        {
            int err;
            if((err = nc_put_att_float(ncid, varid, name, NC_FLOAT, 1, value)) != NC_NOERR)
            {
                NCDF_ERR(err);
                throw SG_Exception_VWrite_Failure("variable", "float attribute");
            }
            return;
        }

        nc_put_vatt_generic<netCDFVFloatAttribute, float>(varid, name, value);
    }

    void netCDFVID::nc_put_vatt_byte(int varid, const char* name, const signed char* value)
    {
        if(directMode)
        {
            int err;
            if((err = nc_put_att_schar(ncid, varid, name, NC_BYTE, 1, value)) != NC_NOERR)
            {
                NCDF_ERR(err);
                throw SG_Exception_VWrite_Failure("variable", "byte attribute");
            }
            return;
        }

        nc_put_vatt_generic<netCDFVByteAttribute, signed char>(varid, name, value);
    }

    void netCDFVTextAttribute::vsync(int realncid, int realvarid)
    {
        if(nc_put_att_text(realncid, realvarid, name.c_str(), value.size(), value.c_str()) != NC_NOERR)
        {
            throw SG_Exception_VWrite_Failure("variable", "attribute");
        }
    }

    /* Single Datum Writing
     * (mostly just convenience functions)
     */
    void netCDFVID::nc_put_vvar1_text(int varid, const size_t* index, const char* value)
    {
        int rvarid = !directMode ? virtualVIDToVar(varid).getRealID() : varid;
        if(rvarid == INVALID_VAR_ID)
            return; // invalidated variable, don't care condition that Scribe relies on

        if(nc_put_var1_text(ncid, rvarid, index, value) != NC_NOERR)
        {
            throw SG_Exception_VWrite_Failure("variable", "datum");
        }
    }

    void netCDFVID::nc_put_vvara_text(int varid, const size_t* start, const size_t* count, const char* value)
    {
        int rvarid = !directMode ? virtualVIDToVar(varid).getRealID() : varid;
        if(rvarid == INVALID_VAR_ID)
            return; // invalidated variable, don't care condition that Scribe relies on
        if(nc_put_vara_text(ncid, rvarid, start, count, value) != NC_NOERR)
        {
            throw SG_Exception_VWrite_Failure("variable", "datum");
        }
    }

#ifdef NETCDF_HAS_NC4
    void netCDFVID::nc_put_vvar1_string(int varid, const size_t* index, const char** value)
    {
        int rvarid = !directMode ? virtualVIDToVar(varid).getRealID() : varid;

        if(rvarid == INVALID_VAR_ID)
            return; // invalidated variable

        if(nc_put_var1_string(ncid, rvarid, index, value) != NC_NOERR)
        {
            throw SG_Exception_VWrite_Failure("variable", "datum");
        }
    }
#endif

}
