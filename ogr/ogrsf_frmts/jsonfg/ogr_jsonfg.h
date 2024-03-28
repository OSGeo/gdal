/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGC Features and Geometries JSON (JSON-FG)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
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

#ifndef OGR_JSONFG_H_INCLUDED
#define OGR_JSONFG_H_INCLUDED

#include "cpl_vsi_virtual.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogrgeojsonutils.h"
#include "ogrgeojsonwriter.h"
#include "ogrjsoncollectionstreamingparser.h"
#include "ogr_mem.h"
#include "directedacyclicgraph.hpp"

#include <map>
#include <set>
#include <utility>

/************************************************************************/
/*                         OGRJSONFGMemLayer                            */
/************************************************************************/

/** Layer with all features ingested into memory. */
class OGRJSONFGMemLayer final : public OGRMemLayer
{
  public:
    OGRJSONFGMemLayer(GDALDataset *poDS, const char *pszName,
                      OGRSpatialReference *poSRS, OGRwkbGeometryType eGType);
    ~OGRJSONFGMemLayer();

    const char *GetFIDColumn() override
    {
        return osFIDColumn_.c_str();
    }

    int TestCapability(const char *pszCap) override;

    void SetFIDColumn(const char *pszName)
    {
        osFIDColumn_ = pszName;
    }

    void AddFeature(std::unique_ptr<OGRFeature> poFeature);

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }

  private:
    GDALDataset *m_poDS = nullptr;
    std::string osFIDColumn_{};
    bool bOriginalIdModified_ = false;

    CPL_DISALLOW_COPY_ASSIGN(OGRJSONFGMemLayer)
};

/************************************************************************/
/*                    OGRJSONFGStreamedLayer                            */
/************************************************************************/

class OGRJSONFGStreamingParser;

/** Layer with features being acquired progressively through a streaming
 parser.

 Only applies for FeatureCollection read through a file
*/
class OGRJSONFGStreamedLayer final
    : public OGRLayer,
      public OGRGetNextFeatureThroughRaw<OGRJSONFGStreamedLayer>
{
  public:
    OGRJSONFGStreamedLayer(GDALDataset *poDS, const char *pszName,
                           OGRSpatialReference *poSRS,
                           OGRwkbGeometryType eGType);
    ~OGRJSONFGStreamedLayer();

    // BEGIN specific public API

    //! Set the FID column name
    void SetFIDColumn(const char *pszName)
    {
        osFIDColumn_ = pszName;
    }

    //! Set the total feature count
    void SetFeatureCount(GIntBig nCount)
    {
        nFeatureCount_ = nCount;
    }

    /** Set the file handle.

     Must be called before GetNextFeature() is called
     */
    void SetFile(VSIVirtualHandleUniquePtr &&poFile);

    /** Set the streaming parser

     Must be called before GetNextFeature() is called
     */
    void SetStreamingParser(
        std::unique_ptr<OGRJSONFGStreamingParser> &&poStreamingParser);

    // END specific public API

    const char *GetFIDColumn() override
    {
        return osFIDColumn_.c_str();
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn_;
    }

    int TestCapability(const char *pszCap) override;

    GIntBig GetFeatureCount(int bForce) override;

    void ResetReading() override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRJSONFGStreamedLayer)

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }

  private:
    GDALDataset *m_poDS = nullptr;
    OGRFeatureDefn *poFeatureDefn_ = nullptr;
    std::string osFIDColumn_{};

    /** Total number of features. */
    GIntBig nFeatureCount_ = -1;

    VSIVirtualHandleUniquePtr poFile_{};

    std::unique_ptr<OGRJSONFGStreamingParser> poStreamingParser_{};

    /** Whether a warning has been emitted about feature IDs having been
     * modified */
    bool bOriginalIdModified_ = false;
    /** Set of feature IDs read/allocated up to that point */
    std::set<GIntBig> oSetUsedFIDs_{};

    /** Ensure the FID of the feature is unique */
    OGRFeature *EnsureUniqueFID(OGRFeature *poFeat);

    /** Return next feature (without filter) */
    OGRFeature *GetNextRawFeature();

    CPL_DISALLOW_COPY_ASSIGN(OGRJSONFGStreamedLayer)
};

/************************************************************************/
/*                         OGRJSONFGWriteLayer                          */
/************************************************************************/

class OGRJSONFGDataset;

class OGRJSONFGWriteLayer final : public OGRLayer
{
  public:
    OGRJSONFGWriteLayer(
        const char *pszName, const OGRSpatialReference *poSRS,
        std::unique_ptr<OGRCoordinateTransformation> &&poCTToWGS84,
        const std::string &osCoordRefSys, OGRwkbGeometryType eGType,
        CSLConstList papszOptions, OGRJSONFGDataset *poDS);
    ~OGRJSONFGWriteLayer();

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

    OGRErr SyncToDisk() override;

    GDALDataset *GetDataset() override;

  private:
    OGRJSONFGDataset *poDS_{};
    OGRFeatureDefn *poFeatureDefn_ = nullptr;
    std::unique_ptr<OGRCoordinateTransformation> poCTToWGS84_;
    bool bIsWGS84CRS_ = false;
    bool m_bMustSwapForPlace = false;
    int nOutCounter_ = 0;
    std::string osCoordRefSys_{};

    OGRGeoJSONWriteOptions oWriteOptions_{};
    OGRGeoJSONWriteOptions oWriteOptionsPlace_{};
    bool bWriteFallbackGeometry_ = true;

    CPL_DISALLOW_COPY_ASSIGN(OGRJSONFGWriteLayer)
};

/************************************************************************/
/*                           OGRJSONFGDataset                           */
/************************************************************************/

class OGRJSONFGReader;

class OGRJSONFGDataset final : public GDALDataset
{
  public:
    OGRJSONFGDataset() = default;
    ~OGRJSONFGDataset();

    bool Open(GDALOpenInfo *poOpenInfo, GeoJSONSourceType nSrcType);
    bool Create(const char *pszName, CSLConstList papszOptions);

    int GetLayerCount() override
    {
        return static_cast<int>(apoLayers_.size());
    }

    OGRLayer *GetLayer(int i) override;

    //! Return the output file handle. Used by OGRJSONFGWriteLayer
    VSILFILE *GetOutputFile() const
    {
        return fpOut_;
    }

    /** Return whether there is a single output layer.
     * Used by OGRJSONFGWriteLayer
     */
    bool IsSingleOutputLayer() const
    {
        return bSingleOutputLayer_;
    }

    //! Return whether the output file is seekable
    bool GetFpOutputIsSeekable() const
    {
        return bFpOutputIsSeekable_;
    }

    void BeforeCreateFeature();

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *pszCap) override;

    OGRErr SyncToDiskInternal();

  protected:
    friend class OGRJSONFGReader;
    OGRJSONFGMemLayer *AddLayer(std::unique_ptr<OGRJSONFGMemLayer> &&poLayer);
    OGRJSONFGStreamedLayer *
    AddLayer(std::unique_ptr<OGRJSONFGStreamedLayer> &&poLayer);

  private:
    char *pszGeoData_ = nullptr;
    size_t nGeoDataLen_ = 0;
    std::vector<std::unique_ptr<OGRLayer>> apoLayers_{};
    std::unique_ptr<OGRJSONFGReader> poReader_{};

    // Write side
    VSILFILE *fpOut_ = nullptr;
    bool bSingleOutputLayer_ = false;
    bool bHasEmittedFeatures_ = false;
    bool bFpOutputIsSeekable_ = false;

    /** Offset at which the '] }' terminating sequence has already been
     * written by SyncToDisk(). 0 if it has not been written.
     */
    vsi_l_offset m_nPositionBeforeFCClosed = 0;

    bool ReadFromFile(GDALOpenInfo *poOpenInfo, const char *pszUnprefixed);
    bool ReadFromService(GDALOpenInfo *poOpenInfo, const char *pszSource);

    void FinishWriting();

    bool EmitStartFeaturesIfNeededAndReturnIfFirstFeature();

    CPL_DISALLOW_COPY_ASSIGN(OGRJSONFGDataset)
};

/************************************************************************/
/*                          OGRJSONFGReader                             */
/************************************************************************/

class OGRJSONFGReader
{
  public:
    OGRJSONFGReader() = default;
    ~OGRJSONFGReader();

    /** Load all features from the passed in JSON text in OGRJSONFGMemLayer(s)
     *
     * This method should only be called once, and is exclusive with
     * AnalyzeWithStreamingParser()
     */
    bool Load(OGRJSONFGDataset *poDS, const char *pszText,
              const std::string &osDefaultLayerName);

    /** Do a first pass analysis of the content of the passed file to create
     * OGRJSONFGStreamedLayer's
     *
     * It is the responsibility of the caller to call
     * SetFile() and SetStreamingParser() on the created layers afterwards
     *
     * This method should only be called once, and is exclusive with
     * Load()
     */
    bool AnalyzeWithStreamingParser(OGRJSONFGDataset *poDS, VSILFILE *fp,
                                    const std::string &osDefaultLayerName,
                                    bool &bCanTryWithNonStreamingParserOut);

    /** Geometry element we are interested in. */
    enum class GeometryElement
    {
        /** Use "place" when possible, fallback to "geometry" otherwise. */
        AUTO,
        /** Only use "place" */
        PLACE,
        /** Only use "geometry" */
        GEOMETRY,
    };

    /** Sets the geometry element we are interested in. */
    void SetGeometryElement(GeometryElement elt)
    {
        eGeometryElement_ = elt;
    }

    /** Returns a OGRFeature built from the passed in JSON object.
     *
     * @param poObj JSON feature
     * @param pszRequestedLayer name of the layer of interest, or nullptr if
     * no filtering needed on the layer name. If the feature does not belong
     * to the requested layer, nullptr is returned.
     * @param pOutMemLayer Pointer to the OGRJSONFGMemLayer* layer to which
     * the returned feature belongs to. May be nullptr. Only applies when
     * the Load() method has been used.
     * @param pOutStreamedLayer Pointer to the OGRJSONFGStreamedLayer* layer to
     * which the returned feature belongs to. May be nullptr. Only applies when
     * the AnalyzeWithStreamingParser() method has been used.
     */
    std::unique_ptr<OGRFeature>
    ReadFeature(json_object *poObj, const char *pszRequestedLayer,
                OGRJSONFGMemLayer **pOutMemLayer,
                OGRJSONFGStreamedLayer **pOutStreamedLayer);

  protected:
    friend class OGRJSONFGStreamingParser;

    bool GenerateLayerDefnFromFeature(json_object *poObj);

  private:
    GeometryElement eGeometryElement_ = GeometryElement::AUTO;

    OGRJSONFGDataset *poDS_ = nullptr;
    std::string osDefaultLayerName_{};
    json_object *poObject_ = nullptr;

    bool bFlattenNestedAttributes_ = false;
    char chNestedAttributeSeparator_ = 0;
    bool bArrayAsString_ = false;
    bool bDateAsString_ = false;

    /** Layer building context, specific to one layer. */
    struct LayerDefnBuildContext
    {
        //! Maps a field name to its index in apoFieldDefn[]
        std::map<std::string, int> oMapFieldNameToIdx{};

        //! Vector of OGRFieldDefn
        std::vector<std::unique_ptr<OGRFieldDefn>> apoFieldDefn{};

        //! Directed acyclic graph used to build the order of fields.
        gdal::DirectedAcyclicGraph<int, std::string> dag{};

        /** Set of indices of apoFieldDefn[] for which no type information is
         * known yet. */
        std::set<int> aoSetUndeterminedTypeFields{};

        //! Whether at least one feature has a "coordRefSys" member.
        bool bHasCoordRefSysAtFeatureLevel = false;

        /** CRS object corresponding to "coordRefsys" member at feature level.
         * Only set if homogeneous among features.
         */
        std::unique_ptr<OGRSpatialReference> poCRSAtFeatureLevel{};

        /** Serialized JSON value of "coordRefsys" member at feature level.
         * Only set if homogeneous among features.
         */
        std::string osCoordRefSysAtFeatureLevel{};

        /** Whether to switch X/Y ordinates in geometries appearing in "place"
         * element. Only applies to CRS at layer level.
         */
        bool bSwapPlacesXY = false;

        //! Whether the layer CRS is WGS 84.
        bool bLayerCRSIsWGS84 = false;

        //! Coordinate transformation from WGS 84 to layer CRS (might be null)
        std::unique_ptr<OGRCoordinateTransformation> poCTWGS84ToLayerCRS{};

        /** Feature count */
        GIntBig nFeatureCount = 0;

        //! Whether the Feature.id should be mapped to a OGR field.
        bool bFeatureLevelIdAsAttribute = false;

        //! Whether the Feature.id should be mapped to a OGR FID.
        bool bFeatureLevelIdAsFID = false;

        //! Whether 64-bit integers are needed for OGR FID.
        bool bNeedFID64 = false;

        //! Whether detection of layer geometry type is still needed.
        bool bDetectLayerGeomType = true;

        //! Whether no geometry has been analyzed yet.
        bool bFirstGeometry = true;

        //! Layer geometry type.
        OGRwkbGeometryType eLayerGeomType = wkbUnknown;

        //! Whether a Feature.time.date element has been found.
        bool bHasTimeDate = false;

        //! Whether a Feature.time.timestamp element has been found.
        bool bHasTimeTimestamp = false;

        /** Whether a Feature.time.interval[0] element of type timestamp has
         * been found */
        bool bHasTimeIntervalStartTimestamp = false;

        /** Whether a Feature.time.interval[0] element of type date has
         * been found */
        bool bHasTimeIntervalStartDate = false;

        /** Whether a Feature.time.interval[1] element of type timestamp has
         * been found */
        bool bHasTimeIntervalEndTimestamp = false;

        /** Whether a Feature.time.interval[1] element of type date has
         * been found */
        bool bHasTimeIntervalEndDate = false;

        //! Index of OGR field "time" / "jsonfg_time"
        int nIdxFieldTime = -1;

        //! Index of OGR field "time_start" / "jsonfg_time_start"
        int nIdxFieldTimeStart = -1;

        //! Index of OGR field "time_end" / "jsonfg_time_end"
        int nIdxFieldTimeEnd = -1;

        //! Corresponding OGRJSONFGMemLayer (only for Load() ingestion mode)
        OGRJSONFGMemLayer *poMemLayer = nullptr;

        /** Corresponding OGRJSONFGStreamedLayer(only for
         * AnalyzeWithStreamingParser() mode) */
        OGRJSONFGStreamedLayer *poStreamedLayer = nullptr;

        LayerDefnBuildContext() = default;
        LayerDefnBuildContext(LayerDefnBuildContext &&) = default;
        LayerDefnBuildContext &operator=(LayerDefnBuildContext &&) = default;

      private:
        CPL_DISALLOW_COPY_ASSIGN(LayerDefnBuildContext)
    };

    //! Maps a layer name to its build context
    std::map<std::string, LayerDefnBuildContext> oMapBuildContext_{};

    //
    // Copy operations not supported.
    //
    CPL_DISALLOW_COPY_ASSIGN(OGRJSONFGReader)

    const char *GetLayerNameForFeature(json_object *poObj) const;
    bool GenerateLayerDefns();
    bool FinalizeGenerateLayerDefns(bool bStreamedLayer);
    void FinalizeBuildContext(LayerDefnBuildContext &oBuildContext,
                              const char *pszLayerName, bool bStreamedLayer,
                              bool bInvalidCRS, bool bSwapPlacesXYTopLevel,
                              OGRSpatialReference *poSRSTopLevel);
};

/************************************************************************/
/*                      OGRJSONFGStreamingParser                        */
/************************************************************************/

/** FeatureCollection streaming parser. */
class OGRJSONFGStreamingParser final : public OGRJSONCollectionStreamingParser
{
    OGRJSONFGReader &m_oReader;
    std::string m_osRequestedLayer{};

    std::vector<std::pair<std::unique_ptr<OGRFeature>, OGRLayer *>>
        m_apoFeatures{};
    size_t m_nCurFeatureIdx = 0;

    CPL_DISALLOW_COPY_ASSIGN(OGRJSONFGStreamingParser)

  protected:
    void GotFeature(json_object *poObj, bool bFirstPass,
                    const std::string &osJson) override;
    void TooComplex() override;

  public:
    OGRJSONFGStreamingParser(OGRJSONFGReader &oReader, bool bFirstPass);
    ~OGRJSONFGStreamingParser();

    void SetRequestedLayer(const char *pszRequestedLayer)
    {
        m_osRequestedLayer = pszRequestedLayer;
    }

    std::unique_ptr<OGRJSONFGStreamingParser> Clone();

    std::pair<std::unique_ptr<OGRFeature>, OGRLayer *> GetNextFeature();
};

bool OGRJSONFGMustSwapXY(const OGRSpatialReference *poSRS);

#endif  // OGR_JSONFG_H_INCLUDED
