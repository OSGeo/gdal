/******************************************************************************
 * $Id $
 *
 * Project:  GeoRSS Translator
 * Purpose:  Definition of classes for OGR GeoRSS driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGR_GEORSS_H_INCLUDED
#define _OGR_GEORSS_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "cpl_hash_set.h"

#ifdef HAVE_EXPAT
#include "ogr_expat.h"
#endif

class OGRGeoRSSDataSource;

typedef enum
{
    GEORSS_ATOM,
    GEORSS_RSS,
    GEORSS_RSS_RDF,
} OGRGeoRSSFormat;

typedef enum
{
    GEORSS_GML,
    GEORSS_SIMPLE,
    GEORSS_W3C_GEO
} OGRGeoRSSGeomDialect;

/************************************************************************/
/*                             OGRGeoRSSLayer                              */
/************************************************************************/

class OGRGeoRSSLayer : public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;
    OGRGeoRSSDataSource*  poDS;
    OGRGeoRSSFormat     eFormat;

    int                bWriteMode;
    int                nTotalFeatureCount;

    int                eof;
    int                nNextFID;
    VSILFILE*          fpGeoRSS; /* Large file API */
    int                bHasReadSchema;
#ifdef HAVE_EXPAT
    XML_Parser         oParser;
    XML_Parser         oSchemaParser;
#endif
    OGRGeometry*       poGlobalGeom;
    int                bStopParsing;
    int                bInFeature;
    int                hasFoundLat;
    int                hasFoundLon;
    double             latVal;
    double             lonVal;
    char*              pszSubElementName;
    char*              pszSubElementValue;
    int                nSubElementValueLen;
    int                iCurrentField;
    int                bInSimpleGeometry;
    int                bInGMLGeometry;
    int                bInGeoLat;
    int                bInGeoLong;
    int                bFoundGeom;
    OGRwkbGeometryType eGeomType;
    int                bSameSRS;
    char*              pszGMLSRSName;
    int                bInTagWithSubTag;
    char*              pszTagWithSubTag;
    int                currentDepth;
    int                featureDepth;
    int                geometryDepth;
    OGRFieldDefn*      currentFieldDefn;
    int                nWithoutEventCounter;
    CPLHashSet*        setOfFoundFields;
    int                nDataHandlerCounter;

    OGRFeature*        poFeature;
    OGRFeature **      ppoFeatureTab;
    int                nFeatureTabLength;
    int                nFeatureTabIndex;

  private:
#ifdef HAVE_EXPAT
    void               AddStrToSubElementValue(const char* pszStr);
#endif
    int                IsStandardField(const char* pszName);
    
  public:
                        OGRGeoRSSLayer(const char *pszFilename,
                                    const char* layerName,
                                    OGRGeoRSSDataSource* poDS,
                                    OGRSpatialReference *poSRSIn,
                                    int bWriteMode = FALSE);
                        ~OGRGeoRSSLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    
    OGRErr              CreateFeature( OGRFeature *poFeature );
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK );

    OGRFeatureDefn *    GetLayerDefn();
    
    int                 TestCapability( const char * );
    
    int                 GetFeatureCount( int bForce );

    void                LoadSchema();

#ifdef HAVE_EXPAT
    void                startElementCbk(const char *pszName, const char **ppszAttr);
    void                endElementCbk(const char *pszName);
    void                dataHandlerCbk(const char *data, int nLen);

    void                startElementLoadSchemaCbk(const char *pszName, const char **ppszAttr);
    void                endElementLoadSchemaCbk(const char *pszName);
    void                dataHandlerLoadSchemaCbk(const char *data, int nLen);
#endif
};

/************************************************************************/
/*                           OGRGeoRSSDataSource                           */
/************************************************************************/

typedef enum
{
    GEORSS_VALIDITY_UNKNOWN,
    GEORSS_VALIDITY_INVALID,
    GEORSS_VALIDITY_VALID
} OGRGeoRSSValidity;

class OGRGeoRSSDataSource : public OGRDataSource
{
    char*               pszName;

    OGRGeoRSSLayer**    papoLayers;
    int                 nLayers;

    /*  Export related */
    VSILFILE           *fpOutput; /* Virtual file API */
    
    OGRGeoRSSValidity   validity;
    OGRGeoRSSFormat     eFormat;
    OGRGeoRSSGeomDialect eGeomDialect;
    int                 bUseExtensions;
    int                 bWriteHeaderAndFooter;
#ifdef HAVE_EXPAT
    XML_Parser          oCurrentParser;
    int                 nDataHandlerCounter;
#endif

  public:
                        OGRGeoRSSDataSource();
                        ~OGRGeoRSSDataSource();

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
    
    VSILFILE *          GetOutputFP() { return fpOutput; }
    OGRGeoRSSFormat     GetFormat() { return eFormat; }
    OGRGeoRSSGeomDialect GetGeomDialect() { return eGeomDialect; }
    int                 GetUseExtensions() { return bUseExtensions; }
    
#ifdef HAVE_EXPAT
    void                startElementValidateCbk(const char *pszName, const char **ppszAttr);
    void                dataHandlerValidateCbk(const char *data, int nLen);
#endif
};

/************************************************************************/
/*                             OGRGeoRSSDriver                             */
/************************************************************************/

class OGRGeoRSSDriver : public OGRSFDriver
{
  public:
                ~OGRGeoRSSDriver();

    const char*         GetName();
    OGRDataSource*      Open( const char *, int );
    OGRDataSource*      CreateDataSource( const char * pszName, char **papszOptions );
    int                 DeleteDataSource( const char *pszFilename );
    int                 TestCapability( const char * );
    
};


#endif /* ndef _OGR_GeoRSS_H_INCLUDED */
