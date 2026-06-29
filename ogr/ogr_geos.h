/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definitions related to support for use of GEOS in OGR.
 *           This file is only intended to be pulled in by OGR implementation
 *           code directly accessing GEOS.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_GEOS_H_INCLUDED
#define OGR_GEOS_H_INCLUDED

#ifdef HAVE_GEOS
// To avoid accidental use of non reentrant GEOS API.
// (check only effective in GEOS >= 3.5)
#define GEOS_USE_ONLY_R_API

#include <geos_c.h>

struct GDALGEOSProgressUserData
{
    explicit GDALGEOSProgressUserData(GDALProgressFunc pfnProgress,
                                      void *pProgressData)
        : m_pfnProgress(pfnProgress), m_pProgressData(pProgressData),
          m_bRequestedInterrupt(false)
    {
    }

    GDALProgressFunc m_pfnProgress;
    void *m_pProgressData;
    bool m_bRequestedInterrupt;

    CPL_DISALLOW_COPY_ASSIGN(GDALGEOSProgressUserData)
};

/** Class that automatically registers progress/interrupt handlers with GEOS on
 *  initialization and unregisters them upon destruction. If GDAL is built
 *  against a version of GEOS that does not support progress/interrupt handlers,
 *  no action will be taken on initialization/destruction.
 */
class GDALGEOSProgressReporter
{
  public:
    GDALGEOSProgressReporter(GEOSContextHandle_t hGEOSCtxt,
                             GDALProgressFunc pfnProgress, void *pProgressData);
    ~GDALGEOSProgressReporter();
    GEOSContextHandle_t m_context;
    GDALGEOSProgressUserData m_userData;

    CPL_DISALLOW_COPY_ASSIGN(GDALGEOSProgressReporter)
};

void GDALGEOSProgress(double frac, const char *message, void *userData);
int GDALGEOSCheckInterrupt(void *userData);

#else

namespace geos
{
class Geometry;
}

#endif

#endif /* ndef OGR_GEOS_H_INCLUDED */
