/******************************************************************************
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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef OGR_KML_H_INCLUDED
#define OGR_KML_H_INCLUDED

#include "ogrsf_frmts.h"

#ifdef HAVE_EXPAT
#include "kmlvector.h"
#endif

class OGRKMLDataSource;

/************************************************************************/
/*                            OGRKMLLayer                               */
/************************************************************************/

class OGRKMLLayer final : public OGRLayer
{
  public:
    OGRKMLLayer(const char *pszName_, const OGRSpatialReference *poSRS,
                bool bWriter, OGRwkbGeometryType eType, OGRKMLDataSource *poDS);
    ~OGRKMLLayer();

    //
    // OGRLayer Interface
    //
    OGRFeatureDefn *GetLayerDefn() override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(const OGRFieldDefn *poField,
                       int bApproxOK = TRUE) override;
    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    GIntBig GetFeatureCount(int bForce = TRUE) override;
    int TestCapability(const char *pszCap) override;

    GDALDataset *GetDataset() override;

    //
    // OGRKMLLayer Interface
    //
    void SetLayerNumber(int nLayer);

    void SetClosedForWriting()
    {
        bClosedForWriting = true;
    }

    CPLString WriteSchema();

  private:
    friend class OGRKMLDataSource;

    OGRKMLDataSource *poDS_;
    OGRSpatialReference *poSRS_;
    OGRCoordinateTransformation *poCT_;

    OGRFeatureDefn *poFeatureDefn_;

    int iNextKMLId_;
    bool bWriter_;
    int nLayerNumber_;
    GIntBig nWroteFeatureCount_;
    bool bSchemaWritten_;
    bool bClosedForWriting;
    char *pszName_;

    int nLastAsked;
    int nLastCount;
};

/************************************************************************/
/*                           OGRKMLDataSource                           */
/************************************************************************/

class OGRKMLDataSource final : public GDALDataset
{
  public:
    OGRKMLDataSource();
    ~OGRKMLDataSource();

    int Open(const char *pszName, int bTestOpen);

    int GetLayerCount() override
    {
        return nLayers_;
    }

    OGRLayer *GetLayer(int nLayer) override;
    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    int TestCapability(const char *pszCap) override;

    //
    // OGRKMLDataSource Interface
    //
    int Create(const char *pszName, char **papszOptions);

    const char *GetNameField() const
    {
        return pszNameField_;
    }

    const char *GetDescriptionField() const
    {
        return pszDescriptionField_;
    }

    const char *GetAltitudeMode()
    {
        return pszAltitudeMode_;
    }

    VSILFILE *GetOutputFP()
    {
        return fpOutput_;
    }

    void GrowExtents(OGREnvelope *psGeomBounds);
#ifdef HAVE_EXPAT
    KML *GetKMLFile()
    {
        return poKMLFile_;
    }
#endif

    bool IsFirstCTError() const
    {
        return !bIssuedCTError_;
    }

    void IssuedFirstCTError()
    {
        bIssuedCTError_ = true;
    }

  private:
#ifdef HAVE_EXPAT
    KML *poKMLFile_;
#endif

    OGRKMLLayer **papoLayers_;
    int nLayers_;

    // The name of the field to use for the KML name element.
    char *pszNameField_;
    char *pszDescriptionField_;

    // The KML altitude mode to use.
    char *pszAltitudeMode_;

    char **papszCreateOptions_;

    // Output related parameters.
    VSILFILE *fpOutput_;

    OGREnvelope oEnvelope_;

    // Have we issued a coordinate transformation already for this datasource.
    bool bIssuedCTError_;
};

#endif /* OGR_KML_H_INCLUDED */
