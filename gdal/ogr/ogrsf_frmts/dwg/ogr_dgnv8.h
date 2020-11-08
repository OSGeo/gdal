/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGNv8
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#ifndef OGR_DGNV8_H_INCLUDED
#define OGR_DGNV8_H_INCLUDED

#include "ogrsf_frmts.h"

#include <set>
#include <utility>
#include <vector>

#include "dgnv8_headers.h"

/************************************************************************/
/*                         OGRDGNV8Services                             */
/*                                                                      */
/*      Services implementation for OGR.  Eventually we should          */
/*      override the OdExDgnSystemServices IO to use VSI*L.             */
/************************************************************************/

class OGRDGNV8Services: public OdExDgnSystemServices,
                         public OdExDgnHostAppServices
{
protected:
  ODRX_USING_HEAP_OPERATORS(OdExDgnSystemServices);
};

/************************************************************************/
/*                           OGRDGNV8Layer                              */
/************************************************************************/

class OGRDGNV8DataSource;
typedef std::pair<OGRFeature*, bool> tPairFeatureHoleFlag;

class OGRDGNV8Layer final: public OGRLayer
{
    friend class OGRDGNV8DataSource;

    OGRDGNV8DataSource         *m_poDS;
    OGRFeatureDefn             *m_poFeatureDefn;
    OdDgModelPtr                m_pModel;
    OdDgElementIteratorPtr      m_pIterator;
    std::vector<tPairFeatureHoleFlag>    m_aoPendingFeatures;
    size_t                      m_nIdxInPendingFeatures;
    std::set<CPLString>         m_aoSetIgnoredFeatureClasses;

    void                                 CleanPendingFeatures();
    std::vector<tPairFeatureHoleFlag>    CollectSubElements( OdDgElementIteratorPtr iterator,
                                                    int level );
    std::vector<tPairFeatureHoleFlag>    ProcessElement(OdDgGraphicsElementPtr element,
                                               int level = 0);
    OGRFeature*                 GetNextUnfilteredFeature();

    void                        AddToComplexCurve( OGRFeature* poFeature,
                                                   OGRCircularString* poCS,
                                                   OdDgComplexCurvePtr complexCurve );    
    void                        AddToComplexCurve( OGRFeature* poFeature,
                                                   OGRCompoundCurve* poCC,
                                                   OdDgComplexCurvePtr complexCurve );
    OdDgGraphicsElementPtr      CreateShape( OGRFeature* poFeature,
                                             OGRCurve* poCurve,
                                             bool bIsHole = false );
    OdDgGraphicsElementPtr      CreateGraphicsElement( OGRFeature *poFeature,
                                                       OGRGeometry *poGeom );
    OdDgGraphicsElementPtr      TranslateLabel(
                                    OGRFeature *poFeature, OGRPoint *poPoint );
    void                        AttachFillLinkage( OGRFeature* poFeature,
                                                   OdDgGraphicsElementPtr element);
    void                        AttachCommonAttributes( OGRFeature *poFeature,
                                                        OdDgGraphicsElementPtr element );
    int                         GetColorFromString(const char* pszColor);
    OdDgGraphicsElementPtr      GetFeatureInternal(GIntBig nFID, OdDg::OpenMode openMode);

  public:
                        OGRDGNV8Layer( OGRDGNV8DataSource* poDS,
                                       OdDgModelPtr pModel );
                        virtual ~OGRDGNV8Layer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;
    OGRFeature *        GetFeature(GIntBig nFID) override;
    OGRErr              GetExtent( OGREnvelope *psExtent, int bForce ) override;
    virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent,
                                   int bForce ) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    OGRFeatureDefn *    GetLayerDefn() override { return m_poFeatureDefn; }

    int                 TestCapability( const char * ) override;

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    OGRErr              DeleteFeature(GIntBig nFID) override;
};


/************************************************************************/
/*                         OGRDGNV8DataSource                           */
/************************************************************************/

class OGRDGNV8DataSource final: public GDALDataset
{
    OGRDGNV8Services   *m_poServices;
    OGRDGNV8Layer     **m_papoLayers;
    int                 m_nLayers;
    char              **m_papszOptions;
    OdDgDatabasePtr     m_poDb;
    bool                m_bUpdate;
    bool                m_bModified;
    CPLStringList       m_osDGNMD;

    void                InitWithSeed();

  public:
                        explicit OGRDGNV8DataSource(OGRDGNV8Services* poServices);
                        ~OGRDGNV8DataSource();

    int                 Open( const char *, bool bUpdate );
    bool                PreCreate( const char *, char ** );

    OGRLayer           *ICreateLayer( const char *,
                                     OGRSpatialReference * = nullptr,
                                     OGRwkbGeometryType = wkbUnknown,
                                     char ** = nullptr ) override;

    int                 GetLayerCount() override { return m_nLayers; }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;
    virtual void        FlushCache() override;

    virtual char **     GetMetadataDomainList() override;
    virtual char**      GetMetadata(const char* pszDomain = "") override;
    virtual const char* GetMetadataItem(const char* pszName,
                                        const char* pszDomain = "") override;

    OdDgDatabasePtr     GetDb() const { return m_poDb; }

    bool                GetUpdate() const { return m_bUpdate; }
    void                SetModified() { m_bModified = true; }

    static OdString     FromUTF8(const CPLString& str);
    static CPLString    ToUTF8(const OdString& str);
};

#endif /* ndef OGR_DGNV8_H_INCLUDED */
