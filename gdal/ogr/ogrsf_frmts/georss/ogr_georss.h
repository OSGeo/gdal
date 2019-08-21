/******************************************************************************
 * $Id $
 *
 * Project:  GeoRSS Translator
 * Purpose:  Definition of classes for OGR GeoRSS driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_GEORSS_H_INCLUDED
#define OGR_GEORSS_H_INCLUDED

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

class OGRGeoRSSLayer final: public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;
    OGRGeoRSSDataSource*  poDS;
    OGRGeoRSSFormat     eFormat;

    bool               bWriteMode;
    int                nTotalFeatureCount;

    // TODO(schwehr): Remove eof?
    bool               eof;
    int                nNextFID;
    VSILFILE*          fpGeoRSS; /* Large file API */
    bool               bHasReadSchema;
#ifdef HAVE_EXPAT
    XML_Parser         oParser;
    XML_Parser         oSchemaParser;
#endif
    OGRGeometry*       poGlobalGeom;
    bool               bStopParsing;
    bool               bInFeature;
    bool               hasFoundLat;
    bool               hasFoundLon;
#ifdef HAVE_EXPAT
    double             latVal;
    double             lonVal;
#endif
    char*              pszSubElementName;
    char*              pszSubElementValue;
    int                nSubElementValueLen;
#ifdef HAVE_EXPAT
    int                iCurrentField;
#endif
    bool               bInSimpleGeometry;
    bool               bInGMLGeometry;
    bool               bInGeoLat;
    bool               bInGeoLong;
#ifdef HAVE_EXPAT
    bool               bFoundGeom;
    bool               bSameSRS;
#endif
    OGRwkbGeometryType eGeomType;
    char*              pszGMLSRSName;
    bool               bInTagWithSubTag;
    char*              pszTagWithSubTag;
    int                currentDepth;
    int                featureDepth;
    int                geometryDepth;
#ifdef HAVE_EXPAT
    OGRFieldDefn*      currentFieldDefn;
    int                nWithoutEventCounter;
    int                nDataHandlerCounter;
#endif
    CPLHashSet*        setOfFoundFields;

    OGRFeature*        poFeature;
    OGRFeature **      ppoFeatureTab;
    int                nFeatureTabLength;
    int                nFeatureTabIndex;

  private:
#ifdef HAVE_EXPAT
    void               AddStrToSubElementValue( const char* pszStr );
#endif
    bool               IsStandardField( const char* pszName );

  public:
                        OGRGeoRSSLayer( const char *pszFilename,
                                        const char* layerName,
                                        OGRGeoRSSDataSource* poDS,
                                        OGRSpatialReference *poSRSIn,
                                        bool bWriteMode = false );
                        ~OGRGeoRSSLayer() override;

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK ) override;

    OGRFeatureDefn *    GetLayerDefn() override;

    int                 TestCapability( const char * ) override;

    GIntBig             GetFeatureCount( int bForce ) override;

    void                LoadSchema();

#ifdef HAVE_EXPAT
    void                startElementCbk( const char *pszName, const char **ppszAttr );
    void                endElementCbk( const char *pszName );
    void                dataHandlerCbk( const char *data, int nLen );

    void                startElementLoadSchemaCbk( const char *pszName, const char **ppszAttr );
    void                endElementLoadSchemaCbk( const char *pszName );
    void                dataHandlerLoadSchemaCbk( const char *data, int nLen );
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

class OGRGeoRSSDataSource final: public OGRDataSource
{
    char*               pszName;

    OGRGeoRSSLayer**    papoLayers;
    int                 nLayers;

    /*  Export related */
    VSILFILE           *fpOutput; /* Virtual file API */

#ifdef HAVE_EXPAT
    OGRGeoRSSValidity   validity;
#endif
    OGRGeoRSSFormat     eFormat;
    OGRGeoRSSGeomDialect eGeomDialect;
    bool                bUseExtensions;
    bool                bWriteHeaderAndFooter;
#ifdef HAVE_EXPAT
    XML_Parser          oCurrentParser;
    int                 nDataHandlerCounter;
#endif

  public:
                        OGRGeoRSSDataSource();
                        ~OGRGeoRSSDataSource() override;

    int                 Open( const char * pszFilename,
                              int bUpdate );

    int                 Create( const char *pszFilename,
                              char **papszOptions );

    const char*         GetName() override { return pszName; }

    int                 GetLayerCount() override { return nLayers; }
    OGRLayer*           GetLayer( int ) override;

    OGRLayer *          ICreateLayer( const char * pszLayerName,
                                      OGRSpatialReference *poSRS,
                                      OGRwkbGeometryType eType,
                                      char ** papszOptions ) override;

    int                 TestCapability( const char * ) override;

    VSILFILE *          GetOutputFP() { return fpOutput; }
    OGRGeoRSSFormat     GetFormat() { return eFormat; }
    OGRGeoRSSGeomDialect GetGeomDialect() { return eGeomDialect; }
    bool                GetUseExtensions() { return bUseExtensions; }

#ifdef HAVE_EXPAT
    void                startElementValidateCbk(const char *pszName, const char **ppszAttr);
    void                dataHandlerValidateCbk(const char *data, int nLen);
#endif
};

#endif /* ndef _OGR_GeoRSS_H_INCLUDED */
