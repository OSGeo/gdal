/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Definition of classes for OGR FlatGeobuf driver.
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2019, Björn Harrtell <bjorn at wololo dot org>
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

#ifndef OGR_FLATGEOBUF_H_INCLUDED
#define OGR_FLATGEOBUF_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include "header_generated.h"
#include "feature_generated.h"
#include "packedrtree.h"

using namespace FlatGeobuf;

class OGRFlatGeobufDataset;

static constexpr uint8_t magicbytes[8] = { 0x66, 0x67, 0x62, 0x00, 0x66, 0x67, 0x62, 0x00 };

static constexpr uint32_t header_max_buffer_size = 1048576;
static constexpr uint32_t feature_max_buffer_size = 2147483648 - 1;

struct FeatureItem : Item {
    uint32_t size;
    uint64_t offset;
};

struct GeometryContext {
    std::vector<double> xy;
    std::vector<double> z;
    std::vector<double> m;
    std::vector<uint32_t> ends;
    std::vector<uint32_t> lengths;
};

class OGRFlatGeobufLayer final : public OGRLayer
{
    private:
        std::string m_osFilename;
        std::string m_osLayerName;

        VSILFILE *m_poFp = nullptr;

        const Header *m_poHeader = nullptr;
        GByte *m_headerBuf = nullptr;
        OGRwkbGeometryType m_eGType;
        GeometryType m_geometryType;
        bool m_hasM = false;
        bool m_hasZ = false;
        bool m_hasT = false;
        bool m_hasTM = false;
        uint64_t m_featuresCount = 0;
        OGREnvelope m_sExtent;

        OGRFeatureDefn *m_poFeatureDefn = nullptr;
        OGRSpatialReference *m_poSRS = nullptr;

        // iteration
        uint64_t m_featuresPos = 0;
        uint64_t m_featuresSize = 0;
        uint64_t m_offset = 0;
        uint64_t m_offsetFeatures = 0;
        uint64_t m_offsetIndices = 0;
        std::vector<uint64_t> m_foundFeatureIndices;
        bool m_queriedSpatialIndex = false;
        bool m_ignoreSpatialFilter = false;
        bool m_ignoreAttributeFilter = false;

        // creation
        bool m_create = false;
        std::vector<std::shared_ptr<Item>> m_featureItems;
        GByte *m_featureBuf = nullptr;
        uint32_t m_featureSize = 0;
        uint32_t m_featureBufSize = 0;
        bool m_bCreateSpatialIndexAtClose = true;
        bool m_bVerifyBuffers = true;
        bool m_bCanCreate = true;
        VSILFILE *m_poFpWrite = nullptr;
        uint64_t m_writeOffset = 0;
        uint16_t m_indexNodeSize = 0;
        std::string m_oTempFile;

        // deserialize
        void ensurePadfBuffers(size_t count);
        OGRErr ensureFeatureBuf();
        OGRErr parseFeature(OGRFeature *poFeature);
        OGRPoint *readPoint(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t offset = 0);
        OGRMultiPoint *readMultiPoint(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len);
        OGRErr readSimpleCurve(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len, uint32_t offset, OGRSimpleCurve *c);
        OGRLineString *readLineString(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len, uint32_t offset = 0);
        OGRMultiLineString *readMultiLineString(const Feature *feature, const flatbuffers::Vector<double> &pXy);
        OGRLinearRing *readLinearRing(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len, uint32_t offset = 0);
        OGRPolygon *readPolygon(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len, uint32_t offset = 0);
        OGRMultiPolygon *readMultiPolygon(const Feature *feature, const flatbuffers::Vector<double> &pXy, uint32_t len);
        OGRGeometry *readGeometry(const Feature *feature);
        ColumnType toColumnType(OGRFieldType fieldType, OGRFieldSubType subType);
        static OGRFieldType toOGRFieldType(ColumnType type);
        const std::vector<flatbuffers::Offset<Column>> writeColumns(flatbuffers::FlatBufferBuilder &fbb);
        void readColumns();
        OGRErr readIndex();
        OGRErr readFeatureOffset(uint64_t index, uint64_t &featureOffset);

        // serialize
        void Create();
        void WriteHeader(VSILFILE *poFp, uint64_t featuresCount, std::vector<double> *extentVector);
        void writePoint(OGRPoint *p, GeometryContext &gc);
        void writeMultiPoint(OGRMultiPoint *mp, GeometryContext &gc);
        uint32_t writeLineString(OGRLineString *ls, GeometryContext &gc);
        void writeMultiLineString(OGRMultiLineString *mls, GeometryContext &gc);
        uint32_t writePolygon(OGRPolygon *p, GeometryContext &gc, bool isMulti, uint32_t end);
        void writeMultiPolygon(OGRMultiPolygon *mp, GeometryContext &gc);

        bool translateOGRwkbGeometryType();
        OGRwkbGeometryType getOGRwkbGeometryType();
    public:
        OGRFlatGeobufLayer(const Header *, GByte *headerBuf, const char *pszFilename, VSILFILE *poFp, uint64_t offset, uint64_t offsetIndices);
        OGRFlatGeobufLayer(const char *pszLayerName, const char *pszFilename, OGRSpatialReference *poSpatialRef, OGRwkbGeometryType eGType, VSILFILE *poFpWrite, std::string oTempFile, bool bCreateSpatialIndexAtClose);
        virtual ~OGRFlatGeobufLayer();

        virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;
        virtual OGRFeature *GetNextFeature() override;
        virtual OGRErr CreateField(OGRFieldDefn *poField, int bApproxOK = true) override;
        virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
        virtual int TestCapability(const char *) override;

        virtual void ResetReading() override;
        virtual OGRFeatureDefn *GetLayerDefn() override { return m_poFeatureDefn; }
        virtual GIntBig GetFeatureCount(int bForce) override;
        virtual OGRErr GetExtent(OGREnvelope* psExtent, int bForce) override;
        virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent,
                                       int bForce ) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

        void VerifyBuffers( int bFlag ) { m_bVerifyBuffers = CPL_TO_BOOL(bFlag); }

        const std::string& GetFilename() const { return m_osFilename; }
};

class OGRFlatGeobufDataset final: public GDALDataset
{
    private:
        std::vector<std::unique_ptr<OGRFlatGeobufLayer>> m_apoLayers;
        bool m_bCreate = false;
        bool m_bIsDir = false;

        bool OpenFile(const char* pszFilename, VSILFILE* fp, bool bVerifyBuffers);

    public:
        explicit OGRFlatGeobufDataset(const char *pszName, bool bIsDir, bool bCreate);
        ~OGRFlatGeobufDataset();

        static GDALDataset *Open(GDALOpenInfo*);
        static GDALDataset *Create( const char *pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions );
        virtual OGRLayer *GetLayer( int ) override;
        int TestCapability( const char *pszCap ) override;
        virtual OGRLayer *ICreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = nullptr,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char **papszOptions = nullptr ) override;

        virtual int GetLayerCount() override { return static_cast<int>(m_apoLayers.size()); }
        char** GetFileList() override;
};

#endif /* ndef OGR_FLATGEOBUF_H_INCLUDED */

