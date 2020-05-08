/*
 * Hdf5rAttributeBase.h
 *
 *  Created on: Sep 27, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_HDF5RATTRIBUTEBASE_H_
#define FRMTS_HDF5R_HDF5RATTRIBUTEBASE_H_

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <map>
#include <cstring>

#include "hdf5.h"

class Hdf5rAttributeBase
{
public:
    Hdf5rAttributeBase();

    virtual ~Hdf5rAttributeBase();

    struct H5Attr_t
    {
        enum Union_t {CV_UNKNOWN, CV_I32, CV_U32, CV_I64, CV_U64, CV_FLT, CV_DBL, CV_CSTR };

        H5Attr_t() : name(), h5TypeId( -1 ), h5SpaceId( -1 ),
                       h5StrSz( 0 ), unionType( CV_UNKNOWN ), value{ 0 }
        {}

        explicit H5Attr_t( const std::string& nm,
                             hid_t tid,
                             hid_t sid,
                             int32_t v )
        :  name( nm ), h5TypeId( tid ), h5SpaceId( sid ), h5StrSz( 0 ), unionType( CV_I32 )
        {
            value.i32 = v;
        }

        explicit H5Attr_t( const std::string& nm,
                             hid_t tid,
                             hid_t sid,
                             int64_t v )
        :  name( nm ), h5TypeId( tid ), h5SpaceId( sid ), h5StrSz( 0 ), unionType( CV_I64 )
        {
            value.i64 = v;
        }

        explicit H5Attr_t( const std::string& nm,
                             hid_t tid,
                             hid_t sid,
                             uint32_t v )
        :  name( nm ), h5TypeId( tid ), h5SpaceId( sid ), h5StrSz( 0 ), unionType( CV_U32 )
        {
            value.u32 = v;
        }

        explicit H5Attr_t( const std::string& nm,
                             hid_t tid,
                             hid_t sid,
                             uint64_t v )
        :  name( nm ), h5TypeId( tid ), h5SpaceId( sid ), h5StrSz( 0 ), unionType( CV_U64 )
        {
            value.u64 = v;
        }

        explicit H5Attr_t( const std::string& nm,
                             hid_t tid,
                             hid_t sid,
                             float v )
        :  name( nm ), h5TypeId( tid ), h5SpaceId( sid ), h5StrSz( 0 ), unionType( CV_FLT )
        {
            value.flt = v;
        }

        explicit H5Attr_t( const std::string& nm,
                             hid_t tid,
                             hid_t sid,
                             double v )
        :  name( nm ), h5TypeId( tid ), h5SpaceId( sid ), h5StrSz( 0 ), unionType( CV_DBL )
        {
            value.dbl = v;
        }

        explicit H5Attr_t( const std::string& nm,
                             hid_t tid,
                             hid_t sid,
                             char* cstr,
                             int h5StrSize = -1 ) /* Variable len H5 string */
        :  name( nm ), h5TypeId( tid ), h5SpaceId( sid ), h5StrSz( h5StrSize ), unionType( CV_CSTR )
        {
            value.cstr = cstr;
        }


        ~H5Attr_t() {}

        std::string name;       ///< Case sensitive HDF5-R File Attribute
        hid_t       h5TypeId;   ///< HDF5 type name (HDT_NATIVE_*)
        hid_t       h5SpaceId;  ///< HDF5 Space id for this attribute
        int         h5StrSz;    ///< For string (H5T_C_S1) the strlen (-1 == variable sz)
        Union_t     unionType;
        union
        {
            int32_t  i32;
            uint32_t u32;
            int64_t  i64;
            uint64_t u64;
            float    flt;
            double   dbl;
            char*    cstr;
        }           value;

        void setValue( const std::string& v )
        {
            switch (unionType)
            {
            case CV_I32:
                value.i32 = std::stoi( v );
                break;
            case CV_U32:
                value.u32 = (uint32_t)std::stoul( v );
                break;
            case CV_I64:
                value.i64 = std::stoll( v );
                break;
            case CV_U64:
                value.u64 = std::stoull( v );
                break;
            case CV_DBL:
                value.dbl = std::stod( v );
                break;
            case CV_FLT:
                value.flt = std::stof( v );
                break;
            case CV_CSTR:
                free( value.cstr );
                if (h5StrSz < 0)
                {
                    value.cstr = static_cast<char*>( calloc( v.size() + 1, 1 ) );
                    std::strcpy( value.cstr, v.c_str() );
                }
                else
                {
                    value.cstr = static_cast<char*>( calloc( h5StrSz + 1, 1 ) );
                    std::strncpy( value.cstr, v.c_str(), h5StrSz );
                }
                break;
            default:
                return;
            }
        }

        std::string toString() const
        {
            switch (unionType)
            {
            case CV_I32:
                return std::to_string( value.i32 );
            case CV_U32:
                return std::to_string( value.u32 );
            case CV_I64:
                return std::to_string( value.i64 );
            case CV_U64:
                return std::to_string( value.u64 );
            case CV_DBL:
            {
                std::ostringstream oss;
                oss << std::setprecision( 18 ) << value.dbl;
                return oss.str();
            }
            case CV_FLT:
            {
                std::ostringstream oss;
                oss << std::setprecision( 18 ) << value.flt;
                return oss.str();
            }
            case CV_CSTR:
                return std::string( value.cstr );
            default:
                return std::string( "UNKNOWN Conversion" );
            }
        }
    };

    typedef std::map<std::string, H5Attr_t> H5AttrMap_t;

    const H5AttrMap_t& getConstAttrMap() const {return h5AttrMap_;}

    H5AttrMap_t& getAttrMap() {return h5AttrMap_;}

    bool modifyValue( const std::string& name,
                      const std::string& value );

    bool getValue( const std::string& name,
                   std::string* value ) const;

    bool getValue( const std::string& name,
                   int32_t* value ) const;

    bool getValue( const std::string& name,
                   double* value ) const;

    bool setValue( const std::string& name,
                   int32_t value );

    bool setValue( const std::string& name,
                   uint64_t value );

    bool setValue( const std::string& name,
                   double value );

protected:

    H5AttrMap_t h5AttrMap_;

    hid_t h5ScalarSpaceId_;
};

#endif /* FRMTS_HDF5R_HDF5RATTRIBUTEBASE_H_ */
