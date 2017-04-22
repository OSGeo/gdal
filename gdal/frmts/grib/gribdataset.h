/******************************************************************************
 * $Id$
 *
 * Project:  GRIB Driver
 * Purpose:  GDALDataset driver for GRIB translator for read support
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2007, ITC
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "degrib18/degrib/datasource.h"
#include "degrib18/degrib/degrib2.h"
#include "degrib18/degrib/filedatasource.h"
#include "degrib18/degrib/inventory.h"
#include "degrib18/degrib/memorydatasource.h"
#include "degrib18/degrib/meta.h"
#include "degrib18/degrib/myerror.h"
#include "degrib18/degrib/type.h"
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

class GRIBRasterBand;

class GRIBDataset : public GDALPamDataset
{
    friend class GRIBRasterBand;

  public:
                GRIBDataset();
                ~GRIBDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );

    CPLErr      GetGeoTransform( double *padfTransform ) override;
    const char *GetProjectionRef() override;

  private:
    void SetGribMetaData(grib_MetaData *meta);
    VSILFILE *fp;
    char *pszProjection;
    // Calculate and store once as GetGeoTransform may be called multiple times.
    double adfGeoTransform[6];

    GIntBig nCachedBytes;
    GIntBig nCachedBytesThreshold;
    int bCacheOnlyOneBand;
    GRIBRasterBand *poLastUsedBand;
};

/************************************************************************/
/* ==================================================================== */
/*                            GRIBRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GRIBRasterBand : public GDALPamRasterBand
{
    friend class GRIBDataset;

public:
    GRIBRasterBand( GRIBDataset *, int, inventoryType * );
    virtual ~GRIBRasterBand();
    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual const char *GetDescription() const override;

    virtual double GetNoDataValue( int *pbSuccess = NULL ) override;

    void    FindPDSTemplate();

    void    UncacheData();

private:
    CPLErr       LoadData();

    static void ReadGribData( DataSource &, sInt4, int, double **,
                              grib_MetaData ** );
    sInt4 start;
    int subgNum;
    char *longFstLevel;

    double *m_Grib_Data;
    grib_MetaData *m_Grib_MetaData;

    int nGribDataXSize;
    int nGribDataYSize;
};

namespace gdal {
namespace grib {

// Thin layer to manage allocation and deallocation.
class InventoryWrapper {
  public:
    explicit InventoryWrapper(const std::string &filepath)
        : inv_(NULL), inv_len_(0), num_messages_(0), result_(0) {
      FileDataSource grib(filepath.c_str());
      result_ = GRIB2Inventory(grib, &inv_, &inv_len_, 0 /* all messages */,
                               &num_messages_);
    }
    explicit InventoryWrapper(FileDataSource file_data_source)
        : inv_(NULL), inv_len_(0), num_messages_(0), result_(0) {
      result_ = GRIB2Inventory(file_data_source, &inv_, &inv_len_,
                               0 /* all messages */, &num_messages_);
    }

    ~InventoryWrapper() {
        if (inv_ == NULL) return;
        for (uInt4 i = 0; i < inv_len_; i++) {
            GRIB2InventoryFree(inv_ + i);
        }
        free(inv_);
    }

    // Modifying the contents pointed to by the return is allowed.
    inventoryType * get(int i) const {
      if (i < 0 || i >= static_cast<int>(inv_len_)) return NULL;
      return inv_ + i;
    }

    uInt4 length() const { return inv_len_; }
    size_t num_messages() const { return num_messages_; }
    int result() const { return result_; }

  private:
    inventoryType *inv_;
    uInt4 inv_len_;
    int num_messages_;
    int result_;
};

}  // namespace grib
}  // namespace gdal
