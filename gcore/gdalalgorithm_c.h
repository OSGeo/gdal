/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDALAlgorithm C API
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_ALGORITHM_C_INCLUDED
#define GDAL_ALGORITHM_C_INCLUDED

#include "cpl_port.h"
#include "cpl_progress.h"

#include "gdal.h"

/************************************************************************/
/************************************************************************/
/*                      GDAL Algorithm C API                            */
/************************************************************************/
/************************************************************************/

CPL_C_START

/** Type of an argument */
typedef enum
{
    /** Boolean type. Value is a bool. */
    GAAT_BOOLEAN,
    /** Single-value string type. Value is a std::string */
    GAAT_STRING,
    /** Single-value integer type. Value is a int */
    GAAT_INTEGER,
    /** Single-value real type. Value is a double */
    GAAT_REAL,
    /** Dataset type. Value is a GDALArgDatasetValue */
    GAAT_DATASET,
    /** Multi-value string type. Value is a std::vector<std::string> */
    GAAT_STRING_LIST,
    /** Multi-value integer type. Value is a std::vector<int> */
    GAAT_INTEGER_LIST,
    /** Multi-value real type. Value is a std::vector<double> */
    GAAT_REAL_LIST,
    /** Multi-value dataset type. Value is a std::vector<GDALArgDatasetValue> */
    GAAT_DATASET_LIST,
} GDALAlgorithmArgType;

/** Return whether the argument type is a list / multi-valued one. */
bool CPL_DLL GDALAlgorithmArgTypeIsList(GDALAlgorithmArgType type);

/** Return the string representation of the argument type */
const char CPL_DLL *GDALAlgorithmArgTypeName(GDALAlgorithmArgType type);

/** Opaque C type for GDALArgDatasetValue */
typedef struct GDALArgDatasetValueHS *GDALArgDatasetValueH;

/** Opaque C type for GDALAlgorithmArg */
typedef struct GDALAlgorithmArgHS *GDALAlgorithmArgH;

/** Opaque C type for GDALAlgorithm */
typedef struct GDALAlgorithmHS *GDALAlgorithmH;

/** Opaque C type for GDALAlgorithmRegistry */
typedef struct GDALAlgorithmRegistryHS *GDALAlgorithmRegistryH;

/************************************************************************/
/*                  GDALAlgorithmRegistryH API                          */
/************************************************************************/

GDALAlgorithmRegistryH CPL_DLL GDALGetGlobalAlgorithmRegistry(void);

void CPL_DLL GDALAlgorithmRegistryRelease(GDALAlgorithmRegistryH);

char CPL_DLL **GDALAlgorithmRegistryGetAlgNames(GDALAlgorithmRegistryH);

GDALAlgorithmH CPL_DLL GDALAlgorithmRegistryInstantiateAlg(
    GDALAlgorithmRegistryH, const char *pszAlgName);

GDALAlgorithmH CPL_DLL GDALAlgorithmRegistryInstantiateAlgFromPath(
    GDALAlgorithmRegistryH, const char *const *papszAlgPath);

/************************************************************************/
/*                        GDALAlgorithmH API                            */
/************************************************************************/

void CPL_DLL GDALAlgorithmRelease(GDALAlgorithmH);

const char CPL_DLL *GDALAlgorithmGetName(GDALAlgorithmH);

const char CPL_DLL *GDALAlgorithmGetDescription(GDALAlgorithmH);

const char CPL_DLL *GDALAlgorithmGetLongDescription(GDALAlgorithmH);

const char CPL_DLL *GDALAlgorithmGetHelpFullURL(GDALAlgorithmH);

bool CPL_DLL GDALAlgorithmHasSubAlgorithms(GDALAlgorithmH);

char CPL_DLL **GDALAlgorithmGetSubAlgorithmNames(GDALAlgorithmH);

GDALAlgorithmH CPL_DLL
GDALAlgorithmInstantiateSubAlgorithm(GDALAlgorithmH, const char *pszSubAlgName);

bool CPL_DLL GDALAlgorithmParseCommandLineArguments(GDALAlgorithmH,
                                                    CSLConstList papszArgs);

GDALAlgorithmH CPL_DLL GDALAlgorithmGetActualAlgorithm(GDALAlgorithmH);

bool CPL_DLL GDALAlgorithmRun(GDALAlgorithmH, GDALProgressFunc pfnProgress,
                              void *pProgressData);

bool CPL_DLL GDALAlgorithmFinalize(GDALAlgorithmH);

char CPL_DLL *GDALAlgorithmGetUsageAsJSON(GDALAlgorithmH);

char CPL_DLL **GDALAlgorithmGetArgNames(GDALAlgorithmH);

GDALAlgorithmArgH CPL_DLL GDALAlgorithmGetArg(GDALAlgorithmH,
                                              const char *pszArgName);

GDALAlgorithmArgH CPL_DLL GDALAlgorithmGetArgNonConst(GDALAlgorithmH,
                                                      const char *pszArgName);

/************************************************************************/
/*                      GDALAlgorithmArgH API                           */
/************************************************************************/

void CPL_DLL GDALAlgorithmArgRelease(GDALAlgorithmArgH);

const char CPL_DLL *GDALAlgorithmArgGetName(GDALAlgorithmArgH);

GDALAlgorithmArgType CPL_DLL GDALAlgorithmArgGetType(GDALAlgorithmArgH);

const char CPL_DLL *GDALAlgorithmArgGetDescription(GDALAlgorithmArgH);

const char CPL_DLL *GDALAlgorithmArgGetShortName(GDALAlgorithmArgH);

char CPL_DLL **GDALAlgorithmArgGetAliases(GDALAlgorithmArgH);

const char CPL_DLL *GDALAlgorithmArgGetMetaVar(GDALAlgorithmArgH);

const char CPL_DLL *GDALAlgorithmArgGetCategory(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgIsPositional(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgIsRequired(GDALAlgorithmArgH);

int CPL_DLL GDALAlgorithmArgGetMinCount(GDALAlgorithmArgH);

int CPL_DLL GDALAlgorithmArgGetMaxCount(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgGetPackedValuesAllowed(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgGetRepeatedArgAllowed(GDALAlgorithmArgH);

char CPL_DLL **GDALAlgorithmArgGetChoices(GDALAlgorithmArgH);

char CPL_DLL **GDALAlgorithmArgGetMetadataItem(GDALAlgorithmArgH, const char *);

bool CPL_DLL GDALAlgorithmArgIsExplicitlySet(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgHasDefaultValue(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgGetDefaultAsBoolean(GDALAlgorithmArgH);

const char CPL_DLL *GDALAlgorithmArgGetDefaultAsString(GDALAlgorithmArgH);

int CPL_DLL GDALAlgorithmArgGetDefaultAsInteger(GDALAlgorithmArgH);

double CPL_DLL GDALAlgorithmArgGetDefaultAsDouble(GDALAlgorithmArgH);

char CPL_DLL **GDALAlgorithmArgGetDefaultAsStringList(GDALAlgorithmArgH);

const int CPL_DLL *GDALAlgorithmArgGetDefaultAsIntegerList(GDALAlgorithmArgH,
                                                           size_t *pnCount);

const double CPL_DLL *GDALAlgorithmArgGetDefaultAsDoubleList(GDALAlgorithmArgH,
                                                             size_t *pnCount);

bool CPL_DLL GDALAlgorithmArgIsHidden(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgIsHiddenForCLI(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgIsHiddenForAPI(GDALAlgorithmArgH);

#ifndef DOXYGEN_SKIP
bool CPL_DLL GDALAlgorithmArgIsOnlyForCLI(GDALAlgorithmArgH)
    CPL_WARN_DEPRECATED("Use GDALAlgorithmArgIsHiddenForAPI() instead");
#endif

bool CPL_DLL GDALAlgorithmArgIsInput(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgIsOutput(GDALAlgorithmArgH);

const char CPL_DLL *GDALAlgorithmArgGetMutualExclusionGroup(GDALAlgorithmArgH);

bool CPL_DLL GDALAlgorithmArgGetAsBoolean(GDALAlgorithmArgH);

const char CPL_DLL *GDALAlgorithmArgGetAsString(GDALAlgorithmArgH);

GDALArgDatasetValueH
    CPL_DLL GDALAlgorithmArgGetAsDatasetValue(GDALAlgorithmArgH);

int CPL_DLL GDALAlgorithmArgGetAsInteger(GDALAlgorithmArgH);

double CPL_DLL GDALAlgorithmArgGetAsDouble(GDALAlgorithmArgH);

char CPL_DLL **GDALAlgorithmArgGetAsStringList(GDALAlgorithmArgH);

const int CPL_DLL *GDALAlgorithmArgGetAsIntegerList(GDALAlgorithmArgH,
                                                    size_t *pnCount);

const double CPL_DLL *GDALAlgorithmArgGetAsDoubleList(GDALAlgorithmArgH,
                                                      size_t *pnCount);

bool CPL_DLL GDALAlgorithmArgSetAsBoolean(GDALAlgorithmArgH, bool);

bool CPL_DLL GDALAlgorithmArgSetAsString(GDALAlgorithmArgH, const char *);

bool CPL_DLL GDALAlgorithmArgSetAsDatasetValue(GDALAlgorithmArgH hArg,
                                               GDALArgDatasetValueH value);

bool CPL_DLL GDALAlgorithmArgSetDataset(GDALAlgorithmArgH hArg, GDALDatasetH);

bool CPL_DLL GDALAlgorithmArgSetDatasets(GDALAlgorithmArgH hArg, size_t nCount,
                                         GDALDatasetH *);

bool CPL_DLL GDALAlgorithmArgSetDatasetNames(GDALAlgorithmArgH hArg,
                                             CSLConstList);

bool CPL_DLL GDALAlgorithmArgSetAsInteger(GDALAlgorithmArgH, int);

bool CPL_DLL GDALAlgorithmArgSetAsDouble(GDALAlgorithmArgH, double);

bool CPL_DLL GDALAlgorithmArgSetAsStringList(GDALAlgorithmArgH, CSLConstList);

bool CPL_DLL GDALAlgorithmArgSetAsIntegerList(GDALAlgorithmArgH, size_t nCount,
                                              const int *pnValues);

bool CPL_DLL GDALAlgorithmArgSetAsDoubleList(GDALAlgorithmArgH, size_t nCount,
                                             const double *pnValues);

/** Binary-or combination of GDAL_OF_RASTER, GDAL_OF_VECTOR,
 * GDAL_OF_MULTIDIM_RASTER, possibly with GDAL_OF_UPDATE.
 */
typedef int GDALArgDatasetType;

GDALArgDatasetType CPL_DLL GDALAlgorithmArgGetDatasetType(GDALAlgorithmArgH);

/** Bit indicating that the name component of GDALArgDatasetValue is accepted. */
#define GADV_NAME (1 << 0)
/** Bit indicating that the dataset component of GDALArgDatasetValue is accepted. */
#define GADV_OBJECT (1 << 1)

int CPL_DLL GDALAlgorithmArgGetDatasetInputFlags(GDALAlgorithmArgH);

int CPL_DLL GDALAlgorithmArgGetDatasetOutputFlags(GDALAlgorithmArgH);

/************************************************************************/
/*                    GDALArgDatasetValueH API                          */
/************************************************************************/

GDALArgDatasetValueH CPL_DLL GDALArgDatasetValueCreate(void);

void CPL_DLL GDALArgDatasetValueRelease(GDALArgDatasetValueH);

const char CPL_DLL *GDALArgDatasetValueGetName(GDALArgDatasetValueH);

GDALDatasetH CPL_DLL GDALArgDatasetValueGetDatasetRef(GDALArgDatasetValueH);

GDALDatasetH
    CPL_DLL GDALArgDatasetValueGetDatasetIncreaseRefCount(GDALArgDatasetValueH);

void CPL_DLL GDALArgDatasetValueSetName(GDALArgDatasetValueH, const char *);

void CPL_DLL GDALArgDatasetValueSetDataset(GDALArgDatasetValueH, GDALDatasetH);

CPL_C_END

#endif  // GDAL_ALGORITHM_C_INCLUDED
