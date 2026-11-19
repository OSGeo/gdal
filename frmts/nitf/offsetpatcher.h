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

#ifndef OFFSET_PATCHER_INCLUDED
#define OFFSET_PATCHER_INCLUDED

#include "cpl_vsi_virtual.h"

#include <cstdint>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace GDALOffsetPatcher
{

class OffsetPatcher;
class OffsetPatcherBuffer;

constexpr vsi_l_offset INVALID_OFFSET = static_cast<vsi_l_offset>(-1);

/************************************************************************/
/*                         OffsetOrSizeLocation                         */
/************************************************************************/

/** Location of an offset or size */
struct OffsetOrSizeLocation
{
    OffsetPatcherBuffer *buffer = nullptr;
    size_t offsetInBuffer = 0;
};

/************************************************************************/
/*                        OffsetOrSizeReference                         */
/************************************************************************/

/** Reference to an offset or size */
struct OffsetOrSizeReference
{
    OffsetPatcherBuffer *buffer = nullptr;
    size_t offsetInBuffer = 0;
    int objectSizeBytes = 0;
    bool bEndiannessIsLittle = true;
};

/************************************************************************/
/*                       OffsetOrSizeDeclaration                        */
/************************************************************************/

/** Declaration of a to-be-resolved offset location or size. */
class OffsetOrSizeDeclaration
{
  public:
    explicit OffsetOrSizeDeclaration(const std::string &osName,
                                     bool bRelativeToStartOfBuffer = false)
        : m_osName(osName), m_bRelativeToStartOfBuffer(bRelativeToStartOfBuffer)
    {
    }

    bool HasAlreadyRegisteredLocation() const
    {
        return m_location.buffer != nullptr;
    }

    const OffsetOrSizeLocation &GetLocation() const
    {
        return m_location;
    }

    bool SetLocation(OffsetPatcherBuffer *buffer, size_t offsetInBuffer);
    void SetReference(OffsetPatcherBuffer *buffer, size_t offsetInBuffer,
                      int objectSizeBytes, bool bEndiannessIsLittle);

  private:
    friend class OffsetPatcher;

    const std::string m_osName;
    const bool m_bRelativeToStartOfBuffer;
    OffsetOrSizeLocation m_location{};
    std::vector<OffsetOrSizeReference> m_references{};
};

/************************************************************************/
/*                         OffsetPatcherBuffer                          */
/************************************************************************/

/** Buffer that can contain unresolved references to an offset in another
 * buffer or the size of another buffer.
 */
class OffsetPatcherBuffer
{
  public:
    explicit OffsetPatcherBuffer(const std::string &osName,
                                 OffsetPatcher &offsetPatcher,
                                 bool bEndiannessIsLittle)
        : m_osName(osName), m_offsetPatcher(offsetPatcher),
          m_bEndiannessIsLittle(bEndiannessIsLittle)
    {
    }

    void AppendUInt32RefForOffset(const std::string &osName,
                                  bool bRelativeToStartOfBuffer = false);
    void AppendUInt16RefForSizeOfBuffer(const std::string &osBufferName);
    void AppendUInt32RefForSizeOfBuffer(const std::string &osBufferName);
    void AppendByte(uint8_t byVal);
    void AppendUInt16(uint16_t nVal);
    void AppendUInt32(uint32_t nVal);
    void AppendFloat64(double dfVal);
    void AppendString(const std::string &s);

    bool DeclareOffsetAtCurrentPosition(const std::string &osName);

    void DeclareBufferWrittenAtPosition(vsi_l_offset nFileOffset);

    const std::vector<uint8_t> &GetBuffer() const
    {
        return m_abyBuffer;
    }

    vsi_l_offset GetFileLocation() const
    {
        return m_nOffset;
    }

  private:
    friend class OffsetPatcher;

    const std::string m_osName;
    OffsetPatcher &m_offsetPatcher;
    const bool m_bEndiannessIsLittle;
    std::vector<uint8_t> m_abyBuffer{};
    vsi_l_offset m_nOffset = INVALID_OFFSET;

    bool NeedByteSwap() const
    {
#if CPL_IS_LSB
        return !m_bEndiannessIsLittle;
#else
        return m_bEndiannessIsLittle;
#endif
    }
};

/************************************************************************/
/*                            OffsetPatcher                             */
/************************************************************************/

/** Higher level class managing buffers, offset and sizes declarations */
class OffsetPatcher
{
  public:
    OffsetPatcher() = default;

    OffsetPatcherBuffer *CreateBuffer(const std::string &osName,
                                      bool bEndiannessIsLittle);

    OffsetPatcherBuffer *GetBufferFromName(const std::string &osName) const;
    OffsetOrSizeDeclaration *
    GetOffsetDeclaration(const std::string &osName) const;

    bool Finalize(VSILFILE *fp);

  private:
    friend class OffsetPatcherBuffer;

    std::map<std::string, std::unique_ptr<OffsetPatcherBuffer>> m_buffers{};
    std::map<std::string, std::unique_ptr<OffsetOrSizeDeclaration>> m_offsets{};
    std::map<std::string, std::unique_ptr<OffsetOrSizeDeclaration>> m_sizes{};

    OffsetPatcher(const OffsetPatcher &) = delete;
    OffsetPatcher &operator=(const OffsetPatcher &) = delete;
};

}  // namespace GDALOffsetPatcher

#endif  // OFFSET_PATCHER_INCLUDED
