/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes related to format registration, and file opening.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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

#ifndef OGRSF_FRMTS_H_INCLUDED
#define OGRSF_FRMTS_H_INCLUDED

#include "cpl_progress.h"
#include "ogr_feature.h"
#include "ogr_featurestyle.h"
#include "gdal_priv.h"

#include <memory>

/**
 * \file ogrsf_frmts.h
 *
 * Classes related to registration of format support, and opening datasets.
 */

//! @cond Doxygen_Suppress
#if !defined(GDAL_COMPILATION) && !defined(SUPPRESS_DEPRECATION_WARNINGS)
#define OGR_DEPRECATED(x) CPL_WARN_DEPRECATED(x)
#else
#define OGR_DEPRECATED(x)
#endif
//! @endcond

class OGRLayerAttrIndex;
class OGRSFDriver;

/************************************************************************/
/*                               OGRLayer                               */
/************************************************************************/

/**
 * This class represents a layer of simple features, with access methods.
 *
 */

/* Note: any virtual method added to this class must also be added in the */
/* OGRLayerDecorator and OGRMutexedLayer classes. */

class CPL_DLL OGRLayer : public GDALMajorObject
{
  private:
    struct Private;
    std::unique_ptr<Private> m_poPrivate;

    void         ConvertGeomsIfNecessary( OGRFeature *poFeature );

    class CPL_DLL FeatureIterator
    {
            struct Private;
            std::unique_ptr<Private> m_poPrivate;
        public:
            FeatureIterator(OGRLayer* poLayer, bool bStart);
            FeatureIterator(FeatureIterator&& oOther) noexcept; // declared but not defined. Needed for gcc 5.4 at least
            ~FeatureIterator();
            OGRFeatureUniquePtr& operator*();
            FeatureIterator& operator++();
            bool operator!=(const FeatureIterator& it) const;
    };

    friend inline FeatureIterator begin(OGRLayer* poLayer);
    friend inline FeatureIterator end(OGRLayer* poLayer);

    CPL_DISALLOW_COPY_ASSIGN(OGRLayer)

  protected:
//! @cond Doxygen_Suppress
    int          m_bFilterIsEnvelope;
    OGRGeometry *m_poFilterGeom;
    OGRPreparedGeometry *m_pPreparedFilterGeom; /* m_poFilterGeom compiled as a prepared geometry */
    OGREnvelope  m_sFilterEnvelope;
    int          m_iGeomFieldFilter; // specify the index on which the spatial
                                     // filter is active.

    int          FilterGeometry( OGRGeometry * );
    //int          FilterGeometry( OGRGeometry *, OGREnvelope* psGeometryEnvelope);
    int          InstallFilter( OGRGeometry * );

    OGRErr       GetExtentInternal(int iGeomField, OGREnvelope *psExtent, int bForce );
//! @endcond

    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) CPL_WARN_UNUSED_RESULT;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature )  CPL_WARN_UNUSED_RESULT;

  public:
    OGRLayer();
    virtual     ~OGRLayer();

    /** Return begin of feature iterator.
     *
     * Using this iterator for standard range-based loops is safe, but
     * due to implementation limitations, you shouldn't try to access
     * (dereference) more than one iterator step at a time, since the
     * OGRFeatureUniquePtr reference is reused.
     *
     * Only one iterator per layer can be active at a time.
     * @since GDAL 2.3
     */
    FeatureIterator begin();

    /** Return end of feature iterator. */
    FeatureIterator end();

    virtual OGRGeometry *GetSpatialFilter();
    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual void        SetSpatialFilterRect( double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY );

    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    virtual void        SetSpatialFilterRect( int iGeomField,
                                            double dfMinX, double dfMinY,
                                            double dfMaxX, double dfMaxY );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual void        ResetReading() = 0;
    virtual OGRFeature *GetNextFeature() CPL_WARN_UNUSED_RESULT = 0;
    virtual OGRErr      SetNextByIndex( GIntBig nIndex );
    virtual OGRFeature *GetFeature( GIntBig nFID )  CPL_WARN_UNUSED_RESULT;

    OGRErr      SetFeature( OGRFeature *poFeature )  CPL_WARN_UNUSED_RESULT;
    OGRErr      CreateFeature( OGRFeature *poFeature ) CPL_WARN_UNUSED_RESULT;

    virtual OGRErr      DeleteFeature( GIntBig nFID )  CPL_WARN_UNUSED_RESULT;

    virtual const char *GetName();
    virtual OGRwkbGeometryType GetGeomType();
    virtual OGRFeatureDefn *GetLayerDefn() = 0;
    virtual int         FindFieldIndex( const char *pszFieldName, int bExactMatch );

    virtual OGRSpatialReference *GetSpatialRef();

    virtual GIntBig     GetFeatureCount( int bForce = TRUE );
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE)  CPL_WARN_UNUSED_RESULT;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent,
                                  int bForce = TRUE)  CPL_WARN_UNUSED_RESULT;

    virtual int         TestCapability( const char * ) = 0;

    virtual OGRErr      Rename( const char* pszNewName ) CPL_WARN_UNUSED_RESULT;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlagsIn );

    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRErr      SyncToDisk();

    virtual OGRStyleTable *GetStyleTable();
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable );

    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

    virtual OGRErr      StartTransaction() CPL_WARN_UNUSED_RESULT;
    virtual OGRErr      CommitTransaction() CPL_WARN_UNUSED_RESULT;
    virtual OGRErr      RollbackTransaction();

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    virtual OGRErr      SetIgnoredFields( const char **papszFields );

    OGRErr              Intersection( OGRLayer *pLayerMethod,
                                      OGRLayer *pLayerResult,
                                      char** papszOptions = nullptr,
                                      GDALProgressFunc pfnProgress = nullptr,
                                      void * pProgressArg = nullptr );
    OGRErr              Union( OGRLayer *pLayerMethod,
                               OGRLayer *pLayerResult,
                               char** papszOptions = nullptr,
                               GDALProgressFunc pfnProgress = nullptr,
                               void * pProgressArg = nullptr );
    OGRErr              SymDifference( OGRLayer *pLayerMethod,
                                       OGRLayer *pLayerResult,
                                       char** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressArg );
    OGRErr              Identity( OGRLayer *pLayerMethod,
                                  OGRLayer *pLayerResult,
                                  char** papszOptions = nullptr,
                                  GDALProgressFunc pfnProgress = nullptr,
                                  void * pProgressArg = nullptr );
    OGRErr              Update( OGRLayer *pLayerMethod,
                                OGRLayer *pLayerResult,
                                char** papszOptions = nullptr,
                                GDALProgressFunc pfnProgress = nullptr,
                                void * pProgressArg = nullptr );
    OGRErr              Clip( OGRLayer *pLayerMethod,
                              OGRLayer *pLayerResult,
                              char** papszOptions = nullptr,
                              GDALProgressFunc pfnProgress = nullptr,
                              void * pProgressArg = nullptr );
    OGRErr              Erase( OGRLayer *pLayerMethod,
                               OGRLayer *pLayerResult,
                               char** papszOptions = nullptr,
                               GDALProgressFunc pfnProgress = nullptr,
                               void * pProgressArg = nullptr );

    int                 Reference();
    int                 Dereference();
    int                 GetRefCount() const;
//! @cond Doxygen_Suppress
    GIntBig             GetFeaturesRead();
//! @endcond

    /* non virtual : convenience wrapper for ReorderFields() */
    OGRErr              ReorderField( int iOldFieldPos, int iNewFieldPos );

//! @cond Doxygen_Suppress
    int                 AttributeFilterEvaluationNeedsGeometry();

    /* consider these private */
    OGRErr               InitializeIndexSupport( const char * );
    OGRLayerAttrIndex   *GetIndex() { return m_poAttrIndex; }
    int                 GetGeomFieldFilter() const { return m_iGeomFieldFilter; }
    const char          *GetAttrQueryString() const { return m_pszAttrQueryString; }
//! @endcond

    /** Convert a OGRLayer* to a OGRLayerH.
     * @since GDAL 2.3
     */
    static inline OGRLayerH ToHandle(OGRLayer* poLayer)
        { return reinterpret_cast<OGRLayerH>(poLayer); }

    /** Convert a OGRLayerH to a OGRLayer*.
     * @since GDAL 2.3
     */
    static inline OGRLayer* FromHandle(OGRLayerH hLayer)
        { return reinterpret_cast<OGRLayer*>(hLayer); }

 protected:
//! @cond Doxygen_Suppress
    OGRStyleTable       *m_poStyleTable;
    OGRFeatureQuery     *m_poAttrQuery;
    char                *m_pszAttrQueryString;
    OGRLayerAttrIndex   *m_poAttrIndex;

    int                  m_nRefCount;

    GIntBig              m_nFeaturesRead;
//! @endcond
};

/** Return begin of feature iterator.
 *
 * Using this iterator for standard range-based loops is safe, but
 * due to implementation limitations, you shouldn't try to access
 * (dereference) more than one iterator step at a time, since the
 * std::unique_ptr&lt;OGRFeature&gt; reference is reused.
 *
 * Only one iterator per layer can be active at a time.
 * @since GDAL 2.3
 * @see OGRLayer::begin()
 */
inline OGRLayer::FeatureIterator begin(OGRLayer* poLayer) { return poLayer->begin(); }

/** Return end of feature iterator.
 * @see OGRLayer::end()
 */
inline OGRLayer::FeatureIterator end(OGRLayer* poLayer) { return poLayer->end(); }

/** Unique pointer type for OGRLayer.
 * @since GDAL 3.2
 */
using OGRLayerUniquePtr = std::unique_ptr<OGRLayer>;

/************************************************************************/
/*                     OGRGetNextFeatureThroughRaw                      */
/************************************************************************/

/** Template class offering a GetNextFeature() implementation relying on
 * GetNextRawFeature()
 *
 * @since GDAL 3.2
 */
template<class BaseLayer> class OGRGetNextFeatureThroughRaw
{
protected:
    ~OGRGetNextFeatureThroughRaw() = default;

public:

    /** Implement OGRLayer::GetNextFeature(), relying on BaseLayer::GetNextRawFeature() */
    OGRFeature* GetNextFeature()
    {
        const auto poThis = static_cast<BaseLayer*>(this);
        while( true )
        {
            OGRFeature *poFeature = poThis->GetNextRawFeature();
            if (poFeature == nullptr)
                return nullptr;

            if((poThis->m_poFilterGeom == nullptr
                || poThis->FilterGeometry( poFeature->GetGeometryRef() ) )
            && (poThis->m_poAttrQuery == nullptr
                || poThis->m_poAttrQuery->Evaluate( poFeature )) )
            {
                return poFeature;
            }
            else
                delete poFeature;
        }
    }
};

/** Utility macro to define GetNextFeature() through GetNextRawFeature() */
#define DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(BaseLayer) \
    private: \
        friend class OGRGetNextFeatureThroughRaw<BaseLayer>; \
    public: \
        OGRFeature* GetNextFeature() override { return OGRGetNextFeatureThroughRaw<BaseLayer>::GetNextFeature(); }


/************************************************************************/
/*                            OGRDataSource                             */
/************************************************************************/

/**
 * LEGACY class. Use GDALDataset in your new code ! This class may be
 * removed in a later release.
 *
 * This class represents a data source.  A data source potentially
 * consists of many layers (OGRLayer).  A data source normally consists
 * of one, or a related set of files, though the name doesn't have to be
 * a real item in the file system.
 *
 * When an OGRDataSource is destroyed, all its associated OGRLayers objects
 * are also destroyed.
 *
 * NOTE: Starting with GDAL 2.0, it is *NOT* safe to cast the handle of
 * a C function that returns a OGRDataSourceH to a OGRDataSource*. If a C++ object
 * is needed, the handle should be cast to GDALDataset*.
 *
 * @deprecated
 */

class CPL_DLL OGRDataSource : public GDALDataset
{
public:
                        OGRDataSource();
//! @cond Doxygen_Suppress
    virtual const char  *GetName() OGR_DEPRECATED("Use GDALDataset class instead") = 0;

    static void         DestroyDataSource( OGRDataSource * ) OGR_DEPRECATED("Use GDALDataset class instead");
//! @endcond
};

/************************************************************************/
/*                             OGRSFDriver                              */
/************************************************************************/

/**
 * LEGACY class. Use GDALDriver in your new code ! This class may be
 * removed in a later release.
 *
 * Represents an operational format driver.
 *
 * One OGRSFDriver derived class will normally exist for each file format
 * registered for use, regardless of whether a file has or will be opened.
 * The list of available drivers is normally managed by the
 * OGRSFDriverRegistrar.
 *
 * NOTE: Starting with GDAL 2.0, it is *NOT* safe to cast the handle of
 * a C function that returns a OGRSFDriverH to a OGRSFDriver*. If a C++ object
 * is needed, the handle should be cast to GDALDriver*.
 *
 * @deprecated
 */

class CPL_DLL OGRSFDriver : public GDALDriver
{
  public:
//! @cond Doxygen_Suppress
    virtual     ~OGRSFDriver();

    virtual const char  *GetName() OGR_DEPRECATED("Use GDALDriver class instead") = 0;

    virtual OGRDataSource *Open( const char *pszName, int bUpdate=FALSE ) OGR_DEPRECATED("Use GDALDriver class instead") = 0;

    virtual int            TestCapability( const char *pszCap ) OGR_DEPRECATED("Use GDALDriver class instead") = 0;

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = nullptr ) OGR_DEPRECATED("Use GDALDriver class instead");
    virtual OGRErr      DeleteDataSource( const char *pszName ) OGR_DEPRECATED("Use GDALDriver class instead");
//! @endcond
};

/************************************************************************/
/*                         OGRSFDriverRegistrar                         */
/************************************************************************/

/**
 * LEGACY class. Use GDALDriverManager in your new code ! This class may be
 * removed in a later release.
 *
 * Singleton manager for OGRSFDriver instances that will be used to try
 * and open datasources.  Normally the registrar is populated with
 * standard drivers using the OGRRegisterAll() function and does not need
 * to be directly accessed.  The driver registrar and all registered drivers
 * may be cleaned up on shutdown using OGRCleanupAll().
 *
 * @deprecated
 */

class CPL_DLL OGRSFDriverRegistrar
{

                OGRSFDriverRegistrar();
                ~OGRSFDriverRegistrar();

    static GDALDataset* OpenWithDriverArg(GDALDriver* poDriver,
                                                 GDALOpenInfo* poOpenInfo);
    static GDALDataset* CreateVectorOnly( GDALDriver* poDriver,
                                          const char * pszName,
                                          char ** papszOptions );
    static CPLErr       DeleteDataSource( GDALDriver* poDriver,
                                          const char * pszName );

  public:
//! @cond Doxygen_Suppress
    static OGRSFDriverRegistrar *GetRegistrar() OGR_DEPRECATED("Use GDALDriverManager class instead");

    // cppcheck-suppress functionStatic
    void        RegisterDriver( OGRSFDriver * poDriver ) OGR_DEPRECATED("Use GDALDriverManager class instead");

    // cppcheck-suppress functionStatic
    int         GetDriverCount( void ) OGR_DEPRECATED("Use GDALDriverManager class instead");
    // cppcheck-suppress functionStatic
    GDALDriver *GetDriver( int iDriver ) OGR_DEPRECATED("Use GDALDriverManager class instead");
    // cppcheck-suppress functionStatic
    GDALDriver *GetDriverByName( const char * ) OGR_DEPRECATED("Use GDALDriverManager class instead");

    // cppcheck-suppress functionStatic
    int         GetOpenDSCount() OGR_DEPRECATED("Use GDALDriverManager class instead");
    // cppcheck-suppress functionStatic
    OGRDataSource *GetOpenDS( int ) OGR_DEPRECATED("Use GDALDriverManager class instead");
//! @endcond
};

/* -------------------------------------------------------------------- */
/*      Various available registration methods.                         */
/* -------------------------------------------------------------------- */
CPL_C_START

//! @cond Doxygen_Suppress
void OGRRegisterAllInternal();

void CPL_DLL RegisterOGRFileGDB();
void CPL_DLL RegisterOGRShape();
void CPL_DLL RegisterOGRNTF();
void CPL_DLL RegisterOGRSDTS();
void CPL_DLL RegisterOGRTiger();
void CPL_DLL RegisterOGRS57();
void CPL_DLL RegisterOGRTAB();
void CPL_DLL RegisterOGRMIF();
void CPL_DLL RegisterOGROGDI();
void CPL_DLL RegisterOGRODBC();
void CPL_DLL RegisterOGRWAsP();
void CPL_DLL RegisterOGRPG();
void CPL_DLL RegisterOGRMSSQLSpatial();
void CPL_DLL RegisterOGRMySQL();
void CPL_DLL RegisterOGROCI();
void CPL_DLL RegisterOGRDGN();
void CPL_DLL RegisterOGRGML();
void CPL_DLL RegisterOGRLIBKML();
void CPL_DLL RegisterOGRKML();
void CPL_DLL RegisterOGRFlatGeobuf();
void CPL_DLL RegisterOGRGeoJSON();
void CPL_DLL RegisterOGRGeoJSONSeq();
void CPL_DLL RegisterOGRESRIJSON();
void CPL_DLL RegisterOGRTopoJSON();
void CPL_DLL RegisterOGRAVCBin();
void CPL_DLL RegisterOGRAVCE00();
void CPL_DLL RegisterOGRMEM();
void CPL_DLL RegisterOGRVRT();
void CPL_DLL RegisterOGRDODS();
void CPL_DLL RegisterOGRSQLite();
void CPL_DLL RegisterOGRCSV();
void CPL_DLL RegisterOGRILI1();
void CPL_DLL RegisterOGRILI2();
void CPL_DLL RegisterOGRGRASS();
void CPL_DLL RegisterOGRPGeo();
void CPL_DLL RegisterOGRDXF();
void CPL_DLL RegisterOGRCAD();
void CPL_DLL RegisterOGRDWG();
void CPL_DLL RegisterOGRDGNV8();
void CPL_DLL RegisterOGRIDB();
void CPL_DLL RegisterOGRGMT();
void CPL_DLL RegisterOGRGPX();
void CPL_DLL RegisterOGRGeoconcept();
void CPL_DLL RegisterOGRNAS();
void CPL_DLL RegisterOGRGeoRSS();
void CPL_DLL RegisterOGRVFK();
void CPL_DLL RegisterOGRPGDump();
void CPL_DLL RegisterOGROSM();
void CPL_DLL RegisterOGRGPSBabel();
void CPL_DLL RegisterOGRPDS();
void CPL_DLL RegisterOGRWFS();
void CPL_DLL RegisterOGROAPIF();
void CPL_DLL RegisterOGRSOSI();
void CPL_DLL RegisterOGREDIGEO();
void CPL_DLL RegisterOGRSVG();
void CPL_DLL RegisterOGRIdrisi();
void CPL_DLL RegisterOGRXLS();
void CPL_DLL RegisterOGRODS();
void CPL_DLL RegisterOGRXLSX();
void CPL_DLL RegisterOGRElastic();
void CPL_DLL RegisterOGRGeoPackage();
void CPL_DLL RegisterOGRCarto();
void CPL_DLL RegisterOGRAmigoCloud();
void CPL_DLL RegisterOGRSXF();
void CPL_DLL RegisterOGROpenFileGDB();
void CPL_DLL RegisterOGRSelafin();
void CPL_DLL RegisterOGRJML();
void CPL_DLL RegisterOGRPLSCENES();
void CPL_DLL RegisterOGRCSW();
void CPL_DLL RegisterOGRMongoDBv3();
void CPL_DLL RegisterOGRVDV();
void CPL_DLL RegisterOGRGMLAS();
void CPL_DLL RegisterOGRMVT();
void CPL_DLL RegisterOGRNGW();
void CPL_DLL RegisterOGRMapML();
void CPL_DLL RegisterOGRLVBAG();
void CPL_DLL RegisterOGRHANA();
void CPL_DLL RegisterOGRParquet();
void CPL_DLL RegisterOGRArrow();
// @endcond

CPL_C_END

#endif /* ndef OGRSF_FRMTS_H_INCLUDED */
