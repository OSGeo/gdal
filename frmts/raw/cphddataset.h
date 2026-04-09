/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  CPHD driver multidimensional classes
 * Author:   Norman Barker <norman at analyticaspatial.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Norman Barker <norman at analyticaspatial.com>
 *
 ****************************************************************************/

#ifndef CPHDDATASET_H_INCLUDED
#define CPHDDATASET_H_INCLUDED

#include "gdal_pam.h"
#include "gdal_priv.h"

/************************************************************************/
/*                             CPHDDataset                              */
/************************************************************************/

class CPL_DLL CPHDDataset CPL_FINAL : public GDALPamDataset
{
    CPL_DISALLOW_COPY_ASSIGN(CPHDDataset)
    std::shared_ptr<GDALGroup> m_poRootGroup{};

  public:
    CPHDDataset()
    {
    }

    static GDALDataset *OpenMultiDim(GDALOpenInfo *poOpenInfo);

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poRootGroup;
    }

    static GDALDataset *Open(GDALOpenInfo *);
};

#endif /* ndef CPHDDATASET_H_INCLUDED */
