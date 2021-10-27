/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  GDALGeorefPamDataset with helper to read georeferencing and other
 *           metadata from JP2Boxes
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GDAL_JP2_ABSTRACT_DATASET_H_INCLUDED
#define GDAL_JP2_ABSTRACT_DATASET_H_INCLUDED

//! @cond Doxygen_Suppress
#include "gdalgeorefpamdataset.h"

class CPL_DLL GDALJP2AbstractDataset: public GDALGeorefPamDataset
{
    char*               pszWldFilename = nullptr;

    GDALDataset*        poMemDS = nullptr;
    char**              papszMetadataFiles = nullptr;
    int                 m_nWORLDFILEIndex = -1;

    CPL_DISALLOW_COPY_ASSIGN(GDALJP2AbstractDataset)

  protected:
    int CloseDependentDatasets() override;

  public:
    GDALJP2AbstractDataset();
    ~GDALJP2AbstractDataset() override;

    void LoadJP2Metadata( GDALOpenInfo* poOpenInfo,
                          const char* pszOverrideFilename = nullptr );
    void LoadVectorLayers( int bOpenRemoteResources = FALSE );

    char **GetFileList( void ) override;

    int GetLayerCount() override;
    OGRLayer *GetLayer( int i ) override;
};
//! @endcond

#endif /* GDAL_JP2_ABSTRACT_DATASET_H_INCLUDED */
