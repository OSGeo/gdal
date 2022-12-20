/******************************************************************************
 * Project:  Selafin importer
 * Purpose:  Definition of classes for OGR driver for Selafin files.
 * Author:   François Hissel, francois.hissel@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2014,  François Hissel <francois.hissel@gmail.com>
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
    SelafinTypeDef eType;
    bool bUpdate;
    int nStepNumber;
    Selafin::Header *poHeader;
    OGRFeatureDefn *poFeatureDefn;
    OGRSpatialReference *poSpatialRef;
    GIntBig nCurrentId;

  public:
    OGRSelafinLayer(const char *pszLayerNameP, int bUpdateP,
                    OGRSpatialReference *poSpatialRefP,
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
    OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce) override
    {
        return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
    }
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(OGRFieldDefn *poField, int bApproxOK = TRUE) override;
    OGRErr DeleteField(int iField) override;
    OGRErr ReorderFields(int *panMap) override;
    OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                          int nFlags) override;
    OGRErr DeleteFeature(GIntBig nFID) override;
};

/************************************************************************/
/*                           OGRSelafinDataSource                       */
/************************************************************************/

class OGRSelafinDataSource final : public OGRDataSource
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
    const char *GetName() override
    {
        return pszName;
    }
    int GetLayerCount() override
    {
        return nLayers;
    }
    OGRLayer *GetLayer(int) override;
    virtual OGRLayer *ICreateLayer(const char *pszName,
                                   OGRSpatialReference *poSpatialRefP = nullptr,
                                   OGRwkbGeometryType eGType = wkbUnknown,
                                   char **papszOptions = nullptr) override;
    virtual OGRErr DeleteLayer(int) override;
    int TestCapability(const char *) override;
    void SetDefaultSelafinName(const char *pszNameIn)
    {
        osDefaultSelafinName = pszNameIn;
    }
};

#endif /* ndef OGR_SELAFIN_H_INCLUDED */
