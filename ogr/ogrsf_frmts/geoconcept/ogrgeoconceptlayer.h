/**********************************************************************
 * $Id: ogrgeoconceptlayer.h$
 *
 * Name:     ogrgeoconceptlayer.h
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeoconceptLayer class.
 * Language: C++
 *
 **********************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "ogrsf_frmts.h"
#include "geoconcept.h"

#ifndef GEOCONCEPT_OGR_LAYER_H_INCLUDED_
#define GEOCONCEPT_OGR_LAYER_H_INCLUDED_

/**********************************************************************/
/*            OGCGeoconceptLayer Class                           */
/**********************************************************************/
class OGRGeoconceptLayer final: public OGRLayer
{
  private:
    OGRFeatureDefn      *_poFeatureDefn;

    GCSubType           *_gcFeature;

  public:
                         OGRGeoconceptLayer();
                        virtual ~OGRGeoconceptLayer();

    OGRErr               Open( GCSubType* Subclass );

//    OGRGeometry*         GetSpatialFilter( );
//    void                 SetSpatialFilter( OGRGeometry* poGeomIn );
//    void                 SetSpatialFilterRect( double dfMinX, double dfMinY, double dfMaxX, double dfMaxY );
//    OGRErr               SetAttributeFilter( const char* pszQuery );
    void                 ResetReading() override;
    OGRFeature*          GetNextFeature() override;
//    OGRErr               SetNextByIndex( GIntBig nIndex );

//    OGRFeature*          GetFeature( GIntBig nFID );
//    OGRErr               ISetFeature( OGRFeature* poFeature );
//    OGRErr               DeleteFeature( GIntBig nFID );
    OGRErr               ICreateFeature( OGRFeature* poFeature ) override;
    OGRFeatureDefn*      GetLayerDefn( ) override { return _poFeatureDefn; } // FIXME
    OGRSpatialReference* GetSpatialRef( ) override;
    GIntBig              GetFeatureCount( int bForce = TRUE ) override;
    OGRErr               GetExtent( OGREnvelope *psExtent, int bForce = TRUE ) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
    int                  TestCapability( const char* pszCap ) override;
//    const char*          GetInfo( const char* pszTag );
    OGRErr               CreateField( OGRFieldDefn* poField, int bApproxOK = TRUE ) override;
    OGRErr               SyncToDisk( ) override;
//    OGRStyleTable*       GetStyleTable( );
//    void                 SetStyleTableDirectly( OGRStyleTable* poStyleTable );
//    void                 SetStyleTable( OGRStyleTable* poStyleTable );
//    const char*          GetFIDColumn( );
//    const char*          GetGeometryColumn( );

    void                   SetSpatialRef( OGRSpatialReference *poSpatialRef );
};

#endif /* GEOCONCEPT_OGR_LAYER_H_INCLUDED_ */
