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
#include "ogr_p.h"

#include <algorithm>
#include <cmath>
#include <climits>
#include <limits>

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

static bool ReadWKBPointSequence(const uint8_t *data, size_t size,
                                 OGRwkbByteOrder eByteOrder, int nDim,
                                 size_t &iOffset, OGREnvelope &sEnvelope)
{
    const uint32_t nPoints =
        OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
    if (nPoints > (size - iOffset) / (nDim * sizeof(double)))
        return false;
    double dfX = 0;
    double dfY = 0;
    for (uint32_t j = 0; j < nPoints; j++)
    {
        memcpy(&dfX, data + iOffset, sizeof(double));
        memcpy(&dfY, data + iOffset + sizeof(double), sizeof(double));
        iOffset += nDim * sizeof(double);
        if (OGR_SWAP(eByteOrder))
        {
            CPL_SWAP64PTR(&dfX);
            CPL_SWAP64PTR(&dfY);
        }
        sEnvelope.MinX = std::min(sEnvelope.MinX, dfX);
        sEnvelope.MinY = std::min(sEnvelope.MinY, dfY);
        sEnvelope.MaxX = std::max(sEnvelope.MaxX, dfX);
        sEnvelope.MaxY = std::max(sEnvelope.MaxY, dfY);
    }
    return true;
}

/************************************************************************/
/*                         ReadWKBRingSequence()                        */
/************************************************************************/

static bool ReadWKBRingSequence(const uint8_t *data, size_t size,
                                OGRwkbByteOrder eByteOrder, int nDim,
                                size_t &iOffset, OGREnvelope &sEnvelope)
{
    const uint32_t nRings = OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
    if (nRings > (size - iOffset) / sizeof(uint32_t))
        return false;
    for (uint32_t i = 0; i < nRings; i++)
    {
        if (iOffset + sizeof(uint32_t) > size)
            return false;
        if (!ReadWKBPointSequence(data, size, eByteOrder, nDim, iOffset,
                                  sEnvelope))
            return false;
    }
    return true;
}

/************************************************************************/
/*                        OGRWKBGetBoundingBox()                        */
/************************************************************************/

constexpr uint32_t WKB_PREFIX_SIZE = 1 + sizeof(uint32_t);
constexpr uint32_t MIN_WKB_SIZE = WKB_PREFIX_SIZE + sizeof(uint32_t);

static bool OGRWKBGetBoundingBox(const uint8_t *data, size_t size,
                                 size_t &iOffset, OGREnvelope &sEnvelope,
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
    const int nDim = 2 + (OGR_GT_HasZ(eGeometryType) ? 1 : 0) +
                     (OGR_GT_HasM(eGeometryType) ? 1 : 0);

    if (eFlatType == wkbPoint)
    {
        if (size - iOffset < nDim * sizeof(double))
            return false;
        double dfX = 0;
        double dfY = 0;
        memcpy(&dfX, data + iOffset, sizeof(double));
        memcpy(&dfY, data + iOffset + sizeof(double), sizeof(double));
        iOffset += nDim * sizeof(double);
        if (OGR_SWAP(eByteOrder))
        {
            CPL_SWAP64PTR(&dfX);
            CPL_SWAP64PTR(&dfY);
        }
        sEnvelope.MinX = dfX;
        sEnvelope.MinY = dfY;
        sEnvelope.MaxX = dfX;
        sEnvelope.MaxY = dfY;
        return true;
    }

    if (eFlatType == wkbLineString || eFlatType == wkbCircularString)
    {
        sEnvelope = OGREnvelope();

        return ReadWKBPointSequence(data, size, eByteOrder, nDim, iOffset,
                                    sEnvelope);
    }

    if (eFlatType == wkbPolygon)
    {
        sEnvelope = OGREnvelope();

        return ReadWKBRingSequence(data, size, eByteOrder, nDim, iOffset,
                                   sEnvelope);
    }

    if (eFlatType == wkbMultiPoint)
    {
        sEnvelope = OGREnvelope();

        uint32_t nParts = OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
        if (nParts >
            (size - iOffset) / (WKB_PREFIX_SIZE + nDim * sizeof(double)))
            return false;
        double dfX = 0;
        double dfY = 0;
        for (uint32_t k = 0; k < nParts; k++)
        {
            iOffset += WKB_PREFIX_SIZE;
            memcpy(&dfX, data + iOffset, sizeof(double));
            memcpy(&dfY, data + iOffset + sizeof(double), sizeof(double));
            iOffset += nDim * sizeof(double);
            if (OGR_SWAP(eByteOrder))
            {
                CPL_SWAP64PTR(&dfX);
                CPL_SWAP64PTR(&dfY);
            }
            sEnvelope.MinX = std::min(sEnvelope.MinX, dfX);
            sEnvelope.MinY = std::min(sEnvelope.MinY, dfY);
            sEnvelope.MaxX = std::max(sEnvelope.MaxX, dfX);
            sEnvelope.MaxY = std::max(sEnvelope.MaxY, dfY);
        }
        return true;
    }

    if (eFlatType == wkbMultiLineString)
    {
        sEnvelope = OGREnvelope();

        const uint32_t nParts =
            OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
        if (nParts > (size - iOffset) / MIN_WKB_SIZE)
            return false;
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (iOffset + MIN_WKB_SIZE > size)
                return false;
            iOffset += WKB_PREFIX_SIZE;
            if (!ReadWKBPointSequence(data, size, eByteOrder, nDim, iOffset,
                                      sEnvelope))
                return false;
        }
        return true;
    }

    if (eFlatType == wkbMultiPolygon)
    {
        sEnvelope = OGREnvelope();

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
            if (!ReadWKBRingSequence(data, size, eByteOrder, nDim, iOffset,
                                     sEnvelope))
                return false;
        }
        return true;
    }

    if (eFlatType == wkbGeometryCollection || eFlatType == wkbCompoundCurve ||
        eFlatType == wkbCurvePolygon || eFlatType == wkbMultiCurve ||
        eFlatType == wkbMultiSurface)
    {
        if (nRec == 128)
            return false;
        sEnvelope = OGREnvelope();

        const uint32_t nParts =
            OGRWKBReadUInt32AtOffset(data, eByteOrder, iOffset);
        if (nParts > (size - iOffset) / MIN_WKB_SIZE)
            return false;
        OGREnvelope sEnvelopeSubGeom;
        for (uint32_t k = 0; k < nParts; k++)
        {
            if (!OGRWKBGetBoundingBox(data, size, iOffset, sEnvelopeSubGeom,
                                      nRec + 1))
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
    return OGRWKBGetBoundingBox(pabyWkb, nWKBSize, iOffset, sEnvelope, 0);
}
