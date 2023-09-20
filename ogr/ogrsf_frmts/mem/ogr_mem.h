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

    typedef std::map<int64_t, OGRFeature *> FeatureMap;
    typedef std::map<int64_t, OGRFeature *>::iterator FeatureIterator;

    OGRFeatureDefn *m_poFeatureDefn;

    int64_t m_nFeatureCount;

    int64_t m_iNextReadFID;
    int64_t m_nMaxFeatureCount;  // Max size of papoFeatures.
    OGRFeature **m_papoFeatures;
    bool m_bHasHoles;

    FeatureMap m_oMapFeatures;
    FeatureIterator m_oMapFeaturesIter;

    int64_t m_iNextCreateFID;

    bool m_bUpdatable;
    bool m_bAdvertizeUTF8;

    bool m_bUpdated;

    std::string m_osFIDColumn{};

    // Only use it in the lifetime of a function where the list of features
    // doesn't change.
    IOGRMemLayerFeatureIterator *GetIterator();

    OGRFeature *GetFeatureRef(int64_t nFeatureId);

  public:
    // Clone poSRS if not nullptr
    OGRMemLayer(const char *pszName, const OGRSpatialReference *poSRS,
                OGRwkbGeometryType eGeomType);
    virtual ~OGRMemLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    virtual OGRErr SetNextByIndex(int64_t nIndex) override;

    OGRFeature *GetFeature(int64_t nFeatureId) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr IUpsertFeature(OGRFeature *poFeature) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;
    virtual OGRErr DeleteFeature(int64_t nFID) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    int64_t GetFeatureCount(int) override;

    virtual OGRErr CreateField(OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr DeleteField(int iField) override;
    virtual OGRErr ReorderFields(int *panMap) override;
    virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                  int nFlags) override;
    virtual OGRErr
    AlterGeomFieldDefn(int iGeomField,
                       const OGRGeomFieldDefn *poNewGeomFieldDefn,
                       int nFlagsIn) override;
    virtual OGRErr CreateGeomField(OGRGeomFieldDefn *poGeomField,
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

    int64_t GetNextReadFID()
    {
        return m_iNextReadFID;
    }
};

/************************************************************************/
/*                           OGRMemDataSource                           */
/************************************************************************/

class OGRMemDataSource CPL_NON_FINAL : public OGRDataSource
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMemDataSource)

    OGRMemLayer **papoLayers;
    int nLayers;

    char *pszName;

  public:
    OGRMemDataSource(const char *, char **);
    virtual ~OGRMemDataSource();

    const char *GetName() override
    {
        return pszName;
    }
    int GetLayerCount() override
    {
        return nLayers;
    }
    OGRLayer *GetLayer(int) override;

    virtual OGRLayer *ICreateLayer(const char *,
                                   OGRSpatialReference * = nullptr,
                                   OGRwkbGeometryType = wkbUnknown,
                                   char ** = nullptr) override;
    OGRErr DeleteLayer(int iLayer) override;

    int TestCapability(const char *) override;

    bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                        std::string &failureReason) override;

    bool DeleteFieldDomain(const std::string &name,
                           std::string &failureReason) override;

    bool UpdateFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                           std::string &failureReason) override;
};

/************************************************************************/
/*                             OGRMemDriver                             */
/************************************************************************/

class OGRMemDriver final : public OGRSFDriver
{
  public:
    virtual ~OGRMemDriver();

    const char *GetName() override;
    OGRDataSource *Open(const char *, int) override;

    virtual OGRDataSource *CreateDataSource(const char *pszName,
                                            char ** = nullptr) override;

    int TestCapability(const char *) override;
};

#endif  // ndef OGRMEM_H_INCLUDED
