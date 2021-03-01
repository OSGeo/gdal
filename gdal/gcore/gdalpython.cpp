/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Python interface
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2017-2019, Even Rouault, <even dot rouault at spatialys dot com>
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

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_spawn.h"
#include "gdalpython.h"

#include <algorithm>
#include <mutex>
#include <vector>

using namespace GDALPy;

typedef struct PyThreadState_t PyThreadState;

static PyThreadState* (*PyEval_SaveThread)(void) = nullptr;
static void (*PyEval_RestoreThread)(PyThreadState*) = nullptr;
static void (*Py_Finalize)(void) = nullptr;
static void (*Py_InitializeEx)(int) = nullptr;
static void (*PyEval_InitThreads)(void) = nullptr;
static PyObject* (*Py_CompileStringExFlags)(const char*, const char*, int, void*, int) = nullptr;

static std::mutex gMutex;
static bool gbHasInitializedPython = false;
static PyThreadState* gphThreadState = nullptr;

// Emulate Py_CompileString with Py_CompileStringExFlags
// Probably just a temporary measure for a bug of Python 3.8.0 on Windows
// https://bugs.python.org/issue37633
static PyObject* GDAL_Py_CompileString(const char *str, const char *filename, int start)
{
    return Py_CompileStringExFlags(str, filename, start, nullptr, -1);
}

namespace GDALPy
{
    int (*Py_IsInitialized)(void) = nullptr;
    PyGILState_STATE (*PyGILState_Ensure)(void) = nullptr;
    void (*PyGILState_Release)(PyGILState_STATE) = nullptr;
    void (*Py_SetProgramName)(const char*) = nullptr;
    PyObject* (*PyObject_Type)(PyObject*) = nullptr;
    int (*PyObject_IsInstance)(PyObject*, PyObject*) = nullptr;
    PyObject* (*PyTuple_New)(size_t) = nullptr;
    PyObject* (*PyBool_FromLong)(long) = nullptr;
    PyObject* (*PyLong_FromLong)(long) = nullptr;
    long (*PyLong_AsLong)(PyObject *) = nullptr;
    PyObject* (*PyLong_FromLongLong)(GIntBig) = nullptr;
    GIntBig (*PyLong_AsLongLong)(PyObject *) = nullptr;
    PyObject* (*PyFloat_FromDouble)(double) = nullptr;
    double (*PyFloat_AsDouble)(PyObject*) = nullptr;
    PyObject* (*PyObject_Call)(PyObject*, PyObject*, PyObject*) = nullptr;
    PyObject* (*PyObject_GetIter)(PyObject*) = nullptr;
    PyObject* (*PyIter_Next)(PyObject*) = nullptr;
    void (*Py_IncRef)(PyObject*) = nullptr;
    void (*Py_DecRef)(PyObject*) = nullptr;
    PyObject* (*PyErr_Occurred)(void) = nullptr;
    void (*PyErr_Print)(void) = nullptr;

    PyObject* (*Py_CompileString)(const char*, const char*, int) = nullptr;
    PyObject* (*PyImport_ExecCodeModule)(const char*, PyObject*) = nullptr;
    int (*PyObject_HasAttrString)(PyObject*, const char*) = nullptr;
    PyObject* (*PyObject_GetAttrString)(PyObject*, const char*) = nullptr;
    int (*PyObject_SetAttrString)(PyObject*, const char*, PyObject*) = nullptr;
    int (*PyTuple_SetItem)(PyObject *, size_t, PyObject *) = nullptr;
    void (*PyObject_Print)(PyObject*,FILE*,int) = nullptr;
    Py_ssize_t (*PyBytes_Size)(PyObject *) = nullptr;
    const char* (*PyBytes_AsString)(PyObject*) = nullptr;
    PyObject* (*PyBytes_FromStringAndSize)(const void*, size_t) = nullptr;
    PyObject* (*PyUnicode_FromString)(const char*) = nullptr;
    PyObject* (*PyUnicode_AsUTF8String)(PyObject *) = nullptr;
    PyObject* (*PyImport_ImportModule)(const char*) = nullptr;
    int (*PyCallable_Check)(PyObject*) = nullptr;
    PyObject* (*PyDict_New)(void) = nullptr;
    int (*PyDict_SetItemString)(PyObject *p, const char *key,
                                    PyObject *val) = nullptr;
    int (*PyDict_Next)(PyObject *p, size_t *, PyObject **, PyObject **) = nullptr;
    PyObject* (*PyDict_GetItemString)(PyObject *p, const char *key) = nullptr;
    PyObject* (*PyList_New)(Py_ssize_t) = nullptr;
    int (*PyList_SetItem)(PyObject *, Py_ssize_t , PyObject *) = nullptr;
    int (*PyArg_ParseTuple)(PyObject *, const char *, ...) = nullptr;

    int (*PySequence_Check)(PyObject *o) = nullptr;
    Py_ssize_t (*PySequence_Size)(PyObject *o) = nullptr;
    PyObject* (*PySequence_GetItem)(PyObject *o, Py_ssize_t i) = nullptr;

    void (*PyErr_Fetch)(PyObject **poPyType, PyObject **poPyValue,
                            PyObject **poPyTraceback) = nullptr;
    void (*PyErr_Clear)(void) = nullptr;
    const char* (*Py_GetVersion)(void) = nullptr;

    int (*PyBuffer_FillInfo)(Py_buffer *view, PyObject *obj, void *buf,
                                    size_t len, int readonly, int infoflags) = nullptr;
    PyObject* (*PyMemoryView_FromBuffer)(Py_buffer *view) = nullptr;

    PyObject * (*PyModule_Create2)(struct PyModuleDef*, int) = nullptr;
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

#if defined(LOAD_NOCHECK_WITH_NAME) && defined(HAVE_DLFCN_H) && !defined(WIN32)
static LibraryHandle libHandleStatic = nullptr;
#endif

/** Load the subset of the Python C API that we need */
static bool LoadPythonAPI()
{
    static bool bInit = false;
    if( bInit )
        return true;

#ifdef LOAD_NOCHECK_WITH_NAME
    // The static here is just to avoid Coverity warning about resource leak.
    LibraryHandle libHandle = nullptr;

    const char* pszPythonSO = CPLGetConfigOption("PYTHONSO", nullptr);
#if defined(HAVE_DLFCN_H) && !defined(WIN32)

    // First try in the current process in case the python symbols would
    // be already loaded
    (void) libHandle;
    libHandle = dlopen(nullptr, RTLD_LAZY);
    libHandleStatic = libHandle;
    if( libHandle != nullptr &&
        dlsym(libHandle, "Py_SetProgramName") != nullptr )
    {
        CPLDebug("GDAL", "Current process has python symbols loaded");
    }
    else
    {
        libHandle = nullptr;
    }

    // Then try the user provided shared object name
    if( libHandle == nullptr && pszPythonSO != nullptr )
    {
        // coverity[tainted_string]
        libHandle = dlopen(pszPythonSO, RTLD_NOW | RTLD_GLOBAL);
        if( libHandle == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot load %s",
                     pszPythonSO);
            return false;
        }
        if( dlsym(libHandle, "Py_SetProgramName") == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find Py_SetProgramName symbol in %s",
                     pszPythonSO);
            return false;
        }
    }

    // Then try the PYTHONSO_DEFAULT if defined at compile time
#ifdef PYTHONSO_DEFAULT
    if( libHandle == nullptr )
    {
        libHandle = dlopen(PYTHONSO_DEFAULT, RTLD_NOW | RTLD_GLOBAL);
        if( !libHandle )
        {
            CPLDebug("GDAL", "%s found", PYTHONSO_DEFAULT);
        }
    }
#endif

#if defined(__MACH__) && defined(__APPLE__)
#define SO_EXT "dylib"
#else
#define IS_SO_EXT
#define SO_EXT "so"
#endif

    const auto tryDlopen = [](CPLString osPythonSO)
    {
        CPLDebug("GDAL", "Trying %s", osPythonSO.c_str());
        auto l_libHandle = dlopen(osPythonSO.c_str(), RTLD_NOW | RTLD_GLOBAL);
#ifdef IS_SO_EXT
        if( l_libHandle == nullptr )
        {
            osPythonSO += ".1.0";
            CPLDebug("GDAL", "Trying %s", osPythonSO.c_str());
            l_libHandle = dlopen(osPythonSO.c_str(), RTLD_NOW | RTLD_GLOBAL);
        }
#endif
        return l_libHandle;
    };

    // Then try to find the libpython that corresponds to the python binary
    // in the PATH
    if( libHandle == nullptr )
    {
        CPLString osVersion;
        char* pszPath = getenv("PATH");
        if( pszPath != nullptr
#ifdef DEBUG
           // For testing purposes
           && CPLTestBool( CPLGetConfigOption(
                                    "GDAL_ENABLE_PYTHON_PATH", "YES") )
#endif
          )
        {
            char** papszTokens = CSLTokenizeString2(pszPath, ":", 0);
            for( int iTry = 0; iTry < 2; ++iTry )
            {
                for( char** papszIter = papszTokens;
                        papszIter != nullptr && *papszIter != nullptr;
                        ++papszIter )
                {
                    struct stat sStat;
                    CPLString osPythonBinary(
                        CPLFormFilename(*papszIter, "python", nullptr));
                    if( iTry == 1 )
                        osPythonBinary += "3";
                    if( lstat(osPythonBinary, &sStat) != 0 )
                        continue;

                    CPLDebug("GDAL", "Found %s", osPythonBinary.c_str());

                    if( S_ISLNK(sStat.st_mode)
#ifdef DEBUG
                        // For testing purposes
                        && CPLTestBool( CPLGetConfigOption(
                                    "GDAL_ENABLE_PYTHON_SYMLINK", "YES") )
#endif
                        )
                    {
                        // If this is a symlink, hopefully the resolved
                        // name will be like "python2.7"
                        const int nBufSize = 2048;
                        std::vector<char> oFilename(nBufSize);
                        char *szPointerFilename = &oFilename[0];
                        int nBytes = static_cast<int>(
                            readlink( osPythonBinary, szPointerFilename,
                                      nBufSize ) );
                        if (nBytes != -1)
                        {
                            szPointerFilename[std::min(nBytes,
                                                       nBufSize - 1)] = 0;
                            CPLString osFilename(
                                            CPLGetFilename(szPointerFilename));
                            CPLDebug("GDAL", "Which is an alias to: %s",
                                     szPointerFilename);
                            if( STARTS_WITH(osFilename, "python") )
                            {
                                osVersion = osFilename.substr(strlen("python"));
                                CPLDebug("GDAL",
                                         "Python version from binary name: %s",
                                         osVersion.c_str());
                            }
                        }
                        else
                        {
                            CPLDebug("GDAL", "realink(%s) failed",
                                        osPythonBinary.c_str());
                        }
                    }

                    // Otherwise, expensive way: start the binary and ask
                    // it for its version...
                    if( osVersion.empty() )
                    {
                        const char* pszPrintVersion =
                            "import sys; print(str(sys.version_info[0]) +"
                            "'.' + str(sys.version_info[1]))";
                        const char* const apszArgv[] = {
                                osPythonBinary.c_str(), "-c",
                                pszPrintVersion,
                                nullptr };
                        const CPLString osTmpFilename(
                                        "/vsimem/LoadPythonAPI/out.txt");
                        VSILFILE* fout = VSIFOpenL( osTmpFilename, "wb+");
                        if( CPLSpawn( apszArgv, nullptr, fout, FALSE ) == 0 )
                        {
                            char* pszStr = reinterpret_cast<char*>(
                                VSIGetMemFileBuffer( osTmpFilename,
                                                        nullptr, FALSE ));
                            osVersion = pszStr;
                            if( !osVersion.empty() &&
                                osVersion.back() == '\n' )
                            {
                                osVersion.resize(osVersion.size() - 1);
                            }
                            CPLDebug("GDAL", "Python version from binary: %s",
                                        osVersion.c_str());
                        }
                        VSIFCloseL(fout);
                        VSIUnlink(osTmpFilename);
                    }
                    break;
                }
                if( !osVersion.empty() )
                    break;
            }
            CSLDestroy(papszTokens);
        }

        if( !osVersion.empty() )
        {
            libHandle = tryDlopen("libpython" + osVersion + "." SO_EXT);
            if( libHandle != nullptr )
            {
                CPLDebug("GDAL", "... success");
            }
            else if( osVersion[0] == '3' )
            {
                libHandle = tryDlopen("libpython" + osVersion + "m." SO_EXT);
                if( libHandle != nullptr )
                {
                    CPLDebug("GDAL", "... success");
                }
            }
        }
    }

    // Otherwise probe a few known objects.
    // Note: update doc/source/drivers/raster/vrt.rst if change
    if( libHandle == nullptr )
    {
        const char* const apszPythonSO[] = { "libpython2.7." SO_EXT,
                                                "libpython3.5m." SO_EXT,
                                                "libpython3.6m." SO_EXT,
                                                "libpython3.7m." SO_EXT,
                                                "libpython3.8m." SO_EXT,
                                                "libpython3.9m." SO_EXT,
                                                "libpython3.4m." SO_EXT,
                                                "libpython3.3." SO_EXT,
                                                "libpython3.2." SO_EXT };
        for( size_t i = 0; libHandle == nullptr &&
                            i < CPL_ARRAYSIZE(apszPythonSO); ++i )
        {
            libHandle = tryDlopen(apszPythonSO[i]);
            if( libHandle != nullptr )
                CPLDebug("GDAL", "... success");
        }
    }

#elif defined(WIN32)

    // First try in the current process in case the python symbols would
    // be already loaded
    HANDLE hProcess = GetCurrentProcess();
    std::vector<HMODULE> ahModules;

    // 100 is not large enough when GDAL is loaded from QGIS for example
    ahModules.resize(1000);
    for( int i = 0; i < 2; i++ )
    {
        DWORD nSizeNeeded = 0;
        const DWORD nSizeIn = static_cast<DWORD>(
                ahModules.size() * sizeof(HMODULE));
        EnumProcessModules(hProcess, &ahModules[0], nSizeIn, &nSizeNeeded);
        ahModules.resize(static_cast<size_t>(nSizeNeeded) / sizeof(HMODULE));
        if( nSizeNeeded <= nSizeIn )
        {
            break;
        }
    }

    for( size_t i = 0; i < ahModules.size(); i++ )
    {
        if( GetProcAddress(ahModules[i], "Py_SetProgramName") )
        {
            libHandle = ahModules[i];
            CPLDebug("GDAL", "Current process has python symbols loaded");
            break;
        }
    }

    // Then try the user provided shared object name
    if( libHandle == nullptr && pszPythonSO != nullptr )
    {
        UINT        uOldErrorMode;
        /* Avoid error boxes to pop up (#5211, #5525) */
        uOldErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX |
                                     SEM_FAILCRITICALERRORS);

#if (defined(WIN32) && _MSC_VER >= 1310) || __MSVCRT_VERSION__ >= 0x0601
        if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
        {
            wchar_t *pwszFilename =
                CPLRecodeToWChar( pszPythonSO, CPL_ENC_UTF8, CPL_ENC_UCS2 );
            libHandle = LoadLibraryW(pwszFilename);
            CPLFree( pwszFilename );
        }
        else
#endif
        {
            libHandle = LoadLibrary(pszPythonSO);
        }

        SetErrorMode(uOldErrorMode);

        if( libHandle == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot load %s",
                     pszPythonSO);
            return false;
        }
        if( GetProcAddress(libHandle, "Py_SetProgramName") == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find Py_SetProgramName symbol in %s",
                     pszPythonSO);
            return false;
        }
    }

    // Then try the PYTHONSO_DEFAULT if defined at compile time
#ifdef PYTHONSO_DEFAULT
    if( libHandle == nullptr )
    {
        UINT        uOldErrorMode;
        uOldErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX |
                                        SEM_FAILCRITICALERRORS);

        libHandle = LoadLibrary(PYTHONSO_DEFAULT);
        SetErrorMode(uOldErrorMode);
        if( !libHandle )
        {
            CPLDebug("GDAL", "%s found", PYTHONSO_DEFAULT);
        }
    }
#endif

    // Then try to find the pythonXY.dll that corresponds to the python binary
    // in the PATH
    if( libHandle == nullptr )
    {
        CPLString osDLLName;
        char* pszPath = getenv("PATH");
        if( pszPath != nullptr
#ifdef DEBUG
           // For testing purposes
           && CPLTestBool( CPLGetConfigOption(
                                    "GDAL_ENABLE_PYTHON_PATH", "YES") )
#endif
          )
        {
            char** papszTokens = CSLTokenizeString2(pszPath, ";", 0);
            for( int iTry = 0; iTry < 2; ++iTry )
            {
                for( char** papszIter = papszTokens;
                        papszIter != nullptr && *papszIter != nullptr;
                        ++papszIter )
                {
                    VSIStatBufL sStat;
                    CPLString osPythonBinary(
                            CPLFormFilename(*papszIter, "python.exe", nullptr));
                    if( iTry == 1 )
                        osPythonBinary += "3";
                    if( VSIStatL(osPythonBinary, &sStat) != 0 )
                        continue;

                    CPLDebug("GDAL", "Found %s", osPythonBinary.c_str());

                    // In python2.7, the dll is in the same directory as the exe
                    char** papszFiles = VSIReadDir(*papszIter);
                    for( char** papszFileIter = papszFiles;
                                papszFileIter != nullptr && *papszFileIter != nullptr;
                                ++papszFileIter )
                    {
                        if( STARTS_WITH_CI(*papszFileIter, "python") &&
                            !EQUAL(*papszFileIter, "python3.dll") &&
                            EQUAL(CPLGetExtension(*papszFileIter), "dll") )
                        {
                            osDLLName = CPLFormFilename(*papszIter,
                                                        *papszFileIter,
                                                        nullptr);
                            break;
                        }
                    }
                    CSLDestroy(papszFiles);

                    // In python3.2, the dll is in the DLLs subdirectory
                    if( osDLLName.empty() )
                    {
                        CPLString osDLLsDir(
                                CPLFormFilename(*papszIter, "DLLs", nullptr));
                        papszFiles = VSIReadDir( osDLLsDir );
                        for( char** papszFileIter = papszFiles;
                                    papszFileIter != nullptr && *papszFileIter != nullptr;
                                    ++papszFileIter )
                        {
                            if( STARTS_WITH_CI(*papszFileIter, "python") &&
                                EQUAL(CPLGetExtension(*papszFileIter), "dll") )
                            {
                                osDLLName = CPLFormFilename(osDLLsDir,
                                                            *papszFileIter,
                                                            nullptr);
                                break;
                            }
                        }
                        CSLDestroy(papszFiles);
                    }

                    break;
                }
                if( !osDLLName.empty() )
                    break;
            }
            CSLDestroy(papszTokens);
        }

        if( !osDLLName.empty() )
        {
            //CPLDebug("GDAL", "Trying %s", osDLLName.c_str());
            UINT        uOldErrorMode;
            uOldErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX |
                                            SEM_FAILCRITICALERRORS);
            libHandle = LoadLibrary(osDLLName);
            SetErrorMode(uOldErrorMode);
            if( libHandle != nullptr )
            {
                CPLDebug("GDAL", "%s loaded", osDLLName.c_str());
            }
        }
    }

    // Otherwise probe a few known objects
    // Note: update doc/source/drivers/raster/vrt.rst if change
    if( libHandle == nullptr )
    {
        const char* const apszPythonSO[] = { "python27.dll",
                                            "python35.dll",
                                            "python36.dll",
                                            "python37.dll",
                                            "python38.dll",
                                            "python39.dll",
                                            "python34.dll",
                                            "python33.dll",
                                            "python32.dll" };
        UINT        uOldErrorMode;
        uOldErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX |
                                        SEM_FAILCRITICALERRORS);

        for( size_t i = 0; libHandle == nullptr &&
                            i < CPL_ARRAYSIZE(apszPythonSO); ++i )
        {
            CPLDebug("GAL", "Trying %s", apszPythonSO[i]);
            libHandle = LoadLibrary(apszPythonSO[i]);
            if( libHandle != nullptr )
                CPLDebug("GDAL", "... success");
        }
        SetErrorMode(uOldErrorMode);
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
    LOAD(libHandle, PyBuffer_FillInfo);
    LOAD(libHandle, PyMemoryView_FromBuffer);
    LOAD(libHandle, PyObject_Type);
    LOAD(libHandle, PyObject_IsInstance);
    LOAD(libHandle, PyTuple_New);
    LOAD(libHandle, PyBool_FromLong);
    LOAD(libHandle, PyLong_FromLong);
    LOAD(libHandle, PyLong_AsLong);
    LOAD(libHandle, PyLong_FromLongLong);
    LOAD(libHandle, PyLong_AsLongLong);
    LOAD(libHandle, PyBytes_Size);
    LOAD(libHandle, PyBytes_AsString);
    LOAD(libHandle, PyBytes_FromStringAndSize);

    LOAD(libHandle, PyModule_Create2);

    LOAD_NOCHECK_WITH_NAME(libHandle, PyUnicode_FromString,
                           "PyUnicode_FromString");
    if( PyUnicode_FromString == nullptr )
    {
        LOAD_NOCHECK_WITH_NAME(libHandle, PyUnicode_FromString,
                                    "PyUnicodeUCS2_FromString");
    }
    if( PyUnicode_FromString == nullptr )
    {
        LOAD_WITH_NAME(libHandle, PyUnicode_FromString,
                                    "PyUnicodeUCS4_FromString");
    }
    LOAD_NOCHECK_WITH_NAME(libHandle, PyUnicode_AsUTF8String,
                           "PyUnicode_AsUTF8String");
    if( PyUnicode_AsUTF8String == nullptr )
    {
        LOAD_NOCHECK_WITH_NAME(libHandle, PyUnicode_AsUTF8String,
                                    "PyUnicodeUCS2_AsUTF8String");
    }
    if( PyUnicode_AsUTF8String == nullptr )
    {
        LOAD_WITH_NAME(libHandle, PyUnicode_AsUTF8String,
                                    "PyUnicodeUCS4_AsUTF8String");
    }

    LOAD(libHandle, PyFloat_FromDouble);
    LOAD(libHandle, PyFloat_AsDouble);
    LOAD(libHandle, PyObject_Call);
    LOAD(libHandle, PyObject_GetIter);
    LOAD(libHandle, PyIter_Next);
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
    LOAD_NOCHECK(libHandle, Py_CompileString);
    if( Py_CompileString == nullptr )
    {
        // Probably just a temporary measure for a bug of Python 3.8.0 on Windows
        // https://bugs.python.org/issue37633
        LOAD(libHandle, Py_CompileStringExFlags);
        Py_CompileString = GDAL_Py_CompileString;
    }
    LOAD(libHandle, PyImport_ExecCodeModule);
    LOAD(libHandle, PyObject_HasAttrString);
    LOAD(libHandle, PyObject_GetAttrString);
    LOAD(libHandle, PyObject_SetAttrString);
    LOAD(libHandle, PyTuple_SetItem);
    LOAD(libHandle, PyObject_Print);
    LOAD(libHandle, PyImport_ImportModule);
    LOAD(libHandle, PyCallable_Check);
    LOAD(libHandle, PyDict_New);
    LOAD(libHandle, PyDict_SetItemString);
    LOAD(libHandle, PyDict_Next);
    LOAD(libHandle, PyDict_GetItemString);
    LOAD(libHandle, PyList_New);
    LOAD(libHandle, PyList_SetItem);
    LOAD(libHandle, PySequence_Check);
    LOAD(libHandle, PySequence_Size);
    LOAD(libHandle, PySequence_GetItem);
    LOAD(libHandle, PyArg_ParseTuple);
    LOAD(libHandle, PyGILState_Ensure);
    LOAD(libHandle, PyGILState_Release);
    LOAD(libHandle, PyErr_Fetch);
    LOAD(libHandle, PyErr_Clear);
    LOAD(libHandle, Py_GetVersion);

    CPLString osPythonVersion(Py_GetVersion());
    osPythonVersion.replaceAll("\r\n", ' ');
    osPythonVersion.replaceAll('\n', ' ');
    CPLDebug("GDAL", "Python version used: %s", osPythonVersion.c_str());

#else // LOAD_NOCHECK_WITH_NAME
    CPLError(CE_Failure, CPLE_AppDefined,
             "This platform doesn't support dynamic loading of libraries")
    return false;
#endif // LOAD_NOCHECK_WITH_NAME

    bInit = true;
    return bInit;
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALPythonInitialize()                        */
/************************************************************************/

/** Call this to initialize the Python environment.
 */
bool GDALPythonInitialize()
{
   std::lock_guard<std::mutex> guard(gMutex);

    if( !LoadPythonAPI() )
        return false;

    int bIsInitialized = Py_IsInitialized();
    if( !bIsInitialized)
    {
        gbHasInitializedPython = true;
        Py_InitializeEx(0);
        CPLDebug("GDAL", "Py_Initialize()");
        PyEval_InitThreads();
        gphThreadState = PyEval_SaveThread();
    }

    return true;
}

/************************************************************************/
/*                        GDALPythonFinalize()                          */
/************************************************************************/

/** To be called by GDALDestroy() */
void GDALPythonFinalize()
{
    if( gbHasInitializedPython )
    {
        CPLDebug("GDAL", "Py_Finalize() = %p", Py_Finalize);
        PyEval_RestoreThread(gphThreadState);
        Py_Finalize();
        gbHasInitializedPython = false;
        gphThreadState = nullptr;
    }
}

namespace GDALPy
{

/************************************************************************/
/*                            GIL_Holder()                              */
/************************************************************************/

GIL_Holder::GIL_Holder(bool bExclusiveLock):
    m_bExclusiveLock(bExclusiveLock)
{
    if( bExclusiveLock )
    {
        gMutex.lock();
    }
    m_eState = PyGILState_Ensure();
}

/************************************************************************/
/*                           ~GIL_Holder()                              */
/************************************************************************/

GIL_Holder::~GIL_Holder()
{
    PyGILState_Release(m_eState);
    if( m_bExclusiveLock )
    {
        gMutex.unlock();
    }
    else
    {
    }
}

/************************************************************************/
/*                             GetString()                              */
/************************************************************************/

CPLString GetString(PyObject* obj, bool bEmitError)
{
    PyObject* unicode = PyUnicode_AsUTF8String(obj);
    if( PyErr_Occurred() )
    {
        if( bEmitError)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s", GetPyExceptionString().c_str());
        }
        return CPLString();
    }

    const char* pszRet = PyBytes_AsString(unicode);
    CPLString osRet = pszRet ? pszRet : "";
    Py_DecRef(unicode);
    return osRet;
}

/************************************************************************/
/*                      GetPyExceptionString()                          */
/************************************************************************/

CPLString GetPyExceptionString()
{
    PyObject *poPyType = nullptr;
    PyObject *poPyValue = nullptr;
    PyObject *poPyTraceback = nullptr;

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
        "       return ''.join(traceback.format_exception_only(etype, value))\n"
        "    except:\n"
        "       return (str(etype) + ', ' + str(value))\n"
        "\n"
        "def GDALFormatException3(etype, value, tb):\n"
        //"    print(etype, value, tb)\n"
        "    try:\n"
        "       return ''.join(traceback.format_exception(etype, value, tb))\n"
        "    except:\n"
        "       return (str(etype) + ', ' + str(value))\n");

    CPLString osRet("An exception occurred in exception formatting code...");

    static int nCounter = 0;
    CPLString osModuleName( CPLSPrintf("gdal_exception_%d", nCounter));
    PyObject* poCompiledString = Py_CompileString(osPythonCode,
                                                  osModuleName, Py_file_input);
    if( poCompiledString == nullptr || PyErr_Occurred() )
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
            pyArgs, nullptr );
        Py_DecRef(pyArgs);

        if( PyErr_Occurred() )
        {
            osRet = "An exception occurred in exception formatting code...";
            PyErr_Print();
        }
        else
        {
            osRet = GetString(poPyRet, false);
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
/*                      ErrOccurredEmitCPLError()                       */
/************************************************************************/

bool ErrOccurredEmitCPLError()
{
    if (PyErr_Occurred())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "%s", GetPyExceptionString().c_str());
        return true;
    }
    return false;
}

} // namespace GDALPy

//! @endcond

