/*
 * Hdf5rAttributeBase.cpp
 *
 *  Created on: Sep 27, 2018
 *      Author: nielson
 */

#include "Hdf5rAttributeBase.h"

Hdf5rAttributeBase::Hdf5rAttributeBase()
:  h5AttrMap_(),
   h5ScalarSpaceId_( -1 )
{
    h5ScalarSpaceId_ = H5Screate( H5S_SCALAR );
    if (h5ScalarSpaceId_ < 0)
        throw std::runtime_error( "Hdf5rFileAttributes::Hdf5rFileAttributes()"
                " Call to H5Screate( H5S_SCALAR ) failed!" );
}


Hdf5rAttributeBase::~Hdf5rAttributeBase()
{
    Hdf5rAttributeBase::H5AttrMap_t::iterator i = h5AttrMap_.begin();
    while (i != h5AttrMap_.end())
    {
        Hdf5rAttributeBase::H5Attr_t& fileAttr = i->second;
        if (fileAttr.unionType == H5Attr_t::CV_CSTR)
        {
            free( fileAttr.value.cstr );
            fileAttr.value.cstr = nullptr;
        }
        ++i;
    }
    H5Sclose( h5ScalarSpaceId_ );
    h5ScalarSpaceId_ = -1;
}

bool Hdf5rAttributeBase::modifyValue( const std::string& name,
                                      const std::string& value )
{
    H5AttrMap_t::iterator ifind = h5AttrMap_.find( name );

    // locate in map
    if (ifind != h5AttrMap_.end())
    {
        // found name ==> change value
        ifind->second.setValue( value );
        return true;
    }

    return false;
}

bool Hdf5rAttributeBase::getValue( const std::string& name,
                                   std::string* value ) const
{
    H5AttrMap_t::const_iterator ifind = h5AttrMap_.find( name );

    // locate in map
    if (ifind != h5AttrMap_.end())
    {
        // found name ==> return value as string
        *value = ifind->second.toString();
        return true;
    }

    return false;
}

bool Hdf5rAttributeBase::getValue( const std::string& name, int32_t* value ) const
{
    H5AttrMap_t::const_iterator ifind = h5AttrMap_.find( name );

    // locate in map
    if (ifind != h5AttrMap_.end())
    {
        // found name ==> value type must match pointer type
        if (ifind->second.unionType == H5Attr_t::CV_I32)
        {
            *value = ifind->second.value.i32;
            return true;
        }
        else
            throw std::domain_error( "Hdf5rFileAttributes::Hdf5rFileAttributes()"
                    " getValue called for int32_t (enum: "
                    + std::to_string( H5Attr_t::CV_I32 ) + " ) but it is enum: "
                    + std::to_string( ifind->second.unionType ) );
    }

    return false;
}

bool Hdf5rAttributeBase::getValue( const std::string& name, double* value ) const
{
    H5AttrMap_t::const_iterator ifind = h5AttrMap_.find( name );

    // locate in map
    if (ifind != h5AttrMap_.end())
    {
        // found name ==> value type must match pointer type
        if (ifind->second.unionType == H5Attr_t::CV_DBL)
        {
            *value = ifind->second.value.dbl;
            return true;
        }
        else
            throw std::domain_error( "Hdf5rFileAttributes::Hdf5rFileAttributes()"
                    " getValue called for double (enum: "
                    + std::to_string( H5Attr_t::CV_DBL ) + " ) but it is enum: "
                    + std::to_string( ifind->second.unionType ) );
    }

    return false;
}

bool Hdf5rAttributeBase::setValue( const std::string& name, int32_t value )
{
    H5AttrMap_t::iterator ifind = h5AttrMap_.find( name );

    // locate in map
    if (ifind != h5AttrMap_.end())
    {
        // found name ==> value type must match pointer type
        if (ifind->second.unionType == H5Attr_t::CV_I32)
        {
            ifind->second.value.i32 = value;
            return true;
        }
        else
            throw std::domain_error( "Hdf5rFileAttributes::Hdf5rFileAttributes()"
                    " setValue called for int32_t (enum: "
                    + std::to_string( H5Attr_t::CV_I32 ) + " ) but it is enum: "
                    + std::to_string( ifind->second.unionType ) );
    }

    return false;
}

bool Hdf5rAttributeBase::setValue( const std::string& name, uint64_t value )
{
    H5AttrMap_t::iterator ifind = h5AttrMap_.find( name );

    // locate in map
    if (ifind != h5AttrMap_.end())
    {
        // found name ==> value type must match pointer type
        if (ifind->second.unionType == H5Attr_t::CV_U64)
        {
            ifind->second.value.u64 = value;
            return true;
        }
        else
            throw std::domain_error( "Hdf5rFileAttributes::Hdf5rFileAttributes()"
                    " setValue called for int32_t (enum: "
                    + std::to_string( H5Attr_t::CV_U64 ) + " ) but it is enum: "
                    + std::to_string( ifind->second.unionType ) );
    }

    return false;
}

bool Hdf5rAttributeBase::setValue( const std::string& name, double value )
{
    H5AttrMap_t::iterator ifind = h5AttrMap_.find( name );

    // locate in map
    if (ifind != h5AttrMap_.end())
    {
        // found name ==> value type must match pointer type
        if (ifind->second.unionType == H5Attr_t::CV_DBL)
        {
            ifind->second.value.dbl = value;
            return true;
        }
        else
            throw std::domain_error( "Hdf5rFileAttributes::Hdf5rFileAttributes()"
                    " setValue called for double (enum: "
                    + std::to_string( H5Attr_t::CV_DBL ) + " ) but it is enum: "
                    + std::to_string( ifind->second.unionType ) );
    }

    return false;
}
