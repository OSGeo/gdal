/******************************************************************************
 * Name:     Algorithm.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Algorithm SWIG Interface declarations.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

%{
#include "gdalalgorithm.h"

typedef GDALAlgorithmRegistryHS GDALAlgorithmRegistryHS;
typedef GDALAlgorithmHS GDALAlgorithmHS;
typedef GDALAlgorithmArgHS GDALAlgorithmArgHS;
typedef GDALArgDatasetValueHS GDALArgDatasetValueHS;
%}

typedef int GDALAlgorithmArgType;

%rename (AlgorithmRegistry) GDALAlgorithmRegistryHS;
%rename (Algorithm) GDALAlgorithmHS;
%rename (AlgorithmArg) GDALAlgorithmArgHS;
%rename (ArgDatasetValue) GDALArgDatasetValueHS;

/* -------------------------------------------------------------------- */
/*      Global methods                                                  */
/* -------------------------------------------------------------------- */

%rename (GetGlobalAlgorithmRegistry) GDALGetGlobalAlgorithmRegistry;
%newobject GDALGetGlobalAlgorithmRegistry;
GDALAlgorithmRegistryHS* GDALGetGlobalAlgorithmRegistry(void);

%rename (AlgorithmArgTypeIsList) GDALAlgorithmArgTypeIsList;
bool GDALAlgorithmArgTypeIsList(GDALAlgorithmArgType type);

%rename (AlgorithmArgTypeName) GDALAlgorithmArgTypeName;
const char *GDALAlgorithmArgTypeName(GDALAlgorithmArgType type);

/* -------------------------------------------------------------------- */
/*      GDALAlgorithmArgHS                                              */
/* -------------------------------------------------------------------- */

class GDALAlgorithmArgHS {
private:
  GDALAlgorithmArgHS();
public:
%extend {
  ~GDALAlgorithmArgHS() {
    GDALAlgorithmArgRelease( self );
  }

  const char* GetName() {
    return GDALAlgorithmArgGetName(self);
  }

#if defined(SWIGCSHARP)
  GDALAlgorithmArgType GetType_()
#else
  GDALAlgorithmArgType GetType()
#endif
  {
    return GDALAlgorithmArgGetType(self);
  }

  const char* GetDescription() {
    return GDALAlgorithmArgGetDescription(self);
  }

  const char* GetShortName() {
    return GDALAlgorithmArgGetShortName(self);
  }

%apply (char **CSL) {char **};
  char **GetAliases() {
    return GDALAlgorithmArgGetAliases( self );
  }
%clear char **;

  const char* GetMetaVar() {
    return GDALAlgorithmArgGetMetaVar(self);
  }

  const char* GetCategory() {
    return GDALAlgorithmArgGetCategory(self);
  }

  bool IsPositional() {
    return GDALAlgorithmArgIsPositional(self);
  }

  bool IsRequired() {
    return GDALAlgorithmArgIsRequired(self);
  }

  int GetMinCount() {
    return GDALAlgorithmArgGetMinCount(self);
  }

  int GetMaxCount() {
    return GDALAlgorithmArgGetMaxCount(self);
  }

  bool GetPackedValuesAllowed() {
    return GDALAlgorithmArgGetPackedValuesAllowed(self);
  }

  bool GetRepeatedArgAllowed() {
    return GDALAlgorithmArgGetRepeatedArgAllowed(self);
  }

%apply (char **CSL) {char **};
  char **GetChoices() {
    return GDALAlgorithmArgGetChoices( self );
  }
%clear char **;

  bool IsExplicitlySet() {
    return GDALAlgorithmArgIsExplicitlySet(self);
  }

  bool HasDefaultValue() {
    return GDALAlgorithmArgHasDefaultValue(self);
  }

  bool IsHiddenForCLI() {
    return GDALAlgorithmArgIsHiddenForCLI(self);
  }

  bool IsOnlyForCLI() {
    return GDALAlgorithmArgIsOnlyForCLI(self);
  }

  bool IsInput() {
    return GDALAlgorithmArgIsInput(self);
  }

  bool IsOutput() {
    return GDALAlgorithmArgIsOutput(self);
  }

  const char* GetMutualExclusionGroup() {
    return GDALAlgorithmArgGetMutualExclusionGroup(self);
  }

  bool GetAsBoolean() {
    return GDALAlgorithmArgGetAsBoolean(self);
  }

  const char* GetAsString() {
    return GDALAlgorithmArgGetAsString(self);
  }

  int GetAsInteger() {
    return GDALAlgorithmArgGetAsInteger(self);
  }

  int GetAsDouble() {
    return GDALAlgorithmArgGetAsDouble(self);
  }

%newobject GetAsDatasetValue;
  GDALArgDatasetValueHS* GetAsDatasetValue() {
    return GDALAlgorithmArgGetAsDatasetValue(self);
  }

%apply (char **CSL) {char **};
  char **GetAsStringList() {
    return GDALAlgorithmArgGetAsStringList( self );
  }
%clear char **;

#if defined(SWIGPYTHON)
  void GetAsIntegerList(int *nLen, const int **pList) {
     size_t nLenSizet = 0;
    *pList = GDALAlgorithmArgGetAsIntegerList(self, &nLenSizet);
    *nLen = (int)nLenSizet;
  }
#endif

#if defined(SWIGPYTHON)
  void GetAsDoubleList(int *nLen, const double **pList) {
     size_t nLenSizet = 0;
    *pList = GDALAlgorithmArgGetAsDoubleList(self, &nLenSizet);
    *nLen = (int)nLenSizet;
  }
#endif

  bool SetAsBoolean(bool value) {
    return GDALAlgorithmArgSetAsBoolean(self, value);
  }

  bool SetAsString(const char* value) {
    return GDALAlgorithmArgSetAsString(self, value);
  }

  bool SetAsInteger(int value) {
    return GDALAlgorithmArgSetAsInteger(self, value);
  }

  bool SetAsDouble(double value) {
    return GDALAlgorithmArgSetAsDouble(self, value);
  }

  bool SetAsDatasetValue(GDALArgDatasetValueHS* value) {
    return GDALAlgorithmArgSetAsDatasetValue(self, value);
  }

%apply (char **options) {char**value};
  bool SetAsStringList(char** value) {
    return GDALAlgorithmArgSetAsStringList(self, value);
  }
%clear char** value;

  bool SetAsIntegerList(int nList, int *pList) {
    return GDALAlgorithmArgSetAsIntegerList(self, nList, pList);
  }

  bool SetAsDoubleList(int nList, double *pList) {
    return GDALAlgorithmArgSetAsDoubleList(self, nList, pList);
  }

  bool SetDataset(GDALDatasetShadow* ds) {
    return GDALAlgorithmArgSetDataset(self, ds);
  }

#if defined(SWIGPYTHON)
  bool SetDatasets(int object_list_count, GDALDatasetShadow** poObjects) {
    return GDALAlgorithmArgSetDatasets(self, object_list_count, poObjects);
  }
#endif

%apply (char **options) {char** names};
  bool SetDatasetNames(char** names) {
    return GDALAlgorithmArgSetDatasetNames(self, names);
  }
%clear char** names;

}
};

/* -------------------------------------------------------------------- */
/*      GDALAlgorithmHS                                                 */
/* -------------------------------------------------------------------- */

class GDALAlgorithmHS {
private:
  GDALAlgorithmHS();
public:
%extend {
  ~GDALAlgorithmHS() {
    GDALAlgorithmRelease( self );
  }

  const char* GetName() {
    return GDALAlgorithmGetName(self);
  }

  const char* GetDescription() {
    return GDALAlgorithmGetDescription(self);
  }

  const char* GetLongDescription() {
    return GDALAlgorithmGetLongDescription(self);
  }

  const char* GetHelpFullURL() {
    return GDALAlgorithmGetHelpFullURL(self);
  }

  bool HasSubAlgorithms() {
    return GDALAlgorithmHasSubAlgorithms(self);
  }

%apply (char **CSL) {char **};
  char **GetSubAlgorithmNames() {
    return GDALAlgorithmGetSubAlgorithmNames( self );
  }
%clear char **;

%apply Pointer NONNULL {const char *algName};
%newobject InstantiateSubAlgorithm;
  GDALAlgorithmHS* InstantiateSubAlgorithm(const char *algName) {
    return GDALAlgorithmInstantiateSubAlgorithm(self, algName);
  }
%clear const char *algName;

%apply (char **options) { char ** args };
  bool ParseCommandLineArguments(char** args) {
    return GDALAlgorithmParseCommandLineArguments(self, args);
  }
%clear char** args;

%newobject GetActualAlgorithm;
  GDALAlgorithmHS* GetActualAlgorithm() {
    return GDALAlgorithmGetActualAlgorithm(self);
  }

  bool Run(GDALProgressFunc callback=NULL, void* callback_data=NULL) {
    return GDALAlgorithmRun(self, callback, callback_data);
  }

#if defined(SWIGCSHARP)
  bool Finalize_()
#else
  bool Finalize()
#endif
  {
    return GDALAlgorithmFinalize(self);
  }

%apply (char **options) { char ** args };
  bool ParseRunAndFinalize(char** args, GDALProgressFunc callback=NULL, void* callback_data=NULL) {
    return GDALAlgorithmParseCommandLineArguments(self, args) &&
           GDALAlgorithmRun(self, callback, callback_data) &&
           GDALAlgorithmFinalize(self);
  }
%clear char** args;

  retStringAndCPLFree *GetUsageAsJSON() {
    return GDALAlgorithmGetUsageAsJSON(self);
  }

%apply (char **CSL) {char **};
  char **GetArgNames() {
    return GDALAlgorithmGetArgNames( self );
  }
%clear char **;

%apply Pointer NONNULL {const char *argName};
%newobject GetArg;
  GDALAlgorithmArgHS* GetArg(const char *argName) {
    return GDALAlgorithmGetArg(self, argName);
  }
%clear const char *argName;

}
};

/* -------------------------------------------------------------------- */
/*      GDALAlgorithmRegistryHS                                         */
/* -------------------------------------------------------------------- */

class GDALAlgorithmRegistryHS {
private:
  GDALAlgorithmRegistryHS();
public:
%extend {
  ~GDALAlgorithmRegistryHS() {
    GDALAlgorithmRegistryRelease( self );
  }

%apply (char **CSL) {char **};
  char **GetAlgNames() {
    return GDALAlgorithmRegistryGetAlgNames( self );
  }
%clear char **;

%apply Pointer NONNULL {const char *algName};
%newobject InstantiateAlg;
  GDALAlgorithmHS* InstantiateAlg(const char *algName) {
    return GDALAlgorithmRegistryInstantiateAlg(self, algName);
  }
%clear const char *algName;
}
};

/* -------------------------------------------------------------------- */
/*      GDALArgDatasetValueHS                                           */
/* -------------------------------------------------------------------- */

class GDALArgDatasetValueHS {
private:
  GDALArgDatasetValueHS();
public:
%extend {
  ~GDALArgDatasetValueHS() {
    GDALArgDatasetValueRelease( self );
  }

#if defined(SWIGCSHARP)
  int GetType_()
#else
  int GetType()
#endif
  {
    return GDALArgDatasetValueGetType(self);
  }

  const char* GetName() {
    return GDALArgDatasetValueGetName(self);
  }

%newobject GetDataset;
  GDALDatasetShadow* GetDataset() {
    return GDALArgDatasetValueGetDatasetIncreaseRefCount(self);
  }

  int GetInputFlags() {
    return GDALArgDatasetValueGetInputFlags(self);
  }

  int GetOutputFlags() {
    return GDALArgDatasetValueGetOutputFlags(self);
  }

  void SetName(const char* name) {
    GDALArgDatasetValueSetName(self, name);
  }

  void SetDataset(GDALDatasetShadow* ds) {
    GDALArgDatasetValueSetDataset(self, ds);
  }
}
};
