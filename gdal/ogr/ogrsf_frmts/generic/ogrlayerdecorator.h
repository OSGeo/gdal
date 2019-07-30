/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLayerDecorator class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGRLAYERDECORATOR_H_INCLUDED
#define OGRLAYERDECORATOR_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrsf_frmts.h"

class CPL_DLL OGRLayerDecorator : public OGRLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRLayerDecorator)

  protected:
    OGRLayer *m_poDecoratedLayer;
    int       m_bHasOwnership;

  public:

                       OGRLayerDecorator(OGRLayer* poDecoratedLayer,
                                         int bTakeOwnership);
    virtual           ~OGRLayerDecorator();

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

    virtual const char *GetName() override;
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

    virtual OGRStyleTable *GetStyleTable() override;
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable ) override;

    virtual void        SetStyleTable(OGRStyleTable *poStyleTable) override;

    virtual OGRErr      StartTransaction() override;
    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;

    virtual OGRErr      SetIgnoredFields( const char **papszFields ) override;

    virtual char      **GetMetadata( const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" ) override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" ) override;

    OGRLayer* GetBaseLayer() const { return m_poDecoratedLayer; }
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif // OGRLAYERDECORATOR_H_INCLUDED
