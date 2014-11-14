/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1 Translator
 * Purpose:   Definition of classes for OGR Interlis 1 driver.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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

#ifndef _OGR_ILI1_H_INCLUDED
#define _OGR_ILI1_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ili1reader.h"



class OGRILI1DataSource;

/************************************************************************/
/*                           OGRILI1Layer                               */
/************************************************************************/

class OGRILI1Layer : public OGRLayer
{
private:
    OGRSpatialReference *poSRS;
    OGRFeatureDefn      *poFeatureDefn;
    GeomFieldInfos      oGeomFieldInfos;

    int                 nFeatures;
    OGRFeature          **papoFeatures;
    int                 nFeatureIdx;

    int                 bWriter;

    int                 bGeomsJoined;

    OGRILI1DataSource   *poDS;

  public:
                        OGRILI1Layer( OGRFeatureDefn* poFeatureDefn,
                                      GeomFieldInfos oGeomFieldInfos,
                                      OGRILI1DataSource *poDS );

                       ~OGRILI1Layer();

    OGRErr              AddFeature(OGRFeature *poFeature);

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    OGRFeature *        GetNextFeatureRef();
    OGRFeature *        GetFeatureRef( long nFID );

    int                 GetFeatureCount( int bForce = TRUE );

    OGRErr              ICreateFeature( OGRFeature *poFeature );
    int                 GeometryAppend( OGRGeometry *poGeometry );

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE );

    int                 TestCapability( const char * );

  private:
    void                JoinGeomLayers();
    void                JoinSurfaceLayer( OGRILI1Layer* poSurfacePolyLayer, int nSurfaceFieldIndex );
    OGRMultiPolygon*    Polygonize( OGRGeometryCollection* poLines, bool fix_crossing_lines = false );
    void                PolygonizeAreaLayer( OGRILI1Layer* poAreaLineLayer, int nAreaFieldIndex, int nPointFieldIndex );
};

/************************************************************************/
/*                          OGRILI1DataSource                           */
/************************************************************************/

class OGRILI1DataSource : public OGRDataSource
{
  private:
    char       *pszName;
    ImdReader  *poImdReader;
    IILI1Reader *poReader;
    FILE       *fpTransfer;
    char       *pszTopic;
    int         nLayers;
    OGRILI1Layer** papoLayers;

  public:
                OGRILI1DataSource();
               ~OGRILI1DataSource();

    int         Open( const char *, int bTestOpen );
    int         Create( const char *pszFile, char **papszOptions );

    const char *GetName() { return pszName; }
    int         GetLayerCount() { return poReader ? poReader->GetLayerCount() : 0; }
    OGRLayer   *GetLayer( int );
    OGRILI1Layer *GetLayerByName( const char* );

    FILE       *GetTransferFile() { return fpTransfer; }

    virtual OGRLayer *ICreateLayer( const char *,
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    int         TestCapability( const char * );
};

#endif /* _OGR_ILI1_H_INCLUDED */
