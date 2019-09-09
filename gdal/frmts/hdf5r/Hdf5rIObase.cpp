/*
 * Hdf5rIObase.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: nielson
 */

#include "Hdf5rIObase.h"
#include "cpl_error.h"

//******************************************************************************
// Iterate through a Compound data element map and do an H5Insert for each
//******************************************************************************
void Hdf5rIObase::h5InsertFromMap( const CompoundBase::CompoundElementMap_t& elementMap,
                                   hid_t memHid,
                                   const std::string& who )
{
    CompoundBase::CompoundElementMap_t::const_iterator i = elementMap.begin();
    while( i != elementMap.end())
    {
        // reference to the element
        const CompoundBase::CompoundElement_t& dataElement = i->second;

        // scalar types
        if (dataElement.dimension == 0)
        {
            CPLDebug( HDF5R_DEBUG_STR, "%s inserting: %s",
                      who.c_str(), dataElement.name.c_str() );
            if (H5Tinsert( memHid, dataElement.name.c_str(), dataElement.offset, dataElement.h5TypeId ) < 0)
                CPLError( CE_Warning, CPLE_AppDefined, "HDF5-R %s insert failed for: %s",
                          who.c_str(), dataElement.name.c_str() );
        }

        // strings
        else if (dataElement.h5TypeId == H5T_C_S1)
        {
            hid_t strHid = H5Tcopy( H5T_C_S1 );
            H5Tset_size( strHid, dataElement.dimension );
            CPLDebug( HDF5R_DEBUG_STR, "%s inserting: %s",
                      who.c_str(), dataElement.name.c_str() );
            if (H5Tinsert( memHid, dataElement.name.c_str(), dataElement.offset, strHid ) < 0)
                CPLError( CE_Warning, CPLE_AppDefined, "HDF5-R %s insert failed for: %s",
                          who.c_str(), dataElement.name.c_str() );
            H5Tclose( strHid );
        }

        // rank 1 arrays (vectors)
        else
        {
            hsize_t dims[1] = {dataElement.dimension};
            hid_t vectTypeId = H5Tarray_create( dataElement.h5TypeId, 1, dims, nullptr );
            CPLDebug( HDF5R_DEBUG_STR, "%s inserting: %s",
                      who.c_str(), dataElement.name.c_str() );
            if (H5Tinsert( memHid, dataElement.name.c_str(), dataElement.offset, vectTypeId ) < 0)
                CPLError( CE_Warning, CPLE_AppDefined, "HDF5-R %s insert failed for: %s",
                          who.c_str(), dataElement.name.c_str() );
            H5Tclose( vectTypeId );
        }

        ++i;
    }
}
