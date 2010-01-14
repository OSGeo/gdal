/******************************************************************************
 * $Id: ogr_xplane_apt_reader.h
 *
 * Project:  X-Plane apt.dat file reader headers
 * Purpose:  Definition of classes for X-Plane apt.dat file reader
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#ifndef _OGR_XPLANE_APT_READER_H_INCLUDED
#define _OGR_XPLANE_APT_READER_H_INCLUDED

#include "ogr_xplane.h"
#include "ogr_xplane_reader.h"

/************************************************************************/
/*                           OGRXPlaneAPTLayer                          */
/************************************************************************/


class OGRXPlaneAPTLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneAPTLayer();

    /* If the airport has a tower, its coordinates are the tower coordinates */
    /* If it has no tower, then we pick up the coordinates of the threshold of its first found runway */
    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszAptName,
                                   int nAPTType,
                                   double dfElevation,
                                   int bHasCoordinates = FALSE,
                                   double dfLat = 0,
                                   double dfLon = 0,
                                   int bHasTower = FALSE,
                                   double dfHeightTower = 0,
                                   const char* pszTowerName = NULL);
};


/************************************************************************/
/*                   OGRXPlaneRunwayThresholdLayer                      */
/************************************************************************/

static const sEnumerationElement runwaySurfaceType[] =
{
    { 1, "Asphalt" },
    { 2, "Concrete" },
    { 3, "Turf/grass" },
    { 4, "Dirt" },
    { 5, "Gravel" },
    { 6, "Asphalt" /* helipad V810 */},
    { 7, "Concrete" /* helipad  V810 */},
    { 8, "Turf/grass" /* helipad V810 */},
    { 9, "Dirt" /* helipad V810 */},
    { 10, "Asphalt" /* taxiway  V810 */ },
    { 11, "Concrete" /* taxiway V810  */ },
    { 12, "Dry lakebed" },
    { 13, "Water" },
    { 14, "Snow/ice" /* V850 */ },
    { 15, "Transparent" /* V850 */ }
};

static const sEnumerationElement runwayShoulderType[] =
{
    { 0, "None" },
    { 1, "Asphalt" },
    { 2, "Concrete" }
};

static const sEnumerationElement runwayMarkingType[] =
{
    { 0, "None" },
    { 1, "Visual" },
    { 2, "Non-precision approach" },
    { 3, "Precision approach" },
    { 4, "UK-style non-precision" },
    { 5, "UK-style precision" }
};

static const sEnumerationElement approachLightingType[] =
{
    { 0, "None" },
    { 1, "ALSF-I" },
    { 2, "ALSF-II" },
    { 3, "Calvert" },
    { 4, "Calvert ISL Cat II and III" },
    { 5, "SSALR" },
    { 6, "SSALF" },
    { 7, "SALS" },
    { 8, "MALSR" },
    { 9, "MALSF" },
    { 10, "MALS" },
    { 11, "ODALS" },
    { 12, "RAIL" }
};

static const sEnumerationElement approachLightingTypeV810[] =
{
    { 1, "None" },
    { 2, "SSALS" },
    { 3, "SALSF" },
    { 4, "ALSF-I" },
    { 5, "ALSF-II" },
    { 6, "ODALS" },
    { 7, "Calvert" },
    { 8, "Calvert ISL Cat II and III" },
};

static const sEnumerationElement runwayEdgeLigthingType[] =
{
    { 0, "None" },
    { 1, "LIRL" }, /* proposed for V90x */
    { 2, "MIRL" },
    { 3, "HIRL" } /* proposed for V90x */
};

static const sEnumerationElement runwayREILType[] =
{
    { 0, "None" },
    { 1, "Omni-directional" },
    { 2, "Unidirectional" }
};

static const sEnumerationElement runwayVisualApproachPathIndicatorTypeV810[] = 
{
    { 1, "None" },
    { 2, "VASI" },
    { 3, "PAPI Right" },
    { 4, "Space Shuttle PAPI" }
};

DEFINE_XPLANE_ENUMERATION(RunwaySurfaceEnumeration, runwaySurfaceType);
DEFINE_XPLANE_ENUMERATION(RunwayShoulderEnumeration, runwayShoulderType);
DEFINE_XPLANE_ENUMERATION(RunwayMarkingEnumeration, runwayMarkingType);
DEFINE_XPLANE_ENUMERATION(RunwayApproachLightingEnumeration, approachLightingType);
DEFINE_XPLANE_ENUMERATION(RunwayApproachLightingEnumerationV810, approachLightingTypeV810);
DEFINE_XPLANE_ENUMERATION(RunwayEdgeLightingEnumeration, runwayEdgeLigthingType);
DEFINE_XPLANE_ENUMERATION(RunwayREILEnumeration, runwayREILType);
DEFINE_XPLANE_ENUMERATION(RunwayVisualApproachPathIndicatorEnumerationV810, runwayVisualApproachPathIndicatorTypeV810);

class OGRXPlaneRunwayThresholdLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneRunwayThresholdLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   double dfLat,
                                   double dfLon,
                                   double dfWidth,
                                   const char* pszSurfaceType,
                                   const char* pszShoulderType,
                                   double dfSmoothness,
                                   int bHasCenterLineLights,
                                   const char* pszEdgeLighting,
                                   int bHasDistanceRemainingSigns,
                                   double dfDisplacedThresholdLength,
                                   double dfStopwayLength,
                                   const char* pszMarkings,
                                   const char* pszApproachLightingCode,
                                   int bHasTouchdownLights,
                                   const char* pszREIL);

    /* Set a few computed values */
    void                 SetRunwayLengthAndHeading(OGRFeature* poFeature,
                                                   double dfLength,
                                                   double dfHeading);

    OGRFeature*          AddFeatureFromNonDisplacedThreshold(OGRFeature* poNonDisplacedThresholdFeature);
};

/************************************************************************/
/*                          OGRXPlaneRunwayLayer                        */
/************************************************************************/


class OGRXPlaneRunwayLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneRunwayLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszRwyNum1,
                                   const char* pszRwyNum2,
                                   double dfLat1,
                                   double dfLon1,
                                   double dfLat2,
                                   double dfLon2,
                                   double dfWidth,
                                   const char* pszSurfaceType,
                                   const char* pszShoulderType,
                                   double dfSmoothness,
                                   int bHasCenterLineLights,
                                   const char* pszEdgeLighting,
                                   int bHasDistanceRemainingSigns);
};


/************************************************************************/
/*                        OGRXPlaneStopwayLayer                         */
/************************************************************************/


class OGRXPlaneStopwayLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneStopwayLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   double dfLatThreshold,
                                   double dfLonThreshold,
                                   double dfRunwayHeading,
                                   double dfWidth,
                                   double dfStopwayLength);
};

/************************************************************************/
/*                   OGRXPlaneWaterRunwayThresholdLayer                 */
/************************************************************************/


class OGRXPlaneWaterRunwayThresholdLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneWaterRunwayThresholdLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   double dfLat,
                                   double dfLon,
                                   double dfWidth,
                                   int bBuoys);

    /* Set a few computed values */
    void                 SetRunwayLengthAndHeading(OGRFeature* poFeature,
                                                   double dfLength,
                                                   double dfHeading);
};


/************************************************************************/
/*                         OGRXPlaneWaterRunwayLayer                    */
/************************************************************************/

/* Polygonal object */

class OGRXPlaneWaterRunwayLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneWaterRunwayLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszRwyNum1,
                                   const char* pszRwyNum2,
                                   double dfLat1,
                                   double dfLon1,
                                   double dfLat2,
                                   double dfLon2,
                                   double dfWidth,
                                   int bBuoys);
};


/************************************************************************/
/*                        OGRXPlaneHelipadLayer                         */
/************************************************************************/

static const sEnumerationElement helipadEdgeLigthingType[] =
{
    { 0, "None" },
    { 1, "Yellow" }, 
    { 2, "White" }, /* proposed for V90x */
    { 3, "Red" } /* proposed for V90x */
};

DEFINE_XPLANE_ENUMERATION(HelipadEdgeLightingEnumeration, helipadEdgeLigthingType);

class OGRXPlaneHelipadLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneHelipadLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszHelipadNum,
                                   double dfLat,
                                   double dfLon,
                                   double dfTrueHeading,
                                   double dfLength,
                                   double dfWidth,
                                   const char* pszSurfaceType,
                                   const char* pszMarkings,
                                   const char* pszShoulderType,
                                   double dfSmoothness,
                                   const char* pszEdgeLighing);
};

/************************************************************************/
/*                     OGRXPlaneHelipadPolygonLayer                     */
/************************************************************************/


class OGRXPlaneHelipadPolygonLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneHelipadPolygonLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszHelipadNum,
                                   double dfLat,
                                   double dfLon,
                                   double dfTrueHeading,
                                   double dfLength,
                                   double dfWidth,
                                   const char* pszSurfaceType,
                                   const char* pszMarkings,
                                   const char* pszShoulderType,
                                   double dfSmoothness,
                                   const char* pszEdgeLighing);
};


/************************************************************************/
/*                    OGRXPlaneTaxiwayRectangleLayer                    */
/************************************************************************/


class OGRXPlaneTaxiwayRectangleLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneTaxiwayRectangleLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   double dfLat,
                                   double dfLon,
                                   double dfTrueHeading,
                                   double dfLength,
                                   double dfWidth,
                                   const char* pszSurfaceType,
                                   double dfSmoothness,
                                   int bBlueEdgeLights);
};


/************************************************************************/
/*                          OGRXPlanePavementLayer                      */
/************************************************************************/


class OGRXPlanePavementLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlanePavementLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszPavementName,
                                   const char* pszSurfaceType,
                                   double dfSmoothness,
                                   double dfTextureHeading,
                                   OGRPolygon* poPolygon);
};

/************************************************************************/
/*                       OGRXPlaneAPTBoundaryLayer                      */
/************************************************************************/


class OGRXPlaneAPTBoundaryLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneAPTBoundaryLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszBoundaryName,
                                   OGRPolygon* poPolygon);
};


/************************************************************************/
/*                 OGRXPlaneAPTLinearFeatureLayer                       */
/************************************************************************/


class OGRXPlaneAPTLinearFeatureLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneAPTLinearFeatureLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszLinearFeatureName,
                                   OGRMultiLineString* poMultilineString);
};


/************************************************************************/
/*                         OGRXPlaneATCFreqLayer                         */
/************************************************************************/

class OGRXPlaneATCFreqLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneATCFreqLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszATCType,
                                   const char* pszATCFreqName,
                                   double dfFrequency);
};


/************************************************************************/
/*                     OGRXPlaneStartupLocationLayer                    */
/************************************************************************/

class OGRXPlaneStartupLocationLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneStartupLocationLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszName,
                                   double dfLat,
                                   double dfLon,
                                   double dfTrueHeading);
};

/************************************************************************/
/*                     OGRXPlaneAPTLightBeaconLayer                     */
/************************************************************************/


static const sEnumerationElement APTLightBeaconColorType[] =
{
    { 0, "None" },
    { 1, "White-green" },           /* land airport */
    { 2, "White-yellow" },          /* seaplane base */
    { 3, "Green-yellow-white" },    /* heliports */
    { 4, "White-white-green" }     /* military field */
};

DEFINE_XPLANE_ENUMERATION(APTLightBeaconColorEnumeration, APTLightBeaconColorType);

class OGRXPlaneAPTLightBeaconLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneAPTLightBeaconLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszName,
                                   double dfLat,
                                   double dfLon,
                                   const char* pszColor);
};

/************************************************************************/
/*                       OGRXPlaneAPTWindsockLayer                      */
/************************************************************************/

class OGRXPlaneAPTWindsockLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneAPTWindsockLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszName,
                                   double dfLat,
                                   double dfLon,
                                   int bIsIllumnited);
};


/************************************************************************/
/*                       OGRXPlaneTaxiwaySignLayer                      */
/************************************************************************/

class OGRXPlaneTaxiwaySignLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneTaxiwaySignLayer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszText,
                                   double dfLat,
                                   double dfLon,
                                   double dfHeading,
                                   int nSize);
};

/************************************************************************/
/*                   OGRXPlane_VASI_PAPI_WIGWAG_Layer                   */
/************************************************************************/

static const sEnumerationElement VASI_PAPI_WIGWAG_Type[] =
{
    { 1, "VASI" },
    { 2, "PAPI Left" },
    { 3, "PAPI Right" },
    { 4, "Space Shuttle PAPI" },
    { 5, "Tri-colour VASI" },
    { 6, "Wig-Wag lights" }
};

DEFINE_XPLANE_ENUMERATION(VASI_PAPI_WIGWAG_Enumeration, VASI_PAPI_WIGWAG_Type);

class OGRXPlane_VASI_PAPI_WIGWAG_Layer : public OGRXPlaneLayer
{
  public:
                        OGRXPlane_VASI_PAPI_WIGWAG_Layer();

    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   const char* pszObjectType,
                                   double dfLat,
                                   double dfLon,
                                   double dfHeading,
                                   double dfVisualGlidePathAngle);
};



enum
{
    APT_AIRPORT_HEADER         = 1,
    APT_RUNWAY_TAXIWAY_V_810   = 10,
    APT_TOWER                  = 14,
    APT_STARTUP_LOCATION       = 15,
    APT_SEAPLANE_HEADER        = 16,
    APT_HELIPORT_HEADER        = 17,
    APT_LIGHT_BEACONS          = 18,
    APT_WINDSOCKS              = 19,
    APT_TAXIWAY_SIGNS          = 20,
    APT_VASI_PAPI_WIGWAG       = 21,
    APT_ATC_AWOS_ASOS_ATIS     = 50,
    APT_ATC_CTAF               = 51,
    APT_ATC_CLD                = 52,
    APT_ATC_GND                = 53,
    APT_ATC_TWR                = 54,
    APT_ATC_APP                = 55,
    APT_ATC_DEP                = 56,
    APT_RUNWAY                 = 100,
    APT_WATER_RUNWAY           = 101,
    APT_HELIPAD                = 102,
    APT_PAVEMENT_HEADER        = 110,
    APT_NODE                   = 111,
    APT_NODE_WITH_BEZIER       = 112,
    APT_NODE_CLOSE             = 113,
    APT_NODE_CLOSE_WITH_BEZIER = 114,
    APT_NODE_END               = 115,
    APT_NODE_END_WITH_BEZIER   = 116,
    APT_LINEAR_HEADER          = 120,
    APT_BOUNDARY_HEADER        = 130,
};



/************************************************************************/
/*                           OGRXPlaneAptReader                         */
/************************************************************************/

class OGRXPlaneAptReader : public OGRXPlaneReader
{
    private:
        OGRXPlaneAPTLayer*                  poAPTLayer;
        OGRXPlaneRunwayLayer*               poRunwayLayer;
        OGRXPlaneStopwayLayer*              poStopwayLayer;
        OGRXPlaneRunwayThresholdLayer*      poRunwayThresholdLayer;
        OGRXPlaneWaterRunwayLayer*          poWaterRunwayLayer;
        OGRXPlaneWaterRunwayThresholdLayer* poWaterRunwayThresholdLayer;
        OGRXPlaneHelipadLayer*              poHelipadLayer;
        OGRXPlaneHelipadPolygonLayer*       poHelipadPolygonLayer;
        OGRXPlaneTaxiwayRectangleLayer*     poTaxiwayRectangleLayer;
        OGRXPlanePavementLayer*             poPavementLayer;
        OGRXPlaneAPTBoundaryLayer*          poAPTBoundaryLayer;
        OGRXPlaneAPTLinearFeatureLayer*     poAPTLinearFeatureLayer;
        OGRXPlaneATCFreqLayer*              poATCFreqLayer;
        OGRXPlaneStartupLocationLayer*      poStartupLocationLayer;
        OGRXPlaneAPTLightBeaconLayer*       poAPTLightBeaconLayer;
        OGRXPlaneAPTWindsockLayer*          poAPTWindsockLayer;
        OGRXPlaneTaxiwaySignLayer*          poTaxiwaySignLayer;
        OGRXPlane_VASI_PAPI_WIGWAG_Layer*   poVASI_PAPI_WIGWAG_Layer;

        int       bAptHeaderFound;
        double    dfElevation;
        int       bControlTower;
        CPLString osAptICAO;
        CPLString osAptName;
        int       nAPTType;

        int       bTowerFound;
        double    dfLatTower, dfLonTower;
        double    dfHeightTower;
        CPLString osTowerName;

        int     bRunwayFound;
        double  dfLatFirstRwy , dfLonFirstRwy;

        int     bResumeLine;

    private:
                OGRXPlaneAptReader();

        void    ParseAptHeaderRecord();
        void    ParsePavement();
        void    ParseAPTBoundary();
        void    ParseAPTLinearFeature();
        void    ParseRunwayTaxiwayV810Record();
        void    ParseRunwayRecord();
        void    ParseWaterRunwayRecord();
        void    ParseHelipadRecord();
        void    ParseTowerRecord();
        void    ParseATCRecord(int nType);
        void    ParseStartupLocationRecord();
        void    ParseLightBeaconRecord();
        void    ParseWindsockRecord();
        void    ParseTaxiwaySignRecord();
        void    ParseVasiPapiWigWagRecord();

        OGRGeometry* FixPolygonTopology(OGRPolygon& polygon);
        int     ParsePolygonalGeometry(OGRGeometry** ppoGeom);
        int     ParseLinearGeometry(OGRMultiLineString& multilinestring, int* pbIsValid);

        static void    AddBezierCurve (OGRLineString& lineString,
                                double dfLatA, double dfLonA,
                                double dfCtrPtLatA, double dfCtrPtLonA,
                                double dfSymCtrlPtLatB, double dfSymCtrlPtLonB,
                                double dfLatB, double dfLonB);
        static void    AddBezierCurve (OGRLineString& lineString,
                                double dfLatA, double dfLonA,
                                double dfCtrPtLat, double dfCtrPtLon,
                                double dfLatB, double dfLonB);

    protected:
        virtual void             Read();

    public:
                                 OGRXPlaneAptReader( OGRXPlaneDataSource* poDataSource );
        virtual OGRXPlaneReader* CloneForLayer(OGRXPlaneLayer* poLayer);
        virtual int              IsRecognizedVersion( const char* pszVersionString);
        virtual void             Rewind();
};

#endif
