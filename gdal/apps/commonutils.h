/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Common utility routines
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef COMMONUTILS_H_INCLUDED
#define COMMONUTILS_H_INCLUDED

#include "cpl_port.h"

#ifdef __cplusplus

#if defined(WIN32) && (defined(_MSC_VER) || defined(SUPPORTS_WMAIN))

#include <wchar.h>
#include <stdlib.h>
#include "cpl_conv.h"
#include "cpl_string.h"

class ARGVDestroyer
{
        char** m_papszList = nullptr;
        ARGVDestroyer(const ARGVDestroyer&) = delete;
        ARGVDestroyer& operator= (const ARGVDestroyer&) = delete;

    public:
        explicit ARGVDestroyer(char** papszList) : m_papszList(papszList) {}
        ~ARGVDestroyer() { CSLDestroy(m_papszList); }
};

extern "C" int wmain( int argc, wchar_t ** argv_w, wchar_t ** /* envp */ );

#define MAIN_START(argc, argv) \
  extern "C" \
  int wmain( int argc, wchar_t ** argv_w, wchar_t ** /* envp */ ) \
  { \
    char **argv = static_cast<char**>(CPLCalloc(argc + 1, sizeof(char*))); \
    for( int i = 0; i < argc; i++ ) \
    { \
        argv[i] = CPLRecodeFromWChar( argv_w[i], CPL_ENC_UCS2, CPL_ENC_UTF8 ); \
    } \
    ARGVDestroyer argvDestroyer(argv);

#define MAIN_END }

#else // defined(WIN32)

#define MAIN_START(argc, argv) \
    int main( int argc, char ** argv )

#define MAIN_END

#endif // defined(WIN32)
#endif // defined(__cplusplus)


CPL_C_START

void CPL_DLL EarlySetConfigOptions( int argc, char ** argv );

CPL_C_END

#ifdef __cplusplus

#include "cpl_string.h"
#include <vector>

std::vector<CPLString> CPL_DLL GetOutputDriversFor(const char* pszDestFilename,
                                                   int nFlagRasterVector);
CPLString CPL_DLL GetOutputDriverForRaster(const char* pszDestFilename);

#endif /* __cplusplus */

#endif /* COMMONUTILS_H_INCLUDED */
