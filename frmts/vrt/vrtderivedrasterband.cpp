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
 *****************************************************************************/

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "vrtdataset.h"
#include "cpl_multiproc.h"
#include "gdalpython.h"

#include <algorithm>
#include <map>
#include <vector>
#include <utility>

/*! @cond Doxygen_Suppress */

CPL_CVSID("$Id$")

using namespace GDALPy;

// #define GDAL_VRT_DISABLE_PYTHON

#ifndef GDAL_VRT_ENABLE_PYTHON_DEFAULT
// Can be YES, NO or TRUSTED_MODULES
#define GDAL_VRT_ENABLE_PYTHON_DEFAULT "TRUSTED_MODULES"
#endif

static std::map<CPLString, std::pair<VRTDerivedRasterBand::PixelFunc, CPLString>> osMapPixelFunction;

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

static PyObject* GDALCreateNumpyArray(PyObject* pCreateArray,
                                      void* pBuffer,
                                      GDALDataType eType,
                                      int nHeight,
                                      int nWidth )
{
    PyObject* poPyBuffer;
    const size_t nSize = static_cast<size_t>(nHeight) * nWidth *
                                    GDALGetDataTypeSizeBytes(eType);
    Py_buffer pybuffer;
    if( PyBuffer_FillInfo(&pybuffer, nullptr, static_cast<char*>(pBuffer),
                          nSize,
                          0, PyBUF_FULL) != 0)
    {
        return nullptr;
    }
    poPyBuffer = PyMemoryView_FromBuffer(&pybuffer);
    PyObject* pArgsCreateArray = PyTuple_New(4);
    PyTuple_SetItem(pArgsCreateArray, 0, poPyBuffer);
    const char* pszDataType = nullptr;
    switch( eType )
    {
        case GDT_Byte: pszDataType = "uint8"; break;
        case GDT_UInt16: pszDataType = "uint16"; break;
        case GDT_Int16: pszDataType = "int16"; break;
        case GDT_UInt32: pszDataType = "uint32"; break;
        case GDT_Int32: pszDataType = "int32"; break;
        case GDT_Float32: pszDataType = "float32"; break;
        case GDT_Float64: pszDataType = "float64"; break;
        case GDT_CInt16:
        case GDT_CInt32:
            CPLAssert(FALSE);
            break;
        case GDT_CFloat32: pszDataType = "complex64"; break;
        case GDT_CFloat64: pszDataType = "complex128"; break;
        default:
            CPLAssert(FALSE);
            break;
    }
    PyTuple_SetItem(pArgsCreateArray, 1,
                PyBytes_FromStringAndSize(pszDataType, strlen(pszDataType)));
    PyTuple_SetItem(pArgsCreateArray, 2, PyLong_FromLong(nHeight));
    PyTuple_SetItem(pArgsCreateArray, 3, PyLong_FromLong(nWidth));
    PyObject* poNumpyArray = PyObject_Call(pCreateArray, pArgsCreateArray, nullptr);
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
        VRTDerivedRasterBandPrivateData(const VRTDerivedRasterBandPrivateData&) = delete;
        VRTDerivedRasterBandPrivateData& operator= (const VRTDerivedRasterBandPrivateData&) = delete;

    public:
        CPLString m_osCode{};
        CPLString m_osLanguage;
        int       m_nBufferRadius;
        PyObject* m_poGDALCreateNumpyArray;
        PyObject* m_poUserFunction;
        bool      m_bPythonInitializationDone;
        bool      m_bPythonInitializationSuccess;
        bool      m_bExclusiveLock;
        bool      m_bFirstTime;
        std::vector< std::pair<CPLString,CPLString> > m_oFunctionArgs{};

        VRTDerivedRasterBandPrivateData():
            m_osLanguage("C"),
            m_nBufferRadius(0),
            m_poGDALCreateNumpyArray(nullptr),
            m_poUserFunction(nullptr),
            m_bPythonInitializationDone(false),
            m_bPythonInitializationSuccess(false),
            m_bExclusiveLock(false),
            m_bFirstTime(true)
        {
        }

        virtual ~VRTDerivedRasterBandPrivateData()
        {
            if( m_poGDALCreateNumpyArray )
                Py_DecRef(m_poGDALCreateNumpyArray);
            if( m_poUserFunction )
                Py_DecRef(m_poUserFunction);
        }
};

/************************************************************************/
/* ==================================================================== */
/*                          VRTDerivedRasterBand                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VRTDerivedRasterBand()                        */
/************************************************************************/

VRTDerivedRasterBand::VRTDerivedRasterBand( GDALDataset *poDSIn, int nBandIn ) :
    VRTSourcedRasterBand( poDSIn, nBandIn ),
    m_poPrivate(nullptr),
    pszFuncName(nullptr),
    eSourceTransferType(GDT_Unknown)
{
    m_poPrivate = new VRTDerivedRasterBandPrivateData;
}

/************************************************************************/
/*                        VRTDerivedRasterBand()                        */
/************************************************************************/

VRTDerivedRasterBand::VRTDerivedRasterBand( GDALDataset *poDSIn, int nBandIn,
                                            GDALDataType eType,
                                            int nXSize, int nYSize ) :
    VRTSourcedRasterBand(poDSIn, nBandIn, eType, nXSize, nYSize),
    m_poPrivate(nullptr),
    pszFuncName(nullptr),
    eSourceTransferType(GDT_Unknown)
{
    m_poPrivate = new VRTDerivedRasterBandPrivateData;
}

/************************************************************************/
/*                       ~VRTDerivedRasterBand()                        */
/************************************************************************/

VRTDerivedRasterBand::~VRTDerivedRasterBand()

{
    CPLFree( pszFuncName );
    delete m_poPrivate;
}

/************************************************************************/
/*                               Cleanup()                              */
/************************************************************************/

void VRTDerivedRasterBand::Cleanup()
{
}

/************************************************************************/
/*                           AddPixelFunction()                         */
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
CPLErr CPL_STDCALL
GDALAddDerivedBandPixelFunc( const char *pszName,
                             GDALDerivedPixelFunc pfnNewFunction )
{
    if( pszName == nullptr || pszName[0] == '\0' ||
        pfnNewFunction == nullptr )
    {
      return CE_None;
    }

    osMapPixelFunction[pszName] = {
        [pfnNewFunction](void **papoSources, int nSources, void *pData,
                                         int nBufXSize, int nBufYSize,
                                         GDALDataType eSrcType, GDALDataType eBufType,
                                         int nPixelSpace, int nLineSpace, CSLConstList papszFunctionArgs) {
            (void) papszFunctionArgs;
            return pfnNewFunction(papoSources, nSources, pData, nBufXSize, nBufYSize,
                                eSrcType, eBufType, nPixelSpace, nLineSpace);
        },
        ""
    };

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
CPLErr CPL_STDCALL
GDALAddDerivedBandPixelFuncWithArgs(const char *pszName,
                                    GDALDerivedPixelFuncWithArgs pfnNewFunction,
                                    const char *pszMetadata)
{
    if( pszName == nullptr || pszName[0] == '\0' ||
        pfnNewFunction == nullptr )
    {
        return CE_None;
    }

    osMapPixelFunction[pszName] = {
        pfnNewFunction,
        pszMetadata != nullptr ? pszMetadata : ""
    };

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
VRTDerivedRasterBand::AddPixelFunction(
    const char *pszFuncNameIn, GDALDerivedPixelFunc pfnNewFunction )
{
    return GDALAddDerivedBandPixelFunc(pszFuncNameIn, pfnNewFunction);
}

CPLErr
VRTDerivedRasterBand::AddPixelFunction(
        const char *pszFuncNameIn,
        GDALDerivedPixelFuncWithArgs pfnNewFunction,
        const char *pszMetadata)
{
    return GDALAddDerivedBandPixelFuncWithArgs(pszFuncNameIn, pfnNewFunction, pszMetadata);
}

/************************************************************************/
/*                           GetPixelFunction()                         */
/************************************************************************/

/**
 * Get a pixel function previously registered using the global
 * AddPixelFunction.
 *
 * @param pszFuncNameIn The name associated with the pixel function.
 *
 * @return A derived band pixel function, or NULL if none have been
 * registered for pszFuncName.
 */
std::pair<VRTDerivedRasterBand::PixelFunc, CPLString>*
VRTDerivedRasterBand::GetPixelFunction( const char *pszFuncNameIn )
{
    if( pszFuncNameIn == nullptr || pszFuncNameIn[0] == '\0' )
    {
        return nullptr;
    }

    auto oIter = osMapPixelFunction.find(pszFuncNameIn);

    if( oIter == osMapPixelFunction.end())
        return nullptr;

    return &(oIter->second);
}

/************************************************************************/
/*                         SetPixelFunctionName()                       */
/************************************************************************/

/**
 * Set the pixel function name to be applied to this derived band.  The
 * name should match a pixel function registered using AddPixelFunction.
 *
 * @param pszFuncNameIn Name of pixel function to be applied to this derived
 * band.
 */
void VRTDerivedRasterBand::SetPixelFunctionName( const char *pszFuncNameIn )
{
    CPLFree(pszFuncName);
    pszFuncName = CPLStrdup( pszFuncNameIn );
}

/************************************************************************/
/*                         SetPixelFunctionLanguage()                   */
/************************************************************************/

/**
 * Set the language of the pixel function.
 *
 * @param pszLanguage Language of the pixel function (only "C" and "Python"
 * are supported currently)
 * @since GDAL 2.3
 */
void VRTDerivedRasterBand::SetPixelFunctionLanguage( const char* pszLanguage )
{
    m_poPrivate->m_osLanguage = pszLanguage;
}

/************************************************************************/
/*                         SetSourceTransferType()                      */
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
void VRTDerivedRasterBand::SetSourceTransferType( GDALDataType eDataTypeIn )
{
    eSourceTransferType = eDataTypeIn;
}

/************************************************************************/
/*                           InitializePython()                         */
/************************************************************************/

bool VRTDerivedRasterBand::InitializePython()
{
    if( m_poPrivate->m_bPythonInitializationDone )
        return m_poPrivate->m_bPythonInitializationSuccess;

    m_poPrivate->m_bPythonInitializationDone = true;
    m_poPrivate->m_bPythonInitializationSuccess = false;

    const CPLString osPythonFullname( pszFuncName ? pszFuncName : "" );
    const size_t nIdxDot = osPythonFullname.rfind(".");
    CPLString osPythonModule;
    CPLString osPythonFunction;
    if( nIdxDot != std::string::npos )
    {
        osPythonModule = osPythonFullname.substr(0, nIdxDot);
        osPythonFunction = osPythonFullname.substr(nIdxDot+1);
    }
    else
    {
        osPythonFunction = osPythonFullname;
    }

#ifndef GDAL_VRT_DISABLE_PYTHON
    const char* pszPythonEnabled =
                            CPLGetConfigOption("GDAL_VRT_ENABLE_PYTHON", nullptr);
#else
    const char* pszPythonEnabled = "NO";
#endif
    const CPLString osPythonEnabled(pszPythonEnabled ? pszPythonEnabled :
                                            GDAL_VRT_ENABLE_PYTHON_DEFAULT);

    if( EQUAL(osPythonEnabled, "TRUSTED_MODULES") )
    {
        bool bIsTrustedModule = false;
        const CPLString osVRTTrustedModules(
                    CPLGetConfigOption( "GDAL_VRT_PYTHON_TRUSTED_MODULES", "") );
        if( !osPythonModule.empty() )
        {
            char** papszTrustedModules = CSLTokenizeString2(
                                                osVRTTrustedModules, ",", 0 );
            for( char** papszIter = papszTrustedModules;
                !bIsTrustedModule && papszIter && *papszIter;
                ++papszIter )
            {
                const char* pszIterModule = *papszIter;
                size_t nIterModuleLen = strlen(pszIterModule);
                if( nIterModuleLen > 2 &&
                    strncmp(pszIterModule + nIterModuleLen - 2, ".*", 2) == 0 )
                {
                    bIsTrustedModule =
                        (strncmp( osPythonModule, pszIterModule,
                                                  nIterModuleLen - 2 ) == 0) &&
                        (osPythonModule.size() == nIterModuleLen - 2 ||
                         (osPythonModule.size() >= nIterModuleLen &&
                          osPythonModule[nIterModuleLen-1] == '.') );
                }
                else if( nIterModuleLen >= 1 &&
                        pszIterModule[nIterModuleLen-1] == '*' )
                {
                    bIsTrustedModule = (strncmp( osPythonModule, pszIterModule,
                                                nIterModuleLen - 1 ) == 0);
                }
                else
                {
                    bIsTrustedModule =
                                (strcmp(osPythonModule, pszIterModule) == 0);
                }
            }
            CSLDestroy(papszTrustedModules);
        }

        if( !bIsTrustedModule )
        {
            if( osPythonModule.empty() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Python code needs to be executed, but it uses online code "
                         "in the VRT whereas the current policy is to trust only "
                         "code from external trusted modules (defined in the "
                         "GDAL_VRT_PYTHON_TRUSTED_MODULES configuration option). "
                         "If you trust the code in %s, you can set the "
                         "GDAL_VRT_ENABLE_PYTHON configuration option to YES.",
                         GetDataset() ? GetDataset()->GetDescription() :
                                    "(unknown VRT)");
            }
            else if( osVRTTrustedModules.empty() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Python code needs to be executed, but it uses code "
                         "from module '%s', whereas the current policy is to "
                         "trust only code from modules defined in the "
                         "GDAL_VRT_PYTHON_TRUSTED_MODULES configuration option, "
                         "which is currently unset. "
                         "If you trust the code in '%s', you can add module '%s' "
                         "to GDAL_VRT_PYTHON_TRUSTED_MODULES (or set the "
                         "GDAL_VRT_ENABLE_PYTHON configuration option to YES).",
                         osPythonModule.c_str(),
                         GetDataset() ? GetDataset()->GetDescription() :
                                    "(unknown VRT)",
                         osPythonModule.c_str());
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Python code needs to be executed, but it uses code "
                         "from module '%s', whereas the current policy is to "
                         "trust only code from modules '%s' (defined in the "
                         "GDAL_VRT_PYTHON_TRUSTED_MODULES configuration option). "
                         "If you trust the code in '%s', you can add module '%s' "
                         "to GDAL_VRT_PYTHON_TRUSTED_MODULES (or set the "
                         "GDAL_VRT_ENABLE_PYTHON configuration option to YES).",
                         osPythonModule.c_str(),
                         osVRTTrustedModules.c_str(),
                         GetDataset() ? GetDataset()->GetDescription() :
                                    "(unknown VRT)",
                         osPythonModule.c_str());
            }
            return false;
        }
    }

#ifdef disabled_because_this_is_probably_broken_by_design
    // See https://lwn.net/Articles/574215/
    // and http://nedbatchelder.com/blog/201206/eval_really_is_dangerous.html
    else if( EQUAL(osPythonEnabled, "IF_SAFE") )
    {
        bool bSafe = true;
        // If the function comes from another module, then we don't know
        if( !osPythonModule.empty() )
        {
            CPLDebug("VRT", "Python function is from another module");
            bSafe = false;
        }

        CPLString osCode(m_poPrivate->m_osCode);

        // Reject all imports except a few trusted modules
        const char* const apszTrustedImports[] = {
                "import math",
                "from math import",
                "import numpy", // caution: numpy has lots of I/O functions !
                "from numpy import",
                // TODO: not sure if importing arbitrary stuff from numba is OK
                // so let's just restrict to jit.
                "from numba import jit",

                // Not imports but still whitelisted, whereas other __ is banned
                "__init__",
                "__call__",
        };
        for( size_t i = 0; i < CPL_ARRAYSIZE(apszTrustedImports); ++i )
        {
            osCode.replaceAll(CPLString(apszTrustedImports[i]), "");
        }

        // Some dangerous built-in functions or numpy functions
        const char* const apszUntrusted[] = { "import", // and __import__
                                              "eval",
                                              "compile",
                                              "open",
                                              "load", // reload, numpy.load
                                              "file", // and exec_file, numpy.fromfile, numpy.tofile
                                              "input", // and raw_input
                                              "save", // numpy.save
                                              "memmap", // numpy.memmap
                                              "DataSource", // numpy.DataSource
                                              "genfromtxt", // numpy.genfromtxt
                                              "getattr",
                                              "ctypeslib", // numpy.ctypeslib
                                              "testing", // numpy.testing
                                              "dump", // numpy.ndarray.dump
                                              "fromregex", // numpy.fromregex
                                              "__"
                                             };
        for( size_t i = 0; i < CPL_ARRAYSIZE(apszUntrusted); ++i )
        {
            if( osCode.find(apszUntrusted[i]) != std::string::npos )
            {
                CPLDebug("VRT", "Found '%s' word in Python code",
                         apszUntrusted[i]);
                bSafe = false;
            }
        }

        if( !bSafe )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Python code needs to be executed, but we cannot verify "
                     "if it is safe, so this is disabled by default. "
                     "If you trust the code in %s, you can set the "
                     "GDAL_VRT_ENABLE_PYTHON configuration option to YES.",
                     GetDataset() ? GetDataset()->GetDescription() :
                                    "(unknown VRT)");
            return false;
        }
    }
#endif //disabled_because_this_is_probably_broken_by_design

    else if( !EQUAL(osPythonEnabled, "YES") &&
             !EQUAL(osPythonEnabled, "ON") &&
             !EQUAL(osPythonEnabled, "TRUE") )
    {
        if( pszPythonEnabled == nullptr )
        {
            // Note: this is dead code with our current default policy
            // GDAL_VRT_ENABLE_PYTHON == "TRUSTED_MODULES"
            CPLError(CE_Failure, CPLE_AppDefined,
                 "Python code needs to be executed, but this is "
                 "disabled by default. If you trust the code in %s, "
                 "you can set the GDAL_VRT_ENABLE_PYTHON configuration "
                 "option to YES.",
                GetDataset() ? GetDataset()->GetDescription() :
                                                    "(unknown VRT)");
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Python code in %s needs to be executed, but this has been "
                    "explicitly disabled.",
                     GetDataset() ? GetDataset()->GetDescription() :
                                                            "(unknown VRT)");
        }
        return false;
    }

    if( !GDALPythonInitialize() )
        return false;

    // Whether we should just use our own global mutex, in addition to Python
    // GIL locking.
    m_poPrivate->m_bExclusiveLock =
        CPLTestBool(CPLGetConfigOption("GDAL_VRT_PYTHON_EXCLUSIVE_LOCK", "NO"));

    // numba jit'ification doesn't seem to be thread-safe, so force use of
    // lock now and at first execution of function. Later executions seem to
    // be thread-safe. This problem doesn't seem to appear for code in
    // regular files
    const bool bUseExclusiveLock = m_poPrivate->m_bExclusiveLock ||
                    m_poPrivate->m_osCode.find("@jit") != std::string::npos;
    GIL_Holder oHolder(bUseExclusiveLock);

    // As we don't want to depend on numpy C API/ABI, we use a trick to build
    // a numpy array object. We define a Python function to which we pass a
    // Python buffer object.

    // We need to build a unique module name, otherwise this will crash in
    // multithreaded use cases.
    CPLString osModuleName( CPLSPrintf("gdal_vrt_module_%p", this) );
    PyObject* poCompiledString = Py_CompileString(
        ("import numpy\n"
        "def GDALCreateNumpyArray(buffer, dtype, height, width):\n"
        "    return numpy.frombuffer(buffer, str(dtype.decode('ascii')))."
                                                "reshape([height, width])\n"
        "\n" + m_poPrivate->m_osCode).c_str(),
        osModuleName, Py_file_input);
    if( poCompiledString == nullptr || PyErr_Occurred() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Couldn't compile code:\n%s",
                 GetPyExceptionString().c_str());
        return false;
    }
    PyObject* poModule =
        PyImport_ExecCodeModule(osModuleName, poCompiledString);
    Py_DecRef(poCompiledString);

    if( poModule == nullptr || PyErr_Occurred() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
        return false;
    }

    // Fetch user computation function
    if( !osPythonModule.empty() )
    {
        PyObject* poUserModule = PyImport_ImportModule(osPythonModule);
        if (poUserModule == nullptr || PyErr_Occurred())
        {
            CPLString osException = GetPyExceptionString();
            if( !osException.empty() && osException.back() == '\n' )
            {
                osException.resize( osException.size() - 1 );
            }
            if( osException.find("ModuleNotFoundError") == 0 )
            {
                osException += ". You may need to define PYTHONPATH";
            }
            CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", osException.c_str());
            Py_DecRef(poModule);
            return false;
        }
        m_poPrivate->m_poUserFunction = PyObject_GetAttrString(poUserModule,
                                                            osPythonFunction );
        Py_DecRef(poUserModule);
    }
    else
    {
        m_poPrivate->m_poUserFunction = PyObject_GetAttrString(poModule,
                                            osPythonFunction );
    }
    if (m_poPrivate->m_poUserFunction == nullptr || PyErr_Occurred())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
        Py_DecRef(poModule);
        return false;
    }
    if( !PyCallable_Check(m_poPrivate->m_poUserFunction) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Object '%s' is not callable",
                 osPythonFunction.c_str());
        Py_DecRef(poModule);
        return false;
    }

    // Fetch our GDALCreateNumpyArray python function
    m_poPrivate->m_poGDALCreateNumpyArray =
        PyObject_GetAttrString(poModule, "GDALCreateNumpyArray" );
    if (m_poPrivate->m_poGDALCreateNumpyArray == nullptr || PyErr_Occurred())
    {
        // Shouldn't happen normally...
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
        Py_DecRef(poModule);
        return false;
    }
    Py_DecRef(poModule);

    m_poPrivate->m_bPythonInitializationSuccess = true;
    return true;
}

CPLErr
VRTDerivedRasterBand::GetPixelFunctionArguments(
  const CPLString& osMetadata,
  std::vector<std::pair<CPLString, CPLString>>& oAdditionalArgs)
{

    auto poArgs = CPLXMLTreeCloser(CPLParseXMLString(osMetadata));
    if (poArgs != nullptr && poArgs->eType == CXT_Element &&
        !strcmp(poArgs->pszValue,
                 "PixelFunctionArgumentsList"))
    {
        for (CPLXMLNode* psIter = poArgs->psChild; psIter != nullptr;
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
                    double dfVal;
                    int success;
                    if (osValue == "NoData")
                        dfVal = this->GetNoDataValue(&success);
                    else if (osValue == "scale")
                        dfVal = this->GetScale(&success);
                    else if (osValue == "offset")
                        dfVal = this->GetOffset(&success);
                    else
                    {
                        CPLError(CE_Failure,
                                 CPLE_NotSupported,
                                 "PixelFunction builtin %s not supported",
                                 osValue.c_str());
                        return CE_Failure;
                    }
                    if (!success)
                    {
                        CPLError(CE_Failure,
                                 CPLE_AppDefined,
                                 "Raster has no %s",
                                 osValue.c_str());
                        return CE_Failure;
                    }

                    oAdditionalArgs.push_back(std::pair<CPLString, CPLString>(
                      osValue, CPLSPrintf("%.18g", dfVal)));
                    CPLDebug("VRT", "Added builtin pixel function argument %s = %s",
                           osValue.c_str(),
                           CPLSPrintf("%.18g", dfVal));
                }
            }
        }
    }

    return CE_None;
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
CPLErr VRTDerivedRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                        int nXOff, int nYOff, int nXSize,
                                        int nYSize, void * pData, int nBufXSize,
                                        int nBufYSize, GDALDataType eBufType,
                                        GSpacing nPixelSpace,
                                        GSpacing nLineSpace,
                                        GDALRasterIOExtraArg* psExtraArg )
{
    if( eRWFlag == GF_Write )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Writing through VRTSourcedRasterBand is not supported." );
        return CE_Failure;
    }

    const int nBufTypeSize = GDALGetDataTypeSizeBytes(eBufType);
    GDALDataType eSrcType = eSourceTransferType;
    if( eSrcType == GDT_Unknown || eSrcType >= GDT_TypeCount ) {
        eSrcType = eBufType;
    }
    const int nSrcTypeSize = GDALGetDataTypeSizeBytes(eSrcType);

/* -------------------------------------------------------------------- */
/*      Initialize the buffer to some background value. Use the         */
/*      nodata value if available.                                      */
/* -------------------------------------------------------------------- */
    if( SkipBufferInitialization() )
    {
        // Do nothing
    }
    else if( nPixelSpace == nBufTypeSize &&
        (!m_bNoDataValueSet || m_dfNoDataValue == 0) ) {
        memset( pData, 0,
                static_cast<size_t>(nBufXSize * nBufYSize * nPixelSpace) );
    }
    else if( m_bNoDataValueSet )
    {
        double dfWriteValue = m_dfNoDataValue;

        for( int iLine = 0; iLine < nBufYSize; iLine++ )
        {
            GDALCopyWords(
                &dfWriteValue, GDT_Float64, 0,
                static_cast<GByte *>( pData ) + nLineSpace * iLine,
                eBufType, static_cast<int>(nPixelSpace), nBufXSize );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* -------------------------------------------------------------------- */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 )
    {
        if( OverviewRasterIO(
               eRWFlag, nXOff, nYOff, nXSize, nYSize,
               pData, nBufXSize, nBufYSize,
               eBufType, nPixelSpace, nLineSpace, psExtraArg ) == CE_None )
            return CE_None;
    }

    /* ---- Get pixel function for band ---- */
    std::pair<PixelFunc, CPLString> *poPixelFunc = nullptr;
    std::vector<std::pair<CPLString, CPLString>> oAdditionalArgs;

    if( EQUAL(m_poPrivate->m_osLanguage, "C") )
    {
        poPixelFunc = VRTDerivedRasterBand::GetPixelFunction(pszFuncName);
        if( poPixelFunc == nullptr )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                    "VRTDerivedRasterBand::IRasterIO:"
                    "Derived band pixel function '%s' not registered.",
                    this->pszFuncName) ;
            return CE_Failure;
        }

        if (poPixelFunc->second != "")
        {
            if (GetPixelFunctionArguments(poPixelFunc->second,
                                        oAdditionalArgs) != CE_None)
            {
                return CE_Failure;
            }
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
    if( nBufferRadius > (INT_MAX - nBufXSize) / 2 ||
        nBufferRadius > (INT_MAX - nBufYSize) / 2 )
    {
        return CE_Failure;
    }
    const int nExtBufXSize = nBufXSize + 2 * nBufferRadius;
    const int nExtBufYSize = nBufYSize + 2 * nBufferRadius;
    void **pBuffers
        = static_cast<void **>( CPLMalloc(sizeof(void *) * nSources) );
    for( int iSource = 0; iSource < nSources; iSource++ ) {
        pBuffers[iSource] =
            VSI_MALLOC3_VERBOSE(nSrcTypeSize, nExtBufXSize, nExtBufYSize);
        if( pBuffers[iSource] == nullptr )
        {
            for (int i = 0; i < iSource; i++) {
                VSIFree(pBuffers[i]);
            }
            CPLFree(pBuffers);
            return CE_Failure;
        }

        /* ------------------------------------------------------------ */
        /* #4045: Initialize the newly allocated buffers before handing */
        /* them off to the sources. These buffers are packed, so we     */
        /* don't need any special line-by-line handling when a nonzero  */
        /* nodata value is set.                                         */
        /* ------------------------------------------------------------ */
        if( !m_bNoDataValueSet || m_dfNoDataValue == 0 )
        {
            memset( pBuffers[iSource], 0, static_cast<size_t>(nSrcTypeSize) *
                    nExtBufXSize * nExtBufYSize );
        }
        else
        {
            GDALCopyWords( &m_dfNoDataValue, GDT_Float64, 0,
                           static_cast<GByte *>( pBuffers[iSource] ),
                           eSrcType, nSrcTypeSize,
                           nExtBufXSize * nExtBufYSize );
        }
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

    if( nBufferRadius )
    {
        double dfXRatio = static_cast<double>(nXSize) / nBufXSize;
        double dfYRatio = static_cast<double>(nYSize) / nBufYSize;

        if( !sExtraArg.bFloatingPointWindowValidity )
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
        if( sExtraArg.dfXOff < 0 )
        {
            nXShiftInBuffer = -static_cast<int>(sExtraArg.dfXOff / dfXRatio);
            nExtBufXSizeReq -= nXShiftInBuffer;
            sExtraArg.dfXSize += sExtraArg.dfXOff;
            sExtraArg.dfXOff = 0;
        }
        if( sExtraArg.dfYOff < 0 )
        {
            nYShiftInBuffer = -static_cast<int>(sExtraArg.dfYOff / dfYRatio);
            nExtBufYSizeReq -= nYShiftInBuffer;
            sExtraArg.dfYSize += sExtraArg.dfYOff;
            sExtraArg.dfYOff = 0;
        }
        if( sExtraArg.dfXOff + sExtraArg.dfXSize > nRasterXSize )
        {
            nExtBufXSizeReq -= static_cast<int>((sExtraArg.dfXOff +
                        sExtraArg.dfXSize - nRasterXSize) / dfXRatio);
            sExtraArg.dfXSize = nRasterXSize - sExtraArg.dfXOff;
        }
        if( sExtraArg.dfYOff + sExtraArg.dfYSize > nRasterYSize )
        {
            nExtBufYSizeReq -= static_cast<int>((sExtraArg.dfYOff +
                        sExtraArg.dfYSize - nRasterYSize) / dfYRatio);
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
    for( int iSource = 0; iSource < nSources && eErr == CE_None; iSource++ ) {
        GByte* pabyBuffer = static_cast<GByte*>(pBuffers[iSource]);
        eErr = static_cast<VRTSource *>( papoSources[iSource] )->RasterIO(
            eSrcType,
            nXOffExt, nYOffExt, nXSizeExt, nYSizeExt,
            pabyBuffer + (nYShiftInBuffer * nExtBufXSize +
                                            nXShiftInBuffer) * nSrcTypeSize,
            nExtBufXSizeReq, nExtBufYSizeReq,
            eSrcType,
            nSrcTypeSize,
            nSrcTypeSize * nExtBufXSize,
            &sExtraArg );

        // Extend first lines
        for( int iY = 0; iY < nYShiftInBuffer; iY++ )
        {
            memcpy( pabyBuffer + iY * nExtBufXSize * nSrcTypeSize,
                    pabyBuffer + nYShiftInBuffer * nExtBufXSize * nSrcTypeSize,
                    nExtBufXSize * nSrcTypeSize );
        }
        // Extend last lines
        for( int iY = nYShiftInBuffer + nExtBufYSizeReq; iY < nExtBufYSize; iY++ )
        {
            memcpy( pabyBuffer + iY * nExtBufXSize * nSrcTypeSize,
                    pabyBuffer + (nYShiftInBuffer + nExtBufYSizeReq - 1) *
                                                    nExtBufXSize * nSrcTypeSize,
                    nExtBufXSize * nSrcTypeSize );
        }
        // Extend first cols
        if( nXShiftInBuffer )
        {
            for( int iY = 0; iY < nExtBufYSize; iY ++ )
            {
                for( int iX = 0; iX < nXShiftInBuffer; iX++ )
                {
                    memcpy( pabyBuffer + (iY * nExtBufXSize + iX) * nSrcTypeSize,
                            pabyBuffer + (iY * nExtBufXSize +
                                                nXShiftInBuffer) * nSrcTypeSize,
                            nSrcTypeSize );
                }
            }
        }
        // Extent last cols
        if( nXShiftInBuffer + nExtBufXSizeReq < nExtBufXSize )
        {
            for( int iY = 0; iY < nExtBufYSize; iY ++ )
            {
                for( int iX = nXShiftInBuffer + nExtBufXSizeReq;
                         iX < nExtBufXSize; iX++ )
                {
                    memcpy( pabyBuffer + (iY * nExtBufXSize + iX) * nSrcTypeSize,
                            pabyBuffer + (iY * nExtBufXSize + nXShiftInBuffer +
                                            nExtBufXSizeReq - 1) * nSrcTypeSize,
                            nSrcTypeSize );
                }
            }
        }
    }

    // Apply pixel function.
    if( eErr == CE_None && EQUAL(m_poPrivate->m_osLanguage, "Python") )
    {
        eErr = CE_Failure;

        // numpy doesn't have native cint16/cint32
        if( eSrcType == GDT_CInt16 || eSrcType == GDT_CInt32 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CInt16/CInt32 data type not supported for SourceTransferType");
            goto end;
        }
        if( eDataType == GDT_CInt16 || eDataType == GDT_CInt32 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CInt16/CInt32 data type not supported for data type");
            goto end;
        }

        if( !InitializePython() )
            goto end;

        GByte* pabyTmpBuffer = nullptr;
        // Do we need a temporary buffer or can we use directly the output
        // buffer ?
        if( nBufferRadius != 0 ||
            eDataType != eBufType ||
            nPixelSpace != nBufTypeSize ||
            nLineSpace != static_cast<GSpacing>(nBufTypeSize) * nBufXSize )
        {
            pabyTmpBuffer = static_cast<GByte*>(VSI_CALLOC_VERBOSE(
                            static_cast<size_t>(nExtBufXSize) * nExtBufYSize,
                            GDALGetDataTypeSizeBytes(eDataType)));
            if( !pabyTmpBuffer )
                goto end;
        }

        {
        const bool bUseExclusiveLock = m_poPrivate->m_bExclusiveLock ||
                    ( m_poPrivate->m_bFirstTime &&
                    m_poPrivate->m_osCode.find("@jit") != std::string::npos);
        m_poPrivate->m_bFirstTime = false;
        GIL_Holder oHolder(bUseExclusiveLock);

        // Prepare target numpy array
        PyObject* poPyDstArray = GDALCreateNumpyArray(
                                    m_poPrivate->m_poGDALCreateNumpyArray,
                                    pabyTmpBuffer ? pabyTmpBuffer : pData,
                                    eDataType,
                                    nExtBufYSize,
                                    nExtBufXSize);
        if( !poPyDstArray )
        {
            VSIFree(pabyTmpBuffer);
            goto end;
        }

        // Wrap source buffers as input numpy arrays
        PyObject* pyArgInputArray = PyTuple_New(nSources);
        for( int i = 0; i < nSources; i++ )
        {
            GByte* pabyBuffer = static_cast<GByte*>(pBuffers[i]);
            PyObject* poPySrcArray = GDALCreateNumpyArray(
                        m_poPrivate->m_poGDALCreateNumpyArray,
                        pabyBuffer,
                        eSrcType,
                        nExtBufYSize,
                        nExtBufXSize);
            CPLAssert(poPySrcArray);
            PyTuple_SetItem(pyArgInputArray, i, poPySrcArray);
        }

        // Create arguments
        PyObject* pyArgs = PyTuple_New(10);
        PyTuple_SetItem(pyArgs, 0, pyArgInputArray);
        PyTuple_SetItem(pyArgs, 1, poPyDstArray);
        PyTuple_SetItem(pyArgs, 2, PyLong_FromLong(nXOff));
        PyTuple_SetItem(pyArgs, 3, PyLong_FromLong(nYOff));
        PyTuple_SetItem(pyArgs, 4, PyLong_FromLong(nXSize));
        PyTuple_SetItem(pyArgs, 5, PyLong_FromLong(nYSize));
        PyTuple_SetItem(pyArgs, 6, PyLong_FromLong(nRasterXSize));
        PyTuple_SetItem(pyArgs, 7, PyLong_FromLong(nRasterYSize));
        PyTuple_SetItem(pyArgs, 8, PyLong_FromLong(nBufferRadius));

        double adfGeoTransform[6];
        adfGeoTransform[0] = 0;
        adfGeoTransform[1] = 1;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = 0;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = 1;
        if( GetDataset() )
            GetDataset()->GetGeoTransform(adfGeoTransform);
        PyObject* pyGT = PyTuple_New(6);
        for(int i = 0; i < 6; i++ )
            PyTuple_SetItem(pyGT, i, PyFloat_FromDouble(adfGeoTransform[i]));
        PyTuple_SetItem(pyArgs, 9, pyGT);

        // Prepare kwargs
        PyObject* pyKwargs = PyDict_New();
        for( size_t i = 0; i < m_poPrivate->m_oFunctionArgs.size(); ++i )
        {
            const char* pszKey =
                m_poPrivate->m_oFunctionArgs[i].first.c_str();
            const char* pszValue =
                m_poPrivate->m_oFunctionArgs[i].second.c_str();
            PyDict_SetItemString(pyKwargs, pszKey,
                PyBytes_FromStringAndSize(pszValue, strlen(pszValue)));
        }

        // Call user function
        PyObject* pRetValue = PyObject_Call(
                                        m_poPrivate->m_poUserFunction,
                                        pyArgs, pyKwargs);

        Py_DecRef(pyArgs);
        Py_DecRef(pyKwargs);

        if( ErrOccurredEmitCPLError() )
        {
            // do nothing
        }
        else
        {
            eErr = CE_None;
        }
        if( pRetValue )
            Py_DecRef(pRetValue);
        } // End of GIL section

        if( pabyTmpBuffer )
        {
            // Copy numpy destination array to user buffer
            for( int iY = 0; iY < nBufYSize; iY++ )
            {
                size_t nSrcOffset = (static_cast<size_t>(iY + nBufferRadius) *
                    nExtBufXSize + nBufferRadius) *
                    GDALGetDataTypeSizeBytes(eDataType);
                GDALCopyWords(pabyTmpBuffer + nSrcOffset,
                              eDataType,
                              GDALGetDataTypeSizeBytes(eDataType),
                              static_cast<GByte*>(pData) + iY * nLineSpace,
                              eBufType,
                              static_cast<int>(nPixelSpace),
                              nBufXSize);
            }

            VSIFree(pabyTmpBuffer);
        }
    }
    else if( eErr == CE_None && poPixelFunc != nullptr )
    {
        char **papszArgs = nullptr;

        oAdditionalArgs.insert(oAdditionalArgs.end(),
            m_poPrivate->m_oFunctionArgs.begin(), m_poPrivate->m_oFunctionArgs.end());
        for (const auto& oArg : oAdditionalArgs)
        {
            const char *pszKey = oArg.first.c_str();
            const char *pszValue = oArg.second.c_str();
            papszArgs = CSLSetNameValue(papszArgs, pszKey, pszValue);
        }

        eErr = (poPixelFunc->first)(static_cast<void **>( pBuffers ), nSources,
                              pData, nBufXSize, nBufYSize,
                              eSrcType, eBufType, static_cast<int>(nPixelSpace),
                              static_cast<int>(nLineSpace),
                              papszArgs);

        CSLDestroy(papszArgs);
    }
end:
    // Release buffers.
    for ( int iSource = 0; iSource < nSources; iSource++ ) {
        VSIFree(pBuffers[iSource]);
    }
    CPLFree(pBuffers);

    return eErr;
}

/************************************************************************/
/*                         IGetDataCoverageStatus()                     */
/************************************************************************/

int  VRTDerivedRasterBand::IGetDataCoverageStatus( int /* nXOff */,
                                                   int /* nYOff */,
                                                   int /* nXSize */,
                                                   int /* nYSize */,
                                                   int /* nMaskFlagStop */,
                                                   double* pdfDataPct)
{
    if( pdfDataPct != nullptr )
        *pdfDataPct = -1.0;
    return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED | GDAL_DATA_COVERAGE_STATUS_DATA;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTDerivedRasterBand::XMLInit( CPLXMLNode *psTree,
                                      const char *pszVRTPath,
                                      std::map<CPLString, GDALDataset*>& oMapSharedSources )

{
    const CPLErr eErr = VRTSourcedRasterBand::XMLInit( psTree, pszVRTPath,
                                                       oMapSharedSources );
    if( eErr != CE_None )
        return eErr;

    // Read derived pixel function type.
    SetPixelFunctionName( CPLGetXMLValue( psTree, "PixelFunctionType", nullptr ) );
    if( pszFuncName == nullptr || EQUAL(pszFuncName, "") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "PixelFunctionType missing");
        return CE_Failure;
    }

    m_poPrivate->m_osLanguage = CPLGetXMLValue( psTree,
                                                "PixelFunctionLanguage", "C" );
    if( !EQUAL(m_poPrivate->m_osLanguage, "C") &&
        !EQUAL(m_poPrivate->m_osLanguage, "Python") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported PixelFunctionLanguage");
        return CE_Failure;
    }

    m_poPrivate->m_osCode =
                        CPLGetXMLValue( psTree, "PixelFunctionCode", "" );
    if( !m_poPrivate->m_osCode.empty() &&
        !EQUAL(m_poPrivate->m_osLanguage, "Python") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "PixelFunctionCode can only be used with Python");
        return CE_Failure;
    }

    m_poPrivate->m_nBufferRadius =
                        atoi(CPLGetXMLValue( psTree, "BufferRadius", "0" ));
    if( m_poPrivate->m_nBufferRadius < 0 ||
        m_poPrivate->m_nBufferRadius > 1024 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for BufferRadius");
        return CE_Failure;
    }
    if( m_poPrivate->m_nBufferRadius != 0 &&
        !EQUAL(m_poPrivate->m_osLanguage, "Python") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "BufferRadius can only be used with Python");
        return CE_Failure;
    }

    CPLXMLNode* psArgs = CPLGetXMLNode( psTree, "PixelFunctionArguments" );
    if( psArgs != nullptr )
    {
        for( CPLXMLNode* psIter = psArgs->psChild;
                         psIter != nullptr;
                         psIter = psIter->psNext )
        {
            if( psIter->eType == CXT_Attribute )
            {
                m_poPrivate->m_oFunctionArgs.push_back(
                    std::pair<CPLString,CPLString>(psIter->pszValue,
                                                   psIter->psChild->pszValue));
            }
        }
    }

    // Read optional source transfer data type.
    const char *pszTypeName = CPLGetXMLValue(psTree, "SourceTransferType", nullptr);
    if( pszTypeName != nullptr )
    {
        eSourceTransferType = GDALGetDataTypeByName( pszTypeName );
    }

    return CE_None;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTDerivedRasterBand::SerializeToXML( const char *pszVRTPath )
{
    CPLXMLNode *psTree = VRTSourcedRasterBand::SerializeToXML( pszVRTPath );

/* -------------------------------------------------------------------- */
/*      Set subclass.                                                   */
/* -------------------------------------------------------------------- */
    CPLCreateXMLNode(
        CPLCreateXMLNode( psTree, CXT_Attribute, "subClass" ),
        CXT_Text, "VRTDerivedRasterBand" );

    /* ---- Encode DerivedBand-specific fields ---- */
    if( !EQUAL( m_poPrivate->m_osLanguage, "C" ) )
    {
        CPLSetXMLValue( psTree, "PixelFunctionLanguage",
                        m_poPrivate->m_osLanguage );
    }
    if( pszFuncName != nullptr && strlen(pszFuncName) > 0 )
        CPLSetXMLValue( psTree, "PixelFunctionType", pszFuncName );
    if( !m_poPrivate->m_oFunctionArgs.empty() )
    {
        CPLXMLNode* psArgs = CPLCreateXMLNode( psTree, CXT_Element,
                                               "PixelFunctionArguments" );
        for( size_t i = 0; i < m_poPrivate->m_oFunctionArgs.size(); ++i )
        {
            const char* pszKey =
                m_poPrivate->m_oFunctionArgs[i].first.c_str();
            const char* pszValue =
                m_poPrivate->m_oFunctionArgs[i].second.c_str();
            CPLCreateXMLNode(
                CPLCreateXMLNode( psArgs, CXT_Attribute, pszKey ),
                                  CXT_Text, pszValue );
        }
    }
    if( !m_poPrivate->m_osCode.empty() )
    {
        if( m_poPrivate->m_osCode.find("<![CDATA[") == std::string::npos )
        {
            CPLCreateXMLNode(
                CPLCreateXMLNode( psTree,
                                  CXT_Element, "PixelFunctionCode" ),
                 CXT_Literal,
                 ("<![CDATA[" + m_poPrivate->m_osCode + "]]>").c_str() );
        }
        else
        {
            CPLSetXMLValue( psTree, "PixelFunctionCode",
                            m_poPrivate->m_osCode );
        }
    }
    if( m_poPrivate->m_nBufferRadius != 0 )
        CPLSetXMLValue( psTree, "BufferRadius",
                        CPLSPrintf("%d",m_poPrivate->m_nBufferRadius) );
    if( this->eSourceTransferType != GDT_Unknown)
        CPLSetXMLValue( psTree, "SourceTransferType",
                        GDALGetDataTypeName( eSourceTransferType ) );

    return psTree;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTDerivedRasterBand::GetMinimum( int *pbSuccess )
{
    return GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTDerivedRasterBand::GetMaximum( int *pbSuccess )
{
    return GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                       ComputeRasterMinMax()                          */
/************************************************************************/

CPLErr VRTDerivedRasterBand::ComputeRasterMinMax( int bApproxOK, double* adfMinMax )
{
    return GDALRasterBand::ComputeRasterMinMax( bApproxOK, adfMinMax );
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

CPLErr
VRTDerivedRasterBand::ComputeStatistics( int bApproxOK,
                                         double *pdfMin, double *pdfMax,
                                         double *pdfMean, double *pdfStdDev,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData )

{
    return GDALRasterBand::ComputeStatistics(  bApproxOK,
                                            pdfMin, pdfMax,
                                            pdfMean, pdfStdDev,
                                            pfnProgress, pProgressData );
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTDerivedRasterBand::GetHistogram( double dfMin, double dfMax,
                                           int nBuckets, GUIntBig *panHistogram,
                                           int bIncludeOutOfRange, int bApproxOK,
                                           GDALProgressFunc pfnProgress,
                                           void *pProgressData )

{
    return VRTRasterBand::GetHistogram( dfMin, dfMax,
                                            nBuckets, panHistogram,
                                            bIncludeOutOfRange, bApproxOK,
                                            pfnProgress, pProgressData );
}

/*! @endcond */
