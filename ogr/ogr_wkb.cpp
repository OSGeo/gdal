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

#include <cmath>
#include <climits>

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
