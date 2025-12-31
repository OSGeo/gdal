/******************************************************************************
*
 * Project:  GDAL Algorithms
 * Purpose:  Hilbert encoding
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2020, Björn Harrtell <bjorn at wololo dot org>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_alg.h"

// Subtract 1 from the theoretical max, reserving that number for empty or
// null geometries.
constexpr uint32_t GDAL_HILBERT_MAX = (1 << 16) - 2;

static uint32_t GDALHilbertCode(uint32_t x, uint32_t y)
{
    // Based on public domain code at
    // https://github.com/rawrunprotected/hilbert_curves

    uint32_t a = x ^ y;
    uint32_t b = 0xFFFF ^ a;
    uint32_t c = 0xFFFF ^ (x | y);
    uint32_t d = x & (y ^ 0xFFFF);

    uint32_t A = a | (b >> 1);
    uint32_t B = (a >> 1) ^ a;
    uint32_t C = ((c >> 1) ^ (b & (d >> 1))) ^ c;
    uint32_t D = ((a & (c >> 1)) ^ (d >> 1)) ^ d;

    a = A;
    b = B;
    c = C;
    d = D;
    A = ((a & (a >> 2)) ^ (b & (b >> 2)));
    B = ((a & (b >> 2)) ^ (b & ((a ^ b) >> 2)));
    C ^= ((a & (c >> 2)) ^ (b & (d >> 2)));
    D ^= ((b & (c >> 2)) ^ ((a ^ b) & (d >> 2)));

    a = A;
    b = B;
    c = C;
    d = D;
    A = ((a & (a >> 4)) ^ (b & (b >> 4)));
    B = ((a & (b >> 4)) ^ (b & ((a ^ b) >> 4)));
    C ^= ((a & (c >> 4)) ^ (b & (d >> 4)));
    D ^= ((b & (c >> 4)) ^ ((a ^ b) & (d >> 4)));

    a = A;
    b = B;
    c = C;
    d = D;
    C ^= ((a & (c >> 8)) ^ (b & (d >> 8)));
    D ^= ((b & (c >> 8)) ^ ((a ^ b) & (d >> 8)));

    a = C ^ (C >> 1);
    b = D ^ (D >> 1);

    uint32_t i0 = x ^ y;
    uint32_t i1 = b | (0xFFFF ^ (i0 | a));

    i0 = (i0 | (i0 << 8)) & 0x00FF00FF;
    i0 = (i0 | (i0 << 4)) & 0x0F0F0F0F;
    i0 = (i0 | (i0 << 2)) & 0x33333333;
    i0 = (i0 | (i0 << 1)) & 0x55555555;

    i1 = (i1 | (i1 << 8)) & 0x00FF00FF;
    i1 = (i1 | (i1 << 4)) & 0x0F0F0F0F;
    i1 = (i1 | (i1 << 2)) & 0x33333333;
    i1 = (i1 | (i1 << 1)) & 0x55555555;

    uint32_t value = ((i1 << 1) | i0);

    return value;
}

uint32_t GDALHilbertCode(const OGREnvelope *poDomain, double dfX, double dfY)
{
    uint32_t x = 0;
    uint32_t y = 0;
    if (poDomain->Width() != 0.0)
        x = static_cast<uint32_t>(std::round(
            GDAL_HILBERT_MAX * (dfX - poDomain->MinX) / poDomain->Width()));
    if (poDomain->Height() != 0.0)
        y = static_cast<uint32_t>(std::round(
            GDAL_HILBERT_MAX * (dfY - poDomain->MinY) / poDomain->Height()));
    return GDALHilbertCode(x, y);
}
