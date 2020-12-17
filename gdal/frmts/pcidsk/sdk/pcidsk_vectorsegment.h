/******************************************************************************
 *
 * Purpose:  PCIDSK Vector Segment public interface. Declaration.
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

#ifndef INCLUDE_PCIDSK_VECTORSEGMENT_H
#define INCLUDE_PCIDSK_VECTORSEGMENT_H

#include <string>
#include <vector>
#include <iterator>
#include "pcidsk_shape.h"

#define ATT_RINGSTART "RingStart"

namespace PCIDSK
{
    class ShapeIterator;

/************************************************************************/
/*                         PCIDSKVectorSegment                          */
/************************************************************************/

/**
\brief Interface to PCIDSK vector segment.

The vector segment contains a set of vector features with a common set
of attribute data (fields).   Each feature has a numeric identifier (ShapeId),
a set of field values, and a set of geometric vertices.   The layer as a
whole has a description of the attribute fields, and an RST (Representation
Style Table).

The geometry and attribute fields of shapes can be fetched with the
GetVertices() and GetFields() methods by giving the ShapeId of the desired
feature.  The set of shapeid's can be identified using the FindFirst(),
and FindNext() methods or the STL compatible ShapeIterator (begin() and
end() methods).

The PCIDSKSegment interface for the segment can be used to fetch the
LAYER_TYPE metadata describing how the vertices should be interpreted
as a geometry.  Some layers will also have a RingStart attribute field
which is used in conjunction with the LAYER_TYPE to interpret the
geometry.  Some vector segments may have no LAYER_TYPE metadata in which
case single vertices are interpreted as points, and multiple vertices
as linestrings.

More details are available in the GDB.HLP description of the GDB vector
data model.

Note that there are no mechanisms for fast spatial or attribute searches
in a PCIDSK vector segment.  Accessing features randomly (rather than
in the order shapeids are returned by FindFirst()/FindNext() or ShapeIterator
) may result in reduced performance, and the use of large amounts of memory
for large vector segments.

*/

class PCIDSK_DLL PCIDSKVectorSegment
{
    public:
        virtual ~PCIDSKVectorSegment() {}

/**
\brief Fetch RST.

No attempt is made to parse the RST, it is up to the caller to decode it.

NOTE: There is some header info on RST format that may be needed to do this
for older RSTs.

@return RST as a string.
*/
        virtual std::string GetRst() = 0;


/**
\brief Fetch Projection

The returned values are the projection parameters in the same form returned
by PCIDSKGeoref::GetParameters() and the passed in geosys argument is
updated with the coordinate system string.

@return Projection parameters as a vector.
*/
        virtual std::vector<double> GetProjection( std::string &geosys ) = 0;

/**
\brief Get field count.

Note that this includes any system attributes, like RingStart, that would
not normally be shown to the user.

@return the number of attribute fields defined on this layer.
*/

        virtual int         GetFieldCount() = 0;

/**
\brief Get field name.

@param field_index index of the field requested from zero to GetFieldCount()-1.
@return the field name.
*/
        virtual std::string GetFieldName(int field_index) = 0;

/**
\brief Get field description.

@param field_index index of the field requested from zero to GetFieldCount()-1.
@return the field description, often empty.
*/
        virtual std::string GetFieldDescription(int field_index) = 0;

/**
\brief Get field type.

@param field_index index of the field requested from zero to GetFieldCount()-1.
@return the field type.
*/
        virtual ShapeFieldType GetFieldType(int field_index) = 0;

/**
\brief Get field format.

@param field_index index of the field requested from zero to GetFieldCount()-1.
@return the field format as a C style format string suitable for use with printf.
*/
        virtual std::string GetFieldFormat(int field_index) = 0;

/**
\brief Get field default.

@param field_index index of the field requested from zero to GetFieldCount()-1.
@return the field default value.
*/
        virtual ShapeField  GetFieldDefault(int field_index) = 0;

/**
\brief Get iterator to first shape.
@return iterator.
*/
        virtual ShapeIterator begin() = 0;

/**
\brief Get iterator to end of shape lib (a wrapper for NullShapeId).
@return iterator.
*/
        virtual ShapeIterator end() = 0;

/**
\brief Fetch first shapeid in the layer.
@return first shape's shapeid.
*/
        virtual ShapeId     FindFirst() = 0;

/**
\brief Fetch the next shape id after the indicated shape id.
@param id the previous shapes id.
@return next shape's shapeid.
*/
        virtual ShapeId     FindNext(ShapeId id) = 0;


/**
\brief Fetch the number of shapes in this segment.
@return the shape count.
*/

        virtual int         GetShapeCount() = 0;

/**
\brief Fetch the vertices for the indicated shape.
@param id the shape to fetch
@param list the list is updated with the vertices for this shape.
*/
        virtual void        GetVertices( ShapeId id,
                                         std::vector<ShapeVertex>& list ) = 0;

/**
\brief Fetch the fields for the indicated shape.
@param id the shape to fetch
@param list the field list is updated with the field values for this shape.
*/
        virtual void        GetFields( ShapeId id,
                                       std::vector<ShapeField>& list ) = 0;


/**
\brief Set the projection for the segment.

For details on the geosys and parms values see the PCIDSKGeoref class.

@param geosys the usual 16 character coordinate system string.
@param parms additional parameters needed for user parametrized projection.
*/
        virtual void        SetProjection(std::string geosys,
                                          std::vector<double> parms ) = 0;

/**
\brief Create new attribute field.

@param name the field name, should be unique in layer.
@param type the field type.
@param description the field description.
@param format the C style format string or "" for default formatting.
@param default_value the default value for this field or NULL for system default.
*/

        virtual void        AddField( std::string name, ShapeFieldType type,
                                      std::string description,
                                      std::string format,
                                      ShapeField *default_value=nullptr ) = 0;

/**
\brief Create a new shape.

Newly created shapes have no geometry or attribute values.

@param id The ShapeId to assign to the new shape, or default to assign the next available shapeid.

@return the shapeid assigned to the newly created shape.
*/

        virtual ShapeId     CreateShape( ShapeId id = NullShapeId ) = 0;

/**
\brief Delete a shape.

An exception is thrown if the shape does not exist.

@param id the shapeid to delete.

*/
        virtual void        DeleteShape( ShapeId id ) = 0;

/**
\brief Assign vertices to shape.

@param id the shape to assign vertices to.
@param list the list of vertices to assign.
*/

        virtual void        SetVertices( ShapeId id,
                                         const std::vector<ShapeVertex> &list ) = 0;


/**
\brief Assign attribute value to a shape.

The list of fields should match the types and length from the schema
(GetFieldCount(), GetFieldType()).

@param id the shape to update.
@param list the list of field value to assign.
*/
        virtual void        SetFields( ShapeId id,
                                       const std::vector<ShapeField>& list) = 0;

// Methods needed
        // DeleteField
    };

/************************************************************************/
/*                            ShapeIterator                             */
/************************************************************************/

//! Iterator over shapeids in a vector segment.

    class ShapeIterator : public std::iterator<std::input_iterator_tag, ShapeId>
    {
        ShapeId id;
        PCIDSKVectorSegment *seg;

    public:
        ShapeIterator(PCIDSKVectorSegment *seg_in)
                : seg(seg_in)  { id = seg->FindFirst(); }
        ShapeIterator(PCIDSKVectorSegment *seg_in, ShapeId id_in )
                : id(id_in), seg(seg_in)  {}
        ShapeIterator(const ShapeIterator& mit) : id(mit.id), seg(mit.seg) {}
        ShapeIterator& operator++() { id=seg->FindNext(id); return *this;}
        ShapeIterator& operator++(int) { id=seg->FindNext(id); return *this;}
        bool operator==(const ShapeIterator& rhs) {return id == rhs.id;}
        bool operator!=(const ShapeIterator& rhs) {return id != rhs.id;}
        ShapeId& operator*() {return id;}
    };

} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_VECTORSEGMENT_H
