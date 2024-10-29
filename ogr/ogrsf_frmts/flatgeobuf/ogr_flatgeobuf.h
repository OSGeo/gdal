/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Declaration of classes for OGR FlatGeobuf driver.
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2020, Björn Harrtell <bjorn at wololo dot org>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_FLATGEOBUF_H_INCLUDED
#define OGR_FLATGEOBUF_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "ogreditablelayer.h"

#include "header_generated.h"
#include "feature_generated.h"
#include "packedrtree.h"

#include <deque>
#include <limits>

class OGRFlatGeobufDataset;

static constexpr uint8_t magicbytes[8] = {0x66, 0x67, 0x62, 0x03,
                                          0x66, 0x67, 0x62, 0x01};

static constexpr uint32_t header_max_buffer_size = 1048576 * 10;

// Cannot be larger than that, due to a x2 logic done in ensureFeatureBuf()
static constexpr uint32_t feature_max_buffer_size =
    static_cast<uint32_t>(std::numeric_limits<int32_t>::max());

// holds feature meta needed to build spatial index
struct FeatureItem : FlatGeobuf::Item
{
    uint32_t size;
    uint64_t offset;
};

class OGRFlatGeobufBaseLayerInterface CPL_NON_FINAL
{
  public:
    virtual ~OGRFlatGeobufBaseLayerInterface();

    virtual const std::string &GetFilename() const = 0;
    virtual OGRLayer *GetLayer() = 0;
    virtual CPLErr Close() = 0;
};

class OGRFlatGeobufLayer final : public OGRLayer,
                                 public OGRFlatGeobufBaseLayerInterface
{
  private:
    std::string m_osFilename;
    std::string m_osLayerName;

    VSILFILE *m_poFp = nullptr;
    vsi_l_offset m_nFileSize = 0;

    const FlatGeobuf::Header *m_poHeader = nullptr;
    GByte *m_headerBuf = nullptr;
    OGRwkbGeometryType m_eGType;
    FlatGeobuf::GeometryType m_geometryType;
    bool m_hasM = false;
    bool m_hasZ = false;
    bool m_hasT = false;
    bool m_hasTM = false;
    uint64_t m_featuresCount = 0;
    OGREnvelope m_sExtent;
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    OGRSpatialReference *m_poSRS = nullptr;

    // iteration
    bool m_bEOF = false;
    size_t m_featuresPos = 0;       // current iteration position
    uint64_t m_offset = 0;          // current read offset
    uint64_t m_offsetFeatures = 0;  // offset of feature data
    std::vector<FlatGeobuf::SearchResultItem>
        m_foundItems;  // found node items in spatial index search
    bool m_queriedSpatialIndex = false;
    bool m_ignoreSpatialFilter = false;
    bool m_ignoreAttributeFilter = false;

    // creation
    GDALDataset *m_poDS = nullptr;  // parent dataset to get metadata from it
    bool m_create = false;
    std::deque<FeatureItem> m_featureItems;  // feature item description used to
                                             // create spatial index
    bool m_bCreateSpatialIndexAtClose = true;
    bool m_bVerifyBuffers = true;
    VSILFILE *m_poFpWrite = nullptr;
    CPLStringList m_aosCreationOption{};  // layer creation options
    uint64_t m_writeOffset = 0;           // current write offset
    uint64_t m_offsetAfterHeader =
        0;  // offset after dummy header writing (when creating a file without
            // spatial index)
    uint16_t m_indexNodeSize = 0;
    std::string
        m_osTempFile;  // holds generated temp file name for two pass writing
    uint32_t m_maxFeatureSize = 0;
    std::vector<uint8_t> m_writeProperties{};

    // shared
    GByte *m_featureBuf = nullptr;  // reusable/resizable feature data buffer
    uint32_t m_featureBufSize = 0;  // current feature buffer size

    // deserialize
    void ensurePadfBuffers(size_t count);
    OGRErr ensureFeatureBuf(uint32_t featureSize);
    OGRErr parseFeature(OGRFeature *poFeature);
    const std::vector<flatbuffers::Offset<FlatGeobuf::Column>>
    writeColumns(flatbuffers::FlatBufferBuilder &fbb);
    void readColumns();
    OGRErr readIndex();
    OGRErr readFeatureOffset(uint64_t index, uint64_t &featureOffset);

    // serialize
    bool CreateFinalFile();
    void writeHeader(VSILFILE *poFp, uint64_t featuresCount,
                     std::vector<double> *extentVector);

    // construction
    OGRFlatGeobufLayer(const FlatGeobuf::Header *, GByte *headerBuf,
                       const char *pszFilename, VSILFILE *poFp,
                       uint64_t offset);
    OGRFlatGeobufLayer(GDALDataset *poDS, const char *pszLayerName,
                       const char *pszFilename,
                       const OGRSpatialReference *poSpatialRef,
                       OGRwkbGeometryType eGType,
                       bool bCreateSpatialIndexAtClose, VSILFILE *poFpWrite,
                       std::string &osTempFile, CSLConstList papszOptions);

  protected:
    virtual int GetNextArrowArray(struct ArrowArrayStream *,
                                  struct ArrowArray *out_array) override;

    CPLErr Close() override;

  public:
    virtual ~OGRFlatGeobufLayer();

    static OGRFlatGeobufLayer *Open(const FlatGeobuf::Header *,
                                    GByte *headerBuf, const char *pszFilename,
                                    VSILFILE *poFp, uint64_t offset);
    static OGRFlatGeobufLayer *Open(const char *pszFilename, VSILFILE *fp,
                                    bool bVerifyBuffers);
    static OGRFlatGeobufLayer *
    Create(GDALDataset *poDS, const char *pszLayerName, const char *pszFilename,
           const OGRSpatialReference *poSpatialRef, OGRwkbGeometryType eGType,
           bool bCreateSpatialIndexAtClose, CSLConstList papszOptions);

    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = true) override;
    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    virtual int TestCapability(const char *) override;

    virtual void ResetReading() override;

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    virtual GIntBig GetFeatureCount(int bForce) override;
    virtual OGRErr GetExtent(OGREnvelope *psExtent, int bForce) override;

    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce) override
    {
        return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
    }

    void VerifyBuffers(int bFlag)
    {
        m_bVerifyBuffers = CPL_TO_BOOL(bFlag);
    }

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }

    const std::string &GetFilename() const override
    {
        return m_osFilename;
    }

    OGRLayer *GetLayer() override
    {
        return this;
    }

    static std::string GetTempFilePath(const CPLString &fileName,
                                       CSLConstList papszOptions);
    static VSILFILE *CreateOutputFile(const CPLString &pszFilename,
                                      CSLConstList papszOptions, bool isTemp);

    uint16_t GetIndexNodeSize() const
    {
        return m_indexNodeSize;
    }

    OGRwkbGeometryType getOGRwkbGeometryType();
};

class OGRFlatGeobufEditableLayer final : public OGREditableLayer,
                                         public OGRFlatGeobufBaseLayerInterface
{
  public:
    OGRFlatGeobufEditableLayer(OGRFlatGeobufLayer *poFlatGeobufLayer,
                               char **papszOpenOptions);

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;

    const std::string &GetFilename() const override
    {
        return static_cast<OGRFlatGeobufLayer *>(m_poDecoratedLayer)
            ->GetFilename();
    }

    OGRLayer *GetLayer() override
    {
        return this;
    }

    int TestCapability(const char *pszCap) override;

    CPLErr Close() override
    {
        return CE_None;
    }
};

class OGRFlatGeobufDataset final : public GDALDataset
{
  private:
    std::vector<std::unique_ptr<OGRFlatGeobufBaseLayerInterface>> m_apoLayers;
    bool m_bCreate = false;
    bool m_bUpdate = false;
    bool m_bIsDir = false;

    bool OpenFile(const char *pszFilename, VSILFILE *fp, bool bVerifyBuffers);

    CPLErr Close() override;

  public:
    OGRFlatGeobufDataset(const char *pszName, bool bIsDir, bool bCreate,
                         bool bUpdate);
    ~OGRFlatGeobufDataset();

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszName, CPL_UNUSED int nBands,
                               CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                               CPL_UNUSED GDALDataType eDT,
                               char **papszOptions);
    virtual OGRLayer *GetLayer(int) override;
    int TestCapability(const char *pszCap) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    virtual int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    char **GetFileList() override;
};

#endif /* ndef OGR_FLATGEOBUF_H_INCLUDED */
