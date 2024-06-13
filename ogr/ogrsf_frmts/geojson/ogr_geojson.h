/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definitions of OGR OGRGeoJSON driver types.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_GEOJSON_H_INCLUDED
#define OGR_GEOJSON_H_INCLUDED

#include "cpl_port.h"
#include "ogrsf_frmts.h"
#include "../mem/ogr_mem.h"

#include <cstdio>
#include <vector>  // Used by OGRGeoJSONLayer.
#include "ogrgeojsonutils.h"
#include "ogrgeojsonwriter.h"

class OGRGeoJSONDataSource;

GDALDataset *OGRGeoJSONDriverOpenInternal(GDALOpenInfo *poOpenInfo,
                                          GeoJSONSourceType nSrcType,
                                          const char *pszJSonFlavor);
void OGRGeoJSONDriverStoreContent(const char *pszSource, char *pszText);
char *OGRGeoJSONDriverStealStoredContent(const char *pszSource);

/************************************************************************/
/*                           OGRGeoJSONLayer                            */
/************************************************************************/

class OGRGeoJSONReader;

class OGRGeoJSONLayer final : public OGRMemLayer
{
    friend class OGRGeoJSONDataSource;

  public:
    static const char *const DefaultName;
    static const OGRwkbGeometryType DefaultGeometryType;

    OGRGeoJSONLayer(const char *pszName, OGRSpatialReference *poSRS,
                    OGRwkbGeometryType eGType, OGRGeoJSONDataSource *poDS,
                    OGRGeoJSONReader *poReader);
    virtual ~OGRGeoJSONLayer();

    //
    // OGRLayer Interface
    //
    virtual const char *GetFIDColumn() override;
    virtual int TestCapability(const char *pszCap) override;

    virtual OGRErr SyncToDisk() override;

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature(GIntBig nFID) override;
    virtual GIntBig GetFeatureCount(int bForce) override;

    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;
    virtual OGRErr DeleteFeature(GIntBig nFID) override;
    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr DeleteField(int iField) override;
    virtual OGRErr ReorderFields(int *panMap) override;
    virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                  int nFlags) override;
    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomField,
                                   int bApproxOK = TRUE) override;
    virtual OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce = TRUE) override;
    virtual OGRErr GetExtent3D(int iGeomField, OGREnvelope3D *psExtent3D,
                               int bForce = TRUE) override;

    GDALDataset *GetDataset() override;

    //
    // OGRGeoJSONLayer Interface
    //
    void SetFIDColumn(const char *pszFIDColumn);
    void AddFeature(OGRFeature *poFeature);
    void DetectGeometryType();

    void IncFeatureCount()
    {
        nTotalFeatureCount_++;
    }

    void UnsetReader()
    {
        poReader_ = nullptr;
    }

    void InvalidateFeatureCount()
    {
        nTotalFeatureCount_ = -1;
    }

    void SetWriteOptions(const OGRGeoJSONWriteOptions &options)
    {
        oWriteOptions_ = options;
    }

  private:
    OGRGeoJSONDataSource *poDS_;
    OGRGeoJSONReader *poReader_;
    bool bHasAppendedFeatures_;
    CPLString sFIDColumn_;
    bool bOriginalIdModified_;
    GIntBig nTotalFeatureCount_;
    GIntBig nFeatureReadSinceReset_ = 0;

    //! Write options used by ICreateFeature() in append scenarios
    OGRGeoJSONWriteOptions oWriteOptions_;

    bool IngestAll();
    void TerminateAppendSession();
    bool SetOrUpdateFeaturePreparation();

    CPL_DISALLOW_COPY_ASSIGN(OGRGeoJSONLayer)
};

/************************************************************************/
/*                         OGRGeoJSONWriteLayer                         */
/************************************************************************/

class OGRGeoJSONWriteLayer final : public OGRLayer
{
  public:
    OGRGeoJSONWriteLayer(const char *pszName, OGRwkbGeometryType eGType,
                         CSLConstList papszOptions, bool bWriteFC_BBOXIn,
                         OGRCoordinateTransformation *poCT,
                         OGRGeoJSONDataSource *poDS);
    ~OGRGeoJSONWriteLayer();

    //
    // OGRLayer Interface
    //
    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn_;
    }

    OGRSpatialReference *GetSpatialRef() override
    {
        return nullptr;
    }

    void ResetReading() override
    {
    }

    OGRFeature *GetNextFeature() override
    {
        return nullptr;
    }

    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(const OGRFieldDefn *poField, int bApproxOK) override;
    int TestCapability(const char *pszCap) override;
    OGRErr GetExtent(OGREnvelope *psExtent, int bForce) override;

    OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
    {
        return iGeomField == 0
                   ? OGRGeoJSONWriteLayer::GetExtent(psExtent, bForce)
                   : OGRERR_FAILURE;
    }

    OGRErr SyncToDisk() override;

    GDALDataset *GetDataset() override;

  private:
    OGRGeoJSONDataSource *poDS_;
    OGRFeatureDefn *poFeatureDefn_;
    int nOutCounter_;
    /** Offset at which the '] }' terminating sequence has already been
     * written by SyncToDisk(). 0 if it has not been written.
     */
    vsi_l_offset m_nPositionBeforeFCClosed = 0;

    bool bWriteBBOX;
    bool bBBOX3D;
    bool bWriteFC_BBOX;
    OGREnvelope3D sEnvelopeLayer;

    int nSignificantFigures_;

    bool bRFC7946_;
    bool bWrapDateLine_ = false;
    std::string osForeignMembers_{};
    OGRCoordinateTransformation *poCT_;
    OGRGeometryFactory::TransformWithOptionsCache oTransformCache_;
    OGRGeoJSONWriteOptions oWriteOptions_;

    CPL_DISALLOW_COPY_ASSIGN(OGRGeoJSONWriteLayer)

    void FinishWriting();
};

/************************************************************************/
/*                           OGRGeoJSONDataSource                       */
/************************************************************************/

class OGRGeoJSONDataSource final : public OGRDataSource
{
  public:
    OGRGeoJSONDataSource();
    virtual ~OGRGeoJSONDataSource();

    //
    // OGRDataSource Interface
    //
    int Open(GDALOpenInfo *poOpenInfo, GeoJSONSourceType nSrcType,
             const char *pszJSonFlavor);
    const char *GetName() override;
    int GetLayerCount() override;
    OGRLayer *GetLayer(int nLayer) override;
    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    int TestCapability(const char *pszCap) override;

    void AddLayer(OGRGeoJSONLayer *poLayer);

    //
    // OGRGeoJSONDataSource Interface
    //
    int Create(const char *pszName, char **papszOptions);

    VSILFILE *GetOutputFile() const
    {
        return fpOut_;
    }

    enum GeometryTranslation
    {
        eGeometryPreserve,
        eGeometryAsCollection,
    };

    void SetGeometryTranslation(GeometryTranslation type);

    enum AttributesTranslation
    {
        eAttributesPreserve,
        eAttributesSkip
    };

    void SetAttributesTranslation(AttributesTranslation type);

    int GetFpOutputIsSeekable() const
    {
        return bFpOutputIsSeekable_;
    }

    int GetBBOXInsertLocation() const
    {
        return nBBOXInsertLocation_;
    }

    int HasOtherPages() const
    {
        return bOtherPages_;
    }

    bool IsUpdatable() const
    {
        return bUpdatable_;
    }

    const CPLString &GetJSonFlavor() const
    {
        return osJSonFlavor_;
    }

    virtual CPLErr FlushCache(bool bAtClosing) override;

    CPLErr Close() override;

    static const size_t SPACE_FOR_BBOX = 130;

  private:
    //
    // Private data members
    //
    char *pszName_;
    char *pszGeoData_;
    vsi_l_offset nGeoDataLen_;
    OGRGeoJSONLayer **papoLayers_;
    OGRGeoJSONWriteLayer **papoLayersWriter_;
    int nLayers_;
    VSILFILE *fpOut_;

    //
    // Translation/Creation control flags
    //
    GeometryTranslation flTransGeom_;
    AttributesTranslation flTransAttrs_;
    bool bOtherPages_;  // ESRI Feature Service specific.

    bool bFpOutputIsSeekable_;
    int nBBOXInsertLocation_;

    bool bUpdatable_;

    CPLString osJSonFlavor_;

    //
    // Private utility functions
    //
    bool Clear();
    int ReadFromFile(GDALOpenInfo *poOpenInfo, const char *pszUnprefixed);
    int ReadFromService(GDALOpenInfo *poOpenInfo, const char *pszSource);
    void LoadLayers(GDALOpenInfo *poOpenInfo, GeoJSONSourceType nSrcType,
                    const char *pszUnprefixed, const char *pszJSonFlavor);
    void SetOptionsOnReader(GDALOpenInfo *poOpenInfo,
                            OGRGeoJSONReader *poReader);
    void CheckExceededTransferLimit(json_object *poObj);
    void RemoveJSonPStuff();

    CPL_DISALLOW_COPY_ASSIGN(OGRGeoJSONDataSource)
};

#endif  // OGR_GEOJSON_H_INCLUDED
