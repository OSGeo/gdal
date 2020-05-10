/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  HDF5 convenience functions.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gh5_convenience.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                    GH5_FetchAttribute(CPLString)                     */
/************************************************************************/

bool GH5_FetchAttribute( hid_t loc_id, const char *pszAttrName,
                         CPLString &osResult, bool bReportError )

{
    if( !bReportError && H5Aexists(loc_id, pszAttrName) <= 0 )
    {
        return false;
    }

    hid_t hAttr = H5Aopen_name(loc_id, pszAttrName);

    osResult.clear();

    if( hAttr < 0 )
    {
        if( bReportError )
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to read attribute %s failed, not found.",
                     pszAttrName);
        return false;
    }

    const hid_t hAttrSpace = H5Aget_space(hAttr);
    hsize_t anSize[H5S_MAX_RANK] = {};
    const unsigned int nAttrDims =
        H5Sget_simple_extent_dims(hAttrSpace, anSize, nullptr);
    if( nAttrDims != 0 && !(nAttrDims == 1 && anSize[0] == 1) )
    {
        H5Sclose(hAttrSpace);
        H5Aclose(hAttr);
        return false;
    }

    hid_t hAttrTypeID = H5Aget_type(hAttr);
    hid_t hAttrNativeType = H5Tget_native_type(hAttrTypeID, H5T_DIR_DEFAULT);

    bool retVal = false;
    if( H5Tget_class(hAttrNativeType) == H5T_STRING )
    {
        if ( H5Tis_variable_str(hAttrNativeType) )
        {
            char* aszBuffer[1] = { nullptr };
            H5Aread(hAttr, hAttrNativeType, aszBuffer);

            if( aszBuffer[0] )
                osResult = aszBuffer[0];

            H5Dvlen_reclaim(hAttrNativeType, hAttrSpace, H5P_DEFAULT,
                            aszBuffer);
        }
        else
        {
            const size_t nAttrSize = H5Tget_size(hAttrTypeID);
            char *pachBuffer = static_cast<char *>(CPLCalloc(nAttrSize + 1, 1));
            H5Aread(hAttr, hAttrNativeType, pachBuffer);

            osResult = pachBuffer;
            CPLFree(pachBuffer);
        }

        retVal = true;
    }
    else
    {
        if( bReportError )
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Attribute %s of unsupported type for conversion to string.",
                pszAttrName);

        retVal = false;
    }

    H5Sclose(hAttrSpace);
    H5Tclose(hAttrNativeType);
    H5Tclose(hAttrTypeID);
    H5Aclose(hAttr);
    return retVal;
}

/************************************************************************/
/*                      GH5_FetchAttribute(double)                      */
/************************************************************************/

bool GH5_FetchAttribute( hid_t loc_id, const char *pszAttrName,
                         double &dfResult, bool bReportError )

{
    if( !bReportError && H5Aexists(loc_id, pszAttrName) <= 0 )
    {
        return false;
    }

    const hid_t hAttr = H5Aopen_name(loc_id, pszAttrName);

    dfResult = 0.0;
    if( hAttr < 0 )
    {
        if( bReportError )
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to read attribute %s failed, not found.",
                     pszAttrName);
        return false;
    }

    hid_t hAttrTypeID = H5Aget_type(hAttr);
    hid_t hAttrNativeType = H5Tget_native_type(hAttrTypeID, H5T_DIR_DEFAULT);

    // Confirm that we have a single element value.
    hid_t hAttrSpace = H5Aget_space(hAttr);
    hsize_t anSize[H5S_MAX_RANK] = {};
    int nAttrDims = H5Sget_simple_extent_dims(hAttrSpace, anSize, nullptr);

    int i, nAttrElements = 1;

    for( i = 0; i < nAttrDims; i++ )
    {
        nAttrElements *= (int)anSize[i];
    }

    if( nAttrElements != 1 )
    {
        if( bReportError )
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to read attribute %s failed, count=%d, not 1.",
                     pszAttrName, nAttrElements);

        H5Sclose(hAttrSpace);
        H5Tclose(hAttrNativeType);
        H5Tclose(hAttrTypeID);
        H5Aclose(hAttr);
        return false;
    }

    // Read the value.
    void *buf = CPLMalloc(H5Tget_size(hAttrNativeType));
    H5Aread(hAttr, hAttrNativeType, buf);

    // Translate to double.
    if( H5Tequal(H5T_NATIVE_SHORT, hAttrNativeType) )
        dfResult = *((short *)buf);
    else if( H5Tequal(H5T_NATIVE_INT, hAttrNativeType) )
        dfResult = *((int *)buf);
    else if( H5Tequal(H5T_NATIVE_FLOAT,    hAttrNativeType) )
        dfResult = *((float *)buf);
    else if( H5Tequal(H5T_NATIVE_DOUBLE,    hAttrNativeType) )
        dfResult = *((double *)buf);
    else
    {
        if( bReportError )
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Attribute %s of unsupported type for conversion to double.",
                pszAttrName);
        CPLFree(buf);

        H5Sclose(hAttrSpace);
        H5Tclose(hAttrNativeType);
        H5Tclose(hAttrTypeID);
        H5Aclose(hAttr);

        return false;
    }

    CPLFree(buf);

    H5Sclose(hAttrSpace);
    H5Tclose(hAttrNativeType);
    H5Tclose(hAttrTypeID);
    H5Aclose(hAttr);
    return true;
}

/************************************************************************/
/*                          GH5_GetDataType()                           */
/*                                                                      */
/*      Transform HDF5 datatype to GDAL datatype                        */
/************************************************************************/
GDALDataType GH5_GetDataType(hid_t TypeID)
{
    if( H5Tequal(H5T_NATIVE_CHAR,        TypeID) )
        return GDT_Byte;
    else if( H5Tequal(H5T_NATIVE_SCHAR,  TypeID) )
        return GDT_Byte;
    else if( H5Tequal(H5T_NATIVE_UCHAR,  TypeID) )
        return GDT_Byte;
    else if( H5Tequal(H5T_NATIVE_SHORT,  TypeID) )
        return GDT_Int16;
    else if( H5Tequal(H5T_NATIVE_USHORT, TypeID) )
        return GDT_UInt16;
    else if( H5Tequal(H5T_NATIVE_INT,    TypeID) )
        return GDT_Int32;
    else if( H5Tequal(H5T_NATIVE_UINT,   TypeID) )
        return GDT_UInt32;
    else if( H5Tequal(H5T_NATIVE_LONG,   TypeID) )
    {
#if SIZEOF_UNSIGNED_LONG == 4
        return GDT_Int32;
#else
        return GDT_Unknown;
#endif
    }
    else if( H5Tequal(H5T_NATIVE_ULONG,  TypeID) )
    {
#if SIZEOF_UNSIGNED_LONG == 4
        return GDT_UInt32;
#else
        return GDT_Unknown;
#endif
    }
    else if( H5Tequal(H5T_NATIVE_FLOAT,  TypeID) )
        return GDT_Float32;
    else if( H5Tequal(H5T_NATIVE_DOUBLE, TypeID) )
        return GDT_Float64;
#ifdef notdef
    else if( H5Tequal(H5T_NATIVE_LLONG,  TypeID) )
        return GDT_Unknown;
    else if( H5Tequal(H5T_NATIVE_ULLONG, TypeID) )
        return GDT_Unknown;
#endif

    return GDT_Unknown;
}

/************************************************************************/
/*                        GH5_CreateAttribute()                         */
/************************************************************************/

bool GH5_CreateAttribute (hid_t loc_id, const char *pszAttrName,
                          hid_t TypeID, unsigned nMaxLen)
{
#ifdef notdef_write_variable_length_string
    if (TypeID == H5T_C_S1)
    {
        hsize_t   dims[1] = {1};
        hid_t dataspace = H5Screate_simple(1, dims, nullptr);
        hid_t type = H5Tcopy(TypeID);
        H5Tset_size (type, H5T_VARIABLE);
        hid_t att = H5Acreate(loc_id,pszAttrName, type, dataspace, H5P_DEFAULT);
        H5Tclose(type);
        H5Aclose(att);
        H5Sclose(dataspace);
        return true;
    }
#endif

    hid_t hDataSpace = H5Screate(H5S_SCALAR);
    if (hDataSpace < 0)
        return false;

    hid_t hDataType = H5Tcopy(TypeID);
    if (hDataType < 0)
    {
        H5Sclose (hDataSpace);
        return false;
    }

    if (TypeID == H5T_C_S1)
    {
        if( H5Tset_size(hDataType, nMaxLen) < 0 )
        {
            H5Tclose(hDataType);
            H5Sclose(hDataSpace);
            return false;
        }
    }

    hid_t hAttr = H5Acreate(loc_id, pszAttrName,
                            hDataType, hDataSpace, H5P_DEFAULT);
    if (hAttr < 0)
    {
        H5Sclose(hDataSpace);
        H5Tclose(hDataType);
        return false;
    }

    H5Aclose(hAttr);
    H5Sclose(hDataSpace);
    H5Tclose(hDataType);

    return true;
}

/************************************************************************/
/*                        GH5_WriteAttribute()                          */
/************************************************************************/

bool GH5_WriteAttribute (hid_t loc_id, const char *pszAttrName,
                         const char* pszValue)
{

    hid_t hAttr = H5Aopen_name (loc_id, pszAttrName);
    if (hAttr < 0)
        return false;

    hid_t hDataType = H5Aget_type (hAttr);
    if (hDataType < 0)
    {
        H5Aclose (hAttr);
        return false;
    }

    hid_t hAttrNativeType = H5Tget_native_type(hDataType, H5T_DIR_DEFAULT);
    bool bSuccess = false;
    if( H5Tget_class(hAttrNativeType) == H5T_STRING )
    {
#ifdef notdef_write_variable_length_string
        bSuccess = H5Awrite(hAttr, hDataType, &pszValue) >= 0;
#else
        bSuccess = H5Awrite(hAttr, hDataType, pszValue) >= 0;
#endif
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attribute %s is not of type string", pszAttrName);
    }

    H5Tclose(hAttrNativeType);
    H5Tclose(hDataType);
    H5Aclose(hAttr);

    return bSuccess;
}

/************************************************************************/
/*                        GH5_WriteAttribute()                          */
/************************************************************************/

bool GH5_WriteAttribute (hid_t loc_id, const char *pszAttrName,
                         double dfValue)
{

    hid_t hAttr = H5Aopen_name (loc_id, pszAttrName);
    if (hAttr < 0)
        return false;

    hid_t hDataType = H5Aget_type (hAttr);
    if (hDataType < 0)
    {
        H5Aclose (hAttr);
        return false;
    }

    hid_t hAttrNativeType = H5Tget_native_type(hDataType, H5T_DIR_DEFAULT);
    bool bSuccess = false;
    if( H5Tequal(hAttrNativeType, H5T_NATIVE_FLOAT) )
    {
        float fVal = static_cast<float>(dfValue);
        bSuccess = H5Awrite(hAttr, hAttrNativeType, &fVal) >= 0;
    }
    else if( H5Tequal(hAttrNativeType, H5T_NATIVE_DOUBLE) )
    {
        bSuccess = H5Awrite(hAttr, hAttrNativeType, &dfValue) >= 0;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attribute %s is not of type float or double", pszAttrName);
    }

    H5Tclose(hAttrNativeType);
    H5Aclose(hAttr);
    H5Tclose(hDataType);

    return bSuccess;
}

/************************************************************************/
/*                        GH5_WriteAttribute()                          */
/************************************************************************/

bool GH5_WriteAttribute (hid_t loc_id, const char *pszAttrName,
                         unsigned nValue)
{

    hid_t hAttr = H5Aopen_name (loc_id, pszAttrName);
    if (hAttr < 0)
        return false;

    hid_t hDataType = H5Aget_type (hAttr);
    if (hDataType < 0)
    {
        H5Aclose (hAttr);
        return false;
    }

    hid_t hAttrNativeType = H5Tget_native_type(hDataType, H5T_DIR_DEFAULT);
    bool bSuccess = false;
    if( H5Tequal(hAttrNativeType, H5T_NATIVE_INT) ||
        H5Tequal(hAttrNativeType, H5T_NATIVE_UINT) )
    {
        bSuccess = H5Awrite(hAttr, hAttrNativeType, &nValue) >= 0;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attribute %s is not of type int/uint", pszAttrName);
    }

    H5Tclose(hAttrNativeType);
    H5Aclose(hAttr);
    H5Tclose(hDataType);

    return bSuccess;
}
