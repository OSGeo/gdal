/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 * Purpose:  OSM XML and OSM PBF parser
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OSM_PARSER_H_INCLUDED
#define _OSM_PARSER_H_INCLUDED

#include "cpl_port.h"
/* typedef long long GIntBig; */

CPL_C_START

typedef struct
{
    const char *pszK;
    const char *pszV;
} OSMTag;

typedef struct
{
    union
    {
        GIntBig      nTimeStamp;
        const char  *pszTimeStamp;
    } ts;
    GIntBig      nChangeset;
    int          nVersion;
    int          nUID;
    int          bTimeStampIsStr;
    const char  *pszUserSID;
} OSMInfo;

typedef struct
{
    GIntBig      nID;
    double       dfLat;
    double       dfLon;
    OSMInfo      sInfo;
    unsigned int nTags;
    OSMTag      *pasTags;
} OSMNode;

typedef struct
{
    GIntBig      nID;
    OSMInfo      sInfo;
    unsigned int nTags;
    unsigned int nRefs;
    OSMTag      *pasTags;
    GIntBig     *panNodeRefs;
} OSMWay;

typedef enum
{
    MEMBER_NODE = 0,
    MEMBER_WAY = 1,
    MEMBER_RELATION = 2
} OSMMemberType;

typedef struct
{
    GIntBig        nID;
    const char    *pszRole;
    OSMMemberType  eType;
} OSMMember;

typedef struct
{
    GIntBig      nID;
    OSMInfo      sInfo;
    unsigned int nTags;
    unsigned int nMembers;
    OSMTag      *pasTags;
    OSMMember   *pasMembers;
} OSMRelation;

typedef enum
{
    OSM_OK,
    OSM_EOF,
    OSM_ERROR
} OSMRetCode;

typedef struct _OSMContext OSMContext;

typedef void (*NotifyNodesFunc) (unsigned int nNodes, OSMNode* pasNodes, OSMContext* psOSMContext, void* user_data);
typedef void (*NotifyWayFunc) (OSMWay* psWay, OSMContext* psOSMContext, void* user_data);
typedef void (*NotifyRelationFunc) (OSMRelation* psRelation, OSMContext* psOSMContext, void* user_data);
typedef void (*NotifyBoundsFunc) (double dfXMin, double dfYMin, double dfXMax, double dfYMax, OSMContext* psOSMContext, void* user_data);

OSMContext* OSM_Open( const char* pszFilename,
                      NotifyNodesFunc pfnNotifyNodes,
                      NotifyWayFunc pfnNotifyWay,
                      NotifyRelationFunc pfnNotifyRelation,
                      NotifyBoundsFunc pfnNotifyBounds,
                      void* user_data );

GUIntBig OSM_GetBytesRead( OSMContext* psOSMContext );

void OSM_ResetReading( OSMContext* psOSMContext );

OSMRetCode OSM_ProcessBlock( OSMContext* psOSMContext );

void OSM_Close( OSMContext* psOSMContext );

CPL_C_END

#endif /*  _OSM_PARSER_H_INCLUDED */
