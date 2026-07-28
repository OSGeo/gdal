/******************************************************************************
 *
 * Name:     gdalmultidim_misc.cpp
 * Project:  GDAL Core
 * Purpose:  Implementation of a few utility classes related to multidim
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"
#include "gdalmultidim_priv.h"

/************************************************************************/
/*                           GDALRawResult()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALRawResult::GDALRawResult(GByte *raw, const GDALExtendedDataType &dt,
                             size_t nEltCount)
    : m_dt(dt), m_nEltCount(nEltCount), m_nSize(nEltCount * dt.GetSize()),
      m_raw(raw)
{
}

//! @endcond

/************************************************************************/
/*                           GDALRawResult()                            */
/************************************************************************/

/** Move constructor. */
GDALRawResult::GDALRawResult(GDALRawResult &&other)
    : m_dt(std::move(other.m_dt)), m_nEltCount(other.m_nEltCount),
      m_nSize(other.m_nSize), m_raw(other.m_raw)
{
    other.m_nEltCount = 0;
    other.m_nSize = 0;
    other.m_raw = nullptr;
}

/************************************************************************/
/*                               FreeMe()                               */
/************************************************************************/

void GDALRawResult::FreeMe()
{
    if (m_raw && m_dt.NeedsFreeDynamicMemory())
    {
        GByte *pabyPtr = m_raw;
        const auto nDTSize(m_dt.GetSize());
        for (size_t i = 0; i < m_nEltCount; ++i)
        {
            m_dt.FreeDynamicMemory(pabyPtr);
            pabyPtr += nDTSize;
        }
    }
    VSIFree(m_raw);
}

/************************************************************************/
/*                             operator=()                              */
/************************************************************************/

/** Move assignment. */
GDALRawResult &GDALRawResult::operator=(GDALRawResult &&other)
{
    FreeMe();
    m_dt = std::move(other.m_dt);
    m_nEltCount = other.m_nEltCount;
    m_nSize = other.m_nSize;
    m_raw = other.m_raw;
    other.m_nEltCount = 0;
    other.m_nSize = 0;
    other.m_raw = nullptr;
    return *this;
}

/************************************************************************/
/*                           ~GDALRawResult()                           */
/************************************************************************/

/** Destructor. */
GDALRawResult::~GDALRawResult()
{
    FreeMe();
}

/************************************************************************/
/*                             StealData()                              */
/************************************************************************/

//! @cond Doxygen_Suppress
/** Return buffer to caller which becomes owner of it.
 * Only to be used by GDALAttributeReadAsRaw().
 */
GByte *GDALRawResult::StealData()
{
    GByte *ret = m_raw;
    m_raw = nullptr;
    m_nEltCount = 0;
    m_nSize = 0;
    return ret;
}

//! @endcond

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALMDIAsAttribute::GetDimensions()                  */
/************************************************************************/

const std::vector<std::shared_ptr<GDALDimension>> &
GDALMDIAsAttribute::GetDimensions() const
{
    return m_dims;
}

/************************************************************************/
/*         GDALMDArrayRawBlockInfo::~GDALMDArrayRawBlockInfo()          */
/************************************************************************/

GDALMDArrayRawBlockInfo::~GDALMDArrayRawBlockInfo()
{
    clear();
}

/************************************************************************/
/*                   GDALMDArrayRawBlockInfo::clear()                   */
/************************************************************************/

void GDALMDArrayRawBlockInfo::clear()
{
    CPLFree(pszFilename);
    pszFilename = nullptr;
    CSLDestroy(papszInfo);
    papszInfo = nullptr;
    nOffset = 0;
    nSize = 0;
    CPLFree(pabyInlineData);
    pabyInlineData = nullptr;
}

/************************************************************************/
/*          GDALMDArrayRawBlockInfo::GDALMDArrayRawBlockInfo()          */
/************************************************************************/

GDALMDArrayRawBlockInfo::GDALMDArrayRawBlockInfo(
    const GDALMDArrayRawBlockInfo &other)
    : pszFilename(other.pszFilename ? CPLStrdup(other.pszFilename) : nullptr),
      nOffset(other.nOffset), nSize(other.nSize),
      papszInfo(CSLDuplicate(other.papszInfo)), pabyInlineData(nullptr)
{
    if (other.pabyInlineData)
    {
        pabyInlineData = static_cast<GByte *>(
            VSI_MALLOC_VERBOSE(static_cast<size_t>(other.nSize)));
        if (pabyInlineData)
            memcpy(pabyInlineData, other.pabyInlineData,
                   static_cast<size_t>(other.nSize));
    }
}

/************************************************************************/
/*                 GDALMDArrayRawBlockInfo::operator=()                 */
/************************************************************************/

GDALMDArrayRawBlockInfo &
GDALMDArrayRawBlockInfo::operator=(const GDALMDArrayRawBlockInfo &other)
{
    if (this != &other)
    {
        CPLFree(pszFilename);
        pszFilename =
            other.pszFilename ? CPLStrdup(other.pszFilename) : nullptr;
        nOffset = other.nOffset;
        nSize = other.nSize;
        CSLDestroy(papszInfo);
        papszInfo = CSLDuplicate(other.papszInfo);
        CPLFree(pabyInlineData);
        pabyInlineData = nullptr;
        if (other.pabyInlineData)
        {
            pabyInlineData = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(static_cast<size_t>(other.nSize)));
            if (pabyInlineData)
                memcpy(pabyInlineData, other.pabyInlineData,
                       static_cast<size_t>(other.nSize));
        }
    }
    return *this;
}

/************************************************************************/
/*          GDALMDArrayRawBlockInfo::GDALMDArrayRawBlockInfo()          */
/************************************************************************/

GDALMDArrayRawBlockInfo::GDALMDArrayRawBlockInfo(
    GDALMDArrayRawBlockInfo &&other)
    : pszFilename(other.pszFilename), nOffset(other.nOffset),
      nSize(other.nSize), papszInfo(other.papszInfo),
      pabyInlineData(other.pabyInlineData)
{
    other.pszFilename = nullptr;
    other.papszInfo = nullptr;
    other.pabyInlineData = nullptr;
}

/************************************************************************/
/*                 GDALMDArrayRawBlockInfo::operator=()                 */
/************************************************************************/

GDALMDArrayRawBlockInfo &
GDALMDArrayRawBlockInfo::operator=(GDALMDArrayRawBlockInfo &&other)
{
    if (this != &other)
    {
        std::swap(pszFilename, other.pszFilename);
        nOffset = other.nOffset;
        nSize = other.nSize;
        std::swap(papszInfo, other.papszInfo);
        std::swap(pabyInlineData, other.pabyInlineData);
    }
    return *this;
}

//! @endcond
