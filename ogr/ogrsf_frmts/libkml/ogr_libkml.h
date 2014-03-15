/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
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

#include <kml/engine.h>
#include <kml/dom.h>


using kmldom::KmlFactory;
using kmldom::KmlPtr;
using kmldom::DocumentPtr;
using kmldom::ContainerPtr;
using kmldom::ElementPtr;
using kmldom::SchemaPtr;
using kmldom::UpdatePtr;
using kmlengine::KmzFile;
using kmlengine::KmzFilePtr;

using kmlengine::KmlFile;
using kmlengine::KmlFilePtr;

class OGRLIBKMLDataSource;

CPLString OGRLIBKMLGetSanitizedNCName(const char* pszName);

/******************************************************************************
  layer class
******************************************************************************/

class OGRLIBKMLLayer:public OGRLayer
{
    int                       bUpdate;
    int                       bUpdated;
    int                       nFeatures;
    int                       iFeature;
    long                      nFID;
    const char                *m_pszName;
    const char                *m_pszFileName;

    ContainerPtr              m_poKmlLayer;
    ElementPtr                m_poKmlLayerRoot;
    UpdatePtr                 m_poKmlUpdate;

    DocumentPtr               m_poKmlDocument;
    //OGRStyleTable            *m_poStyleTable;
    OGRLIBKMLDataSource      *m_poOgrDS;
    OGRFeatureDefn           *m_poOgrFeatureDefn;
    SchemaPtr                 m_poKmlSchema;
    OGRSpatialReference      *m_poOgrSRS;

    int                       m_bReadGroundOverlay;

    int                       m_bWriteRegion;
    int                       m_bRegionBoundsAuto;
    double                    m_dfRegionMinLodPixels;
    double                    m_dfRegionMaxLodPixels;
    double                    m_dfRegionMinFadeExtent;
    double                    m_dfRegionMaxFadeExtent;
    double                    m_dfRegionMinX;
    double                    m_dfRegionMinY;
    double                    m_dfRegionMaxX;
    double                    m_dfRegionMaxY;

  public:
    OGRLIBKMLLayer            ( const char *pszLayerName,
                                OGRSpatialReference * poSpatialRef,
                                OGRwkbGeometryType eGType,
                                OGRLIBKMLDataSource *poOgrDS,
                                ElementPtr poKmlRoot,
                                ContainerPtr poKmlContainer,
                                UpdatePtr poKmlUpdate,
                                const char *pszFileName,
                                int bNew,
                                int bUpdate);
    ~OGRLIBKMLLayer           (  );

    void                      ResetReading  (  ) { iFeature = 0; nFID = 1; };
    OGRFeature               *GetNextFeature (  );
    OGRFeature               *GetNextRawFeature (  );
    OGRFeatureDefn           *GetLayerDefn (  ) { return m_poOgrFeatureDefn; };
    //OGRErr                    SetAttributeFilter (const char * );
    OGRErr                    CreateFeature ( OGRFeature * poOgrFeat );
    OGRErr                    SetFeature ( OGRFeature * poOgrFeat );
    OGRErr                    DeleteFeature( long nFID );

    int                       GetFeatureCount ( int bForce = TRUE );
    OGRErr                    GetExtent ( OGREnvelope * psExtent,
                                          int bForce = TRUE );


    //const char               *GetInfo ( const char * );

    OGRErr                    CreateField ( OGRFieldDefn * poField,
                                            int bApproxOK = TRUE );

    OGRErr                    SyncToDisk (  );

    OGRStyleTable            *GetStyleTable (  );
    void                      SetStyleTableDirectly ( OGRStyleTable * poStyleTable );
    void                      SetStyleTable ( OGRStyleTable * poStyleTable );
    const char               *GetName(  ) { return m_pszName; };
    int                       TestCapability ( const char * );
    ContainerPtr              GetKmlLayer () { return m_poKmlLayer; };
    ElementPtr                GetKmlLayerRoot () { return m_poKmlLayerRoot; };
    SchemaPtr                 GetKmlSchema () { return m_poKmlSchema; };
    const char               *GetFileName (  ) { return m_pszFileName; };

    void                      SetLookAt(const char* pszLookatLongitude,
                                        const char* pszLookatLatitude,
                                        const char* pszLookatAltitude,
                                        const char* pszLookatHeading,
                                        const char* pszLookatTilt,
                                        const char* pszLookatRange,
                                        const char* pszLookatAltitudeMode);
    void                      SetCamera(const char* pszCameraLongitude,
                                        const char* pszCameraLatitude,
                                        const char* pszCameraAltitude,
                                        const char* pszCameraHeading,
                                        const char* pszCameraTilt,
                                        const char* pszCameraRoll,
                                        const char* pszCameraAltitudeMode);

    static CPLString          LaunderFieldNames(CPLString osName);
    
    void                      SetWriteRegion(double dfMinLodPixels,
                                             double dfMaxLodPixels,
                                             double dfMinFadeExtent,
                                             double dfMaxFadeExtent);
    void                      SetRegionBounds(double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY);

    void                      SetScreenOverlay(const char* pszSOHref,
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
                                               const char* pszSOSizeYUnits);

    void                      Finalize();
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
    

    int                       bUpdate;
    int                       bUpdated;
    CPLString                 osUpdateTargetHref;

    /***** for kml files *****/
    int                       m_isKml;
    KmlPtr                    m_poKmlDSKml;
    ContainerPtr              m_poKmlDSContainer;
    UpdatePtr                 m_poKmlUpdate;

    /***** for kmz files *****/

    int                       m_isKmz;
    ContainerPtr              m_poKmlDocKml;
    ElementPtr                m_poKmlDocKmlRoot;
    ContainerPtr              m_poKmlStyleKml;
    char               *pszStylePath;

    /***** for dir *****/

    int                       m_isDir;
    
    /***** the kml factory *****/
    
    KmlFactory               *m_poKmlFactory;
    
    /***** style table pointer *****/
    
    //OGRStyleTable            *m_poStyleTable;

  public:
    OGRLIBKMLDataSource       ( KmlFactory *poKmlFactory );
    ~OGRLIBKMLDataSource      (  );

    const char               *GetName (  ) { return pszName; };

    int                       GetLayerCount (  ) { return nLayers; }
    OGRLayer                 *GetLayer ( int );
    OGRLayer                 *GetLayerByName ( const char * );
    OGRErr                    DeleteLayer ( int );


    OGRLayer                 *CreateLayer ( const char *pszName,
                                            OGRSpatialReference * poSpatialRef = NULL,
                                            OGRwkbGeometryType eGType = wkbUnknown,
                                            char **papszOptions = NULL );

    OGRStyleTable            *GetStyleTable (  );
    void                      SetStyleTableDirectly ( OGRStyleTable * poStyleTable );
    void                      SetStyleTable ( OGRStyleTable * poStyleTable );

    int                       Open ( const char *pszFilename,
                                     int bUpdate );
    int                       Create ( const char *pszFilename,
                                       char **papszOptions );

    OGRErr                    SyncToDisk (  );
    int                       TestCapability (const char * );
    
    KmlFactory               *GetKmlFactory() { return m_poKmlFactory; };
        
    const char               *GetStylePath() {return pszStylePath; };
    int                       ParseIntoStyleTable ( std::string * oKmlStyleKml,
                                                    const char *pszStylePath);
        
    //KmzFile                  *GetKmz() { return m_poKmlKmzfile; };
        
    int                       IsKml() {return m_isKml;};
    int                       IsKmz() {return m_isKmz;};
    int                       IsDir() {return m_isDir;};
    
    void                      Updated() {bUpdated = TRUE;};

    int                       ParseLayers ( ContainerPtr poKmlContainer,
                                            OGRSpatialReference *poOgrSRS );
    SchemaPtr                 FindSchema ( const char *pszSchemaUrl);
        
  private:

    /***** methods to write out various datasource types at destroy *****/

    void                      WriteKml();
    void                      WriteKmz();
    void                      WriteDir();
    
    /***** methods to open various datasource types *****/
        
    int                       OpenKmz ( const char *pszFilename,
                                        int bUpdate );
    int                       OpenKml ( const char *pszFilename,
                                        int bUpdate );
    int                       OpenDir ( const char *pszFilename,
                                        int bUpdate );

    /***** methods to create various datasource types *****/
    
    int                       CreateKml ( const char *pszFilename,
                                          char **papszOptions );
    int                       CreateKmz ( const char *pszFilename,
                                          char **papszOptions );
    int                       CreateDir ( const char *pszFilename,
                                          char **papszOptions );

    /***** methods to create layers on various datasource types *****/
    
    OGRLIBKMLLayer           *CreateLayerKml ( const char *pszLayerName,
                                               OGRSpatialReference * poOgrSRS,
                                               OGRwkbGeometryType eGType,
                                               char **papszOptions );
    OGRLIBKMLLayer           *CreateLayerKmz ( const char *pszLayerName,
                                               OGRSpatialReference * poOgrSRS,
                                               OGRwkbGeometryType eGType,
                                               char **papszOptions );
        
    /***** methods to delete layers on various datasource types *****/
    
    OGRErr                    DeleteLayerKml ( int );
    OGRErr                    DeleteLayerKmz ( int );
    
    /***** methods to write a styletable to various datasource types *****/

    void                      SetStyleTable2Kml ( OGRStyleTable * poStyleTable );
    void                      SetStyleTable2Kmz ( OGRStyleTable * poStyleTable );
    

    

    OGRLIBKMLLayer           *AddLayer ( const char *pszLayerName,
                                         OGRSpatialReference * poSpatialRef,
                                         OGRwkbGeometryType eGType,
                                         OGRLIBKMLDataSource * poOgrDS,
                                         ElementPtr poKmlRoot,
                                         ContainerPtr poKmlContainer,
                                         const char *pszFileName,
                                         int bNew,
                                         int bUpdate,
                                         int nGuess);
};


/******************************************************************************
  driver class
******************************************************************************/

class OGRLIBKMLDriver:public OGRSFDriver
{
    KmlFactory               *m_poKmlFactory;
    
  public:
    OGRLIBKMLDriver           (  );
    ~OGRLIBKMLDriver          (  );

    const char               *GetName (  );
    OGRDataSource            *Open ( const char *pszFilename,
                                     int bUpdate );
    OGRDataSource            *CreateDataSource ( const char *pszFilename,
                                                 char **papszOptions );

    OGRErr                    DeleteDataSource ( const char *pszName );

    int                       TestCapability ( const char * );
};

#endif
