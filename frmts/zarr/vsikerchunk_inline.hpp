/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VSIKERCHUNK_INLINE_HPP
#define VSIKERCHUNK_INLINE_HPP

#include <string_view>

inline bool
ZARRIsLikelyStreamableKerchunkJSONRefContent(const std::string_view &str)
{
    // v0
    for (const char *pszExpectedPrefix : {"{\".zgroup\":{", "{\".zgroup\":\"{",
                                          "{\".zattrs\":{", "{\".zattrs\":\"{"})
    {
        for (char ch : str)
        {
            if (!(ch == ' ' || ch == '\n' || ch == '\r'))
            {
                if (ch != *pszExpectedPrefix)
                    break;
                ++pszExpectedPrefix;
                if (*pszExpectedPrefix == 0)
                {
                    return true;
                }
            }
        }
    }

    {
        // v1
        const char *pszExpectedPrefix = "{\"version\":1,\"refs\":{";
        for (char ch : str)
        {
            if (!(ch == ' ' || ch == '\n' || ch == '\r'))
            {
                if (ch != *pszExpectedPrefix)
                    break;
                ++pszExpectedPrefix;
                if (*pszExpectedPrefix == 0)
                {
                    return str.find("\".zgroup\"") != std::string::npos ||
                           str.find("\".zarray\"") != std::string::npos;
                }
            }
        }
    }
    return false;
}

#endif
