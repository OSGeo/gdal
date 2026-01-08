/******************************************************************************
 *
 * Name:     gdal_driver.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALDriver class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALDRIVER_H_INCLUDED
#define GDALDRIVER_H_INCLUDED

#include "cpl_port.h"
#include "gdal.h"
#include "gdal_majorobject.h"

#include <vector>

class GDALAlgorithm;
class GDALDataset;
class GDALOpenInfo;

/* ******************************************************************** */
/*                          GDALIdentifyEnum                            */
/* ******************************************************************** */

/**
 * Enumeration used by GDALDriver::pfnIdentify().
 *
 */
typedef enum
{
    /** Identify could not determine if the file is recognized or not by the
       probed driver. */
    GDAL_IDENTIFY_UNKNOWN = -1,
    /** Identify determined the file is not recognized by the probed driver. */
    GDAL_IDENTIFY_FALSE = 0,
    /** Identify determined the file is recognized by the probed driver. */
    GDAL_IDENTIFY_TRUE = 1
} GDALIdentifyEnum;

/* ******************************************************************** */
/*                              GDALDriver                              */
/* ******************************************************************** */

/**
 * \brief Format specific driver.
 *
 * An instance of this class is created for each supported format, and
 * manages information about the format.
 *
 * This roughly corresponds to a file format, though some
 * drivers may be gateways to many formats through a secondary
 * multi-library.
 */

class CPL_DLL GDALDriver : public GDALMajorObject
{
  public:
    GDALDriver();
    ~GDALDriver() override;

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override;

    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain = "") override;

    /* -------------------------------------------------------------------- */
    /*      Public C++ methods.                                             */
    /* -------------------------------------------------------------------- */
    GDALDataset *Create(const char *pszName, int nXSize, int nYSize, int nBands,
                        GDALDataType eType,
                        CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;

    GDALDataset *
    CreateMultiDimensional(const char *pszName,
                           CSLConstList papszRootGroupOptions,
                           CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;

    CPLErr Delete(const char *pszName);
    CPLErr Delete(GDALDataset *poDS, CSLConstList papszFileList);
    CPLErr Rename(const char *pszNewName, const char *pszOldName);
    CPLErr CopyFiles(const char *pszNewName, const char *pszOldName);

    GDALDataset *CreateCopy(const char *, GDALDataset *, int,
                            CSLConstList papszOptions,
                            GDALProgressFunc pfnProgress,
                            void *pProgressData) CPL_WARN_UNUSED_RESULT;

    bool CanVectorTranslateFrom(const char *pszDestName,
                                GDALDataset *poSourceDS,
                                CSLConstList papszVectorTranslateArguments,
                                char ***ppapszFailureReasons);

    /**
     * \brief Returns TRUE if the given open option is supported by the driver.
     * @param pszOpenOptionName name of the open option to be checked
     * @return TRUE if the driver supports the open option
     * @since GDAL 3.11
     */
    bool HasOpenOption(const char *pszOpenOptionName) const;

    GDALDataset *
    VectorTranslateFrom(const char *pszDestName, GDALDataset *poSourceDS,
                        CSLConstList papszVectorTranslateArguments,
                        GDALProgressFunc pfnProgress,
                        void *pProgressData) CPL_WARN_UNUSED_RESULT;

    /* -------------------------------------------------------------------- */
    /*      The following are semiprivate, not intended to be accessed      */
    /*      by anyone but the formats instantiating and populating the      */
    /*      drivers.                                                        */
    /* -------------------------------------------------------------------- */
    //! @cond Doxygen_Suppress

    // Not aimed at being used outside of GDAL. Use GDALDataset::Open() instead
    GDALDataset *Open(GDALOpenInfo *poOpenInfo, bool bSetOpenOptions);

    typedef GDALDataset *(*OpenCallback)(GDALOpenInfo *);

    OpenCallback pfnOpen = nullptr;

    virtual OpenCallback GetOpenCallback()
    {
        return pfnOpen;
    }

    typedef GDALDataset *(*CreateCallback)(const char *pszName, int nXSize,
                                           int nYSize, int nBands,
                                           GDALDataType eType,
                                           char **papszOptions);

    CreateCallback pfnCreate = nullptr;

    virtual CreateCallback GetCreateCallback()
    {
        return pfnCreate;
    }

    GDALDataset *(*pfnCreateEx)(GDALDriver *, const char *pszName, int nXSize,
                                int nYSize, int nBands, GDALDataType eType,
                                char **papszOptions) = nullptr;

    typedef GDALDataset *(*CreateMultiDimensionalCallback)(
        const char *pszName, CSLConstList papszRootGroupOptions,
        CSLConstList papszOptions);

    CreateMultiDimensionalCallback pfnCreateMultiDimensional = nullptr;

    virtual CreateMultiDimensionalCallback GetCreateMultiDimensionalCallback()
    {
        return pfnCreateMultiDimensional;
    }

    typedef CPLErr (*DeleteCallback)(const char *pszName);
    DeleteCallback pfnDelete = nullptr;

    virtual DeleteCallback GetDeleteCallback()
    {
        return pfnDelete;
    }

    typedef GDALDataset *(*CreateCopyCallback)(const char *, GDALDataset *, int,
                                               char **,
                                               GDALProgressFunc pfnProgress,
                                               void *pProgressData);

    CreateCopyCallback pfnCreateCopy = nullptr;

    virtual CreateCopyCallback GetCreateCopyCallback()
    {
        return pfnCreateCopy;
    }

    void *pDriverData = nullptr;

    void (*pfnUnloadDriver)(GDALDriver *) = nullptr;

    /** Identify() if the file is recognized or not by the driver.

       Return GDAL_IDENTIFY_TRUE (1) if the passed file is certainly recognized
       by the driver. Return GDAL_IDENTIFY_FALSE (0) if the passed file is
       certainly NOT recognized by the driver. Return GDAL_IDENTIFY_UNKNOWN (-1)
       if the passed file may be or may not be recognized by the driver, and
       that a potentially costly test must be done with pfnOpen.
    */
    int (*pfnIdentify)(GDALOpenInfo *) = nullptr;
    int (*pfnIdentifyEx)(GDALDriver *, GDALOpenInfo *) = nullptr;

    typedef CPLErr (*RenameCallback)(const char *pszNewName,
                                     const char *pszOldName);
    RenameCallback pfnRename = nullptr;

    virtual RenameCallback GetRenameCallback()
    {
        return pfnRename;
    }

    typedef CPLErr (*CopyFilesCallback)(const char *pszNewName,
                                        const char *pszOldName);
    CopyFilesCallback pfnCopyFiles = nullptr;

    virtual CopyFilesCallback GetCopyFilesCallback()
    {
        return pfnCopyFiles;
    }

    // Used for legacy OGR drivers, and Python drivers
    GDALDataset *(*pfnOpenWithDriverArg)(GDALDriver *,
                                         GDALOpenInfo *) = nullptr;

    /* For legacy OGR drivers */
    GDALDataset *(*pfnCreateVectorOnly)(GDALDriver *, const char *pszName,
                                        char **papszOptions) = nullptr;
    CPLErr (*pfnDeleteDataSource)(GDALDriver *, const char *pszName) = nullptr;

    /** Whether pfnVectorTranslateFrom() can be run given the source dataset
     * and the non-positional arguments of GDALVectorTranslate() stored
     * in papszVectorTranslateArguments.
     */
    bool (*pfnCanVectorTranslateFrom)(
        const char *pszDestName, GDALDataset *poSourceDS,
        CSLConstList papszVectorTranslateArguments,
        char ***ppapszFailureReasons) = nullptr;

    /** Creates a copy from the specified source dataset, using the
     * non-positional arguments of GDALVectorTranslate() stored
     * in papszVectorTranslateArguments.
     */
    GDALDataset *(*pfnVectorTranslateFrom)(
        const char *pszDestName, GDALDataset *poSourceDS,
        CSLConstList papszVectorTranslateArguments,
        GDALProgressFunc pfnProgress, void *pProgressData) = nullptr;

    /**
     * Returns a (possibly null) pointer to the Subdataset informational function
     * from the subdataset file name.
     */
    GDALSubdatasetInfo *(*pfnGetSubdatasetInfoFunc)(const char *pszFileName) =
        nullptr;

    typedef GDALAlgorithm *(*InstantiateAlgorithmCallback)(
        const std::vector<std::string> &aosPath);
    InstantiateAlgorithmCallback pfnInstantiateAlgorithm = nullptr;

    virtual InstantiateAlgorithmCallback GetInstantiateAlgorithmCallback()
    {
        return pfnInstantiateAlgorithm;
    }

    /** Instantiate an algorithm by its full path (omitting leading "gdal").
     * For example {"driver", "pdf", "list-layers"}
     */
    GDALAlgorithm *
    InstantiateAlgorithm(const std::vector<std::string> &aosPath);

    /** Declare an algorithm by its full path (omitting leading "gdal").
     * For example {"driver", "pdf", "list-layers"}
     */
    void DeclareAlgorithm(const std::vector<std::string> &aosPath);

    //! @endcond

    /* -------------------------------------------------------------------- */
    /*      Helper methods.                                                 */
    /* -------------------------------------------------------------------- */
    //! @cond Doxygen_Suppress
    GDALDataset *DefaultCreateCopy(const char *, GDALDataset *, int,
                                   CSLConstList papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData) CPL_WARN_UNUSED_RESULT;

    static CPLErr DefaultCreateCopyMultiDimensional(
        GDALDataset *poSrcDS, GDALDataset *poDstDS, bool bStrict,
        CSLConstList /*papszOptions*/, GDALProgressFunc pfnProgress,
        void *pProgressData);

    static CPLErr DefaultCopyMasks(GDALDataset *poSrcDS, GDALDataset *poDstDS,
                                   int bStrict);
    static CPLErr DefaultCopyMasks(GDALDataset *poSrcDS, GDALDataset *poDstDS,
                                   int bStrict, CSLConstList papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    CPLErr QuietDeleteForCreateCopy(const char *pszFilename,
                                    GDALDataset *poSrcDS);

    //! @endcond
    static CPLErr QuietDelete(const char *pszName,
                              CSLConstList papszAllowedDrivers = nullptr);

    //! @cond Doxygen_Suppress
    static CPLErr DefaultRename(const char *pszNewName, const char *pszOldName);
    static CPLErr DefaultCopyFiles(const char *pszNewName,
                                   const char *pszOldName);
    static void DefaultCopyMetadata(GDALDataset *poSrcDS, GDALDataset *poDstDS,
                                    CSLConstList papszOptions,
                                    CSLConstList papszExcludedDomains);

    //! @endcond

    /** Convert a GDALDriver* to a GDALDriverH.
     */
    static inline GDALDriverH ToHandle(GDALDriver *poDriver)
    {
        return static_cast<GDALDriverH>(poDriver);
    }

    /** Convert a GDALDriverH to a GDALDriver*.
     */
    static inline GDALDriver *FromHandle(GDALDriverH hDriver)
    {
        return static_cast<GDALDriver *>(hDriver);
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALDriver)
};

// Macro used so that Identify and driver metadata methods in drivers built
// as plugin can be duplicated in libgdal core and in the driver under different
// names
#ifdef PLUGIN_FILENAME
#define PLUGIN_SYMBOL_NAME(x) GDAL_core_##x
#else
#define PLUGIN_SYMBOL_NAME(x) GDAL_driver_##x
#endif

#endif
