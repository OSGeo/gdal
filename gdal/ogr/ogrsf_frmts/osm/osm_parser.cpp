/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 * Purpose:  OSM XML and OSM PBF parser
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "osm_parser.h"
#include "gpb.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

#ifdef HAVE_EXPAT
#include "ogr_expat.h"
#endif

/* The buffer that are passed to GPB decoding are extended with 0's */
/* to be sure that we will be able to read a single 64bit value without */
/* doing checks for each byte */
#define EXTRA_BYTES     1

#define XML_BUFSIZE 64*1024

CPL_CVSID("$Id$");

/************************************************************************/
/*                            INIT_INFO()                               */
/************************************************************************/

#define INIT_INFO(sInfo) \
    sInfo.ts.nTimeStamp = 0; \
    sInfo.nChangeset = 0; \
    sInfo.nVersion = 0; \
    sInfo.nUID = 0; \
    sInfo.bTimeStampIsStr = 0; \
    sInfo.pszUserSID = NULL;
/*    \    sInfo.nVisible = 1; */


/************************************************************************/
/*                            _OSMContext                               */
/************************************************************************/

struct _OSMContext
{
    char          *pszStrBuf;
    int           *panStrOff;
    unsigned int   nStrCount;
    unsigned int   nStrAllocated;

    OSMNode       *pasNodes;
    unsigned int   nNodesAllocated;

    OSMTag        *pasTags;
    unsigned int   nTagsAllocated;

    OSMMember     *pasMembers;
    unsigned int   nMembersAllocated;

    GIntBig       *panNodeRefs;
    unsigned int   nNodeRefsAllocated;

    int            nGranularity;
    int            nDateGranularity;
    GIntBig        nLatOffset;
    GIntBig        nLonOffset;

    unsigned int   nBlobSizeAllocated;
    GByte         *pabyBlob;

    GByte         *pabyUncompressed;
    unsigned int   nUncompressedAllocated;

#ifdef HAVE_EXPAT
    XML_Parser     hXMLParser;
    int            bEOF;
    int            bStopParsing;
    int            bHasFoundFeature;
    int            nWithoutEventCounter;
    int            nDataHandlerCounter;

    unsigned int   nStrLength;
    unsigned int   nTags;

    int            bInNode;
    int            bInWay;
    int            bInRelation;

    OSMWay         sWay;
    OSMRelation    sRelation;

    int            bTryToFetchBounds;
#endif

    VSILFILE      *fp;

    int            bPBF;

    double         dfLeft;
    double         dfRight;
    double         dfTop;
    double         dfBottom;

    GUIntBig        nBytesRead;

    NotifyNodesFunc     pfnNotifyNodes;
    NotifyWayFunc       pfnNotifyWay;
    NotifyRelationFunc  pfnNotifyRelation;
    NotifyBoundsFunc    pfnNotifyBounds;
    void               *user_data;
};

/************************************************************************/
/*                          ReadBlobHeader()                            */
/************************************************************************/

#define BLOBHEADER_IDX_TYPE         1
#define BLOBHEADER_IDX_INDEXDATA    2
#define BLOBHEADER_IDX_DATASIZE     3

typedef enum
{
    BLOB_UNKNOW,
    BLOB_OSMHEADER,
    BLOB_OSMDATA
} BlobType;

static
int ReadBlobHeader(GByte* pabyData, GByte* pabyDataLimit,
                   unsigned int* pnBlobSize, BlobType* peBlobType)
{
    *pnBlobSize = 0;
    *peBlobType = BLOB_UNKNOW;

    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(BLOBHEADER_IDX_TYPE, WT_DATA))
        {
            unsigned int nDataLength;
            READ_SIZE(pabyData, pabyDataLimit, nDataLength);

            if (nDataLength == 7 && memcmp(pabyData, "OSMData", 7) == 0)
            {
                *peBlobType = BLOB_OSMDATA;
            }
            else if (nDataLength == 9 && memcmp(pabyData, "OSMHeader", 9) == 0)
            {
                *peBlobType = BLOB_OSMHEADER;
            }

            pabyData += nDataLength;
        }
        else if (nKey == MAKE_KEY(BLOBHEADER_IDX_INDEXDATA, WT_DATA))
        {
            /* Ignored if found */
            unsigned int nDataLength;
            READ_SIZE(pabyData, pabyDataLimit, nDataLength);
            pabyData += nDataLength;
        }
        else if (nKey == MAKE_KEY(BLOBHEADER_IDX_DATASIZE, WT_VARINT))
        {
            unsigned int nBlobSize;
            READ_VARUINT32(pabyData, pabyDataLimit, nBlobSize);
            /* printf("nBlobSize = %d\n", nBlobSize); */
            *pnBlobSize = nBlobSize;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    return pabyData == pabyDataLimit;

end_error:
    return FALSE;
}

/************************************************************************/
/*                          ReadHeaderBBox()                            */
/************************************************************************/

#define HEADERBBOX_IDX_LEFT     1
#define HEADERBBOX_IDX_RIGHT    2
#define HEADERBBOX_IDX_TOP      3
#define HEADERBBOX_IDX_BOTTOM   4

static
int ReadHeaderBBox(GByte* pabyData, GByte* pabyDataLimit,
                   OSMContext* psCtxt)
{
    psCtxt->dfLeft = 0.0;
    psCtxt->dfRight = 0.0;
    psCtxt->dfTop = 0.0;
    psCtxt->dfBottom = 0.0;

    /* printf(">ReadHeaderBBox\n"); */

    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(HEADERBBOX_IDX_LEFT, WT_VARINT))
        {
            GIntBig nLeft;
            READ_VARSINT64(pabyData, pabyDataLimit, nLeft);
            psCtxt->dfLeft = nLeft * 1e-9;
        }
        else if (nKey == MAKE_KEY(HEADERBBOX_IDX_RIGHT, WT_VARINT))
        {
            GIntBig nRight;
            READ_VARSINT64(pabyData, pabyDataLimit, nRight);
            psCtxt->dfRight = nRight * 1e-9;
        }
        else if (nKey == MAKE_KEY(HEADERBBOX_IDX_TOP, WT_VARINT))
        {
            GIntBig nTop;
            READ_VARSINT64(pabyData, pabyDataLimit, nTop);
            psCtxt->dfTop = nTop * 1e-9;
        }
        else if (nKey == MAKE_KEY(HEADERBBOX_IDX_BOTTOM, WT_VARINT))
        {
            GIntBig nBottom;
            READ_VARSINT64(pabyData, pabyDataLimit, nBottom);
            psCtxt->dfBottom = nBottom * 1e-9;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    psCtxt->pfnNotifyBounds(psCtxt->dfLeft, psCtxt->dfBottom,
                            psCtxt->dfRight, psCtxt->dfTop,
                            psCtxt, psCtxt->user_data);

    /* printf("<ReadHeaderBBox\n"); */
    return pabyData == pabyDataLimit;

end_error:
    /* printf("<ReadHeaderBBox\n"); */
    return FALSE;
}

/************************************************************************/
/*                          ReadOSMHeader()                             */
/************************************************************************/

#define OSMHEADER_IDX_BBOX                  1
#define OSMHEADER_IDX_REQUIRED_FEATURES     4
#define OSMHEADER_IDX_OPTIONAL_FEATURES     5
#define OSMHEADER_IDX_WRITING_PROGRAM       16
#define OSMHEADER_IDX_SOURCE                17

/* Ignored */
#define OSMHEADER_IDX_OSMOSIS_REPLICATION_TIMESTAMP  32
#define OSMHEADER_IDX_OSMOSIS_REPLICATION_SEQ_NUMBER 33
#define OSMHEADER_IDX_OSMOSIS_REPLICATION_BASE_URL   34

static
int ReadOSMHeader(GByte* pabyData, GByte* pabyDataLimit,
                  OSMContext* psCtxt)
{
    char* pszTxt;

    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(OSMHEADER_IDX_BBOX, WT_DATA))
        {
            unsigned int nBBOXSize;
            READ_SIZE(pabyData, pabyDataLimit, nBBOXSize);

            if (!ReadHeaderBBox(pabyData, pabyData + nBBOXSize, psCtxt)) GOTO_END_ERROR;

            pabyData += nBBOXSize;
        }
        else if (nKey == MAKE_KEY(OSMHEADER_IDX_REQUIRED_FEATURES, WT_DATA))
        {
            READ_TEXT(pabyData, pabyDataLimit, pszTxt);
            /* printf("OSMHEADER_IDX_REQUIRED_FEATURES = %s\n", pszTxt); */
            if (!(strcmp(pszTxt, "OsmSchema-V0.6") == 0 ||
                  strcmp(pszTxt, "DenseNodes") == 0))
            {
                fprintf(stderr, "Error: unsupported required feature : %s\n", pszTxt);
                VSIFree(pszTxt);
                GOTO_END_ERROR;
            }
            VSIFree(pszTxt);
        }
        else if (nKey == MAKE_KEY(OSMHEADER_IDX_OPTIONAL_FEATURES, WT_DATA))
        {
            READ_TEXT(pabyData, pabyDataLimit, pszTxt);
            /* printf("OSMHEADER_IDX_OPTIONAL_FEATURES = %s\n", pszTxt); */
            VSIFree(pszTxt);
        }
        else if (nKey == MAKE_KEY(OSMHEADER_IDX_WRITING_PROGRAM, WT_DATA))
        {
            READ_TEXT(pabyData, pabyDataLimit, pszTxt);
            /* printf("OSMHEADER_IDX_WRITING_PROGRAM = %s\n", pszTxt); */
            VSIFree(pszTxt);
        }
        else if (nKey == MAKE_KEY(OSMHEADER_IDX_SOURCE, WT_DATA))
        {
            READ_TEXT(pabyData, pabyDataLimit, pszTxt);
            /* printf("OSMHEADER_IDX_SOURCE = %s\n", pszTxt); */
            VSIFree(pszTxt);
        }
        else if (nKey == MAKE_KEY(OSMHEADER_IDX_OSMOSIS_REPLICATION_TIMESTAMP, WT_VARINT))
        {
            /* TODO: Do something with nVal or change this to a seek forward. */
            GIntBig nVal;
            READ_VARINT64(pabyData, pabyDataLimit, nVal);
        }
        else if (nKey == MAKE_KEY(OSMHEADER_IDX_OSMOSIS_REPLICATION_SEQ_NUMBER, WT_VARINT))
        {
            GIntBig nVal;
            READ_VARINT64(pabyData, pabyDataLimit, nVal);
        }
        else if (nKey == MAKE_KEY(OSMHEADER_IDX_OSMOSIS_REPLICATION_BASE_URL, WT_DATA))
        {
            READ_TEXT(pabyData, pabyDataLimit, pszTxt);
            /* printf("OSMHEADER_IDX_OSMOSIS_REPLICATION_BASE_URL = %s\n", pszTxt); */
            VSIFree(pszTxt);
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    return pabyData == pabyDataLimit;

end_error:
    return FALSE;
}

/************************************************************************/
/*                         ReadStringTable()                            */
/************************************************************************/

#define READSTRINGTABLE_IDX_STRING  1

static
int ReadStringTable(GByte* pabyData, GByte* pabyDataLimit,
                    OSMContext* psCtxt)
{
    char* pszStrBuf = (char*)pabyData;

    unsigned int nStrCount = 0;
    int* panStrOff = psCtxt->panStrOff;

    psCtxt->pszStrBuf = pszStrBuf;

    if (pabyDataLimit - pabyData > psCtxt->nStrAllocated)
    {
        int* panStrOffNew;
        psCtxt->nStrAllocated = MAX(psCtxt->nStrAllocated * 2,
                                          pabyDataLimit - pabyData);
        panStrOffNew = (int*) VSIRealloc(
            panStrOff, psCtxt->nStrAllocated * sizeof(int));
        if( panStrOffNew == NULL )
            GOTO_END_ERROR;
        panStrOff = panStrOffNew;
    }

    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        while (nKey == MAKE_KEY(READSTRINGTABLE_IDX_STRING, WT_DATA))
        {
            GByte* pbSaved;
            unsigned int nDataLength;
            READ_SIZE(pabyData, pabyDataLimit, nDataLength);

            panStrOff[nStrCount ++] = pabyData - (GByte*)pszStrBuf;
            pbSaved = &pabyData[nDataLength];

            pabyData += nDataLength;

            if (pabyData < pabyDataLimit)
            {
                READ_FIELD_KEY(nKey);
                *pbSaved = 0;
                /* printf("string[%d] = %s\n", nStrCount-1, pbSaved - nDataLength); */
            }
            else
            {
                *pbSaved = 0;
                /* printf("string[%d] = %s\n", nStrCount-1, pbSaved - nDataLength); */
                break;
            }
        }

        if (pabyData < pabyDataLimit)
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    psCtxt->panStrOff = panStrOff;
    psCtxt->nStrCount = nStrCount;

    return pabyData == pabyDataLimit;

end_error:

    psCtxt->panStrOff = panStrOff;
    psCtxt->nStrCount = nStrCount;

    return FALSE;
}

/************************************************************************/
/*                         ReadDenseNodes()                             */
/************************************************************************/

#define DENSEINFO_IDX_VERSION     1
#define DENSEINFO_IDX_TIMESTAMP   2
#define DENSEINFO_IDX_CHANGESET   3
#define DENSEINFO_IDX_UID         4
#define DENSEINFO_IDX_USER_SID    5
#define DENSEINFO_IDX_VISIBLE     6

#define DENSENODES_IDX_ID           1
#define DENSENODES_IDX_DENSEINFO    5
#define DENSENODES_IDX_LAT          8
#define DENSENODES_IDX_LON          9
#define DENSENODES_IDX_KEYVALS      10

static
int ReadDenseNodes(GByte* pabyData, GByte* pabyDataLimit,
                   OSMContext* psCtxt)
{
    GByte* pabyDataIDs = NULL;
    GByte* pabyDataIDsLimit = NULL;
    GByte* pabyDataLat = NULL;
    GByte* pabyDataLon = NULL;
    GByte* apabyData[DENSEINFO_IDX_VISIBLE] = {NULL, NULL, NULL, NULL, NULL, NULL};
    GByte* pabyDataKeyVal = NULL;

    /* printf(">ReadDenseNodes\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(DENSENODES_IDX_ID, WT_DATA) )
        {
            unsigned int nSize;

            if (pabyDataIDs != NULL)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (nSize > psCtxt->nNodesAllocated)
            {
                OSMNode* pasNodesNew;
                psCtxt->nNodesAllocated = MAX(psCtxt->nNodesAllocated * 2,
                                                 nSize);
                pasNodesNew = (OSMNode*) VSIRealloc(
                    psCtxt->pasNodes, psCtxt->nNodesAllocated * sizeof(OSMNode));
                if( pasNodesNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasNodes = pasNodesNew;
            }

            pabyDataIDs = pabyData;
            pabyDataIDsLimit = pabyData + nSize;
            pabyData += nSize;
        }
        else if( nKey == MAKE_KEY(DENSENODES_IDX_DENSEINFO, WT_DATA) )
        {
            unsigned int nSize;
            GByte* pabyDataNewLimit;
 
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            /* Inline reading of DenseInfo structure */

            pabyDataNewLimit = pabyData + nSize;
            while(pabyData < pabyDataNewLimit)
            {
                int nFieldNumber;
                READ_FIELD_KEY(nKey);

                nFieldNumber = GET_FIELDNUMBER(nKey);
                if (GET_WIRETYPE(nKey) == WT_DATA &&
                    nFieldNumber >= DENSEINFO_IDX_VERSION && nFieldNumber <= DENSEINFO_IDX_VISIBLE)
                {
                    if( apabyData[nFieldNumber - 1] != NULL) GOTO_END_ERROR;
                    READ_SIZE(pabyData, pabyDataNewLimit, nSize);

                    apabyData[nFieldNumber - 1] = pabyData;
                    pabyData += nSize;
                }
                else
                {
                    SKIP_UNKNOWN_FIELD(pabyData, pabyDataNewLimit, TRUE);
                }
            }

            if( pabyData != pabyDataNewLimit )
                GOTO_END_ERROR;
        }
        else if( nKey == MAKE_KEY(DENSENODES_IDX_LAT, WT_DATA) )
        {
            unsigned int nSize;
            if (pabyDataLat != NULL)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);
            pabyDataLat = pabyData;
            pabyData += nSize;
        }
        else if( nKey == MAKE_KEY(DENSENODES_IDX_LON, WT_DATA) )
        {
            unsigned int nSize;
            if (pabyDataLon != NULL)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);
            pabyDataLon = pabyData;
            pabyData += nSize;
        }
        else if( nKey == MAKE_KEY(DENSENODES_IDX_KEYVALS, WT_DATA) )
        {
            unsigned int nSize;
            if( pabyDataKeyVal != NULL )
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            pabyDataKeyVal = pabyData;

            if (nSize > psCtxt->nTagsAllocated)
            {
                OSMTag* pasTagsNew;

                psCtxt->nTagsAllocated = MAX(
                    psCtxt->nTagsAllocated * 2, nSize);
                pasTagsNew = (OSMTag*) VSIRealloc(
                    psCtxt->pasTags,
                    psCtxt->nTagsAllocated * sizeof(OSMTag));
                if( pasTagsNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasTags = pasTagsNew;
            }

            pabyData += nSize;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    if( pabyData != pabyDataLimit )
        GOTO_END_ERROR;

    if( pabyDataIDs != NULL && pabyDataLat != NULL && pabyDataLon != NULL )
    {
        GByte* pabyDataVersion = apabyData[DENSEINFO_IDX_VERSION - 1];
        GByte* pabyDataTimeStamp = apabyData[DENSEINFO_IDX_TIMESTAMP - 1];
        GByte* pabyDataChangeset = apabyData[DENSEINFO_IDX_CHANGESET - 1];
        GByte* pabyDataUID = apabyData[DENSEINFO_IDX_UID - 1];
        GByte* pabyDataUserSID = apabyData[DENSEINFO_IDX_USER_SID - 1];
        /* GByte* pabyDataVisible = apabyData[DENSEINFO_IDX_VISIBLE - 1]; */

        GIntBig nID = 0;
        GIntBig nLat = 0;
        GIntBig nLon = 0;
        GIntBig nTimeStamp = 0;
        GIntBig nChangeset = 0;
        int nUID = 0;
        unsigned int nUserSID = 0;
        int nTags = 0;
        int nNodes = 0;

        const char* pszStrBuf = psCtxt->pszStrBuf;
        int* panStrOff = psCtxt->panStrOff;
        const unsigned int nStrCount = psCtxt->nStrCount;
        OSMTag* pasTags = psCtxt->pasTags;
        OSMNode* pasNodes = psCtxt->pasNodes;

        int nVersion = 0;
        /* int nVisible = 1; */

        while(pabyDataIDs < pabyDataIDsLimit)
        {
            GIntBig nDelta1, nDelta2;
            int nKVIndexStart = nTags;

            READ_VARSINT64_NOCHECK(pabyDataIDs, pabyDataIDsLimit, nDelta1);
            READ_VARSINT64(pabyDataLat, pabyDataLimit, nDelta2);
            nID += nDelta1;
            nLat += nDelta2;

            READ_VARSINT64(pabyDataLon, pabyDataLimit, nDelta1);
            nLon += nDelta1;

            if( pabyDataTimeStamp )
            {
                READ_VARSINT64(pabyDataTimeStamp, pabyDataLimit, nDelta2);
                nTimeStamp += nDelta2;
            }
            if( pabyDataChangeset )
            {
                READ_VARSINT64(pabyDataChangeset, pabyDataLimit, nDelta1);
                nChangeset += nDelta1;
            }
            if( pabyDataVersion )
            {
                READ_VARINT32(pabyDataVersion, pabyDataLimit, nVersion);
            }
            if( pabyDataUID )
            {
                int nDeltaUID;
                READ_VARSINT32(pabyDataUID, pabyDataLimit, nDeltaUID);
                nUID += nDeltaUID;
            }
            if( pabyDataUserSID )
            {
                int nDeltaUserSID;
                READ_VARSINT32(pabyDataUserSID, pabyDataLimit, nDeltaUserSID);
                nUserSID += nDeltaUserSID;
                if (nUserSID >= nStrCount)
                    GOTO_END_ERROR;
            }
            /* if( pabyDataVisible )
                READ_VARINT32(pabyDataVisible, pabyDataLimit, nVisible); */

            if( pabyDataKeyVal )
            {
                while (TRUE)
                {
                    unsigned int nKey, nVal;
                    READ_VARUINT32(pabyDataKeyVal, pabyDataLimit, nKey);
                    if (nKey == 0)
                        break;
                    if (nKey >= nStrCount)
                        GOTO_END_ERROR;

                    READ_VARUINT32(pabyDataKeyVal, pabyDataLimit, nVal);
                    if (nVal >= nStrCount)
                        GOTO_END_ERROR;

                    pasTags[nTags].pszK = pszStrBuf + panStrOff[nKey];
                    pasTags[nTags].pszV = pszStrBuf + panStrOff[nVal];
                    nTags ++;

                    /* printf("nKey = %d, nVal = %d\n", nKey, nVal); */
                }
            }

            if( nTags > nKVIndexStart )
                pasNodes[nNodes].pasTags = pasTags + nKVIndexStart;
            else
                pasNodes[nNodes].pasTags = NULL;
            pasNodes[nNodes].nTags = nTags - nKVIndexStart;

            pasNodes[nNodes].nID = nID;
            pasNodes[nNodes].dfLat = .000000001 * (psCtxt->nLatOffset + (psCtxt->nGranularity * nLat));
            pasNodes[nNodes].dfLon = .000000001 * (psCtxt->nLonOffset + (psCtxt->nGranularity * nLon));
            pasNodes[nNodes].sInfo.bTimeStampIsStr = FALSE;
            pasNodes[nNodes].sInfo.ts.nTimeStamp = nTimeStamp;
            pasNodes[nNodes].sInfo.nChangeset = nChangeset;
            pasNodes[nNodes].sInfo.nVersion = nVersion;
            pasNodes[nNodes].sInfo.nUID = nUID;
            pasNodes[nNodes].sInfo.pszUserSID = pszStrBuf + panStrOff[nUserSID];
            /* pasNodes[nNodes].sInfo.nVisible = nVisible; */
            nNodes ++;
            /* printf("nLat = " CPL_FRMT_GIB "\n", nLat); printf("nLon = " CPL_FRMT_GIB "\n", nLon); */
        }

        psCtxt->pfnNotifyNodes(nNodes, pasNodes, psCtxt, psCtxt->user_data);

        if(pabyDataIDs != pabyDataIDsLimit)
            GOTO_END_ERROR;
    }

    /* printf("<ReadDenseNodes\n"); */

    return TRUE;

end_error:
    /* printf("<ReadDenseNodes\n"); */

    return FALSE;
}

/************************************************************************/
/*                           ReadOSMInfo()                              */
/************************************************************************/

#define INFO_IDX_VERSION     1
#define INFO_IDX_TIMESTAMP   2
#define INFO_IDX_CHANGESET   3
#define INFO_IDX_UID         4
#define INFO_IDX_USER_SID    5
#define INFO_IDX_VISIBLE     6

static
int ReadOSMInfo(GByte* pabyData, GByte* pabyDataLimit,
             OSMInfo* psInfo, OSMContext* psContext) CPL_NO_INLINE;

static
int ReadOSMInfo(GByte* pabyData, GByte* pabyDataLimit,
             OSMInfo* psInfo, OSMContext* psContext)
{
    /* printf(">ReadOSMInfo\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(INFO_IDX_VERSION, WT_VARINT))
        {
            READ_VARINT32(pabyData, pabyDataLimit, psInfo->nVersion);
        }
        else if (nKey == MAKE_KEY(INFO_IDX_TIMESTAMP, WT_VARINT))
        {
            READ_VARINT64(pabyData, pabyDataLimit, psInfo->ts.nTimeStamp);
        }
        else if (nKey == MAKE_KEY(INFO_IDX_CHANGESET, WT_VARINT))
        {
            READ_VARINT64(pabyData, pabyDataLimit, psInfo->nChangeset);
        }
        else if (nKey == MAKE_KEY(INFO_IDX_UID, WT_VARINT))
        {
            READ_VARINT32(pabyData, pabyDataLimit, psInfo->nUID);
        }
        else if (nKey == MAKE_KEY(INFO_IDX_USER_SID, WT_VARINT))
        {
            unsigned int nUserSID;
            READ_VARUINT32(pabyData, pabyDataLimit, nUserSID);
            if( nUserSID < psContext->nStrCount)
                psInfo->pszUserSID = psContext->pszStrBuf +
                                     psContext->panStrOff[nUserSID];
        }
        else if (nKey == MAKE_KEY(INFO_IDX_VISIBLE, WT_VARINT))
        {
            int nVisible;
            READ_VARINT32(pabyData, pabyDataLimit, /*psInfo->*/nVisible);
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }
    /* printf("<ReadOSMInfo\n"); */

    return pabyData == pabyDataLimit;

end_error:
    /* printf("<ReadOSMInfo\n"); */

    return FALSE;
}

/************************************************************************/
/*                             ReadNode()                               */
/************************************************************************/

#define NODE_IDX_ID      1
#define NODE_IDX_LAT     7
#define NODE_IDX_LON     8
#define NODE_IDX_KEYS    9
#define NODE_IDX_VALS    10
#define NODE_IDX_INFO    11

static
int ReadNode(GByte* pabyData, GByte* pabyDataLimit,
             OSMContext* psCtxt)
{
    OSMNode sNode;

    sNode.nID = 0;
    sNode.dfLat = 0.0;
    sNode.dfLon = 0.0;
    INIT_INFO(sNode.sInfo);
    sNode.nTags = 0;
    sNode.pasTags = NULL;

    /* printf(">ReadNode\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(NODE_IDX_ID, WT_VARINT))
        {
            READ_VARSINT64_NOCHECK(pabyData, pabyDataLimit, sNode.nID);
        }
        else if (nKey == MAKE_KEY(NODE_IDX_LAT, WT_VARINT))
        {
            GIntBig nLat;
            READ_VARSINT64_NOCHECK(pabyData, pabyDataLimit, nLat);
            sNode.dfLat = .000000001 * (psCtxt->nLatOffset + (psCtxt->nGranularity * nLat));
        }
        else if (nKey == MAKE_KEY(NODE_IDX_LON, WT_VARINT))
        {
            GIntBig nLon;
            READ_VARSINT64_NOCHECK(pabyData, pabyDataLimit, nLon);
            sNode.dfLon = .000000001 * (psCtxt->nLonOffset + (psCtxt->nGranularity * nLon));
        }
        else if (nKey == MAKE_KEY(NODE_IDX_KEYS, WT_DATA))
        {
            unsigned int nSize;
            GByte* pabyDataNewLimit;
            if (sNode.nTags != 0)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (nSize > psCtxt->nTagsAllocated)
            {
                OSMTag* pasTagsNew;

                psCtxt->nTagsAllocated = MAX(
                    psCtxt->nTagsAllocated * 2, nSize);
                pasTagsNew = (OSMTag*) VSIRealloc(
                    psCtxt->pasTags,
                    psCtxt->nTagsAllocated * sizeof(OSMTag));
                if( pasTagsNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasTags = pasTagsNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                unsigned int nKey;
                READ_VARUINT32(pabyData, pabyDataNewLimit, nKey);

                if (nKey >= psCtxt->nStrCount)
                    GOTO_END_ERROR;

                psCtxt->pasTags[sNode.nTags].pszK = psCtxt->pszStrBuf +
                                              psCtxt->panStrOff[nKey];
                psCtxt->pasTags[sNode.nTags].pszV = NULL;
                sNode.nTags ++;
            }
            if (pabyData != pabyDataNewLimit)
                GOTO_END_ERROR;
        }
        else if (nKey == MAKE_KEY(NODE_IDX_VALS, WT_DATA))
        {
            unsigned int nSize;
            unsigned int nIter = 0;
            if (sNode.nTags == 0)
                GOTO_END_ERROR;
            READ_VARUINT32(pabyData, pabyDataLimit, nSize);

            for(; nIter < sNode.nTags; nIter ++)
            {
                unsigned int nVal;
                READ_VARUINT32(pabyData, pabyDataLimit, nVal);

                if (nVal >= psCtxt->nStrCount)
                    GOTO_END_ERROR;

                psCtxt->pasTags[nIter].pszV = psCtxt->pszStrBuf +
                                              psCtxt->panStrOff[nVal];
            }
        }
        else if (nKey == MAKE_KEY(NODE_IDX_INFO, WT_DATA))
        {
            unsigned int nSize;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (!ReadOSMInfo(pabyData, pabyDataLimit + nSize, &sNode.sInfo, psCtxt))
                GOTO_END_ERROR;

            pabyData += nSize;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    if( pabyData != pabyDataLimit )
        GOTO_END_ERROR;

    if (sNode.nTags)
        sNode.pasTags = psCtxt->pasTags;
    else
        sNode.pasTags = NULL;
    psCtxt->pfnNotifyNodes(1, &sNode, psCtxt, psCtxt->user_data);

    /* printf("<ReadNode\n"); */

    return TRUE;

end_error:
    /* printf("<ReadNode\n"); */

    return FALSE;
}


/************************************************************************/
/*                              ReadWay()                               */
/************************************************************************/

#define WAY_IDX_ID      1
#define WAY_IDX_KEYS    2
#define WAY_IDX_VALS    3
#define WAY_IDX_INFO    4
#define WAY_IDX_REFS    8

static
int ReadWay(GByte* pabyData, GByte* pabyDataLimit,
            OSMContext* psCtxt)
{
    OSMWay sWay;
    sWay.nID = 0;
    INIT_INFO(sWay.sInfo);
    sWay.nTags = 0;
    sWay.nRefs = 0;

    /* printf(">ReadWay\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(WAY_IDX_ID, WT_VARINT))
        {
            READ_VARINT64(pabyData, pabyDataLimit, sWay.nID);
        }
        else if (nKey == MAKE_KEY(WAY_IDX_KEYS, WT_DATA))
        {
            unsigned int nSize;
            GByte* pabyDataNewLimit;
            if (sWay.nTags != 0)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (nSize > psCtxt->nTagsAllocated)
            {
                OSMTag* pasTagsNew;

                psCtxt->nTagsAllocated = MAX(
                    psCtxt->nTagsAllocated * 2, nSize);
                pasTagsNew = (OSMTag*) VSIRealloc(
                    psCtxt->pasTags,
                    psCtxt->nTagsAllocated * sizeof(OSMTag));
                if( pasTagsNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasTags = pasTagsNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                unsigned int nKey;
                READ_VARUINT32(pabyData, pabyDataNewLimit, nKey);

                if (nKey >= psCtxt->nStrCount)
                    GOTO_END_ERROR;

                psCtxt->pasTags[sWay.nTags].pszK = psCtxt->pszStrBuf +
                                                   psCtxt->panStrOff[nKey];
                psCtxt->pasTags[sWay.nTags].pszV = NULL;
                sWay.nTags ++;
            }
            if (pabyData != pabyDataNewLimit)
                GOTO_END_ERROR;
        }
        else if (nKey == MAKE_KEY(WAY_IDX_VALS, WT_DATA))
        {
            unsigned int nSize;
            unsigned int nIter = 0;
            if (sWay.nTags == 0)
                GOTO_END_ERROR;
            READ_VARUINT32(pabyData, pabyDataLimit, nSize);

            for(; nIter < sWay.nTags; nIter ++)
            {
                unsigned int nVal;
                READ_VARUINT32(pabyData, pabyDataLimit, nVal);

                if (nVal >= psCtxt->nStrCount)
                    GOTO_END_ERROR;

                psCtxt->pasTags[nIter].pszV = psCtxt->pszStrBuf +
                                              psCtxt->panStrOff[nVal];
            }
        }
        else if (nKey == MAKE_KEY(WAY_IDX_INFO, WT_DATA))
        {
            unsigned int nSize;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (!ReadOSMInfo(pabyData, pabyData + nSize, &sWay.sInfo, psCtxt))
                GOTO_END_ERROR;

            pabyData += nSize;
        }
        else if (nKey == MAKE_KEY(WAY_IDX_REFS, WT_DATA))
        {
            GIntBig nRefVal = 0;
            unsigned int nSize;
            GByte* pabyDataNewLimit;
            if (sWay.nRefs != 0)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (nSize > psCtxt->nNodeRefsAllocated)
            {
                GIntBig* panNodeRefsNew;
                psCtxt->nNodeRefsAllocated =
                    MAX(psCtxt->nNodeRefsAllocated * 2, nSize);
                panNodeRefsNew = (GIntBig*) VSIRealloc(
                        psCtxt->panNodeRefs,
                        psCtxt->nNodeRefsAllocated * sizeof(GIntBig));
                if( panNodeRefsNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->panNodeRefs = panNodeRefsNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                GIntBig nDeltaRef;
                READ_VARSINT64_NOCHECK(pabyData, pabyDataNewLimit, nDeltaRef);
                nRefVal += nDeltaRef;

                psCtxt->panNodeRefs[sWay.nRefs ++] = nRefVal;
            }

            if (pabyData != pabyDataNewLimit)
                GOTO_END_ERROR;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    if( pabyData != pabyDataLimit )
        GOTO_END_ERROR;

    /* printf("<ReadWay\n"); */

    if (sWay.nTags)
        sWay.pasTags = psCtxt->pasTags;
    else
        sWay.pasTags = NULL;
    sWay.panNodeRefs = psCtxt->panNodeRefs;

    psCtxt->pfnNotifyWay(&sWay, psCtxt, psCtxt->user_data);

    return TRUE;

end_error:
    /* printf("<ReadWay\n"); */

    return FALSE;
}

/************************************************************************/
/*                            ReadRelation()                            */
/************************************************************************/

#define RELATION_IDX_ID           1
#define RELATION_IDX_KEYS         2
#define RELATION_IDX_VALS         3
#define RELATION_IDX_INFO         4
#define RELATION_IDX_ROLES_SID    8
#define RELATION_IDX_MEMIDS       9
#define RELATION_IDX_TYPES        10

static
int ReadRelation(GByte* pabyData, GByte* pabyDataLimit,
                 OSMContext* psCtxt)
{
    OSMRelation sRelation;
    sRelation.nID = 0;
    INIT_INFO(sRelation.sInfo);
    sRelation.nTags = 0;
    sRelation.nMembers = 0;

    /* printf(">ReadRelation\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(RELATION_IDX_ID, WT_VARINT))
        {
            READ_VARINT64(pabyData, pabyDataLimit, sRelation.nID);
        }
        else if (nKey == MAKE_KEY(RELATION_IDX_KEYS, WT_DATA))
        {
            unsigned int nSize;
            GByte* pabyDataNewLimit;
            if (sRelation.nTags != 0)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (nSize > psCtxt->nTagsAllocated)
            {
                OSMTag* pasTagsNew;

                psCtxt->nTagsAllocated = MAX(
                    psCtxt->nTagsAllocated * 2, nSize);
                pasTagsNew = (OSMTag*) VSIRealloc(
                    psCtxt->pasTags,
                    psCtxt->nTagsAllocated * sizeof(OSMTag));
                if( pasTagsNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasTags = pasTagsNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                unsigned int nKey;
                READ_VARUINT32(pabyData, pabyDataNewLimit, nKey);

                if (nKey >= psCtxt->nStrCount)
                    GOTO_END_ERROR;

                psCtxt->pasTags[sRelation.nTags].pszK = psCtxt->pszStrBuf +
                                                        psCtxt->panStrOff[nKey];
                psCtxt->pasTags[sRelation.nTags].pszV = NULL;
                sRelation.nTags ++;
            }
            if (pabyData != pabyDataNewLimit)
                GOTO_END_ERROR;
        }
        else if (nKey == MAKE_KEY(RELATION_IDX_VALS, WT_DATA))
        {
            unsigned int nSize;
            unsigned int nIter = 0;
            if (sRelation.nTags == 0)
                GOTO_END_ERROR;
            READ_VARUINT32(pabyData, pabyDataLimit, nSize);

            for(; nIter < sRelation.nTags; nIter ++)
            {
                unsigned int nVal;
                READ_VARUINT32(pabyData, pabyDataLimit, nVal);

                if (nVal >= psCtxt->nStrCount)
                    GOTO_END_ERROR;

                psCtxt->pasTags[nIter].pszV = psCtxt->pszStrBuf +
                                              psCtxt->panStrOff[nVal];
            }
        }
        else if (nKey == MAKE_KEY(RELATION_IDX_INFO, WT_DATA))
        {
            unsigned int nSize;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (!ReadOSMInfo(pabyData, pabyData + nSize, &sRelation.sInfo, psCtxt))
                GOTO_END_ERROR;

            pabyData += nSize;
        }
        else if (nKey == MAKE_KEY(RELATION_IDX_ROLES_SID, WT_DATA))
        {
            unsigned int nSize;
            GByte* pabyDataNewLimit;
            if (sRelation.nMembers != 0)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (nSize > psCtxt->nMembersAllocated)
            {
                OSMMember* pasMembersNew;
                psCtxt->nMembersAllocated =
                    MAX(psCtxt->nMembersAllocated * 2, nSize);
                pasMembersNew = (OSMMember*) VSIRealloc(
                        psCtxt->pasMembers,
                        psCtxt->nMembersAllocated * sizeof(OSMMember));
                if( pasMembersNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasMembers = pasMembersNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                unsigned int nRoleSID;
                READ_VARUINT32(pabyData, pabyDataNewLimit, nRoleSID);
                if (nRoleSID >= psCtxt->nStrCount)
                    GOTO_END_ERROR;

                psCtxt->pasMembers[sRelation.nMembers].pszRole =
                    psCtxt->pszStrBuf + psCtxt->panStrOff[nRoleSID];
                psCtxt->pasMembers[sRelation.nMembers].nID = 0;
                psCtxt->pasMembers[sRelation.nMembers].eType = MEMBER_NODE;
                sRelation.nMembers ++;
            }

            if (pabyData != pabyDataNewLimit)
                GOTO_END_ERROR;
        }
        else if (nKey == MAKE_KEY(RELATION_IDX_MEMIDS, WT_DATA))
        {
            unsigned int nIter = 0;
            GIntBig nMemID = 0;
            unsigned int nSize;
            if (sRelation.nMembers == 0)
                GOTO_END_ERROR;
            READ_VARUINT32(pabyData, pabyDataLimit, nSize);

            for(; nIter < sRelation.nMembers; nIter++)
            {
                GIntBig nDeltaMemID;
                READ_VARSINT64(pabyData, pabyDataLimit, nDeltaMemID);
                nMemID += nDeltaMemID;

                psCtxt->pasMembers[nIter].nID = nMemID;
            }
        }
        else if (nKey == MAKE_KEY(RELATION_IDX_TYPES, WT_DATA))
        {
            unsigned int nIter = 0;
            unsigned int nSize;
            if (sRelation.nMembers == 0)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);
            if (nSize != sRelation.nMembers)
                GOTO_END_ERROR;

            for(; nIter < sRelation.nMembers; nIter++)
            {
                unsigned int nType = pabyData[nIter];
                if (nType > MEMBER_RELATION)
                    GOTO_END_ERROR;

                psCtxt->pasMembers[nIter].eType = (OSMMemberType) nType;
            }
            pabyData += nSize;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }
    /* printf("<ReadRelation\n"); */

    if( pabyData != pabyDataLimit )
        GOTO_END_ERROR;

    if (sRelation.nTags)
        sRelation.pasTags = psCtxt->pasTags;
    else
        sRelation.pasTags = NULL;

    sRelation.pasMembers = psCtxt->pasMembers;

    psCtxt->pfnNotifyRelation(&sRelation, psCtxt, psCtxt->user_data);

    return TRUE;

end_error:
    /* printf("<ReadRelation\n"); */

    return FALSE;
}

/************************************************************************/
/*                          ReadPrimitiveGroup()                        */
/************************************************************************/

#define PRIMITIVEGROUP_IDX_NODES      1
#define PRIMITIVEGROUP_IDX_DENSE      2
#define PRIMITIVEGROUP_IDX_WAYS       3
#define PRIMITIVEGROUP_IDX_RELATIONS  4
#define PRIMITIVEGROUP_IDX_CHANGESETS 5

typedef int (*PrimitiveFuncType)(GByte* pabyData, GByte* pabyDataLimit,
                                 OSMContext* psCtxt);

static const PrimitiveFuncType apfnPrimitives[] =
{
    ReadNode,
    ReadDenseNodes,
    ReadWay,
    ReadRelation
};

static
int ReadPrimitiveGroup(GByte* pabyData, GByte* pabyDataLimit,
                       OSMContext* psCtxt)
{
    /* printf(">ReadPrimitiveGroup\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey;
        int nFieldNumber;
        READ_FIELD_KEY(nKey);

        nFieldNumber = GET_FIELDNUMBER(nKey) - 1;
        if( GET_WIRETYPE(nKey) == WT_DATA &&
            nFieldNumber >= PRIMITIVEGROUP_IDX_NODES - 1 &&
            nFieldNumber <= PRIMITIVEGROUP_IDX_RELATIONS - 1 )
        {
            unsigned int nSize;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (!apfnPrimitives[nFieldNumber](pabyData, pabyData + nSize, psCtxt))
                GOTO_END_ERROR;

            pabyData += nSize;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }
    /* printf("<ReadPrimitiveGroup\n"); */

    return pabyData == pabyDataLimit;

end_error:
    /* printf("<ReadPrimitiveGroup\n"); */

    return FALSE;
}

/************************************************************************/
/*                          ReadPrimitiveBlock()                        */
/************************************************************************/

#define PRIMITIVEBLOCK_IDX_STRINGTABLE      1
#define PRIMITIVEBLOCK_IDX_PRIMITIVEGROUP   2
#define PRIMITIVEBLOCK_IDX_GRANULARITY      17
#define PRIMITIVEBLOCK_IDX_DATE_GRANULARITY 18
#define PRIMITIVEBLOCK_IDX_LAT_OFFSET       19
#define PRIMITIVEBLOCK_IDX_LON_OFFSET       20

static
int ReadPrimitiveBlock(GByte* pabyData, GByte* pabyDataLimit,
                       OSMContext* psCtxt)
{
    GByte* pabyDataSave = pabyData;

    psCtxt->pszStrBuf = NULL;
    psCtxt->nStrCount = 0;
    psCtxt->nGranularity = 100;
    psCtxt->nDateGranularity = 1000;
    psCtxt->nLatOffset = 0;
    psCtxt->nLonOffset = 0;

    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_GRANULARITY, WT_VARINT))
        {
            READ_VARINT32(pabyData, pabyDataLimit, psCtxt->nGranularity);
        }
        else if (nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_DATE_GRANULARITY, WT_VARINT))
        {
            READ_VARINT32(pabyData, pabyDataLimit, psCtxt->nDateGranularity);
        }
        else if (nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_LAT_OFFSET, WT_VARINT))
        {
            READ_VARINT64(pabyData, pabyDataLimit, psCtxt->nLatOffset);
        }
        else if (nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_LON_OFFSET, WT_VARINT))
        {
            READ_VARINT64(pabyData, pabyDataLimit, psCtxt->nLonOffset);
        }
        else
        {
            SKIP_UNKNOWN_FIELD_INLINE(pabyData, pabyDataLimit, FALSE);
        }
    }

    if (pabyData != pabyDataLimit)
        GOTO_END_ERROR;

    pabyData = pabyDataSave;
    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_STRINGTABLE, WT_DATA))
        {
            GByte bSaveAfterByte;
            GByte* pbSaveAfterByte;
            unsigned int nSize;
            if (psCtxt->nStrCount != 0)
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            /* Dirty little trick */
            /* ReadStringTable() will over-write the byte after the */
            /* StringTable message with a NUL charachter, so we backup */
            /* it to be able to restore it just before issuing the next */
            /* READ_FIELD_KEY. Then we will re-NUL it to have valid */
            /* NUL terminated strings */
            /* This trick enable us to keep the strings where there are */
            /* in RAM */
            pbSaveAfterByte = pabyData + nSize;
            bSaveAfterByte = *pbSaveAfterByte;

            if (!ReadStringTable(pabyData, pabyData + nSize,
                                 psCtxt))
                GOTO_END_ERROR;

            pabyData += nSize;

            *pbSaveAfterByte = bSaveAfterByte;
            if (pabyData == pabyDataLimit)
                break;

            READ_FIELD_KEY(nKey);
            *pbSaveAfterByte = 0;

            if (nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_STRINGTABLE, WT_DATA))
                GOTO_END_ERROR;

            /* Yes we go on ! */
        }

        if (nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_PRIMITIVEGROUP, WT_DATA))
        {
            unsigned int nSize;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if (!ReadPrimitiveGroup(pabyData, pabyData + nSize,
                                    psCtxt))
                GOTO_END_ERROR;

            pabyData += nSize;
        }
        else
        {
            SKIP_UNKNOWN_FIELD_INLINE(pabyData, pabyDataLimit, FALSE);
        }
    }

    return pabyData == pabyDataLimit;

end_error:

    return FALSE;
}

/************************************************************************/
/*                              ReadBlob()                              */
/************************************************************************/

#define BLOB_IDX_RAW         1
#define BLOB_IDX_RAW_SIZE    2
#define BLOB_IDX_ZLIB_DATA   3

static
int ReadBlob(GByte* pabyData, unsigned int nDataSize, BlobType eType,
             OSMContext* psCtxt)
{
    unsigned int nUncompressedSize = 0;
    int bRet = TRUE;
    GByte* pabyDataLimit = pabyData + nDataSize;

    while(pabyData < pabyDataLimit)
    {
        int nKey;
        READ_FIELD_KEY(nKey);

        if (nKey == MAKE_KEY(BLOB_IDX_RAW, WT_DATA))
        {
            unsigned int nDataLength;
            READ_SIZE(pabyData, pabyDataLimit, nDataLength);
            if (nDataLength > 64 * 1024 * 1024) GOTO_END_ERROR;

            /* printf("raw data size = %d\n", nDataLength); */

            if (eType == BLOB_OSMHEADER)
            {
                bRet = ReadOSMHeader(pabyData, pabyData + nDataLength, psCtxt);
            }
            else if (eType == BLOB_OSMDATA)
            {
                bRet = ReadPrimitiveBlock(pabyData, pabyData + nDataLength,
                                          psCtxt);
            }

            pabyData += nDataLength;
        }
        else if (nKey == MAKE_KEY(BLOB_IDX_RAW_SIZE, WT_VARINT))
        {
            READ_VARUINT32(pabyData, pabyDataLimit, nUncompressedSize);
            /* printf("nUncompressedSize = %d\n", nUncompressedSize); */
        }
        else if (nKey == MAKE_KEY(BLOB_IDX_ZLIB_DATA, WT_DATA))
        {
            unsigned int nZlibCompressedSize;
            READ_VARUINT32(pabyData, pabyDataLimit, nZlibCompressedSize);
            if (CHECK_OOB && nZlibCompressedSize > nDataSize) GOTO_END_ERROR;

            /* printf("nZlibCompressedSize = %d\n", nZlibCompressedSize); */

            if (nUncompressedSize != 0)
            {
                void* pOut;

                if (nUncompressedSize > psCtxt->nUncompressedAllocated)
                {
                    GByte* pabyUncompressedNew;
                    psCtxt->nUncompressedAllocated =
                        MAX(psCtxt->nUncompressedAllocated * 2, nUncompressedSize);
                    pabyUncompressedNew = (GByte*)VSIRealloc(psCtxt->pabyUncompressed,
                                        psCtxt->nUncompressedAllocated + EXTRA_BYTES);
                    if( pabyUncompressedNew == NULL )
                        GOTO_END_ERROR;
                    psCtxt->pabyUncompressed = pabyUncompressedNew;
                }
                memset(psCtxt->pabyUncompressed + nUncompressedSize, 0, EXTRA_BYTES);

                /* printf("inflate %d -> %d\n", nZlibCompressedSize, nUncompressedSize); */

                pOut = CPLZLibInflate( pabyData, nZlibCompressedSize,
                                       psCtxt->pabyUncompressed, nUncompressedSize,
                                       NULL );
                if( pOut == NULL )
                    GOTO_END_ERROR;

                if (eType == BLOB_OSMHEADER)
                {
                    bRet = ReadOSMHeader(psCtxt->pabyUncompressed,
                                         psCtxt->pabyUncompressed + nUncompressedSize,
                                         psCtxt);
                }
                else if (eType == BLOB_OSMDATA)
                {
                    bRet = ReadPrimitiveBlock(psCtxt->pabyUncompressed,
                                              psCtxt->pabyUncompressed + nUncompressedSize,
                                              psCtxt);
                }
            }

            pabyData += nZlibCompressedSize;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    return bRet;

end_error:
    return FALSE;
}

/************************************************************************/
/*                        EmptyNotifyNodesFunc()                        */
/************************************************************************/

static void EmptyNotifyNodesFunc(CPL_UNUSED unsigned int nNodes, CPL_UNUSED OSMNode* pasNodes,
                                 CPL_UNUSED OSMContext* psCtxt, CPL_UNUSED void* user_data)
{
}


/************************************************************************/
/*                         EmptyNotifyWayFunc()                         */
/************************************************************************/

static void EmptyNotifyWayFunc(CPL_UNUSED OSMWay* psWay,
                               CPL_UNUSED OSMContext* psCtxt, CPL_UNUSED void* user_data)
{
}

/************************************************************************/
/*                       EmptyNotifyRelationFunc()                      */
/************************************************************************/

static void EmptyNotifyRelationFunc(CPL_UNUSED OSMRelation* psRelation,
                                    CPL_UNUSED OSMContext* psCtxt, CPL_UNUSED void* user_data)
{
}

/************************************************************************/
/*                         EmptyNotifyBoundsFunc()                      */
/************************************************************************/

static void EmptyNotifyBoundsFunc( CPL_UNUSED double dfXMin, CPL_UNUSED double dfYMin,
                                   CPL_UNUSED double dfXMax, CPL_UNUSED double dfYMax,
                                   CPL_UNUSED OSMContext* psCtxt, CPL_UNUSED void* user_data )
{
}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                          OSM_AddString()                             */
/************************************************************************/

static const char* OSM_AddString(OSMContext* psCtxt, const char* pszStr)
{
    char* pszRet;
    int nLen = (int)strlen(pszStr);
    if( psCtxt->nStrLength + nLen + 1 > psCtxt->nStrAllocated )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "String buffer too small");
        return "";
    }
    pszRet = psCtxt->pszStrBuf + psCtxt->nStrLength;
    memcpy(pszRet, pszStr, nLen);
    pszRet[nLen] = '\0';
    psCtxt->nStrLength += nLen + 1;
    return pszRet;
}


/************************************************************************/
/*                            OSM_Atoi64()                              */
/************************************************************************/

static GIntBig OSM_Atoi64( const char *pszString )
{
    GIntBig    iValue;

#if defined(__MSVCRT__) || (defined(WIN32) && defined(_MSC_VER))
    iValue = (GIntBig)_atoi64( pszString );
# elif HAVE_ATOLL
    iValue = atoll( pszString );
#else
    iValue = atol( pszString );
#endif

    return iValue;
}

/************************************************************************/
/*                      OSM_XML_startElementCbk()                       */
/************************************************************************/

static void XMLCALL OSM_XML_startElementCbk(void *pUserData, const char *pszName,
                                            const char **ppszAttr)
{
    OSMContext* psCtxt = (OSMContext*) pUserData;
    const char** ppszIter = ppszAttr;

    if (psCtxt->bStopParsing) return;

    psCtxt->nWithoutEventCounter = 0;

    if( psCtxt->bTryToFetchBounds )
    {
        if( strcmp(pszName, "bounds") == 0 ||
            strcmp(pszName, "bound") == 0 /* osmosis uses bound */ )
        {
            int nCountCoords = 0;

            psCtxt->bTryToFetchBounds = FALSE;

            if( ppszIter )
            {
                while( ppszIter[0] != NULL )
                {
                    if( strcmp(ppszIter[0], "minlon") == 0 )
                    {
                        psCtxt->dfLeft = CPLAtof( ppszIter[1] );
                        nCountCoords ++;
                    }
                    else if( strcmp(ppszIter[0], "minlat") == 0 )
                    {
                        psCtxt->dfBottom = CPLAtof( ppszIter[1] );
                        nCountCoords ++;
                    }
                    else if( strcmp(ppszIter[0], "maxlon") == 0 )
                    {
                        psCtxt->dfRight = CPLAtof( ppszIter[1] );
                        nCountCoords ++;
                    }
                    else if( strcmp(ppszIter[0], "maxlat") == 0 )
                    {
                        psCtxt->dfTop = CPLAtof( ppszIter[1] );
                        nCountCoords ++;
                    }
                    else if( strcmp(ppszIter[0], "box") == 0  /* osmosis uses box */ )
                    {
                        char** papszTokens = CSLTokenizeString2( ppszIter[1], ",", 0 );
                        if( CSLCount(papszTokens) == 4 )
                        {
                            psCtxt->dfBottom = CPLAtof( papszTokens[0] );
                            psCtxt->dfLeft = CPLAtof( papszTokens[1] );
                            psCtxt->dfTop = CPLAtof( papszTokens[2] );
                            psCtxt->dfRight = CPLAtof( papszTokens[3] );
                            nCountCoords = 4;
                        }
                        CSLDestroy(papszTokens);
                    }
                    ppszIter += 2;
                }
            }

            if( nCountCoords == 4 )
            {
                psCtxt->pfnNotifyBounds(psCtxt->dfLeft, psCtxt->dfBottom,
                                        psCtxt->dfRight, psCtxt->dfTop,
                                        psCtxt, psCtxt->user_data);
            }
        }
    }

    if( !psCtxt->bInNode && !psCtxt->bInWay && !psCtxt->bInRelation &&
        strcmp(pszName, "node") == 0 )
    {
        psCtxt->bInNode = TRUE;
        psCtxt->bTryToFetchBounds = FALSE;

        psCtxt->nStrLength = 0;
        psCtxt->pszStrBuf[0] = '\0';
        psCtxt->nTags = 0;

        memset( &(psCtxt->pasNodes[0]), 0, sizeof(OSMNode) );
        psCtxt->pasNodes[0].sInfo.pszUserSID = "";

        if( ppszIter )
        {
            while( ppszIter[0] != NULL )
            {
                if( strcmp(ppszIter[0], "id") == 0 )
                {
                    psCtxt->pasNodes[0].nID = OSM_Atoi64( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "lat") == 0 )
                {
                    psCtxt->pasNodes[0].dfLat = CPLAtof( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "lon") == 0 )
                {
                    psCtxt->pasNodes[0].dfLon = CPLAtof( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "version") == 0 )
                {
                    psCtxt->pasNodes[0].sInfo.nVersion = atoi( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "changeset") == 0 )
                {
                    psCtxt->pasNodes[0].sInfo.nChangeset = OSM_Atoi64( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "user") == 0 )
                {
                    psCtxt->pasNodes[0].sInfo.pszUserSID = OSM_AddString(psCtxt, ppszIter[1]);
                }
                else if( strcmp(ppszIter[0], "uid") == 0 )
                {
                    psCtxt->pasNodes[0].sInfo.nUID = atoi( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "timestamp") == 0 )
                {
                    psCtxt->pasNodes[0].sInfo.ts.pszTimeStamp = OSM_AddString(psCtxt, ppszIter[1]);
                    psCtxt->pasNodes[0].sInfo.bTimeStampIsStr = 1;
                }
                ppszIter += 2;
            }
        }
    }

    else if( !psCtxt->bInNode && !psCtxt->bInWay && !psCtxt->bInRelation &&
             strcmp(pszName, "way") == 0 )
    {
        psCtxt->bInWay = TRUE;

        psCtxt->nStrLength = 0;
        psCtxt->pszStrBuf[0] = '\0';
        psCtxt->nTags = 0;

        memset( &(psCtxt->sWay), 0, sizeof(OSMWay) );
        psCtxt->sWay.sInfo.pszUserSID = "";

        if( ppszIter )
        {
            while( ppszIter[0] != NULL )
            {
                if( strcmp(ppszIter[0], "id") == 0 )
                {
                    psCtxt->sWay.nID = OSM_Atoi64( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "version") == 0 )
                {
                    psCtxt->sWay.sInfo.nVersion = atoi( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "changeset") == 0 )
                {
                    psCtxt->sWay.sInfo.nChangeset = OSM_Atoi64( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "user") == 0 )
                {
                    psCtxt->sWay.sInfo.pszUserSID = OSM_AddString(psCtxt, ppszIter[1]);
                }
                else if( strcmp(ppszIter[0], "uid") == 0 )
                {
                    psCtxt->sWay.sInfo.nUID = atoi( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "timestamp") == 0 )
                {
                    psCtxt->sWay.sInfo.ts.pszTimeStamp = OSM_AddString(psCtxt, ppszIter[1]);
                    psCtxt->sWay.sInfo.bTimeStampIsStr = 1;
                }
                ppszIter += 2;
            }
        }
    }

    else if( !psCtxt->bInNode && !psCtxt->bInWay && !psCtxt->bInRelation &&
             strcmp(pszName, "relation") == 0 )
    {
        psCtxt->bInRelation = TRUE;

        psCtxt->nStrLength = 0;
        psCtxt->pszStrBuf[0] = '\0';
        psCtxt->nTags = 0;

        memset( &(psCtxt->sRelation), 0, sizeof(OSMRelation) );
        psCtxt->sRelation.sInfo.pszUserSID = "";

        if( ppszIter )
        {
            while( ppszIter[0] != NULL )
            {
                if( strcmp(ppszIter[0], "id") == 0 )
                {
                    psCtxt->sRelation.nID = OSM_Atoi64( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "version") == 0 )
                {
                    psCtxt->sRelation.sInfo.nVersion = atoi( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "changeset") == 0 )
                {
                    psCtxt->sRelation.sInfo.nChangeset = OSM_Atoi64( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "user") == 0 )
                {
                    psCtxt->sRelation.sInfo.pszUserSID = OSM_AddString(psCtxt, ppszIter[1]);
                }
                else if( strcmp(ppszIter[0], "uid") == 0 )
                {
                    psCtxt->sRelation.sInfo.nUID = atoi( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "timestamp") == 0 )
                {
                    psCtxt->sRelation.sInfo.ts.pszTimeStamp = OSM_AddString(psCtxt, ppszIter[1]);
                    psCtxt->sRelation.sInfo.bTimeStampIsStr = 1;

                }
                ppszIter += 2;
            }
        }
    }

    else if( psCtxt->bInWay &&
             strcmp(pszName, "nd") == 0 )
    {
        if( ppszAttr != NULL && ppszAttr[0] != NULL &&
            strcmp(ppszAttr[0], "ref") == 0 )
        {
            if( psCtxt->sWay.nRefs < psCtxt->nNodeRefsAllocated )
            {
                psCtxt->panNodeRefs[psCtxt->sWay.nRefs] = OSM_Atoi64( ppszAttr[1] );
                psCtxt->sWay.nRefs ++;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too many nodes referenced in way " CPL_FRMT_GIB,
                         psCtxt->sWay.nID);
            }
        }
    }

    else if( psCtxt->bInRelation &&
             strcmp(pszName, "member") == 0 )
    {
        /* 300 is the recommanded value, but there are files with more than 2000 so we should be able */
        /* to realloc over that value */
        if (psCtxt->sRelation.nMembers >= psCtxt->nMembersAllocated)
        {
            OSMMember* pasMembersNew;
            int nMembersAllocated =
                MAX(psCtxt->nMembersAllocated * 2, psCtxt->sRelation.nMembers + 1);
            pasMembersNew = (OSMMember*) VSIRealloc(
                    psCtxt->pasMembers,
                    nMembersAllocated * sizeof(OSMMember));
            if( pasMembersNew == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot allocate enough memory to store members of relation " CPL_FRMT_GIB,
                        psCtxt->sRelation.nID);
                return;
            }
            psCtxt->nMembersAllocated = nMembersAllocated;
            psCtxt->pasMembers = pasMembersNew;
        }

        OSMMember* psMember = &(psCtxt->pasMembers[psCtxt->sRelation.nMembers]);
        psCtxt->sRelation.nMembers ++;

        psMember->nID = 0;
        psMember->pszRole = "";
        psMember->eType = MEMBER_NODE;

        if( ppszIter )
        {
            while( ppszIter[0] != NULL )
            {
                if( strcmp(ppszIter[0], "ref") == 0 )
                {
                    psMember->nID = OSM_Atoi64( ppszIter[1] );
                }
                else if( strcmp(ppszIter[0], "type") == 0 )
                {
                    if( strcmp( ppszIter[1], "node") == 0 )
                        psMember->eType = MEMBER_NODE;
                    else if( strcmp( ppszIter[1], "way") == 0 )
                        psMember->eType = MEMBER_WAY;
                    else if( strcmp( ppszIter[1], "relation") == 0 )
                        psMember->eType = MEMBER_RELATION;
                }
                else if( strcmp(ppszIter[0], "role") == 0 )
                {
                    psMember->pszRole = OSM_AddString(psCtxt, ppszIter[1]);
                }
                ppszIter += 2;
            }
        }
    }
    else if( (psCtxt->bInNode || psCtxt->bInWay || psCtxt->bInRelation) &&
             strcmp(pszName, "tag") == 0 )
    {
        if( psCtxt->nTags < psCtxt->nTagsAllocated )
        {
            OSMTag* psTag = &(psCtxt->pasTags[psCtxt->nTags]);
            psCtxt->nTags ++;

            psTag->pszK = "";
            psTag->pszV = "";

            if( ppszIter )
            {
                while( ppszIter[0] != NULL )
                {
                    if( ppszIter[0][0] == 'k' )
                    {
                        psTag->pszK = OSM_AddString(psCtxt, ppszIter[1]);
                    }
                    else if( ppszIter[0][0] == 'v' )
                    {
                        psTag->pszV = OSM_AddString(psCtxt, ppszIter[1]);
                    }
                    ppszIter += 2;
                }
            }
        }
        else
        {
            if (psCtxt->bInNode)
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Too many tags in node " CPL_FRMT_GIB,
                         psCtxt->pasNodes[0].nID);
            else if (psCtxt->bInWay)
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Too many tags in way " CPL_FRMT_GIB,
                         psCtxt->sWay.nID);
            else if (psCtxt->bInRelation)
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Too many tags in relation " CPL_FRMT_GIB,
                         psCtxt->sRelation.nID);
        }
    }
}

/************************************************************************/
/*                       OSM_XML_endElementCbk()                        */
/************************************************************************/

static void XMLCALL OSM_XML_endElementCbk(void *pUserData, const char *pszName)
{
    OSMContext* psCtxt = (OSMContext*) pUserData;

    if (psCtxt->bStopParsing) return;

    psCtxt->nWithoutEventCounter = 0;

    if( psCtxt->bInNode && strcmp(pszName, "node") == 0 )
    {
        psCtxt->pasNodes[0].nTags = psCtxt->nTags;
        psCtxt->pasNodes[0].pasTags = psCtxt->pasTags;

        psCtxt->pfnNotifyNodes(1, psCtxt->pasNodes, psCtxt, psCtxt->user_data);

        psCtxt->bHasFoundFeature = TRUE;

        psCtxt->bInNode = FALSE;
    }

    else
    if( psCtxt->bInWay && strcmp(pszName, "way") == 0 )
    {
        psCtxt->sWay.nTags = psCtxt->nTags;
        psCtxt->sWay.pasTags = psCtxt->pasTags;

        psCtxt->sWay.panNodeRefs = psCtxt->panNodeRefs;

        psCtxt->pfnNotifyWay(&(psCtxt->sWay), psCtxt, psCtxt->user_data);

        psCtxt->bHasFoundFeature = TRUE;

        psCtxt->bInWay = FALSE;
    }

    else
    if( psCtxt->bInRelation && strcmp(pszName, "relation") == 0 )
    {
        psCtxt->sRelation.nTags = psCtxt->nTags;
        psCtxt->sRelation.pasTags = psCtxt->pasTags;

        psCtxt->sRelation.pasMembers = psCtxt->pasMembers;

        psCtxt->pfnNotifyRelation(&(psCtxt->sRelation), psCtxt, psCtxt->user_data);

        psCtxt->bHasFoundFeature = TRUE;

        psCtxt->bInRelation = FALSE;
    }
}
/************************************************************************/
/*                           dataHandlerCbk()                           */
/************************************************************************/

static void XMLCALL OSM_XML_dataHandlerCbk(void *pUserData,
                                           CPL_UNUSED const char *data,
                                           CPL_UNUSED int nLen)
{
    OSMContext* psCtxt = (OSMContext*) pUserData;

    if (psCtxt->bStopParsing) return;

    psCtxt->nWithoutEventCounter = 0;

    psCtxt->nDataHandlerCounter ++;
    if (psCtxt->nDataHandlerCounter >= XML_BUFSIZE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(psCtxt->hXMLParser, XML_FALSE);
        psCtxt->bStopParsing = TRUE;
        return;
    }
}

/************************************************************************/
/*                          XML_ProcessBlock()                          */
/************************************************************************/

static OSMRetCode XML_ProcessBlock(OSMContext* psCtxt)
{
    if( psCtxt->bEOF )
        return OSM_EOF;
    if( psCtxt->bStopParsing )
        return OSM_ERROR;

    psCtxt->bHasFoundFeature = FALSE;
    psCtxt->nWithoutEventCounter = 0;

    do
    {
        int eErr;
        unsigned int nLen;

        psCtxt->nDataHandlerCounter = 0;

        nLen = (unsigned int)VSIFReadL( psCtxt->pabyBlob, 1,
                                        XML_BUFSIZE, psCtxt->fp );

        psCtxt->nBytesRead += nLen;

        psCtxt->bEOF = VSIFEofL(psCtxt->fp);
        eErr = XML_Parse(psCtxt->hXMLParser, (const char*) psCtxt->pabyBlob,
                         nLen, psCtxt->bEOF );

        if (eErr == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of OSM file failed : %s "
                     "at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(psCtxt->hXMLParser)),
                     (int)XML_GetCurrentLineNumber(psCtxt->hXMLParser),
                     (int)XML_GetCurrentColumnNumber(psCtxt->hXMLParser));
            psCtxt->bStopParsing = TRUE;
        }
        psCtxt->nWithoutEventCounter ++;
    } while (!psCtxt->bEOF && !psCtxt->bStopParsing &&
             psCtxt->bHasFoundFeature == FALSE &&
             psCtxt->nWithoutEventCounter < 10);

    if (psCtxt->nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        psCtxt->bStopParsing = TRUE;
    }

    return psCtxt->bStopParsing ? OSM_ERROR : psCtxt->bEOF ? OSM_EOF : OSM_OK;
}

#endif

/************************************************************************/
/*                              OSM_Open()                              */
/************************************************************************/

OSMContext* OSM_Open( const char* pszFilename,
                      NotifyNodesFunc pfnNotifyNodes,
                      NotifyWayFunc pfnNotifyWay,
                      NotifyRelationFunc pfnNotifyRelation,
                      NotifyBoundsFunc pfnNotifyBounds,
                      void* user_data )
{
    OSMContext* psCtxt;
    GByte abyHeader[1024];
    int nRead;
    VSILFILE* fp;
    int i;
    int bPBF = FALSE;

    fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return NULL;

    nRead = (int)VSIFReadL(abyHeader, 1, sizeof(abyHeader)-1, fp);
    abyHeader[nRead] = '\0';

    if( strstr((const char*)abyHeader, "<osm") != NULL )
    {
        /* OSM XML */
#ifndef HAVE_EXPAT
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OSM XML detected, but Expat parser not available");
        VSIFCloseL(fp);
        return NULL;
#endif
    }
    else
    {
        int nLimitI = nRead - strlen("OSMHeader");
        for(i = 0; i < nLimitI; i++)
        {
            if( memcmp(abyHeader + i, "OSMHeader", strlen("OSMHeader") ) == 0 )
            {
                bPBF = TRUE;
                break;
            }
        }
        if( !bPBF )
        {
            VSIFCloseL(fp);
            return NULL;
        }
    }

    VSIFSeekL(fp, 0, SEEK_SET);

    psCtxt = (OSMContext*) VSIMalloc(sizeof(OSMContext));
    if (psCtxt == NULL)
    {
        VSIFCloseL(fp);
        return NULL;
    }
    memset(psCtxt, 0, sizeof(OSMContext));
    psCtxt->bPBF = bPBF;
    psCtxt->fp = fp;
    psCtxt->pfnNotifyNodes = pfnNotifyNodes;
    if( pfnNotifyNodes == NULL )
        psCtxt->pfnNotifyNodes = EmptyNotifyNodesFunc;
    psCtxt->pfnNotifyWay = pfnNotifyWay;
    if( pfnNotifyWay == NULL )
        psCtxt->pfnNotifyWay = EmptyNotifyWayFunc;
    psCtxt->pfnNotifyRelation = pfnNotifyRelation;
    if( pfnNotifyRelation == NULL )
        psCtxt->pfnNotifyRelation = EmptyNotifyRelationFunc;
    psCtxt->pfnNotifyBounds = pfnNotifyBounds;
    if( pfnNotifyBounds == NULL )
        psCtxt->pfnNotifyBounds = EmptyNotifyBoundsFunc;
    psCtxt->user_data = user_data;

    if( bPBF )
    {
        psCtxt->nBlobSizeAllocated = 64 * 1024 + EXTRA_BYTES;
    }
#ifdef HAVE_EXPAT
    else
    {
        psCtxt->nBlobSizeAllocated = XML_BUFSIZE;

        psCtxt->nStrAllocated = 65536;
        psCtxt->pszStrBuf = (char*) VSIMalloc(psCtxt->nStrAllocated);
        if( psCtxt->pszStrBuf )
            psCtxt->pszStrBuf[0] = '\0';

        psCtxt->hXMLParser = OGRCreateExpatXMLParser();
        XML_SetUserData(psCtxt->hXMLParser, psCtxt);
        XML_SetElementHandler(psCtxt->hXMLParser,
                              OSM_XML_startElementCbk,
                              OSM_XML_endElementCbk);
        XML_SetCharacterDataHandler(psCtxt->hXMLParser, OSM_XML_dataHandlerCbk);

        psCtxt->bTryToFetchBounds = TRUE;

        psCtxt->nNodesAllocated = 1;
        psCtxt->pasNodes = (OSMNode*) VSIMalloc(sizeof(OSMNode) * psCtxt->nNodesAllocated);

        psCtxt->nTagsAllocated = 256;
        psCtxt->pasTags = (OSMTag*) VSIMalloc(sizeof(OSMTag) * psCtxt->nTagsAllocated);

        /* 300 is the recommanded value, but there are files with more than 2000 so we should be able */
        /* to realloc over that value */
        psCtxt->nMembersAllocated = 2000;
        psCtxt->pasMembers = (OSMMember*) VSIMalloc(sizeof(OSMMember) * psCtxt->nMembersAllocated);

        psCtxt->nNodeRefsAllocated = 2000;
        psCtxt->panNodeRefs = (GIntBig*) VSIMalloc(sizeof(GIntBig) * psCtxt->nNodeRefsAllocated);

        if( psCtxt->pszStrBuf == NULL ||
            psCtxt->pasNodes == NULL ||
            psCtxt->pasTags == NULL ||
            psCtxt->pasMembers == NULL ||
            psCtxt->panNodeRefs == NULL )
        {
            OSM_Close(psCtxt);
            return NULL;
        }

    }
#endif

    psCtxt->pabyBlob = (GByte*)VSIMalloc(psCtxt->nBlobSizeAllocated);
    if( psCtxt->pabyBlob == NULL )
    {
        OSM_Close(psCtxt);
        return NULL;
    }

    return psCtxt;
}

/************************************************************************/
/*                              OSM_Close()                             */
/************************************************************************/

void OSM_Close(OSMContext* psCtxt)
{
    if( psCtxt == NULL )
        return;

#ifdef HAVE_EXPAT
    if( !psCtxt->bPBF )
    {
        if (psCtxt->hXMLParser)
            XML_ParserFree(psCtxt->hXMLParser);

        CPLFree(psCtxt->pszStrBuf); /* only for XML case ! */
    }
#endif

    VSIFree(psCtxt->pabyBlob);
    VSIFree(psCtxt->pabyUncompressed);
    VSIFree(psCtxt->panStrOff);
    VSIFree(psCtxt->pasNodes);
    VSIFree(psCtxt->pasTags);
    VSIFree(psCtxt->pasMembers);
    VSIFree(psCtxt->panNodeRefs);

    VSIFCloseL(psCtxt->fp);
    VSIFree(psCtxt);
}
/************************************************************************/
/*                          OSM_ResetReading()                          */
/************************************************************************/

void OSM_ResetReading( OSMContext* psCtxt )
{
    VSIFSeekL(psCtxt->fp, 0, SEEK_SET);

    psCtxt->nBytesRead = 0;

#ifdef HAVE_EXPAT
    if( !psCtxt->bPBF )
    {
        XML_ParserFree(psCtxt->hXMLParser);
        psCtxt->hXMLParser = OGRCreateExpatXMLParser();
        XML_SetUserData(psCtxt->hXMLParser, psCtxt);
        XML_SetElementHandler(psCtxt->hXMLParser,
                              OSM_XML_startElementCbk,
                              OSM_XML_endElementCbk);
        XML_SetCharacterDataHandler(psCtxt->hXMLParser, OSM_XML_dataHandlerCbk);
        psCtxt->bEOF = FALSE;
        psCtxt->bStopParsing = FALSE;
        psCtxt->nStrLength = 0;
        psCtxt->pszStrBuf[0] = '\0';
        psCtxt->nTags = 0;

        psCtxt->bTryToFetchBounds = TRUE;
        psCtxt->bInNode = FALSE;
        psCtxt->bInWay = FALSE;
        psCtxt->bInRelation = FALSE;
    }
#endif
}

/************************************************************************/
/*                          OSM_ProcessBlock()                          */
/************************************************************************/

static OSMRetCode PBF_ProcessBlock(OSMContext* psCtxt)
{
    int nRet = FALSE;
    GByte abyHeaderSize[4];
    unsigned int nHeaderSize;
    unsigned int nBlobSize = 0;
    BlobType eType;

    if (VSIFReadL(abyHeaderSize, 4, 1, psCtxt->fp) != 1)
    {
        return OSM_EOF;
    }
    nHeaderSize = (abyHeaderSize[0] << 24) | (abyHeaderSize[1] << 16) |
                    (abyHeaderSize[2] << 8) | abyHeaderSize[3];

    psCtxt->nBytesRead += 4;

    /* printf("nHeaderSize = %d\n", nHeaderSize); */
    if (nHeaderSize > 64 * 1024)
        GOTO_END_ERROR;
    if (VSIFReadL(psCtxt->pabyBlob, 1, nHeaderSize, psCtxt->fp) != nHeaderSize)
        GOTO_END_ERROR;

    psCtxt->nBytesRead += nHeaderSize;

    memset(psCtxt->pabyBlob + nHeaderSize, 0, EXTRA_BYTES);
    nRet = ReadBlobHeader(psCtxt->pabyBlob, psCtxt->pabyBlob + nHeaderSize, &nBlobSize, &eType);
    if (!nRet || eType == BLOB_UNKNOW)
        GOTO_END_ERROR;

    if (nBlobSize > 64*1024*1024)
        GOTO_END_ERROR;
    if (nBlobSize > psCtxt->nBlobSizeAllocated)
    {
        GByte* pabyBlobNew;
        psCtxt->nBlobSizeAllocated = MAX(psCtxt->nBlobSizeAllocated * 2, nBlobSize);
        pabyBlobNew = (GByte*)VSIRealloc(psCtxt->pabyBlob,
                                        psCtxt->nBlobSizeAllocated + EXTRA_BYTES);
        if( pabyBlobNew == NULL )
            GOTO_END_ERROR;
        psCtxt->pabyBlob = pabyBlobNew;
    }
    if (VSIFReadL(psCtxt->pabyBlob, 1, nBlobSize, psCtxt->fp) != nBlobSize)
        GOTO_END_ERROR;

    psCtxt->nBytesRead += nBlobSize;

    memset(psCtxt->pabyBlob + nBlobSize, 0, EXTRA_BYTES);
    nRet = ReadBlob(psCtxt->pabyBlob, nBlobSize, eType,
                    psCtxt);
    if (!nRet)
        GOTO_END_ERROR;

    return OSM_OK;

end_error:

    return OSM_ERROR;
}

/************************************************************************/
/*                          OSM_ProcessBlock()                          */
/************************************************************************/

OSMRetCode OSM_ProcessBlock(OSMContext* psCtxt)
{
#ifdef HAVE_EXPAT
    if( psCtxt->bPBF )
        return PBF_ProcessBlock(psCtxt);
    else
        return XML_ProcessBlock(psCtxt);
#else
    return PBF_ProcessBlock(psCtxt);
#endif
}

/************************************************************************/
/*                          OSM_GetBytesRead()                          */
/************************************************************************/

GUIntBig OSM_GetBytesRead( OSMContext* psCtxt )
{
    return psCtxt->nBytesRead;
}
