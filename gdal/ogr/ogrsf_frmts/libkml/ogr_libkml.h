/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
 *****************************************************************************/

#ifndef HAVE_OGR_LIBKML_H
#define HAVE_OGR_LIBKML_H

#include "ogrsf_frmts.h"

#include "libkml_headers.h"

class OGRLIBKMLDataSource;

CPLString OGRLIBKMLGetSanitizedNCName(const char* pszName);

/******************************************************************************
  layer class
******************************************************************************/

class OGRLIBKMLLayer:public OGRLayer
{
    int                       bUpdate;
    bool                      bUpdated;
    int                       nFeatures;
    int                       iFeature;
    long                      nFID;
    const char                *m_pszName;
    const char                *m_pszFileName;

    kmldom::ContainerPtr      m_poKmlLayer;
    kmldom::ElementPtr        m_poKmlLayerRoot;
    kmldom::UpdatePtr         m_poKmlUpdate;

    OGRLIBKMLDataSource      *m_poOgrDS;
    OGRFeatureDefn           *m_poOgrFeatureDefn;
    kmldom::SchemaPtr         m_poKmlSchema;
    OGRSpatialReference      *m_poOgrSRS;

    bool                      m_bReadGroundOverlay;
    bool                      m_bUseSimpleField;

    bool                      m_bWriteRegion;
    bool                      m_bRegionBoundsAuto;
    double                    m_dfRegionMinLodPixels;
    double                    m_dfRegionMaxLodPixels;
    double                    m_dfRegionMinFadeExtent;
    double                    m_dfRegionMaxFadeExtent;
    double                    m_dfRegionMinX;
    double                    m_dfRegionMinY;
    double                    m_dfRegionMaxX;
    double                    m_dfRegionMaxY;

    CPLString                 osListStyleType;
    CPLString                 osListStyleIconHref;

    bool                      m_bUpdateIsFolder;

  public:
    OGRLIBKMLLayer            ( const char *pszLayerName,
                                OGRSpatialReference * poSpatialRef,
                                OGRwkbGeometryType eGType,
                                OGRLIBKMLDataSource *poOgrDS,
                                kmldom::ElementPtr poKmlRoot,
                                kmldom::ContainerPtr poKmlContainer,
                                kmldom::UpdatePtr poKmlUpdate,
                                const char *pszFileName,
                                int bNew,
                                int bUpdate );
    virtual ~OGRLIBKMLLayer();

    void                      ResetReading() override { iFeature = 0; nFID = 1; }
    OGRFeature               *GetNextFeature() override;
    OGRFeature               *GetNextRawFeature();
    OGRFeatureDefn           *GetLayerDefn() override { return m_poOgrFeatureDefn; }
    // OGRErr                    SetAttributeFilter(const char * );
    OGRErr                    ICreateFeature( OGRFeature * poOgrFeat ) override;
    OGRErr                    ISetFeature( OGRFeature * poOgrFeat ) override;
    OGRErr                    DeleteFeature( GIntBig nFID ) override;

    GIntBig                   GetFeatureCount( int bForce = TRUE ) override;
    OGRErr                    GetExtent( OGREnvelope * psExtent,
                                         int bForce = TRUE ) override;
    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    //const char               *GetInfo( const char * );

    OGRErr                    CreateField( OGRFieldDefn * poField,
                                           int bApproxOK = TRUE ) override;

    OGRErr                    SyncToDisk() override;

    OGRStyleTable            *GetStyleTable() override;
    void                  SetStyleTableDirectly( OGRStyleTable * poStyleTable ) override;
    void                      SetStyleTable( OGRStyleTable * poStyleTable ) override;
    const char               *GetName() override { return m_pszName; }
    int                       TestCapability( const char * ) override;
    kmldom::ContainerPtr      GetKmlLayer() { return m_poKmlLayer; }
    kmldom::ElementPtr        GetKmlLayerRoot() { return m_poKmlLayerRoot; }
    kmldom::SchemaPtr         GetKmlSchema() { return m_poKmlSchema; }
    const char               *GetFileName() { return m_pszFileName; }

    void                      SetLookAt( const char* pszLookatLongitude,
                                         const char* pszLookatLatitude,
                                         const char* pszLookatAltitude,
                                         const char* pszLookatHeading,
                                         const char* pszLookatTilt,
                                         const char* pszLookatRange,
                                         const char* pszLookatAltitudeMode );
    void                      SetCamera( const char* pszCameraLongitude,
                                         const char* pszCameraLatitude,
                                         const char* pszCameraAltitude,
                                         const char* pszCameraHeading,
                                         const char* pszCameraTilt,
                                         const char* pszCameraRoll,
                                         const char* pszCameraAltitudeMode );

    static CPLString          LaunderFieldNames( CPLString osName );

    void                      SetWriteRegion( double dfMinLodPixels,
                                              double dfMaxLodPixels,
                                              double dfMinFadeExtent,
                                              double dfMaxFadeExtent );
    void                      SetRegionBounds( double dfMinX, double dfMinY,
                                               double dfMaxX, double dfMaxY );

    void                      SetScreenOverlay( const char* pszSOHref,
                                                const char* pszSOName,
                                                const char* pszSODescription,
                                                const char* pszSOOverlayX,
                                                const char* pszSOOverlayY,
                                                const char* pszSOOverlayXUnits,
                                                const char* pszSOOverlayYUnits,
                                                const char* pszSOScreenX,
                                                const char* pszSOScreenY,
                                                const char* pszSOScreenXUnits,
                                                const char* pszSOScreenYUnits,
                                                const char* pszSOSizeX,
                                                const char* pszSOSizeY,
                                                const char* pszSOSizeXUnits,
                                                const char* pszSOSizeYUnits );

    void                      SetListStyle( const char* pszListStyleType,
                                            const char* pszListStyleIconHref );

    void                      Finalize( kmldom::DocumentPtr poKmlDocument );
    void                      SetUpdateIsFolder( int bUpdateIsFolder )
        { m_bUpdateIsFolder = CPL_TO_BOOL(bUpdateIsFolder); }
};

/******************************************************************************
  datasource class
******************************************************************************/

class OGRLIBKMLDataSource:public OGRDataSource
{
    char                     *pszName;

    /***** layers *****/
    OGRLIBKMLLayer          **papoLayers;
    int                       nLayers;
    int                       nAlloced;

    bool                      bUpdate;
    bool                      bUpdated;
    CPLString                 osUpdateTargetHref;

    char                    **m_papszOptions;

    /***** for kml files *****/
    bool                      m_isKml;
    kmldom::KmlPtr            m_poKmlDSKml;
    kmldom::ContainerPtr      m_poKmlDSContainer;
    kmldom::UpdatePtr         m_poKmlUpdate;

    /***** for kmz files *****/
    bool                      m_isKmz;
    kmldom::ContainerPtr      m_poKmlDocKml;
    kmldom::ElementPtr        m_poKmlDocKmlRoot;
    kmldom::ContainerPtr      m_poKmlStyleKml;
    char                     *pszStylePath;

    /***** for dir *****/
    int                       m_isDir;

    /***** the kml factory *****/
    kmldom::KmlFactory       *m_poKmlFactory;

    /***** style table pointer *****/
    void                      SetCommonOptions(
        kmldom::ContainerPtr poKmlContainer,
        char** papszOptions );

    void                      ParseDocumentOptions(
        kmldom::KmlPtr poKml,
        kmldom::DocumentPtr poKmlDocument );

  public:
    explicit OGRLIBKMLDataSource       ( kmldom::KmlFactory *poKmlFactory );
    ~OGRLIBKMLDataSource      ();

    const char               *GetName() override { return pszName; }

    int                       GetLayerCount() override { return nLayers; }
    OGRLayer                 *GetLayer( int ) override;
    OGRLayer                 *GetLayerByName( const char * ) override;
    OGRErr                    DeleteLayer( int ) override;

    OGRLayer                 *ICreateLayer( const char *pszName,
                                            OGRSpatialReference * poSpatialRef = NULL,
                                            OGRwkbGeometryType eGType = wkbUnknown,
                                            char **papszOptions = NULL ) override;

    OGRStyleTable            *GetStyleTable() override;
    void                      SetStyleTableDirectly( OGRStyleTable * poStyleTable ) override;
    void                      SetStyleTable( OGRStyleTable * poStyleTable ) override;

    int                       Open( const char *pszFilename,
                                     int bUpdate );
    int                       Create( const char *pszFilename,
                                      char **papszOptions );

    void                      FlushCache() override;
    int                       TestCapability(const char * ) override;

    kmldom::KmlFactory       *GetKmlFactory() { return m_poKmlFactory; }

    const char               *GetStylePath() { return pszStylePath; }
    int                       ParseIntoStyleTable( std::string * oKmlStyleKml,
                                                   const char *pszStylePath );

    // KmzFile                  *GetKmz() { return m_poKmlKmzfile; }

    int                       IsKml() const { return m_isKml; }
    int                       IsKmz() const { return m_isKmz; }
    int                       IsDir() const { return m_isDir; }

    void                      Updated() { bUpdated = true; }

    int                       ParseLayers( kmldom::ContainerPtr poKmlContainer,
                                           OGRSpatialReference *poOgrSRS );
    kmldom::SchemaPtr         FindSchema( const char *pszSchemaUrl);

  private:
    /***** methods to write out various datasource types at destroy *****/
    void                      WriteKml();
    void                      WriteKmz();
    void                      WriteDir();

    /***** methods to open various datasource types *****/
    int                       OpenKmz( const char *pszFilename,
                                       int bUpdate );
    int                       OpenKml( const char *pszFilename,
                                       int bUpdate );
    int                       OpenDir( const char *pszFilename,
                                       int bUpdate );

    /***** methods to create various datasource types *****/
    int                       CreateKml( const char *pszFilename,
                                         char **papszOptions );
    int                       CreateKmz( const char *pszFilename,
                                         char **papszOptions );
    int                       CreateDir( const char *pszFilename,
                                         char **papszOptions );

    /***** methods to create layers on various datasource types *****/
    OGRLIBKMLLayer           *CreateLayerKml( const char *pszLayerName,
                                              OGRSpatialReference * poOgrSRS,
                                              OGRwkbGeometryType eGType,
                                              char **papszOptions );
    OGRLIBKMLLayer           *CreateLayerKmz( const char *pszLayerName,
                                              OGRSpatialReference * poOgrSRS,
                                              OGRwkbGeometryType eGType,
                                              char **papszOptions );

    /***** methods to delete layers on various datasource types *****/
    OGRErr                    DeleteLayerKml( int );
    OGRErr                    DeleteLayerKmz( int );

    /***** methods to write a styletable to various datasource types *****/
    void                      SetStyleTable2Kml( OGRStyleTable * poStyleTable );
    void                      SetStyleTable2Kmz( OGRStyleTable * poStyleTable );

    OGRLIBKMLLayer           *AddLayer( const char *pszLayerName,
                                        OGRSpatialReference * poSpatialRef,
                                        OGRwkbGeometryType eGType,
                                        OGRLIBKMLDataSource * poOgrDS,
                                        kmldom::ElementPtr poKmlRoot,
                                        kmldom::ContainerPtr poKmlContainer,
                                        const char *pszFileName,
                                        int bNew,
                                        int bUpdate,
                                        int nGuess);
};

#endif
