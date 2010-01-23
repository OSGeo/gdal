///////////////////////////////////////////////////////////////////////////////
// $Id: tut_gdal.h,v 1.4 2006/12/06 15:39:14 mloskot Exp $
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  TUT: C++ Unit Test Framework extensions for GDAL Test Suite
// Author:   Mateusz Loskot <mateusz@loskot.net>
// 
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
//  
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
// 
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
///////////////////////////////////////////////////////////////////////////////
//
//  $Log: tut_gdal.h,v $
//  Revision 1.4  2006/12/06 15:39:14  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////
#ifndef TUT_GDAL_H_INCLUDED
#define TUT_GDAL_H_INCLUDED

#include <ogr_api.h> // GDAL
#include <cassert>
#include <sstream>
#include <string>

namespace tut
{

#if defined(WIN32) || defined(_WIN32_WCE)
#define SEP '\\'
#else
#define SEP '/'
#endif

//
// Template of attribute reading function and its specializations
//
template <typename T>
inline void read_feature_attribute(OGRFeatureH feature, int index, T& val)
{
    assert(!"Can't find read_feature_attribute specialization for given type");
}

template <>
inline void read_feature_attribute(OGRFeatureH feature, int index, int& val)
{
    val = OGR_F_GetFieldAsInteger(feature, index);
}

template <>
inline void read_feature_attribute(OGRFeatureH feature, int index, double& val)
{
    val = OGR_F_GetFieldAsDouble(feature, index);
}

template <>
inline void read_feature_attribute(OGRFeatureH feature, int index, std::string& val)
{
    val = OGR_F_GetFieldAsString(feature, index);
}

//
// Test equality of two OGR geometries according to passed tolerance.
//
void ensure_equal_geometries(OGRGeometryH lhs, OGRGeometryH rhs, double tolerance);

//
// Test layer attributes from given field against expected list of values
//
template <typename T>
void ensure_equal_attributes(OGRLayerH layer, std::string const& field, T const& list)
{
    ensure("Layer is NULL", NULL != layer);

    OGRFeatureDefnH featDefn = OGR_L_GetLayerDefn(layer);
    ensure("Layer schema is NULL", NULL != featDefn);

    int fldIndex = OGR_FD_GetFieldIndex(featDefn, field.c_str());
    std::ostringstream os;
    os << "Can't find field '" << field << "'";
    ensure(os.str(), fldIndex >= 0);

    // Test value in tested field from subsequent features
    OGRFeatureH feat = NULL;
    OGRFieldDefnH fldDefn = NULL;
    typename T::value_type attrVal;

    for (typename T::const_iterator it = list.begin(); it != list.end(); ++it)
    {
        feat = OGR_L_GetNextFeature(layer);

        fldDefn = OGR_F_GetFieldDefnRef(feat, fldIndex);
        ensure("Field schema is NULL", NULL != fldDefn);

        read_feature_attribute(feat, fldIndex, attrVal);
        
        OGR_F_Destroy(feat);

        // Test attribute against expected value
        ensure_equals("Attributes not equal", (*it), attrVal);
    }

    // Check if not too many features filtered
    feat = OGR_L_GetNextFeature(layer);
    bool notTooMany = (NULL == feat);
    OGR_F_Destroy(feat);

    ensure("Got more features than expected", notTooMany);
}


template <typename T>
void ensure_approx_equals(T const& a, T const& b)
{
    std::ostringstream os;
    os << "Approx. equality failed: " << a << " != " << b;
    ensure(os.str(), fabs(1.0 * b / a - 1.0) <= .00000000001);
}

} // namespace tut

#endif // TUT_GDAL_H_INCLUDED
