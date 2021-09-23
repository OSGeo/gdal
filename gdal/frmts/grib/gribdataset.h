/******************************************************************************
 * $Id$
 *
 * Project:  GRIB Driver
 * Purpose:  GDALDataset driver for GRIB translator for read support
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2007, ITC
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
 ******************************************************************************
 *
 */

#ifndef GRIBDATASET_H
#define GRIBDATASET_H

#include "cpl_port.h"

#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <algorithm>
#include <memory>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "degrib/degrib/degrib2.h"
#include "degrib/degrib/inventory.h"
#include "degrib/degrib/meta.h"
#include "degrib/degrib/myerror.h"
#include "degrib/degrib/type.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

/************************************************************************/
/* ==================================================================== */
/*                              GRIBDataset                             */
/* ==================================================================== */
/************************************************************************/

class GRIBArray;
class GRIBRasterBand;

namespace gdal {
namespace grib {
class InventoryWrapper;
}
}

class GRIBDataset final: public GDALPamDataset
{
    friend class GRIBArray;
    friend class GRIBRasterBand;

  public:
                GRIBDataset();
                ~GRIBDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );

    CPLErr      GetGeoTransform( double *padfTransform ) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return m_poSRS.get();
    }

    std::shared_ptr<GDALGroup> GetRootGroup() const override { return m_poRootGroup; }

  private:
    void SetGribMetaData(grib_MetaData *meta);
    static GDALDataset *OpenMultiDim( GDALOpenInfo * );
    static std::unique_ptr<gdal::grib::InventoryWrapper> Inventory(VSILFILE *, GDALOpenInfo *);

    VSILFILE *fp;
    // Calculate and store once as GetGeoTransform may be called multiple times.
    double adfGeoTransform[6];

    GIntBig nCachedBytes;
    GIntBig nCachedBytesThreshold;
    int bCacheOnlyOneBand;

    // Split&Swap: transparent rewrap around the prime meridian instead of the antimeridian
    // rows after nSplitAndSwapColumn are placed at the beginning
    // while rows before are placed at the end
    int nSplitAndSwapColumn;

    GRIBRasterBand *poLastUsedBand;
    std::shared_ptr<GDALGroup> m_poRootGroup{};
    std::shared_ptr<OGRSpatialReference> m_poSRS{};
    std::unique_ptr<OGRSpatialReference> m_poLL{};
    std::unique_ptr<OGRCoordinateTransformation> m_poCT{};
};

/************************************************************************/
/* ==================================================================== */
/*                            GRIBRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GRIBRasterBand final: public GDALPamRasterBand
{
    friend class GRIBArray;
    friend class GRIBDataset;

public:
    GRIBRasterBand( GRIBDataset *, int, inventoryType * );
    virtual ~GRIBRasterBand();
    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual const char *GetDescription() const override;

    virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
    virtual char **GetMetadata(const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName, const char *pszDomain = "") override;

    void    FindPDSTemplate();

    void    UncacheData();

    static void ReadGribData( VSILFILE *, vsi_l_offset, int, double **,
                              grib_MetaData ** );
private:
    CPLErr       LoadData();
    void         FindNoDataGrib2(bool bSeekToStart = true);
    void         FindMetaData();
    // Heuristic search for the start of the message
    static vsi_l_offset FindTrueStart(VSILFILE *, vsi_l_offset);

    vsi_l_offset start;
    int subgNum;
    char *longFstLevel;

    double *m_Grib_Data;
    grib_MetaData *m_Grib_MetaData;

    int nGribDataXSize;
    int nGribDataYSize;
    int     m_nGribVersion;

    bool    m_bHasLookedForNoData;
    double  m_dfNoData;
    bool    m_bHasNoData;

    int     m_nDisciplineCode = -1;
    std::string m_osDisciplineName{};
    int     m_nCenter = -1;
    std::string m_osCenterName{};
    int     m_nSubCenter = -1;
    std::string m_osSubCenterName{};
    std::string m_osSignRefTimeName{};
    std::string m_osRefTime{};
    std::string m_osProductionStatus{};
    std::string m_osType{};
    int     m_nPDTN = -1;
    std::vector<GUInt32> m_anPDSTemplateAssembledValues{};
    bool    bLoadedPDS = false;
    bool    bLoadedMetadata = false;
};

namespace gdal {
namespace grib {

// Thin layer to manage allocation and deallocation.
class InventoryWrapper
{
  public:
    InventoryWrapper() {}
    virtual ~InventoryWrapper() {}

    // Modifying the contents pointed to by the return is allowed.
    inventoryType * get(int i) const {
      if (i < 0 || i >= static_cast<int>(inv_len_)) return nullptr;
      return inv_ + i;
    }

    uInt4 length() const { return inv_len_; }
    size_t num_messages() const { return num_messages_; }
    int result() const { return result_; }

  protected:
    inventoryType *inv_ = nullptr;
    uInt4 inv_len_ = 0;
    int num_messages_ = 0;
    int result_ = 0;
};

} // namespace grib
} // namespace gdal

const char* const apszJ2KDrivers[] =
        { "JP2KAK", "JP2OPENJPEG", "JPEG2000", "JP2ECW" };

#endif // GRIBDATASET_H
