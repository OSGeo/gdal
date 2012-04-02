/******************************************************************************
 * $Id$
 *
 * Project:  PDF Translator
 * Purpose:  Definition of classes for OGR .pdf driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#ifndef _OGR_PDF_H_INCLUDED
#define _OGR_PDF_H_INCLUDED

#include "ogrsf_frmts.h"

#include "ogr_mem.h"
#include "gdal_priv.h"
#include "pdfobject.h"

#include <map>
#include <stack>

/************************************************************************/
/*                             OGRPDFLayer                              */
/************************************************************************/

class OGRPDFDataSource;

class OGRPDFLayer : public OGRMemLayer
{
    OGRPDFDataSource* poDS;
    public:
        OGRPDFLayer(OGRPDFDataSource* poDS,
                    const char * pszName,
                    OGRSpatialReference *poSRS,
                    OGRwkbGeometryType eGeomType);

    void                Fill( GDALPDFArray* poArray );

    virtual int                 TestCapability( const char * );
    virtual OGRErr              CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                           OGRPDFDataSource                           */
/************************************************************************/

class OGRPDFDataSource : public OGRDataSource
{
    char*               pszName;
    char**              papszOptions;

    int                 nLayers;
    OGRLayer          **papoLayers;

    int                 bWritable;
    int                 bModified;

    GDALDataset        *poGDAL_DS;
    GDALPDFObject      *poPageObj;
    GDALPDFObject      *poCatalogObj;
    int                 nXSize;
    int                 nYSize;
    double              adfGeoTransform[6];
    double              dfPageWidth;
    double              dfPageHeight;
    void                PDFCoordsToSRSCoords(double x, double y,
                                             double& X, double &Y);

    std::map<int,OGRGeometry*> oMapMCID;
    void                CleanupIntermediateResources();

    std::map<CPLString, int> oMapOperators;
    void                InitMapOperators();

    void                ExploreTree(GDALPDFObject* poObj);
    void                ExploreContents(GDALPDFObject* poObj, GDALPDFObject* poResources);
    int                 UnstackTokens(const CPLString& osToken,
                                      std::stack<CPLString>& osTokenStack,
                                      double* adfCoords);
    void                ParseContent(const char* pszContent,
                                     int nMCID,
                                     GDALPDFObject* poResources,
                                     int bInitBDCStack);

  public:
                        OGRPDFDataSource();
                        ~OGRPDFDataSource();

    int                 Open( const char * pszName );
    int                 Create( const char * pszName, char **papszOptions );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount();
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );

    virtual OGRLayer* CreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                char ** papszOptions );

    virtual OGRErr      SyncToDisk();

    void                SetModified() { bModified = TRUE; }
    OGRGeometry        *GetGeometryFromMCID(int nMCID);
};

/************************************************************************/
/*                             OGRPDFDriver                              */
/************************************************************************/

class OGRPDFDriver : public OGRSFDriver
{
  public:
                ~OGRPDFDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    virtual OGRErr DeleteDataSource( const char *pszName );
};


#endif /* ndef _OGR_PDF_H_INCLUDED */
