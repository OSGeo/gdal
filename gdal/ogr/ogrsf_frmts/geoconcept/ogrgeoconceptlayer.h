/**********************************************************************
 * $Id: ogrgeoconceptlayer.h 
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

#ifndef _GEOCONCEPT_OGR_LAYER_H_INCLUDED_
#define _GEOCONCEPT_OGR_LAYER_H_INCLUDED_

/**********************************************************************/
/*            OGCGeoconceptLayer Class                           */
/**********************************************************************/
class OGRGeoconceptLayer : public OGRLayer
{
  private:
    OGRSpatialReference *_poSRS;
    OGRFeatureDefn      *_poFeatureDefn;

    GCSubType           *_gcFeature;

  public:
                         OGRGeoconceptLayer();
                        ~OGRGeoconceptLayer();

    OGRErr               Open( GCSubType* Subclass );

//    OGRGeometry*         GetSpatialFilter( );
//    void                 SetSpatialFilter( OGRGeometry* poGeomIn );
//    void                 SetSpatialFilterRect( double dfMinX, double dfMinY, double dfMaxX, double dfMaxY );
//    OGRErr               SetAttributeFilter( const char* pszQuery );
    void                 ResetReading();
    OGRFeature*          GetNextFeature();
//    OGRErr               SetNextByIndex( long nIndex );

//    OGRFeature*          GetFeature( long nFID );
//    OGRErr               SetFeature( OGRFeature* poFeature );
//    OGRErr               DeleteFeature( long nFID );
    OGRErr               CreateFeature( OGRFeature* poFeature );
    OGRFeatureDefn*      GetLayerDefn( ) { return _poFeatureDefn; } // FIXME
    OGRSpatialReference* GetSpatialRef( );
    int                  GetFeatureCount( int bForce = TRUE );
    OGRErr               GetExtent( OGREnvelope *psExtent, int bForce = TRUE );
    int                  TestCapability( const char* pszCap );
//    const char*          GetInfo( const char* pszTag );
    OGRErr               CreateField( OGRFieldDefn* poField, int bApproxOK = TRUE );
    OGRErr               SyncToDisk( );
//    OGRStyleTable*       GetStyleTable( );
//    void                 SetStyleTableDirectly( OGRStyleTable* poStyleTable );
//    void                 SetStyleTable( OGRStyleTable* poStyleTable );
//    const char*          GetFIDColumn( );
//    const char*          GetGeometryColumn( );

    void                   SetSpatialRef( OGRSpatialReference *poSpatialRef );
};

#endif /* _GEOCONCEPT_OGR_LAYER_H_INCLUDED_ */
