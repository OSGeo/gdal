/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  WKB geometry related methods
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_WKB_H_INCLUDED
#define OGR_WKB_H_INCLUDED

#include <cstdint>

#include "cpl_port.h"
#include "ogr_core.h"

#include <vector>

bool CPL_DLL OGRWKBGetGeomType(const GByte *pabyWkb, size_t nWKBSize,
                               bool &bNeedSwap, uint32_t &nType);
bool OGRWKBPolygonGetArea(const GByte *&pabyWkb, size_t &nWKBSize,
                          double &dfArea);
bool OGRWKBMultiPolygonGetArea(const GByte *&pabyWkb, size_t &nWKBSize,
                               double &dfArea);

bool CPL_DLL OGRWKBGetBoundingBox(const GByte *pabyWkb, size_t nWKBSize,
                                  OGREnvelope3D &sEnvelope);

bool CPL_DLL OGRWKBGetBoundingBox(const GByte *pabyWkb, size_t nWKBSize,
                                  OGREnvelope &sEnvelope);

bool CPL_DLL OGRWKBIntersectsPessimistic(const GByte *pabyWkb, size_t nWKBSize,
                                         const OGREnvelope &sEnvelope);

void CPL_DLL OGRWKBFixupCounterClockWiseExternalRing(GByte *pabyWkb,
                                                     size_t nWKBSize);

/** Modifies a PostGIS-style Extended WKB geometry to a regular WKB one.
 * pabyEWKB will be modified in place.
 * The return value will be either at the beginning of pabyEWKB or 4 bytes
 * later, and thus has the same lifetime of pabyEWKB. The function returns in
 * nWKBSizeOut the length of the returned WKB pointer. pnSRIDOut may be NULL, or
 * if not NULL, the function will return in it the SRID, if present, or INT_MIN
 * if not present.
 */
const GByte CPL_DLL *WKBFromEWKB(GByte *pabyEWKB, size_t nEWKBSize,
                                 size_t &nWKBSizeOut, int *pnSRIDOut);

/** Object to update point coordinates in a WKB geometry */
class CPL_DLL OGRWKBPointUpdater
{
  public:
    OGRWKBPointUpdater();
    virtual ~OGRWKBPointUpdater() = default;

    /** Update method */
    virtual bool update(bool bNeedSwap, void *x, void *y, void *z, void *m) = 0;
};

bool CPL_DLL OGRWKBUpdatePoints(GByte *pabyWkb, size_t nWKBSize,
                                OGRWKBPointUpdater &oUpdater);

/** Transformation cache */
struct CPL_DLL OGRWKBTransformCache
{
#ifdef OGR_WKB_TRANSFORM_ALL_AT_ONCE
    std::vector<bool> abNeedSwap{};
    std::vector<bool> abIsEmpty{};
    std::vector<void *> apdfX{};
    std::vector<void *> apdfY{};
    std::vector<void *> apdfZ{};
    std::vector<void *> apdfM{};
    std::vector<double> adfX{};
    std::vector<double> adfY{};
    std::vector<double> adfZ{};
    std::vector<double> adfM{};
    std::vector<int> anErrorCodes{};

    void clear();
#endif
};

class OGRCoordinateTransformation;
bool CPL_DLL OGRWKBTransform(GByte *pabyWkb, size_t nWKBSize,
                             OGRCoordinateTransformation *poCT,
                             OGRWKBTransformCache &oCache,
                             OGREnvelope3D &sEnvelope);

/************************************************************************/
/*                       OGRAppendBuffer                                */
/************************************************************************/

/** Append buffer that can be grown dynamically. */
class CPL_DLL OGRAppendBuffer
{
  public:
    /** Constructor */
    OGRAppendBuffer();

    /** Destructor */
    virtual ~OGRAppendBuffer();

    /** Return the pointer at which nItemSize bytes can be written,
     * or nullptr in case of error.
     */
    inline void *GetPtrForNewBytes(size_t nItemSize)
    {
        if (nItemSize > m_nCapacity - m_nSize)
        {
            if (!Grow(nItemSize))
                return nullptr;
        }
        void *pRet = static_cast<GByte *>(m_pRawBuffer) + m_nSize;
        m_nSize += nItemSize;
        return pRet;
    }

    /** Return the number of valid bytes in the buffer. */
    inline size_t GetSize() const
    {
        return m_nSize;
    }

  protected:
    /** Capacity of the buffer (ie number of bytes allocated). */
    size_t m_nCapacity = 0;

    /** Number of valid bytes in the buffer. */
    size_t m_nSize = 0;

    /** Raw buffer pointer. */
    void *m_pRawBuffer = nullptr;

    /** Extend the capacity of m_pRawBuffer to be at least m_nSize + nItemSize
     * large.
     */
    virtual bool Grow(size_t nItemSize) = 0;

  private:
    OGRAppendBuffer(const OGRAppendBuffer &) = delete;
    OGRAppendBuffer &operator=(const OGRAppendBuffer &) = delete;
};

/************************************************************************/
/*                       OGRWKTToWKBTranslator                          */
/************************************************************************/

/** Translate WKT geometry to WKB geometry and append it to a buffer */
class CPL_DLL OGRWKTToWKBTranslator
{
    OGRAppendBuffer &m_oAppendBuffer;
    bool m_bCanUseStrtod = false;

  public:
    /** Constructor */
    explicit OGRWKTToWKBTranslator(OGRAppendBuffer &oAppendBuffer);

    /** Translate the WKT geometry starting at pabyWKTStart and of length nLength.
     *
     * If pabyWKTStart[nLength] can be dereferenced and temporarily modified,
     * set bCanAlterByteAfter to true, which will optimize performance.
     *
     * Returns the number of bytes of the generated WKB, or -1 in case of error.
     */
    size_t TranslateWKT(void *pabyWKTStart, size_t nLength,
                        bool bCanAlterByteAfter);
};

#endif  // OGR_WKB_H_INCLUDED
