/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef HAVE_OGR_LIBKML_H
#define HAVE_OGR_LIBKML_H

#include "ogrsf_frmts.h"

#include "libkml_headers.h"
#include "fieldconfig.h"

#include <map>

class OGRLIBKMLDataSource;

CPLString OGRLIBKMLGetSanitizedNCName(const char *pszName);

/******************************************************************************
  layer class
******************************************************************************/

class OGRLIBKMLLayer final : public OGRLayer,
                             public OGRGetNextFeatureThroughRaw<OGRLIBKMLLayer>
{
    bool m_bNew = false;
    bool bUpdate = false;

    int nFeatures = 0;
    int iFeature = 0;
    GIntBig nFID = 1;
    const char *m_pszName;
    const char *m_pszFileName;
    std::string m_osSanitizedNCName;

    kmldom::ContainerPtr m_poKmlLayer;
    kmldom::ElementPtr m_poKmlLayerRoot;
    kmldom::UpdatePtr m_poKmlUpdate;

    fieldconfig m_oFieldConfig;
    OGRLIBKMLDataSource *m_poOgrDS;
    OGRFeatureDefn *m_poOgrFeatureDefn;
    kmldom::SchemaPtr m_poKmlSchema;
    OGRSpatialReference *m_poOgrSRS;
    std::unique_ptr<OGRCoordinateTransformation> m_poCT{};

    bool m_bReadGroundOverlay;
    bool m_bUseSimpleField;

    bool m_bWriteRegion;
    bool m_bRegionBoundsAuto;
    double m_dfRegionMinLodPixels;
    double m_dfRegionMaxLodPixels;
    double m_dfRegionMinFadeExtent;
    double m_dfRegionMaxFadeExtent;
    double m_dfRegionMinX;
    double m_dfRegionMinY;
    double m_dfRegionMaxX;
    double m_dfRegionMaxY;

    CPLString osListStyleType;
    CPLString osListStyleIconHref;

    bool m_bUpdateIsFolder;

    bool m_bAllReadAtLeastOnce = false;
    std::map<GIntBig, std::string> m_oMapOGRIdToKmlId{};
    std::map<std::string, GIntBig> m_oMapKmlIdToOGRId{};

    void ScanAllFeatures();
    OGRFeature *GetNextRawFeature();

  public:
    OGRLIBKMLLayer(const char *pszLayerName, OGRwkbGeometryType eGType,
                   const OGRSpatialReference *poSRSIn,
                   OGRLIBKMLDataSource *poOgrDS, kmldom::ElementPtr poKmlRoot,
                   kmldom::ContainerPtr poKmlContainer,
                   kmldom::UpdatePtr poKmlUpdate, const char *pszFileName,
                   bool bNew, bool bUpdate);
    virtual ~OGRLIBKMLLayer();

    void ResetReading() override
    {
        iFeature = 0;
        nFID = 1;
    }
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRLIBKMLLayer)

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poOgrFeatureDefn;
    }

    // OGRErr                    SetAttributeFilter(const char * );
    OGRErr ICreateFeature(OGRFeature *poOgrFeat) override;
    OGRErr ISetFeature(OGRFeature *poOgrFeat) override;
    OGRErr DeleteFeature(GIntBig nFID) override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    // const char               *GetInfo( const char * );

    OGRErr CreateField(const OGRFieldDefn *poField,
                       int bApproxOK = TRUE) override;

    OGRErr SyncToDisk() override;

    OGRStyleTable *GetStyleTable() override;
    void SetStyleTableDirectly(OGRStyleTable *poStyleTable) override;
    void SetStyleTable(OGRStyleTable *poStyleTable) override;

    const char *GetName() override
    {
        return m_pszName;
    }

    int TestCapability(const char *) override;

    GDALDataset *GetDataset() override;

    kmldom::ContainerPtr GetKmlLayer()
    {
        return m_poKmlLayer;
    }

    kmldom::ElementPtr GetKmlLayerRoot()
    {
        return m_poKmlLayerRoot;
    }

    kmldom::SchemaPtr GetKmlSchema()
    {
        return m_poKmlSchema;
    }

    const char *GetFileName()
    {
        return m_pszFileName;
    }

    const fieldconfig &GetFieldConfig() const
    {
        return m_oFieldConfig;
    }

    void SetLookAt(const char *pszLookatLongitude,
                   const char *pszLookatLatitude, const char *pszLookatAltitude,
                   const char *pszLookatHeading, const char *pszLookatTilt,
                   const char *pszLookatRange,
                   const char *pszLookatAltitudeMode);
    void SetCamera(const char *pszCameraLongitude,
                   const char *pszCameraLatitude, const char *pszCameraAltitude,
                   const char *pszCameraHeading, const char *pszCameraTilt,
                   const char *pszCameraRoll,
                   const char *pszCameraAltitudeMode);

    static CPLString LaunderFieldNames(CPLString osName);

    void SetWriteRegion(double dfMinLodPixels, double dfMaxLodPixels,
                        double dfMinFadeExtent, double dfMaxFadeExtent);
    void SetRegionBounds(double dfMinX, double dfMinY, double dfMaxX,
                         double dfMaxY);

    void SetScreenOverlay(const char *pszSOHref, const char *pszSOName,
                          const char *pszSODescription,
                          const char *pszSOOverlayX, const char *pszSOOverlayY,
                          const char *pszSOOverlayXUnits,
                          const char *pszSOOverlayYUnits,
                          const char *pszSOScreenX, const char *pszSOScreenY,
                          const char *pszSOScreenXUnits,
                          const char *pszSOScreenYUnits, const char *pszSOSizeX,
                          const char *pszSOSizeY, const char *pszSOSizeXUnits,
                          const char *pszSOSizeYUnits);

    void SetListStyle(const char *pszListStyleType,
                      const char *pszListStyleIconHref);

    void Finalize(kmldom::DocumentPtr poKmlDocument);

    void SetUpdateIsFolder(int bUpdateIsFolder)
    {
        m_bUpdateIsFolder = CPL_TO_BOOL(bUpdateIsFolder);
    }
};

/******************************************************************************
  datasource class
******************************************************************************/

class OGRLIBKMLDataSource final : public GDALDataset
{
    bool m_bIssuedCTError = false;

    /***** layers *****/
    OGRLIBKMLLayer **papoLayers;
    int nLayers;
    int nAllocated;
    std::map<CPLString, OGRLIBKMLLayer *> m_oMapLayers;

    bool bUpdate;
    bool bUpdated;
    CPLString osUpdateTargetHref;

    char **m_papszOptions;

    /***** for kml files *****/
    bool m_isKml;
    kmldom::KmlPtr m_poKmlDSKml;
    kmldom::ContainerPtr m_poKmlDSContainer;
    kmldom::UpdatePtr m_poKmlUpdate;

    /***** for kmz files *****/
    bool m_isKmz;
    kmldom::ContainerPtr m_poKmlDocKml;
    kmldom::ElementPtr m_poKmlDocKmlRoot;
    kmldom::ContainerPtr m_poKmlStyleKml;
    std::string m_osStylePath{};

    /***** for dir *****/
    int m_isDir;

    /***** the kml factory *****/
    kmldom::KmlFactory *m_poKmlFactory;

    /***** style table pointer *****/
    void SetCommonOptions(kmldom::ContainerPtr poKmlContainer,
                          CSLConstList papszOptions);

    void ParseDocumentOptions(kmldom::KmlPtr poKml,
                              kmldom::DocumentPtr poKmlDocument);

  public:
    explicit OGRLIBKMLDataSource(kmldom::KmlFactory *poKmlFactory);
    ~OGRLIBKMLDataSource();

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;
    OGRLayer *GetLayerByName(const char *) override;
    OGRErr DeleteLayer(int) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    OGRStyleTable *GetStyleTable() override;
    void SetStyleTableDirectly(OGRStyleTable *poStyleTable) override;
    void SetStyleTable(OGRStyleTable *poStyleTable) override;

    int Open(const char *pszFilename, bool bUpdate);
    int Create(const char *pszFilename, char **papszOptions);

    CPLErr FlushCache(bool bAtClosing) override;
    int TestCapability(const char *) override;

    kmldom::KmlFactory *GetKmlFactory()
    {
        return m_poKmlFactory;
    }

    const std::string &GetStylePath() const
    {
        return m_osStylePath;
    }

    int ParseIntoStyleTable(std::string *oKmlStyleKml,
                            const char *pszStylePath);

    // KmzFile                  *GetKmz() { return m_poKmlKmzfile; }

    int IsKml() const
    {
        return m_isKml;
    }

    int IsKmz() const
    {
        return m_isKmz;
    }

    int IsDir() const
    {
        return m_isDir;
    }

    void Updated()
    {
        bUpdated = true;
    }

    int ParseLayers(kmldom::ContainerPtr poKmlContainer, bool bRecurse);
    kmldom::SchemaPtr FindSchema(const char *pszSchemaUrl);

    bool IsFirstCTError() const
    {
        return !m_bIssuedCTError;
    }

    void IssuedFirstCTError()
    {
        m_bIssuedCTError = true;
    }

  private:
    /***** methods to write out various datasource types at destroy *****/
    bool WriteKml();
    bool WriteKmz();
    bool WriteDir();

    /***** methods to open various datasource types *****/
    int OpenKmz(const char *pszFilename, int bUpdate);
    int OpenKml(const char *pszFilename, int bUpdate);
    int OpenDir(const char *pszFilename, int bUpdate);

    /***** methods to create various datasource types *****/
    int CreateKml(const char *pszFilename, char **papszOptions);
    int CreateKmz(const char *pszFilename, char **papszOptions);
    int CreateDir(const char *pszFilename, char **papszOptions);

    /***** methods to create layers on various datasource types *****/
    OGRLIBKMLLayer *CreateLayerKml(const char *pszLayerName,
                                   const OGRSpatialReference *poOgrSRS,
                                   OGRwkbGeometryType eGType,
                                   CSLConstList papszOptions);
    OGRLIBKMLLayer *CreateLayerKmz(const char *pszLayerName,
                                   const OGRSpatialReference *poOgrSRS,
                                   OGRwkbGeometryType eGType,
                                   CSLConstList papszOptions);

    /***** methods to delete layers on various datasource types *****/
    OGRErr DeleteLayerKml(int);
    OGRErr DeleteLayerKmz(int);

    /***** methods to write a styletable to various datasource types *****/
    void SetStyleTable2Kml(OGRStyleTable *poStyleTable);
    void SetStyleTable2Kmz(OGRStyleTable *poStyleTable);

    OGRLIBKMLLayer *
    AddLayer(const char *pszLayerName, OGRwkbGeometryType eGType,
             const OGRSpatialReference *poSRS, OGRLIBKMLDataSource *poOgrDS,
             kmldom::ElementPtr poKmlRoot, kmldom::ContainerPtr poKmlContainer,
             const char *pszFileName, bool bNew, bool bUpdate, int nGuess);
};

#endif
