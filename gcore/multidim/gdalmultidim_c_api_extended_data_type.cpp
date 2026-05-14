/******************************************************************************
 *
 * Name:     gdalmultidim_c_api_extended_data_type.cpp
 * Project:  GDAL Core
 * Purpose:  C API for GDALExtendedDataType
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
#include "gdal_rat.h"

/************************************************************************/
/*                     GDALExtendedDataTypeCreate()                     */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_NUMERIC.
 *
 * This is the same as the C++ method GDALExtendedDataType::Create()
 *
 * The returned handle should be freed with GDALExtendedDataTypeRelease().
 *
 * @param eType Numeric data type. Must be different from GDT_Unknown and
 * GDT_TypeCount
 *
 * @return a new GDALExtendedDataTypeH handle, or nullptr.
 */
GDALExtendedDataTypeH GDALExtendedDataTypeCreate(GDALDataType eType)
{
    if (CPL_UNLIKELY(eType == GDT_Unknown || eType == GDT_TypeCount))
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Illegal GDT_Unknown/GDT_TypeCount argument");
        return nullptr;
    }
    return new GDALExtendedDataTypeHS(
        new GDALExtendedDataType(GDALExtendedDataType::Create(eType)));
}

/************************************************************************/
/*                  GDALExtendedDataTypeCreateString()                  */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_STRING.
 *
 * This is the same as the C++ method GDALExtendedDataType::CreateString()
 *
 * The returned handle should be freed with GDALExtendedDataTypeRelease().
 *
 * @return a new GDALExtendedDataTypeH handle, or nullptr.
 */
GDALExtendedDataTypeH GDALExtendedDataTypeCreateString(size_t nMaxStringLength)
{
    return new GDALExtendedDataTypeHS(new GDALExtendedDataType(
        GDALExtendedDataType::CreateString(nMaxStringLength)));
}

/************************************************************************/
/*                 GDALExtendedDataTypeCreateStringEx()                 */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_STRING.
 *
 * This is the same as the C++ method GDALExtendedDataType::CreateString()
 *
 * The returned handle should be freed with GDALExtendedDataTypeRelease().
 *
 * @return a new GDALExtendedDataTypeH handle, or nullptr.
 * @since GDAL 3.4
 */
GDALExtendedDataTypeH
GDALExtendedDataTypeCreateStringEx(size_t nMaxStringLength,
                                   GDALExtendedDataTypeSubType eSubType)
{
    return new GDALExtendedDataTypeHS(new GDALExtendedDataType(
        GDALExtendedDataType::CreateString(nMaxStringLength, eSubType)));
}

/************************************************************************/
/*                 GDALExtendedDataTypeCreateCompound()                 */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_COMPOUND.
 *
 * This is the same as the C++ method GDALExtendedDataType::Create(const
 * std::string&, size_t, std::vector<std::unique_ptr<GDALEDTComponent>>&&)
 *
 * The returned handle should be freed with GDALExtendedDataTypeRelease().
 *
 * @param pszName Type name.
 * @param nTotalSize Total size of the type in bytes.
 *                   Should be large enough to store all components.
 * @param nComponents Number of components in comps array.
 * @param comps Components.
 * @return a new GDALExtendedDataTypeH handle, or nullptr.
 */
GDALExtendedDataTypeH
GDALExtendedDataTypeCreateCompound(const char *pszName, size_t nTotalSize,
                                   size_t nComponents,
                                   const GDALEDTComponentH *comps)
{
    std::vector<std::unique_ptr<GDALEDTComponent>> compsCpp;
    for (size_t i = 0; i < nComponents; i++)
    {
        compsCpp.emplace_back(
            std::make_unique<GDALEDTComponent>(*(comps[i]->m_poImpl.get())));
    }
    auto dt = GDALExtendedDataType::Create(pszName ? pszName : "", nTotalSize,
                                           std::move(compsCpp));
    if (dt.GetClass() != GEDTC_COMPOUND)
        return nullptr;
    return new GDALExtendedDataTypeHS(new GDALExtendedDataType(std::move(dt)));
}

/************************************************************************/
/*                    GDALExtendedDataTypeRelease()                     */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALExtendedDataTypeH.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALExtendedDataTypeRelease(GDALExtendedDataTypeH hEDT)
{
    delete hEDT;
}

/************************************************************************/
/*                    GDALExtendedDataTypeGetName()                     */
/************************************************************************/

/** Return type name.
 *
 * This is the same as the C++ method GDALExtendedDataType::GetName()
 */
const char *GDALExtendedDataTypeGetName(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1(hEDT, __func__, "");
    return hEDT->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                    GDALExtendedDataTypeGetClass()                    */
/************************************************************************/

/** Return type class.
 *
 * This is the same as the C++ method GDALExtendedDataType::GetClass()
 */
GDALExtendedDataTypeClass
GDALExtendedDataTypeGetClass(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1(hEDT, __func__, GEDTC_NUMERIC);
    return hEDT->m_poImpl->GetClass();
}

/************************************************************************/
/*               GDALExtendedDataTypeGetNumericDataType()               */
/************************************************************************/

/** Return numeric data type (only valid when GetClass() == GEDTC_NUMERIC)
 *
 * This is the same as the C++ method GDALExtendedDataType::GetNumericDataType()
 */
GDALDataType GDALExtendedDataTypeGetNumericDataType(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1(hEDT, __func__, GDT_Unknown);
    return hEDT->m_poImpl->GetNumericDataType();
}

/************************************************************************/
/*                    GDALExtendedDataTypeGetSize()                     */
/************************************************************************/

/** Return data type size in bytes.
 *
 * This is the same as the C++ method GDALExtendedDataType::GetSize()
 */
size_t GDALExtendedDataTypeGetSize(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1(hEDT, __func__, 0);
    return hEDT->m_poImpl->GetSize();
}

/************************************************************************/
/*               GDALExtendedDataTypeGetMaxStringLength()               */
/************************************************************************/

/** Return the maximum length of a string in bytes.
 *
 * 0 indicates unknown/unlimited string.
 *
 * This is the same as the C++ method GDALExtendedDataType::GetMaxStringLength()
 */
size_t GDALExtendedDataTypeGetMaxStringLength(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1(hEDT, __func__, 0);
    return hEDT->m_poImpl->GetMaxStringLength();
}

/************************************************************************/
/*                  GDALExtendedDataTypeCanConvertTo()                  */
/************************************************************************/

/** Return whether this data type can be converted to the other one.
 *
 * This is the same as the C function GDALExtendedDataType::CanConvertTo()
 *
 * @param hSourceEDT Source data type for the conversion being considered.
 * @param hTargetEDT Target data type for the conversion being considered.
 * @return TRUE if hSourceEDT can be convert to hTargetEDT. FALSE otherwise.
 */
int GDALExtendedDataTypeCanConvertTo(GDALExtendedDataTypeH hSourceEDT,
                                     GDALExtendedDataTypeH hTargetEDT)
{
    VALIDATE_POINTER1(hSourceEDT, __func__, FALSE);
    VALIDATE_POINTER1(hTargetEDT, __func__, FALSE);
    return hSourceEDT->m_poImpl->CanConvertTo(*(hTargetEDT->m_poImpl));
}

/************************************************************************/
/*                     GDALExtendedDataTypeEquals()                     */
/************************************************************************/

/** Return whether this data type is equal to another one.
 *
 * This is the same as the C++ method GDALExtendedDataType::operator==()
 *
 * @param hFirstEDT First data type.
 * @param hSecondEDT Second data type.
 * @return TRUE if they are equal. FALSE otherwise.
 */
int GDALExtendedDataTypeEquals(GDALExtendedDataTypeH hFirstEDT,
                               GDALExtendedDataTypeH hSecondEDT)
{
    VALIDATE_POINTER1(hFirstEDT, __func__, FALSE);
    VALIDATE_POINTER1(hSecondEDT, __func__, FALSE);
    return *(hFirstEDT->m_poImpl) == *(hSecondEDT->m_poImpl);
}

/************************************************************************/
/*                   GDALExtendedDataTypeGetSubType()                   */
/************************************************************************/

/** Return the subtype of a type.
 *
 * This is the same as the C++ method GDALExtendedDataType::GetSubType()
 *
 * @param hEDT Data type.
 * @return subtype.
 * @since 3.4
 */
GDALExtendedDataTypeSubType
GDALExtendedDataTypeGetSubType(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1(hEDT, __func__, GEDTST_NONE);
    return hEDT->m_poImpl->GetSubType();
}

/************************************************************************/
/*                     GDALExtendedDataTypeGetRAT()                     */
/************************************************************************/

/** Return associated raster attribute table, when there is one.
 *
 * * For the netCDF driver, the RAT will capture enumerated types, with
 * a "value" column with an integer value and a "name" column with the
 * associated name.
 * This is the same as the C++ method GDALExtendedDataType::GetRAT()
 *
 * @param hEDT Data type.
 * @return raster attribute (owned by GDALExtendedDataTypeH), or NULL
 * @since 3.12
 */
GDALRasterAttributeTableH GDALExtendedDataTypeGetRAT(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1(hEDT, __func__, nullptr);
    return GDALRasterAttributeTable::ToHandle(
        const_cast<GDALRasterAttributeTable *>(hEDT->m_poImpl->GetRAT()));
}

/************************************************************************/
/*                 GDALExtendedDataTypeGetComponents()                  */
/************************************************************************/

/** Return the components of the data type (only valid when GetClass() ==
 * GEDTC_COMPOUND)
 *
 * The returned array and its content must be freed with
 * GDALExtendedDataTypeFreeComponents(). If only the array itself needs to be
 * freed, CPLFree() should be called (and GDALExtendedDataTypeRelease() on
 * individual array members).
 *
 * This is the same as the C++ method GDALExtendedDataType::GetComponents()
 *
 * @param hEDT Data type
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @return an array of *pnCount components.
 */
GDALEDTComponentH *GDALExtendedDataTypeGetComponents(GDALExtendedDataTypeH hEDT,
                                                     size_t *pnCount)
{
    VALIDATE_POINTER1(hEDT, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    const auto &components = hEDT->m_poImpl->GetComponents();
    auto ret = static_cast<GDALEDTComponentH *>(
        CPLMalloc(sizeof(GDALEDTComponentH) * components.size()));
    for (size_t i = 0; i < components.size(); i++)
    {
        ret[i] = new GDALEDTComponentHS(*components[i].get());
    }
    *pnCount = components.size();
    return ret;
}

/************************************************************************/
/*                 GDALExtendedDataTypeFreeComponents()                 */
/************************************************************************/

/** Free the return of GDALExtendedDataTypeGetComponents().
 *
 * @param components return value of GDALExtendedDataTypeGetComponents()
 * @param nCount *pnCount value returned by GDALExtendedDataTypeGetComponents()
 */
void GDALExtendedDataTypeFreeComponents(GDALEDTComponentH *components,
                                        size_t nCount)
{
    for (size_t i = 0; i < nCount; i++)
    {
        delete components[i];
    }
    CPLFree(components);
}

/************************************************************************/
/*                       GDALEDTComponentCreate()                       */
/************************************************************************/

/** Create a new GDALEDTComponent.
 *
 * The returned value must be freed with GDALEDTComponentRelease().
 *
 * This is the same as the C++ constructor GDALEDTComponent::GDALEDTComponent().
 */
GDALEDTComponentH GDALEDTComponentCreate(const char *pszName, size_t nOffset,
                                         GDALExtendedDataTypeH hType)
{
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    VALIDATE_POINTER1(hType, __func__, nullptr);
    return new GDALEDTComponentHS(
        GDALEDTComponent(pszName, nOffset, *(hType->m_poImpl.get())));
}

/************************************************************************/
/*                      GDALEDTComponentRelease()                       */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALEDTComponentH.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALEDTComponentRelease(GDALEDTComponentH hComp)
{
    delete hComp;
}

/************************************************************************/
/*                      GDALEDTComponentGetName()                       */
/************************************************************************/

/** Return the name.
 *
 * The returned pointer is valid until hComp is released.
 *
 * This is the same as the C++ method GDALEDTComponent::GetName().
 */
const char *GDALEDTComponentGetName(GDALEDTComponentH hComp)
{
    VALIDATE_POINTER1(hComp, __func__, nullptr);
    return hComp->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                     GDALEDTComponentGetOffset()                      */
/************************************************************************/

/** Return the offset (in bytes) of the component in the compound data type.
 *
 * This is the same as the C++ method GDALEDTComponent::GetOffset().
 */
size_t GDALEDTComponentGetOffset(GDALEDTComponentH hComp)
{
    VALIDATE_POINTER1(hComp, __func__, 0);
    return hComp->m_poImpl->GetOffset();
}

/************************************************************************/
/*                      GDALEDTComponentGetType()                       */
/************************************************************************/

/** Return the data type of the component.
 *
 * This is the same as the C++ method GDALEDTComponent::GetType().
 */
GDALExtendedDataTypeH GDALEDTComponentGetType(GDALEDTComponentH hComp)
{
    VALIDATE_POINTER1(hComp, __func__, nullptr);
    return new GDALExtendedDataTypeHS(
        new GDALExtendedDataType(hComp->m_poImpl->GetType()));
}
