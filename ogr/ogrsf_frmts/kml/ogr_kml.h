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
/*                             OGRKMLLayer                              */
/************************************************************************/

class OGRKMLLayer final : public OGRLayer
{
  public:
    OGRKMLLayer(const char *pszName_, const OGRSpatialReference *poSRS,
                bool bWriter, OGRwkbGeometryType eType, OGRKMLDataSource *poDS);
    ~OGRKMLLayer() override;

    //
    // OGRLayer Interface
    //
    const OGRFeatureDefn *GetLayerDefn() const override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(const OGRFieldDefn *poField,
                       int bApproxOK = TRUE) override;
    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    GIntBig GetFeatureCount(int bForce = TRUE) override;
    int TestCapability(const char *pszCap) const override;

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

    OGRKMLDataSource *poDS_ = nullptr;
    OGRSpatialReference *poSRS_ = nullptr;
    OGRCoordinateTransformation *poCT_ = nullptr;

    OGRFeatureDefn *poFeatureDefn_ = nullptr;

    int iNextKMLId_ = 0;
    bool bWriter_ = false;
    int nLayerNumber_ = 0;
    GIntBig nWroteFeatureCount_ = 0;
    bool bSchemaWritten_ = false;
    bool bClosedForWriting = false;
    char *pszName_ = nullptr;

    int nLastAsked = -1;
    int nLastCount = -1;

    CPL_DISALLOW_COPY_ASSIGN(OGRKMLLayer)
};

/************************************************************************/
/*                           OGRKMLDataSource                           */
/************************************************************************/

class OGRKMLDataSource final : public GDALDataset
{
  public:
    OGRKMLDataSource();
    ~OGRKMLDataSource() override;

    int Open(const char *pszName, int bTestOpen);

    int GetLayerCount() const override
    {
        return nLayers_;
    }

    const OGRLayer *GetLayer(int nLayer) const override;
    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    int TestCapability(const char *pszCap) const override;

    //
    // OGRKMLDataSource Interface
    //
    int Create(const char *pszName, CSLConstList papszOptions);

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
    KML *poKMLFile_ = nullptr;
#endif

    OGRKMLLayer **papoLayers_ = nullptr;
    int nLayers_ = 0;

    // The name of the field to use for the KML name element.
    char *pszNameField_ = nullptr;
    char *pszDescriptionField_ = nullptr;

    // The KML altitude mode to use.
    char *pszAltitudeMode_ = nullptr;

    char **papszCreateOptions_ = nullptr;

    // Output related parameters.
    VSILFILE *fpOutput_ = nullptr;

    OGREnvelope oEnvelope_{};

    // Have we issued a coordinate transformation already for this datasource.
    bool bIssuedCTError_ = false;

    CPL_DISALLOW_COPY_ASSIGN(OGRKMLDataSource)
};

#endif /* OGR_KML_H_INCLUDED */
