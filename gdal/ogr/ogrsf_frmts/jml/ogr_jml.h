/******************************************************************************
 * $Id$
 *
 * Project:  JML .jml Translator
 * Purpose:  Definition of classes for OGR JML driver.
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef _OGR_JML_H_INCLUDED
#define _OGR_JML_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_p.h"

#ifdef HAVE_EXPAT
#include "ogr_expat.h"
#endif

#include <vector>

class OGRJMLDataset;

#ifdef HAVE_EXPAT

/************************************************************************/
/*                            OGRJMLColumn                              */
/************************************************************************/

class OGRJMLColumn
{
    public:
        CPLString osName;
        CPLString osType;
        CPLString osElementName;
        CPLString osAttributeName;
        CPLString osAttributeValue;
        int       bIsBody; /* if false: attribute */
};

/************************************************************************/
/*                             OGRJMLLayer                              */
/************************************************************************/

class OGRJMLLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRJMLDataset *poDS;

    int                nNextFID;
    VSILFILE*          fp;
    int                bHasReadSchema;

    XML_Parser         oParser;

    int                currentDepth;
    int                bStopParsing;
    int                nWithoutEventCounter;
    int                nDataHandlerCounter;
    
    int                bAccumulateElementValue;
    char              *pszElementValue;
    int                nElementValueLen;
    int                nElementValueAlloc;

    OGRFeature*        poFeature;
    OGRFeature **      ppoFeatureTab;
    int                nFeatureTabLength;
    int                nFeatureTabIndex;

    int                bSchemaFinished;
    int                nJCSGMLInputTemplateDepth;
    int                nCollectionElementDepth;
    CPLString          osCollectionElement;
    int                nFeatureElementDepth;
    CPLString          osFeatureElement;
    int                nGeometryElementDepth;
    CPLString          osGeometryElement;
    int                nColumnDepth;
    int                nNameDepth;
    int                nTypeDepth;
    int                nAttributeElementDepth;
    int                iAttr;
    int                iRGBField;

    OGRJMLColumn  oCurColumn;
    std::vector<OGRJMLColumn> aoColumns;

    void                AddStringToElementValue(const char *data, int nLen);
    void                StopAccumulate();

    void                LoadSchema();

  public:
                        OGRJMLLayer(const char *pszLayerName,
                                         OGRJMLDataset* poDS,
                                         VSILFILE* fp );
                        ~OGRJMLLayer();

    const char         *GetName() { return poFeatureDefn->GetName(); }

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn();
    
    int                 TestCapability( const char * );

    void                startElementCbk(const char *pszName, const char **ppszAttr);
    void                endElementCbk(const char *pszName);
    void                dataHandlerCbk(const char *data, int nLen);

    void                startElementLoadSchemaCbk(const char *pszName, const char **ppszAttr);
    void                endElementLoadSchemaCbk(const char *pszName);
};

#endif /* HAVE_EXPAT */

/************************************************************************/
/*                          OGRJMLWriterLayer                           */
/************************************************************************/

class OGRJMLWriterLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRJMLDataset *poDS;
    VSILFILE           *fp;
    int                 bFeaturesWritten;
    int                 bAddRGBField;
    int                 bAddOGRStyleField;
    int                 bClassicGML;
    int                 nNextFID;

    void                WriteColumnDeclaration( const char* pszName,
                                                const char* pszType );

  public:
                        OGRJMLWriterLayer(const char* pszLayerName,
                                               OGRJMLDataset* poDS,
                                               VSILFILE* fp,
                                               int bAddRGBField,
                                               int bAddOGRStyleField,
                                               int bClassicGML );
                        ~OGRJMLWriterLayer();

    void                ResetReading() {}
    OGRFeature *        GetNextFeature() { return NULL; }

    OGRErr              CreateFeature( OGRFeature *poFeature );
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK );

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                            OGRJMLDataset                             */
/************************************************************************/

class OGRJMLDataset : public GDALDataset
{
    OGRLayer           *poLayer;

    VSILFILE           *fp; /* Virtual file API */
    int                 bWriteMode;

  public:
                        OGRJMLDataset();
                        ~OGRJMLDataset();

    int                 GetLayerCount() { return poLayer != NULL ? 1 : 0; }
    OGRLayer*           GetLayer( int );
    
    OGRLayer *          ICreateLayer( const char * pszLayerName,
                                    OGRSpatialReference *poSRS,
                                    OGRwkbGeometryType eType,
                                    char ** papszOptions );

    int                 TestCapability( const char * );

    static int          Identify( GDALOpenInfo* poOpenInfo );
    static GDALDataset* Open( GDALOpenInfo* poOpenInfo );
    static GDALDataset* Create( const char *pszFilename, 
                                 int nBands,
                                 int nXSize,
                                 int nYSize,
                                 GDALDataType eDT,
                                 char **papszOptions );
};

#endif /* ndef _OGR_JML_H_INCLUDED */
