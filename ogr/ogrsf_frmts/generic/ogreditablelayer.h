/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGREditableLayer class
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

#ifndef OGREDITABLELAYER_H_INCLUDED
#define OGREDITABLELAYER_H_INCLUDED

//! @cond Doxygen_Suppress
#include "ogrlayerdecorator.h"
#include <set>
#include <map>

class CPL_DLL IOGREditableLayerSynchronizer
{
    public:
        virtual ~IOGREditableLayerSynchronizer();

        virtual OGRErr EditableSyncToDisk(OGRLayer* poEditableLayer,
                                          OGRLayer** ppoDecoratedLayer) = 0;
};

class CPL_DLL OGREditableLayer : public OGRLayerDecorator
{
    CPL_DISALLOW_COPY_ASSIGN(OGREditableLayer)

  protected:

    IOGREditableLayerSynchronizer *m_poSynchronizer;
    bool                           m_bTakeOwnershipSynchronizer;
    OGRFeatureDefn                *m_poEditableFeatureDefn;
    GIntBig                        m_nNextFID;
    std::set<GIntBig>              m_oSetCreated{};
    std::set<GIntBig>              m_oSetEdited{};
    std::set<GIntBig>              m_oSetDeleted{};
    std::set<GIntBig>::iterator    m_oIter{};
    std::set<CPLString>            m_oSetDeletedFields{};
    OGRLayer                      *m_poMemLayer;
    bool                           m_bStructureModified;
    bool                           m_bSupportsCreateGeomField;
    bool                           m_bSupportsCurveGeometries;
    std::map<CPLString, int>       m_oMapEditableFDefnFieldNameToIdx{};

    OGRFeature                    *Translate(OGRFeatureDefn* poTargetDefn,
                                             OGRFeature* poSrcFeature,
                                             bool bCanStealSrcFeature,
                                             bool bHideDeletedFields);
    void                           DetectNextFID();
    int                            GetSrcGeomFieldIndex(int iGeomField);

  public:

                       OGREditableLayer(OGRLayer* poDecoratedLayer,
                                        bool bTakeOwnershipDecoratedLayer,
                                        IOGREditableLayerSynchronizer* poSynchronizer,
                                        bool bTakeOwnershipSynchronizer);
    virtual           ~OGREditableLayer();

    void                SetNextFID(GIntBig nNextFID);
    void                SetSupportsCreateGeomField(bool SupportsCreateGeomField);
    void                SetSupportsCurveGeometries(bool bSupportsCurveGeometries);

    virtual OGRGeometry *GetSpatialFilter() override;
    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilterRect( double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * ) override;
    virtual void        SetSpatialFilterRect( int iGeomField, double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY ) override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr      SetNextByIndex( GIntBig nIndex ) override;
    virtual OGRFeature *GetFeature( GIntBig nFID ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;

    virtual OGRwkbGeometryType GetGeomType() override;
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      DeleteField( int iField ) override;
    virtual OGRErr      ReorderFields( int* panMap ) override;
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags ) override;

    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poField,
                                         int bApproxOK = TRUE ) override;

    virtual OGRErr      SyncToDisk() override;

    virtual OGRErr      StartTransaction() override;
    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    virtual const char *GetGeometryColumn() override;
};
//! @endcond

#endif // OGREDITABLELAYER_H_INCLUDED
