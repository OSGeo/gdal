/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of a sourced raster band that derives its raster
 *           by applying an algorithm (GDALDerivedPixelFunc) to the sources.
 * Author:   Pete Nagy
 *
 ******************************************************************************
 * Copyright (c) 2005 Vexcel Corp.
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "vrtdataset.h"
#include "cpl_multiproc.h"
#include "gdalpython.h"
#include "gdalantirecursion.h"

#include <algorithm>
#include <array>
#include <map>
#include <vector>
#include <utility>

/*! @cond Doxygen_Suppress */

using namespace GDALPy;

// #define GDAL_VRT_DISABLE_PYTHON

#ifndef GDAL_VRT_ENABLE_PYTHON_DEFAULT
// Can be YES, NO or TRUSTED_MODULES
#define GDAL_VRT_ENABLE_PYTHON_DEFAULT "TRUSTED_MODULES"
#endif

/* Flags for getting buffers */
#define PyBUF_WRITABLE 0x0001
#define PyBUF_FORMAT 0x0004
#define PyBUF_ND 0x0008
#define PyBUF_STRIDES (0x0010 | PyBUF_ND)
#define PyBUF_INDIRECT (0x0100 | PyBUF_STRIDES)
#define PyBUF_FULL (PyBUF_INDIRECT | PyBUF_WRITABLE | PyBUF_FORMAT)

/************************************************************************/
/*                        GDALCreateNumpyArray()                        */
/************************************************************************/

static PyObject *GDALCreateNumpyArray(PyObject *pCreateArray, void *pBuffer,
                                      GDALDataType eType, int nHeight,
                                      int nWidth)
{
    PyObject *poPyBuffer;
    const size_t nSize =
        static_cast<size_t>(nHeight) * nWidth * GDALGetDataTypeSizeBytes(eType);
    Py_buffer pybuffer;
    if (PyBuffer_FillInfo(&pybuffer, nullptr, static_cast<char *>(pBuffer),
                          nSize, 0, PyBUF_FULL) != 0)
    {
        return nullptr;
    }
    poPyBuffer = PyMemoryView_FromBuffer(&pybuffer);
    PyObject *pArgsCreateArray = PyTuple_New(4);
    PyTuple_SetItem(pArgsCreateArray, 0, poPyBuffer);
    const char *pszDataType = nullptr;
    switch (eType)
    {
        case GDT_UInt8:
            pszDataType = "uint8";
            break;
        case GDT_Int8:
            pszDataType = "int8";
            break;
        case GDT_UInt16:
            pszDataType = "uint16";
            break;
        case GDT_Int16:
            pszDataType = "int16";
            break;
        case GDT_UInt32:
            pszDataType = "uint32";
            break;
        case GDT_Int32:
            pszDataType = "int32";
            break;
        case GDT_Int64:
            pszDataType = "int64";
            break;
        case GDT_UInt64:
            pszDataType = "uint64";
            break;
        case GDT_Float16:
            pszDataType = "float16";
            break;
        case GDT_Float32:
            pszDataType = "float32";
            break;
        case GDT_Float64:
            pszDataType = "float64";
            break;
        case GDT_CInt16:
        case GDT_CInt32:
            CPLAssert(FALSE);
            break;
        case GDT_CFloat16:
            CPLAssert(FALSE);
            break;
        case GDT_CFloat32:
            pszDataType = "complex64";
            break;
        case GDT_CFloat64:
            pszDataType = "complex128";
            break;
        case GDT_Unknown:
        case GDT_TypeCount:
            CPLAssert(FALSE);
            break;
    }
    PyTuple_SetItem(
        pArgsCreateArray, 1,
        PyBytes_FromStringAndSize(pszDataType, strlen(pszDataType)));
    PyTuple_SetItem(pArgsCreateArray, 2, PyLong_FromLong(nHeight));
    PyTuple_SetItem(pArgsCreateArray, 3, PyLong_FromLong(nWidth));
    PyObject *poNumpyArray =
        PyObject_Call(pCreateArray, pArgsCreateArray, nullptr);
    Py_DecRef(pArgsCreateArray);
    if (PyErr_Occurred())
        PyErr_Print();
    return poNumpyArray;
}

/************************************************************************/
/* ==================================================================== */
/*                     VRTDerivedRasterBandPrivateData                  */
/* ==================================================================== */
/************************************************************************/

class VRTDerivedRasterBandPrivateData
{
    VRTDerivedRasterBandPrivateData(const VRTDerivedRasterBandPrivateData &) =
        delete;
    VRTDerivedRasterBandPrivateData &
    operator=(const VRTDerivedRasterBandPrivateData &) = delete;

  public:
    CPLString m_osCode{};
    CPLString m_osLanguage = "C";
    int m_nBufferRadius = 0;
    PyObject *m_poGDALCreateNumpyArray = nullptr;
    PyObject *m_poUserFunction = nullptr;
    bool m_bPythonInitializationDone = false;
    bool m_bPythonInitializationSuccess = false;
    bool m_bExclusiveLock = false;
    bool m_bFirstTime = true;
    std::vector<std::pair<CPLString, CPLString>> m_oFunctionArgs{};
    bool m_bSkipNonContributingSourcesSpecified = false;
    bool m_bSkipNonContributingSources = false;
    GIntBig m_nAllowedRAMUsage = 0;

    VRTDerivedRasterBandPrivateData()
        : m_nAllowedRAMUsage(CPLGetUsablePhysicalRAM() / 10 * 4)
    {
        // Use only up to 40% of RAM to acquire source bands and generate the
        // output buffer.
        // Only for tests now
        const char *pszMAX_RAM = "VRT_DERIVED_DATASET_ALLOWED_RAM_USAGE";
        if (const char *pszVal = CPLGetConfigOption(pszMAX_RAM, nullptr))
        {
            CPL_IGNORE_RET_VAL(
                CPLParseMemorySize(pszVal, &m_nAllowedRAMUsage, nullptr));
        }
    }

    ~VRTDerivedRasterBandPrivateData();
};

VRTDerivedRasterBandPrivateData::~VRTDerivedRasterBandPrivateData()
{
    if (m_poGDALCreateNumpyArray)
        Py_DecRef(m_poGDALCreateNumpyArray);
    if (m_poUserFunction)
        Py_DecRef(m_poUserFunction);
}

/************************************************************************/
/* ==================================================================== */
/*                          VRTDerivedRasterBand                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VRTDerivedRasterBand()                        */
/************************************************************************/

VRTDerivedRasterBand::VRTDerivedRasterBand(GDALDataset *poDSIn, int nBandIn)
    : VRTSourcedRasterBand(poDSIn, nBandIn), m_poPrivate(nullptr),
      eSourceTransferType(GDT_Unknown)
{
    m_poPrivate = new VRTDerivedRasterBandPrivateData;
}

/************************************************************************/
/*                        VRTDerivedRasterBand()                        */
/************************************************************************/

VRTDerivedRasterBand::VRTDerivedRasterBand(GDALDataset *poDSIn, int nBandIn,
                                           GDALDataType eType, int nXSize,
                                           int nYSize, int nBlockXSizeIn,
                                           int nBlockYSizeIn)
    : VRTSourcedRasterBand(poDSIn, nBandIn, eType, nXSize, nYSize,
                           nBlockXSizeIn, nBlockYSizeIn),
      m_poPrivate(nullptr), eSourceTransferType(GDT_Unknown)
{
    m_poPrivate = new VRTDerivedRasterBandPrivateData;
}

/************************************************************************/
/*                       ~VRTDerivedRasterBand()                        */
/************************************************************************/

VRTDerivedRasterBand::~VRTDerivedRasterBand()

{
    delete m_poPrivate;
}

/************************************************************************/
/*                              Cleanup()                               */
/************************************************************************/

void VRTDerivedRasterBand::Cleanup()
{
}

/************************************************************************/
/*                     GetGlobalMapPixelFunction()                      */
/************************************************************************/

static std::map<std::string,
                std::pair<VRTDerivedRasterBand::PixelFunc, std::string>> &
GetGlobalMapPixelFunction()
{
    static std::map<std::string,
                    std::pair<VRTDerivedRasterBand::PixelFunc, std::string>>
        gosMapPixelFunction;
    return gosMapPixelFunction;
}

/************************************************************************/
/*                          AddPixelFunction()                          */
/************************************************************************/

/*! @endcond */

/**
 * This adds a pixel function to the global list of available pixel
 * functions for derived bands.  Pixel functions must be registered
 * in this way before a derived band tries to access data.
 *
 * Derived bands are stored with only the name of the pixel function
 * that it will apply, and if a pixel function matching the name is not
 * found the IRasterIO() call will do nothing.
 *
 * @param pszName Name used to access pixel function
 * @param pfnNewFunction Pixel function associated with name.  An
 *  existing pixel function registered with the same name will be
 *  replaced with the new one.
 *
 * @return CE_None, invalid (NULL) parameters are currently ignored.
 */
CPLErr CPL_STDCALL GDALAddDerivedBandPixelFunc(
    const char *pszName, GDALDerivedPixelFunc pfnNewFunction)
{
    if (pszName == nullptr || pszName[0] == '\0' || pfnNewFunction == nullptr)
    {
        return CE_None;
    }

    GetGlobalMapPixelFunction()[pszName] = {
        [pfnNewFunction](void **papoSources, int nSources, void *pData,
                         int nBufXSize, int nBufYSize, GDALDataType eSrcType,
                         GDALDataType eBufType, int nPixelSpace, int nLineSpace,
                         CSLConstList papszFunctionArgs)
        {
            (void)papszFunctionArgs;
            return pfnNewFunction(papoSources, nSources, pData, nBufXSize,
                                  nBufYSize, eSrcType, eBufType, nPixelSpace,
                                  nLineSpace);
        },
        ""};

    return CE_None;
}

/**
 * This adds a pixel function to the global list of available pixel
 * functions for derived bands.  Pixel functions must be registered
 * in this way before a derived band tries to access data.
 *
 * Derived bands are stored with only the name of the pixel function
 * that it will apply, and if a pixel function matching the name is not
 * found the IRasterIO() call will do nothing.
 *
 * @param pszName Name used to access pixel function
 * @param pfnNewFunction Pixel function associated with name.  An
 *  existing pixel function registered with the same name will be
 *  replaced with the new one.
 * @param pszMetadata Pixel function metadata (not currently implemented)
 *
 * @return CE_None, invalid (NULL) parameters are currently ignored.
 * @since GDAL 3.4
 */
CPLErr CPL_STDCALL GDALAddDerivedBandPixelFuncWithArgs(
    const char *pszName, GDALDerivedPixelFuncWithArgs pfnNewFunction,
    const char *pszMetadata)
{
    if (!pszName || pszName[0] == '\0' || !pfnNewFunction)
    {
        return CE_None;
    }

    GetGlobalMapPixelFunction()[pszName] = {pfnNewFunction,
                                            pszMetadata ? pszMetadata : ""};

    return CE_None;
}

/*! @cond Doxygen_Suppress */

/**
 * This adds a pixel function to the global list of available pixel
 * functions for derived bands.
 *
 * This is the same as the C function GDALAddDerivedBandPixelFunc()
 *
 * @param pszFuncNameIn Name used to access pixel function
 * @param pfnNewFunction Pixel function associated with name.  An
 *  existing pixel function registered with the same name will be
 *  replaced with the new one.
 *
 * @return CE_None, invalid (NULL) parameters are currently ignored.
 */
CPLErr
VRTDerivedRasterBand::AddPixelFunction(const char *pszFuncNameIn,
                                       GDALDerivedPixelFunc pfnNewFunction)
{
    return GDALAddDerivedBandPixelFunc(pszFuncNameIn, pfnNewFunction);
}

CPLErr VRTDerivedRasterBand::AddPixelFunction(
    const char *pszFuncNameIn, GDALDerivedPixelFuncWithArgs pfnNewFunction,
    const char *pszMetadata)
{
    return GDALAddDerivedBandPixelFuncWithArgs(pszFuncNameIn, pfnNewFunction,
                                               pszMetadata);
}

/************************************************************************/
/*                          GetPixelFunction()                          */
/************************************************************************/

/**
 * Get a pixel function previously registered using the global
 * AddPixelFunction.
 *
 * @param pszFuncNameIn The name associated with the pixel function.
 *
 * @return A pointer to a std::pair whose first element is the pixel
 *         function pointer and second element is the pixel function
 *         metadata string. If no pixel function has been registered
 *         for pszFuncNameIn, nullptr will be returned.
 */
/* static */
const std::pair<VRTDerivedRasterBand::PixelFunc, std::string> *
VRTDerivedRasterBand::GetPixelFunction(const char *pszFuncNameIn)
{
    if (pszFuncNameIn == nullptr || pszFuncNameIn[0] == '\0')
    {
        return nullptr;
    }

    const auto &oMapPixelFunction = GetGlobalMapPixelFunction();
    const auto oIter = oMapPixelFunction.find(pszFuncNameIn);

    if (oIter == oMapPixelFunction.end())
        return nullptr;

    return &(oIter->second);
}

/************************************************************************/
/*                       GetPixelFunctionNames()                        */
/************************************************************************/

/**
 * Return the list of available pixel function names.
 */
/* static */
std::vector<std::string> VRTDerivedRasterBand::GetPixelFunctionNames()
{
    std::vector<std::string> res;
    for (const auto &iter : GetGlobalMapPixelFunction())
    {
        res.push_back(iter.first);
    }
    return res;
}

/************************************************************************/
/*                        SetPixelFunctionName()                        */
/************************************************************************/

/**
 * Set the pixel function name to be applied to this derived band.  The
 * name should match a pixel function registered using AddPixelFunction.
 *
 * @param pszFuncNameIn Name of pixel function to be applied to this derived
 * band.
 */
void VRTDerivedRasterBand::SetPixelFunctionName(const char *pszFuncNameIn)
{
    osFuncName = (pszFuncNameIn == nullptr) ? "" : pszFuncNameIn;
}

/************************************************************************/
/*                      AddPixelFunctionArgument()                      */
/************************************************************************/

/**
 *  Set a pixel function argument to a specified value.
 * @param pszArg the argument name
 * @param pszValue the argument value
 *
 * @since 3.12
 */
void VRTDerivedRasterBand::AddPixelFunctionArgument(const char *pszArg,
                                                    const char *pszValue)
{
    m_poPrivate->m_oFunctionArgs.emplace_back(pszArg, pszValue);
}

/************************************************************************/
/*                      SetPixelFunctionLanguage()                      */
/************************************************************************/

/**
 * Set the language of the pixel function.
 *
 * @param pszLanguage Language of the pixel function (only "C" and "Python"
 * are supported currently)
 * @since GDAL 2.3
 */
void VRTDerivedRasterBand::SetPixelFunctionLanguage(const char *pszLanguage)
{
    m_poPrivate->m_osLanguage = pszLanguage;
}

/************************************************************************/
/*                   SetSkipNonContributingSources()                    */
/************************************************************************/

/** Whether sources that do not intersect the VRTRasterBand RasterIO() requested
 * region should be omitted. By default, data for all sources, including ones
 * that do not intersect it, are passed to the pixel function. By setting this
 * parameter to true, only sources that intersect the requested region will be
 * passed.
 *
 * @param bSkip whether to skip non-contributing sources
 *
 * @since 3.12
 */
void VRTDerivedRasterBand::SetSkipNonContributingSources(bool bSkip)
{
    m_poPrivate->m_bSkipNonContributingSources = bSkip;
    m_poPrivate->m_bSkipNonContributingSourcesSpecified = true;
}

/************************************************************************/
/*                       SetSourceTransferType()                        */
/************************************************************************/

/**
 * Set the transfer type to be used to obtain pixel information from
 * all of the sources.  If unset, the transfer type used will be the
 * same as the derived band data type.  This makes it possible, for
 * example, to pass CFloat32 source pixels to the pixel function, even
 * if the pixel function generates a raster for a derived band that
 * is of type Byte.
 *
 * @param eDataTypeIn Data type to use to obtain pixel information from
 * the sources to be passed to the derived band pixel function.
 */
void VRTDerivedRasterBand::SetSourceTransferType(GDALDataType eDataTypeIn)
{
    eSourceTransferType = eDataTypeIn;
}

/************************************************************************/
/*                          InitializePython()                          */
/************************************************************************/

bool VRTDerivedRasterBand::InitializePython()
{
    if (m_poPrivate->m_bPythonInitializationDone)
        return m_poPrivate->m_bPythonInitializationSuccess;

    m_poPrivate->m_bPythonInitializationDone = true;
    m_poPrivate->m_bPythonInitializationSuccess = false;

    const size_t nIdxDot = osFuncName.rfind(".");
    CPLString osPythonModule;
    CPLString osPythonFunction;
    if (nIdxDot != std::string::npos)
    {
        osPythonModule = osFuncName.substr(0, nIdxDot);
        osPythonFunction = osFuncName.substr(nIdxDot + 1);
    }
    else
    {
        osPythonFunction = osFuncName;
    }

#ifndef GDAL_VRT_DISABLE_PYTHON
    const char *pszPythonEnabled =
        CPLGetConfigOption("GDAL_VRT_ENABLE_PYTHON", nullptr);
#else
    const char *pszPythonEnabled = "NO";
#endif
    const CPLString osPythonEnabled(
        pszPythonEnabled ? pszPythonEnabled : GDAL_VRT_ENABLE_PYTHON_DEFAULT);

    if (EQUAL(osPythonEnabled, "TRUSTED_MODULES"))
    {
        bool bIsTrustedModule = false;
        const CPLString osVRTTrustedModules(
            CPLGetConfigOption("GDAL_VRT_PYTHON_TRUSTED_MODULES", ""));
        if (!osPythonModule.empty())
        {
            char **papszTrustedModules =
                CSLTokenizeString2(osVRTTrustedModules, ",", 0);
            for (char **papszIter = papszTrustedModules;
                 !bIsTrustedModule && papszIter && *papszIter; ++papszIter)
            {
                const char *pszIterModule = *papszIter;
                size_t nIterModuleLen = strlen(pszIterModule);
                if (nIterModuleLen > 2 &&
                    strncmp(pszIterModule + nIterModuleLen - 2, ".*", 2) == 0)
                {
                    bIsTrustedModule =
                        (strncmp(osPythonModule, pszIterModule,
                                 nIterModuleLen - 2) == 0) &&
                        (osPythonModule.size() == nIterModuleLen - 2 ||
                         (osPythonModule.size() >= nIterModuleLen &&
                          osPythonModule[nIterModuleLen - 1] == '.'));
                }
                else if (nIterModuleLen >= 1 &&
                         pszIterModule[nIterModuleLen - 1] == '*')
                {
                    bIsTrustedModule = (strncmp(osPythonModule, pszIterModule,
                                                nIterModuleLen - 1) == 0);
                }
                else
                {
                    bIsTrustedModule =
                        (strcmp(osPythonModule, pszIterModule) == 0);
                }
            }
            CSLDestroy(papszTrustedModules);
        }

        if (!bIsTrustedModule)
        {
            if (osPythonModule.empty())
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Python code needs to be executed, but it uses inline code "
                    "in the VRT whereas the current policy is to trust only "
                    "code from external trusted modules (defined in the "
                    "GDAL_VRT_PYTHON_TRUSTED_MODULES configuration option). "
                    "If you trust the code in %s, you can set the "
                    "GDAL_VRT_ENABLE_PYTHON configuration option to YES.",
                    GetDataset() ? GetDataset()->GetDescription()
                                 : "(unknown VRT)");
            }
            else if (osVRTTrustedModules.empty())
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Python code needs to be executed, but it uses code "
                    "from module '%s', whereas the current policy is to "
                    "trust only code from modules defined in the "
                    "GDAL_VRT_PYTHON_TRUSTED_MODULES configuration option, "
                    "which is currently unset. "
                    "If you trust the code in '%s', you can add module '%s' "
                    "to GDAL_VRT_PYTHON_TRUSTED_MODULES (or set the "
                    "GDAL_VRT_ENABLE_PYTHON configuration option to YES).",
                    osPythonModule.c_str(),
                    GetDataset() ? GetDataset()->GetDescription()
                                 : "(unknown VRT)",
                    osPythonModule.c_str());
            }
            else
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Python code needs to be executed, but it uses code "
                    "from module '%s', whereas the current policy is to "
                    "trust only code from modules '%s' (defined in the "
                    "GDAL_VRT_PYTHON_TRUSTED_MODULES configuration option). "
                    "If you trust the code in '%s', you can add module '%s' "
                    "to GDAL_VRT_PYTHON_TRUSTED_MODULES (or set the "
                    "GDAL_VRT_ENABLE_PYTHON configuration option to YES).",
                    osPythonModule.c_str(), osVRTTrustedModules.c_str(),
                    GetDataset() ? GetDataset()->GetDescription()
                                 : "(unknown VRT)",
                    osPythonModule.c_str());
            }
            return false;
        }
    }

#ifdef disabled_because_this_is_probably_broken_by_design
    // See https://lwn.net/Articles/574215/
    // and http://nedbatchelder.com/blog/201206/eval_really_is_dangerous.html
    else if (EQUAL(osPythonEnabled, "IF_SAFE"))
    {
        bool bSafe = true;
        // If the function comes from another module, then we don't know
        if (!osPythonModule.empty())
        {
            CPLDebug("VRT", "Python function is from another module");
            bSafe = false;
        }

        CPLString osCode(m_poPrivate->m_osCode);

        // Reject all imports except a few trusted modules
        const char *const apszTrustedImports[] = {
            "import math",
            "from math import",
            "import numpy",  // caution: numpy has lots of I/O functions !
            "from numpy import",
            // TODO: not sure if importing arbitrary stuff from numba is OK
            // so let's just restrict to jit.
            "from numba import jit",

            // Not imports but still whitelisted, whereas other __ is banned
            "__init__",
            "__call__",
        };
        for (size_t i = 0; i < CPL_ARRAYSIZE(apszTrustedImports); ++i)
        {
            osCode.replaceAll(CPLString(apszTrustedImports[i]), "");
        }

        // Some dangerous built-in functions or numpy functions
        const char *const apszUntrusted[] = {
            "import",  // and __import__
            "eval",       "compile", "open",
            "load",        // reload, numpy.load
            "file",        // and exec_file, numpy.fromfile, numpy.tofile
            "input",       // and raw_input
            "save",        // numpy.save
            "memmap",      // numpy.memmap
            "DataSource",  // numpy.DataSource
            "genfromtxt",  // numpy.genfromtxt
            "getattr",
            "ctypeslib",  // numpy.ctypeslib
            "testing",    // numpy.testing
            "dump",       // numpy.ndarray.dump
            "fromregex",  // numpy.fromregex
            "__"};
        for (size_t i = 0; i < CPL_ARRAYSIZE(apszUntrusted); ++i)
        {
            if (osCode.find(apszUntrusted[i]) != std::string::npos)
            {
                CPLDebug("VRT", "Found '%s' word in Python code",
                         apszUntrusted[i]);
                bSafe = false;
            }
        }

        if (!bSafe)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Python code needs to be executed, but we cannot verify "
                     "if it is safe, so this is disabled by default. "
                     "If you trust the code in %s, you can set the "
                     "GDAL_VRT_ENABLE_PYTHON configuration option to YES.",
                     GetDataset() ? GetDataset()->GetDescription()
                                  : "(unknown VRT)");
            return false;
        }
    }
#endif  // disabled_because_this_is_probably_broken_by_design

    else if (!EQUAL(osPythonEnabled, "YES") && !EQUAL(osPythonEnabled, "ON") &&
             !EQUAL(osPythonEnabled, "TRUE"))
    {
        if (pszPythonEnabled == nullptr)
        {
            // Note: this is dead code with our current default policy
            // GDAL_VRT_ENABLE_PYTHON == "TRUSTED_MODULES"
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Python code needs to be executed, but this is "
                     "disabled by default. If you trust the code in %s, "
                     "you can set the GDAL_VRT_ENABLE_PYTHON configuration "
                     "option to YES.",
                     GetDataset() ? GetDataset()->GetDescription()
                                  : "(unknown VRT)");
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Python code in %s needs to be executed, but this has been "
                "explicitly disabled.",
                GetDataset() ? GetDataset()->GetDescription()
                             : "(unknown VRT)");
        }
        return false;
    }

    if (!GDALPythonInitialize())
        return false;

    // Whether we should just use our own global mutex, in addition to Python
    // GIL locking.
    m_poPrivate->m_bExclusiveLock =
        CPLTestBool(CPLGetConfigOption("GDAL_VRT_PYTHON_EXCLUSIVE_LOCK", "NO"));

    // numba jit'ification doesn't seem to be thread-safe, so force use of
    // lock now and at first execution of function. Later executions seem to
    // be thread-safe. This problem doesn't seem to appear for code in
    // regular files
    const bool bUseExclusiveLock =
        m_poPrivate->m_bExclusiveLock ||
        m_poPrivate->m_osCode.find("@jit") != std::string::npos;
    GIL_Holder oHolder(bUseExclusiveLock);

    // As we don't want to depend on numpy C API/ABI, we use a trick to build
    // a numpy array object. We define a Python function to which we pass a
    // Python buffer object.

    // We need to build a unique module name, otherwise this will crash in
    // multithreaded use cases.
    CPLString osModuleName(CPLSPrintf("gdal_vrt_module_%p", this));
    PyObject *poCompiledString = Py_CompileString(
        ("import numpy\n"
         "def GDALCreateNumpyArray(buffer, dtype, height, width):\n"
         "    return numpy.frombuffer(buffer, str(dtype.decode('ascii')))."
         "reshape([height, width])\n"
         "\n" +
         m_poPrivate->m_osCode)
            .c_str(),
        osModuleName, Py_file_input);
    if (poCompiledString == nullptr || PyErr_Occurred())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Couldn't compile code:\n%s",
                 GetPyExceptionString().c_str());
        return false;
    }
    PyObject *poModule =
        PyImport_ExecCodeModule(osModuleName, poCompiledString);
    Py_DecRef(poCompiledString);

    if (poModule == nullptr || PyErr_Occurred())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 GetPyExceptionString().c_str());
        return false;
    }

    // Fetch user computation function
    if (!osPythonModule.empty())
    {
        PyObject *poUserModule = PyImport_ImportModule(osPythonModule);
        if (poUserModule == nullptr || PyErr_Occurred())
        {
            CPLString osException = GetPyExceptionString();
            if (!osException.empty() && osException.back() == '\n')
            {
                osException.pop_back();
            }
            if (osException.find("ModuleNotFoundError") == 0)
            {
                osException += ". You may need to define PYTHONPATH";
            }
            CPLError(CE_Failure, CPLE_AppDefined, "%s", osException.c_str());
            Py_DecRef(poModule);
            return false;
        }
        m_poPrivate->m_poUserFunction =
            PyObject_GetAttrString(poUserModule, osPythonFunction);
        Py_DecRef(poUserModule);
    }
    else
    {
        m_poPrivate->m_poUserFunction =
            PyObject_GetAttrString(poModule, osPythonFunction);
    }
    if (m_poPrivate->m_poUserFunction == nullptr || PyErr_Occurred())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 GetPyExceptionString().c_str());
        Py_DecRef(poModule);
        return false;
    }
    if (!PyCallable_Check(m_poPrivate->m_poUserFunction))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Object '%s' is not callable",
                 osPythonFunction.c_str());
        Py_DecRef(poModule);
        return false;
    }

    // Fetch our GDALCreateNumpyArray python function
    m_poPrivate->m_poGDALCreateNumpyArray =
        PyObject_GetAttrString(poModule, "GDALCreateNumpyArray");
    if (m_poPrivate->m_poGDALCreateNumpyArray == nullptr || PyErr_Occurred())
    {
        // Shouldn't happen normally...
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 GetPyExceptionString().c_str());
        Py_DecRef(poModule);
        return false;
    }
    Py_DecRef(poModule);

    m_poPrivate->m_bPythonInitializationSuccess = true;
    return true;
}

CPLErr VRTDerivedRasterBand::GetPixelFunctionArguments(
    const CPLString &osMetadata,
    const std::vector<int> &anMapBufferIdxToSourceIdx, int nXOff, int nYOff,
    std::vector<std::pair<CPLString, CPLString>> &oAdditionalArgs)
{

    auto poArgs = CPLXMLTreeCloser(CPLParseXMLString(osMetadata));
    if (poArgs != nullptr && poArgs->eType == CXT_Element &&
        !strcmp(poArgs->pszValue, "PixelFunctionArgumentsList"))
    {
        for (CPLXMLNode *psIter = poArgs->psChild; psIter != nullptr;
             psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                !strcmp(psIter->pszValue, "Argument"))
            {
                CPLString osName, osType, osValue;
                auto pszName = CPLGetXMLValue(psIter, "name", nullptr);
                if (pszName != nullptr)
                    osName = pszName;
                auto pszType = CPLGetXMLValue(psIter, "type", nullptr);
                if (pszType != nullptr)
                    osType = pszType;
                auto pszValue = CPLGetXMLValue(psIter, "value", nullptr);
                if (pszValue != nullptr)
                    osValue = pszValue;
                if (osType == "constant" && osValue != "" && osName != "")
                    oAdditionalArgs.push_back(
                        std::pair<CPLString, CPLString>(osName, osValue));
                if (osType == "builtin")
                {
                    const CPLString &osArgName = osValue;
                    CPLString osVal;
                    double dfVal = 0;

                    int success(FALSE);
                    if (osArgName == "NoData")
                        dfVal = this->GetNoDataValue(&success);
                    else if (osArgName == "scale")
                        dfVal = this->GetScale(&success);
                    else if (osArgName == "offset")
                        dfVal = this->GetOffset(&success);
                    else if (osArgName == "xoff")
                    {
                        dfVal = static_cast<double>(nXOff);
                        success = true;
                    }
                    else if (osArgName == "yoff")
                    {
                        dfVal = static_cast<double>(nYOff);
                        success = true;
                    }
                    else if (osArgName == "crs")
                    {
                        const auto *crs =
                            GetDataset()->GetSpatialRefRasterOnly();
                        if (crs)
                        {
                            osVal =
                                std::to_string(reinterpret_cast<size_t>(crs));
                            success = true;
                        }
                        else
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "VRTDataset has no <SRS>");
                        }
                    }
                    else if (osArgName == "geotransform")
                    {
                        GDALGeoTransform gt;
                        if (GetDataset()->GetGeoTransform(gt) != CE_None)
                        {
                            // Do not fail here because the argument is most
                            // likely not needed by the pixel function. If it
                            // is needed, the pixel function can emit the error.
                            continue;
                        }
                        osVal = gt.ToString();
                        success = true;
                    }
                    else if (osArgName == "source_names")
                    {
                        for (size_t iBuffer = 0;
                             iBuffer < anMapBufferIdxToSourceIdx.size();
                             iBuffer++)
                        {
                            const int iSource =
                                anMapBufferIdxToSourceIdx[iBuffer];
                            const VRTSource *poSource =
                                m_papoSources[iSource].get();

                            if (iBuffer > 0)
                            {
                                osVal += "|";
                            }

                            const auto &osSourceName = poSource->GetName();
                            if (osSourceName.empty())
                            {
                                osVal += "B" + std::to_string(iBuffer + 1);
                            }
                            else
                            {
                                osVal += osSourceName;
                            }
                        }

                        success = true;
                    }
                    else
                    {
                        CPLError(
                            CE_Failure, CPLE_NotSupported,
                            "PixelFunction builtin argument %s not supported",
                            osArgName.c_str());
                        return CE_Failure;
                    }
                    if (!success)
                    {
                        if (CPLTestBool(
                                CPLGetXMLValue(psIter, "optional", "false")))
                            continue;

                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Raster has no %s", osValue.c_str());
                        return CE_Failure;
                    }

                    if (osVal.empty())
                    {
                        osVal = CPLSPrintf("%.17g", dfVal);
                    }

                    oAdditionalArgs.push_back(
                        std::pair<CPLString, CPLString>(osArgName, osVal));
                    CPLDebug("VRT",
                             "Added builtin pixel function argument %s = %s",
                             osArgName.c_str(), osVal.c_str());
                }
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                   CreateMapBufferIdxToSourceIdx()                    */
/************************************************************************/

static bool CreateMapBufferIdxToSourceIdx(
    const std::vector<std::unique_ptr<VRTSource>> &apoSources,
    bool bSkipNonContributingSources, int nXOff, int nYOff, int nXSize,
    int nYSize, int nBufXSize, int nBufYSize, GDALRasterIOExtraArg *psExtraArg,
    bool &bCreateMapBufferIdxToSourceIdxHasRun,
    std::vector<int> &anMapBufferIdxToSourceIdx,
    bool &bSkipOutputBufferInitialization)
{
    CPLAssert(!bCreateMapBufferIdxToSourceIdxHasRun);
    bCreateMapBufferIdxToSourceIdxHasRun = true;
    anMapBufferIdxToSourceIdx.reserve(apoSources.size());
    for (int iSource = 0; iSource < static_cast<int>(apoSources.size());
         iSource++)
    {
        if (bSkipNonContributingSources &&
            apoSources[iSource]->IsSimpleSource())
        {
            bool bError = false;
            double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
            int nReqXOff, nReqYOff, nReqXSize, nReqYSize;
            int nOutXOff, nOutYOff, nOutXSize, nOutYSize;
            auto poSource =
                static_cast<VRTSimpleSource *>(apoSources[iSource].get());
            if (!poSource->GetSrcDstWindow(
                    nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                    psExtraArg->eResampleAlg, &dfReqXOff, &dfReqYOff,
                    &dfReqXSize, &dfReqYSize, &nReqXOff, &nReqYOff, &nReqXSize,
                    &nReqYSize, &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize,
                    bError))
            {
                if (bError)
                {
                    return false;
                }

                // Skip non contributing source
                bSkipOutputBufferInitialization = false;
                continue;
            }
        }

        anMapBufferIdxToSourceIdx.push_back(iSource);
    }
    return true;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

/**
 * Read/write a region of image data for this band.
 *
 * Each of the sources for this derived band will be read and passed to
 * the derived band pixel function.  The pixel function is responsible
 * for applying whatever algorithm is necessary to generate this band's
 * pixels from the sources.
 *
 * The sources will be read using the transfer type specified for sources
 * using SetSourceTransferType().  If no transfer type has been set for
 * this derived band, the band's data type will be used as the transfer type.
 *
 * @see gdalrasterband
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GT_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param pData The buffer into which the data should be read, or from which
 * it should be written.  This buffer must contain at least nBufXSize *
 * nBufYSize words of type eBufType.  It is organized in left to right,
 * top to bottom pixel order.  Spacing is controlled by the nPixelSpace,
 * and nLineSpace parameters.
 *
 * @param nBufXSize The width of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param nBufYSize The height of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param eBufType The type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */
CPLErr VRTDerivedRasterBand::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg)
{
    if (eRWFlag == GF_Write)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Writing through VRTSourcedRasterBand is not supported.");
        return CE_Failure;
    }

    const std::string osFctId("VRTDerivedRasterBand::IRasterIO");
    GDALAntiRecursionGuard oGuard(osFctId);
    if (oGuard.GetCallDepth() >= 32)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "VRTDerivedRasterBand::IRasterIO(): Recursion detected (case 1)");
        return CE_Failure;
    }

    GDALAntiRecursionGuard oGuard2(oGuard, poDS->GetDescription());
    // Allow multiple recursion depths on the same dataset in case the split strategy is applied
    if (oGuard2.GetCallDepth() > 15)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "VRTDerivedRasterBand::IRasterIO(): Recursion detected (case 2)");
        return CE_Failure;
    }

    if constexpr (sizeof(GSpacing) > sizeof(int))
    {
        if (nLineSpace > INT_MAX)
        {
            if (nBufYSize == 1)
            {
                nLineSpace = 0;
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "VRTDerivedRasterBand::IRasterIO(): nLineSpace > "
                         "INT_MAX not supported");
                return CE_Failure;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have overviews that would be appropriate to satisfy       */
    /*      this request?                                                   */
    /* -------------------------------------------------------------------- */
    auto l_poDS = dynamic_cast<VRTDataset *>(poDS);
    if (l_poDS &&
        l_poDS->m_apoOverviews.empty() &&  // do not use virtual overviews
        (nBufXSize < nXSize || nBufYSize < nYSize) && GetOverviewCount() > 0)
    {
        if (OverviewRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                             nBufXSize, nBufYSize, eBufType, nPixelSpace,
                             nLineSpace, psExtraArg) == CE_None)
            return CE_None;
    }

    const int nBufTypeSize = GDALGetDataTypeSizeBytes(eBufType);
    GDALDataType eSrcType = eSourceTransferType;
    if (eSrcType == GDT_Unknown || eSrcType >= GDT_TypeCount)
    {
        // Check the largest data type for all sources
        GDALDataType eAllSrcType = GDT_Unknown;
        for (auto &poSource : m_papoSources)
        {
            if (poSource->IsSimpleSource())
            {
                const auto poSS =
                    static_cast<VRTSimpleSource *>(poSource.get());
                auto l_poBand = poSS->GetRasterBand();
                if (l_poBand)
                {
                    eAllSrcType = GDALDataTypeUnion(
                        eAllSrcType, l_poBand->GetRasterDataType());
                }
                else
                {
                    eAllSrcType = GDT_Unknown;
                    break;
                }
            }
            else
            {
                eAllSrcType = GDT_Unknown;
                break;
            }
        }

        if (eAllSrcType != GDT_Unknown)
            eSrcType = GDALDataTypeUnion(eAllSrcType, eDataType);
        else
            eSrcType = GDALDataTypeUnion(GDT_Float64, eDataType);
    }
    const int nSrcTypeSize = GDALGetDataTypeSizeBytes(eSrcType);

    std::vector<int> anMapBufferIdxToSourceIdx;
    bool bSkipOutputBufferInitialization = !m_papoSources.empty();
    bool bCreateMapBufferIdxToSourceIdxHasRun = false;

    // If acquiring the region of interest in a single time is going
    // to consume too much RAM, split in halves, and that recursively
    // until we get below m_nAllowedRAMUsage.
    if (m_poPrivate->m_nAllowedRAMUsage > 0 && !m_papoSources.empty() &&
        nSrcTypeSize > 0 && nBufXSize == nXSize && nBufYSize == nYSize &&
        static_cast<GIntBig>(nBufXSize) * nBufYSize >
            m_poPrivate->m_nAllowedRAMUsage /
                (static_cast<int>(m_papoSources.size()) * nSrcTypeSize))
    {
        bool bSplit = true;
        if (m_poPrivate->m_bSkipNonContributingSources)
        {
            // More accurate check by comparing against the number of
            // actually contributing sources.
            if (!CreateMapBufferIdxToSourceIdx(
                    m_papoSources, m_poPrivate->m_bSkipNonContributingSources,
                    nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                    psExtraArg, bCreateMapBufferIdxToSourceIdxHasRun,
                    anMapBufferIdxToSourceIdx, bSkipOutputBufferInitialization))
            {
                return CE_Failure;
            }
            bSplit =
                !anMapBufferIdxToSourceIdx.empty() &&
                static_cast<GIntBig>(nBufXSize) * nBufYSize >
                    m_poPrivate->m_nAllowedRAMUsage /
                        (static_cast<int>(anMapBufferIdxToSourceIdx.size()) *
                         nSrcTypeSize);
        }
        if (bSplit)
        {
            CPLErr eErr = SplitRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nPixelSpace, nLineSpace, psExtraArg);
            if (eErr != CE_Warning)
                return eErr;
        }
    }

    /* ---- Get pixel function for band ---- */
    const std::pair<PixelFunc, std::string> *poPixelFunc = nullptr;
    std::vector<std::pair<CPLString, CPLString>> oAdditionalArgs;

    if (EQUAL(m_poPrivate->m_osLanguage, "C"))
    {
        poPixelFunc =
            VRTDerivedRasterBand::GetPixelFunction(osFuncName.c_str());
        if (poPixelFunc == nullptr)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "VRTDerivedRasterBand::IRasterIO:"
                     "Derived band pixel function '%s' not registered.",
                     osFuncName.c_str());
            return CE_Failure;
        }
    }

    /* TODO: It would be nice to use a MallocBlock function for each
       individual buffer that would recycle blocks of memory from a
       cache by reassigning blocks that are nearly the same size.
       A corresponding FreeBlock might only truly free if the total size
       of freed blocks gets to be too great of a percentage of the size
       of the allocated blocks. */

    // Get buffers for each source.
    const int nBufferRadius = m_poPrivate->m_nBufferRadius;
    if (nBufferRadius > (INT_MAX - nBufXSize) / 2 ||
        nBufferRadius > (INT_MAX - nBufYSize) / 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Integer overflow: "
                 "nBufferRadius > (INT_MAX - nBufXSize) / 2 || "
                 "nBufferRadius > (INT_MAX - nBufYSize) / 2)");
        return CE_Failure;
    }
    const int nExtBufXSize = nBufXSize + 2 * nBufferRadius;
    const int nExtBufYSize = nBufYSize + 2 * nBufferRadius;

    if (!bCreateMapBufferIdxToSourceIdxHasRun)
    {
        if (!CreateMapBufferIdxToSourceIdx(
                m_papoSources, m_poPrivate->m_bSkipNonContributingSources,
                nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, psExtraArg,
                bCreateMapBufferIdxToSourceIdxHasRun, anMapBufferIdxToSourceIdx,
                bSkipOutputBufferInitialization))
        {
            return CE_Failure;
        }
    }
    std::vector<std::unique_ptr<void, VSIFreeReleaser>> apBuffers(
        anMapBufferIdxToSourceIdx.size());
    for (size_t iBuffer = 0; iBuffer < anMapBufferIdxToSourceIdx.size();
         ++iBuffer)
    {
        apBuffers[iBuffer].reset(
            VSI_MALLOC3_VERBOSE(nSrcTypeSize, nExtBufXSize, nExtBufYSize));
        if (apBuffers[iBuffer] == nullptr)
        {
            return CE_Failure;
        }

        bool bBufferInit = true;
        const int iSource = anMapBufferIdxToSourceIdx[iBuffer];
        if (m_papoSources[iSource]->IsSimpleSource())
        {
            const auto poSS =
                static_cast<VRTSimpleSource *>(m_papoSources[iSource].get());
            auto l_poBand = poSS->GetRasterBand();
            if (l_poBand != nullptr && poSS->m_dfSrcXOff == 0.0 &&
                poSS->m_dfSrcYOff == 0.0 &&
                poSS->m_dfSrcXOff + poSS->m_dfSrcXSize ==
                    l_poBand->GetXSize() &&
                poSS->m_dfSrcYOff + poSS->m_dfSrcYSize ==
                    l_poBand->GetYSize() &&
                poSS->m_dfDstXOff == 0.0 && poSS->m_dfDstYOff == 0.0 &&
                poSS->m_dfDstXOff + poSS->m_dfDstXSize == nRasterXSize &&
                poSS->m_dfDstYOff + poSS->m_dfDstYSize == nRasterYSize)
            {
                if (m_papoSources[iSource]->GetType() ==
                    VRTSimpleSource::GetTypeStatic())
                    bBufferInit = false;
            }
            else
            {
                bSkipOutputBufferInitialization = false;
            }
        }
        else
        {
            bSkipOutputBufferInitialization = false;
        }
        if (bBufferInit)
        {
            /* ------------------------------------------------------------ */
            /* #4045: Initialize the newly allocated buffers before handing */
            /* them off to the sources. These buffers are packed, so we     */
            /* don't need any special line-by-line handling when a nonzero  */
            /* nodata value is set.                                         */
            /* ------------------------------------------------------------ */
            if (!m_bNoDataValueSet || m_dfNoDataValue == 0)
            {
                memset(apBuffers[iBuffer].get(), 0,
                       static_cast<size_t>(nSrcTypeSize) * nExtBufXSize *
                           nExtBufYSize);
            }
            else
            {
                GDALCopyWords64(&m_dfNoDataValue, GDT_Float64, 0,
                                static_cast<GByte *>(apBuffers[iBuffer].get()),
                                eSrcType, nSrcTypeSize,
                                static_cast<GPtrDiff_t>(nExtBufXSize) *
                                    nExtBufYSize);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize the buffer to some background value. Use the         */
    /*      nodata value if available.                                      */
    /* -------------------------------------------------------------------- */
    if (!bSkipOutputBufferInitialization)
    {
        InitializeOutputBuffer(pData, nBufXSize, nBufYSize, eBufType,
                               nPixelSpace, nLineSpace);
    }

    // No contributing sources and SkipNonContributingSources mode ?
    // Do not call the pixel function and just return the 0/nodata initialized
    // output buffer.
    if (anMapBufferIdxToSourceIdx.empty() &&
        m_poPrivate->m_bSkipNonContributingSources)
    {
        return CE_None;
    }

    GDALRasterIOExtraArg sExtraArg;
    GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

    int nXShiftInBuffer = 0;
    int nYShiftInBuffer = 0;
    int nExtBufXSizeReq = nExtBufXSize;
    int nExtBufYSizeReq = nExtBufYSize;

    int nXOffExt = nXOff;
    int nYOffExt = nYOff;
    int nXSizeExt = nXSize;
    int nYSizeExt = nYSize;

    if (nBufferRadius)
    {
        double dfXRatio = static_cast<double>(nXSize) / nBufXSize;
        double dfYRatio = static_cast<double>(nYSize) / nBufYSize;

        if (!sExtraArg.bFloatingPointWindowValidity)
        {
            sExtraArg.dfXOff = nXOff;
            sExtraArg.dfYOff = nYOff;
            sExtraArg.dfXSize = nXSize;
            sExtraArg.dfYSize = nYSize;
        }

        sExtraArg.dfXOff -= dfXRatio * nBufferRadius;
        sExtraArg.dfYOff -= dfYRatio * nBufferRadius;
        sExtraArg.dfXSize += 2 * dfXRatio * nBufferRadius;
        sExtraArg.dfYSize += 2 * dfYRatio * nBufferRadius;
        if (sExtraArg.dfXOff < 0)
        {
            nXShiftInBuffer = -static_cast<int>(sExtraArg.dfXOff / dfXRatio);
            nExtBufXSizeReq -= nXShiftInBuffer;
            sExtraArg.dfXSize += sExtraArg.dfXOff;
            sExtraArg.dfXOff = 0;
        }
        if (sExtraArg.dfYOff < 0)
        {
            nYShiftInBuffer = -static_cast<int>(sExtraArg.dfYOff / dfYRatio);
            nExtBufYSizeReq -= nYShiftInBuffer;
            sExtraArg.dfYSize += sExtraArg.dfYOff;
            sExtraArg.dfYOff = 0;
        }
        if (sExtraArg.dfXOff + sExtraArg.dfXSize > nRasterXSize)
        {
            nExtBufXSizeReq -= static_cast<int>(
                (sExtraArg.dfXOff + sExtraArg.dfXSize - nRasterXSize) /
                dfXRatio);
            sExtraArg.dfXSize = nRasterXSize - sExtraArg.dfXOff;
        }
        if (sExtraArg.dfYOff + sExtraArg.dfYSize > nRasterYSize)
        {
            nExtBufYSizeReq -= static_cast<int>(
                (sExtraArg.dfYOff + sExtraArg.dfYSize - nRasterYSize) /
                dfYRatio);
            sExtraArg.dfYSize = nRasterYSize - sExtraArg.dfYOff;
        }

        nXOffExt = static_cast<int>(sExtraArg.dfXOff);
        nYOffExt = static_cast<int>(sExtraArg.dfYOff);
        nXSizeExt = std::min(static_cast<int>(sExtraArg.dfXSize + 0.5),
                             nRasterXSize - nXOffExt);
        nYSizeExt = std::min(static_cast<int>(sExtraArg.dfYSize + 0.5),
                             nRasterYSize - nYOffExt);
    }

    // Load values for sources into packed buffers.
    CPLErr eErr = CE_None;
    VRTSource::WorkingState oWorkingState;
    for (size_t iBuffer = 0;
         iBuffer < anMapBufferIdxToSourceIdx.size() && eErr == CE_None;
         iBuffer++)
    {
        const int iSource = anMapBufferIdxToSourceIdx[iBuffer];
        GByte *pabyBuffer = static_cast<GByte *>(apBuffers[iBuffer].get());
        eErr = static_cast<VRTSource *>(m_papoSources[iSource].get())
                   ->RasterIO(
                       eSrcType, nXOffExt, nYOffExt, nXSizeExt, nYSizeExt,
                       pabyBuffer + (static_cast<size_t>(nYShiftInBuffer) *
                                         nExtBufXSize +
                                     nXShiftInBuffer) *
                                        nSrcTypeSize,
                       nExtBufXSizeReq, nExtBufYSizeReq, eSrcType, nSrcTypeSize,
                       static_cast<GSpacing>(nSrcTypeSize) * nExtBufXSize,
                       &sExtraArg, oWorkingState);

        // Extend first lines
        for (int iY = 0; iY < nYShiftInBuffer; iY++)
        {
            memcpy(pabyBuffer +
                       static_cast<size_t>(iY) * nExtBufXSize * nSrcTypeSize,
                   pabyBuffer + static_cast<size_t>(nYShiftInBuffer) *
                                    nExtBufXSize * nSrcTypeSize,
                   static_cast<size_t>(nExtBufXSize) * nSrcTypeSize);
        }
        // Extend last lines
        for (int iY = nYShiftInBuffer + nExtBufYSizeReq; iY < nExtBufYSize;
             iY++)
        {
            memcpy(pabyBuffer +
                       static_cast<size_t>(iY) * nExtBufXSize * nSrcTypeSize,
                   pabyBuffer + static_cast<size_t>(nYShiftInBuffer +
                                                    nExtBufYSizeReq - 1) *
                                    nExtBufXSize * nSrcTypeSize,
                   static_cast<size_t>(nExtBufXSize) * nSrcTypeSize);
        }
        // Extend first cols
        if (nXShiftInBuffer)
        {
            for (int iY = 0; iY < nExtBufYSize; iY++)
            {
                for (int iX = 0; iX < nXShiftInBuffer; iX++)
                {
                    memcpy(pabyBuffer +
                               static_cast<size_t>(iY * nExtBufXSize + iX) *
                                   nSrcTypeSize,
                           pabyBuffer +
                               (static_cast<size_t>(iY) * nExtBufXSize +
                                nXShiftInBuffer) *
                                   nSrcTypeSize,
                           nSrcTypeSize);
                }
            }
        }
        // Extent last cols
        if (nXShiftInBuffer + nExtBufXSizeReq < nExtBufXSize)
        {
            for (int iY = 0; iY < nExtBufYSize; iY++)
            {
                for (int iX = nXShiftInBuffer + nExtBufXSizeReq;
                     iX < nExtBufXSize; iX++)
                {
                    memcpy(pabyBuffer +
                               (static_cast<size_t>(iY) * nExtBufXSize + iX) *
                                   nSrcTypeSize,
                           pabyBuffer +
                               (static_cast<size_t>(iY) * nExtBufXSize +
                                nXShiftInBuffer + nExtBufXSizeReq - 1) *
                                   nSrcTypeSize,
                           nSrcTypeSize);
                }
            }
        }
    }

    // Collect any pixel function arguments into oAdditionalArgs
    if (poPixelFunc != nullptr && !poPixelFunc->second.empty())
    {
        if (GetPixelFunctionArguments(poPixelFunc->second,
                                      anMapBufferIdxToSourceIdx, nXOff, nYOff,
                                      oAdditionalArgs) != CE_None)
        {
            eErr = CE_Failure;
        }
    }

    // Apply pixel function.
    const int nBufferCount = static_cast<int>(anMapBufferIdxToSourceIdx.size());
    if (eErr == CE_None && EQUAL(m_poPrivate->m_osLanguage, "Python"))
    {
        // numpy doesn't have native cint16/cint32/cfloat16
        if (eSrcType == GDT_CInt16 || eSrcType == GDT_CInt32 ||
            eSrcType == GDT_CFloat16)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CInt16/CInt32/CFloat16 data type not supported for "
                     "SourceTransferType");
            return CE_Failure;
        }
        if (eDataType == GDT_CInt16 || eDataType == GDT_CInt32 ||
            eDataType == GDT_CFloat16)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "CInt16/CInt32/CFloat16 data type not supported for data type");
            return CE_Failure;
        }

        if (!InitializePython())
            return CE_Failure;

        std::unique_ptr<GByte, VSIFreeReleaser> pabyTmpBuffer;
        // Do we need a temporary buffer or can we use directly the output
        // buffer ?
        if (nBufferRadius != 0 || eDataType != eBufType ||
            nPixelSpace != nBufTypeSize ||
            nLineSpace != static_cast<GSpacing>(nBufTypeSize) * nBufXSize)
        {
            pabyTmpBuffer.reset(static_cast<GByte *>(VSI_CALLOC_VERBOSE(
                static_cast<size_t>(nExtBufXSize) * nExtBufYSize,
                GDALGetDataTypeSizeBytes(eDataType))));
            if (!pabyTmpBuffer)
                return CE_Failure;
        }

        {
            const bool bUseExclusiveLock =
                m_poPrivate->m_bExclusiveLock ||
                (m_poPrivate->m_bFirstTime &&
                 m_poPrivate->m_osCode.find("@jit") != std::string::npos);
            m_poPrivate->m_bFirstTime = false;
            GIL_Holder oHolder(bUseExclusiveLock);

            // Prepare target numpy array
            PyObject *poPyDstArray = GDALCreateNumpyArray(
                m_poPrivate->m_poGDALCreateNumpyArray,
                pabyTmpBuffer ? pabyTmpBuffer.get() : pData, eDataType,
                nExtBufYSize, nExtBufXSize);
            if (!poPyDstArray)
            {
                return CE_Failure;
            }

            // Wrap source buffers as input numpy arrays
            PyObject *pyArgInputArray = PyTuple_New(nBufferCount);
            for (int i = 0; i < nBufferCount; i++)
            {
                GByte *pabyBuffer = static_cast<GByte *>(apBuffers[i].get());
                PyObject *poPySrcArray = GDALCreateNumpyArray(
                    m_poPrivate->m_poGDALCreateNumpyArray, pabyBuffer, eSrcType,
                    nExtBufYSize, nExtBufXSize);
                CPLAssert(poPySrcArray);
                PyTuple_SetItem(pyArgInputArray, i, poPySrcArray);
            }

            // Create arguments
            PyObject *pyArgs = PyTuple_New(10);
            PyTuple_SetItem(pyArgs, 0, pyArgInputArray);
            PyTuple_SetItem(pyArgs, 1, poPyDstArray);
            PyTuple_SetItem(pyArgs, 2, PyLong_FromLong(nXOff));
            PyTuple_SetItem(pyArgs, 3, PyLong_FromLong(nYOff));
            PyTuple_SetItem(pyArgs, 4, PyLong_FromLong(nXSize));
            PyTuple_SetItem(pyArgs, 5, PyLong_FromLong(nYSize));
            PyTuple_SetItem(pyArgs, 6, PyLong_FromLong(nRasterXSize));
            PyTuple_SetItem(pyArgs, 7, PyLong_FromLong(nRasterYSize));
            PyTuple_SetItem(pyArgs, 8, PyLong_FromLong(nBufferRadius));

            GDALGeoTransform gt;
            if (GetDataset())
                GetDataset()->GetGeoTransform(gt);
            PyObject *pyGT = PyTuple_New(6);
            for (int i = 0; i < 6; i++)
                PyTuple_SetItem(pyGT, i, PyFloat_FromDouble(gt[i]));
            PyTuple_SetItem(pyArgs, 9, pyGT);

            // Prepare kwargs
            PyObject *pyKwargs = PyDict_New();
            for (size_t i = 0; i < m_poPrivate->m_oFunctionArgs.size(); ++i)
            {
                const char *pszKey =
                    m_poPrivate->m_oFunctionArgs[i].first.c_str();
                const char *pszValue =
                    m_poPrivate->m_oFunctionArgs[i].second.c_str();
                PyDict_SetItemString(
                    pyKwargs, pszKey,
                    PyBytes_FromStringAndSize(pszValue, strlen(pszValue)));
            }

            // Call user function
            PyObject *pRetValue =
                PyObject_Call(m_poPrivate->m_poUserFunction, pyArgs, pyKwargs);

            Py_DecRef(pyArgs);
            Py_DecRef(pyKwargs);

            if (ErrOccurredEmitCPLError())
            {
                eErr = CE_Failure;
            }
            if (pRetValue)
                Py_DecRef(pRetValue);
        }  // End of GIL section

        if (pabyTmpBuffer)
        {
            // Copy numpy destination array to user buffer
            for (int iY = 0; iY < nBufYSize; iY++)
            {
                size_t nSrcOffset =
                    (static_cast<size_t>(iY + nBufferRadius) * nExtBufXSize +
                     nBufferRadius) *
                    GDALGetDataTypeSizeBytes(eDataType);
                GDALCopyWords64(pabyTmpBuffer.get() + nSrcOffset, eDataType,
                                GDALGetDataTypeSizeBytes(eDataType),
                                static_cast<GByte *>(pData) + iY * nLineSpace,
                                eBufType, static_cast<int>(nPixelSpace),
                                nBufXSize);
            }
        }
    }
    else if (eErr == CE_None && poPixelFunc != nullptr)
    {
        CPLStringList aosArgs;

        // Apply arguments specified using <PixelFunctionArguments>
        for (const auto &[pszKey, pszValue] : m_poPrivate->m_oFunctionArgs)
        {
            aosArgs.SetNameValue(pszKey, pszValue);
        }

        // Apply built-in arguments, potentially overwriting those in <PixelFunctionArguments>
        // This is important because some pixel functions rely on built-in arguments being
        // properly formatted, or even being a valid pointer. If a user can override these, we could have a crash.
        for (const auto &[pszKey, pszValue] : oAdditionalArgs)
        {
            aosArgs.SetNameValue(pszKey, pszValue);
        }

        static_assert(sizeof(apBuffers[0]) == sizeof(void *));
        eErr = (poPixelFunc->first)(
            // We cast vector<unique_ptr<void>>.data() as void**. This is OK
            // given above static_assert
            reinterpret_cast<void **>(apBuffers.data()), nBufferCount, pData,
            nBufXSize, nBufYSize, eSrcType, eBufType,
            static_cast<int>(nPixelSpace), static_cast<int>(nLineSpace),
            aosArgs.List());
    }

    return eErr;
}

/************************************************************************/
/*                       IGetDataCoverageStatus()                       */
/************************************************************************/

int VRTDerivedRasterBand::IGetDataCoverageStatus(
    int /* nXOff */, int /* nYOff */, int /* nXSize */, int /* nYSize */,
    int /* nMaskFlagStop */, double *pdfDataPct)
{
    if (pdfDataPct != nullptr)
        *pdfDataPct = -1.0;
    return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
           GDAL_DATA_COVERAGE_STATUS_DATA;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTDerivedRasterBand::XMLInit(const CPLXMLNode *psTree,
                                     const char *pszVRTPath,
                                     VRTMapSharedResources &oMapSharedSources)

{
    const CPLErr eErr =
        VRTSourcedRasterBand::XMLInit(psTree, pszVRTPath, oMapSharedSources);
    if (eErr != CE_None)
        return eErr;

    // Read derived pixel function type.
    SetPixelFunctionName(CPLGetXMLValue(psTree, "PixelFunctionType", nullptr));
    if (osFuncName.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "PixelFunctionType missing");
        return CE_Failure;
    }

    m_poPrivate->m_osLanguage =
        CPLGetXMLValue(psTree, "PixelFunctionLanguage", "C");
    if (!EQUAL(m_poPrivate->m_osLanguage, "C") &&
        !EQUAL(m_poPrivate->m_osLanguage, "Python"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported PixelFunctionLanguage");
        return CE_Failure;
    }

    m_poPrivate->m_osCode = CPLGetXMLValue(psTree, "PixelFunctionCode", "");
    if (!m_poPrivate->m_osCode.empty() &&
        !EQUAL(m_poPrivate->m_osLanguage, "Python"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "PixelFunctionCode can only be used with Python");
        return CE_Failure;
    }

    m_poPrivate->m_nBufferRadius =
        atoi(CPLGetXMLValue(psTree, "BufferRadius", "0"));
    if (m_poPrivate->m_nBufferRadius < 0 || m_poPrivate->m_nBufferRadius > 1024)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for BufferRadius");
        return CE_Failure;
    }
    if (m_poPrivate->m_nBufferRadius != 0 &&
        !EQUAL(m_poPrivate->m_osLanguage, "Python"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "BufferRadius can only be used with Python");
        return CE_Failure;
    }

    const CPLXMLNode *const psArgs =
        CPLGetXMLNode(psTree, "PixelFunctionArguments");
    if (psArgs != nullptr)
    {
        for (const CPLXMLNode *psIter = psArgs->psChild; psIter;
             psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Attribute)
            {
                AddPixelFunctionArgument(psIter->pszValue,
                                         psIter->psChild->pszValue);
            }
        }
    }

    // Read optional source transfer data type.
    const char *pszTypeName =
        CPLGetXMLValue(psTree, "SourceTransferType", nullptr);
    if (pszTypeName != nullptr)
    {
        eSourceTransferType = GDALGetDataTypeByName(pszTypeName);
    }

    // Whether to skip non contributing sources
    const char *pszSkipNonContributingSources =
        CPLGetXMLValue(psTree, "SkipNonContributingSources", nullptr);
    if (pszSkipNonContributingSources)
    {
        SetSkipNonContributingSources(
            CPLTestBool(pszSkipNonContributingSources));
    }

    return CE_None;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTDerivedRasterBand::SerializeToXML(const char *pszVRTPath,
                                                 bool &bHasWarnedAboutRAMUsage,
                                                 size_t &nAccRAMUsage)
{
    CPLXMLNode *psTree = VRTSourcedRasterBand::SerializeToXML(
        pszVRTPath, bHasWarnedAboutRAMUsage, nAccRAMUsage);

    /* -------------------------------------------------------------------- */
    /*      Set subclass.                                                   */
    /* -------------------------------------------------------------------- */
    CPLCreateXMLNode(CPLCreateXMLNode(psTree, CXT_Attribute, "subClass"),
                     CXT_Text, "VRTDerivedRasterBand");

    /* ---- Encode DerivedBand-specific fields ---- */
    if (!EQUAL(m_poPrivate->m_osLanguage, "C"))
    {
        CPLSetXMLValue(psTree, "PixelFunctionLanguage",
                       m_poPrivate->m_osLanguage);
    }
    if (!osFuncName.empty())
        CPLSetXMLValue(psTree, "PixelFunctionType", osFuncName.c_str());
    if (!m_poPrivate->m_oFunctionArgs.empty())
    {
        CPLXMLNode *psArgs =
            CPLCreateXMLNode(psTree, CXT_Element, "PixelFunctionArguments");
        for (size_t i = 0; i < m_poPrivate->m_oFunctionArgs.size(); ++i)
        {
            const char *pszKey = m_poPrivate->m_oFunctionArgs[i].first.c_str();
            const char *pszValue =
                m_poPrivate->m_oFunctionArgs[i].second.c_str();
            CPLCreateXMLNode(CPLCreateXMLNode(psArgs, CXT_Attribute, pszKey),
                             CXT_Text, pszValue);
        }
    }
    if (!m_poPrivate->m_osCode.empty())
    {
        if (m_poPrivate->m_osCode.find("<![CDATA[") == std::string::npos)
        {
            CPLCreateXMLNode(
                CPLCreateXMLNode(psTree, CXT_Element, "PixelFunctionCode"),
                CXT_Literal,
                ("<![CDATA[" + m_poPrivate->m_osCode + "]]>").c_str());
        }
        else
        {
            CPLSetXMLValue(psTree, "PixelFunctionCode", m_poPrivate->m_osCode);
        }
    }
    if (m_poPrivate->m_nBufferRadius != 0)
        CPLSetXMLValue(psTree, "BufferRadius",
                       CPLSPrintf("%d", m_poPrivate->m_nBufferRadius));
    if (this->eSourceTransferType != GDT_Unknown)
        CPLSetXMLValue(psTree, "SourceTransferType",
                       GDALGetDataTypeName(eSourceTransferType));

    if (m_poPrivate->m_bSkipNonContributingSourcesSpecified)
    {
        CPLSetXMLValue(psTree, "SkipNonContributingSources",
                       m_poPrivate->m_bSkipNonContributingSources ? "true"
                                                                  : "false");
    }

    return psTree;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTDerivedRasterBand::GetMinimum(int *pbSuccess)
{
    return GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTDerivedRasterBand::GetMaximum(int *pbSuccess)
{
    return GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                        ComputeRasterMinMax()                         */
/************************************************************************/

CPLErr VRTDerivedRasterBand::ComputeRasterMinMax(int bApproxOK,
                                                 double *adfMinMax)
{
    return GDALRasterBand::ComputeRasterMinMax(bApproxOK, adfMinMax);
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

CPLErr VRTDerivedRasterBand::ComputeStatistics(int bApproxOK, double *pdfMin,
                                               double *pdfMax, double *pdfMean,
                                               double *pdfStdDev,
                                               GDALProgressFunc pfnProgress,
                                               void *pProgressData)

{
    return GDALRasterBand::ComputeStatistics(bApproxOK, pdfMin, pdfMax, pdfMean,
                                             pdfStdDev, pfnProgress,
                                             pProgressData);
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTDerivedRasterBand::GetHistogram(double dfMin, double dfMax,
                                          int nBuckets, GUIntBig *panHistogram,
                                          int bIncludeOutOfRange, int bApproxOK,
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData)

{
    return VRTRasterBand::GetHistogram(dfMin, dfMax, nBuckets, panHistogram,
                                       bIncludeOutOfRange, bApproxOK,
                                       pfnProgress, pProgressData);
}

/*! @endcond */
