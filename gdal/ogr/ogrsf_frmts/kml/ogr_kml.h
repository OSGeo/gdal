/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Declarations for OGR wrapper classes for KML, and OGR->KML
 *           translation of geometry.
 * Author:   Christopher Condit, condit@sdsc.edu;
 *           Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
 *               2007, Jens Oberender
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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
#ifndef OGR_KML_H_INCLUDED
#define OGR_KML_H_INCLUDED

#include "ogrsf_frmts.h"

#ifdef HAVE_EXPAT
#  include "kmlvector.h"
#endif

class OGRKMLDataSource;

/************************************************************************/
/*                            OGRKMLLayer                               */
/************************************************************************/

class OGRKMLLayer final: public OGRLayer
{
  public:
    OGRKMLLayer( const char* pszName_,
                 OGRSpatialReference* poSRS,
                 bool bWriter,
                 OGRwkbGeometryType eType,
                 OGRKMLDataSource* poDS );
    ~OGRKMLLayer();

    //
    // OGRLayer Interface
    //
    OGRFeatureDefn* GetLayerDefn() override;
    OGRErr ICreateFeature( OGRFeature* poFeature ) override;
    OGRErr CreateField( OGRFieldDefn* poField, int bApproxOK = TRUE ) override;
    void ResetReading() override;
    OGRFeature* GetNextFeature() override;
    GIntBig GetFeatureCount( int bForce = TRUE ) override;
    int TestCapability( const char* pszCap ) override;

    //
    // OGRKMLLayer Interface
    //
    void SetLayerNumber( int nLayer );

    void SetClosedForWriting() { bClosedForWriting = true; }

    CPLString WriteSchema();

  private:
    friend class OGRKMLDataSource;

    OGRKMLDataSource* poDS_;
    OGRSpatialReference* poSRS_;
    OGRCoordinateTransformation *poCT_;

    OGRFeatureDefn* poFeatureDefn_;

    int iNextKMLId_;
    int nTotalKMLCount_;
    bool bWriter_;
    int nLayerNumber_;
    int nWroteFeatureCount_;
    bool bSchemaWritten_;
    bool bClosedForWriting;
    char* pszName_;

    int nLastAsked;
    int nLastCount;
};

/************************************************************************/
/*                           OGRKMLDataSource                           */
/************************************************************************/

class OGRKMLDataSource final: public OGRDataSource
{
  public:
    OGRKMLDataSource();
    ~OGRKMLDataSource();

    //
    // OGRDataSource Interface
    //
    int Open( const char* pszName, int bTestOpen );
    const char* GetName() override { return pszName_; }
    int GetLayerCount() override { return nLayers_; }
    OGRLayer* GetLayer( int nLayer ) override;
    OGRLayer* ICreateLayer( const char* pszName,
                           OGRSpatialReference* poSRS = nullptr,
                           OGRwkbGeometryType eGType = wkbUnknown,
                           char** papszOptions = nullptr ) override;
    int TestCapability( const char* pszCap ) override;

    //
    // OGRKMLDataSource Interface
    //
    int Create( const char* pszName, char** papszOptions );
    const char* GetNameField() const { return pszNameField_; }
    const char* GetDescriptionField() const { return pszDescriptionField_; }
    const char* GetAltitudeMode() { return pszAltitudeMode_; }
    VSILFILE* GetOutputFP() { return fpOutput_; }
    void GrowExtents( OGREnvelope *psGeomBounds );
#ifdef HAVE_EXPAT
    KML* GetKMLFile() { return poKMLFile_; }
#endif

    bool IsFirstCTError() const { return !bIssuedCTError_; }
    void IssuedFirstCTError() { bIssuedCTError_ = true; }

  private:
#ifdef HAVE_EXPAT
    KML* poKMLFile_;
#endif

    char* pszName_;

    OGRKMLLayer** papoLayers_;
    int nLayers_;

    // The name of the field to use for the KML name element.
    char* pszNameField_;
    char* pszDescriptionField_;

    // The KML altitude mode to use.
    char* pszAltitudeMode_;

    char** papszCreateOptions_;

    // Output related parameters.
    VSILFILE* fpOutput_;

    OGREnvelope oEnvelope_;

    // Have we issued a coordinate transformation already for this datasource.
    bool bIssuedCTError_;
};

#endif /* OGR_KML_H_INCLUDED */
