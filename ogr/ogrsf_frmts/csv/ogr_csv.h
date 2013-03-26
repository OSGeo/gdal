/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Definition of classes for OGR .csv driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004,  Frank Warmerdam
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

#ifndef _OGR_CSV_H_INCLUDED
#define _OGR_CSV_H_INCLUDED

#include "ogrsf_frmts.h"

typedef enum
{
    OGR_CSV_GEOM_NONE,
    OGR_CSV_GEOM_AS_WKT,
    OGR_CSV_GEOM_AS_XYZ,
    OGR_CSV_GEOM_AS_XY,
    OGR_CSV_GEOM_AS_YX,
} OGRCSVGeometryFormat;

class OGRCSVDataSource;

char **OGRCSVReadParseLineL( VSILFILE * fp, char chDelimiter, int bDontHonourStrings );

/************************************************************************/
/*                             OGRCSVLayer                              */
/************************************************************************/

class OGRCSVLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    VSILFILE           *fpCSV;

    int                 nNextFID;

    int                 bHasFieldNames;

    OGRFeature *        GetNextUnfilteredFeature();

    int                 bNew;
    int                 bInWriteMode;
    int                 bUseCRLF;
    int                 bNeedRewindBeforeRead;
    OGRCSVGeometryFormat eGeometryFormat;

    char*               pszFilename;
    int                 bCreateCSVT;
    int                 bWriteBOM;
    char                chDelimiter;

    int                 iWktGeomReadField;
    int                 bFirstFeatureAppendedDuringSession;

    /*http://www.faa.gov/airports/airport_safety/airportdata_5010/menu/index.cfm specific */
    int                 iNfdcLatitudeS, iNfdcLongitudeS;
    int                 bDontHonourStrings;

    /* GNIS specific */
    int                 iLongitudeField, iLatitudeField;

    int                 bIsEurostatTSV;
    int                 nEurostatDims;

    int                 nTotalFeatures;

  public:
    OGRCSVLayer( const char *pszName, VSILFILE *fp, const char *pszFilename,
                 int bNew, int bInWriteMode, char chDelimiter,
                 const char* pszNfdcRunwaysGeomField,
                 const char* pszGeonamesGeomFieldPrefix );
  ~OGRCSVLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRErr      CreateFeature( OGRFeature *poFeature );

    void                SetCRLF(int);
    void                SetWriteGeometry(OGRCSVGeometryFormat eGeometryFormat);
    void                SetCreateCSVT(int bCreateCSVT);
    void                SetWriteBOM(int bWriteBOM);

    virtual int         GetFeatureCount( int bForce = TRUE );

    OGRErr              WriteHeader();
};

/************************************************************************/
/*                           OGRCSVDataSource                           */
/************************************************************************/

class OGRCSVDataSource : public OGRDataSource
{
    char                *pszName;

    OGRCSVLayer       **papoLayers;
    int                 nLayers;

    int                 bUpdate;

    CPLString           osDefaultCSVName;

  public:
                        OGRCSVDataSource();
                        ~OGRCSVDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate, int bForceAccept );
    int                 OpenTable( const char * pszFilename,
                                   const char* pszNfdcRunwaysGeomField = NULL,
                                   const char* pszGeonamesGeomFieldPrefix = NULL);
    
    const char          *GetName() { return pszName; }

    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer   *CreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );

    virtual OGRErr      DeleteLayer(int);

    int                 TestCapability( const char * );

    void                SetDefaultCSVName( const char *pszName ) 
        { osDefaultCSVName = pszName; }

    static CPLString    GetRealExtension(CPLString osFilename);
};

/************************************************************************/
/*                             OGRCSVDriver                             */
/************************************************************************/

class OGRCSVDriver : public OGRSFDriver
{
  public:
                ~OGRCSVDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
    OGRDataSource *CreateDataSource( const char *, char ** );
    int         TestCapability( const char * );

    virtual OGRErr      DeleteDataSource( const char *pszName );
    
};


#endif /* ndef _OGR_CSV_H_INCLUDED */
