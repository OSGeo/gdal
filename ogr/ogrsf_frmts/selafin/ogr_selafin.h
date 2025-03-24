/******************************************************************************
 * Project:  Selafin importer
 * Purpose:  Definition of classes for OGR driver for Selafin files.
 * Author:   François Hissel, francois.hissel@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2014,  François Hissel <francois.hissel@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SELAFIN_H_INCLUDED
#define OGR_SELAFIN_H_INCLUDED

#include "io_selafin.h"
#include "ogrsf_frmts.h"

class OGRSelafinDataSource;

typedef enum
{
    POINTS,
    ELEMENTS,
    ALL
} SelafinTypeDef;

/************************************************************************/
/*                             Range                                    */
/************************************************************************/
class Range
{
  private:
    typedef struct List
    {
        SelafinTypeDef eType;
        int nMin, nMax;
        List *poNext;

        List() : eType(POINTS), nMin(0), nMax(0), poNext(nullptr)
        {
        }

        List(SelafinTypeDef eTypeP, int nMinP, int nMaxP, List *poNextP)
            : eType(eTypeP), nMin(nMinP), nMax(nMaxP), poNext(poNextP)
        {
        }
    } List;

    List *poVals, *poActual;
    int nMaxValue;
    static void sortList(List *&poList, List *poEnd = nullptr);
    static void deleteList(List *poList);

  public:
    Range() : poVals(nullptr), poActual(nullptr), nMaxValue(0)
    {
    }

    void setRange(const char *pszStr);
    ~Range();
    void setMaxValue(int nMaxValueP);
    bool contains(SelafinTypeDef eType, int nValue) const;
    size_t getSize() const;
};

/************************************************************************/
/*                             OGRSelafinLayer                          */
/************************************************************************/

class OGRSelafinLayer final : public OGRLayer
{
  private:
    GDALDataset *m_poDS = nullptr;
    SelafinTypeDef eType;
    bool bUpdate;
    int nStepNumber;
    Selafin::Header *poHeader;
    OGRFeatureDefn *poFeatureDefn;
    OGRSpatialReference *poSpatialRef;
    GIntBig nCurrentId;

  public:
    OGRSelafinLayer(GDALDataset *poDS, const char *pszLayerNameP, int bUpdateP,
                    const OGRSpatialReference *poSpatialRefP,
                    Selafin::Header *poHeaderP, int nStepNumberP,
                    SelafinTypeDef eTypeP);
    ~OGRSelafinLayer();

    OGRSpatialReference *GetSpatialRef() override
    {
        return poSpatialRef;
    }

    int GetStepNumber()
    {
        return nStepNumber;
    }

    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig nFID) override;
    void ResetReading() override;
    OGRErr SetNextByIndex(GIntBig nIndex) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *pszCap) override;
    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(const OGRFieldDefn *poField,
                       int bApproxOK = TRUE) override;
    OGRErr DeleteField(int iField) override;
    OGRErr ReorderFields(int *panMap) override;
    OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                          int nFlags) override;
    OGRErr DeleteFeature(GIntBig nFID) override;

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
};

/************************************************************************/
/*                           OGRSelafinDataSource                       */
/************************************************************************/

class OGRSelafinDataSource final : public GDALDataset
{
  private:
    char *pszName;
    OGRSelafinLayer **papoLayers;
    Range poRange;
    int nLayers;
    bool bUpdate;
    Selafin::Header *poHeader;
    CPLString osDefaultSelafinName;
    OGRSpatialReference *poSpatialRef;

  public:
    OGRSelafinDataSource();
    virtual ~OGRSelafinDataSource();
    int Open(const char *pszFilename, int bUpdate, int bCreate);
    int OpenTable(const char *pszFilename);

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    virtual OGRErr DeleteLayer(int) override;
    int TestCapability(const char *) override;

    void SetDefaultSelafinName(const char *pszNameIn)
    {
        osDefaultSelafinName = pszNameIn;
    }
};

#endif /* ndef OGR_SELAFIN_H_INCLUDED */
