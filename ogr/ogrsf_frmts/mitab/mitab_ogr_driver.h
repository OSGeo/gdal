/**********************************************************************
 *
 * Name:     mitab_ogr_drive.h
 * Project:  Mid/mif tab ogr support
 * Language: C++
 * Purpose:  Header file containing public definitions for the library.
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Stephane Villeneuve
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 **********************************************************************/

#include "mitab.h"
#include "ogrsf_frmts.h"

#ifndef MITAB_OGR_DRIVER_H_INCLUDED_
#define MITAB_OGR_DRIVER_H_INCLUDED_

/*=====================================================================
 *            OGRTABDataSource Class
 *
 * These classes handle all the file types supported by the MITAB lib.
 * through the IMapInfoFile interface.
 *====================================================================*/
class OGRTABDataSource final : public GDALDataset
{
    CPL_DISALLOW_COPY_ASSIGN(OGRTABDataSource)

  private:
    char *m_pszDirectory;

    int m_nLayerCount;
    IMapInfoFile **m_papoLayers;

    char **m_papszOptions;
    int m_bCreateMIF;
    int m_bSingleFile;
    int m_bSingleLayerAlreadyCreated;
    GBool m_bQuickSpatialIndexMode;
    int m_nBlockSize;

  private:
    inline bool GetUpdate() const
    {
        return eAccess == GA_Update;
    }

  public:
    OGRTABDataSource();
    ~OGRTABDataSource() override;

    int Open(GDALOpenInfo *poOpenInfo, int bTestOpen);
    int Create(const char *pszName, CSLConstList papszOptions);

    int GetLayerCount() const override;
    const OGRLayer *GetLayer(int) const override;
    int TestCapability(const char *) const override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    char **GetFileList() override;

    OGRLayer *ExecuteSQL(const char *pszStatement, OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
};

void CPL_DLL RegisterOGRTAB();

#endif /* MITAB_OGR_DRIVER_H_INCLUDED_ */
