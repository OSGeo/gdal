/******************************************************************************
 *
 * Name:     gdalantirecursion.h
 * Project:  GDAL Core
 * Purpose:  GDAL Core C++/Private declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALANTIRECURSION_H_INCLUDED
#define GDALANTIRECURSION_H_INCLUDED

#include <string>

/************************************************************************/
/*                        GDALAntiRecursionGuard                        */
/************************************************************************/

//! @cond Doxygen_Suppress
struct GDALAntiRecursionStruct;

class GDALAntiRecursionGuard
{
    GDALAntiRecursionStruct *m_psAntiRecursionStruct;
    std::string m_osIdentifier;
    int m_nDepth;

    GDALAntiRecursionGuard(const GDALAntiRecursionGuard &) = delete;
    GDALAntiRecursionGuard &operator=(const GDALAntiRecursionGuard &) = delete;

  public:
    explicit GDALAntiRecursionGuard(const std::string &osIdentifier);
    GDALAntiRecursionGuard(const GDALAntiRecursionGuard &other,
                           const std::string &osIdentifier);
    ~GDALAntiRecursionGuard();

    int GetCallDepth() const
    {
        return m_nDepth;
    }
};

//! @endcond

#endif
