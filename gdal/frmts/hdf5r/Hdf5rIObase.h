/*
 * Hdf5rIObase.h
 *
 *  Created on: Dec 5, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_HDF5RIOBASE_H_
#define FRMTS_HDF5R_HDF5RIOBASE_H_

#include <string>

#define H5_USE_16_API
#include "hdf5.h"

#include "hdf5r.h"
#include "CompoundBase.h"

/**
 * Encapsulates shared aspects of H5R Read and Write
 */
class Hdf5rIObase
{
public:
    Hdf5rIObase() {}

    virtual ~Hdf5rIObase() {}

    static void h5InsertFromMap( const CompoundBase::CompoundElementMap_t& elementMap,
                                 hid_t memHid,
                                 const std::string& who );
};

#endif /* FRMTS_HDF5R_HDF5RIOBASE_H_ */
