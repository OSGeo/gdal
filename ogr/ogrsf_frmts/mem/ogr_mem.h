/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the OGR Memory driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRMEM_H_INCLUDED
#define OGRMEM_H_INCLUDED

#include "ogrsf_frmts.h"

#include <map>

/************************************************************************/
/*                             OGRMemLayer                              */
/************************************************************************/
class OGRMemDataSource;

class IOGRMemLayerFeatureIterator;

class CPL_DLL OGRMemLayer CPL_NON_FINAL : public OGRLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMemLayer)

    typedef std::map<GIntBig, std::unique_ptr<OGRFeature>> FeatureMap;
    typedef FeatureMap::iterator FeatureIterator;

    OGRFeatureDefn *m_poFeatureDefn = nullptr;

    GIntBig m_nFeatureCount = 0;

    GIntBig m_iNextReadFID = 0;
    GIntBig m_nMaxFeatureCount = 0;  // Max size of papoFeatures.
    OGRFeature **m_papoFeatures = nullptr;
    bool m_bHasHoles = false;

    FeatureMap m_oMapFeatures{};
    FeatureIterator m_oMapFeaturesIter{};

    GIntBig m_iNextCreateFID = 0;

    bool m_bUpdatable = true;
    bool m_bAdvertizeUTF8 = false;

    bool m_bUpdated = false;

    std::string m_osFIDColumn{};

    GDALDataset *m_poDS{};

    // Only use it in the lifetime of a function where the list of features
    // doesn't change.
    IOGRMemLayerFeatureIterator *GetIterator();

  protected:
    OGRFeature *GetFeatureRef(GIntBig nFeatureId);

  public:
    // Clone poSRS if not nullptr
    OGRMemLayer(const char *pszName, const OGRSpatialReference *poSRS,
                OGRwkbGeometryType eGeomType);
    virtual ~OGRMemLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    virtual OGRErr SetNextByIndex(GIntBig nIndex) override;

    OGRFeature *GetFeature(GIntBig nFeatureId) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr IUpsertFeature(OGRFeature *poFeature) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;
    virtual OGRErr DeleteFeature(GIntBig nFID) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    GIntBig GetFeatureCount(int) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr DeleteField(int iField) override;
    virtual OGRErr ReorderFields(int *panMap) override;
    virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                  int nFlags) override;
    virtual OGRErr
    AlterGeomFieldDefn(int iGeomField,
                       const OGRGeomFieldDefn *poNewGeomFieldDefn,
                       int nFlagsIn) override;
    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomField,
                                   int bApproxOK = TRUE) override;

    int TestCapability(const char *) override;

    const char *GetFIDColumn() override
    {
        return m_osFIDColumn.c_str();
    }

    bool IsUpdatable() const
    {
        return m_bUpdatable;
    }

    void SetUpdatable(bool bUpdatableIn)
    {
        m_bUpdatable = bUpdatableIn;
    }

    void SetAdvertizeUTF8(bool bAdvertizeUTF8In)
    {
        m_bAdvertizeUTF8 = bAdvertizeUTF8In;
    }

    void SetFIDColumn(const char *pszFIDColumn)
    {
        m_osFIDColumn = pszFIDColumn;
    }

    bool HasBeenUpdated() const
    {
        return m_bUpdated;
    }

    void SetUpdated(bool bUpdated)
    {
        m_bUpdated = bUpdated;
    }

    GIntBig GetNextReadFID()
    {
        return m_iNextReadFID;
    }

    void SetDataset(GDALDataset *poDS)
    {
        m_poDS = poDS;
    }

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
};

/************************************************************************/
/*                           OGRMemDataSource                           */
/************************************************************************/

class OGRMemDataSource CPL_NON_FINAL : public GDALDataset
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMemDataSource)

    OGRMemLayer **papoLayers;
    int nLayers;

  public:
    OGRMemDataSource(const char *, char **);
    virtual ~OGRMemDataSource();

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    OGRErr DeleteLayer(int iLayer) override;

    int TestCapability(const char *) override;

    bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                        std::string &failureReason) override;

    bool DeleteFieldDomain(const std::string &name,
                           std::string &failureReason) override;

    bool UpdateFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                           std::string &failureReason) override;
};

#endif  // ndef OGRMEM_H_INCLUDED
