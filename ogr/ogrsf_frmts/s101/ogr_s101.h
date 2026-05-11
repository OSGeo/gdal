/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Header file
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_S101_H_INCLUDED
#define OGR_S101_H_INCLUDED

#include "ogrsf_frmts.h"
#include "iso8211.h"
#include "ddfrecordindex.h"

#include "cpl_int_wrapper.h"
#include "cpl_string.h"

#include <map>
#include <set>
#include <string_view>

/************************************************************************/
/*                            OGRS101Reader                             */
/************************************************************************/

class OGRS101FeatureCatalog;

namespace OGRS101FeatureCatalogTypes
{
struct InformationType;
struct FeatureType;
}  // namespace OGRS101FeatureCatalogTypes

class OGRS101Reader
{
  public:
    struct CRSIdTag
    {
    };

    using CRSId = cpl::IntWrapper<CRSIdTag>;

    static constexpr CRSId INVALID_CRS_ID{-1};
    static constexpr CRSId HORIZONTAL_CRS_ID{1};

    struct RecordNameTag
    {
    };

    // Record "name" is a poor naming from the S100 spec. It is actually
    // a numeric value
    using RecordName = cpl::IntWrapper<RecordNameTag>;

    // Type for NATC subfield (numeric attribute code)
    struct AttrCodeTag
    {
    };

    using AttrCode = cpl::IntWrapper<AttrCodeTag>;

    // Type for PAIX subfield (parent index)
    struct AttrIndexTag
    {
    };

    using AttrIndex = cpl::IntWrapper<AttrIndexTag>;

    // Type for ATIX subfield (attribute index)
    struct AttrRepeatTag
    {
    };

    using AttrRepeat = cpl::IntWrapper<AttrRepeatTag>;

    inline static void AppendUInt8(std::string &s, uint8_t x)
    {
        s.append(reinterpret_cast<const char *>(&x), sizeof(x));
    }

    inline static void AppendUInt16(std::string &s, uint16_t x)
    {
        CPL_LSBPTR16(&x);
        s.append(reinterpret_cast<const char *>(&x), sizeof(x));
    }

    inline static void AppendInt32(std::string &s, int32_t x)
    {
        CPL_LSBPTR32(&x);
        s.append(reinterpret_cast<const char *>(&x), sizeof(x));
    }

  private:
    /////////////////////////////////////////////////////////////////////////
    // Members
    /////////////////////////////////////////////////////////////////////////

    bool m_bStrict = true;

    //! Whereas we are currently processing an update file (.001, .002, ...)
    bool m_bInUpdate = false;

    //! Whether an update file cancels the dataset.
    bool m_bCancelled = false;

    std::string m_osFilename{};
    DDFModule m_oMainModule{};

    CPLStringList m_aosMetadata{};

    // DSSI field
    static constexpr double S101_SHIFT = 0;
    double m_dfXShift = S101_SHIFT;  // DCOX
    double m_dfYShift = S101_SHIFT;  // DCOY
    double m_dfZShift = S101_SHIFT;  // DCOZ
    static constexpr int S101_XSCALE = 10000000;
    int m_nXScale = S101_XSCALE;  // CMFX
    static constexpr int S101_YSCALE = 10000000;
    int m_nYScale = S101_YSCALE;  // CMFY
    static constexpr int S101_ZSCALE = 10;
    int m_nZScale = S101_ZSCALE;           // CMFZ
    int m_nCountInformationRecord = 0;     // NOIR
    int m_nCountPointRecord = 0;           // NOPN
    int m_nCountMultiPointRecord = 0;      // NOMN
    int m_nCountCurveRecord = 0;           // NOCN
    int m_nCountCompositeCurveRecord = 0;  // NOXN
    int m_nCountSurfaceRecord = 0;         // NOSN
    int m_nCountFeatureTypeRecord = 0;     // NOFR

    // from ATCS field
    std::map<AttrCode, std::string> m_attributeCodes{};

    // Maps the AttrCode of the current update to the consolidated one of
    // m_attributeCodes
    std::map<AttrCode, AttrCode> m_attributeCodesRemapping{};

    struct InfoTypeCodeTag
    {
    };

    using InfoTypeCode = cpl::IntWrapper<InfoTypeCodeTag>;
    // from ITCS field
    std::map<InfoTypeCode, std::string> m_informationTypeCodes{};

    // Maps the AttrCode of the current update to the consolidated one of
    // m_informationTypeCodes
    std::map<InfoTypeCode, InfoTypeCode> m_informationTypeCodesRemapping{};

    struct FeatureTypeCodeTag
    {
    };

    using FeatureTypeCode = cpl::IntWrapper<FeatureTypeCodeTag>;
    // from FTCS field
    std::map<FeatureTypeCode, std::string> m_featureTypeCodes{};

    // Maps the AttrCode of the current update to the consolidated one of
    // m_featureTypeCodes
    std::map<FeatureTypeCode, FeatureTypeCode> m_featureTypeCodesRemapping{};

    struct InfoAssocCodeTag
    {
    };

    using InfoAssocCode = cpl::IntWrapper<InfoAssocCodeTag>;
    // from IACS field
    std::map<InfoAssocCode, std::string> m_informationAssociationCodes{};

    // Maps the AttrCode of the current update to the consolidated one of
    // m_informationAssociationCodes
    std::map<InfoAssocCode, InfoAssocCode>
        m_informationAssociationCodesRemapping{};

    struct FeatureAssocCodeTag
    {
    };

    using FeatureAssocCode = cpl::IntWrapper<FeatureAssocCodeTag>;
    // from FACS field
    std::map<FeatureAssocCode, std::string> m_featureAssociationCodes{};

    // Maps the AttrCode of the current update to the consolidated one of
    // m_featureAssociationCodes
    std::map<FeatureAssocCode, FeatureAssocCode>
        m_featureAssociationCodesRemapping{};

    struct AssocRoleCodeTag
    {
    };

    using AssocRoleCode = cpl::IntWrapper<AssocRoleCodeTag>;
    // from ARCS field
    std::map<AssocRoleCode, std::string> m_associationRoleCodes{};

    // Maps the AttrCode of the current update to the consolidated one of
    // m_featureAssociationCodes
    std::map<AssocRoleCode, AssocRoleCode> m_associationRoleCodesRemapping{};

    static constexpr RecordName PSEUDO_RECORD_NAME_NO_GEOM{-1111111111};

    /** Triple (feature type code, geometry type, CRS Id) */
    struct FeatureTypeKey
    {
        FeatureTypeCode nFeatureTypeCode{0};
        RecordName nGeometryType{PSEUDO_RECORD_NAME_NO_GEOM};
        CRSId nCRSId{INVALID_CRS_ID};

        // I'd wish I could use C++20
        inline bool operator<(const FeatureTypeKey &other) const
        {
            return toTuple() < other.toTuple();
        }

      private:
        inline std::tuple<int, int, int> toTuple() const
        {
            return std::make_tuple(static_cast<int>(nFeatureTypeCode),
                                   static_cast<int>(nGeometryType),
                                   static_cast<int>(nCRSId));
        }
    };

    // key is the crs index (CRIX). 1: Horizontal CRS. >= 2: CompoundCRS
    std::map<CRSId, OGRSpatialReference> m_oMapSRS{};

    DDFRecordIndex m_oInformationTypeRecordIndex{};
    DDFRecordIndex m_oPointRecordIndex{};
    DDFRecordIndex m_oMultiPointRecordIndex{};
    DDFRecordIndex m_oCurveRecordIndex{};
    DDFRecordIndex m_oCompositeCurveRecordIndex{};
    DDFRecordIndex m_oSurfaceRecordIndex{};
    DDFRecordIndex m_oFeatureTypeRecordIndex{};

    OGRFeatureDefnRefCountedPtr m_poFeatureDefnInformationType{};

    std::map<CRSId, OGRFeatureDefnRefCountedPtr> m_oMapPointFeatureDefn{};
    std::map<CRSId, std::vector<int>> m_oMapCRSIdToPointRecordIdx{};

    std::map<CRSId, OGRFeatureDefnRefCountedPtr> m_oMapMultiPointFeatureDefn{};
    std::map<CRSId, std::vector<int>> m_oMapCRSIdToMultiPointRecordIdx{};

    OGRFeatureDefnRefCountedPtr m_poFeatureDefnCurve{};
    OGRFeatureDefnRefCountedPtr m_poFeatureDefnCompositeCurve{};
    OGRFeatureDefnRefCountedPtr m_poFeatureDefnSurface{};

    struct LayerDef
    {
        OGRFeatureDefnRefCountedPtr poFeatureDefn{};
        std::string osName{};
        std::string osDefinition{};
        std::string osAlias{};
        std::vector<int> anRecordIndices{};
    };

    std::map<FeatureTypeKey, LayerDef> m_oMapFeatureKeyToLayerDef{};
    std::map<int, const OGRFeatureDefn *> m_oMapFeatureTypeIdToFDefn{};

    std::map<std::string, std::unique_ptr<OGRFieldDomain>> m_oMapFieldDomains{};

    const OGRS101FeatureCatalog *m_poFeatureCatalog = nullptr;

    /////////////////////////////////////////////////////////////////////////
    // Methods
    /////////////////////////////////////////////////////////////////////////

    bool Load(const std::string &osFilename, VSILFILE *fp,
              DDFModule *poCurModule);

    bool CheckFieldDefinitions(const DDFModule *poCurModule) const;
    bool CheckField0000Definition(const DDFModule *poCurModule) const;

    bool ReadDatasetGeneralInformationRecord(const DDFRecord *poRecord);
    bool ReadDSID(const DDFRecord *poRecord);
    bool ReadDSSI(const DDFRecord *poRecord);

    template <class CodeType>
    bool ReadGenericCodeAssociation(
        const DDFRecord *poRecord, const char *pszFieldName,
        const char *pszSubField0Name, const char *pszSubField1Name,
        std::map<CodeType, std::string> &map,
        std::map<CodeType, CodeType> &mapRemapping) const;

    bool ReadATCS(const DDFRecord *poRecord);
    bool ReadITCS(const DDFRecord *poRecord);
    bool ReadFTCS(const DDFRecord *poRecord);
    bool ReadIACS(const DDFRecord *poRecord);
    bool ReadFACS(const DDFRecord *poRecord);
    bool ReadARCS(const DDFRecord *poRecord);

    bool ReadCSID(const DDFRecord *poRecord);

    bool IngestInitialRecords(const DDFRecord *poRecordIn);
    bool IngestUpdateRecords(const DDFRecord *poRecordIn,
                             DDFModule *poCurModule);
    bool UpdateCodesInRecord(DDFRecord *poRecord) const;
    bool ProcessUpdateRecord(const DDFRecord *poUpdateRecord,
                             DDFRecord *poTargetRecord) const;
    bool ProcessUpdateATTR(const DDFRecord *poUpdateRecord,
                           DDFRecord *poTargetRecord) const;
    bool ProcessUpdateINASOrFASC(const DDFRecord *poUpdateRecord,
                                 DDFRecord *poTargetRecord,
                                 const char *pszFieldName) const;
    bool ProcessUpdateAttributeLikeField(const DDFRecord *poUpdateRecord,
                                         const DDFField *poUpdateField,
                                         DDFRecord *poTargetRecord,
                                         DDFField *poTargetField,
                                         int iFieldInstance) const;
    bool ProcessUpdateRecordPoint(const DDFRecord *poUpdateRecord,
                                  DDFRecord *poTargetRecord) const;
    bool ProcessUpdatePointList(const DDFRecord *poUpdateRecord,
                                DDFRecord *poTargetRecord,
                                bool bIs3DAllowed) const;
    bool ProcessUpdateRecordMultiPoint(const DDFRecord *poUpdateRecord,
                                       DDFRecord *poTargetRecord) const;
    bool ProcessUpdateRecordCurve(const DDFRecord *poUpdateRecord,
                                  DDFRecord *poTargetRecord) const;
    bool ProcessUpdateRecordCompositeCurve(const DDFRecord *poUpdateRecord,
                                           DDFRecord *poTargetRecord) const;
    bool ProcessUpdateRecordSurface(const DDFRecord *poUpdateRecord,
                                    DDFRecord *poTargetRecord) const;
    bool ProcessUpdateRecordFeatureType(const DDFRecord *poUpdateRecord,
                                        DDFRecord *poTargetRecord) const;

    using PathElement = std::pair<AttrCode, AttrRepeat>;
    using PathVector = std::vector<PathElement>;

    // Capture essential information for a repetition of the ATTR field
    struct S101AttrDef
    {
        PathVector oReversedPath{};  // ordered from child to root
        std::string osVal{};
        int iField = 0;
        bool bMultipleFields = false;
        bool bIsParent = false;
    };

    bool ReadFeatureCatalog();

    std::string BuildFieldName(const PathVector &oReversedPath,
                               const char *pszAttrFieldName, int iField,
                               bool bMultipleFields,
                               const char *pszIDFieldName) const;

    bool CreateInformationTypeFeatureDefn();
    bool CreatePointFeatureDefns();
    bool CreateMultiPointFeatureDefns();
    bool CreateCurveFeatureDefn();
    bool CreateCompositeCurveFeatureDefn();
    bool CreateSurfaceFeatureDefn();
    bool CreateFeatureTypeFeatureDefns();

    bool InferFeatureDefn(
        const DDFRecordIndex &oIndex, const char *pszIDFieldName,
        const char *pszAttrFieldName, const std::vector<int> &anRecordIndices,
        OGRFeatureDefn &oFeatureDefn,
        std::map<std::string, std::unique_ptr<OGRFieldDomain>>
            &oMapFieldDomains,
        const OGRS101FeatureCatalogTypes::InformationType *psInformationType =
            nullptr,
        const OGRS101FeatureCatalogTypes::FeatureType *psFeatureType =
            nullptr) const;

    using NameOccMinOccMax = std::tuple<const char *, int, int>;
    bool CheckFieldDefinitions(
        const DDFRecord *poRecord, int iRecord, RecordName nRCNM, int nRCID,
        const std::map<RecordName, std::vector<std::vector<NameOccMinOccMax>>>
            &mapExpectedFields) const;

    bool IngestAttributes(const DDFRecord *poRecord, int iRecord,
                          const char *pszIDFieldName,
                          const char *pszAttrFieldName,
                          const DDFField *poATTRField, int iField,
                          bool bMultipleFields,
                          std::vector<S101AttrDef> &asS101AttrDefs) const;

    bool IngestAttributes(const DDFRecord *poRecord, int iRecord,
                          const char *pszIDFieldName,
                          const char *pszAttrFieldName,
                          std::vector<S101AttrDef> &asS101AttrDefs) const;

    bool FillFeatureAttributes(const DDFRecordIndex &oIndex, int iRecord,
                               const char *pszAttrFieldName,
                               OGRFeature &oFeature) const;

    CRSId GetCRSIdForPointRecord(const DDFRecord *poRecord, int iRecord,
                                 int nRecordID) const;

    std::map<OGRS101Reader::CRSId, std::vector<int>>
    CreateMapCRSIdToRecordIdxForPoints(bool &bError) const;

    CRSId GetCRSIdForMultiPointRecord(const DDFRecord *poRecord, int iRecord,
                                      int nRecordID) const;

    std::map<OGRS101Reader::CRSId, std::vector<int>>
    CreateMapCRSIdToRecordIdxForMultiPoints(bool &bError) const;

    bool FillFeatureWithNonAttrAssocSubfields(const DDFRecord *poRecord,
                                              int iRecord,
                                              const char *pszAttrFieldName,
                                              OGRFeature &oFeature) const;

    std::unique_ptr<OGRPoint> ReadPointGeometryInternal(
        const DDFRecord *poRecord, int iRecord, int nRecordID, int iPnt,
        const OGRSpatialReference *poSRS, const bool bIs3D,
        const DDFField *poCoordField, const char *pszRecordFieldName) const;

    std::unique_ptr<OGRPoint>
    ReadPointGeometry(const DDFRecord *poRecord, int iRecord, int nRecordID,
                      const OGRSpatialReference *poSRS) const;

    std::unique_ptr<OGRMultiPoint>
    ReadMultiPointGeometry(const DDFRecord *poRecord, int iRecord,
                           int nRecordID,
                           const OGRSpatialReference *poSRS) const;

    std::unique_ptr<OGRLineString>
    ReadCurveGeometry(const DDFRecord *poRecord, int iRecord, int nRecordID,
                      const OGRSpatialReference *poSRS) const;

    std::unique_ptr<OGRLineString> ReadCompositeCurveGeometryInternal(
        const DDFRecord *poRecord, int iRecord, int nRecordID,
        const OGRSpatialReference *poSRS,
        std::set<int> &oSetAlreadyVisitedCompositeCurveRecords) const;

    std::unique_ptr<OGRLineString>
    ReadCompositeCurveGeometry(const DDFRecord *poRecord, int iRecord,
                               int nRecordID,
                               const OGRSpatialReference *poSRS) const;

    std::unique_ptr<OGRPolygon>
    ReadSurfaceGeometry(const DDFRecord *poRecord, int iRecord, int nRecordID,
                        const OGRSpatialReference *poSRS) const;

    template <typename T, typename GeomReaderMethodType>
    bool ReadGeometry(const DDFRecordIndex &oIndex, const char *pszErrorContext,
                      int nGeomRecordID, const char *pszGeomType, bool bReverse,
                      OGRFeature &oFeature,
                      std::unique_ptr<OGRGeometryCollection> &poMultiGeom,
                      GeomReaderMethodType geomReaderMethod,
                      int iGeomField = 0) const;

    bool FillFeatureTypeGeometry(const DDFRecord *poRecord, int iRecord,
                                 OGRFeature &oFeature) const;

    bool FillFeatureTypeMask(const DDFRecord *poRecord, int iRecord,
                             OGRFeature &oFeature) const;

    static std::string LaunderCRSName(const OGRSpatialReference &oSRS);
    static std::string GetPointLayerName(const OGRSpatialReference &oSRS);
    static std::string GetMultiPointLayerName(const OGRSpatialReference &oSRS);

    OGRS101Reader(const OGRS101Reader &) = delete;
    OGRS101Reader &operator=(const OGRS101Reader &) = delete;

  protected:
    friend class OGRS101FeatureCatalog;
    static bool EmitErrorOrWarning(const char *pszFile, const char *pszFunc,
                                   int nLine, const char *pszMsg, bool bError,
                                   bool bRecoverable);

  public:
    OGRS101Reader();
    ~OGRS101Reader();

    bool Load(GDALOpenInfo *poOpenInfo);

    /** Return whether the dataset is cancelled. */
    bool IsCancelled() const
    {
        return m_bCancelled;
    }

    /** Return feature catalog, or null. */
    const OGRS101FeatureCatalog *GetFeatureCatalog() const
    {
        return m_poFeatureCatalog;
    }

    /** Return dataset metadata from the DSID field. */
    const CPLStringList &GetMetadata() const
    {
        return m_aosMetadata;
    }

    /** Return InformationType records */
    const DDFRecordIndex &GetInformationTypeRecords() const
    {
        return m_oInformationTypeRecordIndex;
    }

    /** Return Point records */
    const DDFRecordIndex &GetPointRecords() const
    {
        return m_oPointRecordIndex;
    }

    /** Return MultiPoint records */
    const DDFRecordIndex &GetMultiPointRecords() const
    {
        return m_oMultiPointRecordIndex;
    }

    /** Return Curve records */
    const DDFRecordIndex &GetCurveRecords() const
    {
        return m_oCurveRecordIndex;
    }

    /** Return CompositeCurve records */
    const DDFRecordIndex &GetCompositeCurveRecords() const
    {
        return m_oCompositeCurveRecordIndex;
    }

    /** Return Surface records */
    const DDFRecordIndex &GetSurfaceRecords() const
    {
        return m_oSurfaceRecordIndex;
    }

    /** Return FeatureType records */
    const DDFRecordIndex &GetFeatureTypeRecords() const
    {
        return m_oFeatureTypeRecordIndex;
    }

    /** Return (and steal) the layer definition for InformationType features */
    OGRFeatureDefnRefCountedPtr StealInformationTypeFeatureDefn()
    {
        return std::move(m_poFeatureDefnInformationType);
    }

    /** Return a map from a CRSId to the layer definition for Point features */
    std::map<CRSId, OGRFeatureDefnRefCountedPtr> &StealPointFeatureDefns()
    {
        return m_oMapPointFeatureDefn;
    }

    /** Return a map from a CRSId to the layer definition for MultiPoint features */
    std::map<CRSId, OGRFeatureDefnRefCountedPtr> &StealMultiPointFeatureDefns()
    {
        return m_oMapMultiPointFeatureDefn;
    }

    /** Return (and steal) a map from a feature type key to the layer definition */
    std::map<FeatureTypeKey, LayerDef> &StealFeatureTypeLayerDefs()
    {
        return m_oMapFeatureKeyToLayerDef;
    }

    /** Return (and steal) the layer definition for curve records */
    OGRFeatureDefnRefCountedPtr StealCurveFeatureDefn()
    {
        return std::move(m_poFeatureDefnCurve);
    }

    /** Return (and steal) the layer definition for composite curve records */
    OGRFeatureDefnRefCountedPtr StealCompositeCurveFeatureDefn()
    {
        return std::move(m_poFeatureDefnCompositeCurve);
    }

    /** Return (and steal) the layer definition for surface records */
    OGRFeatureDefnRefCountedPtr StealSurfaceFeatureDefn()
    {
        return std::move(m_poFeatureDefnSurface);
    }

    /** Return the map from CRS id to indexes of Point records */
    const std::map<OGRS101Reader::CRSId, std::vector<int>> &
    GetMapCRSIdToRecordIdxForPoints() const
    {
        return m_oMapCRSIdToPointRecordIdx;
    }

    /** Return the map from CRS id to indexes of MultiPoint records */
    const std::map<OGRS101Reader::CRSId, std::vector<int>> &
    GetMapCRSIdToRecordIdxForMultiPoints() const
    {
        return m_oMapCRSIdToMultiPointRecordIdx;
    }

    /** Return (and steal) the field domains */
    std::map<std::string, std::unique_ptr<OGRFieldDomain>> &StealFieldDomains()
    {
        return m_oMapFieldDomains;
    }

    bool FillFeatureInformationType(const DDFRecordIndex &oIndex, int iRecord,
                                    OGRFeature &oFeature) const;

    bool FillFeaturePoint(const DDFRecordIndex &oIndex, int iRecord,
                          OGRFeature &oFeature) const;

    bool FillFeatureMultiPoint(const DDFRecordIndex &oIndex, int iRecord,
                               OGRFeature &oFeature) const;

    bool FillFeatureCurve(const DDFRecordIndex &oIndex, int iRecord,
                          OGRFeature &oFeature) const;

    bool FillFeatureCompositeCurve(const DDFRecordIndex &oIndex, int iRecord,
                                   OGRFeature &oFeature) const;

    bool FillFeatureSurface(const DDFRecordIndex &oIndex, int iRecord,
                            OGRFeature &oFeature) const;

    bool FillFeatureFeatureType(const DDFRecordIndex &oIndex, int iRecord,
                                OGRFeature &oFeature) const;
};

/** Emit an error in strict mode (and return false),
 * or an error in non-strict mode (and return true) */
#define EMIT_ERROR_OR_WARNING(msg)                                             \
    EmitErrorOrWarning(__FILE__, __FUNCTION__, __LINE__, msg, m_bStrict, true)

/** Emit an error */
#define EMIT_ERROR(msg)                                                        \
    EmitErrorOrWarning(__FILE__, __FUNCTION__, __LINE__, msg, true, false)

/************************************************************************/
/*                            OGRS101Dataset                            */
/************************************************************************/

class OGRS101Dataset final : public GDALDataset
{
    std::unique_ptr<OGRS101Reader> m_poReader{};
    std::vector<std::unique_ptr<OGRLayer>> m_apoLayers{};

  public:
    OGRS101Dataset() = default;

    int GetLayerCount() const override;
    OGRLayer *GetLayer(int) const override;

    const OGRS101Reader &GetReader() const
    {
        return *m_poReader;
    }

    int TestCapability(const char *) const override;

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static void UnloadDriver(GDALDriver *);
};

/************************************************************************/
/*                            OGRS101Layer()                            */
/************************************************************************/

class OGRS101Layer /* non final */ : public OGRLayer
{
  public:
    OGRS101Layer(OGRS101Dataset &oDS, const DDFRecordIndex &oIndex,
                 OGRFeatureDefnRefCountedPtr poFeatureDefn);
    ~OGRS101Layer() override;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_poFeatureDefn.get();
    }

    void ResetReading() override;

    int TestCapability(const char *) const override;

    GIntBig GetFeatureCount(int bForce) override;

  protected:
    const OGRS101Dataset &m_oDS;
    const DDFRecordIndex &m_oIndex;
    const OGRFeatureDefnRefCountedPtr m_poFeatureDefn{};
    int m_nRecordIdx = 0;

  private:
    CPL_DISALLOW_COPY_ASSIGN(OGRS101Layer)
};

/************************************************************************/
/*                    OGRS101LayerInformationType()                     */
/************************************************************************/

class OGRS101LayerInformationType final
    : public OGRS101Layer,
      OGRGetNextFeatureThroughRaw<OGRS101LayerInformationType>
{
  public:
    OGRS101LayerInformationType(OGRS101Dataset &oDS,
                                const DDFRecordIndex &oIndex,
                                OGRFeatureDefnRefCountedPtr poFeatureDefn);

    OGRFeature *GetNextRawFeature();

    OGRFeature *GetFeature(GIntBig nFID) override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRS101LayerInformationType)
};

/************************************************************************/
/*                         OGRS101LayerPoint()                          */
/************************************************************************/

class OGRS101LayerPoint final : public OGRS101Layer,
                                OGRGetNextFeatureThroughRaw<OGRS101LayerPoint>
{
  public:
    OGRS101LayerPoint(OGRS101Dataset &oDS, const DDFRecordIndex &oIndex,
                      const std::vector<int> &anRecordIndices,
                      OGRFeatureDefnRefCountedPtr poFeatureDefn);

    OGRFeature *GetNextRawFeature();

    GIntBig GetFeatureCount(int bForce) override;

    OGRFeature *GetFeature(GIntBig nFID) override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRS101LayerPoint)

  private:
    const std::vector<int> &m_anRecordIndices;
};

/************************************************************************/
/*                       OGRS101LayerMultiPoint()                       */
/************************************************************************/

class OGRS101LayerMultiPoint final
    : public OGRS101Layer,
      OGRGetNextFeatureThroughRaw<OGRS101LayerMultiPoint>
{
  public:
    OGRS101LayerMultiPoint(OGRS101Dataset &oDS, const DDFRecordIndex &oIndex,
                           const std::vector<int> &anRecordIndices,
                           OGRFeatureDefnRefCountedPtr poFeatureDefn);

    OGRFeature *GetNextRawFeature();

    GIntBig GetFeatureCount(int bForce) override;

    OGRFeature *GetFeature(GIntBig nFID) override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRS101LayerMultiPoint)

  private:
    const std::vector<int> &m_anRecordIndices;
};

/************************************************************************/
/*                         OGRS101LayerCurve()                          */
/************************************************************************/

class OGRS101LayerCurve final : public OGRS101Layer,
                                OGRGetNextFeatureThroughRaw<OGRS101LayerCurve>
{
  public:
    OGRS101LayerCurve(OGRS101Dataset &oDS, const DDFRecordIndex &oIndex,
                      OGRFeatureDefnRefCountedPtr poFeatureDefn);

    OGRFeature *GetNextRawFeature();

    OGRFeature *GetFeature(GIntBig nFID) override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRS101LayerCurve)
};

/************************************************************************/
/*                     OGRS101LayerCompositeCurve()                     */
/************************************************************************/

class OGRS101LayerCompositeCurve final
    : public OGRS101Layer,
      OGRGetNextFeatureThroughRaw<OGRS101LayerCompositeCurve>
{
  public:
    OGRS101LayerCompositeCurve(OGRS101Dataset &oDS,
                               const DDFRecordIndex &oIndex,
                               OGRFeatureDefnRefCountedPtr poFeatureDefn);

    OGRFeature *GetNextRawFeature();

    OGRFeature *GetFeature(GIntBig nFID) override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRS101LayerCompositeCurve)
};

/************************************************************************/
/*                        OGRS101LayerSurface()                         */
/************************************************************************/

class OGRS101LayerSurface final
    : public OGRS101Layer,
      OGRGetNextFeatureThroughRaw<OGRS101LayerSurface>
{
  public:
    OGRS101LayerSurface(OGRS101Dataset &oDS, const DDFRecordIndex &oIndex,
                        OGRFeatureDefnRefCountedPtr poFeatureDefn);

    OGRFeature *GetNextRawFeature();

    OGRFeature *GetFeature(GIntBig nFID) override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRS101LayerSurface)
};

/************************************************************************/
/*                      OGRS101LayerFeatureType()                       */
/************************************************************************/

class OGRS101LayerFeatureType final
    : public OGRS101Layer,
      OGRGetNextFeatureThroughRaw<OGRS101LayerFeatureType>
{
  public:
    OGRS101LayerFeatureType(OGRS101Dataset &oDS, const DDFRecordIndex &oIndex,
                            const std::vector<int> &anRecordIndices,
                            OGRFeatureDefnRefCountedPtr poFeatureDefn);

    OGRFeature *GetNextRawFeature();

    GIntBig GetFeatureCount(int bForce) override;

    OGRFeature *GetFeature(GIntBig nFID) override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRS101LayerFeatureType)

  private:
    const std::vector<int> &m_anRecordIndices;
};

#endif  // OGR_S101_H_INCLUDED
