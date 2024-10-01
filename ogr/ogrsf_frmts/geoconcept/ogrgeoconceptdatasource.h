/**********************************************************************
 * $Id: ogrgeoconceptdatasource.h$
 *
 * Name:     ogrgeoconceptdatasource.h
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeoconceptDataSource class.
 * Language: C++
 *
 **********************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
 *
 * SPDX-License-Identifier: MIT
 **********************************************************************/

#include "ogrsf_frmts.h"
#include "ogrgeoconceptlayer.h"

#ifndef GEOCONCEPT_OGR_DATASOURCE_H_INCLUDED_
#define GEOCONCEPT_OGR_DATASOURCE_H_INCLUDED_

/**********************************************************************/
/*            OGCGeoconceptDataSource Class                           */
/**********************************************************************/
class OGRGeoconceptDataSource : public GDALDataset
{
  private:
    OGRGeoconceptLayer **_papoLayers;
    int _nLayers;

    char *_pszGCT;
    char *_pszDirectory;
    char *_pszExt;
    char **_papszOptions;
    bool _bSingleNewFile;
    bool _bUpdate;
    GCExportFileH *_hGXT;

  public:
    OGRGeoconceptDataSource();
    ~OGRGeoconceptDataSource();

    int Open(const char *pszName, bool bTestOpen, bool bUpdate);
    int Create(const char *pszName, char **papszOptions);

    int GetLayerCount() override
    {
        return _nLayers;
    }

    OGRLayer *GetLayer(int iLayer) override;
    //    OGRErr         DeleteLayer( int iLayer );
    int TestCapability(const char *pszCap) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

  private:
    int LoadFile(const char *);
};

#endif /* GEOCONCEPT_OGR_DATASOURCE_H_INCLUDED_ */
