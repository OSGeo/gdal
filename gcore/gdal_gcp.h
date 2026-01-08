/******************************************************************************
 *
 * Name:     gdal_gcp.h
 * Project:  GDAL Core
 * Purpose:  Declaration of gdal::GCP class
 * Author:   Even Rouault, <even.rouault@spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault, <even.rouault@spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALGCP_H_INCLUDED
#define GDALGCP_H_INCLUDED

#include "cpl_port.h"
#include "gdal.h"

#include <vector>

/* ******************************************************************** */
/*                             gdal::GCP                                */
/* ******************************************************************** */

namespace gdal
{
/** C++ wrapper over the C GDAL_GCP structure.
 *
 * It has the same binary layout, and thus a gdal::GCP pointer can be cast as a
 * GDAL_GCP pointer.
 *
 * @since 3.9
 */
class CPL_DLL GCP
{
  public:
    explicit GCP(const char *pszId = "", const char *pszInfo = "",
                 double dfPixel = 0, double dfLine = 0, double dfX = 0,
                 double dfY = 0, double dfZ = 0);
    ~GCP();
    GCP(const GCP &);
    explicit GCP(const GDAL_GCP &other);
    GCP &operator=(const GCP &);
    GCP(GCP &&);
    GCP &operator=(GCP &&);

    /** Returns the "id" member. */
    inline const char *Id() const
    {
        return gcp.pszId;
    }

    void SetId(const char *pszId);

    /** Returns the "info" member. */
    inline const char *Info() const
    {
        return gcp.pszInfo;
    }

    void SetInfo(const char *pszInfo);

    /** Returns the "pixel" member. */
    inline double Pixel() const
    {
        return gcp.dfGCPPixel;
    }

    /** Returns a reference to the "pixel" member. */
    inline double &Pixel()
    {
        return gcp.dfGCPPixel;
    }

    /** Returns the "line" member. */
    inline double Line() const
    {
        return gcp.dfGCPLine;
    }

    /** Returns a reference to the "line" member. */
    inline double &Line()
    {
        return gcp.dfGCPLine;
    }

    /** Returns the "X" member. */
    inline double X() const
    {
        return gcp.dfGCPX;
    }

    /** Returns a reference to the "X" member. */
    inline double &X()
    {
        return gcp.dfGCPX;
    }

    /** Returns the "Y" member. */
    inline double Y() const
    {
        return gcp.dfGCPY;
    }

    /** Returns a reference to the "Y" member. */
    inline double &Y()
    {
        return gcp.dfGCPY;
    }

    /** Returns the "Z" member. */
    inline double Z() const
    {
        return gcp.dfGCPZ;
    }

    /** Returns a reference to the "Z" member. */
    inline double &Z()
    {
        return gcp.dfGCPZ;
    }

    /** Casts as a C GDAL_GCP pointer */
    inline const GDAL_GCP *c_ptr() const
    {
        return &gcp;
    }

    static const GDAL_GCP *c_ptr(const std::vector<GCP> &asGCPs);

    static std::vector<GCP> fromC(const GDAL_GCP *pasGCPList, int nGCPCount);

  private:
    GDAL_GCP gcp;
};

} /* namespace gdal */

#endif
