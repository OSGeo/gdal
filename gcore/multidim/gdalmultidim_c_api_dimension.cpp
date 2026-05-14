/******************************************************************************
 *
 * Name:     gdalmultidim_c_api_dimension.cpp
 * Project:  GDAL Core
 * Purpose:  C API for GDALDimension
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"
#include "gdalmultidim_priv.h"
#include "gdal_fwd.h"

/************************************************************************/
/*                        GDALDimensionRelease()                        */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALDimension.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALDimensionRelease(GDALDimensionH hDim)
{
    delete hDim;
}

/************************************************************************/
/*                       GDALReleaseDimensions()                        */
/************************************************************************/

/** Free the return of GDALGroupGetDimensions() or GDALMDArrayGetDimensions()
 *
 * @param dims return pointer of above methods
 * @param nCount *pnCount value returned by above methods
 */
void GDALReleaseDimensions(GDALDimensionH *dims, size_t nCount)
{
    for (size_t i = 0; i < nCount; i++)
    {
        delete dims[i];
    }
    CPLFree(dims);
}

/************************************************************************/
/*                        GDALDimensionGetName()                        */
/************************************************************************/

/** Return dimension name.
 *
 * This is the same as the C++ method GDALDimension::GetName()
 */
const char *GDALDimensionGetName(GDALDimensionH hDim)
{
    VALIDATE_POINTER1(hDim, __func__, nullptr);
    return hDim->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                      GDALDimensionGetFullName()                      */
/************************************************************************/

/** Return dimension full name.
 *
 * This is the same as the C++ method GDALDimension::GetFullName()
 */
const char *GDALDimensionGetFullName(GDALDimensionH hDim)
{
    VALIDATE_POINTER1(hDim, __func__, nullptr);
    return hDim->m_poImpl->GetFullName().c_str();
}

/************************************************************************/
/*                        GDALDimensionGetType()                        */
/************************************************************************/

/** Return dimension type.
 *
 * This is the same as the C++ method GDALDimension::GetType()
 */
const char *GDALDimensionGetType(GDALDimensionH hDim)
{
    VALIDATE_POINTER1(hDim, __func__, nullptr);
    return hDim->m_poImpl->GetType().c_str();
}

/************************************************************************/
/*                     GDALDimensionGetDirection()                      */
/************************************************************************/

/** Return dimension direction.
 *
 * This is the same as the C++ method GDALDimension::GetDirection()
 */
const char *GDALDimensionGetDirection(GDALDimensionH hDim)
{
    VALIDATE_POINTER1(hDim, __func__, nullptr);
    return hDim->m_poImpl->GetDirection().c_str();
}

/************************************************************************/
/*                        GDALDimensionGetSize()                        */
/************************************************************************/

/** Return the size, that is the number of values along the dimension.
 *
 * This is the same as the C++ method GDALDimension::GetSize()
 */
GUInt64 GDALDimensionGetSize(GDALDimensionH hDim)
{
    VALIDATE_POINTER1(hDim, __func__, 0);
    return hDim->m_poImpl->GetSize();
}

/************************************************************************/
/*                  GDALDimensionGetIndexingVariable()                  */
/************************************************************************/

/** Return the variable that is used to index the dimension (if there is one).
 *
 * This is the array, typically one-dimensional, describing the values taken
 * by the dimension.
 *
 * The returned value should be freed with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALDimension::GetIndexingVariable()
 */
GDALMDArrayH GDALDimensionGetIndexingVariable(GDALDimensionH hDim)
{
    VALIDATE_POINTER1(hDim, __func__, nullptr);
    auto var(hDim->m_poImpl->GetIndexingVariable());
    if (!var)
        return nullptr;
    return new GDALMDArrayHS(var);
}

/************************************************************************/
/*                  GDALDimensionSetIndexingVariable()                  */
/************************************************************************/

/** Set the variable that is used to index the dimension.
 *
 * This is the array, typically one-dimensional, describing the values taken
 * by the dimension.
 *
 * This is the same as the C++ method GDALDimension::SetIndexingVariable()
 *
 * @return TRUE in case of success.
 */
int GDALDimensionSetIndexingVariable(GDALDimensionH hDim, GDALMDArrayH hArray)
{
    VALIDATE_POINTER1(hDim, __func__, FALSE);
    return hDim->m_poImpl->SetIndexingVariable(hArray ? hArray->m_poImpl
                                                      : nullptr);
}

/************************************************************************/
/*                        GDALDimensionRename()                         */
/************************************************************************/

/** Rename the dimension.
 *
 * This is not implemented by all drivers.
 *
 * Drivers known to implement it: MEM, netCDF.
 *
 * This is the same as the C++ method GDALDimension::Rename()
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALDimensionRename(GDALDimensionH hDim, const char *pszNewName)
{
    VALIDATE_POINTER1(hDim, __func__, false);
    VALIDATE_POINTER1(pszNewName, __func__, false);
    return hDim->m_poImpl->Rename(pszNewName);
}
