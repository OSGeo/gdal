/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  WKB geometry related methods
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_error.h"
#include "ogr_wkb.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

#include <algorithm>
#include <cmath>
#include <climits>
#include <limits>

#include <algorithm>
#include <limits>

#define USE_FAST_FLOAT
#ifdef USE_FAST_FLOAT
#include "include_fast_float.h"
#endif

/************************************************************************/
/*                          OGRWKBNeedSwap()                            */
/************************************************************************/

static inline bool OGRWKBNeedSwap(GByte b)
{
#if CPL_IS_LSB
    const bool bNeedSwap = b == 0;
#else
    const bool bNeedSwap = b == 1;
#endif
    return bNeedSwap;
}

/************************************************************************/
/*                        OGRWKBReadUInt32()                            */
/************************************************************************/

static inline uint32_t OGRWKBReadUInt32(const GByte *pabyWkb, bool bNeedSwap)
{
    uint32_t nVal;
    memcpy(&nVal, pabyWkb, sizeof(nVal));
    if (bNeedSwap)
        CPL_SWAP32PTR(&nVal);
    return nVal;
}

/************************************************************************/
/*                        OGRWKBReadFloat64()                           */
/************************************************************************/

static inline double OGRWKBReadFloat64(const GByte *pabyWkb, bool bNeedSwap)
{
    double dfVal;
    memcpy(&dfVal, pabyWkb, sizeof(dfVal));
    if (bNeedSwap)
        CPL_SWAP64PTR(&dfVal);
    return dfVal;
}

/************************************************************************/
/*                        OGRWKBRingGetArea()                           */
/************************************************************************/

static bool OGRWKBRingGetArea(const GByte *&pabyWkb, size_t &nWKBSize, int nDim,
                              bool bNeedSwap, double &dfArea)
{
    const uint32_t nPoints = OGRWKBReadUInt32(pabyWkb, bNeedSwap);
    if (nPoints >= 4 &&
        (nWKBSize - sizeof(uint32_t)) / (nDim * sizeof(double)) >= nPoints)
    {
        nWKBSize -= sizeof(uint32_t) + nDim * sizeof(double);
        pabyWkb += sizeof(uint32_t);
        // Computation according to Green's Theorem
        // Cf OGRSimpleCurve::get_LinearArea()
        double x_m1 = OGRWKBReadFloat64(pabyWkb, bNeedSwap);
        double y_m1 = OGRWKBReadFloat64(pabyWkb + sizeof(double), bNeedSwap);
        double y_m2 = y_m1;
        dfArea = 0;
        pabyWkb += nDim * sizeof(double);
        for (uint32_t i = 1; i < nPoints; ++i)
        {
            const double x = OGRWKBReadFloat64(pabyWkb, bNeedSwap);
            const double y =
                OGRWKBReadFloat64(pabyWkb + sizeof(double), bNeedSwap);
            pabyWkb += nDim * sizeof(double);
            dfArea += x_m1 * (y - y_m2);
            y_m2 = y_m1;
            x_m1 = x;
            y_m1 = y;
        }
        dfArea += x_m1 * (y_m1 - y_m2);
        dfArea = 0.5 * std::fabs(dfArea);
        return true;
    }
    return false;
}

/************************************************************************/
/*                         OGRWKBGetGeomType()                          */
/************************************************************************/

bool OGRWKBGetGeomType(const GByte *pabyWkb, size_t nWKBSize, bool &bNeedSwap,
                       uint32_t &nType)
{
    if (nWKBSize >= 5)
    {
        bNeedSwap = OGRWKBNeedSwap(pabyWkb[0]);
        nType = OGRWKBReadUInt32(pabyWkb + 1, bNeedSwap);
        return true;
    }
    return false;
}

/************************************************************************/
/*                        OGRWKBPolygonGetArea()                        */
/************************************************************************/

bool OGRWKBPolygonGetArea(const GByte *&pabyWkb, size_t &nWKBSize,
                          double &dfArea)
{
    bool bNeedSwap;
    uint32_t nType;
    if (nWKBSize < 9 || !OGRWKBGetGeomType(pabyWkb, nWKBSize, bNeedSwap, nType))
        return false;

    int nDims = 2;
    if (nType == wkbPolygon)
    {
        // do nothing
    }
    else if (nType == wkbPolygon + 1000 ||  // wkbPolygonZ
             nType == wkbPolygon25D || nType == wkbPolygonM)
    {
        nDims = 3;
    }
    else if (nType == wkbPolygonZM)
    {
        nDims = 4;
    }
    else
    {
        return false;
    }

    const uint32_t nRings = OGRWKBReadUInt32(pabyWkb + 5, bNeedSwap);
    if ((nWKBSize - 9) / sizeof(uint32_t) >= nRings)
    {
        pabyWkb += 9;
        nWKBSize -= 9;
        dfArea = 0;
        if (nRings > 0)
        {
            if (!OGRWKBRingGetArea(pabyWkb, nWKBSize, nDims, bNeedSwap, dfArea))
                return false;
            for (uint32_t i = 1; i < nRings; ++i)
            {
                double dfRingArea;
                if (!OGRWKBRingGetArea(pabyWkb, nWKBSize, nDims, bNeedSwap,
                                       dfRingArea))
                    return false;
                dfArea -= dfRingArea;
            }
        }
        return true;
    }
    return false;
}

/************************************************************************/
/*                    OGRWKBMultiPolygonGetArea()                       */
/************************************************************************/

bool OGRWKBMultiPolygonGetArea(const GByte *&pabyWkb, size_t &nWKBSize,
                               double &dfArea)
{
    if (nWKBSize < 9)
        return false;

    const bool bNeedSwap = OGRWKBNeedSwap(pabyWkb[0]);
    const uint32_t nPolys = OGRWKBReadUInt32(pabyWkb + 5, bNeedSwap);
    if ((nWKBSize - 9) / 9 >= nPolys)
    {
        pabyWkb += 9;
        nWKBSize -= 9;
        dfArea = 0;
        for (uint32_t i = 0; i < nPolys; ++i)
        {
            double dfPolyArea;
            if (!OGRWKBPolygonGetArea(pabyWkb, nWKBSize, dfPolyArea))
                return false;
            dfArea += dfPolyArea;
        }
        return true;
    }
    return false;
}

/************************************************************************/
/*                            WKBFromEWKB()                             */
/************************************************************************/

const GByte *WKBFromEWKB(GByte *pabyEWKB, size_t nEWKBSize, size_t &nWKBSizeOut,
                         int *pnSRIDOut)
{
    if (nEWKBSize < 5U)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid EWKB content : %u bytes",
                 static_cast<unsigned>(nEWKBSize));
        return nullptr;
    }

    const GByte *pabyWKB = pabyEWKB;

    /* -------------------------------------------------------------------- */
    /*      PostGIS EWKB format includes an SRID, but this won't be         */
    /*      understood by OGR, so if the SRID flag is set, we remove the    */
    /*      SRID (bytes at offset 5 to 8).                                  */
    /* -------------------------------------------------------------------- */
    if (nEWKBSize > 9 &&
        ((pabyEWKB[0] == 0 /* big endian */ && (pabyEWKB[1] & 0x20)) ||
         (pabyEWKB[0] != 0 /* little endian */ && (pabyEWKB[4] & 0x20))))
    {
        if (pnSRIDOut)
        {
            memcpy(pnSRIDOut, pabyEWKB + 5, 4);
            const OGRwkbByteOrder eByteOrder =
                (pabyEWKB[0] == 0 ? wkbXDR : wkbNDR);
            if (OGR_SWAP(eByteOrder))
                *pnSRIDOut = CPL_SWAP32(*pnSRIDOut);
        }

        // Drop the SRID flag
        if (pabyEWKB[0] == 0)
            pabyEWKB[1] &= (~0x20);
        else
            pabyEWKB[4] &= (~0x20);

        // Move 5 first bytes of EWKB 4 bytes later to create regular WKB
        memmove(pabyEWKB + 4, pabyEWKB, 5);
        memset(pabyEWKB, 0, 4);
        // and make pabyWKB point to that
        pabyWKB += 4;
        nWKBSizeOut = nEWKBSize - 4;
    }
    else
    {
        if (pnSRIDOut)
        {
            *pnSRIDOut = INT_MIN;
        }
        nWKBSizeOut = nEWKBSize;
    }

    return pabyWKB;
}

/************************************************************************/
/*                     OGRWKBReadUInt32AtOffset()                       */
/************************************************************************/

static uint32_t OGRWKBReadUInt32AtOffset(const uint8_t *data,
                                         OGRwkbByteOrder eByteOrder,
                                         size_t &iOffset)
{
    uint32_t v;
    memcpy(&v, data + iOffset, sizeof(v));
    iOffset += sizeof(v);
    if (OGR_SWAP(eByteOrder))
    {
        CPL_SWAP32PTR(&v);
    }
    return v;
}

/************************************************************************/
/*                         ReadWKBPointSequence()                       */
/************************************************************************/

template <bool INCLUDE_Z, typename EnvelopeType>
static bool ReadWKBPointSequence(const uint8_t *data, size_t size,
                                 OGRwkbByteOrder eByteOrder, int nDim,
                                 bool bHasZ, size_t &iOffset,
                                 EnvelopeType &sEnvelope)
{
    const uint32_t nPoints =
        OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
    if (nPoints > (size - iOffset) / (nDim * sizeof(double)))
        return false;
    double dfX = 0;
    double dfY = 0;
    [[maybe_unused]] double dfZ = 0;
    for (uint32_t j = 0; j < nPoints; j++)
    {
        memcpy(&dfX, data + iOffset, sizeof(double));
        memcpy(&dfY, data + iOffset + sizeof(double), sizeof(double));
        if constexpr (INCLUDE_Z)
        {
            if (bHasZ)
                memcpy(&dfZ, data + iOffset + 2 * sizeof(double),
                       sizeof(double));
        }
        iOffset += nDim * sizeof(double);
        if (OGR_SWAP(eByteOrder))
        {
            CPL_SWAP64PTR(&dfX);
            CPL_SWAP64PTR(&dfY);
            if constexpr (INCLUDE_Z)
            {
                CPL_SWAP64PTR(&dfZ);
            }
        }
        sEnvelope.MinX = std::min(sEnvelope.MinX, dfX);
        sEnvelope.MinY = std::min(sEnvelope.MinY, dfY);
        sEnvelope.MaxX = std::max(sEnvelope.MaxX, dfX);
        sEnvelope.MaxY = std::max(sEnvelope.MaxY, dfY);
        if constexpr (INCLUDE_Z)
        {
            if (bHasZ)
            {
                sEnvelope.MinZ = std::min(sEnvelope.MinZ, dfZ);
                sEnvelope.MaxZ = std::max(sEnvelope.MaxZ, dfZ);
            }
        }
    }
    return true;
}

/************************************************************************/
/*                         ReadWKBRingSequence()                        */
/************************************************************************/

template <bool INCLUDE_Z, typename EnvelopeType>
static bool ReadWKBRingSequence(const uint8_t *data, size_t size,
                                OGRwkbByteOrder eByteOrder, int nDim,
                                bool bHasZ, size_t &iOffset,
                                EnvelopeType &sEnvelope)
{
    const uint32_t nRings = OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
    if (nRings > (size - iOffset) / sizeof(uint32_t))
        return false;
    for (uint32_t i = 0; i < nRings; i++)
    {
        if (iOffset + sizeof(uint32_t) > size)
            return false;
        if (!ReadWKBPointSequence<INCLUDE_Z>(data, size, eByteOrder, nDim,
                                             bHasZ, iOffset, sEnvelope))
            return false;
    }
    return true;
}

/************************************************************************/
/*                        OGRWKBGetBoundingBox()                        */
/************************************************************************/

constexpr uint32_t WKB_PREFIX_SIZE = 1 + sizeof(uint32_t);
constexpr uint32_t MIN_WKB_SIZE = WKB_PREFIX_SIZE + sizeof(uint32_t);

template <bool INCLUDE_Z, typename EnvelopeType>
static bool OGRWKBGetBoundingBox(const uint8_t *data, size_t size,
                                 size_t &iOffset, EnvelopeType &sEnvelope,
                                 int nRec)
{
    if (size - iOffset < MIN_WKB_SIZE)
        return false;
    const int nByteOrder = DB2_V72_FIX_BYTE_ORDER(data[iOffset]);
    if (!(nByteOrder == wkbXDR || nByteOrder == wkbNDR))
        return false;
    const OGRwkbByteOrder eByteOrder = static_cast<OGRwkbByteOrder>(nByteOrder);

    OGRwkbGeometryType eGeometryType = wkbUnknown;
    OGRReadWKBGeometryType(data + iOffset, wkbVariantIso, &eGeometryType);
    iOffset += 5;
    const auto eFlatType = wkbFlatten(eGeometryType);
    const bool bHasZ = CPL_TO_BOOL(OGR_GT_HasZ(eGeometryType));
    const int nDim = 2 + (bHasZ ? 1 : 0) + (OGR_GT_HasM(eGeometryType) ? 1 : 0);

    if (eFlatType == wkbPoint)
    {
        if (size - iOffset < nDim * sizeof(double))
            return false;
        double dfX = 0;
        double dfY = 0;
        [[maybe_unused]] double dfZ = 0;
        memcpy(&dfX, data + iOffset, sizeof(double));
        memcpy(&dfY, data + iOffset + sizeof(double), sizeof(double));
        if constexpr (INCLUDE_Z)
        {
            if (bHasZ)
                memcpy(&dfZ, data + iOffset + 2 * sizeof(double),
                       sizeof(double));
        }
        iOffset += nDim * sizeof(double);
        if (OGR_SWAP(eByteOrder))
        {
            CPL_SWAP64PTR(&dfX);
            CPL_SWAP64PTR(&dfY);
            if constexpr (INCLUDE_Z)
            {
                CPL_SWAP64PTR(&dfZ);
            }
        }
        if (std::isnan(dfX))
        {
            // Point empty
            sEnvelope = EnvelopeType();
        }
        else
        {
            sEnvelope.MinX = dfX;
            sEnvelope.MinY = dfY;
            sEnvelope.MaxX = dfX;
            sEnvelope.MaxY = dfY;
            if constexpr (INCLUDE_Z)
            {
                if (bHasZ)
                {
                    sEnvelope.MinZ = dfZ;
                    sEnvelope.MaxZ = dfZ;
                }
            }
        }
        return true;
    }

    if (eFlatType == wkbLineString || eFlatType == wkbCircularString)
    {
        sEnvelope = EnvelopeType();

        return ReadWKBPointSequence<INCLUDE_Z>(data, size, eByteOrder, nDim,
                                               bHasZ, iOffset, sEnvelope);
    }

    if (eFlatType == wkbPolygon || eFlatType == wkbTriangle)
    {
        sEnvelope = EnvelopeType();

        return ReadWKBRingSequence<INCLUDE_Z>(data, size, eByteOrder, nDim,
                                              bHasZ, iOffset, sEnvelope);
    }

    if (eFlatType == wkbMultiPoint)
    {
        sEnvelope = EnvelopeType();

        uint32_t nParts = OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
        if (nParts >
            (size - iOffset) / (WKB_PREFIX_SIZE + nDim * sizeof(double)))
            return false;
        double dfX = 0;
        double dfY = 0;
        [[maybe_unused]] double dfZ = 0;
        for (uint32_t k = 0; k < nParts; k++)
        {
            iOffset += WKB_PREFIX_SIZE;
            memcpy(&dfX, data + iOffset, sizeof(double));
            memcpy(&dfY, data + iOffset + sizeof(double), sizeof(double));
            if constexpr (INCLUDE_Z)
            {
                if (bHasZ)
                    memcpy(&dfZ, data + iOffset + 2 * sizeof(double),
                           sizeof(double));
            }
            iOffset += nDim * sizeof(double);
            if (OGR_SWAP(eByteOrder))
            {
                CPL_SWAP64PTR(&dfX);
                CPL_SWAP64PTR(&dfY);
                if constexpr (INCLUDE_Z)
                {
                    CPL_SWAP64PTR(&dfZ);
                }
            }
            sEnvelope.MinX = std::min(sEnvelope.MinX, dfX);
            sEnvelope.MinY = std::min(sEnvelope.MinY, dfY);
            sEnvelope.MaxX = std::max(sEnvelope.MaxX, dfX);
            sEnvelope.MaxY = std::max(sEnvelope.MaxY, dfY);
            if constexpr (INCLUDE_Z)
            {
                if (bHasZ)
                {
                    sEnvelope.MinZ = std::min(sEnvelope.MinZ, dfZ);
                    sEnvelope.MaxZ = std::max(sEnvelope.MaxZ, dfZ);
                }
            }
        }
        return true;
    }

    if (eFlatType == wkbMultiLineString)
    {
        sEnvelope = EnvelopeType();

        const uint32_t nParts =
            OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
        if (nParts > (size - iOffset) / MIN_WKB_SIZE)
            return false;
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (iOffset + MIN_WKB_SIZE > size)
                return false;
            iOffset += WKB_PREFIX_SIZE;
            if (!ReadWKBPointSequence<INCLUDE_Z>(data, size, eByteOrder, nDim,
                                                 bHasZ, iOffset, sEnvelope))
                return false;
        }
        return true;
    }

    if (eFlatType == wkbMultiPolygon)
    {
        sEnvelope = EnvelopeType();

        const uint32_t nParts =
            OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
        if (nParts > (size - iOffset) / MIN_WKB_SIZE)
            return false;
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (iOffset + MIN_WKB_SIZE > size)
                return false;
            CPLAssert(data[iOffset] == eByteOrder);
            iOffset += WKB_PREFIX_SIZE;
            if (!ReadWKBRingSequence<INCLUDE_Z>(data, size, eByteOrder, nDim,
                                                bHasZ, iOffset, sEnvelope))
                return false;
        }
        return true;
    }

    if (eFlatType == wkbGeometryCollection || eFlatType == wkbCompoundCurve ||
        eFlatType == wkbCurvePolygon || eFlatType == wkbMultiCurve ||
        eFlatType == wkbMultiSurface || eFlatType == wkbPolyhedralSurface ||
        eFlatType == wkbTIN)
    {
        if (nRec == 128)
            return false;
        sEnvelope = EnvelopeType();

        const uint32_t nParts =
            OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
        if (nParts > (size - iOffset) / MIN_WKB_SIZE)
            return false;
        EnvelopeType sEnvelopeSubGeom;
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (!OGRWKBGetBoundingBox<INCLUDE_Z>(data, size, iOffset,
                                                 sEnvelopeSubGeom, nRec + 1))
                return false;
            sEnvelope.Merge(sEnvelopeSubGeom);
        }
        return true;
    }

    return false;
}

/************************************************************************/
/*                        OGRWKBGetBoundingBox()                        */
/************************************************************************/

bool OGRWKBGetBoundingBox(const GByte *pabyWkb, size_t nWKBSize,
                          OGREnvelope &sEnvelope)
{
    size_t iOffset = 0;
    return OGRWKBGetBoundingBox<false>(pabyWkb, nWKBSize, iOffset, sEnvelope,
                                       0);
}

/************************************************************************/
/*                        OGRWKBGetBoundingBox()                        */
/************************************************************************/

bool OGRWKBGetBoundingBox(const GByte *pabyWkb, size_t nWKBSize,
                          OGREnvelope3D &sEnvelope)
{
    size_t iOffset = 0;
    return OGRWKBGetBoundingBox<true>(pabyWkb, nWKBSize, iOffset, sEnvelope, 0);
}

/************************************************************************/
/*              OGRWKBIntersectsPointSequencePessimistic()              */
/************************************************************************/

static bool OGRWKBIntersectsPointSequencePessimistic(
    const uint8_t *data, const size_t size, const OGRwkbByteOrder eByteOrder,
    const int nDim, size_t &iOffsetInOut, const OGREnvelope &sEnvelope,
    bool &bErrorOut)
{
    const uint32_t nPoints =
        OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffsetInOut);
    if (nPoints > (size - iOffsetInOut) / (nDim * sizeof(double)))
    {
        bErrorOut = true;
        return false;
    }

    double dfX = 0;
    double dfY = 0;
    for (uint32_t j = 0; j < nPoints; j++)
    {
        memcpy(&dfX, data + iOffsetInOut, sizeof(double));
        memcpy(&dfY, data + iOffsetInOut + sizeof(double), sizeof(double));
        iOffsetInOut += nDim * sizeof(double);
        if (OGR_SWAP(eByteOrder))
        {
            CPL_SWAP64PTR(&dfX);
            CPL_SWAP64PTR(&dfY);
        }
        if (dfX >= sEnvelope.MinX && dfY >= sEnvelope.MinY &&
            dfX <= sEnvelope.MaxX && dfY <= sEnvelope.MaxY)
        {
            return true;
        }
    }

    return false;
}

/************************************************************************/
/*               OGRWKBIntersectsRingSequencePessimistic()              */
/************************************************************************/

static bool OGRWKBIntersectsRingSequencePessimistic(
    const uint8_t *data, const size_t size, const OGRwkbByteOrder eByteOrder,
    const int nDim, size_t &iOffsetInOut, const OGREnvelope &sEnvelope,
    bool &bErrorOut)
{
    const uint32_t nRings =
        OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffsetInOut);
    if (nRings > (size - iOffsetInOut) / sizeof(uint32_t))
    {
        bErrorOut = true;
        return false;
    }
    if (nRings == 0)
        return false;
    if (iOffsetInOut + sizeof(uint32_t) > size)
    {
        bErrorOut = true;
        return false;
    }
    if (OGRWKBIntersectsPointSequencePessimistic(
            data, size, eByteOrder, nDim, iOffsetInOut, sEnvelope, bErrorOut))
    {
        return true;
    }
    if (bErrorOut)
        return false;

    // skip inner rings
    for (uint32_t i = 1; i < nRings; ++i)
    {
        if (iOffsetInOut + sizeof(uint32_t) > size)
        {
            bErrorOut = true;
            return false;
        }
        const uint32_t nPoints =
            OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffsetInOut);
        if (nPoints > (size - iOffsetInOut) / (nDim * sizeof(double)))
        {
            bErrorOut = true;
            return false;
        }
        iOffsetInOut += sizeof(double) * nPoints * nDim;
    }
    return false;
}

/************************************************************************/
/*                  OGRWKBIntersectsPessimistic()                         */
/************************************************************************/

static bool OGRWKBIntersectsPessimistic(const GByte *data, const size_t size,
                                        size_t &iOffsetInOut,
                                        const OGREnvelope &sEnvelope,
                                        const int nRec, bool &bErrorOut)
{
    if (size - iOffsetInOut < MIN_WKB_SIZE)
    {
        bErrorOut = true;
        return false;
    }
    const int nByteOrder = DB2_V72_FIX_BYTE_ORDER(data[iOffsetInOut]);
    if (!(nByteOrder == wkbXDR || nByteOrder == wkbNDR))
    {
        bErrorOut = true;
        return false;
    }
    const OGRwkbByteOrder eByteOrder = static_cast<OGRwkbByteOrder>(nByteOrder);

    OGRwkbGeometryType eGeometryType = wkbUnknown;
    OGRReadWKBGeometryType(data + iOffsetInOut, wkbVariantIso, &eGeometryType);
    iOffsetInOut += 5;
    const auto eFlatType = wkbFlatten(eGeometryType);
    const int nDim = 2 + (OGR_GT_HasZ(eGeometryType) ? 1 : 0) +
                     (OGR_GT_HasM(eGeometryType) ? 1 : 0);

    if (eFlatType == wkbPoint)
    {
        if (size - iOffsetInOut < nDim * sizeof(double))
            return false;
        double dfX = 0;
        double dfY = 0;
        memcpy(&dfX, data + iOffsetInOut, sizeof(double));
        memcpy(&dfY, data + iOffsetInOut + sizeof(double), sizeof(double));
        iOffsetInOut += nDim * sizeof(double);
        if (OGR_SWAP(eByteOrder))
        {
            CPL_SWAP64PTR(&dfX);
            CPL_SWAP64PTR(&dfY);
        }
        if (std::isnan(dfX))
        {
            return false;
        }
        else
        {
            return dfX >= sEnvelope.MinX && dfX <= sEnvelope.MaxX &&
                   dfY >= sEnvelope.MinY && dfY <= sEnvelope.MaxY;
        }
    }

    if (eFlatType == wkbLineString || eFlatType == wkbCircularString)
    {
        return OGRWKBIntersectsPointSequencePessimistic(
            data, size, eByteOrder, nDim, iOffsetInOut, sEnvelope, bErrorOut);
    }

    if (eFlatType == wkbPolygon || eFlatType == wkbTriangle)
    {
        return OGRWKBIntersectsRingSequencePessimistic(
            data, size, eByteOrder, nDim, iOffsetInOut, sEnvelope, bErrorOut);
    }

    if (eFlatType == wkbMultiPoint || eFlatType == wkbMultiLineString ||
        eFlatType == wkbMultiPolygon || eFlatType == wkbGeometryCollection ||
        eFlatType == wkbCompoundCurve || eFlatType == wkbCurvePolygon ||
        eFlatType == wkbMultiCurve || eFlatType == wkbMultiSurface ||
        eFlatType == wkbPolyhedralSurface || eFlatType == wkbTIN)
    {
        if (nRec == 128)
        {
            bErrorOut = true;
            return false;
        }
        const uint32_t nParts =
            OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffsetInOut);
        if (nParts > (size - iOffsetInOut) / MIN_WKB_SIZE)
        {
            bErrorOut = true;
            return false;
        }
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (OGRWKBIntersectsPessimistic(data, size, iOffsetInOut, sEnvelope,
                                            nRec + 1, bErrorOut))
            {
                return true;
            }
            else if (bErrorOut)
            {
                return false;
            }
        }
        return false;
    }

    bErrorOut = true;
    return false;
}

/************************************************************************/
/*                  OGRWKBIntersectsPessimistic()                       */
/************************************************************************/

/* Returns whether the geometry (pabyWkb, nWKBSize) intersects, for sure,
 * the passed envelope.
 * When it returns true, the geometry intersects the envelope.
 * When it returns false, the geometry may or may not intersect the envelope.
 */
bool OGRWKBIntersectsPessimistic(const GByte *pabyWkb, size_t nWKBSize,
                                 const OGREnvelope &sEnvelope)
{
    size_t iOffsetInOut = 0;
    bool bErrorOut = false;
    bool bRet = OGRWKBIntersectsPessimistic(pabyWkb, nWKBSize, iOffsetInOut,
                                            sEnvelope, 0, bErrorOut);
    if (!bRet && !bErrorOut)
    {
        // The following assert only holds if there is no trailing data
        // after the WKB
        // CPLAssert(iOffsetInOut == nWKBSize);
    }
    return bRet;
}

/************************************************************************/
/*                            epsilonEqual()                            */
/************************************************************************/

constexpr double EPSILON = 1.0E-5;

static inline bool epsilonEqual(double a, double b, double eps)
{
    return ::fabs(a - b) < eps;
}

/************************************************************************/
/*                     OGRWKBIsClockwiseRing()                          */
/************************************************************************/

static inline double GetX(const GByte *data, uint32_t i, int nDim,
                          bool bNeedSwap)
{
    double dfX;
    memcpy(&dfX, data + static_cast<size_t>(i) * nDim * sizeof(double),
           sizeof(double));
    if (bNeedSwap)
        CPL_SWAP64PTR(&dfX);
    return dfX;
}

static inline double GetY(const GByte *data, uint32_t i, int nDim,
                          bool bNeedSwap)
{
    double dfY;
    memcpy(&dfY, data + (static_cast<size_t>(i) * nDim + 1) * sizeof(double),
           sizeof(double));
    if (bNeedSwap)
        CPL_SWAP64PTR(&dfY);
    return dfY;
}

static bool OGRWKBIsClockwiseRing(const GByte *data, const uint32_t nPoints,
                                  const int nDim, const bool bNeedSwap)
{
    // WARNING: keep in sync OGRLineString::isClockwise(),
    // OGRCurve::isClockwise() and OGRWKBIsClockwiseRing()

    bool bUseFallback = false;

    // Find the lowest rightmost vertex.
    uint32_t v = 0;  // Used after for.
    double vX = GetX(data, v, nDim, bNeedSwap);
    double vY = GetY(data, v, nDim, bNeedSwap);
    for (uint32_t i = 1; i < nPoints - 1; i++)
    {
        // => v < end.
        const double y = GetY(data, i, nDim, bNeedSwap);
        if (y < vY)
        {
            v = i;
            vX = GetX(data, i, nDim, bNeedSwap);
            vY = y;
            bUseFallback = false;
        }
        else if (y == vY)
        {
            const double x = GetX(data, i, nDim, bNeedSwap);
            if (x > vX)
            {
                v = i;
                vX = x;
                vY = y;
                bUseFallback = false;
            }
            else if (x == vX)
            {
                // Two vertex with same coordinates are the lowest rightmost
                // vertex.  Cannot use that point as the pivot (#5342).
                bUseFallback = true;
            }
        }
    }

    // Previous.
    uint32_t next = (v == 0) ? nPoints - 2 : v - 1;
    if (epsilonEqual(GetX(data, next, nDim, bNeedSwap), vX, EPSILON) &&
        epsilonEqual(GetY(data, next, nDim, bNeedSwap), vY, EPSILON))
    {
        // Don't try to be too clever by retrying with a next point.
        // This can lead to false results as in the case of #3356.
        bUseFallback = true;
    }

    const double dx0 = GetX(data, next, nDim, bNeedSwap) - vX;
    const double dy0 = GetY(data, next, nDim, bNeedSwap) - vY;

    // Following.
    next = v + 1;
    if (next >= nPoints - 1)
    {
        next = 0;
    }

    if (epsilonEqual(GetX(data, next, nDim, bNeedSwap), vX, EPSILON) &&
        epsilonEqual(GetY(data, next, nDim, bNeedSwap), vY, EPSILON))
    {
        // Don't try to be too clever by retrying with a next point.
        // This can lead to false results as in the case of #3356.
        bUseFallback = true;
    }

    const double dx1 = GetX(data, next, nDim, bNeedSwap) - vX;
    const double dy1 = GetY(data, next, nDim, bNeedSwap) - vY;

    const double crossproduct = dx1 * dy0 - dx0 * dy1;

    if (!bUseFallback)
    {
        if (crossproduct > 0)  // CCW
            return false;
        else if (crossproduct < 0)  // CW
            return true;
    }

    // This is a degenerate case: the extent of the polygon is less than EPSILON
    // or 2 nearly identical points were found.
    // Try with Green Formula as a fallback, but this is not a guarantee
    // as we'll probably be affected by numerical instabilities.

    double dfSum = GetX(data, 0, nDim, bNeedSwap) *
                   (GetY(data, 1, nDim, bNeedSwap) -
                    GetY(data, nPoints - 1, nDim, bNeedSwap));

    for (uint32_t i = 1; i < nPoints - 1; i++)
    {
        dfSum += GetX(data, i, nDim, bNeedSwap) *
                 (GetY(data, i + 1, nDim, bNeedSwap) -
                  GetY(data, i - 1, nDim, bNeedSwap));
    }

    dfSum += GetX(data, nPoints - 1, nDim, bNeedSwap) *
             (GetY(data, 0, nDim, bNeedSwap) -
              GetX(data, nPoints - 2, nDim, bNeedSwap));

    return dfSum < 0;
}

/************************************************************************/
/*                OGRWKBFixupCounterClockWiseExternalRing()             */
/************************************************************************/

static bool OGRWKBFixupCounterClockWiseExternalRingInternal(
    GByte *data, size_t size, size_t &iOffsetInOut, const int nRec)
{
    if (size - iOffsetInOut < MIN_WKB_SIZE)
    {
        return false;
    }
    const int nByteOrder = DB2_V72_FIX_BYTE_ORDER(data[iOffsetInOut]);
    if (!(nByteOrder == wkbXDR || nByteOrder == wkbNDR))
    {
        return false;
    }
    const OGRwkbByteOrder eByteOrder = static_cast<OGRwkbByteOrder>(nByteOrder);

    OGRwkbGeometryType eGeometryType = wkbUnknown;
    OGRReadWKBGeometryType(data + iOffsetInOut, wkbVariantIso, &eGeometryType);
    iOffsetInOut += 5;
    const auto eFlatType = wkbFlatten(eGeometryType);
    const int nDim = 2 + (OGR_GT_HasZ(eGeometryType) ? 1 : 0) +
                     (OGR_GT_HasM(eGeometryType) ? 1 : 0);

    if (eFlatType == wkbPolygon)
    {
        const uint32_t nRings =
            OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffsetInOut);
        if (nRings > (size - iOffsetInOut) / sizeof(uint32_t))
        {
            return false;
        }
        for (uint32_t iRing = 0; iRing < nRings; ++iRing)
        {
            if (iOffsetInOut + sizeof(uint32_t) > size)
                return false;
            const uint32_t nPoints =
                OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffsetInOut);
            const size_t sizeOfPoint = nDim * sizeof(double);
            if (nPoints > (size - iOffsetInOut) / sizeOfPoint)
            {
                return false;
            }

            if (nPoints >= 4)
            {
                const bool bIsClockwiseRing = OGRWKBIsClockwiseRing(
                    data + iOffsetInOut, nPoints, nDim, OGR_SWAP(eByteOrder));
                if ((bIsClockwiseRing && iRing == 0) ||
                    (!bIsClockwiseRing && iRing > 0))
                {
                    GByte abyTmp[4 * sizeof(double)];
                    for (uint32_t i = 0; i < nPoints / 2; ++i)
                    {
                        GByte *pBegin = data + iOffsetInOut + i * sizeOfPoint;
                        GByte *pEnd = data + iOffsetInOut +
                                      (nPoints - 1 - i) * sizeOfPoint;
                        memcpy(abyTmp, pBegin, sizeOfPoint);
                        memcpy(pBegin, pEnd, sizeOfPoint);
                        memcpy(pEnd, abyTmp, sizeOfPoint);
                    }
                }
            }

            iOffsetInOut += nPoints * sizeOfPoint;
        }
    }

    if (eFlatType == wkbGeometryCollection || eFlatType == wkbMultiPolygon ||
        eFlatType == wkbMultiSurface)
    {
        if (nRec == 128)
        {
            return false;
        }
        const uint32_t nParts =
            OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffsetInOut);
        if (nParts > (size - iOffsetInOut) / MIN_WKB_SIZE)
        {
            return false;
        }
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (!OGRWKBFixupCounterClockWiseExternalRingInternal(
                    data, size, iOffsetInOut, nRec))
            {
                return false;
            }
        }
    }

    return true;
}

/** Modifies the geometry such that exterior rings of polygons are
 * counter-clockwise oriented and inner rings clockwise oriented.
 */
void OGRWKBFixupCounterClockWiseExternalRing(GByte *pabyWkb, size_t nWKBSize)
{
    size_t iOffsetInOut = 0;
    OGRWKBFixupCounterClockWiseExternalRingInternal(
        pabyWkb, nWKBSize, iOffsetInOut, /* nRec = */ 0);
}

/************************************************************************/
/*                         OGRAppendBuffer()                            */
/************************************************************************/

OGRAppendBuffer::OGRAppendBuffer() = default;

/************************************************************************/
/*                        ~OGRAppendBuffer()                            */
/************************************************************************/

OGRAppendBuffer::~OGRAppendBuffer() = default;

/************************************************************************/
/*                       OGRWKTToWKBTranslator()                        */
/************************************************************************/

OGRWKTToWKBTranslator::OGRWKTToWKBTranslator(OGRAppendBuffer &oAppendBuffer)
    : m_oAppendBuffer(oAppendBuffer)
{
#ifndef USE_FAST_FLOAT
    // Test if current locale decimal separator is decimal point
    char szTest[10];
    snprintf(szTest, sizeof(szTest), "%f", 1.5);
    m_bCanUseStrtod = strchr(szTest, '.') != nullptr;
#endif
    CPL_IGNORE_RET_VAL(m_bCanUseStrtod);
}

/************************************************************************/
/*                          TranslateWKT()                              */
/************************************************************************/

size_t OGRWKTToWKBTranslator::TranslateWKT(void *pabyWKTStart, size_t nLength,
                                           bool bCanAlterByteAfter)
{
    const char *pszPtrStart = static_cast<const char *>(pabyWKTStart);
    // Optimize single-part single-ring multipolygon WKT->WKB translation
    if (bCanAlterByteAfter && nLength > strlen("MULTIPOLYGON") &&
        EQUALN(pszPtrStart, "MULTIPOLYGON", strlen("MULTIPOLYGON")))
    {
        int nCountOpenPar = 0;
        size_t nCountComma = 0;
        bool bHasZ = false;
        bool bHasM = false;

        char *pszEnd = static_cast<char *>(pabyWKTStart) + nLength;
        const char chBackup = *pszEnd;
        *pszEnd = 0;

        // Checks that the multipolygon consists of a single part with
        // only an exterior ring.
        for (const char *pszPtr = pszPtrStart + strlen("MULTIPOLYGON"); *pszPtr;
             ++pszPtr)
        {
            const char ch = *pszPtr;
            if (ch == 'Z')
                bHasZ = true;
            else if (ch == 'M')
                bHasM = true;
            if (ch == '(')
            {
                nCountOpenPar++;
                if (nCountOpenPar == 4)
                    break;
            }
            else if (ch == ')')
            {
                nCountOpenPar--;
                if (nCountOpenPar < 0)
                    break;
            }
            else if (ch == ',')
            {
                if (nCountOpenPar < 3)
                {
                    // multipart / multi-ring
                    break;
                }
                nCountComma++;
            }
        }
        const int nDim = 2 + (bHasZ ? 1 : 0) + (bHasM ? 1 : 0);
        if (nCountOpenPar == 0 && nCountComma > 0 &&
            nCountComma < std::numeric_limits<uint32_t>::max())
        {
            const uint32_t nVerticesCount =
                static_cast<uint32_t>(nCountComma + 1);
            const size_t nWKBSize =
                sizeof(GByte) +     // Endianness
                sizeof(uint32_t) +  // multipolygon WKB geometry type
                sizeof(uint32_t) +  // number of parts
                sizeof(GByte) +     // Endianness
                sizeof(uint32_t) +  // polygon WKB geometry type
                sizeof(uint32_t) +  // number of rings
                sizeof(uint32_t) +  // number of vertices
                nDim * sizeof(double) * nVerticesCount;
            GByte *const pabyCurStart = static_cast<GByte *>(
                m_oAppendBuffer.GetPtrForNewBytes(nWKBSize));
            if (!pabyCurStart)
            {
                return static_cast<size_t>(-1);
            }
            GByte *pabyCur = pabyCurStart;
            // Multipolygon byte order
            {
                *pabyCur = wkbNDR;
                pabyCur++;
            }
            // Multipolygon geometry type
            {
                uint32_t nWKBGeomType =
                    wkbMultiPolygon + (bHasZ ? 1000 : 0) + (bHasM ? 2000 : 0);
                CPL_LSBPTR32(&nWKBGeomType);
                memcpy(pabyCur, &nWKBGeomType, sizeof(uint32_t));
                pabyCur += sizeof(uint32_t);
            }
            // Number of parts
            {
                uint32_t nOne = 1;
                CPL_LSBPTR32(&nOne);
                memcpy(pabyCur, &nOne, sizeof(uint32_t));
                pabyCur += sizeof(uint32_t);
            }
            // Polygon byte order
            {
                *pabyCur = wkbNDR;
                pabyCur++;
            }
            // Polygon geometry type
            {
                uint32_t nWKBGeomType =
                    wkbPolygon + (bHasZ ? 1000 : 0) + (bHasM ? 2000 : 0);
                CPL_LSBPTR32(&nWKBGeomType);
                memcpy(pabyCur, &nWKBGeomType, sizeof(uint32_t));
                pabyCur += sizeof(uint32_t);
            }
            // Number of rings
            {
                uint32_t nOne = 1;
                CPL_LSBPTR32(&nOne);
                memcpy(pabyCur, &nOne, sizeof(uint32_t));
                pabyCur += sizeof(uint32_t);
            }
            // Number of vertices
            {
                uint32_t nVerticesCountToWrite = nVerticesCount;
                CPL_LSBPTR32(&nVerticesCountToWrite);
                memcpy(pabyCur, &nVerticesCountToWrite, sizeof(uint32_t));
                pabyCur += sizeof(uint32_t);
            }
            uint32_t nDoubleCount = 0;
            const uint32_t nExpectedDoubleCount = nVerticesCount * nDim;
            for (const char *pszPtr = pszPtrStart + strlen("MULTIPOLYGON");
                 *pszPtr;
                 /* nothing */)
            {
                const char ch = *pszPtr;
                if (ch == '-' || ch == '.' || (ch >= '0' && ch <= '9'))
                {
                    nDoubleCount++;
                    if (nDoubleCount > nExpectedDoubleCount)
                    {
                        break;
                    }
#ifdef USE_FAST_FLOAT
                    double dfVal;
                    auto answer = fast_float::from_chars(pszPtr, pszEnd, dfVal);
                    if (answer.ec != std::errc())
                    {
                        nDoubleCount = 0;
                        break;
                    }
                    pszPtr = answer.ptr;
#else
                    char *endptr = nullptr;
                    const double dfVal =
                        m_bCanUseStrtod ? strtod(pszPtr, &endptr)
                                        : CPLStrtodDelim(pszPtr, &endptr, '.');
                    pszPtr = endptr;
#endif
                    CPL_LSBPTR64(&dfVal);
                    memcpy(pabyCur, &dfVal, sizeof(double));
                    pabyCur += sizeof(double);
                }
                else
                {
                    ++pszPtr;
                }
            }
            if (nDoubleCount == nExpectedDoubleCount)
            {
                CPLAssert(static_cast<size_t>(pabyCur - pabyCurStart) ==
                          nWKBSize);
                // cppcheck-suppress selfAssignment
                *pszEnd = chBackup;
                return nWKBSize;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid WKT geometry: %s", pszPtrStart);
                // cppcheck-suppress selfAssignment
                *pszEnd = chBackup;
                return static_cast<size_t>(-1);
            }
        }
        // cppcheck-suppress selfAssignment
        *pszEnd = chBackup;
    }

    // General case going through a OGRGeometry
    OGRGeometry *poGeometry = nullptr;
    if (bCanAlterByteAfter)
    {
        // Slight optimization for all geometries but the final one, to
        // avoid creating a new string each time.
        // We set the ending byte to '\0' and restore it back after parsing
        // the WKT
        char *pszEnd = static_cast<char *>(pabyWKTStart) + nLength;
        const char chBackup = *pszEnd;
        *pszEnd = 0;
        OGRGeometryFactory::createFromWkt(pszPtrStart, nullptr, &poGeometry);
        // cppcheck-suppress selfAssignment
        *pszEnd = chBackup;
    }
    else
    {
        std::string osTmp;
        osTmp.assign(pszPtrStart, nLength);
        OGRGeometryFactory::createFromWkt(osTmp.c_str(), nullptr, &poGeometry);
    }
    if (!poGeometry)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid WKT geometry");
        return static_cast<size_t>(-1);
    }
    const size_t nWKBSize = poGeometry->WkbSize();
    GByte *pabyWKB =
        static_cast<GByte *>(m_oAppendBuffer.GetPtrForNewBytes(nWKBSize));
    if (!pabyWKB)
    {
        return static_cast<size_t>(-1);
    }
    poGeometry->exportToWkb(wkbNDR, pabyWKB, wkbVariantIso);
    delete poGeometry;
    return nWKBSize;
}
