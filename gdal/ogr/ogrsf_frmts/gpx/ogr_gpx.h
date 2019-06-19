/******************************************************************************
 * $Id$
 *
 * Project:  GPX Translator
 * Purpose:  Definition of classes for OGR .gpx driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_GPX_H_INCLUDED
#define OGR_GPX_H_INCLUDED

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

class OGRGPXLayer final: public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;
    OGRGPXDataSource*  poDS;

    GPXGeometryType    gpxGeomType;

    int                nGPXFields;

    bool               bWriteMode;
    int                nNextFID;
    VSILFILE*          fpGPX; /* Large file API */
#ifdef HAVE_EXPAT
    XML_Parser         oParser;
    XML_Parser         oSchemaParser;
#endif
    bool               inInterestingElement;
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

    OGRFeature*        poFeature;
    OGRFeature **      ppoFeatureTab;
    int                nFeatureTabLength;
    int                nFeatureTabIndex;

    OGRMultiLineString* multiLineString;
    OGRLineString*      lineString;

    int                depthLevel;
    int                interestingDepthLevel;

#ifdef HAVE_EXPAT
    OGRFieldDefn*      currentFieldDefn;
    bool               inExtensions;
    int                extensionsDepthLevel;

    bool               inLink;
    int                iCountLink;
#endif
    int                nMaxLinks;

    bool               bEleAs25D;

    int                trkFID;
    int                trkSegId;
    int                trkSegPtId;

    int                rteFID;
    int                rtePtId;

#ifdef HAVE_EXPAT
    bool               bStopParsing;
    int                nWithoutEventCounter;
    int                nDataHandlerCounter;
#endif

    int                iFirstGPXField;

  private:
    void               WriteFeatureAttributes( OGRFeature *poFeature, int nIdentLevel = 1 );
    void               LoadExtensionsSchema();
#ifdef HAVE_EXPAT
    void               AddStrToSubElementValue(const char* pszStr);
#endif
    bool               OGRGPX_WriteXMLExtension(const char* pszTagName,
                                                const char* pszContent);

  public:
                        OGRGPXLayer(const char *pszFilename,
                                    const char* layerName,
                                    GPXGeometryType gpxGeomType,
                                    OGRGPXDataSource* poDS,
                                    int bWriteMode = FALSE);
                        ~OGRGPXLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int                 TestCapability( const char * ) override;

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

class OGRGPXDataSource final: public OGRDataSource
{
    char*               pszName;

    OGRGPXLayer**       papoLayers;
    int                 nLayers;

    /*  Export related */
    VSILFILE           *fpOutput; /* Large file API */
    bool                bIsBackSeekable;
    const char         *pszEOL;
    int                 nOffsetBounds;
    double              dfMinLat;
    double              dfMinLon;
    double              dfMaxLat;
    double              dfMaxLon;

    GPXGeometryType     lastGPXGeomTypeWritten;

    bool                bUseExtensions;
    char*               pszExtensionsNS;

#ifdef HAVE_EXPAT
    OGRGPXValidity      validity;
    int                 nElementsRead;
    char*               pszVersion;
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

    const char*         GetName() override { return pszName; }

    int                 GetLayerCount() override { return nLayers; }
    OGRLayer*           GetLayer( int ) override;

    OGRLayer *          ICreateLayer( const char * pszLayerName,
                                    OGRSpatialReference *poSRS,
                                    OGRwkbGeometryType eType,
                                    char ** papszOptions ) override;

    int                 TestCapability( const char * ) override;

    VSILFILE *              GetOutputFP() { return fpOutput; }
    void                SetLastGPXGeomTypeWritten(GPXGeometryType gpxGeomType)
                            { lastGPXGeomTypeWritten = gpxGeomType; }
    GPXGeometryType     GetLastGPXGeomTypeWritten() { return lastGPXGeomTypeWritten; }

    int                 GetUseExtensions() { return bUseExtensions; }
    const char*         GetExtensionsNS() { return pszExtensionsNS; }

#ifdef HAVE_EXPAT
    void                startElementValidateCbk(const char *pszName, const char **ppszAttr);
    void                dataHandlerValidateCbk(const char *data, int nLen);
    const char*         GetVersion() { return pszVersion; }
#endif

    void                AddCoord(double dfLon, double dfLat);

    void                PrintLine(const char *fmt, ...) CPL_PRINT_FUNC_FORMAT (2, 3);
};

#endif /* ndef OGR_GPX_H_INCLUDED */
