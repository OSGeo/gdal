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
    void                AddFeature(const char* pszNavaidID,
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
    void                AddFeature(const char* pszNavaidID,
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
    void                AddFeature(const char* pszNavaidID,
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
    void                AddFeature(const char* pszNavaidID,
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
    void                AddFeature(const char* pszAptICAO,
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
    void                AddFeature(const char* pszNavaidID,
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
    void                AddFeature(const char* pszNavaidID,
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
    void                AddFeature(const char* pszAptICAO,
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
/*                         OGRXPlaneRunwayLayer                         */
/************************************************************************/

static const sEnumerationElement runwaySurfaceType[] =
{
    { 1, "Asphalt" },
    { 2, "Concrete" },
    { 3, "Turf/grass" },
    { 4, "Dirt" },
    { 5, "Gravel" },
    { 12, "Dry lakebed" },
    { 13, "Water" },
    { 14, "Snow/ice" },
    { 15, "Transparent" }
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

static const sEnumerationElement runwayREILType[] =
{
    { 0, "None" },
    { 1, "omni-directional" },
    { 2, "unidirectional" }
};

DEFINE_XPLANE_ENUMERATION(RunwaySurfaceEnumeration, runwaySurfaceType);
DEFINE_XPLANE_ENUMERATION(RunwayShoulderEnumeration, runwayShoulderType);
DEFINE_XPLANE_ENUMERATION(RunwayMarkingEnumeration, runwayMarkingType);
DEFINE_XPLANE_ENUMERATION(RunwayApproachLightingEnumeration, approachLightingType);
DEFINE_XPLANE_ENUMERATION(RunwayREILEnumeration, runwayREILType);


class OGRXPlaneRunwayLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneRunwayLayer();

    void                AddFeature(const char* pszAptICAO,
                                   const char* pszRunwayID,
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
};


/************************************************************************/
/*                         OGRXPlaneATCFreqLayer                         */
/************************************************************************/

class OGRXPlaneATCFreqLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneATCFreqLayer();

    void                AddFeature(const char* pszAptICAO,
                                   const char* pszATCType,
                                   const char* pszATCFreqName,
                                   double dFrequency);
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
