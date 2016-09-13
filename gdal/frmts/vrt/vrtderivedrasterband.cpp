/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of a sourced raster band that derives its raster
 *           by applying an algorithm (GDALDerivedPixelFunc) to the sources.
 * Author:   Pete Nagy
 *
 ******************************************************************************
 * Copyright (c) 2005 Vexcel Corp.
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <map>
#include <vector>
#include <utility>

/*! @cond Doxygen_Suppress */

CPL_CVSID("$Id$");

// #define GDAL_VRT_DISABLE_PYTHON
// #define PYTHONSO_DEFAULT "libpython2.7.so"

#ifndef GDAL_VRT_ENABLE_PYTHON_DEFAULT
#define GDAL_VRT_ENABLE_PYTHON_DEFAULT "NO"
#endif

static std::map<CPLString, GDALDerivedPixelFunc> osMapPixelFunction;
static bool gbHasInitializedPython = false;
static int gnPythonInstanceCounter = 0;
static CPLMutex* ghMutex = NULL;

// Subset of Python API defined as function of pointers
typedef struct PyObject_t PyObject;
#define Py_file_input 257
static void (*Py_SetProgramName)(const char*) = NULL;
static PyObject* (*PyBuffer_FromReadWriteMemory)(void*, size_t) = NULL;
static PyObject* (*PyTuple_New)(size_t) = NULL;
static PyObject* (*PyInt_FromLong)(long) = NULL;
static PyObject* (*PyFloat_FromDouble)(double) = NULL;
static PyObject* (*PyObject_Call)(PyObject*, PyObject*, PyObject*) = NULL;
static void (*Py_IncRef)(PyObject*) = NULL;
static void (*Py_DecRef)(PyObject*) = NULL;
static PyObject* (*PyErr_Occurred)(void) = NULL;
static void (*PyErr_Print)(void) = NULL;
static int (*Py_IsInitialized)(void) = NULL;
static void (*Py_InitializeEx)(int) = NULL;
static void (*PyEval_InitThreads)(void) = NULL;
typedef struct PyThreadState_t PyThreadState;
static PyThreadState* (*PyEval_SaveThread)(void) = NULL;
static void (*PyEval_RestoreThread)(PyThreadState*) = NULL;
static void (*Py_Finalize)(void) = NULL;
static PyObject* (*Py_CompileString)(const char*, const char*, int) = NULL;
static PyObject* (*PyImport_ExecCodeModule)(const char*, PyObject*) = NULL;
static PyObject* (*PyObject_GetAttrString)(PyObject*, const char*) = NULL;
static int (*PyTuple_SetItem)(PyObject *, size_t, PyObject *) = NULL;
static void (*PyObject_Print)(PyObject*,FILE*,int) = NULL;
static PyObject* (*PyString_FromStringAndSize)(const void*, size_t) = NULL;
static PyObject* (*PyImport_ImportModule)(const char*) = NULL;
static int (*PyCallable_Check)(PyObject*) = NULL;
static PyObject* (*PyDict_New)(void) = NULL;
static int (*PyDict_SetItemString)(PyObject *p, const char *key,
                                   PyObject *val) = NULL;
static void (*PyErr_Fetch)(PyObject **poPyType, PyObject **poPyValue,
                           PyObject **poPyTraceback) = NULL;
static void (*PyErr_Clear)(void) = NULL;
static const char* (*PyString_AsString)(PyObject*) = NULL;

typedef int PyGILState_STATE;
static PyGILState_STATE (*PyGILState_Ensure)(void) = NULL;
static void (*PyGILState_Release)(PyGILState_STATE) = NULL;

/* Flags for getting buffers */
#define PyBUF_WRITABLE 0x0001
#define PyBUF_FORMAT 0x0004
#define PyBUF_ND 0x0008
#define PyBUF_STRIDES (0x0010 | PyBUF_ND)
#define PyBUF_INDIRECT (0x0100 | PyBUF_STRIDES)
#define PyBUF_FULL (PyBUF_INDIRECT | PyBUF_WRITABLE | PyBUF_FORMAT)

typedef struct
{
    char big_enough[256];
} Py_buffer;
static int (*PyBuffer_FillInfo)(Py_buffer *view, PyObject *obj, void *buf,
                                size_t len, int readonly, int infoflags) = NULL;
static PyObject* (*PyMemoryView_FromBuffer)(Py_buffer *view) = NULL;



static PyThreadState* gphThreadState = NULL;

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
    if( PyBuffer_FromReadWriteMemory )
    {
        // Python 2
        poPyBuffer = PyBuffer_FromReadWriteMemory(pBuffer, nSize);
    }
    else
    {
        // Python 3
        Py_buffer pybuffer;
        if( PyBuffer_FillInfo(&pybuffer, NULL, (char*)pBuffer,
                              nSize,
                              0, PyBUF_FULL) != 0)
        {
            return NULL;
        }
        poPyBuffer = PyMemoryView_FromBuffer(&pybuffer);
    }
    PyObject* pArgsCreateArray = PyTuple_New(4);
    PyTuple_SetItem(pArgsCreateArray, 0, poPyBuffer);
    const char* pszDataType = NULL;
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
                PyString_FromStringAndSize(pszDataType, strlen(pszDataType)));
    PyTuple_SetItem(pArgsCreateArray, 2, PyInt_FromLong(nHeight));
    PyTuple_SetItem(pArgsCreateArray, 3, PyInt_FromLong(nWidth));
    PyObject* poNumpyArray = PyObject_Call(pCreateArray, pArgsCreateArray, NULL);
    Py_DecRef(pArgsCreateArray);
    if (PyErr_Occurred())
        PyErr_Print();
    return poNumpyArray;
}


/* MinGW32 might define HAVE_DLFCN_H, so skip the unix implementation */
#if defined(HAVE_DLFCN_H) && !defined(WIN32)

#include <dlfcn.h>

typedef void* LibraryHandle;

#define LOAD_NOCHECK_WITH_NAME(libHandle, x, name) \
    do { \
            void* ptr = dlsym(libHandle, name); \
            memcpy(&x, &ptr, sizeof(void*)); \
    } while(0)

#elif defined(WIN32)

#include <windows.h>
#include <psapi.h>

typedef HMODULE LibraryHandle;

#define LOAD_NOCHECK_WITH_NAME(libHandle, x, name) \
    do { \
            FARPROC ptr = GetProcAddress(libHandle, name); \
            memcpy(&x, &ptr, sizeof(void*)); \
    } while(0)

#endif

#define STRINGIFY(x) #x

#define LOAD_NOCHECK(libHandle, x) LOAD_NOCHECK_WITH_NAME(libHandle, x, STRINGIFY(x))
#define LOAD_WITH_NAME(libHandle, x, name) \
    do { \
            LOAD_NOCHECK_WITH_NAME(libHandle, x, name); \
            if (!x) \
            { \
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", name); \
                return false; \
            } \
    } while(0)
#define LOAD(libHandle, x) LOAD_WITH_NAME(libHandle, x, STRINGIFY(x))

/************************************************************************/
/*                          LoadPythonAPI()                             */
/************************************************************************/

/** Load the subset of the Python C API that we need */
static bool LoadPythonAPI()
{
    CPLMutexHolder oHolder(&ghMutex);

    static bool bInit = false;
    if( bInit )
        return true;

#ifdef LOAD_NOCHECK_WITH_NAME
    // The static here is just to avoid Coverity warning about resource leak.
    static LibraryHandle libHandle = NULL;

    const char* pszPythonSO = CPLGetConfigOption("PYTHONSO", NULL);
#if defined(HAVE_DLFCN_H) && !defined(WIN32)
    if( pszPythonSO != NULL )
    {
        // coverity[tainted_string]
        libHandle = dlopen(pszPythonSO, RTLD_NOW | RTLD_GLOBAL);
        if( libHandle == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot load %s",
                     pszPythonSO);
            return false;
        }
        if( dlsym(libHandle, "Py_SetProgramName") == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find Py_SetProgramName symbol in %s",
                     pszPythonSO);
            return false;
        }
    }
    else
    {
        libHandle = dlopen(NULL, RTLD_LAZY);
        // First try in the current process in case the python interpret would
        // be already loaded
        if( libHandle != NULL &&
            dlsym(libHandle, "Py_SetProgramName") != NULL )
        {
            CPLDebug("VRT", "Current process has python symbols loaded");
        }
        else
        {
#if defined(__MACH__) && defined(__APPLE__)
#define SO_EXT "dylib"
#else
#define SO_EXT "so"
#endif

#ifdef PYTHONSO_DEFAULT
            libHandle = dlopen(PYTHONSO_DEFAULT, RTLD_NOW | RTLD_GLOBAL);
            if( !libHandle )
            {
                CPLDebug("VRT", "%s found", PYTHONSO_DEFAULT);
            }
#else
            libHandle = NULL;
#endif

            // Otherwise probe a few known objects
            const char* const apszPythonSO[] = { "libpython2.7." SO_EXT,
                                                 "libpython2.6." SO_EXT,
                                                 "libpython3.4m." SO_EXT,
                                                 "libpython3.5m." SO_EXT,
                                                 "libpython3.6m." SO_EXT,
                                                 "libpython3.3." SO_EXT,
                                                 "libpython3.2." SO_EXT };
            for( size_t i = 0; libHandle == NULL &&
                                i < CPL_ARRAYSIZE(apszPythonSO); ++i )
            {
                CPLDebug("VRT", "Trying %s", apszPythonSO[i]);
                libHandle = dlopen(apszPythonSO[i], RTLD_NOW | RTLD_GLOBAL);
                if( libHandle != NULL )
                    CPLDebug("VRT", "... success");
            }
        }
    }
#elif defined(WIN32)
    if( pszPythonSO != NULL )
    {
        UINT        uOldErrorMode;
        /* Avoid error boxes to pop up (#5211, #5525) */
        uOldErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX |
                                     SEM_FAILCRITICALERRORS);
        libHandle = LoadLibrary(pszPythonSO);
        SetErrorMode(uOldErrorMode);

        if( libHandle == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot load %s",
                     pszPythonSO);
            return false;
        }
        if( GetProcAddress(libHandle, "Py_SetProgramName") == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find Py_SetProgramName symbol in %s",
                     pszPythonSO);
            return false;
        }
    }
    else
    {
        HANDLE hProcess = GetCurrentProcess();
        HMODULE ahModules[100];
        DWORD nSizeNeeded = 0;
#if defined(_M_X64)
        EnumProcessModulesEx(hProcess, ahModules, sizeof(ahModules),
                             &nSizeNeeded, LIST_MODULES_DEFAULT);
#else
        EnumProcessModules(hProcess, ahModules, sizeof(ahModules),
                           &nSizeNeeded);
#endif
        int nModules = MIN(100, nSizeNeeded / sizeof(HMODULE));
        for(int i=0;i<nModules;i++)
        {
            if( GetProcAddress(ahModules[i], "Py_SetProgramName") )
            {
                libHandle = ahModules[i];
                CPLDebug("VRT", "Current process has python symbols loaded");
                break;
            }
        }
        if( libHandle == NULL )
        {
            const char* const apszPythonSO[] = { "python27.dll",
                                                "python26.dll",
                                                "python34.dll",
                                                "python35.dll",
                                                "python36.dll",
                                                "python33.dll",
                                                "python32.dll" };
            UINT        uOldErrorMode;
            uOldErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX |
                                         SEM_FAILCRITICALERRORS);
#ifdef PYTHONSO_DEFAULT
            libHandle = LoadLibrary(PYTHONSO_DEFAULT);
            if( !libHandle )
            {
                CPLDebug("VRT", "%s found", PYTHONSO_DEFAULT);
            }
#endif
            for( size_t i = 0; libHandle == NULL &&
                                i < CPL_ARRAYSIZE(apszPythonSO); ++i )
            {
                CPLDebug("VRT", "Trying %s", apszPythonSO[i]);
                libHandle = LoadLibrary(apszPythonSO[i]);
                if( libHandle != NULL )
                    CPLDebug("VRT", "... success");
            }
            SetErrorMode(uOldErrorMode);
        }
    }
#endif
    if( !libHandle )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find python/libpython. You can set the PYTHONSO "
                 "configuration option to point to the a python .so/.dll/.dylib");
        return false;
    }

    LOAD(libHandle, Py_SetProgramName);
    LOAD_NOCHECK(libHandle, PyBuffer_FromReadWriteMemory);
    LOAD_NOCHECK(libHandle, PyBuffer_FillInfo);
    LOAD_NOCHECK(libHandle, PyMemoryView_FromBuffer);
    if( PyBuffer_FromReadWriteMemory == NULL &&
        (PyBuffer_FillInfo == NULL || PyMemoryView_FromBuffer == NULL) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find PyBuffer_FillInfo or "
                 "PyBuffer_FillInfo+PyMemoryView_FromBuffer\n");
        return false;
    }
    LOAD(libHandle, PyTuple_New);
    if( PyBuffer_FromReadWriteMemory )
    {
        // Python 2
        LOAD(libHandle, PyInt_FromLong);
        LOAD(libHandle, PyString_FromStringAndSize);
        LOAD(libHandle, PyString_AsString);
    }
    else
    {
        // Python 3
        LOAD_WITH_NAME(libHandle, PyInt_FromLong, "PyLong_FromLong");
        LOAD_WITH_NAME(libHandle, PyString_FromStringAndSize,
                                        "PyBytes_FromStringAndSize");
        LOAD_WITH_NAME(libHandle, PyString_AsString, "PyBytes_AsString");
    }
    LOAD(libHandle, PyFloat_FromDouble);
    LOAD(libHandle, PyObject_Call);
    LOAD(libHandle, Py_IncRef);
    LOAD(libHandle, Py_DecRef);
    LOAD(libHandle, PyErr_Occurred);
    LOAD(libHandle, PyErr_Print);
    LOAD(libHandle, Py_IsInitialized);
    LOAD(libHandle, Py_InitializeEx);
    LOAD(libHandle, PyEval_InitThreads);
    LOAD(libHandle, PyEval_SaveThread);
    LOAD(libHandle, PyEval_RestoreThread);
    LOAD(libHandle, Py_Finalize);
    LOAD(libHandle, Py_CompileString);
    LOAD(libHandle, PyImport_ExecCodeModule);
    LOAD(libHandle, PyObject_GetAttrString);
    LOAD(libHandle, PyTuple_SetItem);
    LOAD(libHandle, PyObject_Print);
    LOAD(libHandle, PyImport_ImportModule);
    LOAD(libHandle, PyCallable_Check);
    LOAD(libHandle, PyDict_New);
    LOAD(libHandle, PyDict_SetItemString);
    LOAD(libHandle, PyGILState_Ensure);
    LOAD(libHandle, PyGILState_Release);
    LOAD(libHandle, PyErr_Fetch);
    LOAD(libHandle, PyErr_Clear);

#else // LOAD_NOCHECK_WITH_NAME
    CPLError(CE_Failure, CPLE_AppDefined,
             "This platform doesn't support dynamic loading of libraries")
    return false;
#endif // LOAD_NOCHECK_WITH_NAME

    bInit = true;
    return bInit;
}

/************************************************************************/
/*                      GetPyExceptionString()                          */
/************************************************************************/

static CPLString GetPyExceptionString()
{
    PyObject *poPyType = NULL;
    PyObject *poPyValue = NULL;
    PyObject *poPyTraceback = NULL;

    PyErr_Fetch(&poPyType, &poPyValue, &poPyTraceback);
    if( poPyType )
        Py_IncRef(poPyType);
    if( poPyValue )
        Py_IncRef(poPyValue);
    if( poPyTraceback )
        Py_IncRef(poPyTraceback);

    // This is a mess. traceback.format_exception/format_exception_only
    // sometimes throw exceptions themselves !
    CPLString osPythonCode(
        "import traceback\n"
        "\n"
        "def GDALFormatException2(etype, value):\n"
        "    try:\n"
        "       return ''.join(traceback.format_exception_only(etype, value)).encode('UTF-8')\n"
        "    except:\n"
        "       return (str(etype) + ', ' + str(value)).encode('UTF-8')\n"
        "\n"
        "def GDALFormatException3(etype, value, tb):\n"
        //"    print(etype, value, tb)\n"
        "    try:\n"
        "       return ''.join(traceback.format_exception(etype, value, tb)).encode('UTF-8')\n"
        "    except:\n"
        "       return (str(etype) + ', ' + str(value)).encode('UTF-8')\n");

    CPLString osRet("An exception occured in exception formatting code...");

    static int nCounter = 0;
    CPLString osModuleName( CPLSPrintf("gdal_exception_%d", nCounter));
    PyObject* poCompiledString = Py_CompileString(osPythonCode,
                                                  osModuleName, Py_file_input);
    if( poCompiledString == NULL || PyErr_Occurred() )
    {
        PyErr_Print();
    }
    else
    {
        PyObject* poModule =
            PyImport_ExecCodeModule(osModuleName, poCompiledString);
        CPLAssert(poModule);

        Py_DecRef(poCompiledString);

        PyObject* poPyGDALFormatException2 = PyObject_GetAttrString(poModule,
                                                "GDALFormatException2" );
        CPLAssert(poPyGDALFormatException2);

        PyObject* poPyGDALFormatException3 = PyObject_GetAttrString(poModule,
                                                "GDALFormatException3" );
        CPLAssert(poPyGDALFormatException3);

        Py_DecRef(poModule);

        PyObject* pyArgs = PyTuple_New( poPyTraceback ? 3 : 2);
        PyTuple_SetItem(pyArgs, 0, poPyType);
        PyTuple_SetItem(pyArgs, 1, poPyValue);
        if( poPyTraceback )
            PyTuple_SetItem(pyArgs, 2, poPyTraceback );
        PyObject* poPyRet = PyObject_Call(
            poPyTraceback ? poPyGDALFormatException3 : poPyGDALFormatException2,
            pyArgs, NULL );
        Py_DecRef(pyArgs);


        if( PyErr_Occurred() )
        {
            osRet = "An exception occured in exception formatting code...";
            PyErr_Print();
        }
        else
        {
            osRet = PyString_AsString(poPyRet);
            Py_DecRef(poPyRet);
        }

        Py_DecRef(poPyGDALFormatException2);
        Py_DecRef(poPyGDALFormatException3);
    }

    if( poPyType )
        Py_DecRef(poPyType);
    if( poPyValue )
        Py_DecRef(poPyValue);
    if( poPyTraceback )
        Py_DecRef(poPyTraceback);

    return osRet;
}

/************************************************************************/
/* ==================================================================== */
/*                     VRTDerivedRasterBandPrivateData                  */
/* ==================================================================== */
/************************************************************************/

class VRTDerivedRasterBandPrivateData
{
    public:
        CPLString m_osCode;
        CPLString m_osLanguage;
        int       m_nBufferRadius;
        PyObject* m_poGDALCreateNumpyArray;
        PyObject* m_poUserFunction;
        bool      m_bPythonInitializationDone;
        bool      m_bPythonInitializationSuccess;
        bool      m_bExclusiveLock;
        bool      m_bFirstTime;
        std::vector< std::pair<CPLString,CPLString> > m_oFunctionArgs;

        VRTDerivedRasterBandPrivateData():
            m_osLanguage("C"),
            m_nBufferRadius(0),
            m_poGDALCreateNumpyArray(NULL),
            m_poUserFunction(NULL),
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

            CPLMutexHolder oHolder(&ghMutex);
            gnPythonInstanceCounter --;
        }
};

/************************************************************************/
/* ==================================================================== */
/*                            VRT_GIL_Holder                            */
/* ==================================================================== */
/************************************************************************/

class VRT_GIL_Holder
{
        bool             m_bExclusiveLock;
        PyGILState_STATE m_eState;

    public:

        VRT_GIL_Holder(bool bExclusiveLock);
        virtual ~VRT_GIL_Holder();
};

VRT_GIL_Holder::VRT_GIL_Holder(bool bExclusiveLock):
    m_bExclusiveLock(bExclusiveLock)
{
    if( bExclusiveLock )
    {
        if( ghMutex )
            CPLAcquireMutex( ghMutex, 1000.0 );
    }
    m_eState = PyGILState_Ensure();
}

VRT_GIL_Holder::~VRT_GIL_Holder()
{
    PyGILState_Release(m_eState);
    if( m_bExclusiveLock )
    {
        if( ghMutex )
            CPLReleaseMutex( ghMutex );
    }
    else
    {
    }
}

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
    m_poPrivate(NULL),
    pszFuncName(NULL),
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
    m_poPrivate(NULL),
    pszFuncName(NULL),
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
    if( ghMutex )
        CPLDestroyMutex(ghMutex);
    ghMutex = NULL;

    if( gnPythonInstanceCounter == 0 && gbHasInitializedPython &&
        CPLTestBool(CPLGetConfigOption("GDAL_VRT_ENABLE_PYTHON_FINALIZE",
                                       "YES")) )
    {
        // We call Py_Finalize at driver destruction, rather at dataset
        // destruction, since numpy crashes when it is reloaded after the next
        // Py_Initialize
        CPLDebug("VRT", "Py_Finalize() = %p", Py_Finalize);
        PyEval_RestoreThread(gphThreadState);
        Py_Finalize();
        gbHasInitializedPython = false;
        gphThreadState = NULL;
    }
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
 * @param pszFuncName Name used to access pixel function
 * @param pfnNewFunction Pixel function associated with name.  An
 *  existing pixel function registered with the same name will be
 *  replaced with the new one.
 *
 * @return CE_None, invalid (NULL) parameters are currently ignored.
 */
CPLErr CPL_STDCALL
GDALAddDerivedBandPixelFunc( const char *pszFuncName,
                             GDALDerivedPixelFunc pfnNewFunction )
{
    if( pszFuncName == NULL || pszFuncName[0] == '\0' ||
        pfnNewFunction == NULL )
    {
      return CE_None;
    }

    osMapPixelFunction[pszFuncName] = pfnNewFunction;

    return CE_None;
}

/*! @cond Doxygen_Suppress */

/**
 * This adds a pixel function to the global list of available pixel
 * functions for derived bands.
 *
 * This is the same as the c function GDALAddDerivedBandPixelFunc()
 *
 * @param pszFuncName Name used to access pixel function
 * @param pfnNewFunction Pixel function associated with name.  An
 *  existing pixel function registered with the same name will be
 *  replaced with the new one.
 *
 * @return CE_None, invalid (NULL) parameters are currently ignored.
 */
CPLErr
VRTDerivedRasterBand::AddPixelFunction(
    const char *pszFuncName, GDALDerivedPixelFunc pfnNewFunction )
{
    return GDALAddDerivedBandPixelFunc(pszFuncName, pfnNewFunction);
}

/************************************************************************/
/*                           GetPixelFunction()                         */
/************************************************************************/

/**
 * Get a pixel function previously registered using the global
 * AddPixelFunction.
 *
 * @param pszFuncName The name associated with the pixel function.
 *
 * @return A derived band pixel function, or NULL if none have been
 * registered for pszFuncName.
 */
GDALDerivedPixelFunc
VRTDerivedRasterBand::GetPixelFunction( const char *pszFuncName )
{
    if( pszFuncName == NULL || pszFuncName[0] == '\0' )
    {
        return NULL;
    }

    std::map<CPLString, GDALDerivedPixelFunc>::iterator oIter =
        osMapPixelFunction.find(pszFuncName);

    if( oIter == osMapPixelFunction.end())
        return NULL;

    return oIter->second;
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
    pszFuncName = CPLStrdup( pszFuncNameIn );
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

#ifndef GDAL_VRT_DISABLE_PYTHON
    const char* pszPythonEnabled =
                            CPLGetConfigOption("GDAL_VRT_ENABLE_PYTHON", NULL);
#else
    const char* pszPythonEnabled = "NO";
#endif
    CPLString osPythonFunction( pszFuncName ? pszFuncName : "" );

#ifdef disabled_because_this_is_probably_broken_by_design
    // See https://lwn.net/Articles/574215/
    // and http://nedbatchelder.com/blog/201206/eval_really_is_dangerous.html
    if( EQUAL(pszPythonEnabled ? pszPythonEnabled :
                                GDAL_VRT_ENABLE_PYTHON_DEFAULT, "IF_SAFE") )
    {
        bool bSafe = true;
        // If the function comes from another module, then we don't know
        if( osPythonFunction.find('.') != std::string::npos )
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
    else
#endif //disabled_because_this_is_probably_broken_by_design


    if( !CPLTestBool(pszPythonEnabled ? pszPythonEnabled :
                                            GDAL_VRT_ENABLE_PYTHON_DEFAULT) )
    {
        if( pszPythonEnabled == NULL )
        {
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

    if( !LoadPythonAPI() )
        return false;

    // Make sure the python interpreter is initialized
    {
        CPLMutexHolder oHolder(&ghMutex);
        int bIsInitialized = Py_IsInitialized();
        if( !bIsInitialized)
        {
            gbHasInitializedPython = true;
            Py_InitializeEx(0);
            CPLDebug("VRT", "Py_Initialize()");
            PyEval_InitThreads();
            gphThreadState = PyEval_SaveThread();
        }
        gnPythonInstanceCounter ++;
    }

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
    VRT_GIL_Holder oHolder(bUseExclusiveLock);

    // As we don't want to depend on numpy C API/ABI, we use a trick to build
    // a numpy array object. We define a Python function to which we pass a
    // Python buffer oject.

    // We need to build a unique module name, otherwise this will crash in
    // multithreaded use cases.
    CPLString osModuleName( CPLSPrintf("gdal_vrt_module_%p", this) );
    PyObject* poCompiledString = Py_CompileString(
        ("import numpy\n"
        "def GDALCreateNumpyArray(buffer, dtype, height, width):\n"
        "    return numpy.frombuffer(buffer, dtype.decode('ascii'))."
                                                "reshape([height, width])\n"
        "\n" + m_poPrivate->m_osCode).c_str(),
        osModuleName, Py_file_input);
    if( poCompiledString == NULL || PyErr_Occurred() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Couldn't compile code:\n%s",
                 GetPyExceptionString().c_str());
        return false;
    }
    PyObject* poModule =
        PyImport_ExecCodeModule(osModuleName, poCompiledString);
    Py_DecRef(poCompiledString);

    if( poModule == NULL || PyErr_Occurred() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
        return false;
    }

    // Fetch user computation function
    size_t nIdxDot = osPythonFunction.rfind(".");
    if( nIdxDot != std::string::npos )
    {
        CPLString osUserModule = osPythonFunction.substr(0, nIdxDot);
        PyObject* poUserModule = PyImport_ImportModule(osUserModule);
        if (poUserModule == NULL || PyErr_Occurred())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
            Py_DecRef(poModule);
            return false;
        }
        m_poPrivate->m_poUserFunction = PyObject_GetAttrString(poUserModule,
                                osPythonFunction.substr(nIdxDot+1).c_str() );
        Py_DecRef(poUserModule);
    }
    else
    {
        m_poPrivate->m_poUserFunction = PyObject_GetAttrString(poModule,
                                            osPythonFunction );
    }
    if (m_poPrivate->m_poUserFunction == NULL || PyErr_Occurred())
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
    if (m_poPrivate->m_poGDALCreateNumpyArray == NULL || PyErr_Occurred())
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

    int typesize = GDALGetDataTypeSizeBytes(eBufType);
    if( GDALGetDataTypeSize(eBufType) % 8 > 0 ) typesize++;
    GDALDataType eSrcType = eSourceTransferType;
    if( eSrcType == GDT_Unknown || eSrcType >= GDT_TypeCount ) {
        eSrcType = eBufType;
    }
    const int sourcesize = GDALGetDataTypeSizeBytes(eSrcType);

/* -------------------------------------------------------------------- */
/*      Initialize the buffer to some background value. Use the         */
/*      nodata value if available.                                      */
/* -------------------------------------------------------------------- */
    if( bSkipBufferInitialization )
    {
        // Do nothing
    }
    else if( nPixelSpace == typesize &&
        (!m_bNoDataValueSet || m_dfNoDataValue == 0) ) {
        memset( pData, 0,
                static_cast<size_t>(nBufXSize * nBufYSize * nPixelSpace) );
    }
    else if( m_bNoDataValueSet )
    {
        double dfWriteValue = 0.0;
        if( m_bNoDataValueSet )
            dfWriteValue = m_dfNoDataValue;

        for( int iLine = 0; iLine < nBufYSize; iLine++ )
        {
            GDALCopyWords(
                &dfWriteValue, GDT_Float64, 0,
                reinterpret_cast<GByte *>( pData ) + nLineSpace * iLine,
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
    GDALDerivedPixelFunc pfnPixelFunc = NULL;

    if( EQUAL(m_poPrivate->m_osLanguage, "C") )
    {
        pfnPixelFunc = VRTDerivedRasterBand::GetPixelFunction(pszFuncName);
        if( pfnPixelFunc == NULL )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                    "VRTDerivedRasterBand::IRasterIO:"
                    "Derived band pixel function '%s' not registered.",
                    this->pszFuncName) ;
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
    if( nBufferRadius > (INT_MAX - nBufXSize) / 2 ||
        nBufferRadius > (INT_MAX - nBufYSize) / 2 )
    {
        return CE_Failure;
    }
    const int nExtBufXSize = nBufXSize + 2 * nBufferRadius;
    const int nExtBufYSize = nBufYSize + 2 * nBufferRadius;
    void **pBuffers
        = reinterpret_cast<void **>( CPLMalloc(sizeof(void *) * nSources) );
    for( int iSource = 0; iSource < nSources; iSource++ ) {
        pBuffers[iSource] =
            VSI_MALLOC_VERBOSE(sourcesize * nExtBufXSize * nExtBufYSize);
        if( pBuffers[iSource] == NULL )
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
            memset( pBuffers[iSource], 0, sourcesize * nExtBufXSize * nExtBufYSize );
        }
        else
        {
            GDALCopyWords( &m_dfNoDataValue, GDT_Float64, 0,
                           reinterpret_cast<GByte *>( pBuffers[iSource] ),
                           eSrcType, sourcesize,
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
        nXSizeExt = MIN(static_cast<int>(sExtraArg.dfXSize + 0.5),
                        nRasterXSize - nXOffExt);
        nYSizeExt = MIN(static_cast<int>(sExtraArg.dfYSize + 0.5),
                        nRasterYSize - nYOffExt);
    }

    // Load values for sources into packed buffers.
    CPLErr eErr = CE_None;
    for( int iSource = 0; iSource < nSources && eErr == CE_None; iSource++ ) {
        GByte* pabyBuffer = reinterpret_cast<GByte*>(pBuffers[iSource]);
        eErr = reinterpret_cast<VRTSource *>( papoSources[iSource] )->RasterIO(
            nXOffExt, nYOffExt, nXSizeExt, nYSizeExt,
            pabyBuffer + (nYShiftInBuffer * nExtBufXSize +
                                            nXShiftInBuffer) * sourcesize,
            nExtBufXSizeReq, nExtBufYSizeReq,
            eSrcType,
            sourcesize,
            sourcesize * nExtBufXSize,
            &sExtraArg );

        // Extend first lines
        for( int iY = 0; iY < nYShiftInBuffer; iY++ )
        {
            memcpy( pabyBuffer + iY * nExtBufXSize * sourcesize,
                    pabyBuffer + nYShiftInBuffer * nExtBufXSize * sourcesize,
                    nExtBufXSize * sourcesize );
        }
        // Extend last lines
        for( int iY = nYShiftInBuffer + nExtBufYSizeReq; iY < nExtBufYSize; iY++ )
        {
            memcpy( pabyBuffer + iY * nExtBufXSize * sourcesize,
                    pabyBuffer + (nYShiftInBuffer + nExtBufYSizeReq - 1) *
                                                    nExtBufXSize * sourcesize,
                    nExtBufXSize * sourcesize );
        }
        // Extend first cols
        if( nXShiftInBuffer )
        {
            for( int iY = 0; iY < nExtBufYSize; iY ++ )
            {
                for( int iX = 0; iX < nXShiftInBuffer; iX++ )
                {
                    memcpy( pabyBuffer + (iY * nExtBufXSize + iX) * sourcesize,
                            pabyBuffer + (iY * nExtBufXSize +
                                                nXShiftInBuffer) * sourcesize,
                            sourcesize );
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
                    memcpy( pabyBuffer + (iY * nExtBufXSize + iX) * sourcesize,
                            pabyBuffer + (iY * nExtBufXSize + nXShiftInBuffer +
                                            nExtBufXSizeReq - 1) * sourcesize,
                            sourcesize );
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
        GDALDataType eDataTypeModified = eDataType;
        if( eBufType == GDT_CInt16 || eBufType == GDT_CInt32 )
        {
            eDataTypeModified = GDT_CFloat64;
        }

        if( !InitializePython() )
            goto end;

        GByte* pabyTmpBuffer = reinterpret_cast<GByte*>(VSI_CALLOC_VERBOSE(
                        static_cast<size_t>(nExtBufXSize) * nExtBufYSize,
                        GDALGetDataTypeSizeBytes(eDataTypeModified)));
        if( !pabyTmpBuffer )
            goto end;

        {
        const bool bUseExclusiveLock = m_poPrivate->m_bExclusiveLock ||
                    ( m_poPrivate->m_bFirstTime &&
                    m_poPrivate->m_osCode.find("@jit") != std::string::npos);
        m_poPrivate->m_bFirstTime = false;
        VRT_GIL_Holder oHolder(bUseExclusiveLock);

        // Prepare target numpy array
        PyObject* poPyDstArray = GDALCreateNumpyArray(
                                    m_poPrivate->m_poGDALCreateNumpyArray,
                                    pabyTmpBuffer,
                                    eDataTypeModified,
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
            GByte* pabyBuffer = reinterpret_cast<GByte*>(pBuffers[i]);
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
        PyTuple_SetItem(pyArgs, 2, PyInt_FromLong(nXOff));
        PyTuple_SetItem(pyArgs, 3, PyInt_FromLong(nYOff));
        PyTuple_SetItem(pyArgs, 4, PyInt_FromLong(nXSize));
        PyTuple_SetItem(pyArgs, 5, PyInt_FromLong(nYSize));
        PyTuple_SetItem(pyArgs, 6, PyInt_FromLong(nRasterXSize));
        PyTuple_SetItem(pyArgs, 7, PyInt_FromLong(nRasterYSize));
        PyTuple_SetItem(pyArgs, 8, PyInt_FromLong(nBufferRadius));

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
                PyString_FromStringAndSize(pszValue, strlen(pszValue)));
        }

        // Call user function
        PyObject* pRetValue = PyObject_Call(
                                        m_poPrivate->m_poUserFunction,
                                        pyArgs, pyKwargs);

        Py_DecRef(pyArgs);
        Py_DecRef(pyKwargs);

        if (PyErr_Occurred())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s", GetPyExceptionString().c_str());
        }
        else
        {
            eErr = CE_None;
        }
        if( pRetValue )
            Py_DecRef(pRetValue);

        } // End of GIL section

        // Copy numpy destination array to user buffer
        for( int iY = 0; iY < nBufYSize; iY++ )
        {
            size_t nSrcOffset = (static_cast<size_t>(iY + nBufferRadius) *
                nExtBufXSize + nBufferRadius) *
                GDALGetDataTypeSizeBytes(eDataTypeModified);
            GDALCopyWords(pabyTmpBuffer + nSrcOffset,
                          eDataTypeModified,
                          GDALGetDataTypeSizeBytes(eDataTypeModified),
                          reinterpret_cast<GByte*>(pData) + iY * nLineSpace,
                          eBufType,
                          static_cast<int>(nPixelSpace),
                          nBufXSize);
        }
        VSIFree(pabyTmpBuffer);
    }
    else if( eErr == CE_None && pfnPixelFunc != NULL ) {
        eErr = pfnPixelFunc( reinterpret_cast<void **>( pBuffers ), nSources,
                             pData, nBufXSize, nBufYSize,
                             eSrcType, eBufType, static_cast<int>(nPixelSpace),
                             static_cast<int>(nLineSpace) );
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
    if( pdfDataPct != NULL )
        *pdfDataPct = -1.0;
    return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED | GDAL_DATA_COVERAGE_STATUS_DATA;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTDerivedRasterBand::XMLInit( CPLXMLNode *psTree,
                                      const char *pszVRTPath )

{
    const CPLErr eErr = VRTSourcedRasterBand::XMLInit( psTree, pszVRTPath );
    if( eErr != CE_None )
        return eErr;

    // Read derived pixel function type.
    SetPixelFunctionName( CPLGetXMLValue( psTree, "PixelFunctionType", NULL ) );
    if( pszFuncName == NULL || EQUAL(pszFuncName, "") )
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
    if( m_poPrivate->m_nBufferRadius < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid Buffer");
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
    if( psArgs != NULL )
    {
        if( !EQUAL(m_poPrivate->m_osLanguage, "Python") )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "PixelFunctionArguments can only be used with Python");
            return CE_Failure;
        }
        for( CPLXMLNode* psIter = psArgs->psChild;
                         psIter != NULL;
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
    const char *pszTypeName = CPLGetXMLValue(psTree, "SourceTransferType", NULL);
    if( pszTypeName != NULL )
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
    if( pszFuncName != NULL && strlen(pszFuncName) > 0 )
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
