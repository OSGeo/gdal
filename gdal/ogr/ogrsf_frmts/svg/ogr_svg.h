/******************************************************************************
 * $Id$
 *
 * Project:  SVG Translator
 * Purpose:  Definition of classes for OGR .svg driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_SVG_H_INCLUDED
#define OGR_SVG_H_INCLUDED

#include "ogrsf_frmts.h"

#ifdef HAVE_EXPAT
#include "ogr_expat.h"
#endif

class OGRSVGDataSource;

typedef enum
{
    SVG_POINTS,
    SVG_LINES,
    SVG_POLYGONS,
} SVGGeometryType;

/************************************************************************/
/*                             OGRSVGLayer                              */
/************************************************************************/

class OGRSVGLayer final: public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;
#ifdef HAVE_EXPAT
    OGRSVGDataSource*  poDS;
#endif
    CPLString          osLayerName;

    SVGGeometryType    svgGeomType;

    int                nTotalFeatures;
    int                nNextFID;
    VSILFILE*          fpSVG;  // Large file API.

#ifdef HAVE_EXPAT
    XML_Parser         oParser;
    XML_Parser         oSchemaParser;
#endif
    char*              pszSubElementValue;
    int                nSubElementValueLen;
    int                iCurrentField;

    OGRFeature*        poFeature;
    OGRFeature **      ppoFeatureTab;
    int                nFeatureTabLength;
    int                nFeatureTabIndex;

    int                depthLevel;
    int                interestingDepthLevel;
    bool               inInterestingElement;

    bool               bStopParsing;
#ifdef HAVE_EXPAT
    int                nWithoutEventCounter;
    int                nDataHandlerCounter;

    OGRSVGLayer       *poCurLayer;
#endif

  private:
    void               LoadSchema();

  public:
                        OGRSVGLayer(const char *pszFilename,
                                    const char* layerName,
                                    SVGGeometryType svgGeomType,
                                    OGRSVGDataSource* poDS);
                        virtual ~OGRSVGLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature *        GetNextFeature() override;

    virtual const char*         GetName() override { return osLayerName.c_str(); }
    virtual OGRwkbGeometryType  GetGeomType() override;

    virtual GIntBig             GetFeatureCount( int bForce = TRUE ) override;

    virtual OGRFeatureDefn *    GetLayerDefn() override;

    virtual int                 TestCapability( const char * ) override;

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
/*                           OGRSVGDataSource                           */
/************************************************************************/

typedef enum
{
    SVG_VALIDITY_UNKNOWN,
    SVG_VALIDITY_INVALID,
    SVG_VALIDITY_VALID
} OGRSVGValidity;

class OGRSVGDataSource final: public OGRDataSource
{
    char*               pszName;

    OGRSVGLayer**       papoLayers;
    int                 nLayers;

#ifdef HAVE_EXPAT
    OGRSVGValidity      eValidity;
    int                 bIsCloudmade;
    XML_Parser          oCurrentParser;
    int                 nDataHandlerCounter;
#endif

  public:
                        OGRSVGDataSource();
                        virtual ~OGRSVGDataSource();

    int                 Open( const char * pszFilename );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return nLayers; }
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override;

#ifdef HAVE_EXPAT
    void                startElementValidateCbk(const char *pszName, const char **ppszAttr);
    void                dataHandlerValidateCbk(const char *data, int nLen);
#endif
};

#endif /* ndef OGR_SVG_H_INCLUDED */
