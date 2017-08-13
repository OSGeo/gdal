/******************************************************************************
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

#define DO_NOT_DEFINE_READ_VARINT64

#include "osm_parser.h"
#include "gpb.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_multiproc.h"
#include "cpl_worker_thread_pool.h"

#ifdef HAVE_EXPAT
#include "ogr_expat.h"
#endif

#include <algorithm>

CPL_CVSID("$Id$")

// The buffer that are passed to GPB decoding are extended with 0's
// to be sure that we will be able to read a single 64bit value without
// doing checks for each byte.
static const int EXTRA_BYTES = 1;

static const int XML_BUFSIZE = 64 * 1024;

// Per OSM PBF spec
const unsigned int MAX_BLOB_HEADER_SIZE = 64 * 1024;

// Per OSM PBF spec (usually much smaller !)
const unsigned int MAX_BLOB_SIZE = 64 * 1024 * 1024;

// GDAL implementation limits
const unsigned int MAX_ACC_BLOB_SIZE = 50 * 1024 * 1024;
const unsigned int MAX_ACC_UNCOMPRESSED_SIZE = 100 * 1024 * 1024;
const int N_MAX_JOBS = 1024;

/************************************************************************/
/*                            INIT_INFO()                               */
/************************************************************************/

static void INIT_INFO( OSMInfo *sInfo )
{
    sInfo->ts.nTimeStamp = 0;
    sInfo->nChangeset = 0;
    sInfo->nVersion = 0;
    sInfo->nUID = 0;
    sInfo->bTimeStampIsStr = false;
    sInfo->pszUserSID = NULL;
}

/************************************************************************/
/*                            _OSMContext                               */
/************************************************************************/

typedef struct
{
    const GByte *pabySrc;
    size_t       nSrcSize;
    GByte       *pabyDstBase;
    size_t       nDstOffset;
    size_t       nDstSize;
    bool         bStatus;
} DecompressionJob;

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

    // concatenated protocol buffer messages BLOB_OSMDATA, or single BLOB_OSMHEADER
    GByte         *pabyBlob;
    unsigned int   nBlobSizeAllocated;
    unsigned int   nBlobOffset;
    unsigned int   nBlobSize;

    GByte         *pabyBlobHeader; // MAX_BLOB_HEADER_SIZE+EXTRA_BYTES large

    CPLWorkerThreadPool* poWTP;

    GByte         *pabyUncompressed;
    unsigned int   nUncompressedAllocated;
    unsigned int   nTotalUncompressedSize;

    DecompressionJob asJobs[N_MAX_JOBS];
    int              nJobs;
    int              iNextJob;

#ifdef HAVE_EXPAT
    XML_Parser     hXMLParser;
    bool           bEOF;
    bool           bStopParsing;
    bool           bHasFoundFeature;
    int            nWithoutEventCounter;
    int            nDataHandlerCounter;

    unsigned int   nStrLength;
    unsigned int   nTags;

    bool           bInNode;
    bool           bInWay;
    bool           bInRelation;

    OSMWay         sWay;
    OSMRelation    sRelation;

    bool           bTryToFetchBounds;
#endif

    VSILFILE      *fp;

    bool           bPBF;

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

static const int BLOBHEADER_IDX_TYPE = 1;
static const int BLOBHEADER_IDX_INDEXDATA = 2;
static const int BLOBHEADER_IDX_DATASIZE = 3;

typedef enum
{
    BLOB_UNKNOWN,
    BLOB_OSMHEADER,
    BLOB_OSMDATA
} BlobType;

static
bool ReadBlobHeader( GByte* pabyData, GByte* pabyDataLimit,
                     unsigned int* pnBlobSize, BlobType* peBlobType )
{
    *pnBlobSize = 0;
    *peBlobType = BLOB_UNKNOWN;

    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(BLOBHEADER_IDX_TYPE, WT_DATA) )
        {
            unsigned int nDataLength = 0;
            READ_SIZE(pabyData, pabyDataLimit, nDataLength);

            if( nDataLength == 7 && memcmp(pabyData, "OSMData", 7) == 0 )
            {
                *peBlobType = BLOB_OSMDATA;
            }
            else if( nDataLength == 9 && memcmp(pabyData, "OSMHeader", 9) == 0 )
            {
                *peBlobType = BLOB_OSMHEADER;
            }

            pabyData += nDataLength;
        }
        else if( nKey == MAKE_KEY(BLOBHEADER_IDX_INDEXDATA, WT_DATA) )
        {
            // Ignored if found.
            unsigned int nDataLength = 0;
            READ_SIZE(pabyData, pabyDataLimit, nDataLength);
            pabyData += nDataLength;
        }
        else if( nKey == MAKE_KEY(BLOBHEADER_IDX_DATASIZE, WT_VARINT) )
        {
            unsigned int nBlobSize = 0;
            READ_VARUINT32(pabyData, pabyDataLimit, nBlobSize);
            // printf("nBlobSize = %d\n", nBlobSize);
            *pnBlobSize = nBlobSize;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    return pabyData == pabyDataLimit;

end_error:
    return false;
}

/************************************************************************/
/*                          ReadHeaderBBox()                            */
/************************************************************************/

static const int HEADERBBOX_IDX_LEFT = 1;
static const int HEADERBBOX_IDX_RIGHT = 2;
static const int HEADERBBOX_IDX_TOP = 3;
static const int HEADERBBOX_IDX_BOTTOM = 4;

static
bool ReadHeaderBBox( GByte* pabyData, GByte* pabyDataLimit,
                     OSMContext* psCtxt )
{
    psCtxt->dfLeft = 0.0;
    psCtxt->dfRight = 0.0;
    psCtxt->dfTop = 0.0;
    psCtxt->dfBottom = 0.0;

    /* printf(">ReadHeaderBBox\n"); */

    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(HEADERBBOX_IDX_LEFT, WT_VARINT) )
        {
            GIntBig nLeft = 0;
            READ_VARSINT64(pabyData, pabyDataLimit, nLeft);
            psCtxt->dfLeft = nLeft * 1e-9;
        }
        else if( nKey == MAKE_KEY(HEADERBBOX_IDX_RIGHT, WT_VARINT) )
        {
            GIntBig nRight = 0;
            READ_VARSINT64(pabyData, pabyDataLimit, nRight);
            psCtxt->dfRight = nRight * 1e-9;
        }
        else if( nKey == MAKE_KEY(HEADERBBOX_IDX_TOP, WT_VARINT) )
        {
            GIntBig nTop = 0;
            READ_VARSINT64(pabyData, pabyDataLimit, nTop);
            psCtxt->dfTop = nTop * 1e-9;
        }
        else if( nKey == MAKE_KEY(HEADERBBOX_IDX_BOTTOM, WT_VARINT) )
        {
            GIntBig nBottom = 0;
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
    return false;
}

/************************************************************************/
/*                          ReadOSMHeader()                             */
/************************************************************************/

static const int OSMHEADER_IDX_BBOX              = 1;
static const int OSMHEADER_IDX_REQUIRED_FEATURES = 4;
static const int OSMHEADER_IDX_OPTIONAL_FEATURES = 5;
static const int OSMHEADER_IDX_WRITING_PROGRAM   = 16;
static const int OSMHEADER_IDX_SOURCE            = 17;

/* Ignored */
static const int OSMHEADER_IDX_OSMOSIS_REPLICATION_TIMESTAMP  = 32;
static const int OSMHEADER_IDX_OSMOSIS_REPLICATION_SEQ_NUMBER = 33;
static const int OSMHEADER_IDX_OSMOSIS_REPLICATION_BASE_URL   = 34;

static
bool ReadOSMHeader( GByte* pabyData, GByte* pabyDataLimit,
                    OSMContext* psCtxt )
{
    char* pszTxt = NULL;

    // TODO(schwehr): Remove goto macros.

    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(OSMHEADER_IDX_BBOX, WT_DATA) )
        {
            unsigned int nBBOXSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nBBOXSize);

            if( !ReadHeaderBBox(pabyData, pabyData + nBBOXSize, psCtxt) )
                GOTO_END_ERROR;

            pabyData += nBBOXSize;
        }
        else if( nKey == MAKE_KEY(OSMHEADER_IDX_REQUIRED_FEATURES, WT_DATA) )
        {
            READ_TEXT(pabyData, pabyDataLimit, pszTxt);
            // printf("OSMHEADER_IDX_REQUIRED_FEATURES = %s\n", pszTxt)
            if( !(strcmp(pszTxt, "OsmSchema-V0.6") == 0 ||
                  strcmp(pszTxt, "DenseNodes") == 0) )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "Error: unsupported required feature : %s",
                        pszTxt);
                VSIFree(pszTxt);
                GOTO_END_ERROR;  // TODO(schwehr): Get rid of goto.
            }
            VSIFree(pszTxt);
        }
        else if( nKey == MAKE_KEY(OSMHEADER_IDX_OPTIONAL_FEATURES, WT_DATA) )
        {
            READ_TEXT(pabyData, pabyDataLimit, pszTxt);
            // printf("OSMHEADER_IDX_OPTIONAL_FEATURES = %s\n", pszTxt);
            VSIFree(pszTxt);
        }
        else if( nKey == MAKE_KEY(OSMHEADER_IDX_WRITING_PROGRAM, WT_DATA) )
        {
            READ_TEXT(pabyData, pabyDataLimit, pszTxt);
            // printf("OSMHEADER_IDX_WRITING_PROGRAM = %s\n", pszTxt);
            VSIFree(pszTxt);
        }
        else if( nKey == MAKE_KEY(OSMHEADER_IDX_SOURCE, WT_DATA) )
        {
            READ_TEXT(pabyData, pabyDataLimit, pszTxt);
            // printf("OSMHEADER_IDX_SOURCE = %s\n", pszTxt);
            VSIFree(pszTxt);
        }
        else if( nKey == MAKE_KEY(OSMHEADER_IDX_OSMOSIS_REPLICATION_TIMESTAMP,
                                  WT_VARINT) )
        {
            SKIP_VARINT(pabyData, pabyDataLimit);
        }
        else if( nKey == MAKE_KEY(OSMHEADER_IDX_OSMOSIS_REPLICATION_SEQ_NUMBER,
                                  WT_VARINT) )
        {
            SKIP_VARINT(pabyData, pabyDataLimit);
        }
        else if( nKey == MAKE_KEY(OSMHEADER_IDX_OSMOSIS_REPLICATION_BASE_URL,
                                  WT_DATA) )
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
    return false;
}

/************************************************************************/
/*                         ReadStringTable()                            */
/************************************************************************/

static const int READSTRINGTABLE_IDX_STRING = 1;

static
bool ReadStringTable( GByte* pabyData, GByte* pabyDataLimit,
                      OSMContext* psCtxt )
{
    char* pszStrBuf = (char*)pabyData;

    unsigned int nStrCount = 0;
    int* panStrOff = psCtxt->panStrOff;

    psCtxt->pszStrBuf = pszStrBuf;

    if( (unsigned int)(pabyDataLimit - pabyData) > psCtxt->nStrAllocated )
    {
        psCtxt->nStrAllocated = std::max(psCtxt->nStrAllocated * 2,
                                          (unsigned int)(pabyDataLimit - pabyData));
        int* panStrOffNew = (int*) VSI_REALLOC_VERBOSE(
            panStrOff, psCtxt->nStrAllocated * sizeof(int));
        if( panStrOffNew == NULL )
            GOTO_END_ERROR;
        panStrOff = panStrOffNew;
    }

    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        while (nKey == MAKE_KEY(READSTRINGTABLE_IDX_STRING, WT_DATA))
        {
            unsigned int nDataLength = 0;
            READ_SIZE(pabyData, pabyDataLimit, nDataLength);

            panStrOff[nStrCount ++] = static_cast<int>(pabyData - (GByte*)pszStrBuf);
            GByte* pbSaved = &pabyData[nDataLength];

            pabyData += nDataLength;

            if( pabyData < pabyDataLimit )
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

        if( pabyData < pabyDataLimit )
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
/*                     AddWithOverflowAccepted()                        */
/************************************************************************/

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static GIntBig AddWithOverflowAccepted(GIntBig a, GIntBig b)
{
    // Assumes complement-to-two signed integer representation and that
    // the compiler will safely cast a negative number to unsigned and a
    // big unsigned to negative integer.
    return static_cast<GIntBig>(
                        static_cast<GUIntBig>(a) + static_cast<GUIntBig>(b));
}

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static int AddWithOverflowAccepted(int a, int b)
{
    // Assumes complement-to-two signed integer representation and that
    // the compiler will safely cast a negative number to unsigned and a
    // big unsigned to negative integer.
    return static_cast<int>(
                        static_cast<unsigned>(a) + static_cast<unsigned>(b));
}

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static unsigned AddWithOverflowAccepted(unsigned a, int b)
{
    // Assumes complement-to-two signed integer representation and that
    // the compiler will safely cast a negative number to unsigned.
    return a + static_cast<unsigned>(b);
}

/************************************************************************/
/*                         ReadDenseNodes()                             */
/************************************************************************/

static const int DENSEINFO_IDX_VERSION   = 1;
static const int DENSEINFO_IDX_TIMESTAMP = 2;
static const int DENSEINFO_IDX_CHANGESET = 3;
static const int DENSEINFO_IDX_UID       = 4;
static const int DENSEINFO_IDX_USER_SID  = 5;
static const int DENSEINFO_IDX_VISIBLE   = 6;

static const int DENSENODES_IDX_ID        = 1;
static const int DENSENODES_IDX_DENSEINFO = 5;
static const int DENSENODES_IDX_LAT       = 8;
static const int DENSENODES_IDX_LON       = 9;
static const int DENSENODES_IDX_KEYVALS   = 10;

static
bool ReadDenseNodes( GByte* pabyData, GByte* pabyDataLimit,
                     OSMContext* psCtxt )
{
    GByte* pabyDataIDs = NULL;
    GByte* pabyDataIDsLimit = NULL;
    GByte* pabyDataLat = NULL;
    GByte* pabyDataLon = NULL;
    GByte* apabyData[DENSEINFO_IDX_VISIBLE] = {NULL, NULL, NULL, NULL, NULL, NULL};
    GByte* pabyDataKeyVal = NULL;
    unsigned int nMaxTags = 0;

    /* printf(">ReadDenseNodes\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(DENSENODES_IDX_ID, WT_DATA) )
        {
            unsigned int nSize = 0;

            if( pabyDataIDs != NULL )
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( nSize > psCtxt->nNodesAllocated )
            {
                psCtxt->nNodesAllocated = std::max(psCtxt->nNodesAllocated * 2,
                                                 nSize);
                OSMNode* pasNodesNew = (OSMNode*) VSI_REALLOC_VERBOSE(
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
            unsigned int nSize = 0;

            READ_SIZE(pabyData, pabyDataLimit, nSize);

            /* Inline reading of DenseInfo structure */

            GByte* pabyDataNewLimit = pabyData + nSize;
            while(pabyData < pabyDataNewLimit)
            {
                READ_FIELD_KEY(nKey);

                const int nFieldNumber = GET_FIELDNUMBER(nKey);
                if( GET_WIRETYPE(nKey) == WT_DATA &&
                    nFieldNumber >= DENSEINFO_IDX_VERSION &&
                    nFieldNumber <= DENSEINFO_IDX_VISIBLE )
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
            if( pabyDataLat != NULL )
                GOTO_END_ERROR;
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);
            pabyDataLat = pabyData;
            pabyData += nSize;
        }
        else if( nKey == MAKE_KEY(DENSENODES_IDX_LON, WT_DATA) )
        {
            if( pabyDataLon != NULL )
                GOTO_END_ERROR;
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);
            pabyDataLon = pabyData;
            pabyData += nSize;
        }
        else if( nKey == MAKE_KEY(DENSENODES_IDX_KEYVALS, WT_DATA) )
        {
            if( pabyDataKeyVal != NULL )
                GOTO_END_ERROR;
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            pabyDataKeyVal = pabyData;
            nMaxTags = nSize / 2;

            if( nMaxTags > psCtxt->nTagsAllocated )
            {

                psCtxt->nTagsAllocated = std::max(
                    psCtxt->nTagsAllocated * 2, nMaxTags);
                OSMTag* pasTagsNew = (OSMTag*) VSI_REALLOC_VERBOSE(
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
            nID = AddWithOverflowAccepted(nID, nDelta1);
            nLat = AddWithOverflowAccepted(nLat, nDelta2);

            READ_VARSINT64(pabyDataLon, pabyDataLimit, nDelta1);
            nLon = AddWithOverflowAccepted(nLon, nDelta1);

            if( pabyDataTimeStamp )
            {
                READ_VARSINT64(pabyDataTimeStamp, pabyDataLimit, nDelta2);
                nTimeStamp = AddWithOverflowAccepted(nTimeStamp, nDelta2);
            }
            if( pabyDataChangeset )
            {
                READ_VARSINT64(pabyDataChangeset, pabyDataLimit, nDelta1);
                nChangeset = AddWithOverflowAccepted(nChangeset, nDelta1);
            }
            if( pabyDataVersion )
            {
                READ_VARINT32(pabyDataVersion, pabyDataLimit, nVersion);
            }
            if( pabyDataUID )
            {
                int nDeltaUID = 0;
                READ_VARSINT32(pabyDataUID, pabyDataLimit, nDeltaUID);
                nUID = AddWithOverflowAccepted(nUID, nDeltaUID);
            }
            if( pabyDataUserSID )
            {
                int nDeltaUserSID = 0;
                READ_VARSINT32(pabyDataUserSID, pabyDataLimit, nDeltaUserSID);
                nUserSID = AddWithOverflowAccepted(nUserSID, nDeltaUserSID);
                if( nUserSID >= nStrCount )
                    GOTO_END_ERROR;
            }
            /* if( pabyDataVisible )
                READ_VARINT32(pabyDataVisible, pabyDataLimit, nVisible); */

            if( pabyDataKeyVal != NULL && pasTags != NULL )
            {
                while( static_cast<unsigned>(nTags) < nMaxTags )
                {
                    unsigned int nKey, nVal;
                    READ_VARUINT32(pabyDataKeyVal, pabyDataLimit, nKey);
                    if( nKey == 0 )
                        break;
                    if( nKey >= nStrCount )
                        GOTO_END_ERROR;

                    READ_VARUINT32(pabyDataKeyVal, pabyDataLimit, nVal);
                    if( nVal >= nStrCount )
                        GOTO_END_ERROR;

                    pasTags[nTags].pszK = pszStrBuf + panStrOff[nKey];
                    pasTags[nTags].pszV = pszStrBuf + panStrOff[nVal];
                    nTags ++;

                    /* printf("nKey = %d, nVal = %d\n", nKey, nVal); */
                }
            }

            if( pasTags != NULL && nTags > nKVIndexStart )
                pasNodes[nNodes].pasTags = pasTags + nKVIndexStart;
            else
                pasNodes[nNodes].pasTags = NULL;
            pasNodes[nNodes].nTags = nTags - nKVIndexStart;

            pasNodes[nNodes].nID = nID;
            pasNodes[nNodes].dfLat = .000000001 * (psCtxt->nLatOffset + ((double)psCtxt->nGranularity * nLat));
            pasNodes[nNodes].dfLon = .000000001 * (psCtxt->nLonOffset + ((double)psCtxt->nGranularity * nLon));
            if( pasNodes[nNodes].dfLon < -180 || pasNodes[nNodes].dfLon > 180 ||
                pasNodes[nNodes].dfLat < -90 || pasNodes[nNodes].dfLat > 90 )
                GOTO_END_ERROR;
            pasNodes[nNodes].sInfo.bTimeStampIsStr = false;
            pasNodes[nNodes].sInfo.ts.nTimeStamp = nTimeStamp;
            pasNodes[nNodes].sInfo.nChangeset = nChangeset;
            pasNodes[nNodes].sInfo.nVersion = nVersion;
            pasNodes[nNodes].sInfo.nUID = nUID;
            if( nUserSID >= nStrCount )
                pasNodes[nNodes].sInfo.pszUserSID = "";
            else
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

static const int INFO_IDX_VERSION   = 1;
static const int INFO_IDX_TIMESTAMP = 2;
static const int INFO_IDX_CHANGESET = 3;
static const int INFO_IDX_UID       = 4;
static const int INFO_IDX_USER_SID  = 5;
static const int INFO_IDX_VISIBLE   = 6;

static
bool ReadOSMInfo( GByte* pabyData, GByte* pabyDataLimit,
                  OSMInfo* psInfo, OSMContext* psContext ) CPL_NO_INLINE;

static
bool ReadOSMInfo( GByte* pabyData, GByte* pabyDataLimit,
                  OSMInfo* psInfo, OSMContext* psContext )
{
    /* printf(">ReadOSMInfo\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(INFO_IDX_VERSION, WT_VARINT) )
        {
            READ_VARINT32(pabyData, pabyDataLimit, psInfo->nVersion);
        }
        else if( nKey == MAKE_KEY(INFO_IDX_TIMESTAMP, WT_VARINT) )
        {
            READ_VARINT64(pabyData, pabyDataLimit, psInfo->ts.nTimeStamp);
        }
        else if( nKey == MAKE_KEY(INFO_IDX_CHANGESET, WT_VARINT) )
        {
            READ_VARINT64(pabyData, pabyDataLimit, psInfo->nChangeset);
        }
        else if( nKey == MAKE_KEY(INFO_IDX_UID, WT_VARINT) )
        {
            READ_VARINT32(pabyData, pabyDataLimit, psInfo->nUID);
        }
        else if( nKey == MAKE_KEY(INFO_IDX_USER_SID, WT_VARINT) )
        {
            unsigned int nUserSID = 0;
            READ_VARUINT32(pabyData, pabyDataLimit, nUserSID);
            if( nUserSID < psContext->nStrCount)
                psInfo->pszUserSID = psContext->pszStrBuf +
                                     psContext->panStrOff[nUserSID];
        }
        else if( nKey == MAKE_KEY(INFO_IDX_VISIBLE, WT_VARINT) )
        {
            SKIP_VARINT(pabyData, pabyDataLimit);
            // int nVisible = 0;
            // READ_VARINT32(pabyData, pabyDataLimit, /*psInfo->*/nVisible);
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

    return false;
}

/************************************************************************/
/*                             ReadNode()                               */
/************************************************************************/

/* From https://github.com/openstreetmap/osmosis/blob/master/osmosis-osm-binary/src/main/protobuf/osmformat.proto */
/* The one advertized in http://wiki.openstreetmap.org/wiki/PBF_Format and */
/* used previously seem wrong/old-dated */

static const int NODE_IDX_ID   = 1;
static const int NODE_IDX_LAT  = 8;
static const int NODE_IDX_LON  = 9;
static const int NODE_IDX_KEYS = 2;
static const int NODE_IDX_VALS = 3;
static const int NODE_IDX_INFO = 4;

static
bool ReadNode( GByte* pabyData, GByte* pabyDataLimit,
               OSMContext* psCtxt )
{
    OSMNode sNode;

    sNode.nID = 0;
    sNode.dfLat = 0.0;
    sNode.dfLon = 0.0;
    INIT_INFO(&(sNode.sInfo));
    sNode.nTags = 0;
    sNode.pasTags = NULL;

    /* printf(">ReadNode\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(NODE_IDX_ID, WT_VARINT) )
        {
            READ_VARSINT64_NOCHECK(pabyData, pabyDataLimit, sNode.nID);
        }
        else if( nKey == MAKE_KEY(NODE_IDX_LAT, WT_VARINT) )
        {
            GIntBig nLat = 0;
            READ_VARSINT64_NOCHECK(pabyData, pabyDataLimit, nLat);
            sNode.dfLat =
                0.000000001 * (psCtxt->nLatOffset +
                               ((double)psCtxt->nGranularity * nLat));
        }
        else if( nKey == MAKE_KEY(NODE_IDX_LON, WT_VARINT) )
        {
            GIntBig nLon = 0;
            READ_VARSINT64_NOCHECK(pabyData, pabyDataLimit, nLon);
            sNode.dfLon =
                0.000000001 * (psCtxt->nLonOffset +
                               ((double)psCtxt->nGranularity * nLon));
        }
        else if( nKey == MAKE_KEY(NODE_IDX_KEYS, WT_DATA) )
        {
            unsigned int nSize = 0;
            GByte* pabyDataNewLimit = NULL;
            if( sNode.nTags != 0 )
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( nSize > psCtxt->nTagsAllocated )
            {
                psCtxt->nTagsAllocated = std::max(
                    psCtxt->nTagsAllocated * 2, nSize);
                OSMTag* pasTagsNew = (OSMTag*) VSI_REALLOC_VERBOSE(
                    psCtxt->pasTags,
                    psCtxt->nTagsAllocated * sizeof(OSMTag));
                if( pasTagsNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasTags = pasTagsNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                unsigned int nKey2 = 0;
                READ_VARUINT32(pabyData, pabyDataNewLimit, nKey2);

                if( nKey2 >= psCtxt->nStrCount )
                    GOTO_END_ERROR;

                psCtxt->pasTags[sNode.nTags].pszK = psCtxt->pszStrBuf +
                                              psCtxt->panStrOff[nKey2];
                psCtxt->pasTags[sNode.nTags].pszV = "";
                sNode.nTags ++;
            }
            if( pabyData != pabyDataNewLimit )
                GOTO_END_ERROR;
        }
        else if( nKey == MAKE_KEY(NODE_IDX_VALS, WT_DATA) )
        {
            unsigned int nIter = 0;
            if( sNode.nTags == 0 )
                GOTO_END_ERROR;
            // unsigned int nSize = 0;
            // READ_VARUINT32(pabyData, pabyDataLimit, nSize);
            SKIP_VARINT(pabyData, pabyDataLimit);

            for( ; nIter < sNode.nTags; nIter++)
            {
                unsigned int nVal = 0;
                READ_VARUINT32(pabyData, pabyDataLimit, nVal);

                if( nVal >= psCtxt->nStrCount )
                    GOTO_END_ERROR;

                psCtxt->pasTags[nIter].pszV = psCtxt->pszStrBuf +
                                              psCtxt->panStrOff[nVal];
            }
        }
        else if( nKey == MAKE_KEY(NODE_IDX_INFO, WT_DATA) )
        {
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( !ReadOSMInfo(pabyData, pabyDataLimit + nSize,
                             &sNode.sInfo, psCtxt) )
                GOTO_END_ERROR;

            pabyData += nSize;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    if( sNode.dfLon < -180 || sNode.dfLon > 180 ||
        sNode.dfLat < -90 || sNode.dfLat > 90 )
        GOTO_END_ERROR;

    if( pabyData != pabyDataLimit )
        GOTO_END_ERROR;

    if( sNode.nTags )
        sNode.pasTags = psCtxt->pasTags;
    else
        sNode.pasTags = NULL;
    psCtxt->pfnNotifyNodes(1, &sNode, psCtxt, psCtxt->user_data);

    // printf("<ReadNode\n");

    return true;

end_error:
    /* printf("<ReadNode\n"); */

    return false;
}

/************************************************************************/
/*                              ReadWay()                               */
/************************************************************************/

static const int WAY_IDX_ID   = 1;
static const int WAY_IDX_KEYS = 2;
static const int WAY_IDX_VALS = 3;
static const int WAY_IDX_INFO = 4;
static const int WAY_IDX_REFS = 8;

static
bool ReadWay( GByte* pabyData, GByte* pabyDataLimit,
              OSMContext* psCtxt )
{
    OSMWay sWay;
    sWay.nID = 0;
    INIT_INFO(&(sWay.sInfo));
    sWay.nTags = 0;
    sWay.nRefs = 0;

    // printf(">ReadWay\n");
    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(WAY_IDX_ID, WT_VARINT) )
        {
            READ_VARINT64(pabyData, pabyDataLimit, sWay.nID);
        }
        else if( nKey == MAKE_KEY(WAY_IDX_KEYS, WT_DATA) )
        {
            unsigned int nSize = 0;
            GByte* pabyDataNewLimit = NULL;
            if( sWay.nTags != 0 )
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( nSize > psCtxt->nTagsAllocated )
            {
                psCtxt->nTagsAllocated = std::max(
                    psCtxt->nTagsAllocated * 2, nSize);
                OSMTag* pasTagsNew = (OSMTag*) VSI_REALLOC_VERBOSE(
                    psCtxt->pasTags,
                    psCtxt->nTagsAllocated * sizeof(OSMTag));
                if( pasTagsNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasTags = pasTagsNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                unsigned int nKey2 = 0;
                READ_VARUINT32(pabyData, pabyDataNewLimit, nKey2);

                if( nKey2 >= psCtxt->nStrCount )
                    GOTO_END_ERROR;

                psCtxt->pasTags[sWay.nTags].pszK = psCtxt->pszStrBuf +
                                                   psCtxt->panStrOff[nKey2];
                psCtxt->pasTags[sWay.nTags].pszV = "";
                sWay.nTags ++;
            }
            if( pabyData != pabyDataNewLimit )
                GOTO_END_ERROR;
        }
        else if( nKey == MAKE_KEY(WAY_IDX_VALS, WT_DATA) )
        {
            unsigned int nIter = 0;
            if( sWay.nTags == 0 )
                GOTO_END_ERROR;
            // unsigned int nSize = 0;
            // READ_VARUINT32(pabyData, pabyDataLimit, nSize);
            SKIP_VARINT(pabyData, pabyDataLimit);

            for(; nIter < sWay.nTags; nIter ++)
            {
                unsigned int nVal = 0;
                READ_VARUINT32(pabyData, pabyDataLimit, nVal);

                if( nVal >= psCtxt->nStrCount )
                    GOTO_END_ERROR;

                psCtxt->pasTags[nIter].pszV = psCtxt->pszStrBuf +
                                              psCtxt->panStrOff[nVal];
            }
        }
        else if( nKey == MAKE_KEY(WAY_IDX_INFO, WT_DATA) )
        {
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( !ReadOSMInfo(pabyData, pabyData + nSize, &sWay.sInfo, psCtxt) )
                GOTO_END_ERROR;

            pabyData += nSize;
        }
        else if( nKey == MAKE_KEY(WAY_IDX_REFS, WT_DATA) )
        {
            GIntBig nRefVal = 0;
            unsigned int nSize = 0;
            GByte* pabyDataNewLimit = NULL;
            if( sWay.nRefs != 0 )
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( nSize > psCtxt->nNodeRefsAllocated )
            {
                psCtxt->nNodeRefsAllocated =
                    std::max(psCtxt->nNodeRefsAllocated * 2, nSize);
                GIntBig* panNodeRefsNew = (GIntBig*) VSI_REALLOC_VERBOSE(
                        psCtxt->panNodeRefs,
                        psCtxt->nNodeRefsAllocated * sizeof(GIntBig));
                if( panNodeRefsNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->panNodeRefs = panNodeRefsNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                GIntBig nDeltaRef = 0;
                READ_VARSINT64_NOCHECK(pabyData, pabyDataNewLimit, nDeltaRef);
                nRefVal = AddWithOverflowAccepted(nRefVal, nDeltaRef);

                psCtxt->panNodeRefs[sWay.nRefs ++] = nRefVal;
            }

            if( pabyData != pabyDataNewLimit )
                GOTO_END_ERROR;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    if( pabyData != pabyDataLimit )
        GOTO_END_ERROR;

    // printf("<ReadWay\n");

    if( sWay.nTags )
        sWay.pasTags = psCtxt->pasTags;
    else
        sWay.pasTags = NULL;
    sWay.panNodeRefs = psCtxt->panNodeRefs;

    psCtxt->pfnNotifyWay(&sWay, psCtxt, psCtxt->user_data);

    return true;

end_error:
    // printf("<ReadWay\n");

    return false;
}

/************************************************************************/
/*                            ReadRelation()                            */
/************************************************************************/

static const int RELATION_IDX_ID        = 1;
static const int RELATION_IDX_KEYS      = 2;
static const int RELATION_IDX_VALS      = 3;
static const int RELATION_IDX_INFO      = 4;
static const int RELATION_IDX_ROLES_SID = 8;
static const int RELATION_IDX_MEMIDS    = 9;
static const int RELATION_IDX_TYPES     = 10;

static
bool ReadRelation( GByte* pabyData, GByte* pabyDataLimit,
                   OSMContext* psCtxt )
{
    OSMRelation sRelation;
    sRelation.nID = 0;
    INIT_INFO(&(sRelation.sInfo));
    sRelation.nTags = 0;
    sRelation.nMembers = 0;

    /* printf(">ReadRelation\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(RELATION_IDX_ID, WT_VARINT) )
        {
            READ_VARINT64(pabyData, pabyDataLimit, sRelation.nID);
        }
        else if( nKey == MAKE_KEY(RELATION_IDX_KEYS, WT_DATA) )
        {
            unsigned int nSize = 0;
            GByte* pabyDataNewLimit = NULL;
            if( sRelation.nTags != 0 )
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( nSize > psCtxt->nTagsAllocated )
            {
                psCtxt->nTagsAllocated = std::max(
                    psCtxt->nTagsAllocated * 2, nSize);
                OSMTag* pasTagsNew = (OSMTag*) VSI_REALLOC_VERBOSE(
                    psCtxt->pasTags,
                    psCtxt->nTagsAllocated * sizeof(OSMTag));
                if( pasTagsNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasTags = pasTagsNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                unsigned int nKey2 = 0;
                READ_VARUINT32(pabyData, pabyDataNewLimit, nKey2);

                if( nKey2 >= psCtxt->nStrCount )
                    GOTO_END_ERROR;

                psCtxt->pasTags[sRelation.nTags].pszK =
                    psCtxt->pszStrBuf + psCtxt->panStrOff[nKey2];
                psCtxt->pasTags[sRelation.nTags].pszV = "";
                sRelation.nTags ++;
            }
            if( pabyData != pabyDataNewLimit )
                GOTO_END_ERROR;
        }
        else if( nKey == MAKE_KEY(RELATION_IDX_VALS, WT_DATA) )
        {
            unsigned int nIter = 0;
            if( sRelation.nTags == 0 )
                GOTO_END_ERROR;
            // unsigned int nSize = 0;
            // READ_VARUINT32(pabyData, pabyDataLimit, nSize);
            SKIP_VARINT(pabyData, pabyDataLimit);

            for(; nIter < sRelation.nTags; nIter ++)
            {
                unsigned int nVal = 0;
                READ_VARUINT32(pabyData, pabyDataLimit, nVal);

                if( nVal >= psCtxt->nStrCount )
                    GOTO_END_ERROR;

                psCtxt->pasTags[nIter].pszV = psCtxt->pszStrBuf +
                                              psCtxt->panStrOff[nVal];
            }
        }
        else if( nKey == MAKE_KEY(RELATION_IDX_INFO, WT_DATA) )
        {
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( !ReadOSMInfo(pabyData, pabyData + nSize,
                             &sRelation.sInfo, psCtxt) )
                GOTO_END_ERROR;

            pabyData += nSize;
        }
        else if( nKey == MAKE_KEY(RELATION_IDX_ROLES_SID, WT_DATA) )
        {
            unsigned int nSize = 0;
            GByte* pabyDataNewLimit = NULL;
            if( sRelation.nMembers != 0 )
                GOTO_END_ERROR;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( nSize > psCtxt->nMembersAllocated )
            {
                psCtxt->nMembersAllocated =
                    std::max(psCtxt->nMembersAllocated * 2, nSize);
                OSMMember* pasMembersNew = (OSMMember*) VSI_REALLOC_VERBOSE(
                        psCtxt->pasMembers,
                        psCtxt->nMembersAllocated * sizeof(OSMMember));
                if( pasMembersNew == NULL )
                    GOTO_END_ERROR;
                psCtxt->pasMembers = pasMembersNew;
            }

            pabyDataNewLimit = pabyData + nSize;
            while (pabyData < pabyDataNewLimit)
            {
                unsigned int nRoleSID = 0;
                READ_VARUINT32(pabyData, pabyDataNewLimit, nRoleSID);
                if( nRoleSID >= psCtxt->nStrCount )
                    GOTO_END_ERROR;

                psCtxt->pasMembers[sRelation.nMembers].pszRole =
                    psCtxt->pszStrBuf + psCtxt->panStrOff[nRoleSID];
                psCtxt->pasMembers[sRelation.nMembers].nID = 0;
                psCtxt->pasMembers[sRelation.nMembers].eType = MEMBER_NODE;
                sRelation.nMembers ++;
            }

            if( pabyData != pabyDataNewLimit )
                GOTO_END_ERROR;
        }
        else if( nKey == MAKE_KEY(RELATION_IDX_MEMIDS, WT_DATA) )
        {
            unsigned int nIter = 0;
            GIntBig nMemID = 0;
            if( sRelation.nMembers == 0 )
                GOTO_END_ERROR;
            // unsigned int nSize = 0;
            // READ_VARUINT32(pabyData, pabyDataLimit, nSize);
            SKIP_VARINT(pabyData, pabyDataLimit);

            for(; nIter < sRelation.nMembers; nIter++)
            {
                GIntBig nDeltaMemID = 0;
                READ_VARSINT64(pabyData, pabyDataLimit, nDeltaMemID);
                nMemID = AddWithOverflowAccepted(nMemID, nDeltaMemID);

                psCtxt->pasMembers[nIter].nID = nMemID;
            }
        }
        else if( nKey == MAKE_KEY(RELATION_IDX_TYPES, WT_DATA) )
        {
            unsigned int nIter = 0;
            if( sRelation.nMembers == 0 )
                GOTO_END_ERROR;
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);
            if( nSize != sRelation.nMembers )
                GOTO_END_ERROR;

            for(; nIter < sRelation.nMembers; nIter++)
            {
                unsigned int nType = pabyData[nIter];
                if( nType > MEMBER_RELATION )
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

    if( sRelation.nTags )
        sRelation.pasTags = psCtxt->pasTags;
    else
        sRelation.pasTags = NULL;

    sRelation.pasMembers = psCtxt->pasMembers;

    psCtxt->pfnNotifyRelation(&sRelation, psCtxt, psCtxt->user_data);

    return true;

end_error:
    /* printf("<ReadRelation\n"); */

    return false;
}

/************************************************************************/
/*                          ReadPrimitiveGroup()                        */
/************************************************************************/

static const int PRIMITIVEGROUP_IDX_NODES = 1;
// static const int PRIMITIVEGROUP_IDX_DENSE = 2;
// static const int PRIMITIVEGROUP_IDX_WAYS = 3;
static const int PRIMITIVEGROUP_IDX_RELATIONS = 4;
// static const int PRIMITIVEGROUP_IDX_CHANGESETS = 5;

typedef bool (*PrimitiveFuncType)( GByte* pabyData, GByte* pabyDataLimit,
                                   OSMContext* psCtxt );

static const PrimitiveFuncType apfnPrimitives[] =
{
    ReadNode,
    ReadDenseNodes,
    ReadWay,
    ReadRelation
};

static
bool ReadPrimitiveGroup( GByte* pabyData, GByte* pabyDataLimit,
                         OSMContext* psCtxt )
{
    /* printf(">ReadPrimitiveGroup\n"); */
    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        const int nFieldNumber = GET_FIELDNUMBER(nKey) - 1;
        if( GET_WIRETYPE(nKey) == WT_DATA &&
            nFieldNumber >= PRIMITIVEGROUP_IDX_NODES - 1 &&
            nFieldNumber <= PRIMITIVEGROUP_IDX_RELATIONS - 1 )
        {
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( !apfnPrimitives[nFieldNumber](pabyData, pabyData + nSize,
                                              psCtxt) )
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

static const int PRIMITIVEBLOCK_IDX_STRINGTABLE      = 1;
static const int PRIMITIVEBLOCK_IDX_PRIMITIVEGROUP   = 2;
static const int PRIMITIVEBLOCK_IDX_GRANULARITY      = 17;
static const int PRIMITIVEBLOCK_IDX_DATE_GRANULARITY = 18;
static const int PRIMITIVEBLOCK_IDX_LAT_OFFSET       = 19;
static const int PRIMITIVEBLOCK_IDX_LON_OFFSET       = 20;

static
bool ReadPrimitiveBlock( GByte* pabyData, GByte* pabyDataLimit,
                         OSMContext* psCtxt )
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
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_GRANULARITY, WT_VARINT) )
        {
            READ_VARINT32(pabyData, pabyDataLimit, psCtxt->nGranularity);
            if( psCtxt->nGranularity <= 0 )
                GOTO_END_ERROR;
        }
        else if( nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_DATE_GRANULARITY,
                                  WT_VARINT) )
        {
            READ_VARINT32(pabyData, pabyDataLimit, psCtxt->nDateGranularity);
        }
        else if( nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_LAT_OFFSET, WT_VARINT) )
        {
            READ_VARINT64(pabyData, pabyDataLimit, psCtxt->nLatOffset);
        }
        else if( nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_LON_OFFSET, WT_VARINT) )
        {
            READ_VARINT64(pabyData, pabyDataLimit, psCtxt->nLonOffset);
        }
        else
        {
            SKIP_UNKNOWN_FIELD_INLINE(pabyData, pabyDataLimit, FALSE);
        }
    }

    if( pabyData != pabyDataLimit )
        GOTO_END_ERROR;

    pabyData = pabyDataSave;
    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_STRINGTABLE, WT_DATA) )
        {
            GByte bSaveAfterByte = 0;
            GByte* pbSaveAfterByte = NULL;
            if( psCtxt->nStrCount != 0 )
                GOTO_END_ERROR;
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            // Dirty little trick:
            // ReadStringTable() will over-write the byte after the
            // StringTable message with a NUL character, so we backup
            // it to be able to restore it just before issuing the next
            // READ_FIELD_KEY. Then we will re-NUL it to have valid
            // NUL terminated strings.
            // This trick enable us to keep the strings where there are
            // in RAM.
            pbSaveAfterByte = pabyData + nSize;
            bSaveAfterByte = *pbSaveAfterByte;

            if( !ReadStringTable(pabyData, pabyData + nSize, psCtxt) )
                GOTO_END_ERROR;

            pabyData += nSize;

            *pbSaveAfterByte = bSaveAfterByte;
            if( pabyData == pabyDataLimit )
                break;

            READ_FIELD_KEY(nKey);
            *pbSaveAfterByte = 0;

            if( nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_STRINGTABLE, WT_DATA) )
                GOTO_END_ERROR;

            /* Yes we go on ! */
        }

        if( nKey == MAKE_KEY(PRIMITIVEBLOCK_IDX_PRIMITIVEGROUP, WT_DATA) )
        {
            unsigned int nSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nSize);

            if( !ReadPrimitiveGroup(pabyData, pabyData + nSize, psCtxt))
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

    return false;
}

/************************************************************************/
/*                          DecompressFunction()                        */
/************************************************************************/

static void DecompressFunction(void* pDataIn)
{
    DecompressionJob* psJob = static_cast<DecompressionJob*>(pDataIn);
    psJob->bStatus =
        CPLZLibInflate( psJob->pabySrc, psJob->nSrcSize,
                        psJob->pabyDstBase + psJob->nDstOffset,
                        psJob->nDstSize, NULL) != NULL;
}

/************************************************************************/
/*                      RunDecompressionJobs()                          */
/************************************************************************/

static bool RunDecompressionJobs(OSMContext* psCtxt)
{
    psCtxt->nTotalUncompressedSize = 0;

    GByte* pabyDstBase = psCtxt->pabyUncompressed;
    std::vector<void*> ahJobs;
    for( int i = 0; i < psCtxt->nJobs; i++ )
    {
        psCtxt->asJobs[i].pabyDstBase = pabyDstBase;
        if( psCtxt->poWTP )
            ahJobs.push_back(&psCtxt->asJobs[i]);
        else
            DecompressFunction(&psCtxt->asJobs[i]);
    }
    if( psCtxt->poWTP )
    {
        psCtxt->poWTP->SubmitJobs(DecompressFunction, ahJobs);
        psCtxt->poWTP->WaitCompletion();
    }

    bool bRet = true;
    for( int i = 0; bRet && i < psCtxt->nJobs; i++ )
    {
        bRet &= psCtxt->asJobs[i].bStatus;
    }
    return bRet;
}

/************************************************************************/
/*                          ProcessSingleBlob()                         */
/************************************************************************/

static bool ProcessSingleBlob(OSMContext* psCtxt,
                           DecompressionJob& sJob, BlobType eType)
{
    if( eType == BLOB_OSMHEADER )
    {
        return ReadOSMHeader(
            sJob.pabyDstBase + sJob.nDstOffset,
            sJob.pabyDstBase + sJob.nDstOffset + sJob.nDstSize,
            psCtxt);
    }
    else
    {
        CPLAssert( eType == BLOB_OSMDATA );
        return ReadPrimitiveBlock(
            sJob.pabyDstBase + sJob.nDstOffset,
            sJob.pabyDstBase + sJob.nDstOffset + sJob.nDstSize,
            psCtxt);
    }
}

/************************************************************************/
/*                   RunDecompressionJobsAndProcessAll()                */
/************************************************************************/

static bool RunDecompressionJobsAndProcessAll(OSMContext* psCtxt,
                                              BlobType eType)
{
    if( !RunDecompressionJobs(psCtxt) )
    {
        return false;
    }
    for( int i = 0; i < psCtxt->nJobs; i++ )
    {
        if( !ProcessSingleBlob(psCtxt, psCtxt->asJobs[i], eType) )
        {
            return false;
        }
    }
    psCtxt->iNextJob = 0;
    psCtxt->nJobs = 0;
    return true;
}

/************************************************************************/
/*                              ReadBlob()                              */
/************************************************************************/

static const int BLOB_IDX_RAW       = 1;
static const int BLOB_IDX_RAW_SIZE  = 2;
static const int BLOB_IDX_ZLIB_DATA = 3;

static
bool ReadBlob( OSMContext* psCtxt, BlobType eType )
{
    unsigned int nUncompressedSize = 0;
    bool bRet = true;
    GByte* pabyData = psCtxt->pabyBlob + psCtxt->nBlobOffset;
    GByte* pabyLastCheckpointData = pabyData;
    GByte* pabyDataLimit = psCtxt->pabyBlob + psCtxt->nBlobSize;

    while(pabyData < pabyDataLimit)
    {
        int nKey = 0;
        READ_FIELD_KEY(nKey);

        if( nKey == MAKE_KEY(BLOB_IDX_RAW, WT_DATA) )
        {
            if( psCtxt->nJobs > 0 &&
                !RunDecompressionJobsAndProcessAll(psCtxt, eType) )
            {
                GOTO_END_ERROR;
            }

            unsigned int nDataLength = 0;
            READ_SIZE(pabyData, pabyDataLimit, nDataLength);
            if( nDataLength > MAX_BLOB_SIZE ) GOTO_END_ERROR;

            // printf("raw data size = %d\n", nDataLength);

            if( eType == BLOB_OSMHEADER )
            {
                bRet = ReadOSMHeader(pabyData, pabyData + nDataLength, psCtxt);
            }
            else if( eType == BLOB_OSMDATA )
            {
                bRet = ReadPrimitiveBlock(pabyData, pabyData + nDataLength,
                                          psCtxt);
            }

            pabyData += nDataLength;
        }
        else if( nKey == MAKE_KEY(BLOB_IDX_RAW_SIZE, WT_VARINT) )
        {
            READ_VARUINT32(pabyData, pabyDataLimit, nUncompressedSize);
            // printf("nUncompressedSize = %d\n", nUncompressedSize);
        }
        else if( nKey == MAKE_KEY(BLOB_IDX_ZLIB_DATA, WT_DATA) )
        {
            unsigned int nZlibCompressedSize = 0;
            READ_VARUINT32(pabyData, pabyDataLimit, nZlibCompressedSize);
            if( CHECK_OOB && nZlibCompressedSize >
                    psCtxt->nBlobSize - psCtxt->nBlobOffset)
            {
                GOTO_END_ERROR;
            }

            // printf("nZlibCompressedSize = %d\n", nZlibCompressedSize);

            if( nUncompressedSize != 0 )
            {
                if( nUncompressedSize / 100 > nZlibCompressedSize )
                {
                    // Too prevent excessive memory allocations
                    CPLError(CE_Failure, CPLE_AppDefined,
                                "Excessive uncompressed vs compressed ratio");
                    GOTO_END_ERROR;
                }
                if( psCtxt->nJobs > 0 &&
                    (psCtxt->nTotalUncompressedSize >
                                UINT_MAX - nUncompressedSize ||
                     psCtxt->nTotalUncompressedSize +
                            nUncompressedSize > MAX_ACC_UNCOMPRESSED_SIZE) )
                {
                    pabyData = pabyLastCheckpointData;
                    break;
                }
                unsigned nSizeNeeded = psCtxt->nTotalUncompressedSize +
                                       nUncompressedSize;
                if( nSizeNeeded > psCtxt->nUncompressedAllocated )
                {

                    GByte* pabyUncompressedNew = NULL;
                    if( psCtxt->nUncompressedAllocated <=
                            UINT_MAX - psCtxt->nUncompressedAllocated / 3 &&
                        psCtxt->nUncompressedAllocated +
                            psCtxt->nUncompressedAllocated / 3 <
                                MAX_ACC_UNCOMPRESSED_SIZE )
                    {
                        psCtxt->nUncompressedAllocated =
                            std::max(psCtxt->nUncompressedAllocated +
                                            psCtxt->nUncompressedAllocated / 3,
                                     nSizeNeeded);
                    }
                    else
                    {
                        psCtxt->nUncompressedAllocated = nSizeNeeded;
                    }
                    if( psCtxt->nUncompressedAllocated > UINT_MAX - EXTRA_BYTES )
                        GOTO_END_ERROR;
                    pabyUncompressedNew =
                        (GByte*)VSI_REALLOC_VERBOSE(psCtxt->pabyUncompressed,
                                psCtxt->nUncompressedAllocated + EXTRA_BYTES);
                    if( pabyUncompressedNew == NULL )
                        GOTO_END_ERROR;
                    psCtxt->pabyUncompressed = pabyUncompressedNew;
                }
                memset(psCtxt->pabyUncompressed + nSizeNeeded, 0, EXTRA_BYTES);

                psCtxt->asJobs[psCtxt->nJobs].pabySrc = pabyData;
                psCtxt->asJobs[psCtxt->nJobs].nSrcSize = nZlibCompressedSize;
                psCtxt->asJobs[psCtxt->nJobs].nDstOffset =
                                                psCtxt->nTotalUncompressedSize;
                psCtxt->asJobs[psCtxt->nJobs].nDstSize = nUncompressedSize;
                psCtxt->nJobs ++;
                if( psCtxt->poWTP == NULL || eType != BLOB_OSMDATA )
                {
                    if( !RunDecompressionJobsAndProcessAll(psCtxt, eType) )
                    {
                        GOTO_END_ERROR;
                    }
                }
                else
                {
                    // Make sure that uncompressed blobs are separated by
                    // EXTRA_BYTES in the case where in the future we would
                    // implement parallel decoding of them (not sure if that's
                    // doable)
                    psCtxt->nTotalUncompressedSize +=
                                            nUncompressedSize + EXTRA_BYTES;
                }
            }

            nUncompressedSize = 0;
            pabyData += nZlibCompressedSize;
            pabyLastCheckpointData = pabyData;
            if( psCtxt->nJobs == N_MAX_JOBS )
                break;
        }
        else
        {
            SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, TRUE);
        }
    }

    if( psCtxt->nJobs > 0 )
    {
        if( !RunDecompressionJobs(psCtxt) )
        {
            GOTO_END_ERROR;
        }
        // Just process one blob at a time
        if( !ProcessSingleBlob(psCtxt, psCtxt->asJobs[0], eType) )
        {
            GOTO_END_ERROR;
        }
        psCtxt->iNextJob = 1;
    }

    psCtxt->nBlobOffset = static_cast<unsigned>(pabyData - psCtxt->pabyBlob);
    return bRet;

end_error:
    return false;
}

/************************************************************************/
/*                          OSM_ProcessBlock()                          */
/************************************************************************/

static OSMRetCode PBF_ProcessBlock(OSMContext* psCtxt)
{
    // Process any remaining queued jobs one by one
    if (psCtxt->iNextJob < psCtxt->nJobs)
    {
        if( !(ProcessSingleBlob(psCtxt,
                                psCtxt->asJobs[psCtxt->iNextJob],
                                BLOB_OSMDATA)) )
        {
            return OSM_ERROR;
        }
        psCtxt->iNextJob ++;
        return OSM_OK;
    }
    psCtxt->iNextJob = 0;
    psCtxt->nJobs = 0;

    // Make sure to finish parsing the last concatenated blocks
    if( psCtxt->nBlobOffset < psCtxt->nBlobSize )
    {
        return ReadBlob(psCtxt, BLOB_OSMDATA) ? OSM_OK : OSM_ERROR;
    }
    psCtxt->nBlobOffset = 0;
    psCtxt->nBlobSize = 0;

    bool nRet = false;
    int nBlobCount = 0;
    OSMRetCode eRetCode = OSM_OK;
    unsigned int nBlobSizeAcc = 0;
    BlobType eType = BLOB_UNKNOWN;
    while( true )
    {
        GByte abyHeaderSize[4];
        unsigned int nBlobSize = 0;

        if( VSIFReadL(abyHeaderSize, 4, 1, psCtxt->fp) != 1 )
        {
            eRetCode = OSM_EOF;
            break;
        }
        const unsigned int nHeaderSize =
            (static_cast<unsigned int>(abyHeaderSize[0]) << 24) |
            (abyHeaderSize[1] << 16) |
            (abyHeaderSize[2] << 8) | abyHeaderSize[3];

        psCtxt->nBytesRead += 4;

        /* printf("nHeaderSize = %d\n", nHeaderSize); */
        if( nHeaderSize > MAX_BLOB_HEADER_SIZE )
        {
            eRetCode = OSM_ERROR;
            break;
        }
        if( VSIFReadL(psCtxt->pabyBlobHeader, 1, nHeaderSize, psCtxt->fp) !=
                                                                nHeaderSize )
        {
            eRetCode = OSM_ERROR;
            break;
        }

        psCtxt->nBytesRead += nHeaderSize;

        memset(psCtxt->pabyBlobHeader + nHeaderSize, 0, EXTRA_BYTES);
        nRet = ReadBlobHeader(psCtxt->pabyBlobHeader, 
                              psCtxt->pabyBlobHeader + nHeaderSize,
                              &nBlobSize, &eType);
        if( !nRet || eType == BLOB_UNKNOWN )
        {
            eRetCode = OSM_ERROR;
            break;
        }

        // Limit in OSM PBF spec
        if( nBlobSize > MAX_BLOB_SIZE )
        {
            eRetCode = OSM_ERROR;
            break;
        }
        if( nBlobSize + nBlobSizeAcc > psCtxt->nBlobSizeAllocated )
        {
            psCtxt->nBlobSizeAllocated =
                std::max(std::min(MAX_ACC_BLOB_SIZE,
                                  psCtxt->nBlobSizeAllocated * 2),
                         nBlobSize + nBlobSizeAcc);
            GByte* pabyBlobNew = static_cast<GByte *>(
                VSI_REALLOC_VERBOSE(psCtxt->pabyBlob,
                                    psCtxt->nBlobSizeAllocated + EXTRA_BYTES));
            if( pabyBlobNew == NULL )
            {
                eRetCode = OSM_ERROR;
                break;
            }
            psCtxt->pabyBlob = pabyBlobNew;
        }
        // Given how Protocol buffer work, we can merge sevaral buffers
        // by just appending them to the previous ones.
        if( VSIFReadL(psCtxt->pabyBlob + nBlobSizeAcc, 1,
                      nBlobSize, psCtxt->fp) != nBlobSize )
        {
            eRetCode = OSM_ERROR;
            break;
        }
        psCtxt->nBytesRead += nBlobSize;
        nBlobSizeAcc += nBlobSize;
        memset(psCtxt->pabyBlob + nBlobSizeAcc, 0, EXTRA_BYTES);

        nBlobCount ++;

        if( eType == BLOB_OSMDATA && psCtxt->poWTP != NULL )
        {
            // Accumulate BLOB_OSMDATA until we reach either the maximum
            // number of jobs or a theshold in bytes
            if( nBlobCount == N_MAX_JOBS || nBlobSizeAcc > MAX_ACC_BLOB_SIZE )
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    if( nBlobCount > 0 )
    {
        psCtxt->nBlobOffset = 0;
        psCtxt->nBlobSize = nBlobSizeAcc;
        nRet = ReadBlob(psCtxt, eType);
        if( nRet )
        {
            if( eRetCode == OSM_EOF &&
                (psCtxt->iNextJob < psCtxt->nJobs ||
                 psCtxt->nBlobOffset < psCtxt->nBlobSize) )
            {
                eRetCode = OSM_OK;
            }
            CPLAssert( psCtxt->iNextJob == psCtxt->nJobs ||
                       eType == BLOB_OSMDATA );
        }
        else
        {
            eRetCode = OSM_ERROR;
        }
    }

    return eRetCode;
}

/************************************************************************/
/*                        EmptyNotifyNodesFunc()                        */
/************************************************************************/

static void EmptyNotifyNodesFunc( unsigned int /* nNodes */,
                                  OSMNode* /* pasNodes */,
                                  OSMContext* /* psCtxt */,
                                  void* /* user_data */ )
{}

/************************************************************************/
/*                         EmptyNotifyWayFunc()                         */
/************************************************************************/

static void EmptyNotifyWayFunc( OSMWay* /* psWay */,
                                OSMContext* /* psCtxt */,
                                void* /* user_data */ )
{}

/************************************************************************/
/*                       EmptyNotifyRelationFunc()                      */
/************************************************************************/

static void EmptyNotifyRelationFunc( OSMRelation* /* psRelation */,
                                     OSMContext* /* psCtxt */,
                                     void* /* user_data */ )
{}

/************************************************************************/
/*                         EmptyNotifyBoundsFunc()                      */
/************************************************************************/

static void EmptyNotifyBoundsFunc( double /* dfXMin */,
                                   double /* dfYMin */,
                                   double /* dfXMax */,
                                   double /* dfYMax */,
                                   OSMContext* /*psCtxt */,
                                   void * /* user_data */)
{}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                          OSM_AddString()                             */
/************************************************************************/

static const char* OSM_AddString(OSMContext* psCtxt, const char* pszStr)
{
    int nLen = (int)strlen(pszStr);
    if( psCtxt->nStrLength + nLen + 1 > psCtxt->nStrAllocated )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "String buffer too small");
        return "";
    }
    char* pszRet = psCtxt->pszStrBuf + psCtxt->nStrLength;
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

#if defined(__MSVCRT__) || (defined(WIN32) && defined(_MSC_VER))
    const GIntBig iValue = (GIntBig)_atoi64( pszString );
# elif HAVE_ATOLL
    const GIntBig iValue = atoll( pszString );
#else
    const GIntBig iValue = atol( pszString );
#endif

    return iValue;
}

/************************************************************************/
/*                      OSM_XML_startElementCbk()                       */
/************************************************************************/

static void XMLCALL OSM_XML_startElementCbk( void *pUserData,
                                             const char *pszName,
                                             const char **ppszAttr)
{
    OSMContext* psCtxt = (OSMContext*) pUserData;
    const char** ppszIter = ppszAttr;

    if( psCtxt->bStopParsing ) return;

    psCtxt->nWithoutEventCounter = 0;

    if( psCtxt->bTryToFetchBounds )
    {
        if( strcmp(pszName, "bounds") == 0 ||
            strcmp(pszName, "bound") == 0 /* osmosis uses bound */ )
        {
            int nCountCoords = 0;

            psCtxt->bTryToFetchBounds = false;

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
        psCtxt->bInNode = true;
        psCtxt->bTryToFetchBounds = false;

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
                    psCtxt->pasNodes[0].sInfo.bTimeStampIsStr = true;
                }
                ppszIter += 2;
            }
        }
    }

    else if( !psCtxt->bInNode && !psCtxt->bInWay && !psCtxt->bInRelation &&
             strcmp(pszName, "way") == 0 )
    {
        psCtxt->bInWay = true;

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
                    psCtxt->sWay.sInfo.bTimeStampIsStr = true;
                }
                ppszIter += 2;
            }
        }
    }

    else if( !psCtxt->bInNode && !psCtxt->bInWay && !psCtxt->bInRelation &&
             strcmp(pszName, "relation") == 0 )
    {
        psCtxt->bInRelation = true;

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
                    psCtxt->sRelation.sInfo.bTimeStampIsStr = true;
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
        /* 300 is the recommended value, but there are files with more than 2000 so we should be able */
        /* to realloc over that value */
        if( psCtxt->sRelation.nMembers >= psCtxt->nMembersAllocated )
        {
            int nMembersAllocated =
                std::max(psCtxt->nMembersAllocated * 2, psCtxt->sRelation.nMembers + 1);
            OSMMember* pasMembersNew = (OSMMember*) VSI_REALLOC_VERBOSE(
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
        if( psCtxt->nTags == psCtxt->nTagsAllocated )
        {
            psCtxt->nTagsAllocated =
                psCtxt->nTagsAllocated * 2;
            OSMTag* pasTagsNew = (OSMTag*) VSI_REALLOC_VERBOSE(
                psCtxt->pasTags,
                psCtxt->nTagsAllocated * sizeof(OSMTag));
            if( pasTagsNew == NULL )
            {
                if( psCtxt->bInNode )
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Too many tags in node " CPL_FRMT_GIB,
                            psCtxt->pasNodes[0].nID);
                else if( psCtxt->bInWay )
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Too many tags in way " CPL_FRMT_GIB,
                            psCtxt->sWay.nID);
                else if( psCtxt->bInRelation )
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Too many tags in relation " CPL_FRMT_GIB,
                            psCtxt->sRelation.nID);
                return;
            }
            psCtxt->pasTags = pasTagsNew;
        }

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
}

/************************************************************************/
/*                       OSM_XML_endElementCbk()                        */
/************************************************************************/

static void XMLCALL OSM_XML_endElementCbk( void *pUserData,
                                           const char *pszName )
{
    OSMContext* psCtxt = (OSMContext*) pUserData;

    if( psCtxt->bStopParsing ) return;

    psCtxt->nWithoutEventCounter = 0;

    if( psCtxt->bInNode && strcmp(pszName, "node") == 0 )
    {
        if( psCtxt->pasNodes[0].dfLon < -180 || psCtxt->pasNodes[0].dfLon > 180 ||
            psCtxt->pasNodes[0].dfLat < -90 || psCtxt->pasNodes[0].dfLat > 90 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid lon=%f lat=%f",
                     psCtxt->pasNodes[0].dfLon,
                     psCtxt->pasNodes[0].dfLat);
        }
        else
        {
            psCtxt->pasNodes[0].nTags = psCtxt->nTags;
            psCtxt->pasNodes[0].pasTags = psCtxt->pasTags;

            psCtxt->pfnNotifyNodes(1, psCtxt->pasNodes, psCtxt, psCtxt->user_data);

            psCtxt->bHasFoundFeature = true;
        }
        psCtxt->bInNode = false;
    }

    else
    if( psCtxt->bInWay && strcmp(pszName, "way") == 0 )
    {
        psCtxt->sWay.nTags = psCtxt->nTags;
        psCtxt->sWay.pasTags = psCtxt->pasTags;

        psCtxt->sWay.panNodeRefs = psCtxt->panNodeRefs;

        psCtxt->pfnNotifyWay(&(psCtxt->sWay), psCtxt, psCtxt->user_data);

        psCtxt->bHasFoundFeature = true;

        psCtxt->bInWay = false;
    }

    else
    if( psCtxt->bInRelation && strcmp(pszName, "relation") == 0 )
    {
        psCtxt->sRelation.nTags = psCtxt->nTags;
        psCtxt->sRelation.pasTags = psCtxt->pasTags;

        psCtxt->sRelation.pasMembers = psCtxt->pasMembers;

        psCtxt->pfnNotifyRelation(&(psCtxt->sRelation), psCtxt, psCtxt->user_data);

        psCtxt->bHasFoundFeature = true;

        psCtxt->bInRelation = false;
    }
}
/************************************************************************/
/*                           dataHandlerCbk()                           */
/************************************************************************/

static void XMLCALL OSM_XML_dataHandlerCbk( void *pUserData,
                                            const char * /* data */,
                                            int /* nLen */)
{
    OSMContext* psCtxt = static_cast<OSMContext *>(pUserData);

    if( psCtxt->bStopParsing ) return;

    psCtxt->nWithoutEventCounter = 0;

    psCtxt->nDataHandlerCounter ++;
    if( psCtxt->nDataHandlerCounter >= XML_BUFSIZE )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(psCtxt->hXMLParser, XML_FALSE);
        psCtxt->bStopParsing = true;
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

    psCtxt->bHasFoundFeature = false;
    psCtxt->nWithoutEventCounter = 0;

    do
    {
        psCtxt->nDataHandlerCounter = 0;

        const unsigned int nLen =
            (unsigned int)VSIFReadL( psCtxt->pabyBlob, 1,
                                     XML_BUFSIZE, psCtxt->fp );

        psCtxt->nBytesRead += nLen;

        psCtxt->bEOF = CPL_TO_BOOL(VSIFEofL(psCtxt->fp));
        const int eErr =
            XML_Parse(psCtxt->hXMLParser, (const char*) psCtxt->pabyBlob,
                      nLen, psCtxt->bEOF );

        if( eErr == XML_STATUS_ERROR )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of OSM file failed : %s "
                     "at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(psCtxt->hXMLParser)),
                     (int)XML_GetCurrentLineNumber(psCtxt->hXMLParser),
                     (int)XML_GetCurrentColumnNumber(psCtxt->hXMLParser));
            psCtxt->bStopParsing = true;
        }
        psCtxt->nWithoutEventCounter ++;
    } while (!psCtxt->bEOF && !psCtxt->bStopParsing &&
             !psCtxt->bHasFoundFeature &&
             psCtxt->nWithoutEventCounter < 10);

    if( psCtxt->nWithoutEventCounter == 10 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        psCtxt->bStopParsing = true;
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

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if( fp == NULL )
        return NULL;

    GByte abyHeader[1024];
    int nRead = static_cast<int>(
        VSIFReadL(abyHeader, 1, sizeof(abyHeader)-1, fp));
    abyHeader[nRead] = '\0';

    bool bPBF = false;

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
        const int nLimitI = nRead - static_cast<int>(strlen("OSMHeader"));
        for( int i = 0; i < nLimitI; i++)
        {
            if( memcmp(abyHeader + i, "OSMHeader", strlen("OSMHeader") ) == 0 )
            {
                bPBF = true;
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

    OSMContext* psCtxt = static_cast<OSMContext *>(
        VSI_MALLOC_VERBOSE(sizeof(OSMContext)) );
    if( psCtxt == NULL )
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

        psCtxt->nStrAllocated = 1024*1024;
        psCtxt->pszStrBuf = (char*) VSI_MALLOC_VERBOSE(psCtxt->nStrAllocated);
        if( psCtxt->pszStrBuf )
            psCtxt->pszStrBuf[0] = '\0';

        psCtxt->hXMLParser = OGRCreateExpatXMLParser();
        XML_SetUserData(psCtxt->hXMLParser, psCtxt);
        XML_SetElementHandler(psCtxt->hXMLParser,
                              OSM_XML_startElementCbk,
                              OSM_XML_endElementCbk);
        XML_SetCharacterDataHandler(psCtxt->hXMLParser, OSM_XML_dataHandlerCbk);

        psCtxt->bTryToFetchBounds = true;

        psCtxt->nNodesAllocated = 1;
        psCtxt->pasNodes = (OSMNode*) VSI_MALLOC_VERBOSE(sizeof(OSMNode) * psCtxt->nNodesAllocated);

        psCtxt->nTagsAllocated = 256;
        psCtxt->pasTags = (OSMTag*) VSI_MALLOC_VERBOSE(sizeof(OSMTag) * psCtxt->nTagsAllocated);

        /* 300 is the recommended value, but there are files with more than 2000 so we should be able */
        /* to realloc over that value */
        psCtxt->nMembersAllocated = 2000;
        psCtxt->pasMembers = (OSMMember*) VSI_MALLOC_VERBOSE(sizeof(OSMMember) * psCtxt->nMembersAllocated);

        psCtxt->nNodeRefsAllocated = 2000;
        psCtxt->panNodeRefs = (GIntBig*) VSI_MALLOC_VERBOSE(sizeof(GIntBig) * psCtxt->nNodeRefsAllocated);

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

    psCtxt->pabyBlob = (GByte*)VSI_MALLOC_VERBOSE(psCtxt->nBlobSizeAllocated);
    if( psCtxt->pabyBlob == NULL )
    {
        OSM_Close(psCtxt);
        return NULL;
    }
    psCtxt->pabyBlobHeader =
            (GByte*)VSI_MALLOC_VERBOSE(MAX_BLOB_HEADER_SIZE+EXTRA_BYTES);
    if( psCtxt->pabyBlobHeader == NULL )
    {
        OSM_Close(psCtxt);
        return NULL;
    }
    const char* pszNumThreads =
                CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    int nNumCPUs = CPLGetNumCPUs();
    if( pszNumThreads && !EQUAL(pszNumThreads, "ALL_CPUS") )
        nNumCPUs = std::min(2 * nNumCPUs, atoi(pszNumThreads));
    if( nNumCPUs > 1 )
    {
        psCtxt->poWTP = new CPLWorkerThreadPool();
        if( !psCtxt->poWTP->Setup(nNumCPUs , NULL, NULL) )
        {
            delete psCtxt->poWTP;
            psCtxt->poWTP = NULL;
        }
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
        if( psCtxt->hXMLParser )
            XML_ParserFree(psCtxt->hXMLParser);

        CPLFree(psCtxt->pszStrBuf); /* only for XML case ! */
    }
#endif

    VSIFree(psCtxt->pabyBlob);
    VSIFree(psCtxt->pabyBlobHeader);
    VSIFree(psCtxt->pabyUncompressed);
    VSIFree(psCtxt->panStrOff);
    VSIFree(psCtxt->pasNodes);
    VSIFree(psCtxt->pasTags);
    VSIFree(psCtxt->pasMembers);
    VSIFree(psCtxt->panNodeRefs);
    delete psCtxt->poWTP;

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
    psCtxt->nJobs = 0;
    psCtxt->iNextJob = 0;
    psCtxt->nBlobOffset = 0;
    psCtxt->nBlobSize = 0;
    psCtxt->nTotalUncompressedSize = 0;

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
        psCtxt->bEOF = false;
        psCtxt->bStopParsing = false;
        psCtxt->nStrLength = 0;
        psCtxt->pszStrBuf[0] = '\0';
        psCtxt->nTags = 0;

        psCtxt->bTryToFetchBounds = true;
        psCtxt->bInNode = false;
        psCtxt->bInWay = false;
        psCtxt->bInRelation = false;
    }
#endif
}

/************************************************************************/
/*                          OSM_ProcessBlock()                          */
/************************************************************************/

OSMRetCode OSM_ProcessBlock( OSMContext* psCtxt )
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
