/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  GDALPamDataset with internal storage for georeferencing, with
 *           priority for PAM over internal georeferencing
 * Author:   Even Rouault <even dot rouault at mines-paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GDAL_GEOREF_PAM_DATASET_H_INCLUDED
#define GDAL_GEOREF_PAM_DATASET_H_INCLUDED

#include "gdal_pam.h"

class CPL_DLL GDALGeorefPamDataset : public GDALPamDataset
{
  protected:
    int         bGeoTransformValid;
    double      adfGeoTransform[6];
    char        *pszProjection;
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

  public:
        GDALGeorefPamDataset();
        ~GDALGeorefPamDataset();

    virtual CPLErr          GetGeoTransform( double * );
    virtual const char     *GetProjectionRef();

    virtual int             GetGCPCount();
    virtual const char     *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
};

#endif /* GDAL_GEOREF_PAM_DATASET_H_INCLUDED */
