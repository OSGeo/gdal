/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMDBLayer class
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_mdb.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogrpgeogeometry.h"
#include "ogrgeomediageometry.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRMDBLayer()                            */
/************************************************************************/

OGRMDBLayer::OGRMDBLayer(OGRMDBDataSource* poDS, OGRMDBTable* poMDBTable)

{
    this->poDS = poDS;
    this->poMDBTable = poMDBTable;

    eGeometryType = MDB_GEOM_NONE;

    iGeomColumn = -1;
    pszGeomColumn = NULL;
    pszFIDColumn = NULL;

    panFieldOrdinals = NULL;

    poFeatureDefn = NULL;

    iNextShapeId = 0;

    poSRS = NULL;
    nSRSId = -2; // we haven't even queried the database for it yet.

    bHasExtent = FALSE;
}

/************************************************************************/
/*                            ~OGRMDBLayer()                             */
/************************************************************************/

OGRMDBLayer::~OGRMDBLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "MDB", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( poFeatureDefn != NULL )
    {
        poFeatureDefn->Release();
        poFeatureDefn = NULL;
    }

    CPLFree( pszGeomColumn );
    CPLFree( pszFIDColumn );

    CPLFree( panFieldOrdinals );

    if( poSRS != NULL )
    {
        poSRS->Release();
        poSRS = NULL;
    }

    delete poMDBTable;
}

/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      Build feature definition from a set of column definitions       */
/*      set on a statement.  Sift out geometry and FID fields.          */
/************************************************************************/

CPLErr OGRMDBLayer::BuildFeatureDefn()

{
    poFeatureDefn = new OGRFeatureDefn( poMDBTable->GetName() );
    SetDescription( poFeatureDefn->GetName() );

    poFeatureDefn->Reference();


    int nRawColumns = poMDBTable->GetColumnCount();
    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * nRawColumns );

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        const char* pszColName = poMDBTable->GetColumnName(iCol);
        OGRFieldDefn    oField(pszColName, OFTString );

        if( pszGeomColumn != NULL
            && EQUAL(pszColName,pszGeomColumn) )
            continue;

        if( eGeometryType == MDB_GEOM_PGEO
            && pszFIDColumn == NULL
            && EQUAL(pszColName,"OBJECTID") )
        {
            pszFIDColumn = CPLStrdup(pszColName);
        }

        if( eGeometryType == MDB_GEOM_PGEO
            && pszGeomColumn == NULL 
            && EQUAL(pszColName,"Shape") )
        {
            pszGeomColumn = CPLStrdup(pszColName);
            continue;
        }
        
        switch( poMDBTable->GetColumnType(iCol) )
        {
          case MDB_Boolean:
            oField.SetType( OFTInteger );
            oField.SetWidth(1);
            break;

          case MDB_Byte:
          case MDB_Short:
          case MDB_Int:
            oField.SetType( OFTInteger );
            break;

          case MDB_Binary:
          case MDB_OLE:
            oField.SetType( OFTBinary );
            break;

          case MDB_Float:
          case MDB_Double:
            oField.SetType( OFTReal );
            break;

          case MDB_Text:
            oField.SetWidth(poMDBTable->GetColumnLength(iCol));
            break;

          default:
            /* leave it as OFTString */;
        }

        poFeatureDefn->AddFieldDefn( &oField );
        panFieldOrdinals[poFeatureDefn->GetFieldCount() - 1] = iCol+1;
    }

    if( poFeatureDefn->GetGeomFieldCount() > 0 )
    {
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        if( pszGeomColumn != NULL )
            poFeatureDefn->GetGeomFieldDefn(0)->SetName(pszGeomColumn);
    }

    return CE_None;
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMDBLayer::ResetReading()

{
    iNextShapeId = 0;
    poMDBTable->ResetReading();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRMDBLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom != NULL || m_poAttrQuery != NULL)
        return OGRLayer::GetFeatureCount(bForce);
    return poMDBTable->GetRowCount();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRMDBLayer::GetNextFeature()

{
    for( ; TRUE; )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRMDBLayer::GetNextRawFeature()

{
    OGRErr err = OGRERR_NONE;

    if( !poMDBTable->GetNextRow() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    int         iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    if( pszFIDColumn != NULL && poMDBTable->GetColumnIndex(pszFIDColumn) > -1 )
        poFeature->SetFID( 
            poMDBTable->GetColumnAsInt(poMDBTable->GetColumnIndex(pszFIDColumn)) );
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;
    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Set the fields.                                                 */
/* -------------------------------------------------------------------- */
    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        int iSrcField = panFieldOrdinals[iField]-1;
        char *pszValue = poMDBTable->GetColumnAsString( iSrcField );
        OGRFieldType eType = poFeature->GetFieldDefnRef(iField)->GetType();

        if( pszValue == NULL )
            /* no value */;
        else if( eType == OFTBinary )
        {
            int nBytes = 0;
            GByte* pData = poMDBTable->GetColumnAsBinary( iSrcField, &nBytes);
            poFeature->SetField( iField, 
                                 nBytes,
                                 pData );
            CPLFree(pData);
        }
        else if ( eType == OFTInteger && EQUAL(pszValue, "true"))
        {
           poFeature->SetField( iField, 1 );
        }
        else
        {
           poFeature->SetField( iField, pszValue );
        }

        CPLFree(pszValue);
    }

    if( !(m_poAttrQuery == NULL
          || m_poAttrQuery->Evaluate( poFeature )) )
        return poFeature;

/* -------------------------------------------------------------------- */
/*      Try to extract a geometry.                                      */
/* -------------------------------------------------------------------- */
    if( eGeometryType == MDB_GEOM_PGEO && iGeomColumn >= 0)
    {
        int nBytes = 0;
        GByte* pData = poMDBTable->GetColumnAsBinary( iGeomColumn, &nBytes);
        OGRGeometry *poGeom = NULL;

        if( pData != NULL )
        {
            err = OGRCreateFromShapeBin( pData, &poGeom, nBytes );
            if( OGRERR_NONE != err )
            {
                CPLDebug( "MDB",
                          "Translation shape binary to OGR geometry failed (FID=%ld)",
                           (long)poFeature->GetFID() );
            }
        }

        CPLFree(pData);

        if( poGeom != NULL && OGRERR_NONE == err )
        {
            poGeom->assignSpatialReference( poSRS );
            poFeature->SetGeometryDirectly( poGeom );
        }
    }
    else if( eGeometryType == MDB_GEOM_GEOMEDIA && iGeomColumn >= 0)
    {
        int nBytes = 0;
        GByte* pData = poMDBTable->GetColumnAsBinary( iGeomColumn, &nBytes);
        OGRGeometry *poGeom = NULL;

        if( pData != NULL )
        {
            err = OGRCreateFromGeomedia( pData, &poGeom, nBytes );
            if( OGRERR_NONE != err )
            {
                CPLDebug( "MDB",
                          "Translation geomedia binary to OGR geometry failed (FID=%ld)",
                           (long)poFeature->GetFID() );
            }
        }

        CPLFree(pData);

        if( poGeom != NULL && OGRERR_NONE == err )
        {
            poGeom->assignSpatialReference( poSRS );
            poFeature->SetGeometryDirectly( poGeom );
        }
    }

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRMDBLayer::GetFeature( long nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMDBLayer::TestCapability( const char * pszCap )

{
    /*if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else*/
    if( EQUAL(pszCap,OLCFastFeatureCount) ||
        EQUAL(pszCap,OLCFastGetExtent) )
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    else
        return FALSE;
}

/************************************************************************/
/*                           LookupSRID()                               */
/************************************************************************/

void OGRMDBLayer::LookupSRID( int nSRID )

{
/* -------------------------------------------------------------------- */
/*      Fetch the corresponding WKT from the SpatialRef table.          */
/* -------------------------------------------------------------------- */
    OGRMDBDatabase* poDB = poMDBTable->GetDB();
    OGRMDBTable* poSRSTable = poDB->GetTable("GDB_SpatialRefs");
    if (poSRSTable == NULL)
        return;

    int iSRTEXT = poSRSTable->GetColumnIndex("SRTEXT", TRUE);
    int iSRID = poSRSTable->GetColumnIndex("SRID", TRUE);

    if (iSRTEXT < 0 || iSRID < 0)
    {
        delete poSRSTable;
        return;
    }

    char* pszSRText = NULL;
    while(poSRSTable->GetNextRow())
    {
        int nTableSRID = poSRSTable->GetColumnAsInt(iSRID);
        if (nTableSRID == nSRID)
        {
            pszSRText = poSRSTable->GetColumnAsString(iSRTEXT);
            break;
        }
    }

    if (pszSRText == NULL)
    {
        delete poSRSTable;
        return;
    }

/* -------------------------------------------------------------------- */
/*      Check that it isn't just a GUID.  We don't know how to          */
/*      translate those.                                                */
/* -------------------------------------------------------------------- */

    if( pszSRText[0] == '{' )
    {
        CPLDebug( "MDB", "Ignoreing GUID SRTEXT: %s", pszSRText );
        delete poSRSTable;
        CPLFree(pszSRText);
        return;
    }

/* -------------------------------------------------------------------- */
/*      Turn it into an OGRSpatialReference.                            */
/* -------------------------------------------------------------------- */
    poSRS = new OGRSpatialReference();

    char* pszSRTextPtr = pszSRText;
    if( poSRS->importFromWkt( &pszSRTextPtr ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "importFromWKT() failed on SRS '%s'.",
                  pszSRText);
        delete poSRS;
        poSRS = NULL;
    }
    else if( poSRS->morphFromESRI() != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "morphFromESRI() failed on SRS." );
        delete poSRS;
        poSRS = NULL;
    }
    else
        nSRSId = nSRID;

    delete poSRSTable;
    CPLFree(pszSRText);
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRMDBLayer::GetFIDColumn()

{
    if( pszFIDColumn != NULL )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRMDBLayer::Initialize( CPL_UNUSED const char *pszTableName,
                                const char *pszGeomCol,
                                int nShapeType,
                                double dfExtentLeft,
                                double dfExtentRight,
                                double dfExtentBottom,
                                double dfExtentTop,
                                int nSRID,
                                int bHasZ )


{
    CPLFree( pszGeomColumn );

    if( pszGeomCol == NULL )
        pszGeomColumn = NULL;
    else
    {
        pszGeomColumn = CPLStrdup( pszGeomCol );
        iGeomColumn = poMDBTable->GetColumnIndex( pszGeomColumn );
    }

    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

    bHasExtent = TRUE;
    sExtent.MinX = dfExtentLeft;
    sExtent.MaxX = dfExtentRight;
    sExtent.MinY = dfExtentBottom;
    sExtent.MaxY = dfExtentTop;

    LookupSRID( nSRID );

    eGeometryType = MDB_GEOM_PGEO;

    CPLErr eErr;
    eErr = BuildFeatureDefn();
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Setup geometry type.                                            */
/* -------------------------------------------------------------------- */
    OGRwkbGeometryType  eOGRType;

    switch( nShapeType )
    {
        case ESRI_LAYERGEOMTYPE_NULL:
            eOGRType = wkbNone;
            break;

        case ESRI_LAYERGEOMTYPE_POINT:
            eOGRType = wkbPoint;
            break;

        case ESRI_LAYERGEOMTYPE_MULTIPOINT:
            eOGRType = wkbMultiPoint;
            break;

        case ESRI_LAYERGEOMTYPE_POLYLINE:
            eOGRType = wkbLineString;
            break;

        case ESRI_LAYERGEOMTYPE_POLYGON:
        case ESRI_LAYERGEOMTYPE_MULTIPATCH:
            eOGRType = wkbPolygon;
            break;

        default:
            CPLDebug("MDB", "Unexpected value for shape type : %d", nShapeType);
            eOGRType = wkbUnknown;
            break;
    }

    if( eOGRType != wkbUnknown && eOGRType != wkbNone && bHasZ )
        eOGRType = wkbSetZ(eOGRType);

    poFeatureDefn->SetGeomType(eOGRType);

    return CE_None;
}


/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRMDBLayer::Initialize( CPL_UNUSED const char *pszTableName,
                                const char *pszGeomCol,
                                OGRSpatialReference* poSRS )


{
    CPLFree( pszGeomColumn );

    if( pszGeomCol == NULL )
        pszGeomColumn = NULL;
    else
    {
        pszGeomColumn = CPLStrdup( pszGeomCol );
        iGeomColumn = poMDBTable->GetColumnIndex( pszGeomColumn );
    }

    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

    eGeometryType = MDB_GEOM_GEOMEDIA;

    this->poSRS = poSRS;

    CPLErr eErr;
    eErr = BuildFeatureDefn();
    if( eErr != CE_None )
        return eErr;

    return CE_None;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRMDBLayer::GetExtent( OGREnvelope *psExtent, int bForce )

{
    if (bHasExtent)
    {
        *psExtent = sExtent;
        return OGRERR_NONE;
    }
    else
    {
        return OGRLayer::GetExtent(psExtent, bForce);
    }
}
