/******************************************************************************
 * $Id: ogr_xplane_nav_reader.cpp
 *
 * Project:  X-Plane nav.dat file reader header
 * Purpose:  Definition of classes for X-Plane nav.dat file reader
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

#ifndef _OGR_XPLANE_NAV_READER_H_INCLUDED
#define _OGR_XPLANE_NAV_READER_H_INCLUDED

#include "ogr_xplane.h"
#include "ogr_xplane_reader.h"

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


enum
{
    NAVAID_NDB            = 2, 
    NAVAID_VOR            = 3, /* VOR, VORTAC or VOR-DME.*/
    NAVAID_LOC_ILS        = 4, /* Localiser that is part of a full ILS */
    NAVAID_LOC_STANDALONE = 5, /* Stand-alone localiser (LOC), also including a LDA (Landing Directional Aid) or SDF (Simplified Directional Facility) */
    NAVAID_GS             = 6, /* Glideslope */
    NAVAID_OM             = 7, /* Outer marker */
    NAVAID_MM             = 8, /* Middle marker */
    NAVAID_IM             = 9, /* Inner marker */
    NAVAID_DME_COLOC      = 12, /* DME (including the DME element of an ILS, VORTAC or VOR-DME) */
    NAVAID_DME_STANDALONE = 13, /* DME (including the DME element of an NDB-DME) */
};


/************************************************************************/
/*                           OGRXPlaneNavReader                         */
/************************************************************************/

class OGRXPlaneNavReader : public OGRXPlaneReader
{
    private:
        OGRXPlaneILSLayer*    poILSLayer;
        OGRXPlaneVORLayer*    poVORLayer;
        OGRXPlaneNDBLayer*    poNDBLayer;
        OGRXPlaneGSLayer*     poGSLayer ;
        OGRXPlaneMarkerLayer* poMarkerLayer;
        OGRXPlaneDMELayer*    poDMELayer;
        OGRXPlaneDMEILSLayer* poDMEILSLayer;

    private:
                                 OGRXPlaneNavReader();
        void                     ParseRecord(int nType);

    protected:
        virtual void             Read();

    public:
                                 OGRXPlaneNavReader( OGRXPlaneDataSource* poDataSource );
        virtual OGRXPlaneReader* CloneForLayer(OGRXPlaneLayer* poLayer);
        virtual int              IsRecognizedVersion( const char* pszVersionString);
};

#endif
