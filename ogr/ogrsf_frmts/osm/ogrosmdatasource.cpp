/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGROSMDataSource class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault
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

#include "ogr_osm.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "swq.h"

#define LIMIT_IDS_PER_REQUEST 200

#define MAX_NODES_PER_WAY 2000

#define IDX_LYR_POINTS           0
#define IDX_LYR_LINES            1
#define IDX_LYR_POLYGONS         2
#define IDX_LYR_MULTILINESTRINGS 3
#define IDX_LYR_MULTIPOLYGONS    4
#define IDX_LYR_OTHER_RELATIONS  5

#define DBL_TO_INT(x)            (int)floor((x) * 1e7 + 0.5)
#define INT_TO_DBL(x)            ((x) / 1e7)

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGROSMDataSource()                            */
/************************************************************************/

OGROSMDataSource::OGROSMDataSource()

{
    nLayers = 0;
    papoLayers = NULL;
    pszName = NULL;
    bInterleavedReading = -1;
    poCurrentLayer = NULL;
    psParser = NULL;
    bHasParsedFirstChunk = FALSE;
    bStopParsing = FALSE;
#ifdef HAVE_SQLITE_VFS
    pMyVFS = NULL;
#endif
    hDB = NULL;
    hInsertNodeStmt = NULL;
    hInsertWayStmt = NULL;
    hSelectNodeBetweenStmt = NULL;
    nNodesInTransaction = 0;
    bInTransaction = FALSE;
    pahSelectNodeStmt = NULL;
    pahSelectWayStmt = NULL;
    panLonLatCache = NULL;
    bInMemoryTmpDB = FALSE;
    bMustUnlink = TRUE;
    nMaxSizeForInMemoryDBInMB = atoi(CPLGetConfigOption("OSM_MAX_TMPFILE_SIZE", "100"));
    bReportAllNodes = FALSE;
    bFeatureAdded = FALSE;
    nNodeSelectBetween = 0;
    nNodeSelectIn = 0;
    /* The following 4 config options are only usefull for debugging */
    bIndexPoints = CSLTestBoolean(CPLGetConfigOption("OSM_INDEX_POINTS", "YES"));
    bUsePointsIndex = CSLTestBoolean(CPLGetConfigOption("OSM_USE_POINTS_INDEX", "YES"));
    bIndexWays = CSLTestBoolean(CPLGetConfigOption("OSM_INDEX_WAYS", "YES"));
    bUseWaysIndex = CSLTestBoolean(CPLGetConfigOption("OSM_USE_WAYS_INDEX", "YES"));

    poResultSetLayer = NULL;
    bIndexPointsBackup = FALSE;
    bUsePointsIndexBackup = FALSE;
    bIndexWaysBackup = FALSE;
    bUseWaysIndexBackup = FALSE;

    bIsFeatureCountEnabled = FALSE;

    bAttributeNameLaundering = TRUE;
}

/************************************************************************/
/*                          ~OGROSMDataSource()                         */
/************************************************************************/

OGROSMDataSource::~OGROSMDataSource()

{
    CPLDebug("OSM", "nNodeSelectBetween = %d", nNodeSelectBetween);
    CPLDebug("OSM", "nNodeSelectIn = %d", nNodeSelectIn);

    int i;
    for(i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree(papoLayers);

    CPLFree(pszName);

    OSM_Close(psParser);

    CPLFree(panLonLatCache);

    if( hDB != NULL )
        CloseDB();

#ifdef HAVE_SQLITE_VFS
    if (pMyVFS)
    {
        sqlite3_vfs_unregister(pMyVFS);
        CPLFree(pMyVFS->pAppData);
        CPLFree(pMyVFS);
    }
#endif

    if( osTmpDBName.size() && bMustUnlink )
    {
        const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
        if( !EQUAL(pszVal, "NOT_EVEN_AT_END") )
            VSIUnlink(osTmpDBName);
    }
}

/************************************************************************/
/*                             CloseDB()                               */
/************************************************************************/

void OGROSMDataSource::CloseDB()
{
    int i;

    if( hInsertNodeStmt != NULL )
        sqlite3_finalize( hInsertNodeStmt );
    hInsertNodeStmt = NULL;

    if( hInsertWayStmt != NULL )
        sqlite3_finalize( hInsertWayStmt );
    hInsertWayStmt = NULL;

    if( hSelectNodeBetweenStmt != NULL )
        sqlite3_finalize( hSelectNodeBetweenStmt );
    hSelectNodeBetweenStmt = NULL;

    if( pahSelectNodeStmt != NULL )
    {
        for(i = 0; i < LIMIT_IDS_PER_REQUEST; i++)
        {
            if( pahSelectNodeStmt[i] != NULL )
                sqlite3_finalize( pahSelectNodeStmt[i] );
        }
        CPLFree(pahSelectNodeStmt);
        pahSelectNodeStmt = NULL;
    }

    if( pahSelectWayStmt != NULL )
    {
        for(i = 0; i < LIMIT_IDS_PER_REQUEST; i++)
        {
            if( pahSelectWayStmt[i] != NULL )
                sqlite3_finalize( pahSelectWayStmt[i] );
        }
        CPLFree(pahSelectWayStmt);
        pahSelectWayStmt = NULL;
    }

    if( bInTransaction )
        CommitTransaction();

    sqlite3_close(hDB);
    hDB = NULL;
}

/************************************************************************/
/*                             IndexPoint()                             */
/************************************************************************/

void OGROSMDataSource::IndexPoint(OSMNode* psNode)
{
    if( !bIndexPoints )
        return;

    sqlite3_bind_int64( hInsertNodeStmt, 1, psNode->nID );

    int anLonLat[2];
    anLonLat[0] = DBL_TO_INT(psNode->dfLon);
    anLonLat[1] = DBL_TO_INT(psNode->dfLat);

    sqlite3_bind_blob( hInsertNodeStmt, 2, anLonLat, 2 * sizeof(int), SQLITE_STATIC );

    int rc = sqlite3_step( hInsertNodeStmt );
    sqlite3_reset( hInsertNodeStmt );
    if( !(rc == SQLITE_OK || rc == SQLITE_DONE) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed inserting node " CPL_FRMT_GIB ": %s",
             psNode->nID, sqlite3_errmsg(hDB));
    }
}

/************************************************************************/
/*                             NotifyNodes()                            */
/************************************************************************/

void OGROSMDataSource::NotifyNodes(unsigned int nNodes, OSMNode* pasNodes)
{
    unsigned int i;

    const OGREnvelope* psEnvelope =
        papoLayers[IDX_LYR_POINTS]->GetSpatialFilterEnvelope();

    for(i = 0; i < nNodes; i++)
    {
        /* If the point doesn't fit into the envelope of the spatial filter */
        /* then skip it */
        if( psEnvelope != NULL &&
            !(pasNodes[i].dfLon >= psEnvelope->MinX &&
              pasNodes[i].dfLon <= psEnvelope->MaxX &&
              pasNodes[i].dfLat >= psEnvelope->MinY &&
              pasNodes[i].dfLat <= psEnvelope->MaxY) )
            continue;

        IndexPoint(&pasNodes[i]);

        if( !papoLayers[IDX_LYR_POINTS]->IsUserInterested() )
            continue;

        unsigned int j;
        int bInterestingTag = bReportAllNodes;
        OSMTag* pasTags = pasNodes[i].pasTags;

        if( !bReportAllNodes )
        {
            for(j = 0; j < pasNodes[i].nTags; j++)
            {
                const char* pszK = pasTags[j].pszK;
                if( papoLayers[IDX_LYR_POINTS]->aoSetUnsignificantKeys.find(pszK) ==
                    papoLayers[IDX_LYR_POINTS]->aoSetUnsignificantKeys.end() )
                {
                    bInterestingTag = TRUE;
                    break;
                }
            }
        }

        if( bInterestingTag )
        {
            OGRFeature* poFeature = new OGRFeature(
                        papoLayers[IDX_LYR_POINTS]->GetLayerDefn());

            poFeature->SetGeometryDirectly(
                new OGRPoint(pasNodes[i].dfLon, pasNodes[i].dfLat));

            papoLayers[IDX_LYR_POINTS]->SetFieldsFromTags(
                poFeature, pasNodes[i].nID, pasNodes[i].nTags, pasTags, &pasNodes[i].sInfo );

            int bFilteredOut = FALSE;
            if( !papoLayers[IDX_LYR_POINTS]->AddFeature(poFeature, FALSE, &bFilteredOut) )
            {
                bStopParsing = TRUE;
                break;
            }
            else if (!bFilteredOut)
                bFeatureAdded = TRUE;
        }
    }
}

static void OGROSMNotifyNodes (unsigned int nNodes, OSMNode* pasNodes,
                               OSMContext* psOSMContext, void* user_data)
{
    ((OGROSMDataSource*) user_data)->NotifyNodes(nNodes, pasNodes);
}

/************************************************************************/
/*                            LookupNodes()                             */
/************************************************************************/

unsigned int OGROSMDataSource::LookupNodes( std::map< GIntBig, std::pair<int,int> >& aoMapNodes,
                                            OSMWay* psWay )
{
    unsigned int nFound = 0;
    unsigned int iCur = 0;
    unsigned int i;

    GIntBig nMin = psWay->panNodeRefs[0];
    GIntBig nMax = psWay->panNodeRefs[0];
    for(i = 1; i < psWay->nRefs; i++)
    {
        if( psWay->panNodeRefs[i] < nMin ) nMin = psWay->panNodeRefs[i];
        else if( psWay->panNodeRefs[i] > nMax ) nMax = psWay->panNodeRefs[i];
    }

    /* If the node ids to request are rather grouped together, use */
    /* a WHERE id BETWEEN nMin AND nMAX clause for better efficiency */
    if( nMax - nMin < 3 * psWay->nRefs )
    {
        nNodeSelectBetween ++;

        sqlite3_bind_int64( hSelectNodeBetweenStmt, 1, nMin );
        sqlite3_bind_int64( hSelectNodeBetweenStmt, 2, nMax );

        while( sqlite3_step(hSelectNodeBetweenStmt) == SQLITE_ROW )
        {
            GIntBig id = sqlite3_column_int(hSelectNodeBetweenStmt, 0);
            int* panLonLat = (int*)sqlite3_column_blob(hSelectNodeBetweenStmt, 1);
            aoMapNodes[id] = std::pair<int,int>(panLonLat[0], panLonLat[1]);
            nFound++;
        }

        sqlite3_reset(hSelectNodeBetweenStmt);

        return nFound;
    }

    nNodeSelectIn ++;

    while( iCur < psWay->nRefs )
    {
        unsigned int nToQuery = psWay->nRefs - iCur;
        if( nToQuery > LIMIT_IDS_PER_REQUEST )
            nToQuery = LIMIT_IDS_PER_REQUEST;

        sqlite3_stmt* hStmt = pahSelectNodeStmt[nToQuery-1];
        for(i=iCur;i<iCur + nToQuery;i++)
        {
             sqlite3_bind_int64( hStmt, i - iCur +1, psWay->panNodeRefs[i] );
        }
        iCur += nToQuery;

        while( sqlite3_step(hStmt) == SQLITE_ROW )
        {
            GIntBig id = sqlite3_column_int(hStmt, 0);
            int* panLonLat = (int*)sqlite3_column_blob(hStmt, 1);
            aoMapNodes[id] = std::pair<int,int>(panLonLat[0], panLonLat[1]);
            nFound++;
        }

        sqlite3_reset(hStmt);
    }

    return nFound;
}

/************************************************************************/
/*                              IndexWay()                              */
/************************************************************************/

void OGROSMDataSource::IndexWay(GIntBig nWayID, int* panLonLatPairs, int nPairs)
{
    if( !bIndexWays )
        return;

    sqlite3_bind_int64( hInsertWayStmt, 1, nWayID );

    sqlite3_bind_blob( hInsertWayStmt, 2, panLonLatPairs,
                        sizeof(int) * 2 * nPairs, SQLITE_STATIC );

    int rc = sqlite3_step( hInsertWayStmt );
    sqlite3_reset( hInsertWayStmt );
    if( !(rc == SQLITE_OK || rc == SQLITE_DONE) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Failed inserting way " CPL_FRMT_GIB ": %s",
                nWayID, sqlite3_errmsg(hDB));
    }
}

/************************************************************************/
/*                              NotifyWay()                             */
/************************************************************************/

void OGROSMDataSource::NotifyWay (OSMWay* psWay)
{
    unsigned int i;

    if( !bUsePointsIndex )
        return;

    //printf("way %d : %d nodes\n", (int)psWay->nID, (int)psWay->nRefs);
    if( psWay->nRefs > MAX_NODES_PER_WAY )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Ways with more than %d nodes are not supported",
                 MAX_NODES_PER_WAY);
        return;
    }

    if( psWay->nRefs < 2 )
    {
        CPLDebug("OSM", "Way " CPL_FRMT_GIB " with %d nodes. Discarding it",
                 psWay->nID, psWay->nRefs);
        return;
    }

    /* Is a closed way a polygon ? */
    int bIsArea = FALSE;
    if( psWay->panNodeRefs[0] == psWay->panNodeRefs[psWay->nRefs - 1] )
    {
        for(i=0;i<psWay->nTags;i++)
        {
            const char* pszK = psWay->pasTags[i].pszK;
            if( strcmp(pszK, "area") == 0 )
            {
                if( strcmp(psWay->pasTags[i].pszV, "yes") == 0 )
                {
                    bIsArea = TRUE;
                }
                else if( strcmp(psWay->pasTags[i].pszV, "no") == 0 )
                {
                    bIsArea = FALSE;
                    break;
                }
            }
            else if( aoSetClosedWaysArePolygons.find(pszK) !=
                     aoSetClosedWaysArePolygons.end() )
            {
                bIsArea = TRUE;
            }
        }
    }

    int iCurLayer = (bIsArea) ? IDX_LYR_POLYGONS : IDX_LYR_LINES ;

    if( !papoLayers[iCurLayer]->IsUserInterested() &&
        !bIndexWays )
    {
        return;
    }

    OGRFeature* poFeature = NULL;

    /* Optimization : if we have an attribute filter, that does not require geometry, */
    /* and if we don't need to index ways, then we can just evaluate the attribute */
    /* filter without the geometry */
    if( papoLayers[iCurLayer]->HasAttributeFilter() &&
        !papoLayers[iCurLayer]->AttributeFilterEvaluationNeedsGeometry() &&
        !bIndexWays )
    {
        poFeature = new OGRFeature(papoLayers[iCurLayer]->GetLayerDefn());

        papoLayers[iCurLayer]->SetFieldsFromTags(
            poFeature, psWay->nID, psWay->nTags, psWay->pasTags, &psWay->sInfo );

        if( !papoLayers[iCurLayer]->EvaluateAttributeFilter(poFeature) )
        {
            delete poFeature;
            return;
        }
    }

    std::map< GIntBig, std::pair<int,int> > aoMapNodes;
    unsigned int nFound = LookupNodes(aoMapNodes, psWay);
    if( nFound < 2 )
    {
        CPLDebug("OSM", "Way " CPL_FRMT_GIB " with %d nodes that could be found. Discarding it",
                 psWay->nID, nFound);
        delete poFeature;
        return;
    }

    if( panLonLatCache == NULL )
        panLonLatCache = (int*)CPLMalloc(sizeof(int) * 2 * MAX_NODES_PER_WAY);

    std::map< GIntBig, std::pair<int,int> >::iterator oIter;
    nFound = 0;
    for(i=0;i<psWay->nRefs;i++)
    {
        oIter = aoMapNodes.find( psWay->panNodeRefs[i] );
        if( oIter != aoMapNodes.end() )
        {
            const std::pair<int,int>& oPair = oIter->second;
            panLonLatCache[nFound * 2 + 0] = oPair.first;
            panLonLatCache[nFound * 2 + 1] = oPair.second;
            nFound ++;
        }
    }

    if( nFound < 2 )
    {
        CPLDebug("OSM", "Way " CPL_FRMT_GIB " with %d nodes that could be found. Discarding it",
                 psWay->nID, nFound);
        delete poFeature;
        return;
    }

    IndexWay(psWay->nID, panLonLatCache, (int)nFound);

    if( !papoLayers[iCurLayer]->IsUserInterested() )
    {
        delete poFeature;
        return;
    }

    OGRLineString* poLS;
    OGRGeometry* poGeom;
    if( bIsArea )
    {
        /*OGRMultiPolygon* poMulti = new OGRMultiPolygon();*/
        OGRPolygon* poPoly = new OGRPolygon();
        OGRLinearRing* poRing = new OGRLinearRing();
        /*poMulti->addGeometryDirectly(poPoly);*/
        poPoly->addRingDirectly(poRing);
        poLS = poRing;

        poGeom = poPoly;
    }
    else
    {
        poLS = new OGRLineString();
        poGeom = poLS;
    }

    poLS->setNumPoints((int)nFound);
    for(i=0;i<nFound;i++)
    {
        poLS->setPoint(i,
                       INT_TO_DBL(panLonLatCache[i * 2 + 0]),
                       INT_TO_DBL(panLonLatCache[i * 2 + 1]));
    }

    int bAttrFilterAlreadyEvaluated;
    if( poFeature == NULL )
    {
        poFeature = new OGRFeature(papoLayers[iCurLayer]->GetLayerDefn());

        papoLayers[iCurLayer]->SetFieldsFromTags(
            poFeature, psWay->nID, psWay->nTags, psWay->pasTags, &psWay->sInfo );

        bAttrFilterAlreadyEvaluated = FALSE;
    }
    else
        bAttrFilterAlreadyEvaluated = TRUE;

    poFeature->SetGeometryDirectly(poGeom);

    if( nFound != psWay->nRefs )
        CPLDebug("OSM", "For way " CPL_FRMT_GIB ", got only %d nodes instead of %d\n",
                 psWay->nID, nFound, psWay->nRefs);

    int bFilteredOut = FALSE;
    if( !papoLayers[iCurLayer]->AddFeature(poFeature, bAttrFilterAlreadyEvaluated, &bFilteredOut) )
        bStopParsing = TRUE;
    else if (!bFilteredOut)
        bFeatureAdded = TRUE;
}

static void OGROSMNotifyWay (OSMWay* psWay, OSMContext* psOSMContext, void* user_data)
{
    ((OGROSMDataSource*) user_data)->NotifyWay(psWay);
}

/************************************************************************/
/*                            LookupWays()                              */
/************************************************************************/

unsigned int OGROSMDataSource::LookupWays( std::map< GIntBig, std::pair<int,void*> >& aoMapWays,
                                           OSMRelation* psRelation )
{
    unsigned int nFound = 0;
    unsigned int iCur = 0;
    unsigned int i;

    while( iCur < psRelation->nMembers )
    {
        unsigned int nToQuery = 0;
        for(i=iCur;i<psRelation->nMembers;i++)
        {
            if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
                strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0 )
            {
                nToQuery ++;
                if( nToQuery == LIMIT_IDS_PER_REQUEST )
                    break;
            }
        }

        if( nToQuery == 0)
            break;

        unsigned int iLastI = (i == psRelation->nMembers) ? i : i + 1;

        sqlite3_stmt* hStmt = pahSelectWayStmt[nToQuery-1];
        unsigned int nBindIndex = 1;
        for(i=iCur;i<iLastI;i++)
        {
            if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
                strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0 )
            {
                sqlite3_bind_int64( hStmt, nBindIndex,
                                    psRelation->pasMembers[i].nID );
                nBindIndex ++;
            }
        }
        iCur = iLastI;

        while( sqlite3_step(hStmt) == SQLITE_ROW )
        {
            GIntBig id = sqlite3_column_int(hStmt, 0);
            if( aoMapWays.find(id) == aoMapWays.end() )
            {
                int nBlobSize = sqlite3_column_bytes(hStmt, 1);
                const void* blob = sqlite3_column_blob(hStmt, 1);
                void* blob_dup = CPLMalloc(nBlobSize);
                memcpy(blob_dup, blob, nBlobSize);
                aoMapWays[id] = std::pair<int,void*>(nBlobSize, blob_dup);
            }
            nFound++;
        }

        sqlite3_reset(hStmt);
    }

    return nFound;
}

/************************************************************************/
/*                          BuildMultiPolygon()                         */
/************************************************************************/

OGRGeometry* OGROSMDataSource::BuildMultiPolygon(OSMRelation* psRelation)
{

    std::map< GIntBig, std::pair<int,void*> > aoMapWays;
    LookupWays( aoMapWays, psRelation );

    int bMissing = FALSE;
    unsigned int i;
    for(i = 0; i < psRelation->nMembers; i ++ )
    {
        if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
            strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0  &&
            aoMapWays.find( psRelation->pasMembers[i].nID ) == aoMapWays.end() )
        {
            CPLDebug("OSM", "Relation " CPL_FRMT_GIB " has missing ways. Ignoring it",
                     psRelation->nID);
            bMissing = TRUE;
            break;
        }
    }

    OGRGeometry* poRet = NULL;
    OGRMultiLineString* poMLS = NULL;
    OGRGeometry** papoPolygons = NULL;
    int nPolys = 0;

    if( bMissing )
        goto cleanup;

    poMLS = new OGRMultiLineString();
    papoPolygons = (OGRGeometry**) CPLMalloc(
        sizeof(OGRGeometry*) *  psRelation->nMembers);
    nPolys = 0;

    for(i = 0; i < psRelation->nMembers; i ++ )
    {
        if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
            strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0  )
        {
            const std::pair<int, void*>& oGeom = aoMapWays[ psRelation->pasMembers[i].nID ];
            int nPoints = oGeom.first / (2 * sizeof(int));
            int* panCoords = (int*) oGeom.second;
            OGRLineString* poLS;

            if ( panCoords[0] == panCoords[2 * (nPoints - 1)] &&
                    panCoords[1] == panCoords[2 * (nPoints - 1) + 1] )
            {
                OGRPolygon* poPoly = new OGRPolygon();
                OGRLinearRing* poRing = new OGRLinearRing();
                poPoly->addRingDirectly(poRing);
                papoPolygons[nPolys ++] = poPoly;
                poLS = poRing;
            }
            else
            {
                poLS = new OGRLineString();
                poMLS->addGeometryDirectly(poLS);
            }

            poLS->setNumPoints(nPoints);
            for(int j=0;j<nPoints;j++)
            {
                poLS->setPoint( j,
                                INT_TO_DBL(panCoords[2 * j + 0]),
                                INT_TO_DBL(panCoords[2 * j + 1]) );
            }

        }
    }

    if( poMLS->getNumGeometries() > 0 )
    {
        OGRGeometryH hPoly = OGRBuildPolygonFromEdges( (OGRGeometryH) poMLS,
                                                        TRUE,
                                                        FALSE,
                                                        0,
                                                        NULL );
        if( hPoly != NULL && OGR_G_GetGeometryType(hPoly) == wkbPolygon )
        {
            OGRPolygon* poSuperPoly = (OGRPolygon* ) hPoly;
            for(i = 0; i < 1 + (unsigned int)poSuperPoly->getNumInteriorRings(); i++)
            {
                OGRPolygon* poPoly = new OGRPolygon();
                OGRLinearRing* poRing =  (i == 0) ? poSuperPoly->getExteriorRing() :
                                                    poSuperPoly->getInteriorRing(i - 1);
                if( poRing != NULL && poRing->getNumPoints() >= 4 &&
                    poRing->getX(0) == poRing->getX(poRing->getNumPoints() -1) &&
                    poRing->getY(0) == poRing->getY(poRing->getNumPoints() -1) )
                {
                    poPoly->addRing( poRing );
                    papoPolygons[nPolys ++] = poPoly;
                }
            }
        }

        OGR_G_DestroyGeometry(hPoly);
    }
    delete poMLS;

    if( nPolys > 0 )
    {
        int bIsValidGeometry = FALSE;
        const char* apszOptions[2] = { "METHOD=DEFAULT", NULL };
        OGRGeometry* poGeom = OGRGeometryFactory::organizePolygons(
            papoPolygons, nPolys, &bIsValidGeometry, apszOptions );

        if( poGeom != NULL && poGeom->getGeometryType() == wkbPolygon )
        {
            OGRMultiPolygon* poMulti = new OGRMultiPolygon();
            poMulti->addGeometryDirectly(poGeom);
            poGeom = poMulti;
        }

        if( poGeom != NULL && poGeom->getGeometryType() == wkbMultiPolygon )
        {
            poRet = poGeom;
        }
        else
        {
            CPLDebug("OSM", "Relation " CPL_FRMT_GIB ": Geometry has incompatible type : %s",
                     psRelation->nID,
                     poGeom != NULL ? OGR_G_GetGeometryName((OGRGeometryH)poGeom) : "null" );
            delete poGeom;
        }
    }

    CPLFree(papoPolygons);

cleanup:
    /* Cleanup */
    std::map< GIntBig, std::pair<int,void*> >::iterator oIter;
    for( oIter = aoMapWays.begin(); oIter != aoMapWays.end(); ++oIter )
        CPLFree(oIter->second.second);

    return poRet;
}

/************************************************************************/
/*                          BuildGeometryCollection()                   */
/************************************************************************/

OGRGeometry* OGROSMDataSource::BuildGeometryCollection(OSMRelation* psRelation, int bMultiLineString)
{
    std::map< GIntBig, std::pair<int,void*> > aoMapWays;
    LookupWays( aoMapWays, psRelation );

    unsigned int i;

    OGRGeometryCollection* poColl;
    if( bMultiLineString )
        poColl = new OGRMultiLineString();
    else
        poColl = new OGRGeometryCollection();

    for(i = 0; i < psRelation->nMembers; i ++ )
    {
        if( psRelation->pasMembers[i].eType == MEMBER_NODE && !bMultiLineString )
        {
            std::map< GIntBig, std::pair<int,int> > aoMapNodes;
            OSMWay sWay;
            sWay.nRefs = 1;
            sWay.panNodeRefs = &( psRelation->pasMembers[i].nID );
            unsigned int nFound = LookupNodes(aoMapNodes, &sWay);
            if( nFound == 1 )
            {
                const std::pair<int,int>& oPair = aoMapNodes[psRelation->pasMembers[i].nID];
                poColl->addGeometryDirectly(new OGRPoint(INT_TO_DBL(oPair.first), INT_TO_DBL(oPair.second)));
            }
        }
        else if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
                 strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0  &&
                 aoMapWays.find( psRelation->pasMembers[i].nID ) != aoMapWays.end() )
        {
            const std::pair<int, void*>& oGeom = aoMapWays[ psRelation->pasMembers[i].nID ];
            int nPoints = oGeom.first / (2 * sizeof(int));
            int* panCoords = (int*) oGeom.second;
            OGRLineString* poLS;

            poLS = new OGRLineString();
            poColl->addGeometryDirectly(poLS);

            poLS->setNumPoints(nPoints);
            for(int j=0;j<nPoints;j++)
            {
                poLS->setPoint( j,
                                INT_TO_DBL(panCoords[2 * j + 0]),
                                INT_TO_DBL(panCoords[2 * j + 1]) );
            }

        }
    }

    if( poColl->getNumGeometries() == 0 )
    {
        delete poColl;
        poColl = NULL;
    }

    /* Cleanup */
    std::map< GIntBig, std::pair<int,void*> >::iterator oIter;
    for( oIter = aoMapWays.begin(); oIter != aoMapWays.end(); ++oIter )
        CPLFree(oIter->second.second);

    return poColl;
}

/************************************************************************/
/*                            NotifyRelation()                          */
/************************************************************************/

void OGROSMDataSource::NotifyRelation (OSMRelation* psRelation)
{
    unsigned int i;

    if( !bUseWaysIndex )
        return;

    /*for(i=0;i<psRelation->nMembers;i++)
    {
        printf("[%d] %s\n", i, psRelation->pasMembers[i].pszRole);
    }*/

    int bMultiPolygon = FALSE;
    int bMultiLineString = FALSE;
    for(i = 0; i < psRelation->nTags; i ++ )
    {
        if( strcmp(psRelation->pasTags[i].pszK, "type") == 0 )
        {
            const char* pszV = psRelation->pasTags[i].pszV;
            if( strcmp(pszV, "multipolygon") == 0 ||
                strcmp(pszV, "boundary") == 0)
            {
                bMultiPolygon = TRUE;
            }
            else if( strcmp(pszV, "multilinestring") == 0 ||
                     strcmp(pszV, "route") == 0 )
            {
                bMultiLineString = TRUE;
            }
            break;
        }
    }

    /* Optimization : if we have an attribute filter, that does not require geometry, */
    /* then we can just evaluate the attribute filter without the geometry */
    int iCurLayer = (bMultiPolygon) ?    IDX_LYR_MULTIPOLYGONS :
                    (bMultiLineString) ? IDX_LYR_MULTILINESTRINGS :
                                         IDX_LYR_OTHER_RELATIONS;
    if( !papoLayers[iCurLayer]->IsUserInterested() )
        return;

    OGRFeature* poFeature = NULL;

    if( papoLayers[iCurLayer]->HasAttributeFilter() &&
        !papoLayers[iCurLayer]->AttributeFilterEvaluationNeedsGeometry() )
    {
        poFeature = new OGRFeature(papoLayers[iCurLayer]->GetLayerDefn());

        papoLayers[iCurLayer]->SetFieldsFromTags( poFeature,
                                                  psRelation->nID,
                                                  psRelation->nTags,
                                                  psRelation->pasTags,
                                                  &psRelation->sInfo);

        if( !papoLayers[iCurLayer]->EvaluateAttributeFilter(poFeature) )
        {
            delete poFeature;
            return;
        }
    }

    OGRGeometry* poGeom;

    if( bMultiPolygon )
        poGeom = BuildMultiPolygon(psRelation);
    else
        poGeom = BuildGeometryCollection(psRelation, bMultiLineString);

    if( poGeom != NULL )
    {
        int bAttrFilterAlreadyEvaluated;
        if( poFeature == NULL )
        {
            poFeature = new OGRFeature(papoLayers[iCurLayer]->GetLayerDefn());

            papoLayers[iCurLayer]->SetFieldsFromTags( poFeature,
                                                      psRelation->nID,
                                                      psRelation->nTags,
                                                      psRelation->pasTags,
                                                      &psRelation->sInfo);

            bAttrFilterAlreadyEvaluated = FALSE;
        }
        else
            bAttrFilterAlreadyEvaluated = TRUE;

        poFeature->SetGeometryDirectly(poGeom);

        int bFilteredOut = FALSE;
        if( !papoLayers[iCurLayer]->AddFeature( poFeature,
                                                bAttrFilterAlreadyEvaluated,
                                                &bFilteredOut ) )
            bStopParsing = TRUE;
        else if (!bFilteredOut)
            bFeatureAdded = TRUE;
    }
    else
        delete poFeature;
}

static void OGROSMNotifyRelation (OSMRelation* psRelation,
                                  OSMContext* psOSMContext, void* user_data)
{
    ((OGROSMDataSource*) user_data)->NotifyRelation(psRelation);
}

/************************************************************************/
/*                             NotifyBounds()                           */
/************************************************************************/

void OGROSMDataSource::NotifyBounds (double dfXMin, double dfYMin,
                                     double dfXMax, double dfYMax)
{
    sExtent.MinX = dfXMin;
    sExtent.MinY = dfYMin;
    sExtent.MaxX = dfXMax;
    sExtent.MaxY = dfYMax;
    bExtentValid = TRUE;

    CPLDebug("OSM", "Got bounds : minx=%f, miny=%f, maxx=%f, maxy=%f",
             dfXMin, dfYMin, dfXMax, dfYMax);
}

static void OGROSMNotifyBounds( double dfXMin, double dfYMin,
                                double dfXMax, double dfYMax,
                                OSMContext* psCtxt, void* user_data )
{
    ((OGROSMDataSource*) user_data)->NotifyBounds(dfXMin, dfYMin,
                                                  dfXMax, dfYMax);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROSMDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    const char* pszExt = CPLGetExtension(pszFilename);
    if( !EQUAL(pszExt, "pbf") &&
        !EQUAL(pszExt, "osm") &&
        !EQUALN(pszFilename, "/vsicurl_streaming/", strlen("/vsicurl_streaming/")) &&
        strcmp(pszFilename, "/vsistdin/") != 0 &&
        strcmp(pszFilename, "/dev/stdin/") != 0 )
        return FALSE;

    pszName = CPLStrdup( pszFilename );

    psParser = OSM_Open( pszName,
                         OGROSMNotifyNodes,
                         OGROSMNotifyWay,
                         OGROSMNotifyRelation,
                         OGROSMNotifyBounds,
                         this );
    if( psParser == NULL )
        return FALSE;

    if (bUpdateIn)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "OGR/OSM driver does not support opening a file in update mode");
        return FALSE;
    }

    nLayers = 6;
    papoLayers = (OGROSMLayer**) CPLMalloc(nLayers * sizeof(OGROSMLayer*));

    papoLayers[IDX_LYR_POINTS] = new OGROSMLayer(this, "points");
    papoLayers[IDX_LYR_POINTS]->GetLayerDefn()->SetGeomType(wkbPoint);

    papoLayers[IDX_LYR_LINES] = new OGROSMLayer(this, "lines");
    papoLayers[IDX_LYR_LINES]->GetLayerDefn()->SetGeomType(wkbLineString);

    papoLayers[IDX_LYR_POLYGONS] = new OGROSMLayer(this, "polygons");
    papoLayers[IDX_LYR_POLYGONS]->GetLayerDefn()->SetGeomType(wkbPolygon);

    papoLayers[IDX_LYR_MULTILINESTRINGS] = new OGROSMLayer(this, "multilinestrings");
    papoLayers[IDX_LYR_MULTILINESTRINGS]->GetLayerDefn()->SetGeomType(wkbMultiLineString);

    papoLayers[IDX_LYR_MULTIPOLYGONS] = new OGROSMLayer(this, "multipolygons");
    papoLayers[IDX_LYR_MULTIPOLYGONS]->GetLayerDefn()->SetGeomType(wkbMultiPolygon);

    papoLayers[IDX_LYR_OTHER_RELATIONS] = new OGROSMLayer(this, "other_relations");
    papoLayers[IDX_LYR_OTHER_RELATIONS]->GetLayerDefn()->SetGeomType(wkbGeometryCollection);

    if( !ParseConf() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not parse configuration file for OSM import");
        return FALSE;
    }

    return CreateTempDB();
}

/************************************************************************/
/*                             CreateTempDB()                           */
/************************************************************************/

int OGROSMDataSource::CreateTempDB()
{
    char* pszErrMsg = NULL;

    int rc;
    int bIsExisting = FALSE;

#ifdef HAVE_SQLITE_VFS
    const char* pszExistingTmpFile = CPLGetConfigOption("OSM_EXISTING_TMPFILE", NULL);
    if ( pszExistingTmpFile != NULL )
    {
        bIsExisting = TRUE;
        rc = sqlite3_open_v2( pszExistingTmpFile, &hDB,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                              NULL );
    }
    else
    {
        bInMemoryTmpDB = TRUE;
        osTmpDBName.Printf("/vsimem/osm_importer/osm_temp_%p.sqlite", this);
        pMyVFS = OGRSQLiteCreateVFS(NULL, this);
        sqlite3_vfs_register(pMyVFS, 0);
        rc = sqlite3_open_v2( osTmpDBName.c_str(), &hDB,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                            pMyVFS->zName );
    }
#else
    osTmpDBName = CPLGenerateTempFilename("osm_tmp");
    rc = sqlite3_open( osTmpDBName.c_str(), &hDB );

    /* On Unix filesystems, you can remove a file even if it */
    /* opened */
    if( rc == SQLITE_OK )
    {
        const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
        if( EQUAL(pszVal, "YES") )
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            bMustUnlink = VSIUnlink( osTmpDBName ) != 0;
            CPLPopErrorHandler();
        }
    }
#endif

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "sqlite3_open(%s) failed: %s",
                  osTmpDBName.c_str(), sqlite3_errmsg( hDB ) );
        return FALSE;
    }

    if( !SetDBOptions() )
    {
        return FALSE;
    }

    if( !bIsExisting )
    {
        rc = sqlite3_exec( hDB,
                        "CREATE TABLE nodes (id INTEGER PRIMARY KEY, coords BLOB)",
                        NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to create table nodes : %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return FALSE;
        }

        rc = sqlite3_exec( hDB,
                        "CREATE TABLE ways (id INTEGER PRIMARY KEY, coords BLOB)",
                        NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to create table ways : %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return FALSE;
        }
    }

    return CreatePreparedStatements();
}
/************************************************************************/
/*                            SetDBOptions()                            */
/************************************************************************/

int OGROSMDataSource::SetDBOptions()
{
    char* pszErrMsg = NULL;
    int rc;

    rc = sqlite3_exec( hDB, "PRAGMA synchronous = OFF", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to run PRAGMA synchronous : %s",
                    pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    rc = sqlite3_exec( hDB, "PRAGMA journal_mode = OFF", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to run PRAGMA journal_mode : %s",
                    pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    rc = sqlite3_exec( hDB, "PRAGMA temp_store = MEMORY", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to run PRAGMA temp_store : %s",
                    pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    if( !SetCacheSize() )
        return FALSE;

    if( !StartTransaction() )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                              SetCacheSize()                          */
/************************************************************************/

int OGROSMDataSource::SetCacheSize()
{
    int rc;
    const char* pszSqliteCacheMB = CPLGetConfigOption("OSM_SQLITE_CACHE", NULL);
    if (pszSqliteCacheMB != NULL)
    {
        char* pszErrMsg = NULL;
        char **papszResult;
        int nRowCount, nColCount;
        int iSqliteCachePages;
        int iSqlitePageSize = -1;
        int iSqliteCacheBytes = atoi( pszSqliteCacheMB ) * 1024 * 1024;

        /* querying the current PageSize */
        rc = sqlite3_get_table( hDB, "PRAGMA page_size",
                                &papszResult, &nRowCount, &nColCount,
                                &pszErrMsg );
        if( rc == SQLITE_OK )
        {
            int iRow;
            for (iRow = 1; iRow <= nRowCount; iRow++)
            {
                iSqlitePageSize = atoi( papszResult[(iRow * nColCount) + 0] );
            }
            sqlite3_free_table(papszResult);
        }
        if( iSqlitePageSize < 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to run PRAGMA page_size : %s",
                      pszErrMsg ? pszErrMsg : sqlite3_errmsg(hDB) );
            sqlite3_free( pszErrMsg );
            return TRUE;
        }

        /* computing the CacheSize as #Pages */
        iSqliteCachePages = iSqliteCacheBytes / iSqlitePageSize;
        if( iSqliteCachePages <= 0)
            return TRUE;

        rc = sqlite3_exec( hDB, CPLSPrintf( "PRAGMA cache_size = %d",
                                            iSqliteCachePages ),
                           NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unrecognized value for PRAGMA cache_size : %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            rc = SQLITE_OK;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                        CreatePreparedStatements()                    */
/************************************************************************/

int OGROSMDataSource::CreatePreparedStatements()
{
    int rc;

    rc = sqlite3_prepare( hDB, "INSERT INTO nodes (id, coords) VALUES (?,?)", -1,
                          &hInsertNodeStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
        return FALSE;
    }

    ;
    rc = sqlite3_prepare( hDB, "SELECT id, coords FROM nodes WHERE id BETWEEN ? AND ?", -1,
                          &hSelectNodeBetweenStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
        return FALSE;
    }

    pahSelectNodeStmt = (sqlite3_stmt**) CPLCalloc(sizeof(sqlite3_stmt*), LIMIT_IDS_PER_REQUEST);

    char szTmp[LIMIT_IDS_PER_REQUEST*2 + 128];
    strcpy(szTmp, "SELECT id, coords FROM nodes WHERE id IN (");
    int nLen = strlen(szTmp);
    for(int i=0;i<LIMIT_IDS_PER_REQUEST;i++)
    {
        if(i == 0)
        {
            strcpy(szTmp + nLen, "?)");
            nLen += 2;
        }
        else
        {
            strcpy(szTmp + nLen -1, ",?)");
            nLen += 2;
        }
        rc = sqlite3_prepare( hDB, szTmp, -1, &pahSelectNodeStmt[i], NULL );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
            return FALSE;
        }
    }

    rc = sqlite3_prepare( hDB, "INSERT INTO ways (id, coords) VALUES (?,?)", -1,
                          &hInsertWayStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
        return FALSE;
    }

    pahSelectWayStmt = (sqlite3_stmt**) CPLCalloc(sizeof(sqlite3_stmt*), LIMIT_IDS_PER_REQUEST);

    strcpy(szTmp, "SELECT id, coords FROM ways WHERE id IN (");
    nLen = strlen(szTmp);
    for(int i=0;i<LIMIT_IDS_PER_REQUEST;i++)
    {
        if(i == 0)
        {
            strcpy(szTmp + nLen, "?)");
            nLen += 2;
        }
        else
        {
            strcpy(szTmp + nLen -1, ",?)");
            nLen += 2;
        }
        rc = sqlite3_prepare( hDB, szTmp, -1, &pahSelectWayStmt[i], NULL );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
            return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           StartTransaction()                         */
/************************************************************************/

int OGROSMDataSource::StartTransaction()
{
    if( bInTransaction )
        return FALSE;

    char* pszErrMsg = NULL;
    int rc = sqlite3_exec( hDB, "BEGIN", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to start transaction : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    bInTransaction = TRUE;

    return TRUE;
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

int OGROSMDataSource::CommitTransaction()
{
    if( !bInTransaction )
        return FALSE;

    bInTransaction = FALSE;

    char* pszErrMsg = NULL;
    int rc = sqlite3_exec( hDB, "COMMIT", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to commit transaction : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           ParseConf()                                */
/************************************************************************/

int OGROSMDataSource::ParseConf()
{
    const char *pszFilename = CPLGetConfigOption("OSM_CONFIG_FILE", NULL);
    if( pszFilename == NULL )
        pszFilename = CPLFindFile( "gdal", "osmconf.ini" );
    if( pszFilename == NULL )
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Cannot find osmconf.ini configuration file");
        return FALSE;
    }

    VSILFILE* fpConf = VSIFOpenL(pszFilename, "rb");
    if( fpConf == NULL )
        return FALSE;

    const char* pszLine;
    int iCurLayer = -1;

    int i;

    while((pszLine = CPLReadLine2L(fpConf, -1, NULL)) != NULL)
    {
        if(pszLine[0] == '[' && pszLine[strlen(pszLine)-1] == ']' )
        {
            iCurLayer = -1;
            pszLine ++;
            ((char*)pszLine)[strlen(pszLine)-1] = '\0'; /* Evil but OK */
            for(i = 0; i < nLayers; i++)
            {
                if( strcmp(pszLine, papoLayers[i]->GetName()) == 0 )
                {
                    iCurLayer = i;
                    break;
                }
            }
            if( iCurLayer < 0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer '%s' mentionned in %s is unknown to the driver",
                         pszLine, pszFilename);
            }
            continue;
        }

        if( strncmp(pszLine, "closed_ways_are_polygons=",
                    strlen("closed_ways_are_polygons=")) == 0)
        {
            char** papszTokens = CSLTokenizeString2(pszLine, "=", 0);
            if( CSLCount(papszTokens) == 2)
            {
                char** papszTokens2 = CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    aoSetClosedWaysArePolygons.insert(papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            CSLDestroy(papszTokens);
        }

        else if(strncmp(pszLine, "report_all_nodes=", strlen("report_all_nodes=")) == 0)
        {
            if( strcmp(pszLine + strlen("report_all_nodes="), "no") == 0 )
            {
                bReportAllNodes = FALSE;
            }
            else if( strcmp(pszLine + strlen("report_all_nodes="), "yes") == 0 )
            {
                bReportAllNodes = TRUE;
            }
        }

        else if(strncmp(pszLine, "attribute_name_laundering=", strlen("attribute_name_laundering=")) == 0)
        {
            if( strcmp(pszLine + strlen("attribute_name_laundering="), "no") == 0 )
            {
                bAttributeNameLaundering = FALSE;
            }
            else if( strcmp(pszLine + strlen("attribute_name_laundering="), "yes") == 0 )
            {
                bAttributeNameLaundering = TRUE;
            }
        }

        else if( iCurLayer >= 0 )
        {
            char** papszTokens = CSLTokenizeString2(pszLine, "=", 0);
            if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "other_tags") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasOtherTags(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                    papoLayers[iCurLayer]->SetHasOtherTags(TRUE);
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_id") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasOSMId(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasOSMId(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_id", OFTString);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_version") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasVersion(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasVersion(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_version", OFTInteger);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_timestamp") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasTimestamp(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasTimestamp(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_timestamp", OFTDateTime);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_uid") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasUID(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasUID(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_uid", OFTInteger);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_user") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasUser(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasUser(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_user", OFTString);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_changeset") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasChangeset(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasChangeset(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_changeset", OFTInteger);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "attributes") == 0 )
            {
                char** papszTokens2 = CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    papoLayers[iCurLayer]->AddField(papszTokens2[i], OFTString);
                }
                CSLDestroy(papszTokens2);
            }
            else if ( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "unsignificant") == 0 )
            {
                char** papszTokens2 = CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    papoLayers[iCurLayer]->aoSetUnsignificantKeys.insert(papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            else if ( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "ignore") == 0 )
            {
                char** papszTokens2 = CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    papoLayers[iCurLayer]->aoSetIgnoreKeys.insert(papszTokens2[i]);
                    papoLayers[iCurLayer]->aoSetWarnKeys.insert(papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            CSLDestroy(papszTokens);
        }
    }

    for(i=0;i<nLayers;i++)
    {
        if( papoLayers[i]->HasOtherTags() )
            papoLayers[i]->AddField("other_tags", OFTString);
    }

    VSIFCloseL(fpConf);

    return TRUE;
}

/************************************************************************/
/*                           ResetReading()                            */
/************************************************************************/

int OGROSMDataSource::ResetReading()
{
    if( hDB == NULL )
        return FALSE;

    OSM_ResetReading(psParser);

    char* pszErrMsg = NULL;
    int rc = sqlite3_exec( hDB, "DELETE FROM nodes", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to DELETE FROM nodes : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    rc = sqlite3_exec( hDB, "DELETE FROM ways", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to DELETE FROM ways : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    int i;
    for(i=0;i<nLayers;i++)
    {
        papoLayers[i]->ForceResetReading();
    }

    bStopParsing = FALSE;

    return TRUE;
}

/************************************************************************/
/*                           ParseNextChunk()                           */
/************************************************************************/

int OGROSMDataSource::ParseNextChunk()
{
    if( bStopParsing )
        return FALSE;

    bHasParsedFirstChunk = TRUE;
    bFeatureAdded = FALSE;
    while( TRUE )
    {
        OSMRetCode eRet = OSM_ProcessBlock(psParser);
        if( eRet == OSM_EOF || eRet == OSM_ERROR )
        {
            bStopParsing = TRUE;
            return FALSE;
        }
        else if( bFeatureAdded )
            break;
    }

    if( bInMemoryTmpDB )
    {
        VSIStatBufL sStat;
        if( VSIStatL( osTmpDBName, &sStat ) == 0 &&
            sStat.st_size / 1024 / 1024 > nMaxSizeForInMemoryDBInMB )
        {
            bInMemoryTmpDB = FALSE;

            CloseDB();

            CPLString osNewTmpDBName;
            int rc;

            osNewTmpDBName = CPLGenerateTempFilename("osm_tmp");

            CPLDebug("OSM", "%s too big for RAM. Transfering it onto disk in %s",
                     osTmpDBName.c_str(), osNewTmpDBName.c_str());

            if( CPLCopyFile( osNewTmpDBName, osTmpDBName ) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot copy %s to %s",
                         osNewTmpDBName.c_str(), osTmpDBName.c_str() );
                VSIUnlink(osNewTmpDBName);
                bStopParsing = TRUE;
                return FALSE;
            }

            VSIUnlink(osTmpDBName);

            osTmpDBName = osNewTmpDBName;

#ifdef HAVE_SQLITE_VFS
            rc = sqlite3_open_v2( osTmpDBName.c_str(), &hDB,
                                  SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                                  NULL );
#else
            rc = sqlite3_open( osTmpDBName.c_str(), &hDB );
#endif
            if( rc != SQLITE_OK )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                        "sqlite3_open(%s) failed: %s",
                        osTmpDBName.c_str(), sqlite3_errmsg( hDB ) );
                bStopParsing = TRUE;
                CloseDB();
                return FALSE;
            }

            /* On Unix filesystems, you can remove a file even if it */
            /* opened */
            const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if( EQUAL(pszVal, "YES") )
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                bMustUnlink = VSIUnlink( osTmpDBName ) != 0;
                CPLPopErrorHandler();
            }

            if( !SetDBOptions() || !CreatePreparedStatements() )
            {
                bStopParsing = TRUE;
                CloseDB();
                return FALSE;
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROSMDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGROSMDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGROSMDataSource::GetExtent( OGREnvelope *psExtent )
{
    if (!bHasParsedFirstChunk)
    {
        bHasParsedFirstChunk = TRUE;
        OSM_ProcessBlock(psParser);
    }

    if (bExtentValid)
    {
        memcpy(psExtent, &sExtent, sizeof(sExtent));
        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}


/************************************************************************/
/*                   OGROSMSingleFeatureLayer                           */
/************************************************************************/

class OGROSMSingleFeatureLayer : public OGRLayer
{
  private:
    int                 nVal;
    char               *pszVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
                        OGROSMSingleFeatureLayer( const char* pszLayerName,
                                                     int nVal );
                        OGROSMSingleFeatureLayer( const char* pszLayerName,
                                                     const char *pszVal );
                        ~OGROSMSingleFeatureLayer();

    virtual void        ResetReading() { iNextShapeId = 0; }
    virtual OGRFeature *GetNextFeature();
    virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }
    virtual int         TestCapability( const char * ) { return FALSE; }
};


/************************************************************************/
/*                    OGROSMSingleFeatureLayer()                        */
/************************************************************************/

OGROSMSingleFeatureLayer::OGROSMSingleFeatureLayer(  const char* pszLayerName,
                                                     int nVal )
{
    poFeatureDefn = new OGRFeatureDefn( "SELECT" );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( pszLayerName, OFTInteger );
    poFeatureDefn->AddFieldDefn( &oField );

    iNextShapeId = 0;
    this->nVal = nVal;
    pszVal = NULL;
}

/************************************************************************/
/*                    OGROSMSingleFeatureLayer()                        */
/************************************************************************/

OGROSMSingleFeatureLayer::OGROSMSingleFeatureLayer(  const char* pszLayerName,
                                                     const char *pszVal )
{
    poFeatureDefn = new OGRFeatureDefn( "SELECT" );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( pszLayerName, OFTString );
    poFeatureDefn->AddFieldDefn( &oField );

    iNextShapeId = 0;
    nVal = 0;
    this->pszVal = CPLStrdup(pszVal);
}

/************************************************************************/
/*                    ~OGROSMSingleFeatureLayer()                       */
/************************************************************************/

OGROSMSingleFeatureLayer::~OGROSMSingleFeatureLayer()
{
    poFeatureDefn->Release();
    CPLFree(pszVal);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature * OGROSMSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return NULL;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    if (pszVal)
        poFeature->SetField(0, pszVal);
    else
        poFeature->SetField(0, nVal);
    poFeature->SetFID(iNextShapeId ++);
    return poFeature;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGROSMDataSource::ExecuteSQL( const char *pszSQLCommand,
                                         OGRGeometry *poSpatialFilter,
                                         const char *pszDialect )

{
/* -------------------------------------------------------------------- */
/*      Special GetBytesRead() command                                  */
/* -------------------------------------------------------------------- */
    if (strcmp(pszSQLCommand, "GetBytesRead()") == 0)
    {
        char szVal[64];
        sprintf(szVal, CPL_FRMT_GUIB, OSM_GetBytesRead(psParser));
        return new OGROSMSingleFeatureLayer( "GetBytesRead", szVal );
    }

    if( poResultSetLayer != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "A SQL result layer is still in use. Please delete it first");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special SET interest_layers = command                           */
/* -------------------------------------------------------------------- */
    if (strncmp(pszSQLCommand, "SET interest_layers =", 21) == 0)
    {
        char** papszTokens = CSLTokenizeString2(pszSQLCommand + 21, ",", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
        int i;

        for(i=0; i < nLayers; i++)
        {
            papoLayers[i]->SetDeclareInterest(FALSE);
        }

        for(i=0; papszTokens[i] != NULL; i++)
        {
            OGROSMLayer* poLayer = (OGROSMLayer*) GetLayerByName(papszTokens[i]);
            if( poLayer != NULL )
            {
                poLayer->SetDeclareInterest(TRUE);
            }
        }

        if( papoLayers[IDX_LYR_POINTS]->IsUserInterested() &&
            !papoLayers[IDX_LYR_LINES]->IsUserInterested() &&
            !papoLayers[IDX_LYR_POLYGONS]->IsUserInterested() &&
            !papoLayers[IDX_LYR_MULTILINESTRINGS]->IsUserInterested() &&
            !papoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() &&
            !papoLayers[IDX_LYR_OTHER_RELATIONS]->IsUserInterested())
        {
            if( CPLGetConfigOption("OSM_INDEX_POINTS", NULL) == NULL )
            {
                CPLDebug("OSM", "Disabling indexing of nodes");
                bIndexPoints = FALSE;
            }
            if( CPLGetConfigOption("OSM_USE_POINTS_INDEX", NULL) == NULL )
            {
                bUsePointsIndex = FALSE;
            }
            if( CPLGetConfigOption("OSM_INDEX_WAYS", NULL) == NULL )
            {
                CPLDebug("OSM", "Disabling indexing of ways");
                bIndexWays = FALSE;
            }
            if( CPLGetConfigOption("OSM_USE_WAYS_INDEX", NULL) == NULL )
            {
                bUseWaysIndex = FALSE;
            }
        }
        else if( (papoLayers[IDX_LYR_LINES]->IsUserInterested() ||
                  papoLayers[IDX_LYR_POLYGONS]->IsUserInterested()) &&
                 !papoLayers[IDX_LYR_MULTILINESTRINGS]->IsUserInterested() &&
                 !papoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() &&
                 !papoLayers[IDX_LYR_OTHER_RELATIONS]->IsUserInterested() )
        {
            if( CPLGetConfigOption("OSM_INDEX_WAYS", NULL) == NULL )
            {
                CPLDebug("OSM", "Disabling indexing of ways");
                bIndexWays = FALSE;
            }
            if( CPLGetConfigOption("OSM_USE_WAYS_INDEX", NULL) == NULL )
            {
                bUseWaysIndex = FALSE;
            }
        }

        CSLDestroy(papszTokens);

        return NULL;
    }

    while(*pszSQLCommand == ' ')
        pszSQLCommand ++;

    /* Try to analyse the SQL command to get the interest table */
    if( EQUALN(pszSQLCommand, "SELECT", 5) )
    {
        swq_select sSelectInfo;

        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLErr eErr = sSelectInfo.preparse( pszSQLCommand );
        CPLPopErrorHandler();

        if( eErr == CPLE_None )
        {
            int bOnlyFromThisDataSource = TRUE;

            swq_select* pCurSelect = &sSelectInfo;
            while(pCurSelect != NULL && bOnlyFromThisDataSource)
            {
                for( int iTable = 0; iTable < pCurSelect->table_count; iTable++ )
                {
                    swq_table_def *psTableDef = pCurSelect->table_defs + iTable;
                    if( psTableDef->data_source != NULL )
                    {
                        bOnlyFromThisDataSource = FALSE;
                        break;
                    }
                }

                pCurSelect = pCurSelect->poOtherSelect;
            }

            if( bOnlyFromThisDataSource )
            {
                int bLayerAlreadyAdded = FALSE;
                CPLString osInterestLayers = "SET interest_layers =";

                pCurSelect = &sSelectInfo;
                while(pCurSelect != NULL && bOnlyFromThisDataSource)
                {
                    for( int iTable = 0; iTable < pCurSelect->table_count; iTable++ )
                    {
                        swq_table_def *psTableDef = pCurSelect->table_defs + iTable;
                        if( bLayerAlreadyAdded ) osInterestLayers += ",";
                        bLayerAlreadyAdded = TRUE;
                        osInterestLayers += psTableDef->table_name;
                    }
                    pCurSelect = pCurSelect->poOtherSelect;
                }

                /* Backup current optimization parameters */
                abSavedDeclaredInterest.resize(0);
                for(int i=0; i < nLayers; i++)
                {
                    abSavedDeclaredInterest.push_back(papoLayers[i]->IsUserInterested());
                }
                bIndexPointsBackup = bIndexPoints;
                bUsePointsIndexBackup = bUsePointsIndex;
                bIndexWaysBackup = bIndexWays;
                bUseWaysIndexBackup = bUseWaysIndex;

                /* Update optimization parameters */
                ExecuteSQL(osInterestLayers, NULL, NULL);

                ResetReading();

                /* Run the request */
                OGRLayer* poRet = OGRDataSource::ExecuteSQL( pszSQLCommand,
                                                             poSpatialFilter,
                                                             pszDialect );

                poResultSetLayer = poRet;

                /* If the user explicitely run a COUNT() request, then do it ! */
                if( poResultSetLayer )
                    bIsFeatureCountEnabled = TRUE;

                return poRet;
            }
        }
    }

    return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                      poSpatialFilter,
                                      pszDialect );
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGROSMDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    if( poLayer != NULL && poLayer == poResultSetLayer )
    {
        poResultSetLayer = NULL;

        bIsFeatureCountEnabled = FALSE;

        /* Restore backup'ed optimization parameters */
        for(int i=0; i < nLayers; i++)
        {
            papoLayers[i]->SetDeclareInterest(abSavedDeclaredInterest[i]);
        }
        if( bIndexPointsBackup && !bIndexPoints )
            CPLDebug("OSM", "Re-enabling indexing of nodes");
        bIndexPoints = bIndexPointsBackup;
        bUsePointsIndex = bUsePointsIndexBackup;
        if( bIndexWaysBackup && !bIndexWays )
            CPLDebug("OSM", "Re-enabling indexing of ways");
        bIndexWays = bIndexWaysBackup;
        bUseWaysIndex = bUseWaysIndexBackup;
        abSavedDeclaredInterest.resize(0);
    }

    delete poLayer;
}

/************************************************************************/
/*                         IsInterleavedReading()                       */
/************************************************************************/

int OGROSMDataSource::IsInterleavedReading()
{
    if( bInterleavedReading < 0 )
    {
        bInterleavedReading = CSLTestBoolean(
                        CPLGetConfigOption("OGR_INTERLEAVED_READING", "NO"));
        CPLDebug("OSM", "OGR_INTERLEAVED_READING = %d", bInterleavedReading);
    }
    return bInterleavedReading;
}
