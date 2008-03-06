/******************************************************************************
 * $Id: ogr_xplane.h $
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Definition of classes for OGR X-Plane aeronautical data driver.
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

#ifndef _OGR_XPLANE_H_INCLUDED
#define _OGR_XPLANE_H_INCLUDED

#include "ogrsf_frmts.h"

class OGRXPlaneDataSource;

/************************************************************************/
/*                       OGRXPlaneEnumeration                           */
/***********************************************************************/

typedef struct
{
    int         eValue;
    const char* pszText;
} sEnumerationElement;

class OGRXPlaneEnumeration
{
    private:
        const char*                 m_pszEnumerationName;
        const sEnumerationElement*  m_osElements;
        int                         m_nElements;

    public:
        OGRXPlaneEnumeration(const char *pszEnumerationName,
                             const sEnumerationElement*  osElements,
                             int nElements);

        const char* GetText(int eValue);

        int         GetValue(const char* pszText);
};

#define DEFINE_XPLANE_ENUMERATION(enumerationName, enumerationValues) \
static OGRXPlaneEnumeration enumerationName( #enumerationName, enumerationValues, \
                    sizeof( enumerationValues ) / sizeof( enumerationValues[0] ) );

/************************************************************************/
/*                             OGRXPlaneLayer                           */
/************************************************************************/

class OGRXPlaneLayer : public OGRLayer
{
  private:
    int                nFID;
    int                nCurrentID;
    OGRFeature**       papoFeatures;

  protected:
    OGRFeatureDefn*    poFeatureDefn;
                       OGRXPlaneLayer(const char* pszLayerName);

    void               RegisterFeature(OGRFeature* poFeature);

  public:
                        ~OGRXPlaneLayer();

    virtual void              ResetReading();
    virtual OGRFeature *      GetNextFeature();
    virtual OGRFeature *      GetFeature( long nFID );
    virtual int               GetFeatureCount( int bForce = TRUE );

    virtual OGRFeatureDefn *  GetLayerDefn() { return poFeatureDefn; }
    virtual int               TestCapability( const char * pszCap );
};

/************************************************************************/
/*                           OGRXPlaneILSLayer                          */
/************************************************************************/


class OGRXPlaneILSLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneILSLayer();
    OGRFeature*         AddFeature(const char* pszNavaidID,
                                   const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfTrueHeading);
};

/************************************************************************/
/*                           OGRXPlaneVORLayer                          */
/************************************************************************/


class OGRXPlaneVORLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneVORLayer();
    OGRFeature*         AddFeature(const char* pszNavaidID,
                                   const char* pszNavaidName,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfSlavedVariation);
};

/************************************************************************/
/*                           OGRXPlaneNDBLayer                          */
/************************************************************************/


class OGRXPlaneNDBLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneNDBLayer();
    OGRFeature*         AddFeature(const char* pszNavaidID,
                                   const char* pszNavaidName,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange);
};

/************************************************************************/
/*                           OGRXPlaneGSLayer                          */
/************************************************************************/


class OGRXPlaneGSLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneGSLayer();
    OGRFeature*         AddFeature(const char* pszNavaidID,
                                   const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfTrueHeading,
                                   double dfSlope);
};

/************************************************************************/
/*                          OGRXPlaneMarkerLayer                        */
/************************************************************************/


class OGRXPlaneMarkerLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneMarkerLayer();
    OGRFeature*         AddFeature(const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfTrueHeading);
};

/************************************************************************/
/*                         OGRXPlaneDMEILSLayer                         */
/************************************************************************/


class OGRXPlaneDMEILSLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneDMEILSLayer();
    OGRFeature*         AddFeature(const char* pszNavaidID,
                                   const char* pszAptICAO,
                                   const char* pszRwyNum,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfDMEBias);
};


/************************************************************************/
/*                           OGRXPlaneDMELayer                          */
/************************************************************************/


class OGRXPlaneDMELayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneDMELayer();
    OGRFeature*         AddFeature(const char* pszNavaidID,
                                   const char* pszNavaidName,
                                   const char* pszSubType,
                                   double dfLat,
                                   double dfLon,
                                   double dfEle,
                                   double dfFreq,
                                   double dfRange,
                                   double dfDMEBias);
};

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
                                   double dfElevation,
                                   int bHasCoordinates = FALSE,
                                   double dfLat = 0,
                                   double dfLon = 0,
                                   int bHasTower = FALSE,
                                   double dfHeightTower = 0,
                                   const char* pszTowerName = NULL);
};


/************************************************************************/
/*                OGRXPlaneRunwayThresholdLayerLayer                    */
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
                                   int bHasMIRL,
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
                                   int bHasMIRL,
                                   int bHasDistanceRemainingSigns);
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
                                   int bYellowEdgeLights);
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
                                   int bYellowEdgeLights);
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


/************************************************************************/
/*                           OGRXPlaneDataSource                        */
/************************************************************************/

class OGRXPlaneDataSource : public OGRDataSource
{
    char*               pszName;

    OGRXPlaneLayer**    papoLayers;
    int                 nLayers;

    void                RegisterLayer( OGRXPlaneLayer* poLayer );
    int                 ParseNavDatFile( const char * pszFilename );
    int                 ParseAptDatFile( const char * pszFilename );

  public:
                        OGRXPlaneDataSource();
                        ~OGRXPlaneDataSource();

    int                 Open( const char * pszFilename );

    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer*   GetLayer( int );
    virtual const char* GetName() { return pszName; }

    virtual int         TestCapability( const char * pszCap );
};

/************************************************************************/
/*                             OGRXPlaneDriver                          */
/************************************************************************/

class OGRXPlaneDriver : public OGRSFDriver
{
  public:

    virtual const char* GetName();
    OGRDataSource*      Open( const char *, int );

    virtual int         TestCapability( const char * pszCap );
};


#endif /* ndef _OGR_XPLANE_H_INCLUDED */
