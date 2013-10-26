/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReader::HugeFileResolver() method.
 * Author:   Alessandro Furieri, a.furitier@lqt.it
 *
 ******************************************************************************
 * Copyright (c) 2011, Alessandro Furieri
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
 *
 ******************************************************************************
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * This module implents GML_SKIP_RESOLVE_ELEMS HUGE
 * Developed for Faunalia ( http://www.faunalia.it) with funding from 
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ****************************************************************************/

#include "gmlreader.h"
#include "cpl_error.h"

#include "gmlreaderp.h"
#include "gmlutils.h"
#include "cpl_conv.h"
#include "ogr_p.h"
#include "cpl_string.h"
#include "cpl_http.h"

#include <stack>

CPL_CVSID("$Id$");

/****************************************************/
/*      SQLite is absolutely required in order to   */
/*      support the HUGE xlink:href resolver        */
/****************************************************/

#ifdef HAVE_SQLITE
#include <sqlite3.h>
#endif

/* sqlite3_clear_bindings() isn't available in old versions of */
/* sqlite3 */
#if defined(HAVE_SQLITE) && SQLITE_VERSION_NUMBER >= 3006000

/* an internal helper struct supporting GML tags <Edge> */
struct huge_tag
{
    CPLString           *gmlTagValue;
    CPLString           *gmlId;
    CPLString           *gmlNodeFrom;
    CPLString           *gmlNodeTo;
    int	                bIsNodeFromHref;
    int                 bIsNodeToHref;
    int                 bHasCoords;
    int                 bHasZ;
    double              xNodeFrom;
    double              yNodeFrom;
    double              zNodeFrom;
    double              xNodeTo;
    double              yNodeTo;
    double              zNodeTo;
    struct huge_tag     *pNext;
};

/* an internal helper struct supporting GML tags xlink:href */
struct huge_href
{
    CPLString           *gmlId;
    CPLString           *gmlText;
    const CPLXMLNode    *psParent;
    const CPLXMLNode    *psNode;
    int                 bIsDirectedEdge;
    char                cOrientation;
    struct huge_href    *pNext;
};

/* an internal helper struct supporying GML rewriting */
struct huge_child
{
    CPLXMLNode          *psChild;
    struct huge_href    *pItem;
    struct huge_child   *pNext;
};

/* an internal helper struct supporting GML rewriting */
struct huge_parent
{
    CPLXMLNode          *psParent;
    struct huge_child   *pFirst;
    struct huge_child   *pLast;
    struct huge_parent  *pNext;
};

/*
/ an internal helper struct supporting GML 
/ resolver for Huge Files (based on SQLite)
*/
struct huge_helper
{
    sqlite3             *hDB;
    sqlite3_stmt        *hNodes;
    sqlite3_stmt        *hEdges;
    CPLString           *nodeSrs;
    struct huge_tag     *pFirst;
    struct huge_tag     *pLast;
    struct huge_href    *pFirstHref;
    struct huge_href    *pLastHref;
    struct huge_parent  *pFirstParent;
    struct huge_parent  *pLastParent;
};

static int gmlHugeFileSQLiteInit( struct huge_helper *helper )
{
/* attempting to create SQLite tables */
    const char          *osCommand;
    char                *pszErrMsg = NULL;
    int                 rc;
    sqlite3             *hDB = helper->hDB;
    sqlite3_stmt        *hStmt;

    /* DB table: NODES */
    osCommand = "CREATE TABLE nodes ("
                "     gml_id VARCHAR PRIMARY KEY, "
                "     x DOUBLE, "
                "     y DOUBLE, "
                "     z DOUBLE)";
    rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create table nodes: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    /* DB table: GML_EDGES */
    osCommand = "CREATE TABLE gml_edges ("
                "     gml_id VARCHAR PRIMARY KEY, "
                "     gml_string BLOB, "
                "     gml_resolved BLOB, "
				"     node_from_id TEXT, "
                "     node_from_x DOUBLE, "
                "     node_from_y DOUBLE, "
                "     node_from_z DOUBLE, "
				"     node_to_id TEXT, "
                "     node_to_x DOUBLE, "
                "     node_to_y DOUBLE, "
                "     node_to_z DOUBLE)";
    rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create table gml_edges: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    /* DB table: NODES / Insert cursor */
    osCommand = "INSERT OR IGNORE INTO nodes (gml_id, x, y, z) "
                "VALUES (?, ?, ?, ?)";
    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create INSERT stmt for: nodes" );
        return FALSE;
    }
    helper->hNodes = hStmt;

    /* DB table: GML_EDGES / Insert cursor */
    osCommand = "INSERT INTO gml_edges "
                "(gml_id, gml_string, gml_resolved, "
                "node_from_id, node_from_x, node_from_y, "
                "node_from_z, node_to_id, node_to_x, "
                "node_to_y, node_to_z) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create INSERT stmt for: gml_edges" );
        return FALSE;
    }
    helper->hEdges = hStmt;

    /* starting a TRANSACTION */
    rc = sqlite3_exec( hDB, "BEGIN", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to perform BEGIN TRANSACTION: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }	

    return TRUE;
}

static int gmlHugeResolveEdgeNodes( const CPLXMLNode *psNode,
                                    const char *pszFromId,
                                    const char *pszToId )
{
/* resolves an Edge definition */
    CPLXMLNode      *psDirNode_1 = NULL;
    CPLXMLNode      *psDirNode_2 = NULL;
    CPLXMLNode      *psOldNode_1 = NULL;
    CPLXMLNode      *psOldNode_2 = NULL;
    CPLXMLNode      *psNewNode_1 = NULL;
    CPLXMLNode      *psNewNode_2 = NULL;
    int             iToBeReplaced = 0;
    int             iReplaced = 0;
    if( psNode->eType == CXT_Element && EQUAL( psNode->pszValue, "Edge" ) )
        ;
    else
        return FALSE;

    CPLXMLNode *psChild = psNode->psChild;
    while( psChild != NULL )
    {
        if( psChild->eType == CXT_Element &&
            EQUAL( psChild->pszValue, "directedNode" ) )
        {
            char cOrientation = '+';
            CPLXMLNode *psOldNode = NULL;
            CPLXMLNode *psAttr = psChild->psChild;
            while( psAttr != NULL )
            {
                if( psAttr->eType == CXT_Attribute &&
                    EQUAL( psAttr->pszValue, "xlink:href" ) )
                    psOldNode = psAttr;
                if( psAttr->eType == CXT_Attribute &&
                    EQUAL( psAttr->pszValue, "orientation" ) )
                {
                    const CPLXMLNode *psOrientation = psAttr->psChild;
                    if( psOrientation != NULL )
                    {
                        if( psOrientation->eType == CXT_Text )
                            cOrientation = *(psOrientation->pszValue);
                    }
                }
                psAttr = psAttr->psNext;
            }
            if( psOldNode != NULL )
            {
                CPLXMLNode *psNewNode = CPLCreateXMLNode(NULL, CXT_Element, "Node");
                CPLXMLNode *psGMLIdNode = CPLCreateXMLNode(psNewNode, CXT_Attribute, "gml:id");
                if( cOrientation == '-' )
                    CPLCreateXMLNode(psGMLIdNode, CXT_Text, pszFromId);
                else
                    CPLCreateXMLNode(psGMLIdNode, CXT_Text, pszToId);
                if( iToBeReplaced == 0 )
                {
                    psDirNode_1 = psChild;
                    psOldNode_1 = psOldNode;
                    psNewNode_1 = psNewNode;
                }
                else
                {
                    psDirNode_2 = psChild;
                    psOldNode_2 = psOldNode;
                    psNewNode_2 = psNewNode;
                }
                iToBeReplaced++;
            }
        }
        psChild = psChild->psNext;
    }

    /* rewriting the Edge GML definition */
    if( psDirNode_1 != NULL)
    {
        if( psOldNode_1 != NULL )
        {
            CPLRemoveXMLChild( psDirNode_1, psOldNode_1 );
            CPLDestroyXMLNode( psOldNode_1 );
            if( psNewNode_1 != NULL )
            {
                CPLAddXMLChild( psDirNode_1, psNewNode_1 );
                iReplaced++;
            }
        }
    }
    if( psDirNode_2 != NULL)
    {
        if( psOldNode_2 != NULL )
        {
            CPLRemoveXMLChild( psDirNode_2, psOldNode_2 );
            CPLDestroyXMLNode( psOldNode_2 );
            if( psNewNode_2 != NULL )
            {
                CPLAddXMLChild( psDirNode_2, psNewNode_2 );
                iReplaced++;
            }
        }
    }
    if( iToBeReplaced != iReplaced )
        return FALSE;
return TRUE;
}

static int gmlHugeFileResolveEdges( struct huge_helper *helper )
{
/* identifying any not yet resolved <Edge> GML string */
    const char          *osCommand;
    char                *pszErrMsg = NULL;
    int rc;
    sqlite3             *hDB = helper->hDB;
    sqlite3_stmt        *hQueryStmt;
    sqlite3_stmt        *hUpdateStmt;
    int                 iCount = 0;
    int                 bError = FALSE;

    /* query cursor */
    osCommand = "SELECT e.gml_id, e.gml_string, e.node_from_id, "
                "e.node_from_x, e.node_from_y, e.node_from_z, "
                "n1.gml_id, n1.x, n1.y, n1.z, e.node_to_id, "
                "e.node_to_x, e.node_to_y, e.node_to_z, "
                "n2.gml_id, n2.x, n2.y, n2.z "
                "FROM gml_edges AS e "
                "LEFT JOIN nodes AS n1 ON (n1.gml_id = e.node_from_id) "
                "LEFT JOIN nodes AS n2 ON (n2.gml_id = e.node_to_id)";
    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hQueryStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create QUERY stmt for Edge resolver" );
        return FALSE;
    }

    /* update cursor */
    osCommand = "UPDATE gml_edges "
                "SET gml_resolved = ?, "
                "gml_string = NULL "
                "WHERE gml_id = ?";
    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hUpdateStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create UPDATE stmt for resolved Edges" );
        sqlite3_finalize ( hQueryStmt );
        return FALSE;
    }

    /* starting a TRANSACTION */
    rc = sqlite3_exec( hDB, "BEGIN", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to perform BEGIN TRANSACTION: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        sqlite3_finalize ( hQueryStmt );
        sqlite3_finalize ( hUpdateStmt );
        return FALSE;
    }
    
    /* looping on the QUERY result-set */
    while ( TRUE )
    {
        const char      *pszGmlId;
        const char      *pszGmlString = NULL;
        int             bIsGmlStringNull;
        const char      *pszFromId = NULL;
        int             bIsFromIdNull;
        double          xFrom = 0.0;
        int             bIsXFromNull;
        double          yFrom = 0.0;
        int             bIsYFromNull;
        double          zFrom = 0.0;
        int             bIsZFromNull;
        const char      *pszNodeFromId = NULL;
        int             bIsNodeFromIdNull;
        double          xNodeFrom = 0.0;
        int             bIsXNodeFromNull;
        double          yNodeFrom = 0.0;
        int             bIsYNodeFromNull;
        double          zNodeFrom = 0.0;
        int             bIsZNodeFromNull;
        const char      *pszToId = NULL;
        int             bIsToIdNull;
        double          xTo = 0.0;
        int             bIsXToNull;
        double          yTo = 0.0;
        int             bIsYToNull;
        double          zTo = 0.0;
        int             bIsZToNull;
        const char      *pszNodeToId = NULL;
        int             bIsNodeToIdNull;
        double          xNodeTo = 0.0;
        int             bIsXNodeToNull;
        double          yNodeTo = 0.0;
        int             bIsYNodeToNull;
        double          zNodeTo = 0.0;
        int             bIsZNodeToNull;

        rc = sqlite3_step( hQueryStmt );
        if( rc == SQLITE_DONE )
            break;
        else if( rc == SQLITE_ROW )
        {
            bError = FALSE;
            pszGmlId = (const char *) sqlite3_column_text( hQueryStmt, 0 );
            if( sqlite3_column_type( hQueryStmt, 1 ) == SQLITE_NULL )
                bIsGmlStringNull = TRUE;
            else
            {
                pszGmlString = (const char *) sqlite3_column_blob( hQueryStmt, 1 );
                bIsGmlStringNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 2 ) == SQLITE_NULL )
                bIsFromIdNull = TRUE;
            else
            {
                pszFromId = (const char *) sqlite3_column_text( hQueryStmt, 2 );
                bIsFromIdNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 3 ) == SQLITE_NULL )
                bIsXFromNull = TRUE;
            else
            {
                xFrom = sqlite3_column_double( hQueryStmt, 3 );
                bIsXFromNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 4 ) == SQLITE_NULL )
                bIsYFromNull = TRUE;
            else
            {
                yFrom = sqlite3_column_double( hQueryStmt, 4 );
                bIsYFromNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 5 ) == SQLITE_NULL )
                bIsZFromNull = TRUE;
            else
            {
                zFrom = sqlite3_column_double( hQueryStmt, 5 );
                bIsZFromNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 6 ) == SQLITE_NULL )
                bIsNodeFromIdNull = TRUE;
            else
            {
                pszNodeFromId = (const char *) sqlite3_column_text( hQueryStmt, 6 );
                bIsNodeFromIdNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 7 ) == SQLITE_NULL )
                bIsXNodeFromNull = TRUE;
            else
            {
                xNodeFrom = sqlite3_column_double( hQueryStmt, 7 );
                bIsXNodeFromNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 8 ) == SQLITE_NULL )
                bIsYNodeFromNull = TRUE;
            else
            {
                yNodeFrom = sqlite3_column_double( hQueryStmt, 8 );
                bIsYNodeFromNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 9 ) == SQLITE_NULL )
                bIsZNodeFromNull = TRUE;
            else
            {
                zNodeFrom = sqlite3_column_double( hQueryStmt, 9 );
                bIsZNodeFromNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 10 ) == SQLITE_NULL )
                bIsToIdNull = TRUE;
            else
            {
                pszToId = (const char *) sqlite3_column_text( hQueryStmt, 10 );
                bIsToIdNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 11 ) == SQLITE_NULL )
                bIsXToNull = TRUE;
            else
            {
                xTo = sqlite3_column_double( hQueryStmt, 11 );
                bIsXToNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 12 ) == SQLITE_NULL )
                bIsYToNull = TRUE;
            else
            {
                yTo = sqlite3_column_double( hQueryStmt, 12 );
                bIsYToNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 13 ) == SQLITE_NULL )
                bIsZToNull = TRUE;
            else
            {
                zTo = sqlite3_column_double( hQueryStmt, 13 );
                bIsZToNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 14 ) == SQLITE_NULL )
                bIsNodeToIdNull = TRUE;
            else
            {
                pszNodeToId = (const char *) sqlite3_column_text( hQueryStmt, 14 );
                bIsNodeToIdNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 15 ) == SQLITE_NULL )
                bIsXNodeToNull = TRUE;
            else
            {
                xNodeTo = sqlite3_column_double( hQueryStmt, 15 );
                bIsXNodeToNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 16 ) == SQLITE_NULL )
                bIsYNodeToNull = TRUE;
            else
            {
                yNodeTo = sqlite3_column_double( hQueryStmt, 16 );
                bIsYNodeToNull = FALSE;
            }
            if( sqlite3_column_type( hQueryStmt, 17 ) == SQLITE_NULL )
                bIsZNodeToNull = TRUE;
            else
            {
                zNodeTo = sqlite3_column_double( hQueryStmt, 17 );
                bIsZNodeToNull = FALSE;
            }

            /* checking for consistency */
            if( bIsFromIdNull == TRUE || bIsXFromNull == TRUE ||
                bIsYFromNull == TRUE )
            {
                bError = TRUE;
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Edge gml:id=\"%s\": invalid Node-from", 
                          pszGmlId );
            }
            else
            {
                if( bIsNodeFromIdNull == TRUE )
                {
                    bError = TRUE;
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Edge gml:id=\"%s\": undeclared Node gml:id=\"%s\"", 
                              pszGmlId, pszFromId );
                }
                else if( bIsXNodeFromNull == TRUE || bIsYNodeFromNull == TRUE )
                {
                    bError = TRUE;
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Edge gml:id=\"%s\": unknown coords for Node gml:id=\"%s\"", 
                              pszGmlId, pszFromId );
                }
                else if( xFrom != xNodeFrom || yFrom != yNodeFrom )
                {
                    bError = TRUE;
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Edge gml:id=\"%s\": mismatching coords for Node gml:id=\"%s\"", 
                              pszGmlId, pszFromId );
                }
                else
                {
                    if( bIsZFromNull == TRUE && bIsZNodeFromNull == TRUE )
;
                    else if(  bIsZFromNull == TRUE || bIsZNodeFromNull == TRUE )
                    {
                        bError = TRUE;
                        CPLError( CE_Failure, CPLE_AppDefined, 
                                  "Edge gml:id=\"%s\": mismatching 2D/3D for Node gml:id=\"%s\"", 
                                  pszGmlId, pszFromId );
                    }
                    else if( zFrom != zNodeFrom ) 
                    {
                        bError = TRUE;
                        CPLError( CE_Failure, CPLE_AppDefined, 
                                  "Edge gml:id=\"%s\": mismatching Z coord for Node gml:id=\"%s\"", 
                                  pszGmlId, pszFromId );
                    }
                }
            }
            if( bIsToIdNull == TRUE || bIsXToNull == TRUE ||
                bIsYToNull == TRUE )
            {
                bError = TRUE;
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Edge gml:id=\"%s\": invalid Node-to", 
                          pszGmlId );
            }
            else
            {
                if( bIsNodeToIdNull == TRUE )
                {
                    bError = TRUE;
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Edge gml:id=\"%s\": undeclared Node gml:id=\"%s\"", 
                              pszGmlId, pszToId );
                }
                else if( bIsXNodeToNull == TRUE || bIsYNodeToNull == TRUE )
                {
                    bError = TRUE;
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Edge gml:id=\"%s\": unknown coords for Node gml:id=\"%s\"", 
                              pszGmlId, pszToId );
                }
                else if( xTo != xNodeTo || yTo != yNodeTo )
                {
                    bError = TRUE;
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Edge gml:id=\"%s\": mismatching coords for Node gml:id=\"%s\"", 
                              pszGmlId, pszToId );
                }
                else
                {
                    if( bIsZToNull == TRUE && bIsZNodeToNull == TRUE )
;
                    else if(  bIsZToNull == TRUE || bIsZNodeToNull == TRUE )
                    {
                        bError = TRUE;
                        CPLError( CE_Failure, CPLE_AppDefined, 
                                  "Edge gml:id=\"%s\": mismatching 2D/3D for Node gml:id=\"%s\"", 
                                  pszGmlId, pszToId );
                    }
                    else if( zTo != zNodeTo ) 
                    {
                        bError = TRUE;
                        CPLError( CE_Failure, CPLE_AppDefined, 
                                  "Edge gml:id=\"%s\": mismatching Z coord for Node gml:id=\"%s\"", 
                                  pszGmlId, pszToId );
                    }
                }
            }

            /* updating the resolved Node */
            if( bError == FALSE && bIsGmlStringNull == FALSE &&
                bIsFromIdNull == FALSE && bIsToIdNull == FALSE )
            {
                CPLXMLNode *psNode = CPLParseXMLString( pszGmlString );
                if( psNode != NULL )
                {
                    if( gmlHugeResolveEdgeNodes( psNode, pszFromId,
                                                 pszToId ) == TRUE )
                    {
                        char * gmlText = CPLSerializeXMLTree(psNode);
                        sqlite3_reset ( hUpdateStmt );
                        sqlite3_clear_bindings ( hUpdateStmt );
                        sqlite3_bind_blob( hUpdateStmt, 1, gmlText,
                                           strlen(gmlText), SQLITE_STATIC );
                        sqlite3_bind_text( hUpdateStmt, 2, pszGmlId, -1,
                                           SQLITE_STATIC );
                        rc = sqlite3_step( hUpdateStmt );
                        if( rc != SQLITE_OK && rc != SQLITE_DONE )
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "UPDATE resoved Edge \"%s\" sqlite3_step() failed:\n  %s",
                                      pszGmlId, sqlite3_errmsg(hDB) );
                        }
                        CPLFree( gmlText );
                        iCount++;
                        if( (iCount % 1024) == 1023 )
                        {
                            /* committing the current TRANSACTION */
                            rc = sqlite3_exec( hDB, "COMMIT", NULL, NULL,
                                               &pszErrMsg );
                            if( rc != SQLITE_OK )
                            {
                                CPLError( CE_Failure, CPLE_AppDefined, 
                                          "Unable to perform COMMIT TRANSACTION: %s",
                                          pszErrMsg );
                                sqlite3_free( pszErrMsg );
                                return FALSE;
                            }
                            /* restarting a new TRANSACTION */
                            rc = sqlite3_exec( hDB, "BEGIN", NULL, NULL, &pszErrMsg );
                            if( rc != SQLITE_OK )
                            {
                                CPLError( CE_Failure, CPLE_AppDefined, 
                                          "Unable to perform BEGIN TRANSACTION: %s",
                                          pszErrMsg );
                                sqlite3_free( pszErrMsg );
                                sqlite3_finalize ( hQueryStmt );
                                sqlite3_finalize ( hUpdateStmt );
                                return FALSE;
                            }
                        }
                    }
                    CPLDestroyXMLNode( psNode );
                }
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Edge resolver QUERY: sqlite3_step(%s)", 
                      sqlite3_errmsg(hDB) );
            sqlite3_finalize ( hQueryStmt );
            sqlite3_finalize ( hUpdateStmt );
            return FALSE;
        }
    }
    sqlite3_finalize ( hQueryStmt );
    sqlite3_finalize ( hUpdateStmt );

    /* committing the current TRANSACTION */
    rc = sqlite3_exec( hDB, "COMMIT", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to perform COMMIT TRANSACTION: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }
    if( bError == TRUE )
        return FALSE;
    return TRUE;
}

static int gmlHugeFileSQLiteInsert( struct huge_helper *helper )
{
/* inserting any appropriate row into the SQLite DB */
    int rc;
    struct huge_tag *pItem;

    /* looping on GML tags */
    pItem = helper->pFirst;
    while ( pItem != NULL )
    {
        if( pItem->bHasCoords == TRUE )
        {
            if( pItem->gmlNodeFrom != NULL )
            {
                sqlite3_reset ( helper->hNodes );
                sqlite3_clear_bindings ( helper->hNodes );
                sqlite3_bind_text( helper->hNodes, 1,
                                   pItem->gmlNodeFrom->c_str(), -1,
                                   SQLITE_STATIC );
                sqlite3_bind_double ( helper->hNodes, 2, pItem->xNodeFrom );
                sqlite3_bind_double ( helper->hNodes, 3, pItem->yNodeFrom );
                if( pItem->bHasZ == TRUE )
                    sqlite3_bind_double ( helper->hNodes, 4, pItem->zNodeFrom );
                sqlite3_bind_null ( helper->hNodes, 5 );
                rc = sqlite3_step( helper->hNodes );
                if( rc != SQLITE_OK && rc != SQLITE_DONE )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "sqlite3_step() failed:\n  %s (gmlNodeFrom id=%s)", 
                              sqlite3_errmsg(helper->hDB), pItem->gmlNodeFrom->c_str() );
                    return FALSE;
                }
            }
            if( pItem->gmlNodeTo != NULL )
            {
                sqlite3_reset ( helper->hNodes );
                sqlite3_clear_bindings ( helper->hNodes );
                sqlite3_bind_text( helper->hNodes, 1, pItem->gmlNodeTo->c_str(),
                                   -1, SQLITE_STATIC );
                sqlite3_bind_double ( helper->hNodes, 2, pItem->xNodeTo );
                sqlite3_bind_double ( helper->hNodes, 3, pItem->yNodeTo );
                if ( pItem->bHasZ == TRUE )
                    sqlite3_bind_double ( helper->hNodes, 4, pItem->zNodeTo );
                sqlite3_bind_null ( helper->hNodes, 5 );
                rc = sqlite3_step( helper->hNodes );
                if( rc != SQLITE_OK && rc != SQLITE_DONE )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "sqlite3_step() failed:\n  %s (gmlNodeTo id=%s)", 
                              sqlite3_errmsg(helper->hDB), pItem->gmlNodeTo->c_str() );
                    return FALSE;
                }
            }
        }

        /* gml:id */
        sqlite3_reset( helper->hEdges );
        sqlite3_clear_bindings( helper->hEdges );
        sqlite3_bind_text( helper->hEdges, 1, pItem->gmlId->c_str(), -1,
                           SQLITE_STATIC );
        if( pItem->bIsNodeFromHref == FALSE && pItem->bIsNodeToHref == FALSE )
        {
            sqlite3_bind_null( helper->hEdges, 2 );
            sqlite3_bind_blob( helper->hEdges, 3, pItem->gmlTagValue->c_str(),
                               strlen( pItem->gmlTagValue->c_str() ),
                               SQLITE_STATIC );
        }
        else
        {
            sqlite3_bind_blob( helper->hEdges, 2, pItem->gmlTagValue->c_str(),
                               strlen( pItem->gmlTagValue->c_str() ),
                               SQLITE_STATIC );
            sqlite3_bind_null( helper->hEdges, 3 );
        }
        if( pItem->gmlNodeFrom != NULL )
            sqlite3_bind_text( helper->hEdges, 4, pItem->gmlNodeFrom->c_str(),
                               -1, SQLITE_STATIC );
        else
            sqlite3_bind_null( helper->hEdges, 4 );
        if( pItem->bHasCoords == TRUE )
        {
            sqlite3_bind_double( helper->hEdges, 5, pItem->xNodeFrom );
            sqlite3_bind_double( helper->hEdges, 6, pItem->yNodeFrom );
            if( pItem->bHasZ == TRUE )
                sqlite3_bind_double( helper->hEdges, 7, pItem->zNodeFrom );
            else
                sqlite3_bind_null( helper->hEdges, 7 );
        }
        else
        {
            sqlite3_bind_null( helper->hEdges, 5 );
            sqlite3_bind_null( helper->hEdges, 6 );
            sqlite3_bind_null( helper->hEdges, 7 );
        }
        if( pItem->gmlNodeTo != NULL )
            sqlite3_bind_text( helper->hEdges, 8, pItem->gmlNodeTo->c_str(),
                               -1, SQLITE_STATIC );
        else
            sqlite3_bind_null( helper->hEdges, 8 );
        if( pItem->bHasCoords == TRUE )
        {
            sqlite3_bind_double( helper->hEdges, 9, pItem->xNodeTo );
            sqlite3_bind_double( helper->hEdges, 10, pItem->yNodeTo );
            if( pItem->bHasZ == TRUE )
                sqlite3_bind_double( helper->hEdges, 11, pItem->zNodeTo );
            else
                sqlite3_bind_null( helper->hEdges, 11 );
        }
        else
        {
            sqlite3_bind_null( helper->hEdges, 9 );
            sqlite3_bind_null( helper->hEdges, 10 );
            sqlite3_bind_null( helper->hEdges, 11 );
        }
        rc = sqlite3_step( helper->hEdges );
        if( rc != SQLITE_OK && rc != SQLITE_DONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "sqlite3_step() failed:\n  %s (edge gml:id=%s)", 
                      sqlite3_errmsg(helper->hDB), pItem->gmlId->c_str() );
            return FALSE;
        }
        pItem = pItem->pNext;
    }
    return TRUE;
}

static void gmlHugeFileReset( struct huge_helper *helper )
{
/* resetting an empty helper struct */
    struct huge_tag *pNext;
    struct huge_tag *p = helper->pFirst;

    /* cleaning any previous item */
    while( p != NULL )
    {
        pNext = p->pNext;
        if( p->gmlTagValue != NULL )
            delete p->gmlTagValue;
        if( p->gmlId != NULL )
            delete p->gmlId;
        if( p->gmlNodeFrom != NULL )
            delete p->gmlNodeFrom;
        if( p->gmlNodeTo != NULL )
            delete p->gmlNodeTo;
        delete p;
        p = pNext;
    }
    helper->pFirst = NULL;
    helper->pLast = NULL;
}

static void gmlHugeFileHrefReset( struct huge_helper *helper )
{
/* resetting an empty helper struct */
    struct huge_href *pNext;
    struct huge_href *p = helper->pFirstHref;

    /* cleaning any previous item */
    while( p != NULL )
    {
        pNext = p->pNext;
        if( p->gmlId != NULL )
            delete p->gmlId;
        if( p->gmlText != NULL )
            delete p->gmlText;
        delete p;
        p = pNext;
    }
    helper->pFirstHref = NULL;
    helper->pLastHref = NULL;
}

static int gmlHugeFileHrefCheck( struct huge_helper *helper )
{
/* testing for unresolved items */
    int bError = FALSE;
	struct huge_href *p = helper->pFirstHref;
	while( p != NULL )
    {
		if( p->gmlText == NULL)
		{
			bError = TRUE;
			CPLError( CE_Failure, CPLE_AppDefined, 
                      "Edge xlink:href\"%s\": unresolved match", 
                      p->gmlId->c_str() );
		}
        p = p->pNext;
    }
	if( bError == TRUE )
		return FALSE;
	return TRUE;
}

static void gmlHugeFileRewiterReset( struct huge_helper *helper )
{
/* resetting an empty helper struct */
    struct huge_parent *pNext;
    struct huge_parent *p = helper->pFirstParent;

    /* cleaning any previous item */
    while( p != NULL )
    {
        pNext = p->pNext;
        struct huge_child *pChildNext;
        struct huge_child *pChild = p->pFirst;
        while( pChild != NULL )
        {
            pChildNext = pChild->pNext;
            delete pChild;
            pChild = pChildNext;
        }
        delete p;
        p = pNext;
    }
    helper->pFirstParent = NULL;
    helper->pLastParent = NULL;
}

static struct huge_tag *gmlHugeAddToHelper( struct huge_helper *helper,
                                            CPLString *gmlId,
                                            CPLString *gmlFragment )
{
/* adding an item  into the linked list */
    struct huge_tag *pItem;

    /* checking against duplicates */
    pItem = helper->pFirst;
    while( pItem != NULL )
    {
        if( EQUAL( pItem->gmlId->c_str(), gmlId->c_str() ) )
            return NULL;
        pItem = pItem->pNext;
    }	

    pItem = new struct huge_tag;
    pItem->gmlId = gmlId;
    pItem->gmlTagValue = gmlFragment;
    pItem->gmlNodeFrom = NULL;
    pItem->gmlNodeTo = NULL;
    pItem->bIsNodeFromHref = FALSE;
    pItem->bIsNodeToHref = FALSE;
    pItem->bHasCoords = FALSE;
    pItem->bHasZ = FALSE;
    pItem->pNext = NULL;

    /* appending the item to the linked list */
    if ( helper->pFirst == NULL )
        helper->pFirst = pItem;
    if ( helper->pLast != NULL )
        helper->pLast->pNext = pItem;
    helper->pLast = pItem;
    return pItem;
}

static void gmlHugeAddPendingToHelper( struct huge_helper *helper,
                                       CPLString *gmlId,
                                       const CPLXMLNode *psParent,
                                       const CPLXMLNode *psNode,
                                       int bIsDirectedEdge,
                                       char cOrientation )
{
/* inserting an item into the linked list */
    struct huge_href *pItem;

    /* checking against duplicates */
    pItem = helper->pFirstHref;
    while( pItem != NULL )
    {
        if( EQUAL( pItem->gmlId->c_str(), gmlId->c_str() ) &&
            pItem->psParent == psParent  && 
            pItem->psNode == psNode && 
            pItem->cOrientation == cOrientation &&
            pItem->bIsDirectedEdge == bIsDirectedEdge )
            {
                delete gmlId;
                return;
            }
            pItem = pItem->pNext;
    }	

    pItem = new struct huge_href;
    pItem->gmlId = gmlId;
    pItem->gmlText = NULL;
    pItem->psParent = psParent;
    pItem->psNode = psNode;
    pItem->bIsDirectedEdge = bIsDirectedEdge;
    pItem->cOrientation = cOrientation;
    pItem->pNext = NULL;

    /* appending the item to the linked list */
    if ( helper->pFirstHref == NULL )
        helper->pFirstHref = pItem;
    if ( helper->pLastHref != NULL )
        helper->pLastHref->pNext = pItem;
    helper->pLastHref = pItem;
}

static int gmlHugeFindGmlId( const CPLXMLNode *psNode, CPLString **gmlId )
{
/* attempting to identify a gml:id value      */
    *gmlId = NULL;
    const CPLXMLNode *psChild = psNode->psChild;
    while( psChild != NULL )
    {
        if( psChild->eType == CXT_Attribute &&
            EQUAL( psChild->pszValue, "gml:id" ) )
        {
            const CPLXMLNode *psIdValue = psChild->psChild;
            if( psIdValue != NULL )
            {
                if( psIdValue->eType == CXT_Text )
                {
                    *gmlId = new CPLString(psIdValue->pszValue);
                    return TRUE;
                }
            }
        }
        psChild = psChild->psNext;
    }
    return FALSE;
}

static void gmlHugeFileNodeCoords( struct huge_tag *pItem,
                                   const CPLXMLNode * psNode,
                                   CPLString **nodeSrs )
{
/* 
/ this function attempts to set coordinates for <Node> items
/ when required (an <Edge> is expected to be processed)
*/

/* attempting to fetch Node coordinates */
    CPLXMLNode *psTopoCurve = CPLCreateXMLNode(NULL, CXT_Element, "TopoCurve");
    CPLXMLNode *psDirEdge = CPLCreateXMLNode(psTopoCurve, CXT_Element, "directedEdge");
    CPLXMLNode *psEdge = CPLCloneXMLTree((CPLXMLNode *)psNode);
    CPLAddXMLChild( psDirEdge, psEdge );
    OGRGeometryCollection *poColl = (OGRGeometryCollection *)
                                    GML2OGRGeometry_XMLNode( psTopoCurve, FALSE );
    CPLDestroyXMLNode( psTopoCurve );
    if( poColl != NULL )
    {
        int iCount = poColl->getNumGeometries();
        if( iCount == 1 )
        {
            OGRGeometry * poChild = (OGRGeometry*)poColl->getGeometryRef(0);
            int type = wkbFlatten( poChild->getGeometryType());
            if( type == wkbLineString )
            {
                OGRLineString *poLine = (OGRLineString *)poChild;
                int iPoints =  poLine->getNumPoints();
                if( iPoints >= 2 )
                {
                    pItem->bHasCoords = TRUE;
                    pItem->xNodeFrom = poLine->getX( 0 );
                    pItem->yNodeFrom = poLine->getY( 0 );
                    pItem->xNodeTo = poLine->getX( iPoints - 1 );
                    pItem->yNodeTo = poLine->getY( iPoints - 1 );
                    if( poLine->getCoordinateDimension() == 3 )
                    {
                        pItem->zNodeFrom = poLine->getZ( 0 );
                        pItem->zNodeTo = poLine->getZ( iPoints - 1 );
                        pItem->bHasZ = TRUE;
                    }
                    else
                        pItem->bHasZ = FALSE;
                }
            }
        }
        delete poColl;
    }
	
    /* searching the <directedNode> sub-tags */
    const CPLXMLNode *psChild = psNode->psChild;
    while( psChild != NULL )
    {
        if( psChild->eType == CXT_Element &&
            EQUAL( psChild->pszValue, "directedNode" ) )
        {
            char cOrientation = '+';
            const char *pszGmlId = NULL;
            int bIsHref = FALSE;
            const CPLXMLNode *psAttr = psChild->psChild;
            while( psAttr != NULL )
            {
                if( psAttr->eType == CXT_Attribute &&
                    EQUAL( psAttr->pszValue, "xlink:href" ) )
                {
                    const CPLXMLNode *psHref = psAttr->psChild;
                    if( psHref != NULL )
                    {
                        if( psHref->eType == CXT_Text )
                        {
                            pszGmlId = psHref->pszValue;
                            bIsHref = TRUE;
                        }
                    }
                }
                if( psAttr->eType == CXT_Attribute &&
                    EQUAL( psAttr->pszValue, "orientation" ) )
                {
                    const CPLXMLNode *psOrientation = psAttr->psChild;
                    if( psOrientation != NULL )
                    {
                        if( psOrientation->eType == CXT_Text )
                        {
                            cOrientation = *(psOrientation->pszValue);
                        }
                    }
                }
                if( psAttr->eType == CXT_Element &&
                    EQUAL( psAttr->pszValue, "Node" ) )
                {
                    const CPLXMLNode *psId = psAttr->psChild;
                    while( psId != NULL )
                    {		
                        if( psId->eType == CXT_Attribute &&
                            EQUAL( psId->pszValue, "gml:id" ) )
                        {
                            const CPLXMLNode *psIdGml = psId->psChild;
                            if( psIdGml != NULL )
                            {
                                if( psIdGml->eType == CXT_Text )
                                {
                                    pszGmlId = psIdGml->pszValue;
                                    bIsHref = FALSE;
                                }
                            }
                        }
                        psId = psId->psNext;
                    }
                }
                psAttr = psAttr->psNext;
            }
            if( pszGmlId != NULL )
            {
                CPLString* posNode;
                if( bIsHref == TRUE )
                {
                    if (pszGmlId[0] != '#')
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "Only values of xlink:href element starting with '#' are supported, "
                                 "so %s will not be properly recognized", pszGmlId);
                    }
                    posNode = new CPLString(pszGmlId+1);
                }
                else
                    posNode = new CPLString(pszGmlId);
                if( cOrientation == '-' )
                {
                     pItem->gmlNodeFrom = posNode;
                    pItem->bIsNodeFromHref = bIsHref;
                }
                else
                {
                    pItem->gmlNodeTo = posNode;
                    pItem->bIsNodeToHref = bIsHref;
                }
                pszGmlId = NULL;
                bIsHref = FALSE;
                cOrientation = '+';
            }
        }
        psChild = psChild->psNext;
    }
}

static void gmlHugeFileCheckXrefs( struct huge_helper *helper, 
                                   const CPLXMLNode *psNode )
{
/* identifying <Edge> GML nodes */
    if( psNode->eType == CXT_Element )
    {
        if( EQUAL(psNode->pszValue, "Edge") == TRUE )
        {
            CPLString *gmlId = NULL;
            if( gmlHugeFindGmlId( psNode, &gmlId ) == TRUE )
            {
                char * gmlText = CPLSerializeXMLTree((CPLXMLNode *)psNode);
                CPLString *gmlValue = new CPLString(gmlText);
                CPLFree( gmlText );
                struct huge_tag *pItem = gmlHugeAddToHelper( helper, gmlId,
                                                             gmlValue );
                if( pItem != NULL )
                    gmlHugeFileNodeCoords( pItem, psNode, &(helper->nodeSrs) );
                else
                {
                    delete gmlId;
                    delete gmlValue;
                }
            }
        }
    }

    /* recursively scanning each Child GML node */
    const CPLXMLNode *psChild = psNode->psChild;
    while( psChild != NULL )
    {
        if( psChild->eType == CXT_Element )
        {
            if( EQUAL(psChild->pszValue, "Edge") == TRUE ||
                EQUAL(psChild->pszValue, "directedEdge") == TRUE )
            {
                gmlHugeFileCheckXrefs( helper, psChild );
            }
            if( EQUAL(psChild->pszValue, "directedFace") == TRUE )
            {
                const CPLXMLNode *psFace = psChild->psChild;
                if( psFace != NULL )
                {
                    if( psFace->eType == CXT_Element &&
                        EQUAL(psFace->pszValue, "Face") == TRUE)
                    {
                        const CPLXMLNode *psDirEdge = psFace->psChild;
                        while (psDirEdge != NULL)
                        {
                            const CPLXMLNode *psEdge = psDirEdge->psChild;
                            while( psEdge != NULL)
                            {
                                if( psEdge->eType == CXT_Element &&
                                    EQUAL(psEdge->pszValue, "Edge") == TRUE)
                                    gmlHugeFileCheckXrefs( helper, psEdge );
                                psEdge = psEdge->psNext;
                            }
                            psDirEdge = psDirEdge->psNext;
                        }
                    }
                }
            }
        }
        psChild = psChild->psNext;
    }

    /* recursively scanning each GML of the same level */
    const CPLXMLNode *psNext = psNode->psNext;
    while( psNext != NULL )
    {
        if( psNext->eType == CXT_Element )
        {
            if( EQUAL(psNext->pszValue, "Edge") == TRUE ||
                EQUAL(psNext->pszValue, "directedEdge") == TRUE )
            {
                gmlHugeFileCheckXrefs( helper, psNext );
            }
        }
        psNext = psNext->psNext;
    }
}

static void gmlHugeFileCleanUp ( struct huge_helper *helper ) 
{
/* cleaning up any SQLite handle */
    if( helper->hNodes != NULL )
        sqlite3_finalize ( helper->hNodes );
    if( helper->hEdges != NULL )
        sqlite3_finalize ( helper->hEdges );
    if( helper->hDB != NULL )
        sqlite3_close( helper->hDB );
    if( helper->nodeSrs != NULL )
        delete helper->nodeSrs;
}

static void gmlHugeFileCheckPendingHrefs( struct huge_helper *helper,
                                          const CPLXMLNode *psParent,
                                          const CPLXMLNode *psNode )
{
/* identifying any xlink:href to be replaced */
    if( psNode->eType == CXT_Element )
    {
        if( EQUAL(psNode->pszValue, "directedEdge") == TRUE )
        {
            char cOrientation = '+';
            CPLXMLNode *psAttr = psNode->psChild;
            while( psAttr != NULL )
            {
                if( psAttr->eType == CXT_Attribute &&
                    EQUAL( psAttr->pszValue, "orientation" ) )
                {
                    const CPLXMLNode *psOrientation = psAttr->psChild;
                    if( psOrientation != NULL )
                    {
                        if( psOrientation->eType == CXT_Text )
                            cOrientation = *(psOrientation->pszValue);
                    }
                }
                psAttr = psAttr->psNext;
            }
            psAttr = psNode->psChild;
            while( psAttr != NULL )
            {
                if( psAttr->eType == CXT_Attribute &&
                    EQUAL( psAttr->pszValue, "xlink:href" ) )
                {
                    const CPLXMLNode *pszHref = psAttr->psChild;
                    if( pszHref != NULL )
                    {
                        if( pszHref->eType == CXT_Text )
                        {
                            if (pszHref->pszValue[0] != '#')
                            {
                                CPLError(CE_Warning, CPLE_NotSupported,
                                        "Only values of xlink:href element starting with '#' are supported, "
                                        "so %s will not be properly recognized", pszHref->pszValue);
                            }
                            CPLString *gmlId = new CPLString(pszHref->pszValue+1);
                            gmlHugeAddPendingToHelper( helper, gmlId, psParent,
                                                       psNode, TRUE, cOrientation );
                        }
                    }
                }
                psAttr = psAttr->psNext;
            }
        }
    }

    /* recursively scanning each Child GML node */
    const CPLXMLNode *psChild = psNode->psChild;
    while( psChild != NULL )
    {
        if( psChild->eType == CXT_Element )
        {
            if( EQUAL(psChild->pszValue, "directedEdge") == TRUE ||
                EQUAL(psChild->pszValue, "directedFace") == TRUE ||
                EQUAL(psChild->pszValue, "Face") == TRUE)
            {
                gmlHugeFileCheckPendingHrefs( helper, psNode, psChild );
            }
        }
        psChild = psChild->psNext;
    }

    /* recursively scanning each GML of the same level */
    const CPLXMLNode *psNext = psNode->psNext;
    while( psNext != NULL )
    {
        if( psNext->eType == CXT_Element )
        {
            if( EQUAL(psNext->pszValue, "Face") == TRUE )
            {
                gmlHugeFileCheckPendingHrefs( helper, psParent, psNext );
            }
        }
        psNext = psNext->psNext;
    }
}

static void gmlHugeSetHrefGmlText( struct huge_helper *helper,
                                   const char *pszGmlId,
                                   const char *pszGmlText )
{
/* setting the GML text for the corresponding gml:id */
    struct huge_href *pItem = helper->pFirstHref;
    while( pItem != NULL )
    {
        if( EQUAL( pItem->gmlId->c_str(), pszGmlId ) == TRUE )
        {
            if( pItem->gmlText != NULL)
                delete pItem->gmlText;
            pItem->gmlText = new CPLString( pszGmlText );
            return;
        }
        pItem = pItem->pNext;
    }
}

static struct huge_parent *gmlHugeFindParent( struct huge_helper *helper,
                                              CPLXMLNode *psParent )
{
/* inserting a GML Node (parent) to be rewritted */
    struct huge_parent *pItem = helper->pFirstParent;

    /* checking if already exists */
    while( pItem != NULL )
    {
        if( pItem->psParent == psParent )
            return pItem;
        pItem = pItem->pNext;
    }

    /* creating a new Parent Node */
    pItem = new struct huge_parent;
    pItem->psParent = psParent;
    pItem->pFirst = NULL;
    pItem->pLast = NULL;
    pItem->pNext = NULL;
    if( helper->pFirstParent == NULL )
        helper->pFirstParent = pItem;
    if( helper->pLastParent != NULL )
        helper->pLastParent->pNext = pItem;
    helper->pLastParent = pItem;

    /* inserting any Child node into the Parent */
    CPLXMLNode *psChild = psParent->psChild;
    while( psChild != NULL )
    {
        struct huge_child *pChildItem;
        pChildItem = new struct huge_child;
        pChildItem->psChild = psChild;
        pChildItem->pItem = NULL;
        pChildItem->pNext = NULL;
        if( pItem->pFirst == NULL )
            pItem->pFirst = pChildItem;
        if( pItem->pLast != NULL )
            pItem->pLast->pNext = pChildItem;
        pItem->pLast = pChildItem;
        psChild = psChild->psNext;
    }
    return pItem;
}

static int gmlHugeSetChild( struct huge_parent *pParent,
                            struct huge_href *pItem )
{
/* setting a Child Node to be rewritted */
    struct huge_child *pChild = pParent->pFirst;
    while( pChild != NULL )
    {
         if( pChild->psChild == pItem->psNode )
         {
            pChild->pItem = pItem;
            return TRUE;
         }
         pChild = pChild->pNext;
    }
    return FALSE;
}

static int gmlHugeResolveEdges( struct huge_helper *helper,
                                CPLXMLNode *psNode, sqlite3 *hDB )
{
/* resolving GML <Edge> xlink:href */
    CPLString      osCommand;
    sqlite3_stmt   *hStmtEdges;
    int            rc;
    int            bIsComma = FALSE;
    int            bError = FALSE;
    struct huge_href *pItem;
    struct huge_parent *pParent;

    /* query cursor [Edges] */
    osCommand = "SELECT gml_id, gml_resolved "
                "FROM gml_edges "
                "WHERE gml_id IN (";
    pItem = helper->pFirstHref;
    while( pItem != NULL )
    {
        if( bIsComma == TRUE )
            osCommand += ", ";
        else
            bIsComma = TRUE;
        osCommand += "'";
        osCommand += pItem->gmlId->c_str();
        osCommand += "'";
        pItem = pItem->pNext;
    }
    osCommand += ")";
    rc = sqlite3_prepare_v2( hDB, osCommand.c_str(), -1, &hStmtEdges, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create QUERY stmt for EDGES" );
        return FALSE;
    }
    while ( TRUE )
    {
        rc = sqlite3_step( hStmtEdges );
        if( rc == SQLITE_DONE )
            break;
        if ( rc == SQLITE_ROW )
        {
            const char *pszGmlId;
            const char *pszGmlText = NULL;
            pszGmlId = (const char *)sqlite3_column_text ( hStmtEdges, 0 );
            if( sqlite3_column_type( hStmtEdges, 1 ) != SQLITE_NULL )
                pszGmlText = (const char *)sqlite3_column_text ( hStmtEdges, 1 );
            gmlHugeSetHrefGmlText( helper, pszGmlId, pszGmlText );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Edge xlink:href QUERY: sqlite3_step(%s)", 
                      sqlite3_errmsg(hDB) );
            bError = TRUE;
            break;
        }
    }
    sqlite3_finalize ( hStmtEdges );
    if( bError == TRUE )
        return FALSE;

    /* indentifying any GML node to be rewritten */
    pItem = helper->pFirstHref;
    while( pItem != NULL )
    {
        if( pItem->gmlText == NULL || pItem->psParent == NULL ||
            pItem->psNode == NULL )
        {
            bError = TRUE;
            break;
        }
        pParent = gmlHugeFindParent( helper, (CPLXMLNode *)pItem->psParent );
        if( gmlHugeSetChild( pParent, pItem ) == FALSE )
        {
            bError = TRUE;
            break;
        }
        pItem = pItem->pNext;
    }

    if( bError == FALSE )
    {
    /* rewriting GML nodes */
        pParent = helper->pFirstParent;
        while( pParent != NULL )
        {
            struct huge_child *pChild;

            /* removing any Child node from the Parent */
            pChild = pParent->pFirst;
            while( pChild != NULL )
            {
                CPLRemoveXMLChild( pParent->psParent, pChild->psChild );

                /* destroyng any Child Node to be rewritten */
                if( pChild->pItem != NULL )
                    CPLDestroyXMLNode( pChild->psChild );
                pChild = pChild->pNext;
            }

            /* rewriting the Parent Node */
            pChild = pParent->pFirst;
            while( pChild != NULL )
            {
                if( pChild->pItem == NULL )
                {
                    /* reinserting any untouched Child Node */
                    CPLAddXMLChild( pParent->psParent, pChild->psChild );
                }
                else
                {
                    /* rewriting a Child Node */
                    CPLXMLNode *psNewNode = CPLCreateXMLNode(NULL, CXT_Element, "directedEdge");
                    if( pChild->pItem->cOrientation == '-' )
                    {
                        CPLXMLNode *psOrientationNode = CPLCreateXMLNode(psNewNode, CXT_Attribute, "orientation");
                        CPLCreateXMLNode(psOrientationNode, CXT_Text, "-");
                    }
                    CPLXMLNode *psEdge = CPLParseXMLString(pChild->pItem->gmlText->c_str());
                    CPLAddXMLChild( psNewNode, psEdge );
                    CPLAddXMLChild( pParent->psParent, psNewNode );
                }
                pChild = pChild->pNext;
            }
            pParent = pParent->pNext;
        }
    }

    /* resetting the Rewrite Helper to an empty state */
    gmlHugeFileRewiterReset( helper );
    if( bError == TRUE )
        return FALSE;
    return TRUE;
}

static int gmlHugeFileWriteResolved ( struct huge_helper *helper, 
                                      const char *pszOutputFilename, 
                                      GMLReader *pReader,
                                      int *m_bSequentialLayers ) 
{
/* writing the resolved GML file */
    VSILFILE       *fp;
    GMLFeature     *poFeature;
    const char     *osCommand;
    int            rc;
    sqlite3        *hDB = helper->hDB;
    sqlite3_stmt   *hStmtNodes;
    int            bError = FALSE;
    int            iOutCount = 0;

/* -------------------------------------------------------------------- */
/*      Opening the resolved GML file                                   */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszOutputFilename, "w" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open %.500s to write.", pszOutputFilename );
        return FALSE;
    }

    /* query cursor [Nodes] */
    osCommand = "SELECT gml_id, x, y, z "
                "FROM nodes";
    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hStmtNodes, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create QUERY stmt for NODES" );
        VSIFCloseL( fp );
        return FALSE;
    }

    VSIFPrintfL ( fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" ); 
    VSIFPrintfL ( fp, "<ResolvedTopoFeatureCollection  "
                      "xmlns:gml=\"http://www.opengis.net/gml\">\n" );		
    VSIFPrintfL ( fp, "  <ResolvedTopoFeatureMembers>\n" );
    /* exporting Nodes */
    GFSTemplateList *pCC = new GFSTemplateList();
    while ( TRUE )
    {
        rc = sqlite3_step( hStmtNodes );
        if( rc == SQLITE_DONE )
            break;
        if ( rc == SQLITE_ROW )
        {
            const char *pszGmlId;
            char* pszEscaped;
            double x;
            double y;
            double z = 0.0;
            int bHasZ = FALSE;
            pszGmlId = (const char *)sqlite3_column_text ( hStmtNodes, 0 );
            x = sqlite3_column_double ( hStmtNodes, 1 );
            y = sqlite3_column_double ( hStmtNodes, 2 );
            if ( sqlite3_column_type( hStmtNodes, 3 ) == SQLITE_FLOAT )
            {
                z = sqlite3_column_double ( hStmtNodes, 3 );
                bHasZ = TRUE;
            }

            /* inserting a node into the resolved GML file */
            pCC->Update( "ResolvedNodes", TRUE );
            VSIFPrintfL ( fp, "    <ResolvedNodes>\n" );
            pszEscaped = CPLEscapeString( pszGmlId, -1, CPLES_XML );
            VSIFPrintfL ( fp, "      <NodeGmlId>%s</NodeGmlId>\n", pszEscaped );
            CPLFree(pszEscaped);
            VSIFPrintfL ( fp, "      <ResolvedGeometry> \n" );
            if ( helper->nodeSrs == NULL )
                VSIFPrintfL ( fp, "        <gml:Point srsDimension=\"%d\">",
                              ( bHasZ == TRUE ) ? 3 : 2 );
            else
            {
                pszEscaped = CPLEscapeString( helper->nodeSrs->c_str(), -1, CPLES_XML );
                VSIFPrintfL ( fp, "        <gml:Point srsDimension=\"%d\""
                                  " srsName=\"%s\">",
                                  ( bHasZ == TRUE ) ? 3 : 2,
                              pszEscaped );
                CPLFree(pszEscaped);
            }
            if ( bHasZ == TRUE )
                VSIFPrintfL ( fp, "<gml:pos>%1.8f %1.8f %1.8f</gml:pos>"
                                  "</gml:Point>\n",
                              x, y, z );
            else
                VSIFPrintfL ( fp, "<gml:pos>%1.8f %1.8f</gml:pos>"
                                  "</gml:Point>\n",
                              x, y );
            VSIFPrintfL ( fp, "      </ResolvedGeometry> \n" );
            VSIFPrintfL ( fp, "    </ResolvedNodes>\n" );
            iOutCount++;
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "ResolvedNodes QUERY: sqlite3_step(%s)", 
            sqlite3_errmsg(hDB) );
            sqlite3_finalize ( hStmtNodes );
            return FALSE;
        }
    }
    sqlite3_finalize ( hStmtNodes );

    /* processing GML features */
    while( (poFeature = pReader->NextFeature()) != NULL )
    {
        GMLFeatureClass *poClass = poFeature->GetClass();
        const CPLXMLNode* const * papsGeomList = poFeature->GetGeometryList();
        int iPropCount = poClass->GetPropertyCount();
		
        int b_has_geom = FALSE;
        VSIFPrintfL ( fp, "    <%s>\n", poClass->GetElementName() ); 

        for( int iProp = 0; iProp < iPropCount; iProp++ )
        {
            GMLPropertyDefn *poPropDefn = poClass->GetProperty( iProp );
            const char *pszPropName = poPropDefn->GetName();
            const GMLProperty *poProp = poFeature->GetProperty( iProp );

            if( poProp != NULL )
            {
                for( int iSub = 0; iSub < poProp->nSubProperties; iSub++ )
                {
                    char *gmlText = CPLEscapeString( poProp->papszSubProperties[iSub],
                                                    -1,
                                                    CPLES_XML );
                    VSIFPrintfL ( fp, "      <%s>%s</%s>\n", 
                                  pszPropName, gmlText, pszPropName ); 
                    CPLFree( gmlText );
                }
            }
        }

        if( papsGeomList != NULL )
        {
            int i = 0;
            const CPLXMLNode *psNode = papsGeomList[i];
            while( psNode != NULL )
            {
                char *pszResolved = NULL;
                int bNotToBeResolved;
                if( psNode->eType != CXT_Element )
                    bNotToBeResolved = TRUE;
                else
                {
                    if( EQUAL(psNode->pszValue, "TopoCurve") == TRUE ||
                        EQUAL(psNode->pszValue, "TopoSurface") == TRUE )
                        bNotToBeResolved = FALSE;
                    else
                        bNotToBeResolved = TRUE;
                }
                if( bNotToBeResolved == TRUE )
                {
                    VSIFPrintfL ( fp, "      <ResolvedGeometry> \n" );
                    pszResolved = CPLSerializeXMLTree((CPLXMLNode *)psNode);
                    VSIFPrintfL ( fp, "        %s\n", pszResolved );
                    CPLFree( pszResolved );
                    VSIFPrintfL ( fp, "      </ResolvedGeometry>\n" );
                    b_has_geom = TRUE;
                }
                else
                {
                    gmlHugeFileCheckPendingHrefs( helper, psNode, psNode );
                    if( helper->pFirstHref == NULL )
                    {
                        VSIFPrintfL ( fp, "      <ResolvedGeometry> \n" );
                        pszResolved = CPLSerializeXMLTree((CPLXMLNode *)psNode);
                        VSIFPrintfL ( fp, "        %s\n", pszResolved );
                        CPLFree( pszResolved );
                        VSIFPrintfL ( fp, "      </ResolvedGeometry>\n" );
                        b_has_geom = TRUE;
                    }
                    else
                    {
                        if( gmlHugeResolveEdges( helper, (CPLXMLNode *)psNode,
                                                         hDB ) == FALSE)
                            bError = TRUE;
                        if( gmlHugeFileHrefCheck( helper ) == FALSE )
							bError = TRUE;
                        VSIFPrintfL ( fp, "      <ResolvedGeometry> \n" );
                        pszResolved = CPLSerializeXMLTree((CPLXMLNode *)psNode);
                        VSIFPrintfL ( fp, "        %s\n", pszResolved );
                        CPLFree( pszResolved );
                        VSIFPrintfL ( fp, "      </ResolvedGeometry>\n" );
                        b_has_geom = TRUE;
                        gmlHugeFileHrefReset( helper );
                    }
                }
                i++;
                psNode = papsGeomList[i];
            }
        }
        pCC->Update( poClass->GetElementName(), b_has_geom );

        VSIFPrintfL ( fp, "    </%s>\n", poClass->GetElementName() ); 

        delete poFeature;
        iOutCount++;
    }

    VSIFPrintfL ( fp, "  </ResolvedTopoFeatureMembers>\n" ); 
    VSIFPrintfL ( fp, "</ResolvedTopoFeatureCollection>\n" ); 

    VSIFCloseL( fp );
		
    gmlUpdateFeatureClasses( pCC, pReader, m_bSequentialLayers );
    if ( *m_bSequentialLayers == TRUE )
        pReader->ReArrangeTemplateClasses( pCC );
    delete pCC;
    if( bError == TRUE || iOutCount == 0 )
        return FALSE;
    return TRUE;
}

/**************************************************************/
/*                                                            */
/* private member(s):                                         */
/* any other funtion is implemented as "internal" static,     */
/* so to make all the SQLite own stuff nicely "invisible"     */
/*                                                            */
/**************************************************************/

int GMLReader::ParseXMLHugeFile( const char *pszOutputFilename,
                                 const int bSqliteIsTempFile,
                                 const int iSqliteCacheMB )

{
    GMLFeature  *poFeature;
    int iFeatureUID = 0;
    int rc;
    sqlite3 *hDB = NULL;
    CPLString osSQLiteFilename;
    const char *pszSQLiteFilename = NULL;
    struct huge_helper helper;
    char *pszErrMsg = NULL;

    /* initializing the helper struct */
    helper.hDB = NULL;
    helper.hNodes = NULL;
    helper.hEdges = NULL;
    helper.nodeSrs = NULL;
    helper.pFirst = NULL;
    helper.pLast = NULL;
    helper.pFirstHref = NULL;
    helper.pLastHref = NULL;
    helper.pFirstParent = NULL;
    helper.pLastParent = NULL;
	
/* -------------------------------------------------------------------- */
/*      Creating/Opening the SQLite DB file                             */
/* -------------------------------------------------------------------- */
    osSQLiteFilename = CPLResetExtension( m_pszFilename, "sqlite" );
    pszSQLiteFilename = osSQLiteFilename.c_str();

    VSIStatBufL statBufL;
    if ( VSIStatExL ( pszSQLiteFilename, &statBufL, VSI_STAT_EXISTS_FLAG) == 0)
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "sqlite3_open(%s) failed:\n\tDB-file already exists", 
                  pszSQLiteFilename );
        return FALSE;
    }

    rc = sqlite3_open( pszSQLiteFilename, &hDB );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "sqlite3_open(%s) failed: %s", 
                  pszSQLiteFilename, sqlite3_errmsg( hDB ) );
        return FALSE;
    }
    helper.hDB = hDB;
	
/* 
* setting SQLite for max speed; this is intrinsically unsafe,
* and the DB file could be potentially damaged (power failure ...]
*
* but after all this one simply is a TEMPORARY FILE !!
* so there is no real risk condition
*/
    rc = sqlite3_exec( hDB, "PRAGMA synchronous = OFF", NULL, NULL,
                       &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to set PRAGMA synchronous = OFF: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
    }
    rc = sqlite3_exec( hDB, "PRAGMA journal_mode = OFF", NULL, NULL,
                       &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to set PRAGMA journal_mode = OFF: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
    }
    rc = sqlite3_exec( hDB, "PRAGMA locking_mode = EXCLUSIVE", NULL, NULL,
                       &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to set PRAGMA locking_mode = EXCLUSIVE: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
    }
    
    /* setting the SQLite cache */
    if( iSqliteCacheMB > 0 )
    {
        int cache_size = iSqliteCacheMB * 1024;

        /* refusing to allocate more than 1GB */
        if( cache_size > 1024 * 1024 )
            cache_size = 1024 * 1024;
        char sqlPragma[1024];
        sprintf( sqlPragma, "PRAGMA cache_size = %d", cache_size );
        rc = sqlite3_exec( hDB, sqlPragma, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to set %s: %s", sqlPragma, pszErrMsg );
            sqlite3_free( pszErrMsg );
        }
    }
	
    if( !SetupParser() )
    {
        gmlHugeFileCleanUp ( &helper );
        return FALSE;
    }

    /* creating SQLite tables and Insert cursors */
    if( gmlHugeFileSQLiteInit( &helper ) == FALSE )
    {
        gmlHugeFileCleanUp ( &helper );
        return FALSE;
    }

    /* processing GML features */
    while( (poFeature = NextFeature()) != NULL )
    {
        const CPLXMLNode* const * papsGeomList = poFeature->GetGeometryList();
        if (papsGeomList != NULL)
        {
            int i = 0;
            const CPLXMLNode *psNode = papsGeomList[i];
            while(psNode)
            {
                gmlHugeFileCheckXrefs( &helper, psNode );
                /* inserting into the SQLite DB any appropriate row */
                gmlHugeFileSQLiteInsert ( &helper );
                /* resetting an empty helper struct */
                gmlHugeFileReset ( &helper );
                i ++;
                psNode = papsGeomList[i];
            }
        }
        iFeatureUID++;
        delete poFeature;
    }

    /* finalizing any SQLite Insert cursor */
    if( helper.hNodes != NULL )
        sqlite3_finalize ( helper.hNodes );
    helper.hNodes = NULL;
    if( helper.hEdges != NULL )
        sqlite3_finalize ( helper.hEdges );
    helper.hEdges = NULL;

    /* confirming the still pending TRANSACTION */
    rc = sqlite3_exec( hDB, "COMMIT", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to perform COMMIT TRANSACTION: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

/* attempting to resolve GML strings */
    if( gmlHugeFileResolveEdges( &helper ) == FALSE )
    {
        gmlHugeFileCleanUp ( &helper );
        return FALSE;
    }
	
    /* restarting the GML parser */
    if( !SetupParser() )
    {
        gmlHugeFileCleanUp ( &helper );
        return FALSE;
    }

    /* output: writing the revolved GML file */
    if ( gmlHugeFileWriteResolved( &helper, pszOutputFilename, this, 
                                   &m_bSequentialLayers ) == FALSE)
    {
        gmlHugeFileCleanUp ( &helper );
        return FALSE;
    }

    gmlHugeFileCleanUp ( &helper );
    if ( bSqliteIsTempFile == TRUE )
        VSIUnlink( pszSQLiteFilename );
    return TRUE;
}

/**************************************************************/
/*                                                            */
/* an alternative <xlink:href> resolver based on SQLite       */
/*                                                            */
/**************************************************************/
int GMLReader::HugeFileResolver( const char *pszFile,
                                 int bSqliteIsTempFile,
                                 int iSqliteCacheMB )

{
    // Check if the original source file is set.
    if( m_pszFilename == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
        "GML source file needs to be set first with "
        "GMLReader::SetSourceFile()." );
        return FALSE;
    }
    if ( ParseXMLHugeFile( pszFile, bSqliteIsTempFile, iSqliteCacheMB ) == FALSE )
        return FALSE;

    //set the source file to the resolved file
    CleanupParser();
    if (fpGML)
        VSIFCloseL(fpGML);
    fpGML = NULL;
    CPLFree( m_pszFilename );
    m_pszFilename = CPLStrdup( pszFile );
    return TRUE;
}

#else  // HAVE_SQLITE

/**************************************************/
/*    if SQLite support isn't available we'll     */
/*    simply output an error message              */
/**************************************************/

int GMLReader::HugeFileResolver( const char *pszFile,
                                 int bSqliteIsTempFile,
                                 int iSqliteCacheMB )

{
    CPLError( CE_Failure, CPLE_NotSupported,
              "OGR was built without SQLite3 support\n"
              "... sorry, the HUGE GML resolver is unsupported\n" );
    return FALSE;
}

int GMLReader::ParseXMLHugeFile( const char *pszOutputFilename,
                                 const int bSqliteIsTempFile,
                                 const int iSqliteCacheMB )
{
    CPLError( CE_Failure, CPLE_NotSupported,
              "OGR was built without SQLite3 support\n"
              "... sorry, the HUGE GML resolver is unsupported\n" );
    return FALSE;
}

#endif // HAVE_SQLITE
