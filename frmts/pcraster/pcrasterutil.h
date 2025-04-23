/******************************************************************************
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster driver support declarations.
 * Author:   Kor de Jong, Oliver Schmitz
 *
 ******************************************************************************
 * Copyright (c) PCRaster owners
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef INCLUDED_PCRASTERUTIL
#define INCLUDED_PCRASTERUTIL

#include "csf.h"
#include "gdal_priv.h"
#include "pcrtypes.h"

GDALDataType cellRepresentation2GDALType(CSF_CR cellRepresentation);

CSF_VS string2ValueScale(const std::string &string);

std::string valueScale2String(CSF_VS valueScale);

std::string cellRepresentation2String(CSF_CR cellRepresentation);

CSF_VS GDALType2ValueScale(GDALDataType type);

/*
CSF_CR             string2PCRasterCellRepresentation(
                                        const std::string& string);
                                        */

CSF_CR GDALType2CellRepresentation(GDALDataType type, bool exact);

void *createBuffer(size_t size, CSF_CR type);

void deleteBuffer(void *buffer, CSF_CR type);

bool isContinuous(CSF_VS valueScale);

double missingValue(CSF_CR type);

void alterFromStdMV(void *buffer, size_t size, CSF_CR cellRepresentation,
                    double missingValue);

void alterToStdMV(void *buffer, size_t size, CSF_CR cellRepresentation,
                  double missingValue);

MAP *mapOpen(std::string const &filename, MOPEN_PERM mode);

CSF_VS fitValueScale(CSF_VS valueScale, CSF_CR cellRepresentation);

void castValuesToBooleanRange(void *buffer, size_t size,
                              CSF_CR cellRepresentation);

void castValuesToDirectionRange(void *buffer, size_t size);

void castValuesToLddRange(void *buffer, size_t size);

template <typename T> struct CastToBooleanRange
{
    void operator()(T &value) const
    {
        if (!pcr::isMV(value))
        {
            if (value != 0)
            {
                value = T(value > T(0));
            }
            else
            {
                pcr::setMV(value);
            }
        }
    }
};

template <> struct CastToBooleanRange<UINT1>
{
    void operator()(UINT1 &value) const
    {
        if (!pcr::isMV(value))
        {
            value = UINT1(value > UINT1(0));
        }
    }
};

template <> struct CastToBooleanRange<UINT2>
{
    void operator()(UINT2 &value) const
    {
        if (!pcr::isMV(value))
        {
            value = UINT2(value > UINT2(0));
        }
    }
};

template <> struct CastToBooleanRange<UINT4>
{
    void operator()(UINT4 &value) const
    {
        if (!pcr::isMV(value))
        {
            value = UINT4(value > UINT4(0));
        }
    }
};

struct CastToDirection
{
    void operator()(REAL4 &value) const
    {
        REAL4 factor = static_cast<REAL4>(M_PI / 180.0);
        if (!pcr::isMV(value))
        {
            value = REAL4(value * factor);
        }
    }
};

struct CastToLdd
{
    void operator()(UINT1 &value) const
    {
        if (!pcr::isMV(value))
        {
            if ((value < 1) || (value > 9))
            {
                CPLError(CE_Warning, CPLE_IllegalArg,
                         "PCRaster driver: incorrect LDD value used, assigned "
                         "MV instead");
                pcr::setMV(value);
            }
            else
            {
                value = UINT1(value);
            }
        }
    }
};

#endif  // INCLUDED_PCRASTERUTIL
