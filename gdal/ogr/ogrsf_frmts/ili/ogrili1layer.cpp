/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1 Translator
 * Purpose:  Implements OGRILI1Layer class.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_ili1.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_geos.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRILI1Layer()                              */
/************************************************************************/

OGRILI1Layer::OGRILI1Layer( OGRFeatureDefn* poFeatureDefnIn,
                            GeomFieldInfos oGeomFieldInfosIn,
                            OGRILI1DataSource *poDSIn )

{
    poDS = poDSIn;

    poFeatureDefn = poFeatureDefnIn;
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    oGeomFieldInfos = oGeomFieldInfosIn;

    nFeatures = 0;
    papoFeatures = NULL;
    nFeatureIdx = 0;

    bGeomsJoined = FALSE;
}

/************************************************************************/
/*                           ~OGRILI1Layer()                           */
/************************************************************************/

OGRILI1Layer::~OGRILI1Layer()
{
    int i;

    for(i=0;i<nFeatures;i++)
    {
        delete papoFeatures[i];
    }
    CPLFree(papoFeatures);

    if( poFeatureDefn )
        poFeatureDefn->Release();
}


OGRErr OGRILI1Layer::AddFeature (OGRFeature *poFeature)
{
    nFeatures++;

    papoFeatures = (OGRFeature **)
        CPLRealloc( papoFeatures, sizeof(void*) * nFeatures );

    papoFeatures[nFeatures-1] = poFeature;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRILI1Layer::ResetReading(){
    nFeatureIdx = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRILI1Layer::GetNextFeature()
{
    OGRFeature *poFeature;

    if (!bGeomsJoined) JoinGeomLayers();

    while(nFeatureIdx < nFeatures)
    {
        poFeature = GetNextFeatureRef();
        if (poFeature)
            return poFeature->Clone();
    }
    return NULL;
}

OGRFeature *OGRILI1Layer::GetNextFeatureRef() {
    OGRFeature *poFeature = NULL;
    if (nFeatureIdx < nFeatures)
    {
      poFeature = papoFeatures[nFeatureIdx++];
      //apply filters
      if( (m_poFilterGeom == NULL
           || FilterGeometry( poFeature->GetGeometryRef() ) )
          && (m_poAttrQuery == NULL
              || m_poAttrQuery->Evaluate( poFeature )) )
          return poFeature;
    }
    return NULL;
}

/************************************************************************/
/*                             GetFeatureRef()                          */
/************************************************************************/

OGRFeature *OGRILI1Layer::GetFeatureRef( long nFID )

{
    OGRFeature *poFeature;

    ResetReading();
    while( (poFeature = GetNextFeatureRef()) != NULL )
    {
        if( poFeature->GetFID() == nFID )
            return poFeature;
    }

    return NULL;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRILI1Layer::GetFeatureCount( int bForce )
{
    if (m_poFilterGeom == NULL && m_poAttrQuery == NULL &&
        1 /*poAreaLineLayer == NULL*/)
    {
        return nFeatures;
    }
    else
    {
        return OGRLayer::GetFeatureCount(bForce);
    }
}

static char* d2str(double val)
{
    static char strbuf[255];
    if( val == (int) val )
        sprintf( strbuf, "%d", (int) val );
    else if( fabs(val) < 370 )
        CPLsprintf( strbuf, "%.16g", val );
    else if( fabs(val) > 100000000.0  )
        CPLsprintf( strbuf, "%.16g", val );
    else
        CPLsprintf( strbuf, "%.3f", val );
    return strbuf;
}

static void AppendCoordinateList( OGRLineString *poLine, OGRILI1DataSource *poDS)
{
    int         b3D = wkbHasZ(poLine->getGeometryType());

    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        if (iPoint == 0) VSIFPrintf( poDS->GetTransferFile(), "STPT" );
        else VSIFPrintf( poDS->GetTransferFile(), "LIPT" );
        VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poLine->getX(iPoint)) );
        VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poLine->getY(iPoint)) );
        if (b3D) VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poLine->getZ(iPoint)) );
        VSIFPrintf( poDS->GetTransferFile(), "\n" );
    }
    VSIFPrintf( poDS->GetTransferFile(), "ELIN\n" );
}

static void AppendCoumpoundCurve( OGRCompoundCurve *poCC, OGRILI1DataSource *poDS)
{
    for( int iMember = 0; iMember < poCC->getNumCurves(); iMember++)
    {
        OGRCurve *poGeometry = poCC->getCurve( iMember );
        int b3D = wkbHasZ(poGeometry->getGeometryType());
        int bIsArc = (poGeometry->getGeometryType() == wkbCircularString
              || poGeometry->getGeometryType() == wkbCircularStringZ );
        OGRSimpleCurve *poLine = (OGRSimpleCurve *)poGeometry;
        for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
        {
            //Skip last point in curve member
            if (iPoint == poLine->getNumPoints()-1 && iMember < poCC->getNumCurves()-1)
                continue;
            if (iMember == 0 && iPoint == 0) VSIFPrintf( poDS->GetTransferFile(), "STPT" );
            else if (bIsArc && iPoint == 1) VSIFPrintf( poDS->GetTransferFile(), "ARCP" );
            else VSIFPrintf( poDS->GetTransferFile(), "LIPT" );
            VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poLine->getX(iPoint)) );
            VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poLine->getY(iPoint)) );
            if (b3D) VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poLine->getZ(iPoint)) );
            VSIFPrintf( poDS->GetTransferFile(), "\n" );
        }
    }
    VSIFPrintf( poDS->GetTransferFile(), "ELIN\n" );
}

int OGRILI1Layer::GeometryAppend( OGRGeometry *poGeometry )
{
    //CPLDebug( "OGR_ILI", "OGRILI1Layer::GeometryAppend OGRGeometryType: %s", OGRGeometryTypeToName(poGeometry->getGeometryType()));
/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint )
    {
        /* embedded in from non-geometry fields */
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPoint25D )
    {
        /* embedded in from non-geometry fields */
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbLineString
             || poGeometry->getGeometryType() == wkbLineString25D )
    {
        AppendCoordinateList( (OGRLineString *) poGeometry, poDS );
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPolygon
             || poGeometry->getGeometryType() == wkbPolygon25D )
    {
        OGRPolygon      *poPolygon = (OGRPolygon *) poGeometry;

        if( poPolygon->getExteriorRing() != NULL )
        {
            if( !GeometryAppend( poPolygon->getExteriorRing() ) )
                return FALSE;
        }

        for( int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++ )
        {
            OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            if( !GeometryAppend( poRing ) )
                return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      MultiPolygon                                                    */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint
             || wkbFlatten(poGeometry->getGeometryType()) == wkbGeometryCollection
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiCurve
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiCurveZ )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeometry;
        int             iMember;

        if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon )
        {
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString )
        {
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint )
        {
        }
        else
        {
        }

        for( iMember = 0; iMember < poGC->getNumGeometries(); iMember++)
        {
            OGRGeometry *poMember = poGC->getGeometryRef( iMember );
            if( !GeometryAppend( poMember ) )
                return FALSE;
        }

    }
    else if( poGeometry->getGeometryType() == wkbCompoundCurve
             || poGeometry->getGeometryType() == wkbCompoundCurveZ )
    {
        AppendCoumpoundCurve( ( OGRCompoundCurve *) poGeometry, poDS );
    } else {
        CPLError(CE_Warning, CPLE_AppDefined, "Skipping unknown geometry type '%s'", OGRGeometryTypeToName(poGeometry->getGeometryType()));
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRILI1Layer::ICreateFeature( OGRFeature *poFeature ) {
    static long tid = -1; //system generated TID (must be unique within table)
    VSIFPrintf( poDS->GetTransferFile(), "OBJE" );

    if ( poFeatureDefn->GetFieldCount() && !EQUAL(poFeatureDefn->GetFieldDefn(0)->GetNameRef(), "TID") )
    {
        //Input is not generated from an Interlis 1 source
        if (poFeature->GetFID() != OGRNullFID)
            tid = (int)poFeature->GetFID();
        else
            ++tid;
        VSIFPrintf( poDS->GetTransferFile(), " %ld", tid );
        //Embedded geometry
        if( poFeature->GetGeometryRef() != NULL )
        {
            OGRGeometry *poGeometry = poFeature->GetGeometryRef();
            // 2D Point
            if( poGeometry->getGeometryType() == wkbPoint )
            {
                OGRPoint *poPoint = (OGRPoint *) poGeometry;

                VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getX()) );
                VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getY()) );
            }
            // 3D Point
            else if( poGeometry->getGeometryType() == wkbPoint25D )
            {
                OGRPoint *poPoint = (OGRPoint *) poGeometry;

                VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getX()) );
                VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getY()) );
                VSIFPrintf( poDS->GetTransferFile(), " %s", d2str(poPoint->getZ()) );
            }
        }
    }

    // Write all fields.
    for(int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if ( poFeature->IsFieldSet( iField ) )
        {
          const char *pszRaw = poFeature->GetFieldAsString( iField );
          if (poFeatureDefn->GetFieldDefn( iField )->GetType() == OFTString) {
              //Interlis 1 encoding is ISO 8859-1 (Latin1) -> Recode from UTF-8
              char* pszString  = CPLRecode(pszRaw, CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
              //Replace spaces
              for(size_t i=0; i<strlen(pszString); i++ ) {
                  if (pszString[i] == ' ') pszString[i] = '_';
              }
              VSIFPrintf( poDS->GetTransferFile(), " %s", pszString );
              CPLFree( pszString );
          } else {
              VSIFPrintf( poDS->GetTransferFile(), " %s", pszRaw );
          }
        }
        else
        {
          VSIFPrintf( poDS->GetTransferFile(), " @" );
        }
    }
    VSIFPrintf( poDS->GetTransferFile(), "\n" );

    // Write out Geometry
    if( poFeature->GetGeometryRef() != NULL )
    {
        GeometryAppend(poFeature->GetGeometryRef());
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI1Layer::TestCapability( CPL_UNUSED const char * pszCap ) {
    if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRILI1Layer::CreateField( OGRFieldDefn *poField, CPL_UNUSED int bApproxOK ) {
    poFeatureDefn->AddFieldDefn( poField );

    return OGRERR_NONE;
}


/************************************************************************/
/*                         Internal routines                            */
/************************************************************************/

void OGRILI1Layer::JoinGeomLayers()
{
    bGeomsJoined = TRUE;
    int bResetConfigOption = FALSE;
    if (EQUAL(CPLGetConfigOption("OGR_ARC_STEPSIZE", ""), ""))
    {
        bResetConfigOption = TRUE;
        CPLSetThreadLocalConfigOption("OGR_ARC_STEPSIZE", "0.96");
    }

    for (GeomFieldInfos::const_iterator it = oGeomFieldInfos.begin(); it != oGeomFieldInfos.end(); ++it)
    {
        OGRFeatureDefn* geomFeatureDefn = it->second.geomTable;
        if (geomFeatureDefn)
        {
            CPLDebug( "OGR_ILI", "Join geometry table %s of field '%s'", geomFeatureDefn->GetName(), it->first.c_str() );
            OGRILI1Layer* poGeomLayer = poDS->GetLayerByName(geomFeatureDefn->GetName());
            int nGeomFieldIndex = GetLayerDefn()->GetGeomFieldIndex(it->first.c_str());
            if (it->second.iliGeomType == "Surface")
            {
                JoinSurfaceLayer(poGeomLayer, nGeomFieldIndex);
            }
            else if (it->second.iliGeomType == "Area")
            {
                CPLString pointField = it->first + "__Point";
                int nPointFieldIndex = GetLayerDefn()->GetGeomFieldIndex(pointField.c_str());
                PolygonizeAreaLayer(poGeomLayer, nGeomFieldIndex, nPointFieldIndex);
            }
        }
    }

    if( bResetConfigOption )
        CPLSetThreadLocalConfigOption("OGR_ARC_STEPSIZE", NULL);
}


void OGRILI1Layer::JoinSurfaceLayer( OGRILI1Layer* poSurfaceLineLayer, int nSurfaceFieldIndex )
{
    CPLDebug( "OGR_ILI", "Joining surface layer %s with geometries", GetLayerDefn()->GetName());
    OGRwkbGeometryType geomType = GetLayerDefn()->GetGeomFieldDefn(nSurfaceFieldIndex)->GetType();
    poSurfaceLineLayer->ResetReading();
    while (OGRFeature *linefeature = poSurfaceLineLayer->GetNextFeatureRef()) {
        //OBJE entries with same _RefTID are polygon rings of same feature
        //TODO: non-numeric _RefTID/FID is not supported yet!
        GIntBig reftid = linefeature->GetFieldAsInteger64(1); //_RefTID
        OGRFeature *feature = GetFeatureRef((int)reftid);
        if (feature) {
            OGRCurvePolygon *poly;
            if (feature->GetGeomFieldRef(nSurfaceFieldIndex)) {
                CPLDebug( "OGR_ILI", "Adding ring to FID " CPL_FRMT_GIB, reftid );
                poly = (OGRCurvePolygon *)feature->GetGeomFieldRef(nSurfaceFieldIndex);
            } else {
                poly = (geomType == wkbPolygon) ? new OGRPolygon() : new OGRCurvePolygon();
                feature->SetGeomFieldDirectly(nSurfaceFieldIndex, poly);
            }
            OGRMultiCurve *lines = (OGRMultiCurve*)linefeature->GetGeomFieldRef(0);
            for( int i = 0; i < lines->getNumGeometries(); i++ ) {
                OGRCurve *line = (OGRCurve*)lines->getGeometryRef(i);
                OGRCurve *ring = (geomType == wkbPolygon) ?
                    OGRCurve::CastToLinearRing((OGRCurve*)line->clone()) :
                    (OGRCurve*)line->clone();
                poly->addRingDirectly(ring);
            }
        } else {
            CPLError(CE_Warning, CPLE_AppDefined, "Couldn't join feature FID " CPL_FRMT_GIB, reftid );
        }
    }

    ResetReading();
    poSurfaceLineLayer = 0;
}

OGRMultiPolygon* OGRILI1Layer::Polygonize( OGRGeometryCollection* poLines, bool fix_crossing_lines )
{
    OGRMultiPolygon *poPolygon = new OGRMultiPolygon();

    if (poLines->getNumGeometries() == 0) return poPolygon;

#if defined(HAVE_GEOS)
    GEOSGeom *ahInGeoms = NULL;
    int       i = 0;
    OGRGeometryCollection *poNoncrossingLines = poLines;
    GEOSGeom hResultGeom = NULL;
    OGRGeometry *poMP = NULL;

    if (fix_crossing_lines && poLines->getNumGeometries() > 0)
    {
        CPLDebug( "OGR_ILI", "Fixing crossing lines");
        //A union of the geometry collection with one line fixes invalid geometries
        poNoncrossingLines = (OGRGeometryCollection*)poLines->Union(poLines->getGeometryRef(0));
        CPLDebug( "OGR_ILI", "Fixed lines: %d", poNoncrossingLines->getNumGeometries()-poLines->getNumGeometries());
    }
    
    GEOSContextHandle_t hGEOSCtxt = OGRGeometry::createGEOSContext();

    ahInGeoms = (GEOSGeom *) CPLCalloc(sizeof(void*),poNoncrossingLines->getNumGeometries());
    for( i = 0; i < poNoncrossingLines->getNumGeometries(); i++ )
          ahInGeoms[i] = poNoncrossingLines->getGeometryRef(i)->exportToGEOS(hGEOSCtxt);

    hResultGeom = GEOSPolygonize_r( hGEOSCtxt,
                                    ahInGeoms,
                                   poNoncrossingLines->getNumGeometries() );

    for( i = 0; i < poNoncrossingLines->getNumGeometries(); i++ )
        GEOSGeom_destroy_r( hGEOSCtxt, ahInGeoms[i] );
    CPLFree( ahInGeoms );
    if (poNoncrossingLines != poLines) delete poNoncrossingLines;

    if( hResultGeom == NULL )
    {
        OGRGeometry::freeGEOSContext( hGEOSCtxt );
        return NULL;
    }

    poMP = OGRGeometryFactory::createFromGEOS( hGEOSCtxt, hResultGeom );

    GEOSGeom_destroy_r( hGEOSCtxt, hResultGeom );
    OGRGeometry::freeGEOSContext( hGEOSCtxt );

    return (OGRMultiPolygon *) poMP;

#endif

    return poPolygon;
}


void OGRILI1Layer::PolygonizeAreaLayer( OGRILI1Layer* poAreaLineLayer, int nAreaFieldIndex, int nPointFieldIndex )
{
    //add all lines from poAreaLineLayer to collection
    OGRGeometryCollection *gc = new OGRGeometryCollection();
    poAreaLineLayer->ResetReading();
    while (OGRFeature *feature = poAreaLineLayer->GetNextFeatureRef())
        gc->addGeometry(feature->GetGeometryRef());

    //polygonize lines
    CPLDebug( "OGR_ILI", "Polygonizing layer %s with %d multilines", poAreaLineLayer->GetLayerDefn()->GetName(), gc->getNumGeometries());
    poAreaLineLayer = 0;
    OGRMultiPolygon* polys = Polygonize( gc , false);
    CPLDebug( "OGR_ILI", "Resulting polygons: %d", polys->getNumGeometries());
    if (polys->getNumGeometries() != GetFeatureCount())
    {
        CPLDebug( "OGR_ILI", "Feature count of layer %s: " CPL_FRMT_GIB, GetLayerDefn()->GetName(), GetFeatureCount());
        CPLDebug( "OGR_ILI", "Polygonizing again with crossing line fix");
        delete polys;
        polys = Polygonize( gc, true ); //try again with crossing line fix
        CPLDebug( "OGR_ILI", "Resulting polygons: %d", polys->getNumGeometries());
    }
    delete gc;

    //associate polygon feature with data row according to centroid
#if defined(HAVE_GEOS)
    int i;
    OGRPolygon emptyPoly;
    GEOSGeom *ahInGeoms = NULL;

    CPLDebug( "OGR_ILI", "Associating layer %s with area polygons", GetLayerDefn()->GetName());
    ahInGeoms = (GEOSGeom *) CPLCalloc(sizeof(void*), polys->getNumGeometries());
    GEOSContextHandle_t hGEOSCtxt = OGRGeometry::createGEOSContext();
    for( i = 0; i < polys->getNumGeometries(); i++ )
    {
        ahInGeoms[i] = polys->getGeometryRef(i)->exportToGEOS(hGEOSCtxt);
        if (!GEOSisValid_r(hGEOSCtxt, ahInGeoms[i])) ahInGeoms[i] = NULL;
    }
    for ( int nFidx = 0; nFidx < nFeatures; nFidx++)
    {
        OGRFeature *feature = papoFeatures[nFidx];
        OGRGeometry* geomRef = feature->GetGeomFieldRef(nPointFieldIndex);
        if( !geomRef )
        {
            continue;
        }
        GEOSGeom point = (GEOSGeom)(geomRef->exportToGEOS(hGEOSCtxt));
        for (i = 0; i < polys->getNumGeometries(); i++ )
        {
            if (ahInGeoms[i] && GEOSWithin_r(hGEOSCtxt, point, ahInGeoms[i]))
            {
                feature->SetGeomField(nAreaFieldIndex, polys->getGeometryRef(i));
                break;
            }
        }
        if (i == polys->getNumGeometries())
        {
            CPLDebug( "OGR_ILI", "Association between area and point failed.");
            feature->SetGeometry( &emptyPoly );
        }
        GEOSGeom_destroy_r( hGEOSCtxt, point );
    }
    for( i = 0; i < polys->getNumGeometries(); i++ )
        GEOSGeom_destroy_r( hGEOSCtxt, ahInGeoms[i] );
    CPLFree( ahInGeoms );
    OGRGeometry::freeGEOSContext( hGEOSCtxt );
#endif
    poAreaLineLayer = 0;
    delete polys;
}
