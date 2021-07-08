/******************************************************************************
 * $Id: VRCutils.h,v 1.6 2021/06/26 19:10:13 werdna Exp $
 *
 * Author:  Andrew C Aitchison
 *
 ******************************************************************************
 * Copyright (c) 2020, Andrew C Aitchison
 *****************************************************************************/

// Everything declared here is also declared in VRC.h
// #ifndef VRC_H_INCLUDED

#ifndef VRC_UTILS_H_INCLUDED
#define VRC_UTILS_H_INCLUDED

#pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wformat-pedantic"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wsuggest-destructor-override"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wunused-template"
#pragma clang diagnostic ignored "-Wweak-vtables"
#include "gdal_pam.h"
#pragma clang diagnostic pop
#include "ogr_spatialref.h"
//#include "cpl_string.h"

#if GDAL_VERSION_NUM < 2010000
#define GDAL_IDENTIFY_UNKNOWN -1
#define GDAL_IDENTIFY_FALSE 0
#define GDAL_IDENTIFY_TRUE 1
#else
// These are defined in gdal/gdal_priv.h
#endif // GDAL_VERSION_NUM < 2010000

#if 0 // __cplusplus <= 201103L
#define override
#define nullptr 0
#endif

#ifdef CODE_ANALYSIS

// Printing variables with CPLDebug can hide
// the fact that they are not otherwise used ...
#define CPLDebug(...)

#endif // CODE_ANALYSIS

int VRReadChar(VSILFILE *fp);
int VRReadInt(VSILFILE *fp);
void VRC_file_strerror_r(int nFileErr, char *buf, size_t buflen);

extern OGRSpatialReference* CRSfromCountry(int nCountry);
extern const char* CharsetFromCountry(int nCountry);

extern void
dumpTileHeaderData(
                   VSILFILE *fp,
                   unsigned int nTileIndex,
                   unsigned int nOverviewCount,
                   unsigned int anTileOverviewIndex[],
                   const int tile_xx, const int tile_yy );

extern short VRGetShort( void* base, int byteOffset );
extern signed int VRGetInt( void* base, unsigned int byteOffset );
extern unsigned int VRGetUInt( void* base, unsigned int byteOffset );

extern int VRReadChar(VSILFILE *fp);
extern int VRReadShort(VSILFILE *fp);
extern int VRReadInt(VSILFILE *fp);
extern int VRReadInt(VSILFILE *fp, unsigned int byteOffset );
extern unsigned int VRReadUInt(VSILFILE *fp);
extern unsigned int VRReadUInt(VSILFILE *fp, unsigned int byteOffset );

#endif // ndef VRC_UTILS_H_INCLUDED

// #endif // ndef VRC_H_INCLUDED
