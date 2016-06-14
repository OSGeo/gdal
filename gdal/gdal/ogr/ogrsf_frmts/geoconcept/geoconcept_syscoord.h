/**********************************************************************
 * $Id: geoconcept_syscoord.h
 *
 * Name:     geoconcept_syscoord.h
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements translation between Geoconcept SysCoord
 *           and OGRSpatialRef format
 * Language: C
 *
 **********************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
 * Copyright (c) 2008, Even Rouault <even dot rouault at mines-paris dot org>
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
 **********************************************************************/
#ifndef GEOCONCEPT_SYSCOORD_H_INCLUDED
#define GEOCONCEPT_SYSCOORD_H_INCLUDED

#include "ogr_srs_api.h"

#ifdef GCSRS_DLLEXPORT
#  define GCSRSAPI_CALL     __declspec(dllexport)
#  define GCSRSAPI_CALL1(x) __declspec(dllexport) x
#endif

#ifndef GCSRSAPI_CALL
#  define GCSRSAPI_CALL
#endif

#ifndef GCSRSAPI_CALL1
#  define GCSRSAPI_CALL1(x) x GCSRSAPI_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/*      GCSRS API Types                                                 */
/* -------------------------------------------------------------------- */
typedef struct _tSpheroidInfo_GCSRS GCSpheroidInfo;
typedef struct _tDatumInfo_GCSRS GCDatumInfo;
typedef struct _tProjectionInfo_GCSRS GCProjectionInfo;
typedef struct _tSysCoord_GCSRS GCSysCoord;

struct _tSpheroidInfo_GCSRS {
  const char *pszSpheroidName;
  double      dfA; /* semi major axis in meters */
  double      dfE; /* eccentricity */
  int         nEllipsoidID;
};

struct _tDatumInfo_GCSRS {
  const char *pszDatumName;
  double      dfShiftX;
  double      dfShiftY;
  double      dfShiftZ;
  double      dfRotX;
  double      dfRotY;
  double      dfRotZ;
  double      dfScaleFactor;
  double      dfDiffA; /*
                        * semi-major difference to-datum minus from-datum :
                        * http://home.hiwaay.net/~taylorc/bookshelf/math-science/geodesy/datum/transform/molodensky/
                        */
  double      dfDiffFlattening; /*
                                 * Change in flattening : "to" minus "from"
                                 */
  int         nEllipsoidID;
  int         nDatumID;
};

struct _tProjectionInfo_GCSRS {
  const char *pszProjName;
  /* TODO: Translate to English. */
  int         nSphere;/*
                       * 1 = sphere de courbure
                       * 2 = sphere equatoriale
                       * 3 = sphere bitangeante
                       * 4 = sphere polaire nord
                       * 5 = sphere polaire sud
                       * 6 = Hotine
                       */
  int         nProjID;
};

struct _tSysCoord_GCSRS {
  const char   *pszSysCoordName;
  const char   *pszUnit;

  double  dfPM;
  /* inherited : */
  double  dfLambda0;
  double  dfPhi0;
  double  dfk0;
  double  dfX0;
  double  dfY0;
  double  dfPhi1;
  double  dfPhi2;

  int     nDatumID;
  int     nProjID;
  int     coordSystemID;
  int     timeZoneValue;/* when 0, replace by zone */
};

/* -------------------------------------------------------------------- */
/*      GCSRS API Prototypes                                            */
/* -------------------------------------------------------------------- */

#define GetInfoSpheroidID_GCSRS(theSpheroid)              (theSpheroid)->nEllipsoidID
#define GetInfoSpheroidName_GCSRS(theSpheroid)            (theSpheroid)->pszSpheroidName
#define GetInfoSpheroidSemiMajor_GCSRS(theSpheroid)       (theSpheroid)->dfA
#define GetInfoSpheroidExcentricity_GCSRS(theSpheroid)    (theSpheroid)->dfE

#define GetInfoDatumID_GCSRS(theDatum)                    (theDatum)->nDatumID
#define GetInfoDatumName_GCSRS(theDatum)                  (theDatum)->pszDatumName
#define GetInfoDatumShiftX_GCSRS(theDatum)                (theDatum)->dfShiftX
#define GetInfoDatumShiftY_GCSRS(theDatum)                (theDatum)->dfShiftY
#define GetInfoDatumShiftZ_GCSRS(theDatum)                (theDatum)->dfShiftZ
#define GetInfoDatumDiffA_GCSRS(theDatum)                 (theDatum)->dfDiffA
#define GetInfoDatumRotationX_GCSRS(theDatum)             (theDatum)->dfRotX
#define GetInfoDatumRotationY_GCSRS(theDatum)             (theDatum)->dfRotY
#define GetInfoDatumRotationZ_GCSRS(theDatum)             (theDatum)->dfRotZ
#define GetInfoDatumScaleFactor_GCSRS(theDatum)           (theDatum)->dfScaleFactor
#define GetInfoDatumDiffFlattening_GCSRS(theDatum)        (theDatum)->dfDiffFlattening
#define GetInfoDatumSpheroidID_GCSRS(theDatum)            (theDatum)->nEllipsoidID

#define GetInfoProjID_GCSRS(theProj)                      (theProj)->nProjID
#define GetInfoProjName_GCSRS(theProj)                    (theProj)->pszProjName
#define GetInfoProjSphereType_GCSRS(theProj)              (theProj)->nSphere
#define GetInfoProjSpheroidID_GCSRS(theProj)              (theProj)->nEllipsoidID

GCSysCoord GCSRSAPI_CALL1(*) CreateSysCoord_GCSRS ( int srsid, int timezone );
void GCSRSAPI_CALL DestroySysCoord_GCSRS ( GCSysCoord** theSysCoord );
#define GetSysCoordSystemID_GCSRS(theSysCoord)            (theSysCoord)->coordSystemID
#define SetSysCoordSystemID_GCSRS(theSysCoord,v)          (theSysCoord)->coordSystemID= (v)
#define GetSysCoordTimeZone_GCSRS(theSysCoord)            (theSysCoord)->timeZoneValue
#define SetSysCoordTimeZone_GCSRS(theSysCoord,v)          (theSysCoord)->timeZoneValue= (v)
#define GetSysCoordName_GCSRS(theSysCoord)                (theSysCoord)->pszSysCoordName
#define SetSysCoordName_GCSRS(theSysCoord,v)              (theSysCoord)->pszSysCoordName= (v)
#define GetSysCoordUnit_GCSRS(theSysCoord)                (theSysCoord)->pszUnit
#define SetSysCoordUnit_GCSRS(theSysCoord,v)              (theSysCoord)->pszUnit= (v)
#define GetSysCoordPrimeMeridian_GCSRS(theSysCoord)       (theSysCoord)->dfPM
#define SetSysCoordPrimeMeridian_GCSRS(theSysCoord,v)     (theSysCoord)->dfPM= (v)
#define GetSysCoordCentralMeridian_GCSRS(theSysCoord)     (theSysCoord)->dfLambda0
#define SetSysCoordCentralMeridian_GCSRS(theSysCoord,v)   (theSysCoord)->dfLambda0= (v)
#define GetSysCoordLatitudeOfOrigin_GCSRS(theSysCoord)    (theSysCoord)->dfPhi0
#define SetSysCoordLatitudeOfOrigin_GCSRS(theSysCoord,v)  (theSysCoord)->dfPhi0= (v)
#define GetSysCoordStandardParallel1_GCSRS(theSysCoord)   (theSysCoord)->dfPhi1
#define SetSysCoordStandardParallel1_GCSRS(theSysCoord,v) (theSysCoord)->dfPhi1= (v)
#define GetSysCoordStandardParallel2_GCSRS(theSysCoord)   (theSysCoord)->dfPhi2
#define SetSysCoordStandardParallel2_GCSRS(theSysCoord,v) (theSysCoord)->dfPhi2= (v)
#define GetSysCoordScaleFactor_GCSRS(theSysCoord)         (theSysCoord)->dfk0
#define SetSysCoordScaleFactor_GCSRS(theSysCoord,v)       (theSysCoord)->dfk0= (v)
#define GetSysCoordFalseEasting_GCSRS(theSysCoord)        (theSysCoord)->dfX0
#define SetSysCoordFalseEasting_GCSRS(theSysCoord,v)      (theSysCoord)->dfX0= (v)
#define GetSysCoordFalseNorthing_GCSRS(theSysCoord)       (theSysCoord)->dfY0
#define SetSysCoordFalseNorthing_GCSRS(theSysCoord,v)     (theSysCoord)->dfY0= (v)
#define GetSysCoordDatumID_GCSRS(theSysCoord)             (theSysCoord)->nDatumID
#define SetSysCoordDatumID_GCSRS(theSysCoord,v)           (theSysCoord)->nDatumID= (v)
#define GetSysCoordProjID_GCSRS(theSysCoord)              (theSysCoord)->nProjID
#define SetSysCoordProjID_GCSRS(theSysCoord,v)            (theSysCoord)->nProjID= (v)

GCSysCoord GCSRSAPI_CALL1(*) OGRSpatialReference2SysCoord_GCSRS ( OGRSpatialReferenceH poSR );
OGRSpatialReferenceH GCSRSAPI_CALL SysCoord2OGRSpatialReference_GCSRS ( GCSysCoord* syscoord );

#ifdef __cplusplus
}
#endif


#endif /* ndef GEOCONCEPT_SYSCOORD_H_INCLUDED */
