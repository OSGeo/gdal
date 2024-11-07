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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef COMMONUTILS_H_INCLUDED
#define COMMONUTILS_H_INCLUDED

#include "cpl_port.h"

#ifdef __cplusplus

#if defined(_WIN32) && (defined(_MSC_VER) || defined(SUPPORTS_WMAIN))

#include <wchar.h>
#include <stdlib.h>
#include "cpl_conv.h"
#include "cpl_string.h"

class ARGVDestroyer
{
    char **m_papszList = nullptr;
    ARGVDestroyer(const ARGVDestroyer &) = delete;
    ARGVDestroyer &operator=(const ARGVDestroyer &) = delete;

  public:
    explicit ARGVDestroyer(char **papszList) : m_papszList(papszList)
    {
    }

    ~ARGVDestroyer()
    {
        CSLDestroy(m_papszList);
    }
};

extern "C" int wmain(int argc, wchar_t **argv_w, wchar_t ** /* envp */);

#define MAIN_START(argc, argv)                                                 \
    extern "C" int wmain(int argc, wchar_t **argv_w, wchar_t ** /* envp */)    \
    {                                                                          \
        char **argv =                                                          \
            static_cast<char **>(CPLCalloc(argc + 1, sizeof(char *)));         \
        for (int i = 0; i < argc; i++)                                         \
        {                                                                      \
            argv[i] =                                                          \
                CPLRecodeFromWChar(argv_w[i], CPL_ENC_UCS2, CPL_ENC_UTF8);     \
        }                                                                      \
        ARGVDestroyer argvDestroyer(argv);                                     \
        try                                                                    \
        {

#else  // defined(_WIN32)

#define MAIN_START(argc, argv)                                                 \
    int main(int argc, char **argv)                                            \
    {                                                                          \
        try                                                                    \
        {

#endif  // defined(_WIN32)

#define MAIN_END                                                               \
    }                                                                          \
    catch (const std::exception &e)                                            \
    {                                                                          \
        fprintf(stderr, "Unexpected exception: %s", e.what());                 \
        return -1;                                                             \
    }                                                                          \
    }

#endif  // defined(__cplusplus)

CPL_C_START

void CPL_DLL EarlySetConfigOptions(int argc, char **argv);

CPL_C_END

#ifdef __cplusplus

#include "cpl_string.h"
#include <vector>

std::vector<std::string> CPL_DLL
GetOutputDriversFor(const char *pszDestFilename, int nFlagRasterVector);
CPLString CPL_DLL GetOutputDriverForRaster(const char *pszDestFilename);
void GDALRemoveBOM(GByte *pabyData);
std::string GDALRemoveSQLComments(const std::string &osInput);

int ArgIsNumeric(const char *pszArg);

// those values shouldn't be changed, because overview levels >= 0 are meant
// to be overview indices, and ovr_level < OVR_LEVEL_AUTO mean overview level
// automatically selected minus (OVR_LEVEL_AUTO - ovr_level)
constexpr int OVR_LEVEL_AUTO = -2;
constexpr int OVR_LEVEL_NONE = -1;

#endif /* __cplusplus */

#endif /* COMMONUTILS_H_INCLUDED */
