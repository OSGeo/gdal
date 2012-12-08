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
    OGRIngresStatement oStatement( poDS->GetTransaction() );

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
                || EQUAL(osInternalType,"GEOMETRYCOLLECTION")
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
            // Oddly this is the standin for a generic geometry type.
            else if( strstr(osInternalType,"GEOMETRYCOLLECTION"))
            	poDefn->SetGeomType(wkbUnknown);
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

        /* -------------------------------------------------------------------- */
        /*              Date/Time Type Support									*/
        /* -------------------------------------------------------------------- */
        else if( EQUAL(osIngresType,"date") 
                 || EQUAL(osIngresType,"ansidate") )
        {
            oField.SetType( OFTDate );
        }
        else if( EQUAL(osIngresType,"time with local time zone") 
            || EQUAL(osIngresType,"time with time zone") 
            || EQUAL(osIngresType,"time without time zone"))
        {
            oField.SetType( OFTTime );
        }
        else if (EQUAL(osIngresType,"ingresdate") 
            || EQUAL(osIngresType,"datetime")
            || EQUAL(osIngresType, "timestamp with local time zone")
            || EQUAL(osIngresType, "timestamp with time zone")
            || EQUAL(osIngresType, "timestamp without time zone"))
        {
            oField.SetType( OFTDateTime );
        }
        

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

    //We must close the current statement before calling this or else
    //The query within FetchSRSId will fail
    oStatement.Close();

    // Fetch the SRID for this table now
    // But only if it's the new Ingres Geospatial
    if(poDS->IsNewIngres() == TRUE)
        nSRSId = FetchSRSId(poDefn);

    return poDefn;
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRIngresTableLayer::BuildFullQueryStatement()

{
    char *pszFields = BuildFields();

    if (osWHERE.size())
    {    
        osQueryStatement.Printf( "SELECT %s FROM %s WHERE %s", 
            pszFields, poFeatureDefn->GetName(), 
            osWHERE.c_str() );
    }
    else
    {
        osQueryStatement.Printf( "SELECT %s FROM %s ", 
            pszFields, poFeatureDefn->GetName());
    }

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
    OGRIngresStatement oStmt( poDS->GetTransaction() );
    
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

        if( (EQUAL(osIngresGeomType,"LSEG") 
             || EQUAL(osIngresGeomType,"ILSEG")) 
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
        osRetGeomText.Printf( "POINTFROMWKB( ~V , %d )", nSRSId );
    }
/* -------------------------------------------------------------------- */
/*      Linestring                                                      */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        osRetGeomText.Printf("LINEFROMWKB( ~V , %d)", nSRSId);
    }
/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
    {
        osRetGeomText.Printf("POLYFROMWKB( ~V , %d)", nSRSId);
    }
/* -------------------------------------------------------------------- */
/*      Multipoint                                                      */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
    {
        osRetGeomText.Printf("MPOINTFROMWKB( ~V , %d)", nSRSId);
    }
/* -------------------------------------------------------------------- */
/*      Multilinestring                                                 */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString )
    {
    	osRetGeomText.Printf("MLINEFROMWKB( ~V , %d)", nSRSId);
    }
/* -------------------------------------------------------------------- */
/*      Multipolygon                                                    */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon )
    {
    	osRetGeomText.Printf("MPOLYFROMWKB( ~V , %d)", nSRSId);
    }
/* -------------------------------------------------------------------- */
/*      Geometry collection.                                            */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection )
    {
    	osRetGeomText.Printf("GEOMCOLLFROMWKB( ~V , %d)", nSRSId);
    }
/* -------------------------------------------------------------------- */
/*      Fallback generic geometry handling.                             */
/* -------------------------------------------------------------------- */
    else 
    {
        CPLDebug( 
            "INGRES",
            "Unexpected geometry type (%s), attempting to treat generically.",
            poGeom->getGeometryName() );

    	osRetGeomText.Printf("GEOMETRYFROMWKB( ~V , %d)", nSRSId);
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

            // Handle default value for date/time type
            if (EQUAL(pszStrValue, "0000/00/00")
                || EQUAL(pszStrValue, "00:00:00")
                || EQUAL(pszStrValue, "0000/00/00 00:00:00"))
            {
                // Treat as a null value?
                osCommand += "NULL";
                continue;
            }
            
            //We need to quote and escape string fields. 
            osCommand += "'";

            // we handle date/time first
            OGRFieldType fieldType = poFeatureDefn->GetFieldDefn(i)->GetType();
            if (fieldType == OFTDate)
            {
                // what we got is 'yyyy/mm/dd' 
                // which could not be parsed by Ingres
                // we need to convert it to string like 'yyyy.mm.dd'
                for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
                {
                    if (pszStrValue[iChar] == '/')
                    {
                        osCommand +='.';
                    }
                    else
                    {
                        osCommand += pszStrValue[iChar];
                    }
                }
            }
            else if (fieldType == OFTTime)
            {
                for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
                {
                    osCommand += pszStrValue[iChar];
                }
            }
            else if (fieldType == OFTDateTime)
            {
                // what we got is 'yyyy/mm/dd hh:mm:ss'
                // need to convert to 'yyyy-mm-dd hh:mm:ss'
                // The time zone info will be ignored
                osCommand += " TIMESTAMP ' ";
                for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
                {
                    if (pszStrValue[iChar] == '/')
                    {
                        osCommand += '-';
                    }
                    else
                    {
                        osCommand += pszStrValue[iChar];
                    }
                }
                osCommand += " ' ";
            }
            else
            {
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
    OGRIngresStatement oStmt( poDS->GetTransaction() );

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
    OGRIngresStatement  oStatement( poDS->GetTransaction() );
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

    else if( oField.GetType() == OFTDateTime )
    {
        sprintf( szFieldType, "TIMESTAMP WITH LOCAL TIME ZONE" );
    }

    else if( oField.GetType() == OFTTime )
    {
        sprintf( szFieldType, "TIME WITH LOCAL TIME ZONE" );
    }

    else if( oField.GetType() == OFTBinary )
    {
        sprintf( szFieldType, "BLOB" );
    }

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
OGRFeature *OGRIngresTableLayer::GetFeature( long nFeatureId )

{
    if( osFIDColumn.size() == 0 )
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
    CPLString   osSqlCmd;
    OGRIngresStatement oStmt(poDS->GetTransaction());

    CPLAssert(pszFieldList);
    osSqlCmd.Printf("SELECT %s FROM %s WHERE %s = %ld",
        pszFieldList,
        poFeatureDefn->GetName(),
        osFIDColumn.c_str(),
        nFeatureId);
    CPLFree( pszFieldList );

/* -------------------------------------------------------------------- */
/*      Issue the command.                                              */
/* -------------------------------------------------------------------- */
    CPLDebug("Ingres", osSqlCmd.c_str());
    if (!oStmt.ExecuteSQL(osSqlCmd.c_str()))
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the result record.                                        */
/* -------------------------------------------------------------------- */
    char **papszRow = oStmt.GetRow();
    if (!papszRow)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Can't get the result row which may be caused by query "
            "error or result row of id %d is not exist."
            , nFeatureId);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Transform into a feature.                                       */
/* -------------------------------------------------------------------- */
    poResultSet = &oStmt;
    OGRFeature *poFeature = RecordToFeature( papszRow);
    poResultSet = NULL;

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/
int OGRIngresTableLayer::GetFeatureCount( int bForce )

{
    CPLString osSqlCmd;
    OGRIngresStatement oStmt( poDS->GetTransaction() );
    int nCount = 0 ;

    poDS->EstablishActiveLayer( this );
    /* -------------------------------------------------------------------- */
    /* 	If attribute filter or spatial filter is set, a query must be used  */
    /* -------------------------------------------------------------------- */
    if (osWHERE.size())
    {
        osSqlCmd.Printf("SELECT INT4(COUNT(*)) FROM %s WHERE %s",
            poFeatureDefn->GetName(), osWHERE.c_str());

        /* Bind Geometry */
        if (m_poFilterGeom)
        {        
            BindQueryGeometry(&oStmt);
        }
    }
    else
    {
        /* -------------------------------------------------------------------- */
        /* 	Force means use select count(*) method 								*/
        /* -------------------------------------------------------------------- */
        if (bForce)
        {
            osSqlCmd.Printf("SELECT INT4(COUNT(*)) FROM %s",
                poFeatureDefn->GetName());
        }
        else
        {
            osSqlCmd.Printf("SELECT INT4(num_rows) FROM iitables "
                "WHERE TABLE_NAME=LOWERCASE('%s') AND "
                "TABLE_OWNER=(SELECT DBMSINFO('username'))",
		        poFeatureDefn->GetName());
        }        
    }

    CPLDebug("Ingres", osSqlCmd.c_str());
    /* -------------------------------------------------------------------- */
    /*  Execute SQL and get the result  									*/
    /* -------------------------------------------------------------------- */
    if (!oStmt.ExecuteSQL(osSqlCmd.c_str()))
    {
        /* Failed */
        return 0;
    }

    char **papszRow = oStmt.GetRow();
    CPLAssert(papszRow);

    memcpy(&nCount ,papszRow[0], 4);
    
    return nCount;
}

/************************************************************************/
/*                          GetExtent()					*/
/*                                                                      */
/*      Retrieve the MBR of the Ingres table.  This should be made more  */
/*      in the future when Ingres adds support for a single MBR query    */
/*      like PostgreSQL.						*/
/************************************************************************/

OGRErr OGRIngresTableLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
	if( GetLayerDefn()->GetGeomType() == wkbNone )
    {
        psExtent->MinX = 0.0;
        psExtent->MaxX = 0.0;
        psExtent->MinY = 0.0;
        psExtent->MaxY = 0.0;
        
        CPLError(CE_Failure, CPLE_AppDefined, 
            "%s is not a geometry layer", GetLayerDefn()->GetName());
        return OGRERR_FAILURE;
    }

	OGREnvelope oEnv;
	CPLString   osCommand;
	GBool       bExtentSet = FALSE;
    OGRIngresStatement oStmt(poDS->GetTransaction()); 

	osCommand.Printf( "SELECT asbinary(extent(%s)) FROM %s", 
        osGeomColumn.c_str(), 
        GetLayerDefn()->GetName());
    CPLDebug("Ingres", osCommand.c_str());

    if (osWHERE.size())
    {
        osCommand += " WHERE ";
        osCommand += osWHERE;

        /* Bind Geometry */
        if (m_poFilterGeom)
        {        
            BindQueryGeometry(&oStmt);
        }
    }

    /* -------------------------------------------------------------------- */
    /*  Execute SQL and get the result  									*/
    /* -------------------------------------------------------------------- */
    if (!oStmt.ExecuteSQL(osCommand.c_str()))
    {
        /* Failed */
        return OGRERR_FAILURE;
    }

    char **papszRow = oStmt.GetRow();
    if (papszRow == NULL)
    {
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /* 	What kind of result it is?											*/
    /* -------------------------------------------------------------------- */
    OGRGeometry *poGeometry = NULL;
    OGRGeometryFactory::createFromWkb( (GByte*)papszRow[0], 
        NULL,
        &poGeometry,
        -1);

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

	return (bExtentSet ? OGRERR_NONE : OGRERR_FAILURE);
}

