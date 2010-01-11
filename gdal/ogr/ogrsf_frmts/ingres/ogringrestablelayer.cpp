/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIngresTableLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_ingres.h"
#include <geos_c.h> 

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRIngresTableLayer()                         */
/************************************************************************/

OGRIngresTableLayer::OGRIngresTableLayer( OGRIngresDataSource *poDSIn, 
                                  const char * pszTableName,
                                  int bUpdate, int nSRSIdIn )

{
    poDS = poDSIn;

    bUpdateAccess = bUpdate;

    iNextShapeId = 0;

    nSRSId = nSRSIdIn;

    poFeatureDefn = NULL;
    bLaunderColumnNames = TRUE;
}

/************************************************************************/
/*                        ~OGRIngresTableLayer()                         */
/************************************************************************/

OGRIngresTableLayer::~OGRIngresTableLayer()

{
}


/************************************************************************/
/*                        Initialize()                                  */
/*                                                                      */
/*      Make sure we only do a ResetReading once we really have a       */
/*      FieldDefn.  Otherwise, we'll segfault.  After you construct     */
/*      the IngresTableLayer, make sure to do pLayer->Initialize()       */
/************************************************************************/

OGRErr  OGRIngresTableLayer::Initialize(const char * pszTableName)
{
    poFeatureDefn = ReadTableDefinition( pszTableName );   
    if (poFeatureDefn)
    {
        ResetReading();
        return OGRERR_NONE;
    }
    else
    {
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                        ReadTableDefinition()                         */
/*                                                                      */
/*      Build a schema from the named table.  Done by querying the      */
/*      catalog.                                                        */
/************************************************************************/

OGRFeatureDefn *OGRIngresTableLayer::ReadTableDefinition( const char *pszTable )

{
    poDS->EstablishActiveLayer( NULL );

/* -------------------------------------------------------------------- */
/*      Fire off commands to get back the schema of the table.          */
/* -------------------------------------------------------------------- */
    CPLString osCommand;
    OGRIngresStatement oStatement( poDS->GetConn() );

    osCommand.Printf( "select column_name, column_datatype, column_length, "
                      "column_scale, column_ingdatatype, "
                      "column_internal_datatype "
                      "from iicolumns where table_name = '%s'", 
                      pszTable );

    if( !oStatement.ExecuteSQL( osCommand ) )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( pszTable );
    char           **papszRow;

    poDefn->Reference();
    poDefn->SetGeomType( wkbNone );

    while( (papszRow = oStatement.GetRow()) != NULL )
    {
        CPLString       osFieldName = papszRow[0];
        CPLString       osIngresType = papszRow[1];
        CPLString       osInternalType = papszRow[5];
        GInt32          nWidth, nScale;

        osIngresType.Trim();
        osFieldName.Trim();
        osInternalType.Trim();

        memcpy( &nWidth, papszRow[2], 4 );
        memcpy( &nScale, papszRow[3], 4 );

        OGRFieldDefn    oField(osFieldName, OFTString);

        if( osGeomColumn.size() == 0
            && (EQUAL(osInternalType,"POINT")
                || EQUAL(osInternalType,"IPOINT")
                || EQUAL(osInternalType,"BOX")
                || EQUAL(osInternalType,"IBOX")
                || EQUAL(osInternalType,"LSEG")
                || EQUAL(osInternalType,"ILSEG")
                || EQUAL(osInternalType,"LINE")
                || EQUAL(osInternalType,"ILINE")
                || EQUAL(osInternalType,"LONG LINE")
                || EQUAL(osInternalType,"POLYGON")
                || EQUAL(osInternalType,"IPOLYGON")
                || EQUAL(osInternalType,"LONG POLYGON")
                || EQUAL(osInternalType,"CIRCLE")
                || EQUAL(osInternalType,"LINESTRING")
				|| EQUAL(osInternalType,"MULTIPOINT")
				|| EQUAL(osInternalType,"MULTIPOLYGON")
				|| EQUAL(osInternalType,"MULTILINESTRING")
                || EQUAL(osInternalType,"ICIRCLE")) )
        {
            osGeomColumn = osFieldName;
            osIngresGeomType = osInternalType;
            
            if( strstr(osInternalType,"POINT") )
                poDefn->SetGeomType( wkbPoint );
            else if( strstr(osInternalType,"LINE")
                     || strstr(osInternalType,"SEG")
                     || strstr(osInternalType, "LINESTRING"))
                poDefn->SetGeomType( wkbLineString );
            else if( strstr(osInternalType,"MULTIPOINT"))
            	poDefn->SetGeomType(wkbMultiPoint);
            else if( strstr(osInternalType,"MULTIPOLYGON"))
            	poDefn->SetGeomType(wkbMultiPolygon);
            else if( strstr(osInternalType,"MULTILINESTRING"))
            	poDefn->SetGeomType(wkbMultiLineString);
            else
                poDefn->SetGeomType( wkbPolygon );
            continue;
        }
        else if( EQUALN(osIngresType,"byte",4) 
            || EQUALN(osIngresType,"long byte",9) )
        {
            oField.SetType( OFTBinary );
        }
        else if( EQUALN(osIngresType,"varchar",7) 
                 || EQUAL(osIngresType,"text") 
                 || EQUALN(osIngresType,"long varchar",12) )
        {
            oField.SetType( OFTString );
            oField.SetWidth( nWidth );
        }
        else if( EQUALN(osIngresType,"char",4) || EQUAL(osIngresType,"c") )
        {
            oField.SetType( OFTString );
            oField.SetWidth( nWidth );
        }
        else if( EQUAL(osIngresType,"integer") )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(osIngresType,"decimal", 7) )
        {
            if( nScale != 0 )
            {
                oField.SetType( OFTReal );
                oField.SetPrecision( nScale );
                oField.SetWidth( nWidth );
            }
            else
            {
                oField.SetType( OFTInteger );
                oField.SetWidth( nWidth );
            }
        }
        else if( EQUALN(osIngresType,"float", 5) )
        {
            oField.SetType( OFTReal );
        }
#ifdef notdef
        else if( EQUAL(osIngresType,"date") 
                 || EQUAL(osIngresType,"ansidate") 
                 || EQUAL(osIngresType,"ingresdate") )
        {
            oField.SetType( OFTDate );
        }
#endif

        // Is this an integer primary key field?
        if( osFIDColumn.size() == 0 
            && oField.GetType() == OFTInteger 
            && EQUAL(oField.GetNameRef(),"ogr_fid") )
        {
            osFIDColumn = oField.GetNameRef();
            continue;
        }

        poDefn->AddFieldDefn( &oField );
    }

    if( osFIDColumn.size() )
        CPLDebug( "Ingres", "table %s has FID column %s.",
                  pszTable, osFIDColumn.c_str() );
    else
        CPLDebug( "Ingres", 
                  "table %s has no FID column, FIDs will not be reliable!",
                  pszTable );

    // Fetch the SRID for this table now
    //nSRSId = FetchSRSId(); 

    return poDefn;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRIngresTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( !InstallFilter( poGeomIn ) )
        return;

    BuildWhere();

    ResetReading();
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRIngresTableLayer::BuildWhere()

{
    osWHERE = "";

#ifdef notdef
    if( m_poFilterGeom != NULL && pszGeomColumn )
    {
        char szEnvelope[4096];
        OGREnvelope  sEnvelope;
        szEnvelope[0] = '\0';
        
        //POLYGON((MINX MINY, MAXX MINY, MAXX MAXY, MINX MAXY, MINX MINY))
        m_poFilterGeom->getEnvelope( &sEnvelope );
        
        sprintf(szEnvelope,
                "POLYGON((%.12f %.12f, %.12f %.12f, %.12f %.12f, %.12f %.12f, %.12f %.12f))",
                sEnvelope.MinX, sEnvelope.MinY,
                sEnvelope.MaxX, sEnvelope.MinY,
                sEnvelope.MaxX, sEnvelope.MaxY,
                sEnvelope.MinX, sEnvelope.MaxY,
                sEnvelope.MinX, sEnvelope.MinY);

        osWHERE.Printf( "WHERE MBRIntersects(GeomFromText('%s'), %s)",
                        szEnvelope,
                        osGeomColumn.c_str() );

    }
#endif

    if( osQuery.size() > 0 )
    {
        if( osWHERE.size() == 0 )
            osWHERE = "WHERE " + osQuery;
        else
            osWHERE += "&& " + osQuery;
    }
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRIngresTableLayer::BuildFullQueryStatement()

{
    char *pszFields = BuildFields();

    osQueryStatement.Printf( "SELECT %s FROM %s %s", 
                             pszFields, poFeatureDefn->GetName(), 
                             osWHERE.c_str() );
    
    CPLFree( pszFields );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIngresTableLayer::ResetReading()

{
    BuildFullQueryStatement();

    OGRIngresLayer::ResetReading();
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build list of fields to fetch, performing any required          */
/*      transformations (such as on geometry).                          */
/************************************************************************/

char *OGRIngresTableLayer::BuildFields()

{
    int         i, nSize;
    char        *pszFieldList;

    nSize = 25 + osGeomColumn.size() + osFIDColumn.size();

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        nSize += strlen(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + 4;

    pszFieldList = (char *) CPLMalloc(nSize);
    pszFieldList[0] = '\0';

    if( osFIDColumn.size()
        && poFeatureDefn->GetFieldIndex( osFIDColumn ) == -1 )
        sprintf( pszFieldList, "%s", osFIDColumn.c_str() );

    if( osGeomColumn.size() )
    {
        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        if( poDS->IsNewIngres() )
        {
			sprintf( pszFieldList+strlen(pszFieldList),
					 "ASBINARY(%s) %s", osGeomColumn.c_str(), osGeomColumn.c_str() );
        }
        else
        {
			sprintf( pszFieldList+strlen(pszFieldList),
					 "%s %s", osGeomColumn.c_str(), osGeomColumn.c_str() );
        }
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        strcat( pszFieldList, pszName );
    }

    CPLAssert( (int) strlen(pszFieldList) < nSize );

    return pszFieldList;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRIngresTableLayer::SetAttributeFilter( const char *pszQuery )

{
    osQuery = "";

    if( pszQuery != NULL )
        osQuery = pszQuery;

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIngresTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return osFIDColumn.size() != 0;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess && osFIDColumn.size() != 0;

    else if( EQUAL(pszCap,OLCDeleteFeature) )
        return bUpdateAccess && osFIDColumn.size() != 0;

    else 
        return OGRIngresLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                             SetFeature()                             */
/*                                                                      */
/*      SetFeature() is implemented by dropping the old copy of the     */
/*      feature in question (if there is one) and then creating a       */
/*      new one with the provided feature id.                           */
/************************************************************************/

OGRErr OGRIngresTableLayer::SetFeature( OGRFeature *poFeature )

{
    OGRErr eErr;

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return OGRERR_FAILURE;
    }

    eErr = DeleteFeature( poFeature->GetFID() );
    if( eErr != OGRERR_NONE )
        return eErr;

    return CreateFeature( poFeature );
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRIngresTableLayer::DeleteFeature( long nFID )

{
    CPLString           osCommand;

/* -------------------------------------------------------------------- */
/*      We can only delete features if we have a well defined FID       */
/*      column to target.                                               */
/* -------------------------------------------------------------------- */
    if( osFIDColumn.size() == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DeleteFeature(%ld) failed.  Unable to delete features "
                  "in tables without\n a recognised FID column.",
                  nFID );
        return OGRERR_FAILURE;

    }

/* -------------------------------------------------------------------- */
/*      Form the statement to drop the record.                          */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "DELETE FROM %s WHERE %s = %ld",
                      poFeatureDefn->GetName(), osFIDColumn.c_str(), nFID );
                      
/* -------------------------------------------------------------------- */
/*      Execute the delete.                                             */
/* -------------------------------------------------------------------- */
    poDS->EstablishActiveLayer( NULL );
    OGRIngresStatement oStmt( poDS->GetConn() );
    
    if( !oStmt.ExecuteSQL( osCommand ) )
        return OGRERR_FAILURE;
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                      PrepareOldStyleGeometry()                       */
/*                                                                      */
/*      Prepare an ASCII representation of an old style geometry in     */
/*      a form suitable to include in an INSERT command.                */
/************************************************************************/

OGRErr OGRIngresTableLayer::PrepareOldStyleGeometry( 
    OGRGeometry *poGeom, CPLString &osRetGeomText )

{
    osRetGeomText = "";

    if( poGeom == NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Point                                                           */
/* -------------------------------------------------------------------- */
    if( EQUAL(osIngresGeomType,"POINT")
        && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        OGRPoint *poPoint = (OGRPoint *) poGeom;

        osRetGeomText.Printf( "(%.15g,%.15g)", poPoint->getX(), poPoint->getY() );
        return OGRERR_NONE;
    }

    if( EQUAL(osIngresGeomType,"IPOINT")
        && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        OGRPoint *poPoint = (OGRPoint *) poGeom;

        osRetGeomText.Printf( "(%d,%d)", 
                              (int) floor(poPoint->getX()), 
                              (int) floor(poPoint->getY()) );
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Line                                                            */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        OGRLineString *poLS = (OGRLineString *) poGeom;
        CPLString osLastPoint;
        int i;

        if( EQUAL(osIngresGeomType,"LSEG") 
            || EQUAL(osIngresGeomType,"ILSEG") 
            && poLS->getNumPoints() != 2 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to place %d vertex linestring in %s field.", 
                      poLS->getNumPoints(), 
                      osIngresGeomType.c_str() );
            return OGRERR_FAILURE;
        }
        else if( EQUAL(osIngresGeomType,"LINESTRING") 
                 && poLS->getNumPoints() > 124 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to place %d vertex linestring in %s field.", 
                      poLS->getNumPoints(), 
                      osIngresGeomType.c_str() );
            return OGRERR_FAILURE;
        }
        else if( EQUAL(osIngresGeomType,"ILINESTRING") 
                 && poLS->getNumPoints() > 248 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to place %d vertex linestring in %s field.", 
                      poLS->getNumPoints(), 
                      osIngresGeomType.c_str() );
            return OGRERR_FAILURE;
        }

        osRetGeomText = "(";
        for( i = 0; i < poLS->getNumPoints(); i++ )
        {
            CPLString osPoint;

            if( i > 0 
                && poLS->getX(i) == poLS->getX(i-1)
                && poLS->getY(i) == poLS->getY(i-1) )
            {
                CPLDebug( "INGRES", "Dropping duplicate point in linestring.");
                continue;
            }
            
            if( EQUALN(osIngresGeomType,"I",1) )
                osPoint.Printf( "(%d,%d)",
                                (int) floor(poLS->getX(i)), 
                                (int) floor(poLS->getY(i)) );
            else
                osPoint.Printf( "(%.15g,%.15g)", 
                                poLS->getX(i), poLS->getY(i) );

            if( osPoint == osLastPoint )
            {
                CPLDebug( "INGRES",
                          "Dropping duplicate point in linestring(2).");
                continue;
            }
            osLastPoint = osPoint;

            if( osRetGeomText.size() > 1 )
                osRetGeomText += "," + osPoint;
            else
                osRetGeomText += osPoint;
        }
        osRetGeomText += ")";

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
    {
        OGRPolygon *poPoly = (OGRPolygon *) poGeom;
        OGRLinearRing *poLS = poPoly->getExteriorRing();
        int i, nPoints;

        if( poLS == NULL )
            return OGRERR_FAILURE;

        if( poPoly->getNumInteriorRings() > 0 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "%d inner rings discarded from polygon being converted\n"
                      "to old ingres spatial data type '%s'.",
                      poPoly->getNumInteriorRings(),
                      osIngresGeomType.c_str() );
        }

        if( EQUAL(osIngresGeomType,"POLYGON") 
            && poLS->getNumPoints() > 124 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to place %d vertex linestring in %s field.", 
                      poLS->getNumPoints(), 
                      osIngresGeomType.c_str() );
            return OGRERR_FAILURE;
        }
        else if( EQUAL(osIngresGeomType,"IPOLYGON") 
                 && poLS->getNumPoints() > 248 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to place %d vertex linestring in %s field.", 
                      poLS->getNumPoints(), 
                      osIngresGeomType.c_str() );
            return OGRERR_FAILURE;
        }

        // INGRES geometries use *implied* closure of rings.
        nPoints = poLS->getNumPoints();
        if( poLS->getX(0) == poLS->getX(nPoints-1)
            && poLS->getY(0) == poLS->getY(nPoints-1) 
            && nPoints > 1 )
            nPoints--;

        osRetGeomText = "(";
        for( i = 0; i < nPoints; i++ )
        {
            CPLString osPoint;
            
            if( i > 0 
                && poLS->getX(i) == poLS->getX(i-1)
                && poLS->getY(i) == poLS->getY(i-1) )
            {
                CPLDebug( "INGRES", "Dropping duplicate point in linestring.");
                continue;
            }
            
            if( EQUALN(osIngresGeomType,"I",1) )
                osPoint.Printf( "(%d,%d)",
                                (int) floor(poLS->getX(i)), 
                                (int) floor(poLS->getY(i)) );
            else
                osPoint.Printf( "(%.15g,%.15g)", 
                                poLS->getX(i), poLS->getY(i) );

            if( osRetGeomText.size() > 1 )
                osRetGeomText += "," + osPoint;
            else
                osRetGeomText += osPoint;
        }
        osRetGeomText += ")";

        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                      PrepareNewStyleGeometry()                       */
/*                                                                      */
/*      Prepare an ASCII representation of a new style geometry in      */
/*      a form suitable to include in an INSERT command.                */
/*      This pretty much just uses the geometry's export to WKT function*/
/************************************************************************/

OGRErr OGRIngresTableLayer::PrepareNewStyleGeometry(
    OGRGeometry *poGeom, CPLString &osRetGeomText )

{
	OGRErr eErr = OGRERR_NONE;
    osRetGeomText = "";

    if( poGeom == NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Point                                                           */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        osRetGeomText.Printf( "POINTFROMWKB( ~V )");
    }
/* -------------------------------------------------------------------- */
/*      Linestring                                                      */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        osRetGeomText.Printf("LINEFROMWKB( ~V )");
    }
/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
    {
        osRetGeomText.Printf("POLYFROMWKB( ~V )");
    }
/* -------------------------------------------------------------------- */
/*      Multipoint                                                      */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
    {
        osRetGeomText.Printf("MPOINTFROMWKB( ~V )");
    }
/* -------------------------------------------------------------------- */
/*      Multilinestring                                                 */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString )
    {
    	osRetGeomText.Printf("MLINEFROMWKB( ~V )");
    }
/* -------------------------------------------------------------------- */
/*      Multipolygon                                                    */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon )
    {
    	osRetGeomText.Printf("MPOLYFROMWKB( ~V )");
    }
    else
    {
        eErr = OGRERR_FAILURE;
    }

    return eErr;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRIngresTableLayer::CreateFeature( OGRFeature *poFeature )

{
    CPLString           osCommand;
    int                 i, bNeedComma = FALSE;

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "INSERT INTO %s (", poFeatureDefn->GetName() );


/* -------------------------------------------------------------------- */
/*      Accumulate fields to be inserted.                               */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() != NULL && osGeomColumn.size() )
    {
        osCommand = osCommand + osGeomColumn + " ";
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && osFIDColumn.size() )
    {
        if( bNeedComma )
            osCommand += ", ";
        
        osCommand = osCommand + osFIDColumn + " ";
        bNeedComma = TRUE;
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            osCommand += ", ";

        osCommand = osCommand 
             + poFeatureDefn->GetFieldDefn(i)->GetNameRef();
    }

    osCommand += ") VALUES (";

/* -------------------------------------------------------------------- */
/*      Insert the geometry (as a place holder)                         */
/* -------------------------------------------------------------------- */
    CPLString osGeomText;

    // Set the geometry 
    bNeedComma = FALSE;
    if( poFeature->GetGeometryRef() != NULL && osGeomColumn.size() )
    {
        bNeedComma = TRUE;
        OGRErr localErr;

        if( poDS->IsNewIngres() )
        {
        	localErr = PrepareNewStyleGeometry( poFeature->GetGeometryRef(), osGeomText );
        }
        else
        {
        	localErr = PrepareOldStyleGeometry( poFeature->GetGeometryRef(), osGeomText );
        }
        if( localErr == OGRERR_NONE )
        {
            if( CSLTestBoolean( 
                     CPLGetConfigOption( "INGRES_INSERT_SUB", "NO") ) )
            {
                osCommand += " ~V";
            }
            else if( poDS->IsNewIngres() == FALSE )
            {
                osCommand += "'";
                osCommand += osGeomText;
                osCommand += "'";
                osGeomText = "";
            }
            else
            {
            	osCommand += osGeomText;
            	//osGeomText = "";
            }
        }
        else
        {
            osGeomText = "";
            osCommand += "NULL"; /* is this sort of empty geometry legal? */
        }
    }

/* -------------------------------------------------------------------- */
/*      Set the FID                                                     */
/* -------------------------------------------------------------------- */
    if( poFeature->GetFID() != OGRNullFID && osFIDColumn.size() )
    {
        if( bNeedComma )
            osCommand += ", ";
        osCommand += CPLString().Printf( "%ld ", poFeature->GetFID() );
        bNeedComma = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Copy in the attribute values.                                   */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        const char *pszStrValue = poFeature->GetFieldAsString(i);

        if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTReal
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTBinary )
        {
            int         iChar;

            //We need to quote and escape string fields. 
            osCommand += "'";

            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTIntegerList
                    && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTRealList
                    && poFeatureDefn->GetFieldDefn(i)->GetWidth() > 0
                    && iChar == poFeatureDefn->GetFieldDefn(i)->GetWidth() )
                {
                    CPLDebug( "INGRES",
                              "Truncated %s field value, it was too long.",
                              poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
                    break;
                }

                if( pszStrValue[iChar] == '\'' )
                {
                    osCommand += '\'';
                    osCommand += pszStrValue[iChar];
                }
                else
                    osCommand += pszStrValue[iChar];
            }

            osCommand += "'";
        }
        else if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTBinary )
        {
            int binaryCount = 0;
            GByte* binaryData = poFeature->GetFieldAsBinary(i, &binaryCount);
            char* pszHexValue = CPLBinaryToHex( binaryCount, binaryData );

            osCommand += "x'";
            osCommand += pszHexValue;
            osCommand += "'";

            CPLFree( pszHexValue );
        }
        else
        {
            osCommand += pszStrValue;
        }

    }

    osCommand += ")";

/* -------------------------------------------------------------------- */
/*      Execute it.                                                     */
/* -------------------------------------------------------------------- */
    poDS->EstablishActiveLayer( NULL );
    OGRIngresStatement oStmt( poDS->GetConn() );

    oStmt.bDebug = FALSE; 

    if( osGeomText.size() > 0  && poDS->IsNewIngres() == FALSE )
        oStmt.addInputParameter( IIAPI_LVCH_TYPE, osGeomText.size(),
                                 (GByte *) osGeomText.c_str() );
    if( osGeomText.size() > 0 && poDS->IsNewIngres() == TRUE )
    {
    	GByte * pabyWKB;
    	int nSize = poFeature->GetGeometryRef()->WkbSize();
    	pabyWKB = (GByte *) CPLMalloc(nSize);

    	poFeature->GetGeometryRef()->exportToWkb(wkbNDR, pabyWKB);

    	oStmt.addInputParameter( IIAPI_LBYTE_TYPE, nSize, pabyWKB );
    	CPLFree(pabyWKB);
/*
 * Test code
     	char * pszWKT;
    	poFeature->GetGeometryRef()->exportToWkt(&pszWKT);
    	oStmt.addInputParameter(IIAPI_LVCH_TYPE, strlen(pszWKT), (GByte *) pszWKT);*/
    }

    if( !oStmt.ExecuteSQL( osCommand ) )
        return OGRERR_FAILURE;
    
    return OGRERR_NONE;

}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRIngresTableLayer::CreateField( OGRFieldDefn *poFieldIn, 
                                         int bApproxOK )

{
    poDS->EstablishActiveLayer( NULL );

    CPLString           osCommand;
    OGRIngresStatement  oStatement( poDS->GetConn() );
    char                szFieldType[256];
    OGRFieldDefn        oField( poFieldIn );

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into friendly          */
/*      format?                                                         */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = poDS->LaunderName( oField.GetNameRef() );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );
    }

/* -------------------------------------------------------------------- */
/*      Work out the Ingres type.                                        */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            sprintf( szFieldType, "DECIMAL(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "INTEGER" );
    }
    else if( oField.GetType() == OFTReal )
    {
        if( oField.GetWidth() > 0 && oField.GetPrecision() > 0
            && bPreservePrecision )
            sprintf( szFieldType, "DECIMAL(%d,%d)",
                     oField.GetWidth(), oField.GetPrecision() );
        else
            strcpy( szFieldType, "FLOAT" );
    }

    else if( oField.GetType() == OFTDate )
    {
        sprintf( szFieldType, "DATE" );
    }
#ifdef notdef
    else if( oField.GetType() == OFTDateTime )
    {
        sprintf( szFieldType, "DATETIME" );
    }
#endif

    else if( oField.GetType() == OFTTime )
    {
        sprintf( szFieldType, "TIME" );
    }
#ifdef notdefa
    else if( oField.GetType() == OFTBinary )
    {
        sprintf( szFieldType, "LONGBLOB" );
    }
#endif
    else if( oField.GetType() == OFTString )
    {
        if( oField.GetWidth() == 0 ) // We need some fixed maximum.
            sprintf( szFieldType, "VARCHAR(1024)" );
        else
            sprintf( szFieldType, "VARCHAR(%d)", oField.GetWidth() );
    }
    else if( bApproxOK )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Can't create field %s with type %s on Ingres layers.  Creating as VARCHAR(1024).",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "VARCHAR(1024)" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on Ingres layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );

        return OGRERR_FAILURE;
    }

    osCommand.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                      poFeatureDefn->GetName(), oField.GetNameRef(), 
                      szFieldType );

    if( !oStatement.ExecuteSQL( osCommand ) )
        return OGRERR_FAILURE;

    poFeatureDefn->AddFieldDefn( &oField );    
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/
#ifdef notdef
OGRFeature *OGRIngresTableLayer::GetFeature( long nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRIngresLayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Discard any existing resultset.                                 */
/* -------------------------------------------------------------------- */
    ResetReading();

/* -------------------------------------------------------------------- */
/*      Prepare query command that will just fetch the one record of    */
/*      interest.                                                       */
/* -------------------------------------------------------------------- */
    char        *pszFieldList = BuildFields();
    char        *pszCommand = (char *) CPLMalloc(strlen(pszFieldList)+2000);

    sprintf( pszCommand, 
             "SELECT %s FROM %s WHERE %s = %ld", 
             pszFieldList, poFeatureDefn->GetName(), pszFIDColumn, 
             nFeatureId );
    CPLFree( pszFieldList );

/* -------------------------------------------------------------------- */
/*      Issue the command.                                              */
/* -------------------------------------------------------------------- */
    if( ingres_query( poDS->GetConn(), pszCommand ) )
    {
        poDS->ReportError( pszCommand );
        return NULL;
    }
    CPLFree( pszCommand );

    hResultSet = ingres_store_result( poDS->GetConn() );
    if( hResultSet == NULL )
    {
        poDS->ReportError( "ingres_store_result() failed on query." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the result record.                                        */
/* -------------------------------------------------------------------- */
    char **papszRow;
    unsigned long *panLengths;

    papszRow = ingres_fetch_row( hResultSet );
    if( papszRow == NULL )
        return NULL;

    panLengths = ingres_fetch_lengths( hResultSet );

/* -------------------------------------------------------------------- */
/*      Transform into a feature.                                       */
/* -------------------------------------------------------------------- */
    iNextShapeId = nFeatureId;

    OGRFeature *poFeature = RecordToFeature( papszRow, panLengths );

    iNextShapeId = 0;

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( hResultSet != NULL )
        ingres_free_result( hResultSet );
 		hResultSet = NULL;

    return poFeature;
}
#endif

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

#ifdef notdef
int OGRIngresTableLayer::GetFeatureCount( int bForce )

{
/* -------------------------------------------------------------------- */
/*      Ensure any active long result is interrupted.                   */
/* -------------------------------------------------------------------- */
    poDS->InterruptLongResult();
    
/* -------------------------------------------------------------------- */
/*      Issue the appropriate select command.                           */
/* -------------------------------------------------------------------- */
    INGRES_RES    *hResult;
    const char         *pszCommand;

    pszCommand = CPLSPrintf( "SELECT COUNT(*) FROM %s %s", 
                             poFeatureDefn->GetName(), pszWHERE );

    if( ingres_query( poDS->GetConn(), pszCommand ) )
    {
        poDS->ReportError( pszCommand );
        return FALSE;
    }

    hResult = ingres_store_result( poDS->GetConn() );
    if( hResult == NULL )
    {
        poDS->ReportError( "ingres_store_result() failed on SELECT COUNT(*)." );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Capture the result.                                             */
/* -------------------------------------------------------------------- */
    char **papszRow = ingres_fetch_row( hResult );
    int nCount = 0;

    if( papszRow != NULL && papszRow[0] != NULL )
        nCount = atoi(papszRow[0]);

    if( hResultSet != NULL )
        ingres_free_result( hResultSet );
 		hResultSet = NULL;
    
    return nCount;
}
#endif

/************************************************************************/
/*                          GetExtent()					*/
/*                                                                      */
/*      Retrieve the MBR of the Ingres table.  This should be made more  */
/*      in the future when Ingres adds support for a single MBR query    */
/*      like PostgreSQL.						*/
/************************************************************************/
#ifdef notdef
OGRErr OGRIngresTableLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
	if( GetLayerDefn()->GetGeomType() == wkbNone )
    {
        psExtent->MinX = 0.0;
        psExtent->MaxX = 0.0;
        psExtent->MinY = 0.0;
        psExtent->MaxY = 0.0;
        
        return OGRERR_FAILURE;
    }

	OGREnvelope oEnv;
	CPLString   osCommand;
	GBool       bExtentSet = FALSE;

	osCommand.Printf( "SELECT Envelope(%s) FROM %s;", pszGeomColumn, pszGeomColumnTable);

	if (ingres_query(poDS->GetConn(), osCommand) == 0)
	{
		INGRES_RES* result = ingres_use_result(poDS->GetConn());
		if ( result == NULL )
        {
            poDS->ReportError( "ingres_use_result() failed on extents query." );
            return OGRERR_FAILURE;
        }

		INGRES_ROW row; 
		unsigned long *panLengths = NULL;
		while ((row = ingres_fetch_row(result)))
		{
			if (panLengths == NULL)
			{
				panLengths = ingres_fetch_lengths( result );
				if ( panLengths == NULL )
				{
					poDS->ReportError( "ingres_fetch_lengths() failed on extents query." );
					return OGRERR_FAILURE;
				}
			}

			OGRGeometry *poGeometry = NULL;
			// Geometry columns will have the first 4 bytes contain the SRID.
			OGRGeometryFactory::createFromWkb(((GByte *)row[0]) + 4, 
											  NULL,
											  &poGeometry,
											  panLengths[0] - 4 );

			if ( poGeometry != NULL )
			{
				if (poGeometry && !bExtentSet)
				{
					poGeometry->getEnvelope(psExtent);
					bExtentSet = TRUE;
				}
				else if (poGeometry)
				{
					poGeometry->getEnvelope(&oEnv);
					if (oEnv.MinX < psExtent->MinX) 
						psExtent->MinX = oEnv.MinX;
					if (oEnv.MinY < psExtent->MinY) 
						psExtent->MinY = oEnv.MinY;
					if (oEnv.MaxX > psExtent->MaxX) 
						psExtent->MaxX = oEnv.MaxX;
					if (oEnv.MaxY > psExtent->MaxY) 
						psExtent->MaxY = oEnv.MaxY;
				}
				delete poGeometry;
			}
		}

		ingres_free_result(result);      
	}

	return (bExtentSet ? OGRERR_NONE : OGRERR_FAILURE);
}
#endif

