/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  HDF5 convenience functions.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_float.h"
#include "gh5_convenience.h"

#include <limits>

/************************************************************************/
/*                    GH5_FetchAttribute(CPLString)                     */
/************************************************************************/

bool GH5_FetchAttribute(hid_t loc_id, const char *pszAttrName,
                        CPLString &osResult, bool bReportError)

{
    if (!bReportError && H5Aexists(loc_id, pszAttrName) <= 0)
    {
        return false;
    }

    hid_t hAttr = H5Aopen_name(loc_id, pszAttrName);

    osResult.clear();

    if (hAttr < 0)
    {
        if (bReportError)
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to read attribute %s failed, not found.",
                     pszAttrName);
        return false;
    }

    const hid_t hAttrSpace = H5Aget_space(hAttr);
    hsize_t anSize[H5S_MAX_RANK] = {};
    const unsigned int nAttrDims =
        H5Sget_simple_extent_dims(hAttrSpace, anSize, nullptr);
    if (nAttrDims != 0 && !(nAttrDims == 1 && anSize[0] == 1))
    {
        H5Sclose(hAttrSpace);
        H5Aclose(hAttr);
        return false;
    }

    hid_t hAttrTypeID = H5Aget_type(hAttr);
    hid_t hAttrNativeType = H5Tget_native_type(hAttrTypeID, H5T_DIR_DEFAULT);

    bool retVal = false;
    if (H5Tget_class(hAttrNativeType) == H5T_STRING)
    {
        if (H5Tis_variable_str(hAttrNativeType))
        {
            char *aszBuffer[1] = {nullptr};
            H5Aread(hAttr, hAttrNativeType, aszBuffer);

            if (aszBuffer[0])
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
        if (bReportError)
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

bool GH5_FetchAttribute(hid_t loc_id, const char *pszAttrName, double &dfResult,
                        bool bReportError)

{
    if (!bReportError && H5Aexists(loc_id, pszAttrName) <= 0)
    {
        return false;
    }

    const hid_t hAttr = H5Aopen_name(loc_id, pszAttrName);

    dfResult = 0.0;
    if (hAttr < 0)
    {
        if (bReportError)
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

    for (i = 0; i < nAttrDims; i++)
    {
        nAttrElements *= static_cast<int>(anSize[i]);
    }

    if (nAttrElements != 1)
    {
        if (bReportError)
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
    if (H5Tequal(H5T_NATIVE_CHAR, hAttrNativeType))
        dfResult = *(static_cast<char *>(buf));
    else if (H5Tequal(H5T_NATIVE_SCHAR, hAttrNativeType))
        dfResult = *(static_cast<signed char *>(buf));
    else if (H5Tequal(H5T_NATIVE_UCHAR, hAttrNativeType))
        dfResult = *(static_cast<unsigned char *>(buf));
    else if (H5Tequal(H5T_NATIVE_SHORT, hAttrNativeType))
        dfResult = *(static_cast<short *>(buf));
    else if (H5Tequal(H5T_NATIVE_USHORT, hAttrNativeType))
        dfResult = *(static_cast<unsigned short *>(buf));
    else if (H5Tequal(H5T_NATIVE_INT, hAttrNativeType))
        dfResult = *(static_cast<int *>(buf));
    else if (H5Tequal(H5T_NATIVE_UINT, hAttrNativeType))
        dfResult = *(static_cast<unsigned int *>(buf));
    else if (H5Tequal(H5T_NATIVE_INT64, hAttrNativeType))
    {
        const auto nVal = *static_cast<int64_t *>(buf);
        dfResult = static_cast<double>(nVal);
        if (nVal != static_cast<int64_t>(dfResult))
        {
            CPLDebug("HDF5",
                     "Loss of accuracy when reading attribute %s. "
                     "Value " CPL_FRMT_GIB " will be read as %.17g",
                     pszAttrName, static_cast<GIntBig>(nVal), dfResult);
        }
    }
    else if (H5Tequal(H5T_NATIVE_UINT64, hAttrNativeType))
    {
        const auto nVal = *static_cast<uint64_t *>(buf);
        dfResult = static_cast<double>(nVal);
        if (nVal != static_cast<uint64_t>(dfResult))
        {
            CPLDebug("HDF5",
                     "Loss of accuracy when reading attribute %s. "
                     "Value " CPL_FRMT_GUIB " will be read as %.17g",
                     pszAttrName, static_cast<GUIntBig>(nVal), dfResult);
        }
    }
#ifdef HDF5_HAVE_FLOAT16
    else if (H5Tequal(H5T_NATIVE_FLOAT16, hAttrNativeType))
    {
        const uint16_t nVal16 = *(static_cast<uint16_t *>(buf));
        const uint32_t nVal32 = CPLHalfToFloat(nVal16);
        float fVal;
        memcpy(&fVal, &nVal32, sizeof(fVal));
        dfResult = fVal;
    }
#endif
    else if (H5Tequal(H5T_NATIVE_FLOAT, hAttrNativeType))
        dfResult = *(static_cast<float *>(buf));
    else if (H5Tequal(H5T_NATIVE_DOUBLE, hAttrNativeType))
        dfResult = *(static_cast<double *>(buf));
    else
    {
        if (bReportError)
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
    if (H5Tequal(H5T_NATIVE_CHAR, TypeID))
        return GDT_UInt8;
    else if (H5Tequal(H5T_NATIVE_SCHAR, TypeID))
        return GDT_Int8;
    else if (H5Tequal(H5T_NATIVE_UCHAR, TypeID))
        return GDT_UInt8;
    else if (H5Tequal(H5T_NATIVE_SHORT, TypeID))
        return GDT_Int16;
    else if (H5Tequal(H5T_NATIVE_USHORT, TypeID))
        return GDT_UInt16;
    else if (H5Tequal(H5T_NATIVE_INT, TypeID))
        return GDT_Int32;
    else if (H5Tequal(H5T_NATIVE_UINT, TypeID))
        return GDT_UInt32;
    else if (H5Tequal(H5T_NATIVE_LONG, TypeID))
    {
#if SIZEOF_UNSIGNED_LONG == 4
        return GDT_Int32;
#else
        return GDT_Unknown;
#endif
    }
    else if (H5Tequal(H5T_NATIVE_ULONG, TypeID))
    {
#if SIZEOF_UNSIGNED_LONG == 4
        return GDT_UInt32;
#else
        return GDT_Unknown;
#endif
    }
    else if (H5Tequal(H5T_NATIVE_FLOAT, TypeID))
        return GDT_Float32;
    else if (H5Tequal(H5T_NATIVE_DOUBLE, TypeID))
        return GDT_Float64;
#ifdef notdef
    else if (H5Tequal(H5T_NATIVE_LLONG, TypeID))
        return GDT_Unknown;
    else if (H5Tequal(H5T_NATIVE_ULLONG, TypeID))
        return GDT_Unknown;
#endif

    return GDT_Unknown;
}

/************************************************************************/
/*                        GH5_CreateAttribute()                         */
/************************************************************************/

bool GH5_CreateAttribute(hid_t loc_id, const char *pszAttrName, hid_t TypeID,
                         unsigned nMaxLen)
{
    hid_t hDataSpace = H5Screate(H5S_SCALAR);
    if (hDataSpace < 0)
        return false;

    hid_t hDataType = H5Tcopy(TypeID);
    if (hDataType < 0)
    {
        H5Sclose(hDataSpace);
        return false;
    }

    if (TypeID == H5T_C_S1)
    {
        if (nMaxLen == VARIABLE_LENGTH)
        {
            H5Tset_size(hDataType, H5T_VARIABLE);
            H5Tset_strpad(hDataType, H5T_STR_NULLTERM);
        }
        else if (H5Tset_size(hDataType, nMaxLen) < 0)
        {
            H5Tclose(hDataType);
            H5Sclose(hDataSpace);
            return false;
        }
    }

    hid_t hAttr =
        H5Acreate(loc_id, pszAttrName, hDataType, hDataSpace, H5P_DEFAULT);
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
/*                         GH5_WriteAttribute()                         */
/************************************************************************/

bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName,
                        const char *pszValue)
{

    hid_t hAttr = H5Aopen_name(loc_id, pszAttrName);
    if (hAttr < 0)
        return false;

    hid_t hDataType = H5Aget_type(hAttr);
    if (hDataType < 0)
    {
        H5Aclose(hAttr);
        return false;
    }

    hid_t hAttrNativeType = H5Tget_native_type(hDataType, H5T_DIR_DEFAULT);
    bool bSuccess = false;
    if (H5Tget_class(hAttrNativeType) == H5T_STRING)
    {
        if (H5Tis_variable_str(hAttrNativeType) > 0)
            bSuccess = H5Awrite(hAttr, hDataType, &pszValue) >= 0;
        else
            bSuccess = H5Awrite(hAttr, hDataType, pszValue) >= 0;
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
/*                         GH5_WriteAttribute()                         */
/************************************************************************/

bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName, double dfValue)
{

    hid_t hAttr = H5Aopen_name(loc_id, pszAttrName);
    if (hAttr < 0)
        return false;

    hid_t hDataType = H5Aget_type(hAttr);
    if (hDataType < 0)
    {
        H5Aclose(hAttr);
        return false;
    }

    hid_t hAttrNativeType = H5Tget_native_type(hDataType, H5T_DIR_DEFAULT);
    bool bSuccess = false;
    if (H5Tequal(hAttrNativeType, H5T_NATIVE_FLOAT))
    {
        float fVal = static_cast<float>(dfValue);
        bSuccess = H5Awrite(hAttr, hAttrNativeType, &fVal) >= 0;
    }
    else if (H5Tequal(hAttrNativeType, H5T_NATIVE_DOUBLE))
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
/*                         GH5_WriteAttribute()                         */
/************************************************************************/

bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName, int nValue)
{

    hid_t hAttr = H5Aopen_name(loc_id, pszAttrName);
    if (hAttr < 0)
        return false;

    hid_t hDataType = H5Aget_type(hAttr);
    if (hDataType < 0)
    {
        H5Aclose(hAttr);
        return false;
    }

    hid_t hEnumType = -1;
    if (H5Tget_class(hDataType) == H5T_ENUM)
    {
        hEnumType = hDataType;
        hDataType = H5Tget_super(hDataType);
    }

    hid_t hAttrNativeType = H5Tget_native_type(hDataType, H5T_DIR_DEFAULT);
    bool bSuccess = false;
    if (hEnumType < 0 && H5Tequal(hAttrNativeType, H5T_NATIVE_INT))
    {
        bSuccess = H5Awrite(hAttr, hAttrNativeType, &nValue) >= 0;
    }
    else if (hEnumType < 0 && H5Tequal(hAttrNativeType, H5T_NATIVE_UINT))
    {
        if (nValue < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attribute %s has value %d which is negative but the type "
                     "is uint",
                     pszAttrName, nValue);
        }
        else
        {
            bSuccess = H5Awrite(hAttr, hAttrNativeType, &nValue) >= 0;
        }
    }
    else if (hEnumType < 0 && H5Tequal(hAttrNativeType, H5T_NATIVE_UINT8))
    {
        if (nValue < 0 ||
            nValue > static_cast<int>(std::numeric_limits<uint8_t>::max()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attribute %s has value %d which is not in the range of a "
                     "uint8",
                     pszAttrName, nValue);
        }
        else
        {
            uint8_t nUint8 = static_cast<uint8_t>(nValue);
            bSuccess = H5Awrite(hAttr, hAttrNativeType, &nUint8) >= 0;
        }
    }
    else if (hEnumType < 0 && H5Tequal(hAttrNativeType, H5T_NATIVE_UINT16))
    {
        if (nValue < 0 ||
            nValue >= static_cast<int>(std::numeric_limits<uint16_t>::max()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attribute %s has value %d which is not in the range of a "
                     "uint16",
                     pszAttrName, nValue);
        }
        else
        {
            uint16_t nUint16 = static_cast<uint16_t>(nValue);
            bSuccess = H5Awrite(hAttr, hAttrNativeType, &nUint16) >= 0;
        }
    }
    else if (hEnumType >= 0 && H5Tequal(hAttrNativeType, H5T_NATIVE_UINT8))
    {
        if (nValue < 0 || nValue > 255)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attribute %s has value %d which is not in the range of a "
                     "uint8",
                     pszAttrName, nValue);
        }
        else
        {
            uint8_t nUint8 = static_cast<uint8_t>(nValue);
            bSuccess = H5Awrite(hAttr, hEnumType, &nUint8) >= 0;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attribute %s is not of type int/uint", pszAttrName);
    }

    H5Tclose(hAttrNativeType);
    H5Aclose(hAttr);
    H5Tclose(hDataType);
    if (hEnumType >= 0)
        H5Tclose(hEnumType);

    return bSuccess;
}

/************************************************************************/
/*                         GH5_WriteAttribute()                         */
/************************************************************************/

bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName, unsigned nValue)
{

    hid_t hAttr = H5Aopen_name(loc_id, pszAttrName);
    if (hAttr < 0)
        return false;

    hid_t hDataType = H5Aget_type(hAttr);
    if (hDataType < 0)
    {
        H5Aclose(hAttr);
        return false;
    }

    hid_t hEnumType = -1;
    if (H5Tget_class(hDataType) == H5T_ENUM)
    {
        hEnumType = hDataType;
        hDataType = H5Tget_super(hDataType);
    }

    hid_t hAttrNativeType = H5Tget_native_type(hDataType, H5T_DIR_DEFAULT);
    bool bSuccess = false;
    if (H5Tequal(hAttrNativeType, H5T_NATIVE_UINT))
    {
        bSuccess = H5Awrite(hAttr, hAttrNativeType, &nValue) >= 0;
    }
    else if (H5Tequal(hAttrNativeType, H5T_NATIVE_INT))
    {
        if (nValue > static_cast<unsigned>(INT_MAX))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Attribute %s has value %u which does not fit on a signed int",
                pszAttrName, nValue);
        }
        else
        {
            bSuccess = H5Awrite(hAttr, hAttrNativeType, &nValue) >= 0;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attribute %s is not of type int/uint", pszAttrName);
    }

    H5Tclose(hAttrNativeType);
    H5Aclose(hAttr);
    H5Tclose(hDataType);
    if (hEnumType >= 0)
        H5Tclose(hEnumType);

    return bSuccess;
}
