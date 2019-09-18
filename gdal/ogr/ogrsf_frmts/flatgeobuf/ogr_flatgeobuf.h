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

static constexpr const uint8_t magicbytes[8] = { 0x66, 0x67, 0x62, 0x00, 0x66, 0x67, 0x62, 0x00 };

struct FeatureItem : Item {
    flatbuffers::DetachedBuffer buf;
    uint8_t *data;
    uint32_t size;
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

        OGRFeatureDefn *m_poFeatureDefn = nullptr;
        OGRSpatialReference *m_poSRS = nullptr;

        // iteration
        uint64_t m_featuresPos = 0;
        uint64_t m_featuresSize = 0;
        uint64_t m_offset = 0;
        uint64_t m_offsetInit = 0;
        uint64_t *m_featureOffsets = nullptr;
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
        bool bCreateSpatialIndexAtClose = true;
        bool bVerifyBuffers = true;
        bool bCanCreate = true;

        // deserialize
        void ensurePadfBuffers(size_t count);
        OGRErr parseFeature(OGRFeature *poFeature, OGRGeometry **ogrGeometry);
        OGRPoint *readPoint(const Feature *feature, uint32_t offset = 0);
        OGRMultiPoint *readMultiPoint(const Feature *feature, uint32_t len);
        OGRLineString *readLineString(const Feature *feature, uint32_t len, uint32_t offset = 0);
        OGRMultiLineString *readMultiLineString(const Feature *feature);
        OGRLinearRing *readLinearRing(const Feature *feature, uint32_t len, uint32_t offset = 0);
        OGRPolygon *readPolygon(const Feature *feature, uint32_t len, uint32_t offset = 0);
        OGRMultiPolygon *readMultiPolygon(const Feature *feature, uint32_t len);
        OGRGeometry *readGeometry(const Feature *feature);
        ColumnType toColumnType(OGRFieldType fieldType, OGRFieldSubType subType);
        static OGRFieldType toOGRFieldType(ColumnType type);
        const std::vector<flatbuffers::Offset<Column>> writeColumns(flatbuffers::FlatBufferBuilder &fbb);
        void readColumns();
        OGRErr querySpatialIndex();

        // serialize
        void Create();
        void writePoint(OGRPoint *p, GeometryContext &gc);
        void writeMultiPoint(OGRMultiPoint *mp, GeometryContext &gc);
        uint32_t writeLineString(OGRLineString *ls, GeometryContext &gc);
        void writeMultiLineString(OGRMultiLineString *mls, GeometryContext &gc);
        uint32_t writePolygon(OGRPolygon *p, GeometryContext &gc, bool isMulti, uint32_t end);
        void writeMultiPolygon(OGRMultiPolygon *mp, GeometryContext &gc);

        bool translateOGRwkbGeometryType();
        OGRwkbGeometryType getOGRwkbGeometryType();
    public:
        OGRFlatGeobufLayer(const Header *, GByte *headerBuf, const char *pszFilename, uint64_t offset);
        OGRFlatGeobufLayer(const char *pszLayerName, const char *pszFilename, OGRSpatialReference *poSpatialRef, OGRwkbGeometryType eGType);
        virtual ~OGRFlatGeobufLayer();

        virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;
        virtual OGRFeature *GetNextFeature() override;
        virtual OGRErr CreateField(OGRFieldDefn *poField, int bApproxOK = true) override;
        virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
        virtual int TestCapability(const char *) override;

        virtual void ResetReading() override;
        virtual OGRFeatureDefn *GetLayerDefn() override { return m_poFeatureDefn; }
        virtual GIntBig GetFeatureCount(int bForce) override;

        void CreateSpatialIndexAtClose( int bFlag ) { bCreateSpatialIndexAtClose = CPL_TO_BOOL(bFlag); }
        void VerifyBuffers( int bFlag ) { bVerifyBuffers = CPL_TO_BOOL(bFlag); }
};

class OGRFlatGeobufDataset final: public GDALDataset
{
    private:
        std::string m_osName;
        std::vector<std::unique_ptr<OGRLayer>> m_apoLayers;
        bool m_create = false;
    public:
        explicit OGRFlatGeobufDataset();
        explicit OGRFlatGeobufDataset(const char *pszName);
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
};

#endif /* ndef OGR_FLATGEOBUF_H_INCLUDED */

