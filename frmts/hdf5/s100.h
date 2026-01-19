/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S100 bathymetric datasets.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef S100_H
#define S100_H

#include "cpl_port.h"

#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

#include "hdf5_api.h"
#include "gh5_convenience.h"

#include <array>

/************************************************************************/
/*                            S100BaseDataset                           */
/************************************************************************/

class S100BaseDataset CPL_NON_FINAL : public GDALPamDataset
{
  private:
    void ReadSRS();

  protected:
    std::string m_osFilename{};
    std::shared_ptr<GDALGroup> m_poRootGroup{};
    OGRSpatialReference m_oSRS{};
    bool m_bHasGT = false;
    GDALGeoTransform m_gt{};
    std::string m_osMetadataFile{};

    explicit S100BaseDataset(const std::string &osFilename);

    bool Init();

    void SetMetadataForDataDynamicity(const GDALAttribute *poAttr);
    void SetMetadataForCommonPointRule(const GDALAttribute *poAttr);
    void SetMetadataForInterpolationType(const GDALAttribute *poAttr);

  public:
    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    const OGRSpatialReference *GetSpatialRef() const override;

    char **GetFileList() override;
};

/************************************************************************/
/*                            S100BaseWriter                            */
/************************************************************************/

class S100BaseWriter CPL_NON_FINAL
{
  public:
    virtual ~S100BaseWriter();

  protected:
    S100BaseWriter(const char *pszDestFilename, GDALDataset *poSrcDS,
                   CSLConstList papszOptions);

    // to be called by destructor of derived classes which must also
    // end up by calling BaseClose()
    virtual bool Close() = 0;
    bool BaseClose();

    bool BaseChecks(const char *pszDriverName, bool crsMustBeEPSG,
                    bool verticalDatumRequired);

    static bool WriteUInt8Value(hid_t hGroup, const char *pszName, int value);
    static bool WriteUInt16Value(hid_t hGroup, const char *pszName, int value);
    static bool WriteInt32Value(hid_t hGroup, const char *pszName, int value);
    static bool WriteUInt32Value(hid_t hGroup, const char *pszName,
                                 unsigned value);
    static bool WriteFloat32Value(hid_t hGroup, const char *pszName,
                                  double value);
    static bool WriteFloat64Value(hid_t hGroup, const char *pszName,
                                  double value);
    static bool WriteVarLengthStringValue(hid_t hGroup, const char *pszName,
                                          const char *pszValue);
    static bool WriteFixedLengthStringValue(hid_t hGroup, const char *pszName,
                                            const char *pszValue);
    static bool WriteOneDimensionalVarLengthStringArray(hid_t hGroup,
                                                        const char *name,
                                                        CSLConstList values);

    bool OpenFileUpdateMode();
    bool CreateFile();
    bool WriteProductSpecification(const char *pszProductSpecification);
    bool WriteIssueDate();
    bool WriteIssueTime(bool bAutogenerateFromCurrent);
    bool WriteTopLevelBoundingBox();
    bool WriteHorizontalCRS();
    bool WriteVerticalCS(int nCode);
    bool WriteVerticalCoordinateBase(int nCode);
    static bool WriteVerticalDatumReference(hid_t hGroup, int nCode);
    static bool WriteVerticalDatum(hid_t hGroup, hid_t hType, int nCode);

    bool CreateFeatureGroup(const char *name);
    static bool WriteDataCodingFormat(hid_t hGroup, int nCode);
    static bool WriteCommonPointRule(hid_t hGroup, int nCode);
    static bool WriteDataOffsetCode(hid_t hGroup, int nCode);
    static bool WriteDimension(hid_t hGroup, int nCode);
    static bool WriteHorizontalPositionUncertainty(hid_t hGroup, float fValue);
    static bool WriteVerticalUncertainty(hid_t hGroup, float fValue);
    static bool WriteInterpolationType(hid_t hGroup, int nCode);
    static bool WriteNumInstances(hid_t hGroup, hid_t hType, int numInstances);
    static bool WriteSequencingRuleScanDirection(hid_t hGroup,
                                                 const char *pszValue);
    static bool WriteSequencingRuleType(hid_t hGroup, int nCode);
    bool WriteAxisNames(hid_t hGroup);

    bool CreateFeatureInstanceGroup(const char *name);
    bool WriteFIGGridRelatedParameters(hid_t hGroup);
    static bool WriteNumGRP(hid_t hGroup, hid_t hType, int numGRP);

    bool CreateValuesGroup(const char *name);

    bool CreateGroupF();

    static constexpr int GROUP_F_DATASET_FIELD_COUNT = 8;
    bool WriteGroupFDataset(
        const char *name,
        const std::vector<std::array<const char *, GROUP_F_DATASET_FIELD_COUNT>>
            &rows);

    const std::string m_osDestFilename;
    GDALDataset *const m_poSrcDS;
    const CPLStringList m_aosOptions;
    GDALGeoTransform m_gt{};
    GH5_HIDFileHolder m_hdf5{};
    GH5_HIDGroupHolder m_GroupF{};
    GH5_HIDGroupHolder m_featureGroup{};
    GH5_HIDGroupHolder m_featureInstanceGroup{};
    GH5_HIDGroupHolder m_valuesGroup{};
    const OGRSpatialReference *m_poSRS = nullptr;
    int m_nVerticalDatum = 0;
    int m_nEPSGCode = 0;

    CPL_DISALLOW_COPY_ASSIGN(S100BaseWriter)
};

/************************************************************************/
/*                        Function declarations                         */
/************************************************************************/

bool S100GetNumPointsLongitudinalLatitudinal(const GDALGroup *poGroup,
                                             int &nNumPointsLongitudinal,
                                             int &nNumPointsLatitudinal);

bool S100ReadSRS(const GDALGroup *poRootGroup, OGRSpatialReference &oSRS);

bool S100GetDimensions(
    const GDALGroup *poGroup,
    std::vector<std::shared_ptr<GDALDimension>> &apoDims,
    std::vector<std::shared_ptr<GDALMDArray>> &apoIndexingVars);

bool S100GetGeoTransform(const GDALGroup *poGroup, GDALGeoTransform &gt,
                         bool bNorthUp);

constexpr const char *S100_VERTICAL_DATUM_NAME = "VERTICAL_DATUM_NAME";
constexpr const char *S100_VERTICAL_DATUM_ABBREV = "VERTICAL_DATUM_ABBREV";
constexpr const char *S100_VERTICAL_DATUM_EPSG_CODE =
    "VERTICAL_DATUM_EPSG_CODE";

int S100GetVerticalDatumCodeFromNameOrAbbrev(const char *pszStr);
void S100ReadVerticalDatum(GDALMajorObject *poMO, const GDALGroup *poGroup);

std::string S100ReadMetadata(GDALDataset *poDS, const std::string &osFilename,
                             const GDALGroup *poRootGroup);

#endif  // S100_H
