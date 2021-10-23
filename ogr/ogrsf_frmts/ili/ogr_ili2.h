/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 2 Translator
 * Purpose:   Definition of classes for OGR Interlis 2 driver.
 * Author:   Markus Schnider, Sourcepole AG
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

#ifndef OGR_ILI2_H_INCLUDED
#define OGR_ILI2_H_INCLUDED

#include "ogrsf_frmts.h"
#include "imdreader.h"
#include "ili2reader.h"

#include <string>
#include <list>

class OGRILI2DataSource;

/************************************************************************/
/*                           OGRILI2Layer                               */
/************************************************************************/

class OGRILI2Layer final: public OGRLayer
{
  private:
    OGRFeatureDefn     *poFeatureDefn;
    GeomFieldInfos      oGeomFieldInfos;
    std::list<OGRFeature *>    listFeature;
    std::list<OGRFeature *>::const_iterator listFeatureIt;

    OGRILI2DataSource  *poDS;

  public:
                        OGRILI2Layer( OGRFeatureDefn* poFeatureDefn,
                                      const GeomFieldInfos& oGeomFieldInfos,
                                      OGRILI2DataSource *poDS );

                       ~OGRILI2Layer();

    void                AddFeature(OGRFeature *poFeature);

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    GIntBig             GetFeatureCount( int bForce = TRUE ) override;

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    CPLString           GetIliGeomType( const char* cFieldName) { return oGeomFieldInfos[cFieldName].iliGeomType; }

    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE ) override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                          OGRILI2DataSource                           */
/************************************************************************/

class OGRILI2DataSource final: public OGRDataSource
{
  private:
    std::list<OGRLayer *> listLayer;

    char        *pszName;
    ImdReader   *poImdReader;
    IILI2Reader *poReader;
    VSILFILE    *fpOutput;

    int         nLayers;
    OGRILI2Layer** papoLayers;

    CPL_DISALLOW_COPY_ASSIGN(OGRILI2DataSource)

  public:
                OGRILI2DataSource();
               virtual ~OGRILI2DataSource();

    int         Open( const char *, char** papszOpenOptions, int bTestOpen );
    int         Create( const char *pszFile, char **papszOptions );

    const char *GetName() override { return pszName; }
    int         GetLayerCount() override { return static_cast<int>(listLayer.size()); }
    OGRLayer   *GetLayer( int ) override;

    virtual OGRLayer *ICreateLayer( const char *,
                                      OGRSpatialReference * = nullptr,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = nullptr ) override;

    VSILFILE *  GetOutputFP() { return fpOutput; }
    int         TestCapability( const char * ) override;
};

#endif /* OGR_ILI2_H_INCLUDED */
