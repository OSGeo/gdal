/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGROSMLayer class
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_port.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "ogr_osm.h"
#include "osm_parser.h"
#include "sqlite3.h"

CPL_CVSID("$Id$")

constexpr int SWITCH_THRESHOLD = 10000;
constexpr int MAX_THRESHOLD = 100000;

constexpr int ALLTAGS_LENGTH = 8192;

/************************************************************************/
/*                          OGROSMLayer()                               */
/************************************************************************/

OGROSMLayer::OGROSMLayer( OGROSMDataSource* poDSIn, int nIdxLayerIn,
                          const char* pszName ) :
    poDS(poDSIn),
    nIdxLayer(nIdxLayerIn),
    poFeatureDefn(new OGRFeatureDefn( pszName )),
    poSRS(new OGRSpatialReference()),
    nFeatureCount(0),
    bResetReadingAllowed(false),
    nFeatureArraySize(0),
    nFeatureArrayMaxSize(0),
    nFeatureArrayIndex(0),
    papoFeatures(nullptr),
    bHasOSMId(false),
    nIndexOSMId(-1),
    nIndexOSMWayId(-1),
    bHasVersion(false),
    bHasTimestamp(false),
    bHasUID(false),
    bHasUser(false),
    bHasChangeset(false),
    bHasOtherTags(true),
    nIndexOtherTags(-1),
    bHasAllTags(false),
    nIndexAllTags(-1),
    bHasWarnedTooManyFeatures(false),
    pszAllTags(static_cast<char *>(CPLMalloc(ALLTAGS_LENGTH))),
    bHasWarnedAllTagsTruncated(false),
    bUserInterested(true)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    poSRS->SetWellKnownGeogCS("WGS84");
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    if( poFeatureDefn->GetGeomFieldCount() != 0 )
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
}

/************************************************************************/
/*                          ~OGROSMLayer()                           */
/************************************************************************/

OGROSMLayer::~OGROSMLayer()
{
    poFeatureDefn->Release();

    if( poSRS )
        poSRS->Release();

    for( int i=0; i<nFeatureArraySize; i++ )
    {
        if( papoFeatures[i] )
            delete papoFeatures[i];
    }

    for( int i=0; i<static_cast<int>(apszNames.size()); i++ )
        CPLFree(apszNames[i]);

    for( int i=0; i<static_cast<int>(apszInsignificantKeys.size()); i++ )
        CPLFree(apszInsignificantKeys[i]);

    for( int i=0; i<static_cast<int>(apszIgnoreKeys.size()); i++ )
        CPLFree(apszIgnoreKeys[i]);

    for( int i=0; i<static_cast<int>(oComputedAttributes.size()); i++ )
    {
        sqlite3_finalize(oComputedAttributes[i].hStmt);
    }

    CPLFree(pszAllTags);

    CPLFree(papoFeatures);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROSMLayer::ResetReading()
{
    if( !bResetReadingAllowed || poDS->IsInterleavedReading() )
        return;

    poDS->MyResetReading();
}

/************************************************************************/
/*                        ForceResetReading()                           */
/************************************************************************/

void OGROSMLayer::ForceResetReading()
{
    for(int i=0;i<nFeatureArraySize;i++)
    {
        if( papoFeatures[i] )
            delete papoFeatures[i];
    }
    nFeatureArrayIndex = 0;
    nFeatureArraySize = 0;
    nFeatureCount = 0;
    bResetReadingAllowed = false;
}

/************************************************************************/
/*                        SetAttributeFilter()                          */
/************************************************************************/

OGRErr OGROSMLayer::SetAttributeFilter( const char* pszAttrQuery )
{
    if( pszAttrQuery == nullptr && m_pszAttrQueryString == nullptr )
        return OGRERR_NONE;
    if( pszAttrQuery != nullptr && m_pszAttrQueryString != nullptr &&
        strcmp(pszAttrQuery, m_pszAttrQueryString) == 0 )
        return OGRERR_NONE;

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszAttrQuery);
    if( eErr != OGRERR_NONE )
        return eErr;

    if( nFeatureArrayIndex == 0 )
    {
        if( !poDS->IsInterleavedReading() )
        {
            poDS->MyResetReading();
        }
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "The new attribute filter will "
                  "not be taken into account immediately. It is advised to "
                  "set attribute filters for all needed layers, before "
                  "reading *any* layer" );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGROSMLayer::GetFeatureCount( int bForce )
{
    if( poDS->IsFeatureCountEnabled() )
        return OGRLayer::GetFeatureCount(bForce);

    return -1;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGROSMLayer::GetNextFeature()
{
    OGROSMLayer* poNewCurLayer = nullptr;
    OGRFeature* poFeature = MyGetNextFeature(&poNewCurLayer, nullptr, nullptr);
    poDS->SetCurrentLayer(poNewCurLayer);
    return poFeature;
}

OGRFeature *OGROSMLayer::MyGetNextFeature( OGROSMLayer** ppoNewCurLayer,
                                           GDALProgressFunc pfnProgress,
                                           void* pProgressData )
{
    *ppoNewCurLayer = poDS->GetCurrentLayer();
    bResetReadingAllowed = true;

    if( nFeatureArraySize == 0 )
    {
        if( poDS->IsInterleavedReading() )
        {
            if( *ppoNewCurLayer  == nullptr )
            {
                 *ppoNewCurLayer = this;
            }
            else if( *ppoNewCurLayer  != this )
            {
                return nullptr;
            }

            // If too many features have been accumulated in another layer, we
            // force a switch to that layer, so that it gets emptied.
            for( int i = 0; i < poDS->GetLayerCount(); i++ )
            {
                if( poDS->papoLayers[i] != this &&
                    poDS->papoLayers[i]->nFeatureArraySize > SWITCH_THRESHOLD )
                {
                    *ppoNewCurLayer = poDS->papoLayers[i];
                    CPLDebug("OSM", "Switching to '%s' as they are too many "
                                    "features in '%s'",
                             poDS->papoLayers[i]->GetName(),
                             GetName());
                    return nullptr;
                }
            }

            // Read some more data and accumulate features.
            poDS->ParseNextChunk(nIdxLayer, pfnProgress, pProgressData);

            if( nFeatureArraySize == 0 )
            {
                // If there are really no more features to read in the
                // current layer, force a switch to another non-empty layer.

                for( int i = 0; i < poDS->GetLayerCount(); i++ )
                {
                    if( poDS->papoLayers[i] != this &&
                        poDS->papoLayers[i]->nFeatureArraySize > 0 )
                    {
                        *ppoNewCurLayer = poDS->papoLayers[i];
                        CPLDebug("OSM",
                                 "Switching to '%s' as they are "
                                 "no more feature in '%s'",
                                 poDS->papoLayers[i]->GetName(),
                                 GetName());
                        return nullptr;
                    }
                }

                /* Game over : no more data to read from the stream */
                *ppoNewCurLayer = nullptr;
                return nullptr;
            }
        }
        else
        {
            while( true )
            {
                int bRet = poDS->ParseNextChunk(nIdxLayer, nullptr, nullptr);
                if( nFeatureArraySize != 0 )
                    break;
                if( bRet == FALSE )
                    return nullptr;
            }
        }
    }

    OGRFeature* poFeature = papoFeatures[nFeatureArrayIndex];

    papoFeatures[nFeatureArrayIndex] = nullptr;
    nFeatureArrayIndex++;

    if( nFeatureArrayIndex == nFeatureArraySize )
        nFeatureArrayIndex = nFeatureArraySize = 0;

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROSMLayer::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap, OLCFastGetExtent) )
    {
        OGREnvelope sExtent;
        if( poDS->GetExtent(&sExtent) == OGRERR_NONE )
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                             AddToArray()                             */
/************************************************************************/

bool OGROSMLayer::AddToArray( OGRFeature* poFeature,
                              int bCheckFeatureThreshold )
{
    if( bCheckFeatureThreshold && nFeatureArraySize > MAX_THRESHOLD )
    {
        if( !bHasWarnedTooManyFeatures )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Too many features have accumulated in %s layer. "
                    "Use the OGR_INTERLEAVED_READING=YES configuration option, "
                    "or the INTERLEAVED_READING=YES open option, or the "
                    "GDALDataset::GetNextFeature() / GDALDatasetGetNextFeature() API.",
                    GetName());
        }
        bHasWarnedTooManyFeatures = true;
        return false;
    }

    if( nFeatureArraySize == nFeatureArrayMaxSize )
    {
        nFeatureArrayMaxSize =
            nFeatureArrayMaxSize + nFeatureArrayMaxSize / 2 + 128;
        CPLDebug("OSM",
                 "For layer %s, new max size is %d",
                 GetName(), nFeatureArrayMaxSize);
        OGRFeature** papoNewFeatures = static_cast<OGRFeature **>(
            VSI_REALLOC_VERBOSE(papoFeatures,
                                nFeatureArrayMaxSize * sizeof(OGRFeature*)));
        if( papoNewFeatures == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "For layer %s, cannot resize feature array to %d features",
                     GetName(), nFeatureArrayMaxSize);
            return false;
        }
        papoFeatures = papoNewFeatures;
    }
    papoFeatures[nFeatureArraySize ++] = poFeature;

    return true;
}

/************************************************************************/
/*                        EvaluateAttributeFilter()                     */
/************************************************************************/

int OGROSMLayer::EvaluateAttributeFilter(OGRFeature* poFeature)
{
    return (m_poAttrQuery == nullptr
            || m_poAttrQuery->Evaluate( poFeature ));
}

/************************************************************************/
/*                             AddFeature()                             */
/************************************************************************/

int  OGROSMLayer::AddFeature(OGRFeature* poFeature,
                             int bAttrFilterAlreadyEvaluated,
                             int* pbFilteredOut,
                             int bCheckFeatureThreshold)
{
    if( !bUserInterested )
    {
        if( pbFilteredOut )
            *pbFilteredOut = TRUE;
        delete poFeature;
        return TRUE;
    }

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if( poGeom )
        poGeom->assignSpatialReference( poSRS );

    if( (m_poFilterGeom == nullptr
        || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == nullptr || bAttrFilterAlreadyEvaluated
            || m_poAttrQuery->Evaluate( poFeature )) )
    {
        if( !AddToArray(poFeature, bCheckFeatureThreshold) )
        {
            delete poFeature;
            return FALSE;
        }
    }
    else
    {
        if( pbFilteredOut )
            *pbFilteredOut = TRUE;
        delete poFeature;
        return TRUE;
    }

    if( pbFilteredOut )
        *pbFilteredOut = FALSE;
    return TRUE;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGROSMLayer::GetExtent( OGREnvelope *psExtent,
                               int /* bForce */ )
{
    if( poDS->GetExtent(psExtent) == OGRERR_NONE )
        return OGRERR_NONE;

    /* return OGRLayer::GetExtent(psExtent, bForce);*/
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          GetLaunderedFieldName()                     */
/************************************************************************/

const char* OGROSMLayer::GetLaunderedFieldName( const char* pszName )
{
    if( poDS->DoesAttributeNameLaundering()  &&
        strchr(pszName, ':') != nullptr )
    {
        size_t i = 0;
        for( ;
             i < sizeof(szLaunderedFieldName) - 1 && pszName[i] != '\0';
             i++ )
        {
            if( pszName[i] == ':' )
                szLaunderedFieldName[i] = '_';
            else
                szLaunderedFieldName[i] = pszName[i];
        }
        szLaunderedFieldName[i] = '\0';
        return szLaunderedFieldName;
    }

    return pszName;
}

/************************************************************************/
/*                              AddField()                              */
/************************************************************************/

void OGROSMLayer::AddField( const char* pszName, OGRFieldType eFieldType )
{
    const char* pszLaunderedName = GetLaunderedFieldName(pszName);
    OGRFieldDefn oField(pszLaunderedName, eFieldType);
    poFeatureDefn->AddFieldDefn(&oField);

    int nIndex = poFeatureDefn->GetFieldCount() - 1;
    char* pszDupName = CPLStrdup(pszName);
    apszNames.push_back(pszDupName);
    oMapFieldNameToIndex[pszDupName] = nIndex;

    if( strcmp(pszName, "osm_id") == 0 )
        nIndexOSMId = nIndex;

    else if( strcmp(pszName, "osm_way_id") == 0 )
        nIndexOSMWayId = nIndex;

    else if( strcmp(pszName, "other_tags") == 0 )
        nIndexOtherTags = nIndex;

    else if( strcmp(pszName, "all_tags") == 0 )
        nIndexAllTags = nIndex;
}

/************************************************************************/
/*                              GetFieldIndex()                         */
/************************************************************************/

int OGROSMLayer::GetFieldIndex( const char* pszName )
{
    std::map<const char*, int, ConstCharComp>::iterator oIter =
        oMapFieldNameToIndex.find(pszName);
    if( oIter != oMapFieldNameToIndex.end() )
        return oIter->second;

    return -1;
}

/************************************************************************/
/*                         AddInOtherOrAllTags()                        */
/************************************************************************/

int OGROSMLayer::AddInOtherOrAllTags( const char* pszK )
{
    bool bAddToOtherTags = false;

    if( aoSetIgnoreKeys.find(pszK) == aoSetIgnoreKeys.end() )
    {
        char* pszColon = strchr((char*) pszK, ':');
        if( pszColon )
        {
            char chBackup = pszColon[1];
            pszColon[1] = '\0';  /* Evil but OK */
            bAddToOtherTags = ( aoSetIgnoreKeys.find(pszK) ==
                                aoSetIgnoreKeys.end() );
            // cppcheck-suppress redundantAssignment
            pszColon[1] = chBackup;
        }
        else
            bAddToOtherTags = true;
    }

    return bAddToOtherTags;
}

/************************************************************************/
/*                        OGROSMFormatForHSTORE()                       */
/************************************************************************/

static int OGROSMFormatForHSTORE( const char* pszV, char* pszAllTags )
{
    int nAllTagsOff = 0;

    pszAllTags[nAllTagsOff++] = '"';

    for( int k=0; pszV[k] != '\0'; k++ )
    {
        if( pszV[k] == '"' || pszV[k] == '\\' )
            pszAllTags[nAllTagsOff++] = '\\';
        pszAllTags[nAllTagsOff++] = pszV[k];
    }

    pszAllTags[nAllTagsOff++] = '"';

    return nAllTagsOff;
}

/************************************************************************/
/*                            GetValueOfTag()                           */
/************************************************************************/

static const char* GetValueOfTag(const char* pszKeyToSearch,
                                 unsigned int nTags, const OSMTag* pasTags)
{
    for(unsigned int k = 0; k < nTags; k++)
    {
        const char* pszK = pasTags[k].pszK;
        if( strcmp(pszK, pszKeyToSearch) == 0 )
        {
            return pasTags[k].pszV;
        }
    }
    return nullptr;
}

/************************************************************************/
/*                        SetFieldsFromTags()                           */
/************************************************************************/

void OGROSMLayer::SetFieldsFromTags(OGRFeature* poFeature,
                                    GIntBig nID,
                                    bool bIsWayID,
                                    unsigned int nTags, OSMTag* pasTags,
                                    OSMInfo* psInfo)
{
    if( !bIsWayID )
    {
        poFeature->SetFID( nID );

        if( bHasOSMId )
        {
            char szID[32];
            snprintf(szID, sizeof(szID), CPL_FRMT_GIB, nID );
            poFeature->SetField(nIndexOSMId, szID);
        }
    }
    else
    {
        poFeature->SetFID( nID );

        if( nIndexOSMWayId >= 0 )
        {
            char szID[32];
            snprintf(szID, sizeof(szID), CPL_FRMT_GIB, nID );
            poFeature->SetField(nIndexOSMWayId, szID );
        }
    }

    if( bHasVersion )
    {
        poFeature->SetField("osm_version", psInfo->nVersion);
    }
    if( bHasTimestamp )
    {
        if( psInfo->bTimeStampIsStr )
        {
            OGRField sField;
            if( OGRParseXMLDateTime(psInfo->ts.pszTimeStamp, &sField) )
            {
                poFeature->SetField("osm_timestamp", &sField);
            }
        }
        else
        {
            struct tm brokendown;
            CPLUnixTimeToYMDHMS(psInfo->ts.nTimeStamp, &brokendown);
            poFeature->SetField("osm_timestamp",
                                brokendown.tm_year + 1900,
                                brokendown.tm_mon + 1,
                                brokendown.tm_mday,
                                brokendown.tm_hour,
                                brokendown.tm_min,
                                static_cast<float>(brokendown.tm_sec),
                                0);
        }
    }
    if( bHasUID )
    {
        poFeature->SetField("osm_uid", psInfo->nUID);
    }
    if( bHasUser )
    {
        poFeature->SetField("osm_user", psInfo->pszUserSID);
    }
    if( bHasChangeset )
    {
        poFeature->SetField("osm_changeset", (int) psInfo->nChangeset);
    }

    int nAllTagsOff = 0;
    for(unsigned int j = 0; j < nTags; j++)
    {
        const char* pszK = pasTags[j].pszK;
        const char* pszV = pasTags[j].pszV;
        int nIndex = GetFieldIndex(pszK);
        if( nIndex >= 0 && nIndex != nIndexOSMId )
        {
            poFeature->SetField(nIndex, pszV);
            if( nIndexAllTags < 0 )
                continue;
        }
        if( nIndexAllTags >= 0 || nIndexOtherTags >= 0 )
        {
            if( AddInOtherOrAllTags(pszK) )
            {
                int nLenK = (int)strlen(pszK);
                int nLenV = (int)strlen(pszV);
                if( nAllTagsOff +
                    1 + 2 * nLenK + 1 +
                    2 +
                    1 + 2 * nLenV + 1 +
                    1 >= ALLTAGS_LENGTH - 1 )
                {
                    if( !bHasWarnedAllTagsTruncated )
                        CPLDebug( "OSM",
                                  "all_tags/other_tags field truncated for "
                                  "feature " CPL_FRMT_GIB, nID);
                    bHasWarnedAllTagsTruncated = true;
                    continue;
                }

                if( nAllTagsOff )
                    pszAllTags[nAllTagsOff++] = ',';

                nAllTagsOff += OGROSMFormatForHSTORE(pszK,
                                                     pszAllTags + nAllTagsOff);

                pszAllTags[nAllTagsOff++] = '=';
                pszAllTags[nAllTagsOff++] = '>';

                nAllTagsOff += OGROSMFormatForHSTORE(pszV,
                                                     pszAllTags + nAllTagsOff);
            }

#ifdef notdef
            if( aoSetWarnKeys.find(pszK) == aoSetWarnKeys.end() )
            {
                aoSetWarnKeys.insert(pszK);
                CPLDebug("OSM_KEY", "Ignored key : %s", pszK);
            }
#endif
        }
    }

    if( nAllTagsOff )
    {
        pszAllTags[nAllTagsOff] = '\0';
        if( nIndexAllTags >= 0 )
            poFeature->SetField(nIndexAllTags, pszAllTags);
        else
            poFeature->SetField(nIndexOtherTags, pszAllTags);
    }

    for(size_t i=0; i<oComputedAttributes.size();i++)
    {
        const OGROSMComputedAttribute& oAttr = oComputedAttributes[i];
        if( oAttr.bHardcodedZOrder )
        {
            const int nHighwayIdx = oAttr.anIndexToBind[0];
            const int nBridgeIdx = oAttr.anIndexToBind[1];
            const int nTunnelIdx = oAttr.anIndexToBind[2];
            const int nRailwayIdx = oAttr.anIndexToBind[3];
            const int nLayerIdx = oAttr.anIndexToBind[4];

            int nZOrder = 0;
    /*
        "SELECT (CASE [highway] WHEN 'minor' THEN 3 WHEN 'road' THEN 3 "
        "WHEN 'unclassified' THEN 3 WHEN 'residential' THEN 3 WHEN "
        "'tertiary_link' THEN 4 WHEN 'tertiary' THEN 4 WHEN 'secondary_link' "
        "THEN 6 WHEN 'secondary' THEN 6 WHEN 'primary_link' THEN 7 WHEN "
        "'primary' THEN 7 WHEN 'trunk_link' THEN 8 WHEN 'trunk' THEN 8 "
        "WHEN 'motorway_link' THEN 9 WHEN 'motorway' THEN 9 ELSE 0 END) + "
        "(CASE WHEN [bridge] IN ('yes', 'true', '1') THEN 10 ELSE 0 END) + "
        "(CASE WHEN [tunnel] IN ('yes', 'true', '1') THEN -10 ELSE 0 END) + "
        "(CASE WHEN [railway] IS NOT NULL THEN 5 ELSE 0 END) + "
        "(CASE WHEN [layer] IS NOT NULL THEN 10 * CAST([layer] AS INTEGER) " */

            const char* pszHighway = nullptr;
            if( nHighwayIdx >= 0)
            {
                if( poFeature->IsFieldSetAndNotNull(nHighwayIdx) )
                {
                    pszHighway = poFeature->GetFieldAsString(nHighwayIdx);
                }
            }
            else
                pszHighway = GetValueOfTag("highway", nTags, pasTags);
            if( pszHighway )
            {
                if( strcmp(pszHighway, "minor") == 0 ||
                    strcmp(pszHighway, "road") == 0 ||
                    strcmp(pszHighway, "unclassified") == 0 ||
                    strcmp(pszHighway, "residential") == 0 )
                {
                    nZOrder += 3;
                }
                else if( strcmp(pszHighway, "tertiary_link") == 0 ||
                         strcmp(pszHighway, "tertiary") == 0 )
                {
                    nZOrder += 4;
                }
                else if( strcmp(pszHighway, "secondary_link") == 0 ||
                         strcmp(pszHighway, "secondary") == 0 )
                {
                    nZOrder += 6;
                }
                else if( strcmp(pszHighway, "primary_link") == 0 ||
                         strcmp(pszHighway, "primary") == 0 )
                {
                    nZOrder += 7;
                }
                else if( strcmp(pszHighway, "trunk_link") == 0 ||
                         strcmp(pszHighway, "trunk") == 0 )
                {
                    nZOrder += 8;
                }
                else if( strcmp(pszHighway, "motorway_link") == 0 ||
                         strcmp(pszHighway, "motorway") == 0 )
                {
                    nZOrder += 9;
                }
            }

            const char* pszBridge = nullptr;
            if( nBridgeIdx >= 0)
            {
                if( poFeature->IsFieldSetAndNotNull(nBridgeIdx) )
                {
                    pszBridge = poFeature->GetFieldAsString(nBridgeIdx);
                }
            }
            else
                pszBridge = GetValueOfTag("bridge", nTags, pasTags);
            if( pszBridge )
            {
                if( strcmp(pszBridge, "yes") == 0 ||
                    strcmp(pszBridge, "true") == 0 ||
                    strcmp(pszBridge, "1") == 0 )
                {
                    nZOrder += 10;
                }
            }

            const char* pszTunnel = nullptr;
            if( nTunnelIdx >= 0)
            {
                if( poFeature->IsFieldSetAndNotNull(nTunnelIdx) )
                {
                    pszTunnel = poFeature->GetFieldAsString(nTunnelIdx);
                }
            }
            else
                pszTunnel = GetValueOfTag("tunnel", nTags, pasTags);
            if( pszTunnel )
            {
                if( strcmp(pszTunnel, "yes") == 0 ||
                    strcmp(pszTunnel, "true") == 0 ||
                    strcmp(pszTunnel, "1") == 0 )
                {
                    nZOrder -= 10;
                }
            }

            const char* pszRailway = nullptr;
            if( nRailwayIdx >= 0 )
            {
                if( poFeature->IsFieldSetAndNotNull(nRailwayIdx) )
                {
                    pszRailway = poFeature->GetFieldAsString(nRailwayIdx);
                }
            }
            else
                pszRailway = GetValueOfTag("railway", nTags, pasTags);
            if( pszRailway )
            {
                nZOrder += 5;
            }

            const char* pszLayer = nullptr;
            if( nLayerIdx >= 0 )
            {
                if( poFeature->IsFieldSetAndNotNull(nLayerIdx) )
                {
                    pszLayer = poFeature->GetFieldAsString(nLayerIdx);
                }
            }
            else
                pszLayer = GetValueOfTag("layer", nTags, pasTags);
            if( pszLayer )
            {
                nZOrder += 10 * atoi(pszLayer);
            }

            poFeature->SetField( oAttr.nIndex, nZOrder);

            continue;
        }

        for(int j=0;j<static_cast<int>(oAttr.anIndexToBind.size());j++)
        {
            if( oAttr.anIndexToBind[j] >= 0 )
            {
                if( !poFeature->IsFieldSetAndNotNull(oAttr.anIndexToBind[j]) )
                {
                    sqlite3_bind_null( oAttr.hStmt, j + 1 );
                }
                else
                {
                    OGRFieldType eType =
                        poFeatureDefn->GetFieldDefn(oAttr.anIndexToBind[j])->
                          GetType();
                    if( eType == OFTInteger )
                        sqlite3_bind_int( oAttr.hStmt, j + 1,
                                          poFeature->GetFieldAsInteger(oAttr.anIndexToBind[j]) );
                    else if( eType == OFTInteger64 )
                        sqlite3_bind_int64( oAttr.hStmt, j + 1,
                                          poFeature->GetFieldAsInteger64(oAttr.anIndexToBind[j]) );
                    else if( eType == OFTReal )
                        sqlite3_bind_double( oAttr.hStmt, j + 1,
                                             poFeature->GetFieldAsDouble(oAttr.anIndexToBind[j]) );
                    else
                        sqlite3_bind_text( oAttr.hStmt, j + 1,
                                        poFeature->GetFieldAsString(oAttr.anIndexToBind[j]),
                                        -1, SQLITE_TRANSIENT);
                }
            }
            else
            {
                bool bTagFound = false;
                for(unsigned int k = 0; k < nTags; k++)
                {
                    const char* pszK = pasTags[k].pszK;
                    const char* pszV = pasTags[k].pszV;
                    if( strcmp(pszK, oAttr.aosAttrToBind[j]) == 0 )
                    {
                        sqlite3_bind_text( oAttr.hStmt, j + 1, pszV, -1, SQLITE_TRANSIENT);
                        bTagFound = true;
                        break;
                    }
                }
                if( !bTagFound )
                    sqlite3_bind_null( oAttr.hStmt, j + 1 );
            }
        }

        if( sqlite3_step( oAttr.hStmt ) == SQLITE_ROW &&
            sqlite3_column_count( oAttr.hStmt ) == 1 )
        {
            switch( sqlite3_column_type( oAttr.hStmt, 0 ) )
            {
                case SQLITE_INTEGER:
                    poFeature->SetField( oAttr.nIndex,
                            (GIntBig)sqlite3_column_int64(oAttr.hStmt, 0) );
                    break;
                case SQLITE_FLOAT:
                    poFeature->SetField( oAttr.nIndex,
                            sqlite3_column_double(oAttr.hStmt, 0) );
                    break;
                case SQLITE_TEXT:
                    poFeature->SetField( oAttr.nIndex,
                            (const char*)sqlite3_column_text(oAttr.hStmt, 0) );
                    break;
                default:
                    break;
            }
        }

        sqlite3_reset( oAttr.hStmt );
    }
}

/************************************************************************/
/*                      GetSpatialFilterEnvelope()                      */
/************************************************************************/

const OGREnvelope* OGROSMLayer::GetSpatialFilterEnvelope()
{
    if( m_poFilterGeom != nullptr )
        return &m_sFilterEnvelope;
    else
        return nullptr;
}

/************************************************************************/
/*                        AddInsignificantKey()                         */
/************************************************************************/

void OGROSMLayer::AddInsignificantKey( const char* pszK )
{
    char* pszKDup = CPLStrdup(pszK);
    apszInsignificantKeys.push_back(pszKDup);
    aoSetInsignificantKeys[pszKDup] = 1;
}

/************************************************************************/
/*                          AddIgnoreKey()                              */
/************************************************************************/

void OGROSMLayer::AddIgnoreKey( const char* pszK )
{
    char* pszKDup = CPLStrdup(pszK);
    apszIgnoreKeys.push_back(pszKDup);
    aoSetIgnoreKeys[pszKDup] = 1;
}

/************************************************************************/
/*                           AddWarnKey()                               */
/************************************************************************/

void OGROSMLayer::AddWarnKey( const char* pszK )
{
    aoSetWarnKeys.insert(pszK);
}

/************************************************************************/
/*                           AddWarnKey()                               */
/************************************************************************/

void OGROSMLayer::AddComputedAttribute( const char* pszName,
                                        OGRFieldType eType,
                                        const char* pszSQL )
{
    if( poDS->hDBForComputedAttributes == nullptr )
    {
        const int rc =
            sqlite3_open_v2(
                ":memory:", &(poDS->hDBForComputedAttributes),
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                SQLITE_OPEN_NOMUTEX,
                nullptr );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot open temporary sqlite DB" );
            return;
        }
    }

    if( poFeatureDefn->GetFieldIndex(pszName) >= 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "A field with same name %s already exists", pszName );
        return;
    }

    CPLString osSQL(pszSQL);
    const bool bHardcodedZOrder = (eType == OFTInteger) &&
      strcmp(pszSQL,
        "SELECT (CASE [highway] WHEN 'minor' THEN 3 WHEN 'road' THEN 3 "
        "WHEN 'unclassified' THEN 3 WHEN 'residential' THEN 3 WHEN "
        "'tertiary_link' THEN 4 WHEN 'tertiary' THEN 4 WHEN 'secondary_link' "
        "THEN 6 WHEN 'secondary' THEN 6 WHEN 'primary_link' THEN 7 WHEN "
        "'primary' THEN 7 WHEN 'trunk_link' THEN 8 WHEN 'trunk' THEN 8 "
        "WHEN 'motorway_link' THEN 9 WHEN 'motorway' THEN 9 ELSE 0 END) + "
        "(CASE WHEN [bridge] IN ('yes', 'true', '1') THEN 10 ELSE 0 END) + "
        "(CASE WHEN [tunnel] IN ('yes', 'true', '1') THEN -10 ELSE 0 END) + "
        "(CASE WHEN [railway] IS NOT NULL THEN 5 ELSE 0 END) + "
        "(CASE WHEN [layer] IS NOT NULL THEN 10 * CAST([layer] AS INTEGER) "
        "ELSE 0 END)") == 0;
    std::vector<CPLString> aosAttrToBind;
    std::vector<int> anIndexToBind;
    size_t nStartSearch = 0;
    while( true )
    {
        size_t nPos = osSQL.find("[", nStartSearch);
        if( nPos == std::string::npos )
            break;
        nStartSearch = nPos + 1;
        if( nPos > 0 && osSQL[nPos-1] != '\\' )
        {
            CPLString osAttr = osSQL.substr(nPos + 1);
            size_t nPos2 = osAttr.find("]");
            if( nPos2 == std::string::npos )
                break;
            osAttr.resize(nPos2);

            osSQL = osSQL.substr(0, nPos) + "?" + osSQL.substr(nPos + 1 + nPos2+1);

            aosAttrToBind.push_back(osAttr);
            anIndexToBind.push_back(poFeatureDefn->GetFieldIndex(osAttr));
        }
    }
    while( true )
    {
        size_t nPos = osSQL.find("\\");
        if( nPos == std::string::npos || nPos == osSQL.size() - 1 )
            break;
        osSQL = osSQL.substr(0, nPos) + osSQL.substr(nPos + 1);
    }

    CPLDebug("OSM", "SQL : \"%s\"", osSQL.c_str());

    sqlite3_stmt *hStmt = nullptr;
    int rc = sqlite3_prepare_v2( poDS->hDBForComputedAttributes, osSQL, -1,
                              &hStmt, nullptr );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare_v2() failed :  %s",
                  sqlite3_errmsg(poDS->hDBForComputedAttributes) );
        return;
    }

    OGRFieldDefn oField(pszName, eType);
    poFeatureDefn->AddFieldDefn(&oField);
    oComputedAttributes.push_back(OGROSMComputedAttribute(pszName));
    OGROSMComputedAttribute& oComputedAttribute = oComputedAttributes.back();
    oComputedAttribute.eType = eType;
    oComputedAttribute.nIndex = poFeatureDefn->GetFieldCount() - 1;
    oComputedAttribute.osSQL = pszSQL;
    oComputedAttribute.hStmt = hStmt;
    oComputedAttribute.aosAttrToBind = aosAttrToBind;
    oComputedAttribute.anIndexToBind = anIndexToBind;
    oComputedAttribute.bHardcodedZOrder = bHardcodedZOrder;
}
