/******************************************************************************
 *
 * Project:  VICAR Driver; JPL/MIPL VICAR Format
 * Purpose:  Implementation of VICARDataset
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Sebastian Walter <sebastian dot walter at fu-berlin dot de>
 * Copyright (c) 2019, Even Rouault, <even.rouault at spatialys.com>
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

#ifndef VICARDATASET_H
#define VICARDATASET_H

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"
#include "vicarkeywordhandler.h"
#include <array>

/************************************************************************/
/* ==================================================================== */
/*                             VICARDataset                             */
/* ==================================================================== */
/************************************************************************/

class VICARDataset final: public RawDataset
{
    VSILFILE    *fpImage = nullptr;

    VICARKeywordHandler  oKeywords;

    CPLJSONObject m_oJSonLabel;
    CPLStringList m_aosVICARMD;

    bool        bGotTransform = false;
    std::array<double, 6> adfGeoTransform = {{0.0,1.0,0,0.0,0.0,1.0}};

    CPLString   osProjection;

    const char *GetKeyword( const char *pszPath,
                            const char *pszDefault = "");

public:
    VICARDataset() = default;
    virtual ~VICARDataset();

    virtual CPLErr GetGeoTransform( double * padfTransform ) override;
    virtual const char *_GetProjectionRef(void) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    bool GetRawBinaryLayout(GDALDataset::RawBinaryLayout&) override;

    char **GetMetadataDomainList() override;
    char **GetMetadata( const char* pszDomain = "" ) override;

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );

    static GDALDataType GetDataTypeFromFormat(const char* pszFormat);
    static bool         GetSpacings(const VICARKeywordHandler& keywords,
                                    GUInt64& nPixelOffset,
                                    GUInt64& nLineOffset,
                                    GUInt64& nBandOffset,
                                    GUInt64& nImageOffsetWithoutNBB,
                                    GUInt64& nNBB,
                                    GUInt64& nImageSize);
};

#endif // VICARDATASET_H
