/******************************************************************************
 *
 * Purpose:  PCIDSK Vector Shape interface.  Declaration.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

#ifndef INCLUDE_PCIDSK_SHAPE_H
#define INCLUDE_PCIDSK_SHAPE_H

#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

namespace PCIDSK
{

    //! Type used for shape identifier, use constant NullShapeId as a NULL value
    typedef int32 ShapeId;

    static const ShapeId NullShapeId = -1;

    //! Structure for an x,y,z point.
    typedef struct
    {
        double x;
        double y;
        double z;
    } ShapeVertex;

/************************************************************************/
/*                            ShapeFieldType                            */
/************************************************************************/
    //! Attribute field types.
    typedef enum  // These deliberately match GDBFieldType values.
    {
        FieldTypeNone = 0,
        FieldTypeFloat = 1,
        FieldTypeDouble = 2,
        FieldTypeString = 3,
        FieldTypeInteger = 4,
        FieldTypeCountedInt = 5
    } ShapeFieldType;

/************************************************************************/
/*                         ShapeFieldTypeName()                         */
/************************************************************************/
    /**
     \brief Translate field type into a textual description.
     @param type the type enumeration value to translate.
     @return name for field type.
    */
    inline std::string ShapeFieldTypeName( ShapeFieldType type )
    {
        switch( type ) {
          case FieldTypeNone: return "None";
          case FieldTypeFloat: return "Float";
          case FieldTypeDouble: return "Double";
          case FieldTypeString: return "String";
          case FieldTypeInteger: return "Integer";
          case FieldTypeCountedInt: return "CountedInt";
        }
        return "";
    }


/************************************************************************/
/*                              ShapeField                              */
/************************************************************************/
    /**
     \brief Attribute field value.

     This class encapsulates any of the supported vector attribute field
     types in a convenient way that avoids memory leaks or ownership confusion.
     The object has a field type (initially FieldTypeNone on construction)
     and a value of the specified type.  Note that the appropriate value
     accessor (i.e. GetValueInteger()) must be used that corresponds to the
     fields type. No attempt is made to automatically convert (i.e. float to
     double) if the wrong accessor is used.

    */

    class ShapeField
    {
      private:
        ShapeFieldType  type; // use FieldTypeNone for NULL fields.

        union
        {
            float       float_val;
            double      double_val;
            char       *string_val;
            int32       integer_val;
            int32      *integer_list_val;
        } v;

      public:
        //! Simple constructor.
        ShapeField()
            { v.string_val = nullptr; type = FieldTypeNone; }

        //! Copy constructor.
        ShapeField( const ShapeField &src )
            { v.string_val = nullptr; type = FieldTypeNone; *this = src; }

        ~ShapeField()
            { Clear(); }

        //! Assignment operator.
        ShapeField &operator=( const ShapeField &src )
            {
                switch( src.GetType() )
                {
                  case FieldTypeFloat:
                    SetValue( src.GetValueFloat() );
                    break;
                  case FieldTypeDouble:
                    SetValue( src.GetValueDouble() );
                    break;
                  case FieldTypeInteger:
                    SetValue( src.GetValueInteger() );
                    break;
                  case FieldTypeCountedInt:
                    SetValue( src.GetValueCountedInt() );
                    break;
                  case FieldTypeString:
                    SetValue( src.GetValueString() );
                    break;
                  case FieldTypeNone:
                    Clear();
                    break;
                }
                return *this;
            }

        //! Assignment operator.
        bool operator==( const ShapeField &other )
            {
                if( GetType() != other.GetType() )
                    return false;

                switch( other.GetType() )
                {
                  case FieldTypeFloat:
                    return GetValueFloat() == other.GetValueFloat();
                  case FieldTypeDouble:
                    return GetValueDouble() == other.GetValueDouble();
                  case FieldTypeInteger:
                    return GetValueInteger() == other.GetValueInteger();
                  case FieldTypeString:
                    return GetValueString() == other.GetValueString();
                  case FieldTypeCountedInt:
                    return GetValueCountedInt() == other.GetValueCountedInt();
                  case FieldTypeNone:
                    return false;
                  default:
                    return false;
                }
            }

        //! Clear field value.
        void Clear()
            {
                if( (type == FieldTypeString || type == FieldTypeCountedInt)
                    && v.string_val != nullptr )
                {
                    free( v.string_val );
                    v.string_val = nullptr;
                }
                type = FieldTypeNone;
            }

        //! Fetch field type
        ShapeFieldType  GetType() const
            { return type; }

        //! Set integer value on field.
        void SetValue( int32 val )
            {
                Clear();
                type = FieldTypeInteger;
                v.integer_val = val;
            }

        //! Set integer list value on field.
        void SetValue( const std::vector<int32> &val )
            {
                Clear();
                type = FieldTypeCountedInt;
                v.integer_list_val = (int32*)
                    malloc(sizeof(int32) * (val.size()+1) );
                v.integer_list_val[0] = static_cast<int32>(val.size());
                if( !val.empty() )
                    memcpy( v.integer_list_val+1, &(val[0]),
                            sizeof(int32) * val.size() );
            }

        //! Set string value on field.
        void SetValue( const std::string &val )
            {
                Clear();
                type = FieldTypeString;
                v.string_val = strdup(val.c_str());
            }

        //! Set double precision floating point value on field.
        void SetValue( double val )
            {
                Clear();
                type = FieldTypeDouble;
                v.double_val = val;
            }

        //! Set single precision floating point value on field.
        void SetValue( float val )
            {
                Clear();
                type = FieldTypeFloat;
                v.float_val = val;
            }

        //! Fetch value as integer or zero if field not of appropriate type.
        int32 GetValueInteger() const
            { if( type == FieldTypeInteger ) return v.integer_val; else return 0; }
        //! Fetch value as integer list or empty list if field not of appropriate type.
        std::vector<int32> GetValueCountedInt() const
            {
                std::vector<int32> result;
                if( type == FieldTypeCountedInt )
                {
                    result.resize( v.integer_list_val[0] );
                    if( v.integer_list_val[0] > 0 )
                        memcpy( &(result[0]), &(v.integer_list_val[1]),
                                (v.integer_list_val[0]) * sizeof(int32) );
                }
                return result;
            }
        //! Fetch value as string or "" if field not of appropriate type.
        std::string GetValueString() const
            { if( type == FieldTypeString ) return v.string_val; else return ""; }
        //! Fetch value as float or 0.0 if field not of appropriate type.
        float GetValueFloat() const
            { if( type == FieldTypeFloat ) return v.float_val; else return 0.0; }
        //! Fetch value as double or 0.0 if field not of appropriate type.
        double GetValueDouble() const
            { if( type == FieldTypeDouble ) return v.double_val; else return 0.0; }
    };

} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_SHAPE_H
