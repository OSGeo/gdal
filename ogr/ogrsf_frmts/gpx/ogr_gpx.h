/******************************************************************************
 * $Id$
 *
 * Project:  GPX Translator
 * Purpose:  Definition of classes for OGR .gpx driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
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

#ifndef _OGR_GPX_H_INCLUDED
#define _OGR_GPX_H_INCLUDED

#include "ogrsf_frmts.h"

#ifdef HAVE_EXPAT
#include "ogr_expat.h"
#endif

class OGRGPXDataSource;


typedef enum
{
    GPX_NONE,
    GPX_WPT,
    GPX_TRACK,
    GPX_ROUTE,
    GPX_ROUTE_POINT,
    GPX_TRACK_POINT,
} GPXGeometryType;

/************************************************************************/
/*                             OGRGPXLayer                              */
/************************************************************************/

class OGRGPXLayer : public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;
    OGRGPXDataSource*  poDS;
    
    GPXGeometryType    gpxGeomType;

    int                nGPXFields;

    int                bWriteMode;
    int                nFeatures;
    int                eof;
    int                nNextFID;
    VSILFILE*          fpGPX; /* Large file API */
    const char*        pszElementToScan;
#ifdef HAVE_EXPAT
    XML_Parser         oParser;
    XML_Parser         oSchemaParser;
#endif
    int                inInterestingElement;
    int                hasFoundLat;
    int                hasFoundLon;
    double             latVal;
    double             lonVal;
    char*              pszSubElementName;
    char*              pszSubElementValue;
    int                nSubElementValueLen;
    int                iCurrentField;

    OGRFeature*        poFeature;
    OGRFeature **      ppoFeatureTab;
    int                nFeatureTabLength;
    int                nFeatureTabIndex;
    
    OGRMultiLineString* multiLineString;
    OGRLineString*      lineString;
    
    int                depthLevel;
    int                interestingDepthLevel;
    
    OGRFieldDefn*      currentFieldDefn;
    int                inExtensions;
    int                extensionsDepthLevel;
    
    int                inLink;
    int                iCountLink;
    int                nMaxLinks;
    
    int                bEleAs25D;
    
    int                trkFID;
    int                trkSegId;
    int                trkSegPtId;
    
    int                rteFID;
    int                rtePtId;
    
    int                bStopParsing;
    int                nWithoutEventCounter;
    int                nDataHandlerCounter;
    
    int                iFirstGPXField;
    
  private:
    void               WriteFeatureAttributes( OGRFeature *poFeature, int nIdentLevel = 1 );
    void               LoadExtensionsSchema();
#ifdef HAVE_EXPAT
    void               AddStrToSubElementValue(const char* pszStr);
#endif
    int                OGRGPX_WriteXMLExtension(const char* pszTagName,
                                                const char* pszContent);

  public:
                        OGRGPXLayer(const char *pszFilename,
                                    const char* layerName,
                                    GPXGeometryType gpxGeomType,
                                    OGRGPXDataSource* poDS,
                                    int bWriteMode = FALSE);
                        ~OGRGPXLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    
    OGRErr              CreateFeature( OGRFeature *poFeature );
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK );

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }
    
    int                 TestCapability( const char * );
    
    OGRSpatialReference *GetSpatialRef();
    
#ifdef HAVE_EXPAT
    void                startElementCbk(const char *pszName, const char **ppszAttr);
    void                endElementCbk(const char *pszName);
    void                dataHandlerCbk(const char *data, int nLen);
    
    void                startElementLoadSchemaCbk(const char *pszName, const char **ppszAttr);
    void                endElementLoadSchemaCbk(const char *pszName);
    void                dataHandlerLoadSchemaCbk(const char *data, int nLen);
#endif

    static OGRErr       CheckAndFixCoordinatesValidity( double* pdfLatitude, double* pdfLongitude );
};

/************************************************************************/
/*                           OGRGPXDataSource                           */
/************************************************************************/

typedef enum
{
    GPX_VALIDITY_UNKNOWN,
    GPX_VALIDITY_INVALID,
    GPX_VALIDITY_VALID
} OGRGPXValidity;

class OGRGPXDataSource : public OGRDataSource
{
    char*               pszName;

    OGRGPXLayer**       papoLayers;
    int                 nLayers;

    /*  Export related */
    VSILFILE           *fpOutput; /* Large file API */
    int                 bIsBackSeekable;
    const char         *pszEOL;
    int                 nOffsetBounds;
    double              dfMinLat, dfMinLon, dfMaxLat, dfMaxLon;
    
    GPXGeometryType     lastGPXGeomTypeWritten;
    
    int                 bUseExtensions;
    char*               pszExtensionsNS;
    
    OGRGPXValidity      validity;
    int                 nElementsRead;
    char*               pszVersion;
#ifdef HAVE_EXPAT
    XML_Parser          oCurrentParser;
    int                 nDataHandlerCounter;
#endif

  public:
                        OGRGPXDataSource();
                        ~OGRGPXDataSource();

    int                nLastRteId;
    int                nLastTrkId;
    int                nLastTrkSegId;

    int                 Open( const char * pszFilename,
                              int bUpdate );
    
    int                 Create( const char *pszFilename, 
                              char **papszOptions );
    
    const char*         GetName() { return pszName; }

    int                 GetLayerCount() { return nLayers; }
    OGRLayer*           GetLayer( int );
    
    OGRLayer *          CreateLayer( const char * pszLayerName,
                                    OGRSpatialReference *poSRS,
                                    OGRwkbGeometryType eType,
                                    char ** papszOptions );

    int                 TestCapability( const char * );
    
    VSILFILE *              GetOutputFP() { return fpOutput; }
    void                SetLastGPXGeomTypeWritten(GPXGeometryType gpxGeomType)
                            { lastGPXGeomTypeWritten = gpxGeomType; }
    GPXGeometryType     GetLastGPXGeomTypeWritten() { return lastGPXGeomTypeWritten; }
    
    int                 GetUseExtensions() { return bUseExtensions; }
    const char*         GetExtensionsNS() { return pszExtensionsNS; }
    
#ifdef HAVE_EXPAT
    void                startElementValidateCbk(const char *pszName, const char **ppszAttr);
    void                dataHandlerValidateCbk(const char *data, int nLen);
#endif

    const char*         GetVersion() { return pszVersion; }
    
    void                AddCoord(double dfLon, double dfLat);
    
    void                PrintLine(const char *fmt, ...) CPL_PRINT_FUNC_FORMAT (2, 3);
};

/************************************************************************/
/*                             OGRGPXDriver                             */
/************************************************************************/

class OGRGPXDriver : public OGRSFDriver
{
  public:
                ~OGRGPXDriver();

    const char*         GetName();
    OGRDataSource*      Open( const char *, int );
    OGRDataSource*      CreateDataSource( const char * pszName, char **papszOptions );
    int                 DeleteDataSource( const char *pszFilename );
    int                 TestCapability( const char * );
    
};


#endif /* ndef _OGR_GPX_H_INCLUDED */
