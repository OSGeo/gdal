/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGROSMLayer class
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
#include "cpl_time.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

#define SWITCH_THRESHOLD   10000
#define MAX_THRESHOLD      100000

#define ALLTAGS_LENGTH     8192

/************************************************************************/
/*                          OGROSMLayer()                               */
/************************************************************************/


OGROSMLayer::OGROSMLayer(OGROSMDataSource* poDS, const char* pszName )
{
    this->poDS = poDS;

    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();

    poSRS = new OGRSpatialReference();
    poSRS->SetWellKnownGeogCS("WGS84");

    nFeatureArraySize = 0;
    nFeatureArrayMaxSize = 0;
    nFeatureArrayIndex = 0;
    papoFeatures = NULL;
    
    nFeatureCount = 0;

    bHasOSMId = FALSE;
    nIndexOSMId = -1;
    nIndexOSMWayId = -1;
    bHasVersion = FALSE;
    bHasTimestamp = FALSE;
    bHasUID = FALSE;
    bHasUser = FALSE;
    bHasChangeset = FALSE;
    bHasOtherTags = TRUE;

    bResetReadingAllowed = FALSE;
    bHasWarnedTooManyFeatures = FALSE;

    pszAllTags = (char*)CPLMalloc(ALLTAGS_LENGTH);
    bHasWarnedAllTagsTruncated = FALSE;

    bUserInterested = TRUE;
}

/************************************************************************/
/*                          ~OGROSMLayer()                           */
/************************************************************************/

OGROSMLayer::~OGROSMLayer()
{
    int i;
    
    poFeatureDefn->Release();
    
    if (poSRS)
        poSRS->Release();

    for(i=0;i<nFeatureArraySize;i++)
    {
        if (papoFeatures[i])
            delete papoFeatures[i];
    }

    for(i=0;i<(int)apszNames.size();i++)
        CPLFree(apszNames[i]);

    for(i=0;i<(int)apszUnsignificantKeys.size();i++)
        CPLFree(apszUnsignificantKeys[i]);

    for(i=0;i<(int)apszIgnoreKeys.size();i++)
        CPLFree(apszIgnoreKeys[i]);
    
    CPLFree(pszAllTags);

    CPLFree(papoFeatures);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROSMLayer::ResetReading()
{
    if ( !bResetReadingAllowed || poDS->IsInterleavedReading() )
        return;

    poDS->ResetReading();
}

/************************************************************************/
/*                        ForceResetReading()                           */
/************************************************************************/

void OGROSMLayer::ForceResetReading()
{
    for(int i=0;i<nFeatureArraySize;i++)
    {
        if (papoFeatures[i])
            delete papoFeatures[i];
    }
    nFeatureArrayIndex = 0;
    nFeatureArraySize = 0;
    nFeatureCount = 0;
    bResetReadingAllowed = FALSE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGROSMLayer::GetFeatureCount( int bForce )
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
    bResetReadingAllowed = TRUE;

    if ( nFeatureArraySize == 0)
    {
        if ( poDS->IsInterleavedReading() )
        {
            int i;

            OGRLayer* poCurrentLayer = poDS->GetCurrentLayer();
            if ( poCurrentLayer == NULL )
            {
                poDS->SetCurrentLayer(this);
            }
            else if( poCurrentLayer != this )
            {
                return NULL;
            }

            /* If too many features have been accumulated in */
            /* another layer, we force */
            /* a switch to that layer, so that it gets emptied */
            for(i=0;i<poDS->GetLayerCount();i++)
            {
                if (poDS->papoLayers[i] != this &&
                    poDS->papoLayers[i]->nFeatureArraySize > SWITCH_THRESHOLD)
                {
                    poDS->SetCurrentLayer(poDS->papoLayers[i]);
                    CPLDebug("OSM", "Switching to '%s' as they are too many "
                                    "features in '%s'",
                             poDS->papoLayers[i]->GetName(),
                             GetName());
                    return NULL;
                }
            }

            /* Read some more data and accumulate features */
            poDS->ParseNextChunk();

            if ( nFeatureArraySize == 0 )
            {
                /* If there are really no more features to read in the */
                /* current layer, force a switch to another non-empty layer */

                for(i=0;i<poDS->GetLayerCount();i++)
                {
                    if (poDS->papoLayers[i] != this &&
                        poDS->papoLayers[i]->nFeatureArraySize > 0)
                    {
                        poDS->SetCurrentLayer(poDS->papoLayers[i]);
                        CPLDebug("OSM",
                                 "Switching to '%s' as they are no more feature in '%s'",
                                 poDS->papoLayers[i]->GetName(),
                                 GetName());
                        return NULL;
                    }
                }

                /* Game over : no more data to read from the stream */
                poDS->SetCurrentLayer(NULL);
                return NULL;
            }
        }
        else
        {
            while(TRUE)
            {
                int bRet = poDS->ParseNextChunk();
                if (nFeatureArraySize != 0)
                    break;
                if (bRet == FALSE)
                    return NULL;
            }
        }
    }

    OGRFeature* poFeature = papoFeatures[nFeatureArrayIndex];
    
    papoFeatures[nFeatureArrayIndex] = NULL;
    nFeatureArrayIndex++;

    if ( nFeatureArrayIndex == nFeatureArraySize)
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
        if (poDS->GetExtent(&sExtent) == OGRERR_NONE)
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                             AddToArray()                             */
/************************************************************************/

int  OGROSMLayer::AddToArray(OGRFeature* poFeature)
{
    if( nFeatureArraySize > MAX_THRESHOLD)
    {
        if( !bHasWarnedTooManyFeatures )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Too many features have accumulated in %s layer. "
                    "Use OGR_INTERLEAVED_READING=YES mode",
                    GetName());
        }
        bHasWarnedTooManyFeatures = TRUE;
        return FALSE;
    }

    if (nFeatureArraySize == nFeatureArrayMaxSize)
    {
        nFeatureArrayMaxSize = nFeatureArrayMaxSize + nFeatureArrayMaxSize / 2 + 128;
        CPLDebug("OSM", "For layer %s, new max size is %d", GetName(), nFeatureArrayMaxSize);
        OGRFeature** papoNewFeatures = (OGRFeature**)VSIRealloc(papoFeatures,
                                nFeatureArrayMaxSize * sizeof(OGRFeature*));
        if (papoNewFeatures == NULL)
        {
            delete poFeature;
            return FALSE;
        }
        papoFeatures = papoNewFeatures;
    }
    papoFeatures[nFeatureArraySize ++] = poFeature;
    
    return TRUE;
}

/************************************************************************/
/*                        EvaluateAttributeFilter()                     */
/************************************************************************/

int OGROSMLayer::EvaluateAttributeFilter(OGRFeature* poFeature)
{
    return (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature ));
}

/************************************************************************/
/*                             AddFeature()                             */
/************************************************************************/

int  OGROSMLayer::AddFeature(OGRFeature* poFeature,
                             int bAttrFilterAlreadyEvaluated,
                             int* pbFilteredOut)
{
    if( !bUserInterested )
    {
        if (pbFilteredOut)
            *pbFilteredOut = TRUE;
        delete poFeature;
        return TRUE;
    }

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if (poGeom)
        poGeom->assignSpatialReference( poSRS );
    
    if( (m_poFilterGeom == NULL
        || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL || bAttrFilterAlreadyEvaluated
            || m_poAttrQuery->Evaluate( poFeature )) )
    {
        if (!AddToArray(poFeature))
        {
            delete poFeature;
            return FALSE;
        }
    }
    else
    {
        if (pbFilteredOut)
            *pbFilteredOut = TRUE;
        delete poFeature;
        return TRUE;
    }
    
    if (pbFilteredOut)
        *pbFilteredOut = FALSE;
    return TRUE;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGROSMLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if (poDS->GetExtent(psExtent) == OGRERR_NONE)
        return OGRERR_NONE;

    /* return OGRLayer::GetExtent(psExtent, bForce);*/
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          GetLaunderedFieldName()                     */
/************************************************************************/

const char* OGROSMLayer::GetLaunderedFieldName(const char* pszName)
{
    if( poDS->DoesAttributeNameLaundering()  &&
        strchr(pszName, ':') != NULL )
    {
        size_t i;
        for( i = 0;
             pszName[i] != '\0' && i < sizeof(szLaunderedFieldName) - 1; i++ )
        {
            if( pszName[i] == ':' )
                szLaunderedFieldName[i] = '_';
            else
                szLaunderedFieldName[i] = pszName[i];
        }
        szLaunderedFieldName[i] = '\0';
        return szLaunderedFieldName;
    }
    else
        return pszName;
}

/************************************************************************/
/*                              AddField()                              */
/************************************************************************/

void OGROSMLayer::AddField(const char* pszName, OGRFieldType eFieldType)
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
}

/************************************************************************/
/*                              GetFieldIndex()                         */
/************************************************************************/

int OGROSMLayer::GetFieldIndex(const char* pszName)
{
    std::map<const char*, int, ConstCharComp>::iterator oIter =
        oMapFieldNameToIndex.find(pszName);
    if( oIter != oMapFieldNameToIndex.end() )
        return oIter->second;
    else
        return -1;
}

/************************************************************************/
/*                              AddInOtherTag()                         */
/************************************************************************/

int OGROSMLayer::AddInOtherTag(const char* pszK)
{
    int bAddToOtherTags = FALSE;

    if ( aoSetIgnoreKeys.find(pszK) == aoSetIgnoreKeys.end() )
    {
        char* pszColon = strchr((char*) pszK, ':');
        if( pszColon )
        {
            char chBackup = pszColon[1];
            pszColon[1] = '\0';  /* Evil but OK */
            bAddToOtherTags = ( aoSetIgnoreKeys.find(pszK) ==
                                aoSetIgnoreKeys.end() );
            pszColon[1] = chBackup;
        }
        else
            bAddToOtherTags = TRUE;
    }

    return bAddToOtherTags;
}

/************************************************************************/
/*                        OGROSMFormatForHSTORE()                       */
/************************************************************************/

static int OGROSMFormatForHSTORE(const char* pszV, char* pszAllTags)
{
    int k;

    int nAllTagsOff = 0;

    pszAllTags[nAllTagsOff++] = '"';

    for(k=0;pszV[k] != '\0'; k++)
    {
        if( pszV[k] == '"' || pszV[k] == '\\' )
            pszAllTags[nAllTagsOff++] = '\\';
        pszAllTags[nAllTagsOff++] = pszV[k];
    }

    pszAllTags[nAllTagsOff++] = '"';

    return nAllTagsOff;
}

/************************************************************************/
/*                        SetFieldsFromTags()                           */
/************************************************************************/

void OGROSMLayer::SetFieldsFromTags(OGRFeature* poFeature,
                                    GIntBig nID,
                                    int bIsWayID,
                                    unsigned int nTags, OSMTag* pasTags,
                                    OSMInfo* psInfo)
{
    if( !bIsWayID )
    {
        if( (long)nID == nID )
            poFeature->SetFID( (long)nID ); /* Will not work with 32bit GDAL if id doesn't fit into 32 bits */

        if( bHasOSMId )
        {
            char szID[32];
            sprintf(szID, CPL_FRMT_GIB, nID );
            poFeature->SetField(nIndexOSMId, szID);
        }
    }
    else
    {
        if( (long)nID == nID )
            poFeature->SetFID( (long)nID ); /* Will not work with 32bit GDAL if id doesn't fit into 32 bits */

        if( nIndexOSMWayId >= 0 )
        {
            char szID[32];
            sprintf(szID, CPL_FRMT_GIB, nID );
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
            int year, month, day, hour, minute, TZ;
            float second;
            if (OGRParseXMLDateTime(psInfo->ts.pszTimeStamp, &year, &month, &day,
                                    &hour, &minute, &second, &TZ))
            {
                poFeature->SetField("osm_timestamp", year, month, day, hour,
                                    minute, (int)(second + .5), TZ);
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
                                brokendown.tm_sec,
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
        if( nIndex >= 0 )
            poFeature->SetField(nIndex, pszV);
        else if ( HasOtherTags() )
        {
            if ( AddInOtherTag(pszK) )
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
                        CPLDebug("OSM", "all_tags field truncated for feature " CPL_FRMT_GIB, nID);
                    bHasWarnedAllTagsTruncated = TRUE;
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
            if ( aoSetWarnKeys.find(pszK) ==
                 aoSetWarnKeys.end() )
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
        poFeature->SetField(GetLayerDefn()->GetFieldCount() - 1, pszAllTags);
    }
}

/************************************************************************/
/*                      GetSpatialFilterEnvelope()                      */
/************************************************************************/

const OGREnvelope* OGROSMLayer::GetSpatialFilterEnvelope()
{
    if( m_poFilterGeom != NULL )
        return &m_sFilterEnvelope;
    else
        return NULL;
}

/************************************************************************/
/*                        AddUnsignificantKey()                         */
/************************************************************************/

void OGROSMLayer::AddUnsignificantKey(const char* pszK)
{
    char* pszKDup = CPLStrdup(pszK);
    apszUnsignificantKeys.push_back(pszKDup);
    aoSetUnsignificantKeys[pszKDup] = 1;
}

/************************************************************************/
/*                          AddIgnoreKey()                              */
/************************************************************************/

void OGROSMLayer::AddIgnoreKey(const char* pszK)
{
    char* pszKDup = CPLStrdup(pszK);
    apszIgnoreKeys.push_back(pszKDup);
    aoSetIgnoreKeys[pszKDup] = 1;
}

/************************************************************************/
/*                           AddWarnKey()                               */
/************************************************************************/

void OGROSMLayer::AddWarnKey(const char* pszK)
{
    aoSetWarnKeys.insert(pszK);
}
