/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 * Purpose:  osm2osm
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_vsi.h"

#include "osm_parser.h"

#include <cstdio>
#include <ctime>

constexpr GIntBig SECSPERMIN = 60L;
constexpr GIntBig MINSPERHOUR = 60L;
constexpr GIntBig HOURSPERDAY = 24L;
constexpr GIntBig SECSPERHOUR = SECSPERMIN * MINSPERHOUR;
constexpr GIntBig SECSPERDAY = SECSPERHOUR * HOURSPERDAY;
constexpr GIntBig DAYSPERWEEK = 7;
constexpr GIntBig MONSPERYEAR = 12;

constexpr GIntBig EPOCH_YEAR = 1970;
constexpr GIntBig EPOCH_WDAY = 4;
constexpr GIntBig TM_YEAR_BASE = 1900;
constexpr GIntBig DAYSPERNYEAR = 365;
constexpr GIntBig DAYSPERLYEAR = 366;

static bool isleap( GIntBig y ) {
    return
        ((y % 4) == 0 &&
         (y % 100) != 0) ||
        (y % 400) == 0;
}

static GIntBig LEAPS_THROUGH_END_OF( GIntBig y )
{
    return y / 4 - y / 100 + y / 400;
}

static const int mon_lengths[2][MONSPERYEAR] = {
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
} ;

static const int year_lengths[2] = { DAYSPERNYEAR, DAYSPERLYEAR };

/************************************************************************/
/*                   CPLUnixTimeToYMDHMS()                              */
/************************************************************************/

/** Converts a time value since the Epoch (aka "unix" time) to a broken-down
 *  UTC time.
 *
 * This function is similar to gmtime_r().
 * This function will always set tm_isdst to 0.
 *
 * @param unixTime number of seconds since the Epoch.
 * @param pRet address of the return structure.
 *
 * @return the structure pointed by pRet filled with a broken-down UTC time.
 */

static
struct tm * myCPLUnixTimeToYMDHMS(GIntBig unixTime, struct tm* pRet)
{
    GIntBig days = unixTime / SECSPERDAY;
    GIntBig rem = unixTime % SECSPERDAY;

    while (rem < 0) {
        rem += SECSPERDAY;
        --days;
    }

    pRet->tm_hour = static_cast<int>(rem / SECSPERHOUR);
    rem = rem % SECSPERHOUR;
    pRet->tm_min = static_cast<int>(rem / SECSPERMIN);
    /*
    ** A positive leap second requires a special
    ** representation.  This uses "... ??:59:60" et seq.
    */
    pRet->tm_sec = static_cast<int>(rem % SECSPERMIN);
    pRet->tm_wday = static_cast<int>((EPOCH_WDAY + days) % DAYSPERWEEK);
    if( pRet->tm_wday < 0 )
        pRet->tm_wday += DAYSPERWEEK;
    int yleap = 0;
    GIntBig y = EPOCH_YEAR;
    while( days < 0 ||
           days >= static_cast<GIntBig>(year_lengths[yleap = isleap(y)]) )
    {
        GIntBig newy = y + days / DAYSPERNYEAR;
        if( days < 0 )
            --newy;
        days -= (newy - y) * DAYSPERNYEAR +
            LEAPS_THROUGH_END_OF(newy - 1) -
            LEAPS_THROUGH_END_OF(y - 1);
        y = newy;
    }
    pRet->tm_year = static_cast<int>(y - TM_YEAR_BASE);
    pRet->tm_yday = static_cast<int>(days);
    const int* ip = mon_lengths[yleap];
    for( pRet->tm_mon = 0; days >= static_cast<GIntBig>(ip[pRet->tm_mon]);
         ++(pRet->tm_mon) )
        days = days - static_cast<GIntBig>(ip[pRet->tm_mon]);

    pRet->tm_mday = static_cast<int>(days + 1);
    pRet->tm_isdst = 0;

    return pRet;
}

static void WriteEscaped(const char* pszStr, VSILFILE* fp)
{
    GByte ch;
    while( (ch = *(pszStr ++)) != 0 )
    {
        if( ch == '<' )
        {
            /* printf("&lt;"); */
            VSIFPrintfL(fp, "&#60;");
        }
        else if( ch == '>' )
        {
            /* printf("&gt;"); */
            VSIFPrintfL(fp, "&#62;");
        }
        else if( ch == '&' )
        {
            /* printf("&amp;"); */
            VSIFPrintfL(fp, "&#38;");
        }
        else if( ch == '"' )
        {
            /* printf("&quot;"); */
            VSIFPrintfL(fp, "&#34;");
        }
        else if( ch == '\'' )
        {
            VSIFPrintfL(fp, "&#39;");
        }
        else if( ch < 0x20 && ch != 0x9 && ch != 0xA && ch != 0xD )
        {
            /* These control characters are unrepresentable in XML format, */
            /* so we just drop them.  #4117 */
        }
        else
        {
            VSIFWriteL(&ch, 1, 1, fp);
        }
    }
}

#define WRITE_STR(str) VSIFWriteL(str, 1, strlen(str), fp)
#define WRITE_ESCAPED(str) WriteEscaped(str, fp)

static
void myNotifyNodesFunc( unsigned int nNodes, OSMNode* pasNodes,
                        OSMContext* /* psOSMContext */, void* user_data )
{
    VSILFILE* fp = static_cast<VSILFILE *>(user_data);

    for( int k = 0; k < static_cast<int>(nNodes); k++ )
    {
        const OSMNode* psNode = &pasNodes[k];

        WRITE_STR(" <node id=\"");
        VSIFPrintfL(fp, "%d", static_cast<int>(psNode->nID));
        WRITE_STR("\" lat=\"");
        VSIFPrintfL(fp, "%.7f", psNode->dfLat);
        WRITE_STR("\" lon=\"");
        VSIFPrintfL(fp, "%.7f", psNode->dfLon);
        WRITE_STR("\" version=\"");
        VSIFPrintfL(fp, "%d", psNode->sInfo.nVersion);
        WRITE_STR("\" changeset=\"");
        VSIFPrintfL(fp, "%d", static_cast<int>(psNode->sInfo.nChangeset));
        if (psNode->sInfo.nUID >= 0)
        {
            WRITE_STR("\" user=\"");
            WRITE_ESCAPED(psNode->sInfo.pszUserSID);
            WRITE_STR("\" uid=\"");
            VSIFPrintfL(fp, "%d", psNode->sInfo.nUID);
        }

        if( !(psNode->sInfo.bTimeStampIsStr) )
        {
            WRITE_STR("\" timestamp=\"");
            struct tm mytm;
            myCPLUnixTimeToYMDHMS(psNode->sInfo.ts.nTimeStamp, &mytm);
            VSIFPrintfL(fp, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                    1900 + mytm.tm_year, mytm.tm_mon + 1, mytm.tm_mday,
                    mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
        }
        else if( psNode->sInfo.ts.pszTimeStamp != NULL &&
                 psNode->sInfo.ts.pszTimeStamp[0] != '\0' )
        {
            WRITE_STR("\" timestamp=\"");
            WRITE_STR(psNode->sInfo.ts.pszTimeStamp);
        }

        if( psNode->nTags )
        {
            WRITE_STR("\">\n");
            OSMTag *pasTags = psNode->pasTags;
            for( unsigned int l = 0; l < psNode->nTags; l++ )
            {
                WRITE_STR("  <tag k=\"");
                WRITE_ESCAPED(pasTags[l].pszK);
                WRITE_STR("\" v=\"");
                WRITE_ESCAPED(pasTags[l].pszV);
                WRITE_STR("\" />\n");
            }
            WRITE_STR(" </node>\n");
        }
        else
        {
            WRITE_STR("\"/>\n");
        }
    }
}

static
void myNotifyWayFunc( OSMWay* psWay, OSMContext* /* psOSMContext */,
                      void* user_data )
{
    VSILFILE* fp = static_cast<VSILFILE *>(user_data);

    WRITE_STR(" <way id=\"");
    VSIFPrintfL(fp, "%d", static_cast<int>(psWay->nID));
    WRITE_STR("\" version=\"");
    VSIFPrintfL(fp, "%d", psWay->sInfo.nVersion);
    WRITE_STR("\" changeset=\"");
    VSIFPrintfL(fp, "%d", static_cast<int>(psWay->sInfo.nChangeset));
    if (psWay->sInfo.nUID >= 0)
    {
        WRITE_STR("\" uid=\"");
        VSIFPrintfL(fp, "%d", psWay->sInfo.nUID);
        WRITE_STR("\" user=\"");
        WRITE_ESCAPED(psWay->sInfo.pszUserSID);
    }

    if( !(psWay->sInfo.bTimeStampIsStr) )
    {
        WRITE_STR("\" timestamp=\"");
        struct tm mytm;
        myCPLUnixTimeToYMDHMS(psWay->sInfo.ts.nTimeStamp, &mytm);
        VSIFPrintfL(fp, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                1900 + mytm.tm_year, mytm.tm_mon + 1, mytm.tm_mday,
                mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
    }
    else if( psWay->sInfo.ts.pszTimeStamp != NULL &&
             psWay->sInfo.ts.pszTimeStamp[0] != '\0' )
    {
        WRITE_STR("\" timestamp=\"");
        WRITE_STR(psWay->sInfo.ts.pszTimeStamp);
    }

    WRITE_STR("\">\n");

    for( unsigned int l = 0; l < psWay->nRefs; l++ )
        VSIFPrintfL(fp, "  <nd ref=\"%d\"/>\n",
                    static_cast<int>(psWay->panNodeRefs[l]));

    for( unsigned int l = 0; l < psWay->nTags; l++ )
    {
        WRITE_STR("  <tag k=\"");
        WRITE_ESCAPED(psWay->pasTags[l].pszK);
        WRITE_STR("\" v=\"");
        WRITE_ESCAPED(psWay->pasTags[l].pszV);
        WRITE_STR("\" />\n");
    }
    VSIFPrintfL(fp, " </way>\n");
}

static
void myNotifyRelationFunc( OSMRelation* psRelation,
                           OSMContext* /* psOSMContext */, void* user_data )
{
    VSILFILE* fp = static_cast<VSILFILE *>(user_data);


    WRITE_STR(" <relation id=\"");
    VSIFPrintfL(fp, "%d", static_cast<int>(psRelation->nID));
    WRITE_STR("\" version=\"");
    VSIFPrintfL(fp, "%d", psRelation->sInfo.nVersion);
    WRITE_STR("\" changeset=\"");
    VSIFPrintfL(fp, "%d", static_cast<int>(psRelation->sInfo.nChangeset));
    if( psRelation->sInfo.nUID >= 0 )
    {
        WRITE_STR("\" uid=\"");
        VSIFPrintfL(fp, "%d", psRelation->sInfo.nUID);
        WRITE_STR("\" user=\"");
        WRITE_ESCAPED(psRelation->sInfo.pszUserSID);
    }

    if( !(psRelation->sInfo.bTimeStampIsStr) )
    {
        struct tm mytm;
        myCPLUnixTimeToYMDHMS(psRelation->sInfo.ts.nTimeStamp, &mytm);
        WRITE_STR("\" timestamp=\"");
        VSIFPrintfL(fp, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                1900 + mytm.tm_year, mytm.tm_mon + 1, mytm.tm_mday,
                mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
    }
    else if( psRelation->sInfo.ts.pszTimeStamp != NULL &&
             psRelation->sInfo.ts.pszTimeStamp[0] != '\0' )
    {
        WRITE_STR("\" timestamp=\"");
        WRITE_STR(psRelation->sInfo.ts.pszTimeStamp);
    }

    WRITE_STR("\">\n");

    const OSMTag* pasTags = psRelation->pasTags;
    const OSMMember* pasMembers = psRelation->pasMembers;

    for( unsigned int l = 0; l < psRelation->nMembers; l++ )
    {
        WRITE_STR("  <member type=\"");
        VSIFPrintfL(fp, "%s",
                    pasMembers[l].eType == MEMBER_NODE
                    ? "node"
                    : pasMembers[l].eType == MEMBER_WAY ? "way": "relation");
        WRITE_STR("\" ref=\"");
        VSIFPrintfL(fp, "%d", static_cast<int>(pasMembers[l].nID));
        WRITE_STR("\" role=\"");
        WRITE_ESCAPED(pasMembers[l].pszRole);
        WRITE_STR("\"/>\n");
    }

    for( unsigned int l = 0; l < psRelation->nTags; l++ )
    {
        WRITE_STR("  <tag k=\"");
        WRITE_ESCAPED(pasTags[l].pszK);
        WRITE_STR("\" v=\"");
        WRITE_ESCAPED(pasTags[l].pszV);
        WRITE_STR("\" />\n");
    }
    VSIFPrintfL(fp, " </relation>\n");
}

static
void myNotifyBoundsFunc( double dfXMin, double dfYMin,
                         double dfXMax, double dfYMax,
                         OSMContext* /* psOSMContext */, void* user_data )
{
    VSILFILE* fp = static_cast<VSILFILE *>(user_data);
    VSIFPrintfL(fp, " <bounds minlat=\"%.7f\" minlon=\"%.7f\""
                " maxlat=\"%.7f\" maxlon=\"%.7f\"/>\n",
                dfYMin, dfXMin, dfYMax, dfXMax);
}

int main( int argc, char* argv[] )
{
    if( argc != 3 )
    {
        fprintf(stderr, "Usage: osm2osm input.pbf output.osm\n");  /*ok*/
        exit(1);
    }

    const char *pszSrcFilename = argv[1];
    const char *pszDstFilename = argv[2];

    VSILFILE* fp = VSIFOpenL(pszDstFilename, "wt");
    if( fp == NULL )
    {
        fprintf(stderr, "Cannot create %s.\n", pszDstFilename);  /*ok*/
        exit(1);
    }

    OSMContext* psContext = OSM_Open(pszSrcFilename,
                                     myNotifyNodesFunc,
                                     myNotifyWayFunc,
                                     myNotifyRelationFunc,
                                     myNotifyBoundsFunc,
                                     fp);
    if( psContext == NULL )
    {
        fprintf(stderr, "Cannot process %s.\n", pszSrcFilename);  /*ok*/
        exit(1);
    }

    VSIFPrintfL(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    VSIFPrintfL(fp, "<osm version=\"0.6\" generator=\"pbttoosm\">\n");

    while( OSM_ProcessBlock(psContext) == OSM_OK );

    VSIFPrintfL(fp, "</osm>\n");

    OSM_Close(psContext);

    VSIFCloseL(fp);

    return 0;
}
