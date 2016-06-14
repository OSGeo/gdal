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

#include "ogrlayerdecorator.h"
#include <set>

class CPL_DLL IOGREditableLayerSynchronizer
{
    public:
        virtual ~IOGREditableLayerSynchronizer();

        virtual OGRErr EditableSyncToDisk(OGRLayer* poEditableLayer,
                                          OGRLayer** ppoDecoratedLayer) = 0;
};

class CPL_DLL OGREditableLayer : public OGRLayerDecorator
{
  protected:

    IOGREditableLayerSynchronizer *m_poSynchronizer;
    bool                           m_bTakeOwnershipSynchronizer;
    OGRFeatureDefn                *m_poEditableFeatureDefn;
    GIntBig                        m_nNextFID;
    std::set<GIntBig>              m_oSetCreated;
    std::set<GIntBig>              m_oSetEdited;
    std::set<GIntBig>              m_oSetDeleted;
    std::set<GIntBig>::iterator    m_oIter;
    std::set<CPLString>            m_oSetDeletedFields;
    OGRLayer                      *m_poMemLayer;
    bool                           m_bStructureModified;
    bool                           m_bSupportsCreateGeomField;
    bool                           m_bSupportsCurveGeometries;

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

    virtual OGRGeometry *GetSpatialFilter();
    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual void        SetSpatialFilterRect( double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY );
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    virtual void        SetSpatialFilterRect( int iGeomField, double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();
    virtual OGRErr      SetNextByIndex( GIntBig nIndex );
    virtual OGRFeature *GetFeature( GIntBig nFID );
    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( GIntBig nFID );

    virtual OGRwkbGeometryType GetGeomType();
    virtual OGRFeatureDefn *GetLayerDefn();

    virtual OGRSpatialReference *GetSpatialRef();

    virtual GIntBig     GetFeatureCount( int bForce = TRUE );
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE);
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    virtual int         TestCapability( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poField,
                                         int bApproxOK = TRUE );

    virtual OGRErr      SyncToDisk();

    virtual OGRErr      StartTransaction();
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    virtual const char *GetGeometryColumn();
};

#endif // OGREDITABLELAYER_H_INCLUDED
