/******************************************************************************
 * $Id: kmlsuperoverlaydataset.h 
 *
 * Project:  KmlSuperOverlay
 * Purpose:  Implements write support for KML superoverlay - KMZ.
 * Author:   Harsh Govind, harsh.govind@spadac.com
 *
 ******************************************************************************
 * Copyright (c) 2010, SPADAC Inc. <harsh.govind@spadac.com>
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
 
#ifndef KMLSUPEROVERLAYDATASET_H_INCLUDED
#define KMLSUPEROVERLAYDATASET_H_INCLUDED

#include "gdal_pam.h"
#include "gdal_priv.h"

CPL_C_START
void CPL_DLL GDALRegister_KMLSUPEROVERLAY(void);
CPL_C_END

/************************************************************************/
/*                        KmlSuperOverlayDataset                        */
/************************************************************************/
class OGRCoordinateTransformation;

class CPL_DLL KmlSuperOverlayDataset : public GDALDataset
{
  private:

    int         bGeoTransformSet;

  public:
                  KmlSuperOverlayDataset();
    virtual      ~KmlSuperOverlayDataset();

    static GDALDataset *Open(GDALOpenInfo *);    

    static GDALDataset *CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                                    int bStrict, char ** papszOptions, GDALProgressFunc pfnProgress, void * pProgressData );

    const char *GetProjectionRef();
};

#endif /* ndef KMLSUPEROVERLAYDATASET_H_INCLUDED */

