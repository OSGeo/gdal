/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation of derived subdatasets
 * Author:   Julien Michel <julien dot michel at cnes dot fr>
 *
 ******************************************************************************
 * Copyright (c) 2016 Julien Michel <julien dot michel at cnes dot fr>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/
#include "derivedlist.h"
#include "gdal.h"

CPL_C_START

static const DerivedDatasetDescription asDDSDesc[] = {
    {"AMPLITUDE", "Amplitude of input bands", "mod", "complex", "Float64"},
    {"PHASE", "Phase of input bands", "phase", "complex", "Float64"},
    {"REAL", "Real part of input bands", "real", "complex", "Float64"},
    {"IMAG", "Imaginary part of input bands", "imag", "complex", "Float64"},
    {"CONJ", "Conjugate of input bands", "conj", "complex", "CFloat64"},
    {"INTENSITY", "Intensity (squared amplitude) of input bands", "intensity",
     "complex", "Float64"},
    {"LOGAMPLITUDE", "log10 of amplitude of input bands", "log10", "all",
     "Float64"}};

#define NB_DERIVED_DATASETS (sizeof(asDDSDesc) / sizeof(asDDSDesc[0]))

const DerivedDatasetDescription *CPL_STDCALL
GDALGetDerivedDatasetDescriptions(unsigned int *pnDescriptionCount)
{
    *pnDescriptionCount = (unsigned int)NB_DERIVED_DATASETS;
    return asDDSDesc;
}

CPL_C_END
