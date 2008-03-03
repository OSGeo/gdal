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
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

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
    oFieldElev.SetWidth( 7 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 6 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 6 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 5 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

void OGRXPlaneILSLayer::AddFeature(const char* pszNavaidID,
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
    oFieldElev.SetWidth( 7 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 6 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 6 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldSlavedVariation("slaved_variation_deg", OFTReal );
    oFieldSlavedVariation.SetWidth( 5 );
    oFieldSlavedVariation.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldSlavedVariation );
}

void OGRXPlaneVORLayer::AddFeature(const char* pszNavaidID,
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
    oFieldElev.SetWidth( 7 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 6 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 6 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );
}

void OGRXPlaneNDBLayer::AddFeature(const char* pszNavaidID,
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
    oFieldElev.SetWidth( 7 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 6 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 6 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 5 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );

    OGRFieldDefn oFieldGlideSlope("glide_slope", OFTReal );
    oFieldGlideSlope.SetWidth( 5 );
    oFieldGlideSlope.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldGlideSlope );
}

void OGRXPlaneGSLayer::AddFeature(const char* pszNavaidID,
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
    oFieldElev.SetWidth( 7 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldTrueHeading("true_heading_deg", OFTReal );
    oFieldTrueHeading.SetWidth( 5 );
    oFieldTrueHeading.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldTrueHeading );
}

void OGRXPlaneMarkerLayer::AddFeature(const char* pszAptICAO,
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
    oFieldElev.SetWidth( 7 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 6 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 6 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldBias("bias", OFTReal );
    oFieldBias.SetWidth( 5 );
    oFieldBias.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldBias );
}

void OGRXPlaneDMEILSLayer::AddFeature(const char* pszNavaidID,
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
    oFieldElev.SetWidth( 7 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldFreq("freq_mhz", OFTReal );
    oFieldFreq.SetWidth( 6 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );

    OGRFieldDefn oFieldRange("range_km", OFTReal );
    oFieldRange.SetWidth( 6 );
    oFieldRange.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRange );

    OGRFieldDefn oFieldBias("bias", OFTReal );
    oFieldBias.SetWidth( 5 );
    oFieldBias.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldBias );
}

void OGRXPlaneDMELayer::AddFeature(const char* pszNavaidID,
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
    oFieldElev.SetWidth( 7 );
    oFieldElev.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldElev );

    OGRFieldDefn oFieldHasTower("has_tower", OFTInteger );
    oFieldHasTower.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldHasTower );

    OGRFieldDefn oFieldHeightTower("hgt_tower_m", OFTReal );
    oFieldHeightTower.SetWidth( 7 );
    oFieldHeightTower.SetPrecision( 2 );
    poFeatureDefn->AddFieldDefn( &oFieldHeightTower );

    OGRFieldDefn oFieldTowerName("tower_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldTowerName );

}

void OGRXPlaneAPTLayer::AddFeature(const char* pszAptICAO,
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
}

/************************************************************************/
/*                         OGRXPlaneRunwayLayer                         */
/************************************************************************/


OGRXPlaneRunwayLayer::OGRXPlaneRunwayLayer() : OGRXPlaneLayer("Runway")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldAptICAO("apt_icao", OFTString );
    oFieldAptICAO.SetWidth( 4 );
    poFeatureDefn->AddFieldDefn( &oFieldAptICAO );

    OGRFieldDefn oFieldRunwayID("runway_id", OFTString );
    oFieldRunwayID.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldRunwayID );

    OGRFieldDefn oFieldWidth("width_m", OFTReal );
    poFeatureDefn->AddFieldDefn( &oFieldWidth );

    OGRFieldDefn oFieldSurface("surface", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSurface );

    OGRFieldDefn oFieldShoulder("shoulder", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldShoulder );

    OGRFieldDefn oFieldSmoothness("smoothness", OFTReal );
    poFeatureDefn->AddFieldDefn( &oFieldSmoothness );

    OGRFieldDefn oFieldCenterLineLights("centerline_lights", OFTInteger );
    poFeatureDefn->AddFieldDefn( &oFieldCenterLineLights );

    OGRFieldDefn oFieldMIRL("MIRL", OFTInteger );
    poFeatureDefn->AddFieldDefn( &oFieldMIRL );

    OGRFieldDefn oFieldDistanceRemainingSigns("distance_remaining_signs", OFTInteger );
    poFeatureDefn->AddFieldDefn( &oFieldDistanceRemainingSigns );

    OGRFieldDefn oFieldDisplacedThreshold("displaced_threshold_m", OFTReal );
    poFeatureDefn->AddFieldDefn( &oFieldDisplacedThreshold );

    OGRFieldDefn oFieldStopwayLength("stopway_length_m", OFTReal );
    poFeatureDefn->AddFieldDefn( &oFieldStopwayLength );

    OGRFieldDefn oFieldMarkings("markings", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldMarkings );

    OGRFieldDefn oFieldApproachLighting("approach_lighting", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldApproachLighting );

    OGRFieldDefn oFieldTouchdownLights("touchdown_lights", OFTInteger );
    poFeatureDefn->AddFieldDefn( &oFieldTouchdownLights );

    OGRFieldDefn oFieldREIL("REIL", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldREIL );
}

void OGRXPlaneRunwayLayer::AddFeature  (const char* pszAptICAO,
                                        const char* pszRunwayID,
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
    poFeature->SetField( nCount++, pszRunwayID );
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
    oFieldFreq.SetWidth( 6 );
    oFieldFreq.SetPrecision( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldFreq );
}

void OGRXPlaneATCFreqLayer::AddFeature (const char* pszAptICAO,
                                        const char* pszATCType,
                                        const char* pszATCFreqName,
                                        double dFrequency)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField( nCount++, pszAptICAO );
    poFeature->SetField( nCount++, pszATCType );
    poFeature->SetField( nCount++, pszATCFreqName );
    poFeature->SetField( nCount++, dFrequency );

    RegisterFeature(poFeature);
}
