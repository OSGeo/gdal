/******************************************************************************
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Manages writing offsets to file locations
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 **********************************************************************
 * Copyright (c) 2026, T-Kartor
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_error.h"

#include <cinttypes>
#include <limits>

#include "offsetpatcher.h"

namespace GDALOffsetPatcher
{

/************************************************************************/
/*                OffsetOrSizeDeclaration::SetLocation()                */
/************************************************************************/

bool OffsetOrSizeDeclaration::SetLocation(OffsetPatcherBuffer *buffer,
                                          size_t offsetInBuffer)
{
    if (m_location.buffer)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Location already declared for object %s", m_osName.c_str());
        return false;
    }
    m_location.buffer = buffer;
    m_location.offsetInBuffer = offsetInBuffer;
    return true;
}

/************************************************************************/
/*               OffsetOrSizeDeclaration::SetReference()                */
/************************************************************************/

void OffsetOrSizeDeclaration::SetReference(OffsetPatcherBuffer *buffer,
                                           size_t offsetInBuffer,
                                           int objectSizeBytes,
                                           bool bEndiannessIsLittle)
{
    OffsetOrSizeReference ref;
    ref.buffer = buffer;
    ref.offsetInBuffer = offsetInBuffer;
    ref.objectSizeBytes = objectSizeBytes;
    ref.bEndiannessIsLittle = bEndiannessIsLittle;
    m_references.push_back(std::move(ref));
}

/************************************************************************/
/*           OffsetPatcherBuffer::AppendUInt32RefForOffset()            */
/************************************************************************/

void OffsetPatcherBuffer::AppendUInt32RefForOffset(
    const std::string &osName, bool bRelativeToStartOfBuffer)
{
    auto oIter = m_offsetPatcher.m_offsets.find(osName);
    if (oIter == m_offsetPatcher.m_offsets.end())
    {
        auto offset = std::make_unique<OffsetOrSizeDeclaration>(
            osName, bRelativeToStartOfBuffer);
        oIter = m_offsetPatcher.m_offsets
                    .insert(std::pair(osName, std::move(offset)))
                    .first;
    }
    oIter->second->SetReference(this, m_abyBuffer.size(),
                                static_cast<int>(sizeof(uint32_t)),
                                m_bEndiannessIsLittle);
    m_abyBuffer.push_back('?');
    m_abyBuffer.push_back('?');
    m_abyBuffer.push_back('?');
    m_abyBuffer.push_back('?');
}

/************************************************************************/
/*        OffsetPatcherBuffer::AppendUInt16RefForSizeOfBuffer()         */
/************************************************************************/

void OffsetPatcherBuffer::AppendUInt16RefForSizeOfBuffer(
    const std::string &osBufferName)
{
    auto oIter = m_offsetPatcher.m_sizes.find(osBufferName);
    if (oIter == m_offsetPatcher.m_sizes.end())
    {
        auto size = std::make_unique<OffsetOrSizeDeclaration>(osBufferName);
        oIter = m_offsetPatcher.m_sizes
                    .insert(std::pair(osBufferName, std::move(size)))
                    .first;
    }
    oIter->second->SetReference(this, m_abyBuffer.size(),
                                static_cast<int>(sizeof(uint16_t)),
                                m_bEndiannessIsLittle);
    m_abyBuffer.push_back('?');
    m_abyBuffer.push_back('?');
}

/************************************************************************/
/*        OffsetPatcherBuffer::AppendUInt32RefForSizeOfBuffer()         */
/************************************************************************/

void OffsetPatcherBuffer::AppendUInt32RefForSizeOfBuffer(
    const std::string &osBufferName)
{
    auto oIter = m_offsetPatcher.m_sizes.find(osBufferName);
    if (oIter == m_offsetPatcher.m_sizes.end())
    {
        auto size = std::make_unique<OffsetOrSizeDeclaration>(osBufferName);
        oIter = m_offsetPatcher.m_sizes
                    .insert(std::pair(osBufferName, std::move(size)))
                    .first;
    }
    oIter->second->SetReference(this, m_abyBuffer.size(),
                                static_cast<int>(sizeof(uint32_t)),
                                m_bEndiannessIsLittle);
    m_abyBuffer.push_back('?');
    m_abyBuffer.push_back('?');
    m_abyBuffer.push_back('?');
    m_abyBuffer.push_back('?');
}

/************************************************************************/
/*                  OffsetPatcherBuffer::AppendByte()                   */
/************************************************************************/

void OffsetPatcherBuffer::AppendByte(uint8_t byVal)
{
    m_abyBuffer.push_back(byVal);
}

/************************************************************************/
/*                 OffsetPatcherBuffer::AppendUInt16()                  */
/************************************************************************/

void OffsetPatcherBuffer::AppendUInt16(uint16_t nVal)
{
    if (NeedByteSwap())
        CPL_SWAP16PTR(&nVal);
    const GByte *pabyVal = reinterpret_cast<const GByte *>(&nVal);
    m_abyBuffer.insert(m_abyBuffer.end(), pabyVal, pabyVal + sizeof(nVal));
}

/************************************************************************/
/*                 OffsetPatcherBuffer::AppendUInt32()                  */
/************************************************************************/

void OffsetPatcherBuffer::AppendUInt32(uint32_t nVal)
{
    if (NeedByteSwap())
        CPL_SWAP32PTR(&nVal);
    const GByte *pabyVal = reinterpret_cast<const GByte *>(&nVal);
    m_abyBuffer.insert(m_abyBuffer.end(), pabyVal, pabyVal + sizeof(nVal));
}

/************************************************************************/
/*                 OffsetPatcherBuffer::AppendFloat64()                 */
/************************************************************************/

void OffsetPatcherBuffer::AppendFloat64(double dfVal)
{
    if (NeedByteSwap())
        CPL_SWAP64PTR(&dfVal);
    const GByte *pabyVal = reinterpret_cast<const GByte *>(&dfVal);
    m_abyBuffer.insert(m_abyBuffer.end(), pabyVal, pabyVal + sizeof(dfVal));
}

/************************************************************************/
/*                 OffsetPatcherBuffer::AppendString()                  */
/************************************************************************/

void OffsetPatcherBuffer::AppendString(const std::string &s)
{
    const GByte *pabyVal = reinterpret_cast<const GByte *>(s.data());
    m_abyBuffer.insert(m_abyBuffer.end(), pabyVal, pabyVal + s.size());
}

/************************************************************************/
/*        OffsetPatcherBuffer::DeclareOffsetAtCurrentPosition()         */
/************************************************************************/

bool OffsetPatcherBuffer::DeclareOffsetAtCurrentPosition(
    const std::string &osName)
{
    auto oIter = m_offsetPatcher.m_offsets.find(osName);
    if (oIter == m_offsetPatcher.m_offsets.end())
    {
        auto offset = std::make_unique<OffsetOrSizeDeclaration>(osName);
        oIter = m_offsetPatcher.m_offsets
                    .insert(std::pair(osName, std::move(offset)))
                    .first;
    }
    return oIter->second->SetLocation(this, m_abyBuffer.size());
}

/************************************************************************/
/*        OffsetPatcherBuffer::DeclareBufferWrittenAtPosition()         */
/************************************************************************/

void OffsetPatcherBuffer::DeclareBufferWrittenAtPosition(
    vsi_l_offset nFileOffset)
{
    m_nOffset = nFileOffset;
}

/************************************************************************/
/*                    OffsetPatcher::CreateBuffer()                     */
/************************************************************************/

OffsetPatcherBuffer *OffsetPatcher::CreateBuffer(const std::string &osName,
                                                 bool bEndiannessIsLittle)
{
    if (m_buffers.find(osName) != m_buffers.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Buffer with name '%s' already created", osName.c_str());
        return nullptr;
    }
    auto buffer = std::make_unique<OffsetPatcherBuffer>(osName, *this,
                                                        bEndiannessIsLittle);
    return m_buffers.insert(std::pair(osName, std::move(buffer)))
        .first->second.get();
}

/************************************************************************/
/*                  OffsetPatcher::GetBufferFromName()                  */
/************************************************************************/

OffsetPatcherBuffer *
OffsetPatcher::GetBufferFromName(const std::string &osName) const
{
    auto oIter = m_buffers.find(osName);
    if (oIter != m_buffers.end())
        return oIter->second.get();
    return nullptr;
}

/************************************************************************/
/*                OffsetPatcher::GetOffsetDeclaration()                 */
/************************************************************************/

OffsetOrSizeDeclaration *
OffsetPatcher::GetOffsetDeclaration(const std::string &osName) const
{
    auto oIter = m_offsets.find(osName);
    if (oIter != m_offsets.end())
        return oIter->second.get();
    return nullptr;
}

/************************************************************************/
/*                      OffsetPatcher::Finalize()                       */
/************************************************************************/

bool OffsetPatcher::Finalize(VSILFILE *fp)
{
    for (const auto &[offsetName, declaration] : m_offsets)
    {
        if (declaration->m_location.buffer == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Location for offset '%s' has not been set",
                     offsetName.c_str());
            return false;
        }
        if (declaration->m_location.buffer->m_nOffset == INVALID_OFFSET)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Buffer '%s' that contains location for offset '%s' has "
                     "not been written to file",
                     declaration->m_location.buffer->m_osName.c_str(),
                     offsetName.c_str());
            return false;
        }

        const vsi_l_offset nOffsetPosition =
            (declaration->m_bRelativeToStartOfBuffer
                 ? 0
                 : declaration->m_location.buffer->m_nOffset) +
            declaration->m_location.offsetInBuffer;

        if (nOffsetPosition > UINT32_MAX)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Position of offset '%s' does not fit on a uint32",
                     offsetName.c_str());
            return false;
        }

        if (declaration->m_references.empty())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "No reference found to offset '%s'", offsetName.c_str());
        }
        else
        {
            for (const auto &ref : declaration->m_references)
            {
                if (ref.buffer->m_nOffset == INVALID_OFFSET)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Buffer '%s' that contains a reference to offset "
                             "'%s' has not been written to file",
                             ref.buffer->m_osName.c_str(), offsetName.c_str());
                    return false;
                }

                if (ref.objectSizeBytes != 4)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Buffer '%s' that contains a reference to offset "
                             "'%s' has an unsupported offset size %d",
                             ref.buffer->m_osName.c_str(), offsetName.c_str(),
                             ref.objectSizeBytes);
                    return false;
                }

#if CPL_IS_LSB
                const bool bNeedByteSwap = !ref.bEndiannessIsLittle;
#else
                const bool bNeedByteSwap = ref.bEndiannessIsLittle;
#endif

                uint32_t nVal = static_cast<uint32_t>(nOffsetPosition);
                if (bNeedByteSwap)
                {
                    CPL_SWAP32PTR(&nVal);
                }

                const auto nRefPosition =
                    ref.buffer->m_nOffset + ref.offsetInBuffer;
                if (fp->Seek(nRefPosition, SEEK_SET) != 0 ||
                    fp->Write(&nVal, 1, sizeof(nVal)) != sizeof(nVal))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot write reference to offset '%s' at "
                             "position %" PRIu64,
                             ref.buffer->m_osName.c_str(),
                             static_cast<uint64_t>(nRefPosition));
                    return false;
                }
            }
        }
    }

    for (const auto &[bufferName, declaration] : m_sizes)
    {
        size_t nBufferSize = 0;
        if (bufferName.find('+') == std::string::npos)
        {
            auto oReferencedBufferIter = m_buffers.find(bufferName);
            if (oReferencedBufferIter == m_buffers.end())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "No buffer registered with name '%s'",
                         bufferName.c_str());
                return false;
            }
            const auto poReferencedBuffer = oReferencedBufferIter->second.get();
            if (poReferencedBuffer->m_nOffset == INVALID_OFFSET)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Buffer '%s' has not been written to file",
                         bufferName.c_str());
                return false;
            }
            nBufferSize = poReferencedBuffer->GetBuffer().size();
        }
        else
        {
            const CPLStringList aosBufferNames(
                CSLTokenizeString2(bufferName.c_str(), "+", 0));
            for (const char *pszBuffer : aosBufferNames)
            {
                auto oReferencedBufferIter = m_buffers.find(pszBuffer);
                if (oReferencedBufferIter == m_buffers.end())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "No buffer registered with name '%s'", pszBuffer);
                    return false;
                }
                const auto poReferencedBuffer =
                    oReferencedBufferIter->second.get();
                if (poReferencedBuffer->m_nOffset == INVALID_OFFSET)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Buffer '%s' has not been written to file",
                             pszBuffer);
                    return false;
                }
                nBufferSize += poReferencedBuffer->GetBuffer().size();
            }
        }

        for (const auto &ref : declaration->m_references)
        {
            if (ref.buffer->m_nOffset == INVALID_OFFSET)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Buffer '%s' that contains a reference to size "
                         "'%s' has not been written to file",
                         ref.buffer->m_osName.c_str(), bufferName.c_str());
                return false;
            }

            if (ref.objectSizeBytes != 2 && ref.objectSizeBytes != 4)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Buffer '%s' that contains a reference to size "
                         "'%s' has an unsupported offset size %d",
                         ref.buffer->m_osName.c_str(), bufferName.c_str(),
                         ref.objectSizeBytes);
                return false;
            }

#if CPL_IS_LSB
            const bool bNeedByteSwap = !ref.bEndiannessIsLittle;
#else
            const bool bNeedByteSwap = ref.bEndiannessIsLittle;
#endif

            const auto nRefPosition =
                ref.buffer->m_nOffset + ref.offsetInBuffer;

            if (ref.objectSizeBytes == 2)
            {
                if (nBufferSize > std::numeric_limits<uint16_t>::max())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot write reference to size '%s' at "
                             "position %" PRIu64
                             " on a uint16 because the size exceeds uint16",
                             ref.buffer->m_osName.c_str(),
                             static_cast<uint64_t>(nRefPosition));
                    return false;
                }
                uint16_t nVal = static_cast<uint16_t>(nBufferSize);
                if (bNeedByteSwap)
                {
                    CPL_SWAP16PTR(&nVal);
                }

                if (fp->Seek(nRefPosition, SEEK_SET) != 0 ||
                    fp->Write(&nVal, 1, sizeof(nVal)) != sizeof(nVal))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot write reference to size '%s' at "
                             "position %" PRIu64,
                             ref.buffer->m_osName.c_str(),
                             static_cast<uint64_t>(nRefPosition));
                    return false;
                }
            }
            else
            {
                CPLAssert(ref.objectSizeBytes == 4);
                uint32_t nVal = static_cast<uint32_t>(nBufferSize);
                if (bNeedByteSwap)
                {
                    CPL_SWAP32PTR(&nVal);
                }

                if (fp->Seek(nRefPosition, SEEK_SET) != 0 ||
                    fp->Write(&nVal, 1, sizeof(nVal)) != sizeof(nVal))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot write reference to size '%s' at "
                             "position %" PRIu64,
                             ref.buffer->m_osName.c_str(),
                             static_cast<uint64_t>(nRefPosition));
                    return false;
                }
            }
        }
    }

    return true;
}

}  // namespace GDALOffsetPatcher
