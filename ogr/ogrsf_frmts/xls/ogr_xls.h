/******************************************************************************
 *
 * Project:  XLS Translator
 * Purpose:  Definition of classes for OGR .xls driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_XLS_H_INCLUDED
#define OGR_XLS_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                             OGRXLSLayer                              */
/************************************************************************/

class OGRXLSDataSource;

class OGRXLSLayer final : public OGRLayer,
                          public OGRGetNextFeatureThroughRaw<OGRXLSLayer>
{
    OGRXLSDataSource *poDS;
    mutable OGRFeatureDefn *poFeatureDefn;

    char *pszName;
    int iSheet;
    bool bFirstLineIsHeaders;
    int nRows;
    unsigned short nCols;

    int nNextFID;

    OGRFeature *GetNextRawFeature();

    void DetectHeaderLine(const void *xlshandle);
    void DetectColumnTypes(const void *xlshandle, int *paeFieldTypes);

  public:
    OGRXLSLayer(OGRXLSDataSource *poDSIn, const char *pszSheetname,
                int iSheetIn, int nRowsIn, unsigned short nColsIn);
    ~OGRXLSLayer() override;

    void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRXLSLayer)

    const OGRFeatureDefn *GetLayerDefn() const override;
    GIntBig GetFeatureCount(int bForce = TRUE) override;

    const char *GetName() const override
    {
        return pszName;
    }

    OGRwkbGeometryType GetGeomType() const override
    {
        return wkbNone;
    }

    int TestCapability(const char *) const override;

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return nullptr;
    }

    GDALDataset *GetDataset() override;
};

/************************************************************************/
/*                           OGRXLSDataSource                           */
/************************************************************************/

class OGRXLSDataSource final : public GDALDataset
{
    OGRLayer **papoLayers;
    int nLayers;

    const void *xlshandle;

    CPLString m_osANSIFilename;
#ifdef _WIN32
    CPLString m_osTempFilename;
#endif
  public:
    OGRXLSDataSource();
    ~OGRXLSDataSource() override;

    int Open(const char *pszFilename, int bUpdate);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;

    const void *GetXLSHandle();
};

#endif /* ndef OGR_XLS_H_INCLUDED */
