/******************************************************************************
 * $Id: ogrxplanelayer.cpp
 *
 * Project:  XPlane Translator
 * Purpose:  Implements OGRXPlaneLayer class.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#include "ogr_xplane.h"
#include "ogr_xplane_geo_utils.h"


/************************************************************************/
/*                            OGRXPlaneLayer()                          */
/************************************************************************/

OGRXPlaneLayer::OGRXPlaneLayer( const char* pszLayerName )

{
    nFID = 0;
    nCurrentID = 0;
    papoFeatures = NULL;

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    poFeatureDefn->Reference();
}


/************************************************************************/
/*                            ~OGRXPlaneLayer()                            */
/************************************************************************/

OGRXPlaneLayer::~OGRXPlaneLayer()

{
    int i;
    poFeatureDefn->Release();
    for(i=0;i<nFID;i++)
    {
        delete papoFeatures[i];
    }
    CPLFree(papoFeatures);
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRXPlaneLayer::ResetReading()

{
    nCurrentID = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRXPlaneLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    while(nCurrentID < nFID)
    {
        poFeature = papoFeatures[nCurrentID ++];
        CPLAssert (poFeature != NULL);

        if( (m_poFilterGeom == NULL
              || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
        {
                return poFeature->Clone();
        }
    }

    return NULL;
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature *  OGRXPlaneLayer::GetFeature( long nFID )
{
    if (nFID >= 0 && nFID < this->nFID)
    {
        return papoFeatures[nFID]->Clone();
    }
    else
    {
        return NULL;
    }
}

/************************************************************************/
/*                      GetFeatureCount()                               */
/************************************************************************/

int  OGRXPlaneLayer::GetFeatureCount( int bForce )
{
    if (m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return nFID;
    else
        return OGRLayer::GetFeatureCount( bForce ) ;
}

/************************************************************************/
/*                       TestCapability()                               */
/************************************************************************/

int  OGRXPlaneLayer::TestCapability( const char * pszCap )
{
    return FALSE;
}


/************************************************************************/
/*                       RegisterFeature()                              */
/************************************************************************/

void OGRXPlaneLayer::RegisterFeature( OGRFeature* poFeature )
{
    CPLAssert (poFeature != NULL);

    papoFeatures = (OGRFeature**) CPLRealloc(papoFeatures, (nFID + 1) * sizeof(OGRFeature*));
    papoFeatures[nFID] = poFeature;
    poFeature->SetFID( nFID );
    nFID ++;
}

/************************************************************************/
/*                           OGRXPlaneILSLayer                          */
/************************************************************************/

OGRXPlaneILSLayer::OGRXPlaneILSLayer() : OGRXPlaneLayer("ILS")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

OGRFeature*
     OGRXPlaneILSLayer::AddFeature(const char* pszNavaidID,
                                   const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfTrueHeading)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfTrueHeading );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneVORLayer                          */
/************************************************************************/


OGRXPlaneVORLayer::OGRXPlaneVORLayer() : OGRXPlaneLayer("VOR")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldName("navaid_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldSlavedVariation("slaved_variation_deg", OFTReal );
    oFieldSlavedVariation.SetWidth( 6 );
    oFieldSlavedVariation.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSlavedVariation );
}

OGRFeature*
     OGRXPlaneVORLayer::AddFeature(const char* pszNavaidID,
                                   const char* pszNavaidName,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfSlavedVariation)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszNavaidName );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfSlavedVariation );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneNDBLayer                          */
/************************************************************************/

OGRXPlaneNDBLayer::OGRXPlaneNDBLayer() : OGRXPlaneLayer("NDB")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldName("navaid_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );
}

OGRFeature*
     OGRXPlaneNDBLayer::AddFeature(const char* pszNavaidID,
                                   const char* pszNavaidName,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszNavaidName );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneGSLayer                          */
/************************************************************************/

OGRXPlaneGSLayer::OGRXPlaneGSLayer() : OGRXPlaneLayer("GS")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldGlideSlope("glide_slope", OFTReal );
    oFieldGlideSlope.SetWidth( 6 );
    oFieldGlideSlope.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldGlideSlope );
}

OGRFeature*
     OGRXPlaneGSLayer::AddFeature(const char* pszNavaidID,
                                   const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfTrueHeading,
                                   double dfSlope)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfTrueHeading );
    poFeature->SetField( nCount++, dfSlope );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                         OGRXPlaneMarkerLayer                         */
/************************************************************************/

OGRXPlaneMarkerLayer::OGRXPlaneMarkerLayer() : OGRXPlaneLayer("Marker")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

OGRFeature*
     OGRXPlaneMarkerLayer::AddFeature(const char* pszAptICAO,
                                      const char* pszRwyNum,
                                      const char* pszSubType,
                                      double dfLat,
                                      double dfLon,
                                      double dfEle,
                                      double dfTrueHeading)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfTrueHeading );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneDMEILSLayer                          */
/************************************************************************/

OGRXPlaneDMEILSLayer::OGRXPlaneDMEILSLayer() : OGRXPlaneLayer("DMEILS")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldBias("bias", OFTReal );
    oFieldBias.SetWidth( 6 );
    oFieldBias.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldBias );
}


OGRFeature*
      OGRXPlaneDMEILSLayer::AddFeature(const char* pszNavaidID,
                                      const char* pszAptICAO,
                                      const char* pszRwyNum,
                                      double dfLat,
                                      double dfLon,
                                      double dfEle,
                                      double dfFreq,
                                      double dfRange,
                                      double dfBias)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfBias );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneDMELayer                          */
/************************************************************************/


OGRXPlaneDMELayer::OGRXPlaneDMELayer() : OGRXPlaneLayer("DME")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("navaid_id", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldName("navaid_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldSubType("subtype", OFTString );
    oFieldSubType.SetWidth( 10 );
    poFeatureDefn->AddFieldDefn( &oFieldSubType );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 7 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldBias("bias", OFTReal );
    oFieldBias.SetWidth( 6 );
    oFieldBias.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldBias );
}

OGRFeature*
     OGRXPlaneDMELayer::AddFeature(const char* pszNavaidID,
                                   const char* pszNavaidName,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfBias)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszNavaidID );
    poFeature->SetField( nCount++, pszNavaidName );
    poFeature->SetField( nCount++, pszSubType );
    poFeature->SetField( nCount++, dfEle );
    poFeature->SetField( nCount++, dfFreq );
    poFeature->SetField( nCount++, dfRange );
    poFeature->SetField( nCount++, dfBias );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                           OGRXPlaneAPTLayer                          */
/************************************************************************/


OGRXPlaneAPTLayer::OGRXPlaneAPTLayer() : OGRXPlaneLayer("APT")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldID("apt_icao", OFTString );
    oFieldID.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldID );

    OGRFieldDefn oFieldName("apt_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldElev("elevation_m", OFTReal );
    oFieldElev.SetWidth( 8 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldHasTower("has_tower", OFTInteger );
    oFieldHasTower.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldHasTower );

    OGRFieldDefn oFieldHeightTower("hgt_tower_m", OFTReal );
    oFieldHeightTower.SetWidth( 8 );
    oFieldHeightTower.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldHeightTower );

    OGRFieldDefn oFieldTowerName("tower_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldTowerName );

}

OGRFeature*
     OGRXPlaneAPTLayer::AddFeature(const char* pszAptICAO,
                                   const char* pszAptName,
                                   double dfElevation,
                                   int bHasCoordinates,
                                   double dfLat,
                                   double dfLon,
                                   int bHasTower,
                                   double dfHeightTower,
                                   const char* pszTowerName)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszAptName );
    poFeature->SetField( nCount++, dfElevation );
    poFeature->SetField( nCount++, bHasTower );
    if (bHasCoordinates)
    {
        poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    }
    if (bHasTower)
    {
        poFeature->SetField( nCount++, dfHeightTower );
        poFeature->SetField( nCount++, pszTowerName );
    }

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                 OGRXPlaneRunwayThresholdLayer                        */
/************************************************************************/


OGRXPlaneRunwayThresholdLayer::OGRXPlaneRunwayThresholdLayer() : OGRXPlaneLayer("RunwayThreshold")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldShoulder("shoulder", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldShoulder );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldCenterLineLights("centerline_lights", OFTInteger );
    oFieldCenterLineLights.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldCenterLineLights );

    OGRFieldDefn oFieldMIRL("MIRL", OFTInteger );
    oFieldMIRL.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldMIRL );

    OGRFieldDefn oFieldDistanceRemainingSigns("distance_remaining_signs", OFTInteger );
    oFieldDistanceRemainingSigns.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldDistanceRemainingSigns );

    OGRFieldDefn oFieldDisplacedThreshold("displaced_threshold_m", OFTReal );
    oFieldDisplacedThreshold.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldDisplacedThreshold );

    OGRFieldDefn oFieldStopwayLength("stopway_length_m", OFTReal );
    oFieldStopwayLength.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldStopwayLength );

    OGRFieldDefn oFieldMarkings("markings", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldMarkings );

    OGRFieldDefn oFieldApproachLighting("approach_lighting", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldApproachLighting );

    OGRFieldDefn oFieldTouchdownLights("touchdown_lights", OFTInteger );
    oFieldTouchdownLights.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldTouchdownLights );

    OGRFieldDefn oFieldREIL("REIL", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldREIL );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

OGRFeature*
     OGRXPlaneRunwayThresholdLayer::AddFeature  (const char* pszAptICAO,
                                        const char* pszRwyNum,
                                        double dfLat,
                                        double dfLon,
                                        double dfWidth,
                                        const char* pszSurfaceType,
                                        const char* pszShoulderType,
                                        double dfSmoothness,
                                        int bHasCenterLineLights,
                                        int bHasMIRL,
                                        int bHasDistanceRemainingSigns,
                                        double dfDisplacedThresholdLength,
                                        double dfStopwayLength,
                                        const char* pszMarkings,
                                        const char* pszApproachLightingCode,
                                        int bHasTouchdownLights,
                                        const char* pszREIL)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, pszShoulderType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bHasCenterLineLights );
    poFeature->SetField( nCount++, bHasMIRL );
    poFeature->SetField( nCount++, bHasDistanceRemainingSigns );
    poFeature->SetField( nCount++, dfDisplacedThresholdLength );
    poFeature->SetField( nCount++, dfStopwayLength );
    poFeature->SetField( nCount++, pszMarkings );
    poFeature->SetField( nCount++, pszApproachLightingCode );
    poFeature->SetField( nCount++, bHasTouchdownLights );
    poFeature->SetField( nCount++, pszREIL );

    RegisterFeature(poFeature);

    return poFeature;
}

void OGRXPlaneRunwayThresholdLayer::SetRunwayLengthAndHeading(OGRFeature* poFeature,
                                                     double dfLength,
                                                     double dfHeading)
{
    int nCount = 15;
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfHeading );
}


/************************************************************************/
/*                         OGRXPlaneRunwayLayer                         */
/************************************************************************/



OGRXPlaneRunwayLayer::OGRXPlaneRunwayLayer() : OGRXPlaneLayer("RunwayPolygon")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum1("rwy_num1", OFTString );
    oFieldRwyNum1.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum1 );

    OGRFieldDefn oFieldRwyNum2("rwy_num2", OFTString );
    oFieldRwyNum2.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum2 );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldShoulder("shoulder", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldShoulder );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldCenterLineLights("centerline_lights", OFTInteger );
    oFieldCenterLineLights.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldCenterLineLights );

    OGRFieldDefn oFieldMIRL("MIRL", OFTInteger );
    oFieldMIRL.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldMIRL );

    OGRFieldDefn oFieldDistanceRemainingSigns("distance_remaining_signs", OFTInteger );
    oFieldDistanceRemainingSigns.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldDistanceRemainingSigns );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

OGRFeature*
     OGRXPlaneRunwayLayer::AddFeature  (const char* pszAptICAO,
                                        const char* pszRwyNum1,
                                        const char* pszRwyNum2,
                                        double dfLat1,
                                        double dfLon1,
                                        double dfLat2,
                                        double dfLon2,
                                        double dfWidth,
                                        const char* pszSurfaceType,
                                        const char* pszShoulderType,
                                        double dfSmoothness,
                                        int bHasCenterLineLights,
                                        int bHasMIRL,
                                        int bHasDistanceRemainingSigns)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    double dfLength = OGRXPlane_Distance(dfLat1, dfLon1, dfLat2, dfLon2);
    double dfTrack12 = OGRXPlane_Track(dfLat1, dfLon1, dfLat2, dfLon2);
    double dfTrack21 = OGRXPlane_Track(dfLat2, dfLon2, dfLat1, dfLon1);
    double adfLat[4], adfLon[4];
    
    OGRXPlane_ExtendPosition(dfLat1, dfLon1, dfWidth / 2, dfTrack12 - 90, &adfLat[0], &adfLon[0]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfTrack21 + 90, &adfLat[1], &adfLon[1]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfTrack21 - 90, &adfLat[2], &adfLon[2]);
    OGRXPlane_ExtendPosition(dfLat1, dfLon1, dfWidth / 2, dfTrack12 + 90, &adfLat[3], &adfLon[3]);
    
    OGRLinearRing* linearRing = new OGRLinearRing();
    linearRing->setNumPoints(5);
    int i;
    for(i=0;i<4;i++)
        linearRing->setPoint(i, adfLon[i], adfLat[i]);
    linearRing->setPoint(4, adfLon[0], adfLat[0]);
    OGRPolygon* polygon = new OGRPolygon();
     polygon->addRingDirectly( linearRing );
    poFeature->SetGeometryDirectly( polygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum1 );
    poFeature->SetField( nCount++, pszRwyNum2 );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, pszShoulderType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bHasCenterLineLights );
    poFeature->SetField( nCount++, bHasMIRL );
    poFeature->SetField( nCount++, bHasDistanceRemainingSigns );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfTrack12 );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                 OGRXPlaneWaterRunwayThresholdLayer                        */
/************************************************************************/


OGRXPlaneWaterRunwayThresholdLayer::OGRXPlaneWaterRunwayThresholdLayer() : OGRXPlaneLayer("WaterRunwayThreshold")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldHasBuoys("has_buoys", OFTInteger );
    oFieldHasBuoys.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldHasBuoys );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

OGRFeature*
     OGRXPlaneWaterRunwayThresholdLayer::AddFeature  (const char* pszAptICAO,
                                                      const char* pszRwyNum,
                                                      double dfLat,
                                                      double dfLon,
                                                      double dfWidth,
                                                      int bBuoys)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, bBuoys );

    RegisterFeature(poFeature);

    return poFeature;
}

void OGRXPlaneWaterRunwayThresholdLayer::SetRunwayLengthAndHeading(OGRFeature* poFeature,
                                                     double dfLength,
                                                     double dfHeading)
{
    int nCount = 4;
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfHeading );
}


/************************************************************************/
/*                         OGRXPlaneWaterRunwayLayer                         */
/************************************************************************/



OGRXPlaneWaterRunwayLayer::OGRXPlaneWaterRunwayLayer() : OGRXPlaneLayer("WaterRunwayPolygon")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum1("rwy_num1", OFTString );
    oFieldRwyNum1.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum1 );

    OGRFieldDefn oFieldRwyNum2("rwy_num2", OFTString );
    oFieldRwyNum2.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum2 );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldHasBuoys("has_buoys", OFTInteger );
    oFieldHasBuoys.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldHasBuoys );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

OGRFeature*
     OGRXPlaneWaterRunwayLayer::AddFeature  (const char* pszAptICAO,
                                             const char* pszRwyNum1,
                                             const char* pszRwyNum2,
                                             double dfLat1,
                                             double dfLon1,
                                             double dfLat2,
                                             double dfLon2,
                                             double dfWidth,
                                             int bBuoys)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    double dfLength = OGRXPlane_Distance(dfLat1, dfLon1, dfLat2, dfLon2);
    double dfTrack12 = OGRXPlane_Track(dfLat1, dfLon1, dfLat2, dfLon2);
    double dfTrack21 = OGRXPlane_Track(dfLat2, dfLon2, dfLat1, dfLon1);
    double adfLat[4], adfLon[4];
    
    OGRXPlane_ExtendPosition(dfLat1, dfLon1, dfWidth / 2, dfTrack12 - 90, &adfLat[0], &adfLon[0]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfTrack21 + 90, &adfLat[1], &adfLon[1]);
    OGRXPlane_ExtendPosition(dfLat2, dfLon2, dfWidth / 2, dfTrack21 - 90, &adfLat[2], &adfLon[2]);
    OGRXPlane_ExtendPosition(dfLat1, dfLon1, dfWidth / 2, dfTrack12 + 90, &adfLat[3], &adfLon[3]);
    
    OGRLinearRing* linearRing = new OGRLinearRing();
    linearRing->setNumPoints(5);
    int i;
    for(i=0;i<4;i++)
        linearRing->setPoint(i, adfLon[i], adfLat[i]);
    linearRing->setPoint(4, adfLon[0], adfLat[0]);
    OGRPolygon* polygon = new OGRPolygon();
     polygon->addRingDirectly( linearRing );
    poFeature->SetGeometryDirectly( polygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum1 );
    poFeature->SetField( nCount++, pszRwyNum2 );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, bBuoys );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfTrack12 );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                     OGRXPlaneHelipadLayer                            */
/************************************************************************/


OGRXPlaneHelipadLayer::OGRXPlaneHelipadLayer() : OGRXPlaneLayer("Helipad")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldHelipadName("helipad_name", OFTString );
    oFieldHelipadName.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldHelipadName );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldMarkings("markings", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldMarkings );

    OGRFieldDefn oFieldShoulder("shoulder", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldShoulder );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldYellowEdgeLighting("edge_lighting", OFTInteger );
    oFieldYellowEdgeLighting.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldYellowEdgeLighting );

}

OGRFeature*
     OGRXPlaneHelipadLayer::AddFeature (const char* pszAptICAO,
                                        const char* pszHelipadNum,
                                        double dfLat,
                                        double dfLon,
                                        double dfTrueHeading,
                                        double dfLength,
                                        double dfWidth,
                                        const char* pszSurfaceType,
                                        const char* pszMarkings,
                                        const char* pszShoulderType,
                                        double dfSmoothness,
                                        int bYellowEdgeLights)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszHelipadNum );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, dfTrueHeading );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, pszMarkings );
    poFeature->SetField( nCount++, pszShoulderType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bYellowEdgeLights );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                 OGRXPlaneHelipadPolygonLayer                         */
/************************************************************************/


OGRXPlaneHelipadPolygonLayer::OGRXPlaneHelipadPolygonLayer() : OGRXPlaneLayer("HelipadPolygon")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldHelipadName("helipad_name", OFTString );
    oFieldHelipadName.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldHelipadName );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldMarkings("markings", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldMarkings );

    OGRFieldDefn oFieldShoulder("shoulder", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldShoulder );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldYellowEdgeLighting("edge_lighting", OFTInteger );
    oFieldYellowEdgeLighting.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldYellowEdgeLighting );

}

OGRFeature*
     OGRXPlaneHelipadPolygonLayer::AddFeature (const char* pszAptICAO,
                                               const char* pszHelipadNum,
                                               double dfLat,
                                               double dfLon,
                                               double dfTrueHeading,
                                               double dfLength,
                                               double dfWidth,
                                               const char* pszSurfaceType,
                                               const char* pszMarkings,
                                               const char* pszShoulderType,
                                               double dfSmoothness,
                                               int bYellowEdgeLights)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    double dfBeforeLat, dfBeforeLon;
    double dfAfterLat, dfAfterLon;
    double adfLat[4], adfLon[4];

    OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading + 180, &dfBeforeLat, &dfBeforeLon);
    OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading, &dfAfterLat, &dfAfterLon);

    OGRXPlane_ExtendPosition(dfBeforeLat, dfBeforeLon, dfWidth / 2, dfTrueHeading - 90, &adfLat[0], &adfLon[0]);
    OGRXPlane_ExtendPosition(dfAfterLat, dfAfterLon, dfWidth / 2, dfTrueHeading - 90, &adfLat[1], &adfLon[1]);
    OGRXPlane_ExtendPosition(dfAfterLat, dfAfterLon, dfWidth / 2, dfTrueHeading + 90, &adfLat[2], &adfLon[2]);
    OGRXPlane_ExtendPosition(dfBeforeLat, dfBeforeLon, dfWidth / 2, dfTrueHeading + 90, &adfLat[3], &adfLon[3]);

    OGRLinearRing* linearRing = new OGRLinearRing();
    linearRing->setNumPoints(5);
    int i;
    for(i=0;i<4;i++)
        linearRing->setPoint(i, adfLon[i], adfLat[i]);
    linearRing->setPoint(4, adfLon[0], adfLat[0]);
    OGRPolygon* polygon = new OGRPolygon();
     polygon->addRingDirectly( linearRing );
    poFeature->SetGeometryDirectly( polygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszHelipadNum );
    poFeature->SetField( nCount++, dfTrueHeading );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, pszMarkings );
    poFeature->SetField( nCount++, pszShoulderType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bYellowEdgeLights );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                 OGRXPlaneTaxiwayRectangleLayer                         */
/************************************************************************/


OGRXPlaneTaxiwayRectangleLayer::OGRXPlaneTaxiwayRectangleLayer() : OGRXPlaneLayer("TaxiwayRectangle")
{
    poFeatureDefn->SetGeomType( wkbPolygon );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldLength("length_m", OFTReal );
    oFieldLength.SetWidth( 5 );
    poFeatureDefn->AddFieldDefn( &oFieldLength );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    oFieldWidth.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    oFieldSmoothness.SetWidth( 4 );
    oFieldSmoothness.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldBlueEdgeLighting("edge_lighting", OFTInteger );
    oFieldBlueEdgeLighting.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldBlueEdgeLighting );

}

OGRFeature*
     OGRXPlaneTaxiwayRectangleLayer::AddFeature(const char* pszAptICAO,
                                                double dfLat,
                                                double dfLon,
                                                double dfTrueHeading,
                                                double dfLength,
                                                double dfWidth,
                                                const char* pszSurfaceType,
                                                double dfSmoothness,
                                                int bBlueEdgeLights)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    double dfBeforeLat, dfBeforeLon;
    double dfAfterLat, dfAfterLon;
    double adfLat[4], adfLon[4];

    OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading + 180, &dfBeforeLat, &dfBeforeLon);
    OGRXPlane_ExtendPosition(dfLat, dfLon, dfLength / 2, dfTrueHeading, &dfAfterLat, &dfAfterLon);

    OGRXPlane_ExtendPosition(dfBeforeLat, dfBeforeLon, dfWidth / 2, dfTrueHeading - 90, &adfLat[0], &adfLon[0]);
    OGRXPlane_ExtendPosition(dfAfterLat, dfAfterLon, dfWidth / 2, dfTrueHeading - 90, &adfLat[1], &adfLon[1]);
    OGRXPlane_ExtendPosition(dfAfterLat, dfAfterLon, dfWidth / 2, dfTrueHeading + 90, &adfLat[2], &adfLon[2]);
    OGRXPlane_ExtendPosition(dfBeforeLat, dfBeforeLon, dfWidth / 2, dfTrueHeading + 90, &adfLat[3], &adfLon[3]);

    OGRLinearRing* linearRing = new OGRLinearRing();
    linearRing->setNumPoints(5);
    int i;
    for(i=0;i<4;i++)
        linearRing->setPoint(i, adfLon[i], adfLat[i]);
    linearRing->setPoint(4, adfLon[0], adfLat[0]);
    OGRPolygon* polygon = new OGRPolygon();
     polygon->addRingDirectly( linearRing );
    poFeature->SetGeometryDirectly( polygon );

    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, dfTrueHeading );
    poFeature->SetField( nCount++, dfLength );
    poFeature->SetField( nCount++, dfWidth );
    poFeature->SetField( nCount++, pszSurfaceType );
    poFeature->SetField( nCount++, dfSmoothness );
    poFeature->SetField( nCount++, bBlueEdgeLights );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                         OGRXPlaneATCFreqLayer                         */
/************************************************************************/


OGRXPlaneATCFreqLayer::OGRXPlaneATCFreqLayer() : OGRXPlaneLayer("ATCFreq")
{
    poFeatureDefn->SetGeomType( wkbNone );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldATCFreqType("atc_type", OFTString );
    oFieldATCFreqType.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldATCFreqType );

    OGRFieldDefn oFieldATCFreqName("freq_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldATCFreqName );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 7 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );
}

OGRFeature*
     OGRXPlaneATCFreqLayer::AddFeature (const char* pszAptICAO,
                                        const char* pszATCType,
                                        const char* pszATCFreqName,
                                        double dfFrequency)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszATCType );
    poFeature->SetField( nCount++, pszATCFreqName );
    poFeature->SetField( nCount++, dfFrequency );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                     OGRXPlaneStartupLocationLayer                    */
/************************************************************************/

OGRXPlaneStartupLocationLayer::OGRXPlaneStartupLocationLayer() : OGRXPlaneLayer("StartupLocation")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

OGRFeature*
     OGRXPlaneStartupLocationLayer::AddFeature (const char* pszAptICAO,
                                                const char* pszName,
                                                double dfLat,
                                                double dfLon,
                                                double dfTrueHeading)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszName );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, dfTrueHeading );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                      OGRXPlaneAPTLightBeaconLayer                    */
/************************************************************************/

OGRXPlaneAPTLightBeaconLayer::OGRXPlaneAPTLightBeaconLayer() : OGRXPlaneLayer("APTLightBeacon")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldColor("color", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldColor );
}

OGRFeature*
     OGRXPlaneAPTLightBeaconLayer::AddFeature (const char* pszAptICAO,
                                               const char* pszName,
                                               double dfLat,
                                               double dfLon,
                                               const char* pszColor)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszName );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszColor );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                        OGRXPlaneAPTWindsockLayer                     */
/************************************************************************/

OGRXPlaneAPTWindsockLayer::OGRXPlaneAPTWindsockLayer() : OGRXPlaneLayer("APTWindsock")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldIsIlluminated("is_illuminated", OFTInteger );
    oFieldIsIlluminated.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldIsIlluminated );
}

OGRFeature*
     OGRXPlaneAPTWindsockLayer::AddFeature (const char* pszAptICAO,
                                            const char* pszName,
                                            double dfLat,
                                            double dfLon,
                                            int bIsIllumnited)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszName );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, bIsIllumnited );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                        OGRXPlaneTaxiwaySignLayer                     */
/************************************************************************/

OGRXPlaneTaxiwaySignLayer::OGRXPlaneTaxiwaySignLayer() : OGRXPlaneLayer("TaxiwaySign")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldText("text", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldText );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldSize("size", OFTInteger );
    oFieldSize.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldSize );
}

OGRFeature*
     OGRXPlaneTaxiwaySignLayer::AddFeature (const char* pszAptICAO,
                                            const char* pszText,
                                            double dfLat,
                                            double dfLon,
                                            double dfHeading,
                                            int nSize)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszText );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, dfHeading );
    poFeature->SetField( nCount++, nSize );

    RegisterFeature(poFeature);

    return poFeature;
}


/************************************************************************/
/*                   OGRXPlane_VASI_PAPI_WIGWAG_Layer                   */
/************************************************************************/

OGRXPlane_VASI_PAPI_WIGWAG_Layer::OGRXPlane_VASI_PAPI_WIGWAG_Layer() : OGRXPlaneLayer("VASI_PAPI_WIGWAG")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRwyNum("rwy_num", OFTString );
    oFieldRwyNum.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRwyNum );

    OGRFieldDefn oFieldType("type", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldType );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 6 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldVisualGlidePathAngle("visual_glide_deg", OFTReal );
    oFieldVisualGlidePathAngle.SetWidth( 4 );
    oFieldVisualGlidePathAngle.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldVisualGlidePathAngle );
}

OGRFeature*
     OGRXPlane_VASI_PAPI_WIGWAG_Layer::AddFeature (const char* pszAptICAO,
                                                   const char* pszRwyNum,
                                                   const char* pszObjectType,
                                                   double dfLat,
                                                   double dfLon,
                                                   double dfHeading,
                                                   double dfVisualGlidePathAngle)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszRwyNum );
    poFeature->SetField( nCount++, pszObjectType );
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, dfHeading );
    poFeature->SetField( nCount++, dfVisualGlidePathAngle );

    RegisterFeature(poFeature);

    return poFeature;
}
