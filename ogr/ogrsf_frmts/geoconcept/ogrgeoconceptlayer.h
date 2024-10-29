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
 * SPDX-License-Identifier: MIT
 **********************************************************************/

#include "ogrsf_frmts.h"
#include "geoconcept.h"

#ifndef GEOCONCEPT_OGR_LAYER_H_INCLUDED_
#define GEOCONCEPT_OGR_LAYER_H_INCLUDED_

/**********************************************************************/
/*            OGCGeoconceptLayer Class                           */
/**********************************************************************/
class OGRGeoconceptLayer final : public OGRLayer
{
  private:
    OGRFeatureDefn *_poFeatureDefn;

    GCSubType *_gcFeature;

  public:
    OGRGeoconceptLayer();
    virtual ~OGRGeoconceptLayer();

    OGRErr Open(GCSubType *Subclass);

    //    OGRGeometry*         GetSpatialFilter( );
    //    void                 SetSpatialFilter( OGRGeometry* poGeomIn );
    //    void                 SetSpatialFilterRect( double dfMinX, double
    //    dfMinY, double dfMaxX, double dfMaxY ); OGRErr SetAttributeFilter(
    //    const char* pszQuery );
    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    //    OGRErr               SetNextByIndex( GIntBig nIndex );

    //    OGRFeature*          GetFeature( GIntBig nFID );
    //    OGRErr               ISetFeature( OGRFeature* poFeature );
    //    OGRErr               DeleteFeature( GIntBig nFID );
    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return _poFeatureDefn;
    }  // FIXME

    OGRSpatialReference *GetSpatialRef() override;
    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce) override
    {
        return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
    }

    int TestCapability(const char *pszCap) override;
    //    const char*          GetInfo( const char* pszTag );
    OGRErr CreateField(const OGRFieldDefn *poField,
                       int bApproxOK = TRUE) override;
    OGRErr SyncToDisk() override;
    //    OGRStyleTable*       GetStyleTable( );
    //    void                 SetStyleTableDirectly( OGRStyleTable*
    //    poStyleTable ); void                 SetStyleTable( OGRStyleTable*
    //    poStyleTable ); const char*          GetFIDColumn( ); const char*
    //    GetGeometryColumn( );

    void SetSpatialRef(OGRSpatialReference *poSpatialRef);
};

#endif /* GEOCONCEPT_OGR_LAYER_H_INCLUDED_ */
