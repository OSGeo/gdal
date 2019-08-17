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

#ifndef OGR_JML_H_INCLUDED
#define OGR_JML_H_INCLUDED

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
        bool      bIsBody; /* if false: attribute */
        OGRJMLColumn() : bIsBody(false) {}
};

/************************************************************************/
/*                             OGRJMLLayer                              */
/************************************************************************/

class OGRJMLLayer final: public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    int                nNextFID;
    VSILFILE*          fp;
    bool               bHasReadSchema;

    XML_Parser         oParser;

    int                currentDepth;
    bool               bStopParsing;
    int                nWithoutEventCounter;
    int                nDataHandlerCounter;

    bool               bAccumulateElementValue;
    char              *pszElementValue;
    int                nElementValueLen;
    int                nElementValueAlloc;

    OGRFeature*        poFeature;
    OGRFeature **      ppoFeatureTab;
    int                nFeatureTabLength;
    int                nFeatureTabIndex;

    bool               bSchemaFinished;
    int                nJCSGMLInputTemplateDepth;
    int                nCollectionElementDepth;
    int                nFeatureCollectionDepth;
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
    CPLString          osSRSName;

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

    const char         *GetName() override { return poFeatureDefn->GetName(); }

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRFeatureDefn *    GetLayerDefn() override;

    int                 TestCapability( const char * ) override;

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

class OGRJMLWriterLayer final: public OGRLayer
{
    OGRJMLDataset      *poDS;
    OGRFeatureDefn     *poFeatureDefn;
    VSILFILE           *fp;
    bool                bFeaturesWritten;
    bool                bAddRGBField;
    bool                bAddOGRStyleField;
    bool                bClassicGML;
    int                 nNextFID;
    CPLString           osSRSAttr;
    OGREnvelope         sLayerExtent;
    vsi_l_offset        nBBoxOffset;

    void                WriteColumnDeclaration( const char* pszName,
                                                const char* pszType );

  public:
                        OGRJMLWriterLayer( const char* pszLayerName,
                                           OGRSpatialReference * poSRS,
                                           OGRJMLDataset* poDSIn,
                                           VSILFILE* fp,
                                           bool bAddRGBField,
                                           bool bAddOGRStyleField,
                                           bool bClassicGML );
                        ~OGRJMLWriterLayer();

    void                ResetReading() override {}
    OGRFeature *        GetNextFeature() override { return nullptr; }

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                            OGRJMLDataset                             */
/************************************************************************/

class OGRJMLDataset final: public GDALDataset
{
    OGRLayer           *poLayer;

    VSILFILE           *fp; /* Virtual file API */
    bool                bWriteMode;

  public:
                        OGRJMLDataset();
                        ~OGRJMLDataset();

    int                 GetLayerCount() override { return poLayer != nullptr ? 1 : 0; }
    OGRLayer*           GetLayer( int ) override;

    OGRLayer *          ICreateLayer( const char * pszLayerName,
                                    OGRSpatialReference *poSRS,
                                    OGRwkbGeometryType eType,
                                    char ** papszOptions ) override;

    int                 TestCapability( const char * ) override;

    static int          Identify( GDALOpenInfo* poOpenInfo );
    static GDALDataset* Open( GDALOpenInfo* poOpenInfo );
    static GDALDataset* Create( const char *pszFilename,
                                 int nBands,
                                 int nXSize,
                                 int nYSize,
                                 GDALDataType eDT,
                                 char **papszOptions );
};

#endif /* ndef OGR_JML_H_INCLUDED */
