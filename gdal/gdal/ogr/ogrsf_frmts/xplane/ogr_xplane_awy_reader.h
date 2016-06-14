/******************************************************************************
 * $Id: ogr_xplane_awy_reader.cpp
 *
 * Project:  X-Plane awy.dat file reader header
 * Purpose:  Definition of classes for X-Plane awy.dat file reader
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef OGR_XPLANE_AWY_READER_H_INCLUDED
#define OGR_XPLANE_AWY_READER_H_INCLUDED

#include "ogr_xplane.h"
#include "ogr_xplane_reader.h"

#include "cpl_hash_set.h"

/************************************************************************/
/*                   OGRXPlaneAirwaySegmentLayer                        */
/************************************************************************/


class OGRXPlaneAirwaySegmentLayer : public OGRXPlaneLayer
{
  public:
                        OGRXPlaneAirwaySegmentLayer();
    OGRFeature*         AddFeature(const char* pszAirwaySegmentName,
                                   const char* pszFirstPointName,
                                   const char* pszSecondPointName,
                                   double dfLat1,
                                   double dfLon1,
                                   double dfLat2,
                                   double dfLon2,
                                   int    bIsHigh,
                                   int    nBaseFL,
                                   int    nTopFL);
};

/************************************************************************/
/*                 OGRXPlaneAirwayIntersectionLayer                     */
/************************************************************************/

class OGRXPlaneAirwayIntersectionLayer : public OGRXPlaneLayer
{
  private:
    CPLHashSet* poSet;

  public:
                        OGRXPlaneAirwayIntersectionLayer();
                        ~OGRXPlaneAirwayIntersectionLayer();

    OGRFeature*         AddFeature(const char* pszIntersectionName,
                                   double dfLat,
                                   double dfLon);

    virtual void        ResetReading();
};

/************************************************************************/
/*                           OGRXPlaneAwyReader                         */
/************************************************************************/

class OGRXPlaneAwyReader : public OGRXPlaneReader
{
    private:
        OGRXPlaneAirwaySegmentLayer*      poAirwaySegmentLayer;
        OGRXPlaneAirwayIntersectionLayer* poAirwayIntersectionLayer;

    private:
                                 OGRXPlaneAwyReader();
        void                     ParseRecord();

    protected:
        virtual void             Read();

    public:
                                 OGRXPlaneAwyReader( OGRXPlaneDataSource* poDataSource );
        virtual OGRXPlaneReader* CloneForLayer(OGRXPlaneLayer* poLayer);
        virtual int              IsRecognizedVersion( const char* pszVersionString);
};

#endif
