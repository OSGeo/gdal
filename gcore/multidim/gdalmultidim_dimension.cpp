/******************************************************************************
 *
 * Name:     gdalmultidim.cpp
 * Project:  GDAL Core
 * Purpose:  GDAL Core C++/Private implementation for multidimensional support
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"

/************************************************************************/
/*                           ~GDALDimension()                           */
/************************************************************************/

GDALDimension::~GDALDimension() = default;

/************************************************************************/
/*                           GDALDimension()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
/** Constructor.
 *
 * @param osParentName Parent name
 * @param osName name
 * @param osType type. See GetType().
 * @param osDirection direction. See GetDirection().
 * @param nSize size.
 */
GDALDimension::GDALDimension(const std::string &osParentName,
                             const std::string &osName,
                             const std::string &osType,
                             const std::string &osDirection, GUInt64 nSize)
    : m_osName(osName),
      m_osFullName(
          !osParentName.empty()
              ? ((osParentName == "/" ? "/" : osParentName + "/") + osName)
              : osName),
      m_osType(osType), m_osDirection(osDirection), m_nSize(nSize)
{
}

//! @endcond

/************************************************************************/
/*                        GetIndexingVariable()                         */
/************************************************************************/

/** Return the variable that is used to index the dimension (if there is one).
 *
 * This is the array, typically one-dimensional, describing the values taken
 * by the dimension.
 */
std::shared_ptr<GDALMDArray> GDALDimension::GetIndexingVariable() const
{
    return nullptr;
}

/************************************************************************/
/*                        SetIndexingVariable()                         */
/************************************************************************/

/** Set the variable that is used to index the dimension.
 *
 * This is the array, typically one-dimensional, describing the values taken
 * by the dimension.
 *
 * Optionally implemented by drivers.
 *
 * Drivers known to implement it: MEM.
 *
 * @param poArray Variable to use to index the dimension.
 * @return true in case of success.
 */
bool GDALDimension::SetIndexingVariable(
    CPL_UNUSED std::shared_ptr<GDALMDArray> poArray)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "SetIndexingVariable() not implemented");
    return false;
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

/** Rename the dimension.
 *
 * This is not implemented by all drivers.
 *
 * Drivers known to implement it: MEM, netCDF, ZARR.
 *
 * This is the same as the C function GDALDimensionRename().
 *
 * @param osNewName New name.
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALDimension::Rename(CPL_UNUSED const std::string &osNewName)
{
    CPLError(CE_Failure, CPLE_NotSupported, "Rename() not implemented");
    return false;
}

/************************************************************************/
/*                             BaseRename()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALDimension::BaseRename(const std::string &osNewName)
{
    m_osFullName.resize(m_osFullName.size() - m_osName.size());
    m_osFullName += osNewName;
    m_osName = osNewName;
}

/************************************************************************/
/*                           ParentDeleted()                            */
/************************************************************************/

void GDALDimension::ParentDeleted()
{
}

/************************************************************************/
/*                           ParentRenamed()                            */
/************************************************************************/

void GDALDimension::ParentRenamed(const std::string &osNewParentFullName)
{
    m_osFullName = osNewParentFullName;
    m_osFullName += "/";
    m_osFullName += m_osName;
}

void GDALDimensionWeakIndexingVar::SetSize(GUInt64 nNewSize)
{
    m_nSize = nNewSize;
}

GDALDimensionWeakIndexingVar::GDALDimensionWeakIndexingVar(
    const std::string &osParentName, const std::string &osName,
    const std::string &osType, const std::string &osDirection, GUInt64 nSize)
    : GDALDimension(osParentName, osName, osType, osDirection, nSize)
{
}

std::shared_ptr<GDALMDArray>
GDALDimensionWeakIndexingVar::GetIndexingVariable() const
{
    return m_poIndexingVariable.lock();
}

// cppcheck-suppress passedByValue
bool GDALDimensionWeakIndexingVar::SetIndexingVariable(
    std::shared_ptr<GDALMDArray> poIndexingVariable)
{
    m_poIndexingVariable = poIndexingVariable;
    return true;
}

//! @endcond
