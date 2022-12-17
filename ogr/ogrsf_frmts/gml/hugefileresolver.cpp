/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReader::HugeFileResolver() method.
 * Author:   Alessandro Furieri, a.furitier@lqt.it
 *
 ******************************************************************************
 * Copyright (c) 2011, Alessandro Furieri
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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
 * This module implements GML_SKIP_RESOLVE_ELEMS HUGE
 * Developed for Faunalia ( http://www.faunalia.it) with funding from
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ****************************************************************************/

#include "cpl_port.h"
#include "gmlreader.h"
#include "gmlreaderp.h"

#include <algorithm>
#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_http.h"
#include "cpl_string.h"
#include "gmlutils.h"
#include "ogr_p.h"

#ifdef HAVE_SQLITE
#include <sqlite3.h>

#undef SQLITE_STATIC
#define SQLITE_STATIC ((sqlite3_destructor_type) nullptr)

#endif

/****************************************************/
/*      SQLite is absolutely required in order to   */
/*      support the HUGE xlink:href resolver        */
/****************************************************/

#ifdef HAVE_SQLITE

// Internal helper struct supporting GML tags <Edge>.
struct huge_tag
{
    CPLString *gmlTagValue;
    CPLString *gmlId;
    CPLString *gmlNodeFrom;
    CPLString *gmlNodeTo;
    bool bIsNodeFromHref;
    bool bIsNodeToHref;
    bool bHasCoords;
    bool bHasZ;
    double xNodeFrom;
    double yNodeFrom;
    double zNodeFrom;
    double xNodeTo;
    double yNodeTo;
    double zNodeTo;
    struct huge_tag *pNext;
};

// Internal helper struct supporting GML tags xlink:href.
struct huge_href
{
    CPLString *gmlId;
    CPLString *gmlText;
    const CPLXMLNode *psParent;
    const CPLXMLNode *psNode;
    // bool                bIsDirectedEdge;
    char cOrientation;
    struct huge_href *pNext;
};

// Internal struct supporting GML rewriting.
struct huge_child
{
    CPLXMLNode *psChild;
    struct huge_href *pItem;
    struct huge_child *pNext;
};

// Internal struct supporting GML rewriting.
struct huge_parent
{
    CPLXMLNode *psParent;
    struct huge_child *pFirst;
    // cppcheck-suppress unusedStructMember
    struct huge_child *pLast;
    struct huge_parent *pNext;
};

// Internal class supporting GML resolver for Huge Files (based on SQLite).
class huge_helper
{
  public:
    huge_helper()
        : hDB(nullptr), hNodes(nullptr), hEdges(nullptr), nodeSrs(nullptr),
          pFirst(nullptr), pLast(nullptr), pFirstHref(nullptr),
          pLastHref(nullptr), pFirstParent(nullptr), pLastParent(nullptr)
    {
    }
    sqlite3 *hDB;
    sqlite3_stmt *hNodes;
    sqlite3_stmt *hEdges;
    CPLString *nodeSrs;
    struct huge_tag *pFirst;
    struct huge_tag *pLast;
    struct huge_href *pFirstHref;
    struct huge_href *pLastHref;
    struct huge_parent *pFirstParent;
    struct huge_parent *pLastParent;
};

static bool gmlHugeFileSQLiteInit(huge_helper *helper)
{
    // Attempting to create SQLite tables.
    char *pszErrMsg = nullptr;
    sqlite3 *hDB = helper->hDB;

    // DB table: NODES.
    {
        const char osCommand[] = "CREATE TABLE nodes ("
                                 "     gml_id VARCHAR PRIMARY KEY, "
                                 "     x DOUBLE, "
                                 "     y DOUBLE, "
                                 "     z DOUBLE)";
        const int rc =
            sqlite3_exec(hDB, osCommand, nullptr, nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create table nodes: %s", pszErrMsg);
            sqlite3_free(pszErrMsg);
            return false;
        }
    }

    // DB table: GML_EDGES.
    {
        const char osCommand[] = "CREATE TABLE gml_edges ("
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
        const int rc =
            sqlite3_exec(hDB, osCommand, nullptr, nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create table gml_edges: %s", pszErrMsg);
            sqlite3_free(pszErrMsg);
            return false;
        }
    }

    // DB table: NODES / Insert cursor.
    {
        const char osCommand[] =
            "INSERT OR IGNORE INTO nodes (gml_id, x, y, z) VALUES (?, ?, ?, ?)";
        sqlite3_stmt *hStmt = nullptr;
        const int rc = sqlite3_prepare_v2(hDB, osCommand, -1, &hStmt, nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create INSERT stmt for: nodes");
            return false;
        }
        helper->hNodes = hStmt;
    }

    // DB table: GML_EDGES / Insert cursor.
    {
        const char osCommand[] = "INSERT INTO gml_edges "
                                 "(gml_id, gml_string, gml_resolved, "
                                 "node_from_id, node_from_x, node_from_y, "
                                 "node_from_z, node_to_id, node_to_x, "
                                 "node_to_y, node_to_z) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        sqlite3_stmt *hStmt = nullptr;
        const int rc = sqlite3_prepare_v2(hDB, osCommand, -1, &hStmt, nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create INSERT stmt for: gml_edges");
            return false;
        }
        helper->hEdges = hStmt;
    }

    // Starting a TRANSACTION.
    const int rc = sqlite3_exec(hDB, "BEGIN", nullptr, nullptr, &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to perform BEGIN TRANSACTION: %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return false;
    }

    return true;
}

static bool gmlHugeResolveEdgeNodes(const CPLXMLNode *psNode,
                                    const char *pszFromId, const char *pszToId)
{
    if (psNode->eType != CXT_Element || !EQUAL(psNode->pszValue, "Edge"))
    {
        return false;
    }

    // Resolves an Edge definition.
    CPLXMLNode *psDirNode_1 = nullptr;
    CPLXMLNode *psDirNode_2 = nullptr;
    CPLXMLNode *psOldNode_1 = nullptr;
    CPLXMLNode *psOldNode_2 = nullptr;
    CPLXMLNode *psNewNode_1 = nullptr;
    CPLXMLNode *psNewNode_2 = nullptr;
    int iToBeReplaced = 0;
    int iReplaced = 0;

    CPLXMLNode *psChild = psNode->psChild;
    while (psChild != nullptr)
    {
        if (psChild->eType == CXT_Element &&
            EQUAL(psChild->pszValue, "directedNode"))
        {
            char cOrientation = '+';
            CPLXMLNode *psOldNode = nullptr;
            CPLXMLNode *psAttr = psChild->psChild;
            while (psAttr != nullptr)
            {
                if (psAttr->eType == CXT_Attribute &&
                    EQUAL(psAttr->pszValue, "xlink:href"))
                    psOldNode = psAttr;
                if (psAttr->eType == CXT_Attribute &&
                    EQUAL(psAttr->pszValue, "orientation"))
                {
                    const CPLXMLNode *psOrientation = psAttr->psChild;
                    if (psOrientation != nullptr)
                    {
                        if (psOrientation->eType == CXT_Text)
                            cOrientation = *(psOrientation->pszValue);
                    }
                }
                psAttr = psAttr->psNext;
            }
            if (psOldNode != nullptr)
            {
                CPLXMLNode *psNewNode =
                    CPLCreateXMLNode(nullptr, CXT_Element, "Node");
                CPLXMLNode *psGMLIdNode =
                    CPLCreateXMLNode(psNewNode, CXT_Attribute, "gml:id");
                if (cOrientation == '-')
                    CPLCreateXMLNode(psGMLIdNode, CXT_Text, pszFromId);
                else
                    CPLCreateXMLNode(psGMLIdNode, CXT_Text, pszToId);
                if (iToBeReplaced == 0)
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

    // Rewriting the Edge GML definition.
    if (psDirNode_1 != nullptr)
    {
        if (psOldNode_1 != nullptr)
        {
            CPLRemoveXMLChild(psDirNode_1, psOldNode_1);
            CPLDestroyXMLNode(psOldNode_1);
            if (psNewNode_1 != nullptr)
            {
                CPLAddXMLChild(psDirNode_1, psNewNode_1);
                iReplaced++;
            }
        }
    }
    if (psDirNode_2 != nullptr)
    {
        if (psOldNode_2 != nullptr)
        {
            CPLRemoveXMLChild(psDirNode_2, psOldNode_2);
            CPLDestroyXMLNode(psOldNode_2);
            if (psNewNode_2 != nullptr)
            {
                CPLAddXMLChild(psDirNode_2, psNewNode_2);
                iReplaced++;
            }
        }
    }

    return iToBeReplaced == iReplaced;
}

static bool gmlHugeFileResolveEdges(huge_helper *helper)
{
    // Identifying any not yet resolved <Edge> GML string.
    sqlite3 *hDB = helper->hDB;

    // Query cursor.
    sqlite3_stmt *hQueryStmt = nullptr;
    {
        const char osCommand[] =
            "SELECT e.gml_id, e.gml_string, e.node_from_id, "
            "e.node_from_x, e.node_from_y, e.node_from_z, "
            "n1.gml_id, n1.x, n1.y, n1.z, e.node_to_id, "
            "e.node_to_x, e.node_to_y, e.node_to_z, "
            "n2.gml_id, n2.x, n2.y, n2.z "
            "FROM gml_edges AS e "
            "LEFT JOIN nodes AS n1 ON (n1.gml_id = e.node_from_id) "
            "LEFT JOIN nodes AS n2 ON (n2.gml_id = e.node_to_id)";
        const int rc =
            sqlite3_prepare_v2(hDB, osCommand, -1, &hQueryStmt, nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create QUERY stmt for Edge resolver");
            return false;
        }
    }

    // Update cursor.
    sqlite3_stmt *hUpdateStmt = nullptr;
    {
        const char osCommand[] = "UPDATE gml_edges "
                                 "SET gml_resolved = ?, "
                                 "gml_string = NULL "
                                 "WHERE gml_id = ?";
        const int rc =
            sqlite3_prepare_v2(hDB, osCommand, -1, &hUpdateStmt, nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create UPDATE stmt for resolved Edges");
            sqlite3_finalize(hQueryStmt);
            return false;
        }
    }

    // Starting a TRANSACTION.
    char *pszErrMsg = nullptr;
    {
        const int rc = sqlite3_exec(hDB, "BEGIN", nullptr, nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to perform BEGIN TRANSACTION: %s", pszErrMsg);
            sqlite3_free(pszErrMsg);
            sqlite3_finalize(hQueryStmt);
            sqlite3_finalize(hUpdateStmt);
            return false;
        }
    }

    int iCount = 0;
    bool bError = false;

    // Looping on the QUERY result-set.
    while (true)
    {
        const char *pszGmlId = nullptr;
        const char *pszGmlString = nullptr;
        const char *pszFromId = nullptr;
        double xFrom = std::numeric_limits<double>::quiet_NaN();
        double yFrom = std::numeric_limits<double>::quiet_NaN();
        double zFrom = std::numeric_limits<double>::quiet_NaN();
        const char *pszNodeFromId = nullptr;
        double xNodeFrom = std::numeric_limits<double>::quiet_NaN();
        double yNodeFrom = std::numeric_limits<double>::quiet_NaN();
        double zNodeFrom = std::numeric_limits<double>::quiet_NaN();
        const char *pszToId = nullptr;
        double xTo = std::numeric_limits<double>::quiet_NaN();
        double yTo = std::numeric_limits<double>::quiet_NaN();
        double zTo = std::numeric_limits<double>::quiet_NaN();
        const char *pszNodeToId = nullptr;
        double xNodeTo = std::numeric_limits<double>::quiet_NaN();
        double yNodeTo = std::numeric_limits<double>::quiet_NaN();
        double zNodeTo = std::numeric_limits<double>::quiet_NaN();

        const int rc2 = sqlite3_step(hQueryStmt);
        if (rc2 == SQLITE_DONE)
            break;

        if (rc2 == SQLITE_ROW)
        {
            bError = false;
            pszGmlId = reinterpret_cast<const char *>(
                sqlite3_column_text(hQueryStmt, 0));
            if (sqlite3_column_type(hQueryStmt, 1) != SQLITE_NULL)
            {
                pszGmlString = static_cast<const char *>(
                    sqlite3_column_blob(hQueryStmt, 1));
            }
            if (sqlite3_column_type(hQueryStmt, 2) != SQLITE_NULL)
            {
                pszFromId = reinterpret_cast<const char *>(
                    sqlite3_column_text(hQueryStmt, 2));
            }
            if (sqlite3_column_type(hQueryStmt, 3) != SQLITE_NULL)
            {
                xFrom = sqlite3_column_double(hQueryStmt, 3);
            }
            if (sqlite3_column_type(hQueryStmt, 4) != SQLITE_NULL)
            {
                yFrom = sqlite3_column_double(hQueryStmt, 4);
            }
            if (sqlite3_column_type(hQueryStmt, 5) != SQLITE_NULL)
            {
                zFrom = sqlite3_column_double(hQueryStmt, 5);
            }
            if (sqlite3_column_type(hQueryStmt, 6) != SQLITE_NULL)
            {
                pszNodeFromId = reinterpret_cast<const char *>(
                    sqlite3_column_text(hQueryStmt, 6));
            }
            if (sqlite3_column_type(hQueryStmt, 7) != SQLITE_NULL)
            {
                xNodeFrom = sqlite3_column_double(hQueryStmt, 7);
            }
            if (sqlite3_column_type(hQueryStmt, 8) != SQLITE_NULL)
            {
                yNodeFrom = sqlite3_column_double(hQueryStmt, 8);
            }
            if (sqlite3_column_type(hQueryStmt, 9) != SQLITE_NULL)
            {
                zNodeFrom = sqlite3_column_double(hQueryStmt, 9);
            }
            if (sqlite3_column_type(hQueryStmt, 10) != SQLITE_NULL)
            {
                pszToId = reinterpret_cast<const char *>(
                    sqlite3_column_text(hQueryStmt, 10));
            }
            if (sqlite3_column_type(hQueryStmt, 11) != SQLITE_NULL)
            {
                xTo = sqlite3_column_double(hQueryStmt, 11);
            }
            if (sqlite3_column_type(hQueryStmt, 12) != SQLITE_NULL)
            {
                yTo = sqlite3_column_double(hQueryStmt, 12);
            }
            if (sqlite3_column_type(hQueryStmt, 13) != SQLITE_NULL)
            {
                zTo = sqlite3_column_double(hQueryStmt, 13);
            }
            if (sqlite3_column_type(hQueryStmt, 14) != SQLITE_NULL)
            {
                pszNodeToId = reinterpret_cast<const char *>(
                    sqlite3_column_text(hQueryStmt, 14));
            }
            if (sqlite3_column_type(hQueryStmt, 15) != SQLITE_NULL)
            {
                xNodeTo = sqlite3_column_double(hQueryStmt, 15);
            }
            if (sqlite3_column_type(hQueryStmt, 16) != SQLITE_NULL)
            {
                yNodeTo = sqlite3_column_double(hQueryStmt, 16);
            }
            if (sqlite3_column_type(hQueryStmt, 17) != SQLITE_NULL)
            {
                zNodeTo = sqlite3_column_double(hQueryStmt, 17);
            }

            // Checking for consistency.
            if (pszFromId == nullptr || std::isnan(xFrom) || std::isnan(yFrom))
            {
                bError = true;
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Edge gml:id=\"%s\": invalid Node-from", pszGmlId);
            }
            else
            {
                if (pszNodeFromId == nullptr)
                {
                    bError = true;
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Edge gml:id=\"%s\": undeclared Node gml:id=\"%s\"",
                        pszGmlId, pszFromId);
                }
                else if (std::isnan(xNodeFrom) || std::isnan(yNodeFrom))
                {
                    bError = true;
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Edge gml:id=\"%s\": "
                             "unknown coords for Node gml:id=\"%s\"",
                             pszGmlId, pszFromId);
                }
                else if (xFrom != xNodeFrom || yFrom != yNodeFrom)
                {
                    bError = true;
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Edge gml:id=\"%s\": mismatching coords for Node "
                             "gml:id=\"%s\"",
                             pszGmlId, pszFromId);
                }
                else
                {
                    if (std::isnan(zFrom) && std::isnan(zNodeFrom))
                    {
                        ;
                    }
                    else if (std::isnan(zFrom) || std::isnan(zNodeFrom))
                    {
                        bError = true;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Edge gml:id=\"%s\": mismatching 2D/3D for "
                                 "Node gml:id=\"%s\"",
                                 pszGmlId, pszFromId);
                    }
                    else if (zFrom != zNodeFrom)
                    {
                        bError = true;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Edge gml:id=\"%s\": mismatching Z coord for "
                                 "Node gml:id=\"%s\"",
                                 pszGmlId, pszFromId);
                    }
                }
            }
            if (pszToId == nullptr || std::isnan(xTo) || std::isnan(yTo))
            {
                bError = true;
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Edge gml:id=\"%s\": invalid Node-to", pszGmlId);
            }
            else
            {
                if (pszNodeToId == nullptr)
                {
                    bError = true;
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Edge gml:id=\"%s\": undeclared Node gml:id=\"%s\"",
                        pszGmlId, pszToId);
                }
                else if (std::isnan(xNodeTo) || std::isnan(yNodeTo))
                {
                    bError = true;
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Edge gml:id=\"%s\": "
                             "unknown coords for Node gml:id=\"%s\"",
                             pszGmlId, pszToId);
                }
                else if (xTo != xNodeTo || yTo != yNodeTo)
                {
                    bError = true;
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Edge gml:id=\"%s\": mismatching coords for Node "
                             "gml:id=\"%s\"",
                             pszGmlId, pszToId);
                }
                else
                {
                    if (std::isnan(zTo) && std::isnan(zNodeTo))
                    {
                        ;
                    }
                    else if (std::isnan(zTo) || std::isnan(zNodeTo))
                    {
                        bError = true;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Edge gml:id=\"%s\": mismatching 2D/3D for "
                                 "Node gml:id=\"%s\"",
                                 pszGmlId, pszToId);
                    }
                    else if (zTo != zNodeTo)
                    {
                        bError = true;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Edge gml:id=\"%s\": mismatching Z coord for "
                                 "Node gml:id=\"%s\"",
                                 pszGmlId, pszToId);
                    }
                }
            }

            // Updating the resolved Node.
            if (bError == false && pszGmlString != nullptr &&
                pszFromId != nullptr && pszToId != nullptr)
            {
                // coverity[tainted_data]
                CPLXMLNode *psNode = CPLParseXMLString(pszGmlString);
                if (psNode != nullptr)
                {
                    if (gmlHugeResolveEdgeNodes(psNode, pszFromId, pszToId))
                    {
                        char *gmlText = CPLSerializeXMLTree(psNode);
                        sqlite3_reset(hUpdateStmt);
                        sqlite3_clear_bindings(hUpdateStmt);
                        sqlite3_bind_blob(hUpdateStmt, 1, gmlText,
                                          static_cast<int>(strlen(gmlText)),
                                          SQLITE_STATIC);
                        sqlite3_bind_text(hUpdateStmt, 2, pszGmlId, -1,
                                          SQLITE_STATIC);
                        {
                            const int rc = sqlite3_step(hUpdateStmt);
                            if (rc != SQLITE_OK && rc != SQLITE_DONE)
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "UPDATE resolved Edge \"%s\" "
                                         "sqlite3_step() failed:\n  %s",
                                         pszGmlId, sqlite3_errmsg(hDB));
                            }
                        }
                        CPLFree(gmlText);
                        iCount++;
                        if ((iCount % 1024) == 1023)
                        {
                            // Committing the current TRANSACTION.
                            const int rc3 = sqlite3_exec(hDB, "COMMIT", nullptr,
                                                         nullptr, &pszErrMsg);
                            if (rc3 != SQLITE_OK)
                            {
                                CPLError(
                                    CE_Failure, CPLE_AppDefined,
                                    "Unable to perform COMMIT TRANSACTION: %s",
                                    pszErrMsg);
                                sqlite3_free(pszErrMsg);
                                return false;
                            }
                            // Restarting a new TRANSACTION.
                            const int rc4 = sqlite3_exec(hDB, "BEGIN", nullptr,
                                                         nullptr, &pszErrMsg);
                            if (rc4 != SQLITE_OK)
                            {
                                CPLError(
                                    CE_Failure, CPLE_AppDefined,
                                    "Unable to perform BEGIN TRANSACTION: %s",
                                    pszErrMsg);
                                sqlite3_free(pszErrMsg);
                                sqlite3_finalize(hQueryStmt);
                                sqlite3_finalize(hUpdateStmt);
                                return false;
                            }
                        }
                    }
                    CPLDestroyXMLNode(psNode);
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Edge resolver QUERY: sqlite3_step(%s)",
                     sqlite3_errmsg(hDB));
            sqlite3_finalize(hQueryStmt);
            sqlite3_finalize(hUpdateStmt);
            return false;
        }
    }
    sqlite3_finalize(hQueryStmt);
    sqlite3_finalize(hUpdateStmt);

    // Committing the current TRANSACTION.
    const int rc = sqlite3_exec(hDB, "COMMIT", nullptr, nullptr, &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to perform COMMIT TRANSACTION: %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return false;
    }

    return !bError;
}

static bool gmlHugeFileSQLiteInsert(huge_helper *helper)
{
    // Inserting any appropriate row into the SQLite DB.

    // Looping on GML tags.
    struct huge_tag *pItem = helper->pFirst;
    while (pItem != nullptr)
    {
        if (pItem->bHasCoords)
        {
            if (pItem->gmlNodeFrom != nullptr)
            {
                sqlite3_reset(helper->hNodes);
                sqlite3_clear_bindings(helper->hNodes);
                sqlite3_bind_text(helper->hNodes, 1,
                                  pItem->gmlNodeFrom->c_str(), -1,
                                  SQLITE_STATIC);
                sqlite3_bind_double(helper->hNodes, 2, pItem->xNodeFrom);
                sqlite3_bind_double(helper->hNodes, 3, pItem->yNodeFrom);
                if (pItem->bHasZ)
                    sqlite3_bind_double(helper->hNodes, 4, pItem->zNodeFrom);
                sqlite3_bind_null(helper->hNodes, 5);
                const int rc = sqlite3_step(helper->hNodes);
                if (rc != SQLITE_OK && rc != SQLITE_DONE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "sqlite3_step() failed:\n  %s (gmlNodeFrom id=%s)",
                             sqlite3_errmsg(helper->hDB),
                             pItem->gmlNodeFrom->c_str());
                    return false;
                }
            }
            if (pItem->gmlNodeTo != nullptr)
            {
                sqlite3_reset(helper->hNodes);
                sqlite3_clear_bindings(helper->hNodes);
                sqlite3_bind_text(helper->hNodes, 1, pItem->gmlNodeTo->c_str(),
                                  -1, SQLITE_STATIC);
                sqlite3_bind_double(helper->hNodes, 2, pItem->xNodeTo);
                sqlite3_bind_double(helper->hNodes, 3, pItem->yNodeTo);
                if (pItem->bHasZ)
                    sqlite3_bind_double(helper->hNodes, 4, pItem->zNodeTo);
                sqlite3_bind_null(helper->hNodes, 5);
                const int rc = sqlite3_step(helper->hNodes);
                if (rc != SQLITE_OK && rc != SQLITE_DONE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "sqlite3_step() failed:\n  %s (gmlNodeTo id=%s)",
                             sqlite3_errmsg(helper->hDB),
                             pItem->gmlNodeTo->c_str());
                    return false;
                }
            }
        }

        // gml:id
        sqlite3_reset(helper->hEdges);
        sqlite3_clear_bindings(helper->hEdges);
        sqlite3_bind_text(helper->hEdges, 1, pItem->gmlId->c_str(), -1,
                          SQLITE_STATIC);
        if (pItem->bIsNodeFromHref == false && pItem->bIsNodeToHref == false)
        {
            sqlite3_bind_null(helper->hEdges, 2);
            sqlite3_bind_blob(
                helper->hEdges, 3, pItem->gmlTagValue->c_str(),
                static_cast<int>(strlen(pItem->gmlTagValue->c_str())),
                SQLITE_STATIC);
        }
        else
        {
            sqlite3_bind_blob(
                helper->hEdges, 2, pItem->gmlTagValue->c_str(),
                static_cast<int>(strlen(pItem->gmlTagValue->c_str())),
                SQLITE_STATIC);
            sqlite3_bind_null(helper->hEdges, 3);
        }
        if (pItem->gmlNodeFrom != nullptr)
            sqlite3_bind_text(helper->hEdges, 4, pItem->gmlNodeFrom->c_str(),
                              -1, SQLITE_STATIC);
        else
            sqlite3_bind_null(helper->hEdges, 4);
        if (pItem->bHasCoords)
        {
            sqlite3_bind_double(helper->hEdges, 5, pItem->xNodeFrom);
            sqlite3_bind_double(helper->hEdges, 6, pItem->yNodeFrom);
            if (pItem->bHasZ)
                sqlite3_bind_double(helper->hEdges, 7, pItem->zNodeFrom);
            else
                sqlite3_bind_null(helper->hEdges, 7);
        }
        else
        {
            sqlite3_bind_null(helper->hEdges, 5);
            sqlite3_bind_null(helper->hEdges, 6);
            sqlite3_bind_null(helper->hEdges, 7);
        }
        if (pItem->gmlNodeTo != nullptr)
            sqlite3_bind_text(helper->hEdges, 8, pItem->gmlNodeTo->c_str(), -1,
                              SQLITE_STATIC);
        else
            sqlite3_bind_null(helper->hEdges, 8);
        if (pItem->bHasCoords)
        {
            sqlite3_bind_double(helper->hEdges, 9, pItem->xNodeTo);
            sqlite3_bind_double(helper->hEdges, 10, pItem->yNodeTo);
            if (pItem->bHasZ)
                sqlite3_bind_double(helper->hEdges, 11, pItem->zNodeTo);
            else
                sqlite3_bind_null(helper->hEdges, 11);
        }
        else
        {
            sqlite3_bind_null(helper->hEdges, 9);
            sqlite3_bind_null(helper->hEdges, 10);
            sqlite3_bind_null(helper->hEdges, 11);
        }
        const int rc = sqlite3_step(helper->hEdges);
        if (rc != SQLITE_OK && rc != SQLITE_DONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "sqlite3_step() failed:\n  %s (edge gml:id=%s)",
                     sqlite3_errmsg(helper->hDB), pItem->gmlId->c_str());
            return false;
        }
        pItem = pItem->pNext;
    }
    return true;
}

static void gmlHugeFileReset(huge_helper *helper)
{
    // Resetting an empty helper struct.
    struct huge_tag *p = helper->pFirst;

    // Cleaning any previous item.
    while (p != nullptr)
    {
        struct huge_tag *pNext = p->pNext;
        if (p->gmlTagValue != nullptr)
            delete p->gmlTagValue;
        if (p->gmlId != nullptr)
            delete p->gmlId;
        if (p->gmlNodeFrom != nullptr)
            delete p->gmlNodeFrom;
        if (p->gmlNodeTo != nullptr)
            delete p->gmlNodeTo;
        delete p;
        p = pNext;
    }
    helper->pFirst = nullptr;
    helper->pLast = nullptr;
}

static void gmlHugeFileHrefReset(huge_helper *helper)
{
    // Resetting an empty helper struct.
    struct huge_href *p = helper->pFirstHref;

    // Cleaning any previous item.
    while (p != nullptr)
    {
        struct huge_href *pNext = p->pNext;
        if (p->gmlId != nullptr)
            delete p->gmlId;
        if (p->gmlText != nullptr)
            delete p->gmlText;
        delete p;
        p = pNext;
    }
    helper->pFirstHref = nullptr;
    helper->pLastHref = nullptr;
}

static bool gmlHugeFileHrefCheck(huge_helper *helper)
{
    // Testing for unresolved items.
    bool bError = false;
    struct huge_href *p = helper->pFirstHref;
    while (p != nullptr)
    {
        if (p->gmlText == nullptr)
        {
            bError = true;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Edge xlink:href\"%s\": unresolved match",
                     p->gmlId->c_str());
        }
        p = p->pNext;
    }

    return !bError;
}

static void gmlHugeFileRewiterReset(huge_helper *helper)
{
    // Resetting an empty helper struct.
    struct huge_parent *p = helper->pFirstParent;

    // Cleaning any previous item.
    while (p != nullptr)
    {
        struct huge_parent *pNext = p->pNext;
        struct huge_child *pChild = p->pFirst;
        while (pChild != nullptr)
        {
            struct huge_child *pChildNext = pChild->pNext;
            delete pChild;
            pChild = pChildNext;
        }
        delete p;
        p = pNext;
    }
    helper->pFirstParent = nullptr;
    helper->pLastParent = nullptr;
}

static struct huge_tag *gmlHugeAddToHelper(huge_helper *helper,
                                           CPLString *gmlId,
                                           CPLString *gmlFragment)
{
    // Adding an item into the linked list.

    // Checking against duplicates.
    struct huge_tag *pItem = helper->pFirst;
    while (pItem != nullptr)
    {
        if (EQUAL(pItem->gmlId->c_str(), gmlId->c_str()))
            return nullptr;
        pItem = pItem->pNext;
    }

    pItem = new struct huge_tag;
    pItem->gmlId = gmlId;
    pItem->gmlTagValue = gmlFragment;
    pItem->gmlNodeFrom = nullptr;
    pItem->gmlNodeTo = nullptr;
    pItem->bIsNodeFromHref = false;
    pItem->bIsNodeToHref = false;
    pItem->bHasCoords = false;
    pItem->bHasZ = false;
    pItem->pNext = nullptr;

    // Appending the item to the linked list.
    if (helper->pFirst == nullptr)
        helper->pFirst = pItem;
    if (helper->pLast != nullptr)
        helper->pLast->pNext = pItem;
    helper->pLast = pItem;
    return pItem;
}

static void gmlHugeAddPendingToHelper(huge_helper *helper, CPLString *gmlId,
                                      const CPLXMLNode *psParent,
                                      const CPLXMLNode *psNode,
                                      // bool bIsDirectedEdge,
                                      char cOrientation)
{
    // Inserting an item into the linked list.

    // Checking against duplicates.
    struct huge_href *pItem = helper->pFirstHref;
    while (pItem != nullptr)
    {
        if( EQUAL(pItem->gmlId->c_str(), gmlId->c_str()) &&
            pItem->psParent == psParent  &&
            pItem->psNode == psNode &&
            pItem->cOrientation == cOrientation /* &&
            pItem->bIsDirectedEdge == bIsDirectedEdge */ )
        {
            delete gmlId;
            return;
        }
        pItem = pItem->pNext;
    }

    pItem = new struct huge_href;
    pItem->gmlId = gmlId;
    pItem->gmlText = nullptr;
    pItem->psParent = psParent;
    pItem->psNode = psNode;
    // pItem->bIsDirectedEdge = bIsDirectedEdge;
    pItem->cOrientation = cOrientation;
    pItem->pNext = nullptr;

    // Appending the item to the linked list.
    if (helper->pFirstHref == nullptr)
        helper->pFirstHref = pItem;
    if (helper->pLastHref != nullptr)
        helper->pLastHref->pNext = pItem;
    helper->pLastHref = pItem;
}

static int gmlHugeFindGmlId(const CPLXMLNode *psNode, CPLString **gmlId)
{
    // Attempting to identify a gml:id value.
    *gmlId = nullptr;
    const CPLXMLNode *psChild = psNode->psChild;
    while (psChild != nullptr)
    {
        if (psChild->eType == CXT_Attribute &&
            EQUAL(psChild->pszValue, "gml:id"))
        {
            const CPLXMLNode *psIdValue = psChild->psChild;
            if (psIdValue != nullptr)
            {
                if (psIdValue->eType == CXT_Text)
                {
                    *gmlId = new CPLString(psIdValue->pszValue);
                    return true;
                }
            }
        }
        psChild = psChild->psNext;
    }
    return false;
}

static void gmlHugeFileNodeCoords(struct huge_tag *pItem,
                                  const CPLXMLNode *psNode,
                                  CPL_UNUSED CPLString **nodeSrs)
{
    // This function attempts to set coordinates for <Node> items
    // when required (an <Edge> is expected to be processed).

    // Attempting to fetch Node coordinates.
    CPLXMLNode *psTopoCurve =
        CPLCreateXMLNode(nullptr, CXT_Element, "TopoCurve");
    CPLXMLNode *psDirEdge =
        CPLCreateXMLNode(psTopoCurve, CXT_Element, "directedEdge");
    CPLXMLNode *psEdge = CPLCloneXMLTree((CPLXMLNode *)psNode);
    CPLAddXMLChild(psDirEdge, psEdge);
    OGRGeometry *poTopoCurve = GML2OGRGeometry_XMLNode(psTopoCurve, FALSE);
    CPLDestroyXMLNode(psTopoCurve);
    if (poTopoCurve != nullptr)
    {
        OGRGeometryCollection *poColl = poTopoCurve->toGeometryCollection();
        const int iCount = poColl->getNumGeometries();
        if (iCount == 1)
        {
            OGRGeometry *poChild = poColl->getGeometryRef(0);
            int type = wkbFlatten(poChild->getGeometryType());
            if (type == wkbLineString)
            {
                OGRLineString *poLine = poChild->toLineString();
                const int iPoints = poLine->getNumPoints();
                if (iPoints >= 2)
                {
                    pItem->bHasCoords = true;
                    pItem->xNodeFrom = poLine->getX(0);
                    pItem->yNodeFrom = poLine->getY(0);
                    pItem->xNodeTo = poLine->getX(iPoints - 1);
                    pItem->yNodeTo = poLine->getY(iPoints - 1);
                    if (poLine->getCoordinateDimension() == 3)
                    {
                        pItem->zNodeFrom = poLine->getZ(0);
                        pItem->zNodeTo = poLine->getZ(iPoints - 1);
                        pItem->bHasZ = true;
                    }
                    else
                    {
                        pItem->bHasZ = false;
                    }
                }
            }
        }
        delete poTopoCurve;
    }

    // Searching the <directedNode> sub-tags.
    const CPLXMLNode *psChild = psNode->psChild;
    while (psChild != nullptr)
    {
        if (psChild->eType == CXT_Element &&
            EQUAL(psChild->pszValue, "directedNode"))
        {
            char cOrientation = '+';
            const char *pszGmlId = nullptr;
            bool bIsHref = false;
            const CPLXMLNode *psAttr = psChild->psChild;
            while (psAttr != nullptr)
            {
                if (psAttr->eType == CXT_Attribute &&
                    EQUAL(psAttr->pszValue, "xlink:href"))
                {
                    const CPLXMLNode *psHref = psAttr->psChild;
                    if (psHref != nullptr)
                    {
                        if (psHref->eType == CXT_Text)
                        {
                            pszGmlId = psHref->pszValue;
                            bIsHref = true;
                        }
                    }
                }
                if (psAttr->eType == CXT_Attribute &&
                    EQUAL(psAttr->pszValue, "orientation"))
                {
                    const CPLXMLNode *psOrientation = psAttr->psChild;
                    if (psOrientation != nullptr)
                    {
                        if (psOrientation->eType == CXT_Text)
                        {
                            cOrientation = *(psOrientation->pszValue);
                        }
                    }
                }
                if (psAttr->eType == CXT_Element &&
                    EQUAL(psAttr->pszValue, "Node"))
                {
                    const CPLXMLNode *psId = psAttr->psChild;
                    while (psId != nullptr)
                    {
                        if (psId->eType == CXT_Attribute &&
                            EQUAL(psId->pszValue, "gml:id"))
                        {
                            const CPLXMLNode *psIdGml = psId->psChild;
                            if (psIdGml != nullptr)
                            {
                                if (psIdGml->eType == CXT_Text)
                                {
                                    pszGmlId = psIdGml->pszValue;
                                    bIsHref = false;
                                }
                            }
                        }
                        psId = psId->psNext;
                    }
                }
                psAttr = psAttr->psNext;
            }
            if (pszGmlId != nullptr)
            {
                CPLString *posNode = nullptr;
                if (bIsHref)
                {
                    if (pszGmlId[0] != '#')
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "Only values of xlink:href element starting "
                                 "with '#' are supported, "
                                 "so %s will not be properly recognized",
                                 pszGmlId);
                    }
                    posNode = new CPLString(pszGmlId + 1);
                }
                else
                {
                    posNode = new CPLString(pszGmlId);
                }
                if (cOrientation == '-')
                {
                    pItem->gmlNodeFrom = posNode;
                    pItem->bIsNodeFromHref = bIsHref;
                }
                else
                {
                    pItem->gmlNodeTo = posNode;
                    pItem->bIsNodeToHref = bIsHref;
                }
                // pszGmlId = NULL;
                // *bIsHref = false;
                // cOrientation = '+';
            }
        }
        psChild = psChild->psNext;
    }
}

static void gmlHugeFileCheckXrefs(huge_helper *helper, const CPLXMLNode *psNode)
{
    // Identifying <Edge> GML nodes.
    if (psNode->eType == CXT_Element)
    {
        if (EQUAL(psNode->pszValue, "Edge"))
        {
            CPLString *gmlId = nullptr;
            if (gmlHugeFindGmlId(psNode, &gmlId))
            {
                char *gmlText = CPLSerializeXMLTree(psNode);
                CPLString *gmlValue = new CPLString(gmlText);
                CPLFree(gmlText);
                struct huge_tag *pItem =
                    gmlHugeAddToHelper(helper, gmlId, gmlValue);
                if (pItem != nullptr)
                {
                    gmlHugeFileNodeCoords(pItem, psNode, &(helper->nodeSrs));
                }
                else
                {
                    delete gmlId;
                    delete gmlValue;
                }
            }
        }
    }

    // Recursively scanning each Child GML node.
    const CPLXMLNode *psChild = psNode->psChild;
    while (psChild != nullptr)
    {
        if (psChild->eType == CXT_Element)
        {
            if (EQUAL(psChild->pszValue, "Edge") ||
                EQUAL(psChild->pszValue, "directedEdge"))
            {
                gmlHugeFileCheckXrefs(helper, psChild);
            }
            if (EQUAL(psChild->pszValue, "directedFace"))
            {
                const CPLXMLNode *psFace = psChild->psChild;
                if (psFace != nullptr)
                {
                    if (psFace->eType == CXT_Element &&
                        EQUAL(psFace->pszValue, "Face"))
                    {
                        const CPLXMLNode *psDirEdge = psFace->psChild;
                        while (psDirEdge != nullptr)
                        {
                            const CPLXMLNode *psEdge = psDirEdge->psChild;
                            while (psEdge != nullptr)
                            {
                                if (psEdge->eType == CXT_Element &&
                                    EQUAL(psEdge->pszValue, "Edge"))
                                    gmlHugeFileCheckXrefs(helper, psEdge);
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

    // Recursively scanning each GML of the same level.
    const CPLXMLNode *psNext = psNode->psNext;
    while (psNext != nullptr)
    {
        if (psNext->eType == CXT_Element)
        {
            if (EQUAL(psNext->pszValue, "Edge") ||
                EQUAL(psNext->pszValue, "directedEdge"))
            {
                gmlHugeFileCheckXrefs(helper, psNext);
            }
        }
        psNext = psNext->psNext;
    }
}

static void gmlHugeFileCleanUp(huge_helper *helper)
{
    // Cleaning up any SQLite handle.
    if (helper->hNodes != nullptr)
        sqlite3_finalize(helper->hNodes);
    if (helper->hEdges != nullptr)
        sqlite3_finalize(helper->hEdges);
    if (helper->hDB != nullptr)
        sqlite3_close(helper->hDB);
    if (helper->nodeSrs != nullptr)
        delete helper->nodeSrs;
}

static void gmlHugeFileCheckPendingHrefs(huge_helper *helper,
                                         const CPLXMLNode *psParent,
                                         const CPLXMLNode *psNode)
{
    // Identifying any xlink:href to be replaced.
    if (psNode->eType == CXT_Element)
    {
        if (EQUAL(psNode->pszValue, "directedEdge"))
        {
            char cOrientation = '+';
            CPLXMLNode *psAttr = psNode->psChild;
            while (psAttr != nullptr)
            {
                if (psAttr->eType == CXT_Attribute &&
                    EQUAL(psAttr->pszValue, "orientation"))
                {
                    const CPLXMLNode *psOrientation = psAttr->psChild;
                    if (psOrientation != nullptr)
                    {
                        if (psOrientation->eType == CXT_Text)
                            cOrientation = *(psOrientation->pszValue);
                    }
                }
                psAttr = psAttr->psNext;
            }
            psAttr = psNode->psChild;
            while (psAttr != nullptr)
            {
                if (psAttr->eType == CXT_Attribute &&
                    EQUAL(psAttr->pszValue, "xlink:href"))
                {
                    const CPLXMLNode *pszHref = psAttr->psChild;
                    if (pszHref != nullptr)
                    {
                        if (pszHref->eType == CXT_Text)
                        {
                            if (pszHref->pszValue[0] != '#')
                            {
                                CPLError(
                                    CE_Warning, CPLE_NotSupported,
                                    "Only values of xlink:href element "
                                    "starting with '#' are supported, "
                                    "so %s will not be properly recognized",
                                    pszHref->pszValue);
                            }
                            CPLString *gmlId =
                                new CPLString(pszHref->pszValue + 1);
                            gmlHugeAddPendingToHelper(
                                helper, gmlId, psParent, psNode,
                                // /*bDirectedEdge=*/ true,
                                cOrientation);
                        }
                    }
                }
                psAttr = psAttr->psNext;
            }
        }
    }

    // Recursively scanning each Child GML node.
    const CPLXMLNode *psChild = psNode->psChild;
    while (psChild != nullptr)
    {
        if (psChild->eType == CXT_Element)
        {
            if (EQUAL(psChild->pszValue, "directedEdge") ||
                EQUAL(psChild->pszValue, "directedFace") ||
                EQUAL(psChild->pszValue, "Face"))
            {
                gmlHugeFileCheckPendingHrefs(helper, psNode, psChild);
            }
        }
        psChild = psChild->psNext;
    }

    // Recursively scanning each GML of the same level.
    const CPLXMLNode *psNext = psNode->psNext;
    while (psNext != nullptr)
    {
        if (psNext->eType == CXT_Element)
        {
            if (EQUAL(psNext->pszValue, "Face"))
            {
                gmlHugeFileCheckPendingHrefs(helper, psParent, psNext);
            }
        }
        psNext = psNext->psNext;
    }
}

static void gmlHugeSetHrefGmlText(huge_helper *helper, const char *pszGmlId,
                                  const char *pszGmlText)
{
    // Setting the GML text for the corresponding gml:id.
    struct huge_href *pItem = helper->pFirstHref;
    while (pItem != nullptr)
    {
        if (EQUAL(pItem->gmlId->c_str(), pszGmlId))
        {
            if (pItem->gmlText != nullptr)
                delete pItem->gmlText;
            pItem->gmlText = new CPLString(pszGmlText);
            return;
        }
        pItem = pItem->pNext;
    }
}

static struct huge_parent *gmlHugeFindParent(huge_helper *helper,
                                             CPLXMLNode *psParent)
{
    // Inserting a GML Node (parent) to be rewritten.
    struct huge_parent *pItem = helper->pFirstParent;

    // Checking if already exists.
    while (pItem != nullptr)
    {
        if (pItem->psParent == psParent)
            return pItem;
        pItem = pItem->pNext;
    }

    // Creating a new Parent Node.
    pItem = new struct huge_parent;
    pItem->psParent = psParent;
    pItem->pFirst = nullptr;
    pItem->pLast = nullptr;
    pItem->pNext = nullptr;
    if (helper->pFirstParent == nullptr)
        helper->pFirstParent = pItem;
    if (helper->pLastParent != nullptr)
        helper->pLastParent->pNext = pItem;
    helper->pLastParent = pItem;

    // Inserting any Child node into the Parent.
    CPLXMLNode *psChild = psParent->psChild;
    while (psChild != nullptr)
    {
        struct huge_child *pChildItem = new struct huge_child;
        pChildItem->psChild = psChild;
        pChildItem->pItem = nullptr;
        pChildItem->pNext = nullptr;
        if (pItem->pFirst == nullptr)
            pItem->pFirst = pChildItem;
        if (pItem->pLast != nullptr)
            pItem->pLast->pNext = pChildItem;
        pItem->pLast = pChildItem;
        psChild = psChild->psNext;
    }
    return pItem;
}

static bool gmlHugeSetChild(struct huge_parent *pParent,
                            struct huge_href *pItem)
{
    // Setting a Child Node to be rewritten.
    struct huge_child *pChild = pParent->pFirst;
    while (pChild != nullptr)
    {
        if (pChild->psChild == pItem->psNode)
        {
            pChild->pItem = pItem;
            return true;
        }
        pChild = pChild->pNext;
    }
    return false;
}

static bool gmlHugeResolveEdges(CPL_UNUSED huge_helper *helper,
                                CPL_UNUSED CPLXMLNode *psNode, sqlite3 *hDB)
{
    // Resolving GML <Edge> xlink:href.
    CPLString osCommand;
    bool bIsComma = false;
    bool bError = false;

    // query cursor [Edges] */
    osCommand = "SELECT gml_id, gml_resolved "
                "FROM gml_edges "
                "WHERE gml_id IN (";
    struct huge_href *pItem = helper->pFirstHref;
    while (pItem != nullptr)
    {
        if (bIsComma)
            osCommand += ", ";
        else
            bIsComma = true;
        osCommand += "'";
        osCommand += pItem->gmlId->c_str();
        osCommand += "'";
        pItem = pItem->pNext;
    }
    osCommand += ")";
    sqlite3_stmt *hStmtEdges = nullptr;
    {
        const int rc = sqlite3_prepare_v2(hDB, osCommand.c_str(), -1,
                                          &hStmtEdges, nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create QUERY stmt for EDGES");
            return false;
        }
    }
    while (true)
    {
        const int rc = sqlite3_step(hStmtEdges);
        if (rc == SQLITE_DONE)
            break;
        if (rc == SQLITE_ROW)
        {
            const char *pszGmlId = reinterpret_cast<const char *>(
                sqlite3_column_text(hStmtEdges, 0));
            if (sqlite3_column_type(hStmtEdges, 1) != SQLITE_NULL)
            {
                const char *pszGmlText = reinterpret_cast<const char *>(
                    sqlite3_column_text(hStmtEdges, 1));
                gmlHugeSetHrefGmlText(helper, pszGmlId, pszGmlText);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Edge xlink:href QUERY: sqlite3_step(%s)",
                     sqlite3_errmsg(hDB));
            bError = true;
            break;
        }
    }
    sqlite3_finalize(hStmtEdges);
    if (bError)
        return false;

    // Identifying any GML node to be rewritten.
    pItem = helper->pFirstHref;
    struct huge_parent *pParent = nullptr;
    while (pItem != nullptr)
    {
        if (pItem->gmlText == nullptr || pItem->psParent == nullptr ||
            pItem->psNode == nullptr)
        {
            bError = true;
            break;
        }
        pParent = gmlHugeFindParent(helper,
                                    const_cast<CPLXMLNode *>(pItem->psParent));
        if (gmlHugeSetChild(pParent, pItem) == false)
        {
            bError = true;
            break;
        }
        pItem = pItem->pNext;
    }

    if (bError == false)
    {
        // Rewriting GML nodes.
        pParent = helper->pFirstParent;
        while (pParent != nullptr)
        {

            // Removing any Child node from the Parent.
            struct huge_child *pChild = pParent->pFirst;
            while (pChild != nullptr)
            {
                CPLRemoveXMLChild(pParent->psParent, pChild->psChild);

                // Destroying any Child Node to be rewritten.
                if (pChild->pItem != nullptr)
                    CPLDestroyXMLNode(pChild->psChild);
                pChild = pChild->pNext;
            }

            // Rewriting the Parent Node.
            pChild = pParent->pFirst;
            while (pChild != nullptr)
            {
                if (pChild->pItem == nullptr)
                {
                    // Reinserting any untouched Child Node.
                    CPLAddXMLChild(pParent->psParent, pChild->psChild);
                }
                else
                {
                    // Rewriting a Child Node.
                    CPLXMLNode *psNewNode =
                        CPLCreateXMLNode(nullptr, CXT_Element, "directedEdge");
                    if (pChild->pItem->cOrientation == '-')
                    {
                        CPLXMLNode *psOrientationNode = CPLCreateXMLNode(
                            psNewNode, CXT_Attribute, "orientation");
                        CPLCreateXMLNode(psOrientationNode, CXT_Text, "-");
                    }
                    CPLXMLNode *psEdge =
                        CPLParseXMLString(pChild->pItem->gmlText->c_str());
                    if (psEdge != nullptr)
                        CPLAddXMLChild(psNewNode, psEdge);
                    CPLAddXMLChild(pParent->psParent, psNewNode);
                }
                pChild = pChild->pNext;
            }
            pParent = pParent->pNext;
        }
    }

    // Resetting the Rewrite Helper to an empty state.
    gmlHugeFileRewiterReset(helper);

    return !bError;
}

static bool gmlHugeFileWriteResolved(huge_helper *helper,
                                     const char *pszOutputFilename,
                                     GMLReader *pReader,
                                     int *m_nHasSequentialLayers)
{
    // Open the resolved GML file for writing.
    VSILFILE *fp = VSIFOpenL(pszOutputFilename, "w");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Failed to open %.500s to write.",
                 pszOutputFilename);
        return false;
    }

    sqlite3 *hDB = helper->hDB;

    // Query cursor [Nodes].
    const char *osCommand = "SELECT gml_id, x, y, z FROM nodes";
    sqlite3_stmt *hStmtNodes = nullptr;
    const int rc1 =
        sqlite3_prepare_v2(hDB, osCommand, -1, &hStmtNodes, nullptr);
    if (rc1 != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create QUERY stmt for NODES");
        VSIFCloseL(fp);
        return false;
    }

    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    VSIFPrintfL(fp, "<ResolvedTopoFeatureCollection  "
                    "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
                    "xmlns:gml=\"http://www.opengis.net/gml\">\n");
    VSIFPrintfL(fp, "  <ResolvedTopoFeatureMembers>\n");

    int iOutCount = 0;

    // Exporting Nodes.
    GFSTemplateList *pCC = new GFSTemplateList();
    while (true)
    {
        const int rc = sqlite3_step(hStmtNodes);
        if (rc == SQLITE_DONE)
            break;

        if (rc == SQLITE_ROW)
        {
            bool bHasZ = false;
            const char *pszGmlId = reinterpret_cast<const char *>(
                sqlite3_column_text(hStmtNodes, 0));
            const double x = sqlite3_column_double(hStmtNodes, 1);
            const double y = sqlite3_column_double(hStmtNodes, 2);
            double z = 0.0;
            if (sqlite3_column_type(hStmtNodes, 3) == SQLITE_FLOAT)
            {
                z = sqlite3_column_double(hStmtNodes, 3);
                bHasZ = true;
            }

            // Inserting a node into the resolved GML file.
            pCC->Update("ResolvedNodes", true);
            VSIFPrintfL(fp, "    <ResolvedNodes>\n");
            char *pszEscaped = CPLEscapeString(pszGmlId, -1, CPLES_XML);
            VSIFPrintfL(fp, "      <NodeGmlId>%s</NodeGmlId>\n", pszEscaped);
            CPLFree(pszEscaped);
            VSIFPrintfL(fp, "      <ResolvedGeometry> \n");
            if (helper->nodeSrs == nullptr)
            {
                VSIFPrintfL(fp, "        <gml:Point srsDimension=\"%d\">",
                            bHasZ ? 3 : 2);
            }
            else
            {
                pszEscaped =
                    CPLEscapeString(helper->nodeSrs->c_str(), -1, CPLES_XML);
                VSIFPrintfL(fp,
                            "        <gml:Point srsDimension=\"%d\""
                            " srsName=\"%s\">",
                            bHasZ ? 3 : 2, pszEscaped);
                CPLFree(pszEscaped);
            }
            if (bHasZ)
                VSIFPrintfL(fp,
                            "<gml:pos>%1.8f %1.8f %1.8f</gml:pos>"
                            "</gml:Point>\n",
                            x, y, z);
            else
                VSIFPrintfL(fp,
                            "<gml:pos>%1.8f %1.8f</gml:pos>"
                            "</gml:Point>\n",
                            x, y);
            VSIFPrintfL(fp, "      </ResolvedGeometry> \n");
            VSIFPrintfL(fp, "    </ResolvedNodes>\n");
            iOutCount++;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ResolvedNodes QUERY: sqlite3_step(%s)",
                     sqlite3_errmsg(hDB));
            sqlite3_finalize(hStmtNodes);
            delete pCC;
            return false;
        }
    }
    sqlite3_finalize(hStmtNodes);

    // Processing GML features.
    GMLFeature *poFeature = nullptr;
    bool bError = false;

    while ((poFeature = pReader->NextFeature()) != nullptr)
    {
        GMLFeatureClass *poClass = poFeature->GetClass();
        const CPLXMLNode *const *papsGeomList = poFeature->GetGeometryList();
        const int iPropCount = poClass->GetPropertyCount();

        bool b_has_geom = false;
        VSIFPrintfL(fp, "    <%s>\n", poClass->GetElementName());

        for (int iProp = 0; iProp < iPropCount; iProp++)
        {
            GMLPropertyDefn *poPropDefn = poClass->GetProperty(iProp);
            const char *pszPropName = poPropDefn->GetName();
            const GMLProperty *poProp = poFeature->GetProperty(iProp);

            if (poProp != nullptr)
            {
                for (int iSub = 0; iSub < poProp->nSubProperties; iSub++)
                {
                    char *gmlText = CPLEscapeString(
                        poProp->papszSubProperties[iSub], -1, CPLES_XML);
                    VSIFPrintfL(fp, "      <%s>%s</%s>\n", pszPropName, gmlText,
                                pszPropName);
                    CPLFree(gmlText);
                }
            }
        }

        if (papsGeomList != nullptr)
        {
            int i = 0;
            const CPLXMLNode *psNode = papsGeomList[i];
            while (psNode != nullptr)
            {
                char *pszResolved = nullptr;
                bool bNotToBeResolved = false;
                if (psNode->eType != CXT_Element)
                {
                    bNotToBeResolved = true;
                }
                else
                {
                    bNotToBeResolved =
                        !(EQUAL(psNode->pszValue, "TopoCurve") ||
                          EQUAL(psNode->pszValue, "TopoSurface"));
                }
                if (bNotToBeResolved)
                {
                    VSIFPrintfL(fp, "      <ResolvedGeometry> \n");
                    pszResolved = CPLSerializeXMLTree((CPLXMLNode *)psNode);
                    VSIFPrintfL(fp, "        %s\n", pszResolved);
                    CPLFree(pszResolved);
                    VSIFPrintfL(fp, "      </ResolvedGeometry>\n");
                    b_has_geom = true;
                }
                else
                {
                    gmlHugeFileCheckPendingHrefs(helper, psNode, psNode);
                    if (helper->pFirstHref == nullptr)
                    {
                        VSIFPrintfL(fp, "      <ResolvedGeometry> \n");
                        pszResolved = CPLSerializeXMLTree(psNode);
                        VSIFPrintfL(fp, "        %s\n", pszResolved);
                        CPLFree(pszResolved);
                        VSIFPrintfL(fp, "      </ResolvedGeometry>\n");
                        b_has_geom = true;
                    }
                    else
                    {
                        if (gmlHugeResolveEdges(
                                helper, const_cast<CPLXMLNode *>(psNode),
                                hDB) == false)
                            bError = true;
                        if (gmlHugeFileHrefCheck(helper) == false)
                            bError = true;
                        VSIFPrintfL(fp, "      <ResolvedGeometry> \n");
                        pszResolved = CPLSerializeXMLTree(psNode);
                        VSIFPrintfL(fp, "        %s\n", pszResolved);
                        CPLFree(pszResolved);
                        VSIFPrintfL(fp, "      </ResolvedGeometry>\n");
                        b_has_geom = true;
                        gmlHugeFileHrefReset(helper);
                    }
                }
                i++;
                psNode = papsGeomList[i];
            }
        }
        pCC->Update(poClass->GetElementName(), b_has_geom);

        VSIFPrintfL(fp, "    </%s>\n", poClass->GetElementName());

        delete poFeature;
        iOutCount++;
    }

    VSIFPrintfL(fp, "  </ResolvedTopoFeatureMembers>\n");
    VSIFPrintfL(fp, "</ResolvedTopoFeatureCollection>\n");

    VSIFCloseL(fp);

    gmlUpdateFeatureClasses(pCC, pReader, m_nHasSequentialLayers);
    if (*m_nHasSequentialLayers)
        pReader->ReArrangeTemplateClasses(pCC);
    delete pCC;

    return !(bError || iOutCount == 0);
}

/**************************************************************/
/*                                                            */
/* private member(s):                                         */
/* any other function is implemented as "internal" static,    */
/* so to make all the SQLite own stuff nicely "invisible"     */
/*                                                            */
/**************************************************************/

bool GMLReader::ParseXMLHugeFile(const char *pszOutputFilename,
                                 const bool bSqliteIsTempFile,
                                 const int iSqliteCacheMB)

{
    /* -------------------------------------------------------------------- */
    /*      Creating/Opening the SQLite DB file                             */
    /* -------------------------------------------------------------------- */
    const CPLString osSQLiteFilename =
        CPLResetExtension(m_pszFilename, "sqlite");
    const char *pszSQLiteFilename = osSQLiteFilename.c_str();

    VSIStatBufL statBufL;
    if (VSIStatExL(pszSQLiteFilename, &statBufL, VSI_STAT_EXISTS_FLAG) == 0)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "sqlite3_open(%s) failed: DB-file already exists",
                 pszSQLiteFilename);
        return false;
    }

    sqlite3 *hDB = nullptr;
    {
        const int rc = sqlite3_open(pszSQLiteFilename, &hDB);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "sqlite3_open(%s) failed: %s",
                     pszSQLiteFilename, sqlite3_errmsg(hDB));
            return false;
        }
    }

    huge_helper helper;
    helper.hDB = hDB;

    char *pszErrMsg = nullptr;

    // Setting SQLite for max speed; this is intrinsically unsafe.
    // The DB file could be potentially damaged.
    // But, this is a temporary file, so there is no real risk.
    {
        const int rc = sqlite3_exec(hDB, "PRAGMA synchronous = OFF", nullptr,
                                    nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to set PRAGMA synchronous = OFF: %s", pszErrMsg);
            sqlite3_free(pszErrMsg);
        }
    }
    {
        const int rc = sqlite3_exec(hDB, "PRAGMA journal_mode = OFF", nullptr,
                                    nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to set PRAGMA journal_mode = OFF: %s", pszErrMsg);
            sqlite3_free(pszErrMsg);
        }
    }
    {
        const int rc = sqlite3_exec(hDB, "PRAGMA locking_mode = EXCLUSIVE",
                                    nullptr, nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to set PRAGMA locking_mode = EXCLUSIVE: %s",
                     pszErrMsg);
            sqlite3_free(pszErrMsg);
        }
    }

    // Setting the SQLite cache.
    if (iSqliteCacheMB > 0)
    {
        // Refuse to allocate more than 1GB.
        const int cache_size = std::min(iSqliteCacheMB * 1024, 1024 * 1024);

        char sqlPragma[64] = {};
        snprintf(sqlPragma, sizeof(sqlPragma), "PRAGMA cache_size = %d",
                 cache_size);
        const int rc =
            sqlite3_exec(hDB, sqlPragma, nullptr, nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unable to set %s: %s",
                     sqlPragma, pszErrMsg);
            sqlite3_free(pszErrMsg);
        }
    }

    if (!SetupParser())
    {
        gmlHugeFileCleanUp(&helper);
        return false;
    }

    // Creating SQLite tables and Insert cursors.
    if (gmlHugeFileSQLiteInit(&helper) == false)
    {
        gmlHugeFileCleanUp(&helper);
        return false;
    }

    // Processing GML features.
    GMLFeature *poFeature = nullptr;
    while ((poFeature = NextFeature()) != nullptr)
    {
        const CPLXMLNode *const *papsGeomList = poFeature->GetGeometryList();
        if (papsGeomList != nullptr)
        {
            int i = 0;
            const CPLXMLNode *psNode = papsGeomList[i];
            while (psNode)
            {
                gmlHugeFileCheckXrefs(&helper, psNode);
                // Inserting into the SQLite DB any appropriate row.
                gmlHugeFileSQLiteInsert(&helper);
                // Resetting an empty helper struct.
                gmlHugeFileReset(&helper);
                i++;
                psNode = papsGeomList[i];
            }
        }
        delete poFeature;
    }

    // Finalizing any SQLite Insert cursor.
    if (helper.hNodes != nullptr)
        sqlite3_finalize(helper.hNodes);
    helper.hNodes = nullptr;
    if (helper.hEdges != nullptr)
        sqlite3_finalize(helper.hEdges);
    helper.hEdges = nullptr;

    // Confirming the still pending TRANSACTION.
    const int rc = sqlite3_exec(hDB, "COMMIT", nullptr, nullptr, &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to perform COMMIT TRANSACTION: %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return false;
    }

    // Attempting to resolve GML strings.
    if (gmlHugeFileResolveEdges(&helper) == false)
    {
        gmlHugeFileCleanUp(&helper);
        return false;
    }

    // Restarting the GML parser.
    if (!SetupParser())
    {
        gmlHugeFileCleanUp(&helper);
        return false;
    }

    // Output: writing the revolved GML file.
    if (gmlHugeFileWriteResolved(&helper, pszOutputFilename, this,
                                 &m_nHasSequentialLayers) == false)
    {
        gmlHugeFileCleanUp(&helper);
        return false;
    }

    gmlHugeFileCleanUp(&helper);
    if (bSqliteIsTempFile)
        VSIUnlink(pszSQLiteFilename);
    return true;
}

/**************************************************************/
/*                                                            */
/* an alternative <xlink:href> resolver based on SQLite       */
/*                                                            */
/**************************************************************/
bool GMLReader::HugeFileResolver(const char *pszFile, bool bSqliteIsTempFile,
                                 int iSqliteCacheMB)

{
    // Check if the original source file is set.
    if (m_pszFilename == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GML source file needs to be set first with "
                 "GMLReader::SetSourceFile().");
        return false;
    }
    if (ParseXMLHugeFile(pszFile, bSqliteIsTempFile, iSqliteCacheMB) == false)
        return false;

    // Set the source file to the resolved file.
    CleanupParser();
    if (fpGML)
        VSIFCloseL(fpGML);
    fpGML = nullptr;
    CPLFree(m_pszFilename);
    m_pszFilename = CPLStrdup(pszFile);
    return true;
}

#else  // HAVE_SQLITE

/**************************************************/
/*    if SQLite support isn't available we'll     */
/*    simply output an error message              */
/**************************************************/

bool GMLReader::HugeFileResolver(CPL_UNUSED const char *pszFile,
                                 CPL_UNUSED bool bSqliteIsTempFile,
                                 CPL_UNUSED int iSqliteCacheMB)

{
    CPLError(CE_Failure, CPLE_NotSupported,
             "OGR was built without SQLite3 support. "
             "Sorry, the HUGE GML resolver is unsupported.");
    return false;
}

bool GMLReader::ParseXMLHugeFile(CPL_UNUSED const char *pszOutputFilename,
                                 CPL_UNUSED const bool bSqliteIsTempFile,
                                 CPL_UNUSED const int iSqliteCacheMB)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "OGR was built without SQLite3 support. "
             "Sorry, the HUGE GML resolver is unsupported.");
    return false;
}

#endif  // HAVE_SQLITE
