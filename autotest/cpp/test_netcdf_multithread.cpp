/******************************************************************************
 * Project:  GDAL
 * Purpose:  Test concurrent access through the netCDF driver.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include "gtest_include.h"
#include "test_data.h"

#include <array>
#include <future>
#include <string>

namespace
{

constexpr const char *apszDrivers[] = {"netCDF", nullptr};
constexpr const char *apszNC4Options[] = {"FORMAT=NC4", nullptr};

GDALDatasetUniquePtr Open(const std::string &osPath, unsigned int nFlags)
{
    return GDALDatasetUniquePtr(GDALDataset::Open(
        osPath.c_str(), nFlags | GDAL_OF_VERBOSE_ERROR, apszDrivers));
}

void CheckRaster(GDALDataset *poDS)
{
    ASSERT_NE(poDS, nullptr);
    ASSERT_EQ(poDS->GetRasterCount(), 3);
    ASSERT_EQ(poDS->GetRasterXSize(), 13);
    ASSERT_EQ(poDS->GetRasterYSize(), 11);

    constexpr const char *apszTimes[] = {"24", "48", "72"};
    constexpr int anChecksums[] = {1487, 1698, 1655};
    for (int i = 0; i < 3; ++i)
    {
        auto *poBand = poDS->GetRasterBand(i + 1);
        ASSERT_NE(poBand, nullptr);
        ASSERT_NE(poBand->GetMetadata(), nullptr);
        EXPECT_STREQ(poBand->GetMetadataItem("NETCDF_DIM_time"), apszTimes[i]);
        EXPECT_EQ(GDALChecksumImage(poBand, 0, 0, 13, 11), anChecksums[i]);
    }
}

void CheckArray(const std::string &osPath)
{
    auto poDS = Open(osPath, GDAL_OF_MULTIDIM_RASTER);
    ASSERT_NE(poDS, nullptr);
    auto poGroup = poDS->GetRootGroup();
    ASSERT_NE(poGroup, nullptr);
    auto poArray = poGroup->OpenMDArray("FWI");
    ASSERT_NE(poArray, nullptr);
    auto poSRS = poArray->GetSpatialRef();
    ASSERT_NE(poSRS, nullptr);
    EXPECT_TRUE(poSRS->IsProjected());
    EXPECT_NEAR(poSRS->GetSemiMajor(), 6378137.0, 0.01);
    bool bHasFill = false;
    EXPECT_EQ(poArray->GetNoDataValueAsDouble(&bHasFill), -9999);
    EXPECT_TRUE(bHasFill);
}

void CheckDefaultFill()
{
    auto poDS = Open(GDRIVERS_DATA_DIR "netcdf/alldatatypes.nc",
                     GDAL_OF_MULTIDIM_RASTER);
    ASSERT_NE(poDS, nullptr);
    auto poGroup = poDS->GetRootGroup();
    ASSERT_NE(poGroup, nullptr);
    const char *const apszOptions[] = {"USE_DEFAULT_FILL_AS_NODATA=YES",
                                       nullptr};
    auto poArray = poGroup->OpenMDArray("float_var", apszOptions);
    ASSERT_NE(poArray, nullptr);
    // Default netCDF fill value for a Float32 array.
    constexpr float fDefaultFill = 9.9692099683868690e+36f;
    bool bHasFill = false;
    EXPECT_EQ(poArray->GetNoDataValueAsDouble(&bHasFill),
              static_cast<double>(fDefaultFill));
    EXPECT_TRUE(bHasFill);
}

struct CopyProgress
{
    const std::string &osSource;
    bool bChecked = false;
};

int CheckCopyProgress(double dfComplete, const char *, void *pUserData)
{
    auto &sProgress = *static_cast<CopyProgress *>(pUserData);
    if (dfComplete >= 0.25 && dfComplete < 1 && !sProgress.bChecked)
    {
        sProgress.bChecked = true;
        // The other thread must be able to read while CreateCopy is paused.
        std::async(std::launch::async,
                   [&sProgress]()
                   {
                       auto poDS = Open(sProgress.osSource, GDAL_OF_RASTER);
                       CheckRaster(poDS.get());
                   })
            .get();
    }
    return TRUE;
}

void CheckCopy(GDALDriver *poDriver, const std::string &osSource,
               const std::string &osDir)
{
    auto poRaster = Open(osSource, GDAL_OF_RASTER);
    ASSERT_NE(poRaster, nullptr);
    const auto osCopy = osDir + "/copy.nc";
    CopyProgress sProgress{osSource};
    GDALDatasetUniquePtr poRasterCopy(
        poDriver->CreateCopy(osCopy.c_str(), poRaster.get(), FALSE,
                             apszNC4Options, CheckCopyProgress, &sProgress));
    ASSERT_NE(poRasterCopy, nullptr);
    EXPECT_TRUE(sProgress.bChecked);
    ASSERT_NO_FATAL_FAILURE(CheckRaster(poRasterCopy.get()));
    ASSERT_EQ(GDALClose(poRasterCopy.release()), CE_None);

    auto poMD = Open(osSource, GDAL_OF_MULTIDIM_RASTER);
    ASSERT_NE(poMD, nullptr);
    GDALDatasetUniquePtr poMDCopy(poDriver->CreateCopy(
        osCopy.c_str(), poMD.get(), FALSE, apszNC4Options, nullptr, nullptr));
    ASSERT_NE(poMDCopy, nullptr);
    ASSERT_EQ(GDALClose(poMDCopy.release()), CE_None);
    CheckArray(osCopy);
}

void CheckVector(GDALDriver *poDriver, const std::string &osPath,
                 const char *pszMode, bool bSeparateFiles)
{
    const char *const apszOptions[] = {"FORMAT=NC4", pszMode,
                                       "GEOMETRY_ENCODING=WKT", nullptr};
    GDALDatasetUniquePtr poOutput(
        poDriver->Create(osPath.c_str(), 0, 0, 0, GDT_Unknown, apszOptions));
    ASSERT_NE(poOutput, nullptr);
    for (const char *pszName : {"first", "second"})
    {
        auto *poLayer = poOutput->CreateLayer(pszName, nullptr, wkbNone);
        ASSERT_NE(poLayer, nullptr);
        OGRFieldDefn oField("value", OFTInteger);
        ASSERT_EQ(poLayer->CreateField(&oField), OGRERR_NONE);
        OGRFeature oFeature(poLayer->GetLayerDefn());
        oFeature.SetField("value", 7);
        ASSERT_EQ(poLayer->CreateFeature(&oFeature), OGRERR_NONE);
    }
    ASSERT_EQ(GDALClose(poOutput.release()), CE_None);

    auto poInput =
        Open(bSeparateFiles ? osPath + "/first.nc" : osPath, GDAL_OF_VECTOR);
    ASSERT_NE(poInput, nullptr);
    ASSERT_EQ(poInput->GetLayerCount(), bSeparateFiles ? 1 : 2);
    for (int i = 0; i < poInput->GetLayerCount(); ++i)
    {
        auto *poLayer = poInput->GetLayer(i);
        ASSERT_NE(poLayer, nullptr);
        EXPECT_EQ(poLayer->GetFeatureCount(), 1);
        std::unique_ptr<OGRFeature> poFeature(poLayer->GetNextFeature());
        ASSERT_NE(poFeature, nullptr);
        EXPECT_EQ(poFeature->GetFieldAsInteger("value"), 7);
    }
}

void CheckSentinel(const std::string &osPath)
{
    ASSERT_EQ(CPLCopyFile(osPath.c_str(), GDRIVERS_DATA_DIR
                          "netcdf/sen3_sral_mwr_fake_standard_measurement.nc"),
              0);
    {
        CPLErrorHandlerPusher oQuietErrors(CPLQuietErrorHandler);
        EXPECT_EQ(Open(osPath, GDAL_OF_VECTOR | GDAL_OF_UPDATE), nullptr);
    }
    auto poInput = Open(osPath, GDAL_OF_VECTOR);
    ASSERT_NE(poInput, nullptr);
    auto *poLayer = poInput->GetLayer(0);
    ASSERT_NE(poLayer, nullptr);
    EXPECT_EQ(poLayer->GetFeatureCount(), 2);
    std::unique_ptr<OGRFeature> poFeature(poLayer->GetFeature(1));
    ASSERT_NE(poFeature, nullptr);
    EXPECT_NE(poFeature->GetGeometryRef(), nullptr);
}

void CheckSimpleGeometry()
{
    auto poInput =
        Open(GDRIVERS_DATA_DIR "netcdf-sg/point_test.nc", GDAL_OF_VECTOR);
    ASSERT_NE(poInput, nullptr);
    auto *poLayer = poInput->GetLayer(0);
    ASSERT_NE(poLayer, nullptr);
    EXPECT_GT(poLayer->GetFeatureCount(), 0);
    std::unique_ptr<OGRFeature> poFeature(poLayer->GetNextFeature());
    ASSERT_NE(poFeature, nullptr);
    EXPECT_NE(poFeature->GetGeometryRef(), nullptr);
}

class test_netcdf_multithread : public ::testing::Test
{
  protected:
    std::string m_osDir{};
    const std::string m_osRaster{GDRIVERS_DATA_DIR
                                 "netcdf/short_geotransform_notgdalcf.nc"};
    GDALDriver *m_poDriver = nullptr;
    GIntBig m_nCacheMax = GDALGetCacheMax64();
    CPLConfigOptionSetter m_oDisablePAM{"GDAL_PAM_ENABLED", "NO", false};

    static void SetUpTestSuite()
    {
        GDALAllRegister();
    }

    static void TearDownTestSuite()
    {
        GDALDestroyDriverManager();
    }

    void SetUp() override
    {
        m_poDriver = GetGDALDriverManager()->GetDriverByName("netCDF");
        if (!m_poDriver)
            GTEST_SKIP() << "netCDF driver missing";
        const char *pszNC4 = m_poDriver->GetMetadataItem("NETCDF_HAS_NC4");
        if (!pszNC4 || !CPLTestBool(pszNC4))
            GTEST_SKIP() << "netCDF driver with netCDF-4 support required";

        const auto osDir = CPLGenerateTempFilenameSafe("netcdf_multithread");
        ASSERT_EQ(VSIMkdir(osDir.c_str(), 0700), 0);
        m_osDir = osDir;
    }

    void TearDown() override
    {
        GDALSetCacheMax64(m_nCacheMax);
        if (!m_osDir.empty())
            EXPECT_EQ(VSIRmdirRecursive(m_osDir.c_str()), 0);
    }
};

TEST_F(test_netcdf_multithread, concurrent_raster_and_multidimensional_reads)
{
    // Keep a handle alive while the workers repeatedly open and close the file.
    auto poAnchor = Open(m_osRaster, GDAL_OF_RASTER);
    ASSERT_NE(poAnchor, nullptr);
    std::array<std::future<void>, 4> aoWorkers;
    for (auto &oWorker : aoWorkers)
    {
        oWorker = std::async(
            std::launch::async,
            [this]()
            {
                for (int i = 0; i < 20; ++i)
                {
                    auto poFirst = Open(m_osRaster, GDAL_OF_RASTER);
                    ASSERT_NE(poFirst, nullptr);
                    auto poSecond = Open(m_osRaster, GDAL_OF_RASTER);
                    ASSERT_NO_FATAL_FAILURE(CheckRaster(poSecond.get()));
                    poSecond.reset();
                    ASSERT_NO_FATAL_FAILURE(CheckRaster(poFirst.get()));
                    ASSERT_NO_FATAL_FAILURE(CheckArray(m_osRaster));
                    ASSERT_NO_FATAL_FAILURE(CheckDefaultFill());
                }
            });
    }
    for (auto &oWorker : aoWorkers)
        oWorker.get();
    CheckRaster(poAnchor.get());
}

TEST_F(test_netcdf_multithread, concurrent_copies_and_vector_access)
{
    GDALSetCacheMax64(1024);
    std::array<std::future<void>, 8> aoWorkers;
    for (size_t i = 0; i < aoWorkers.size(); ++i)
    {
        aoWorkers[i] = std::async(
            std::launch::async,
            [this, i]()
            {
                const auto osWorkerDir = m_osDir + "/" + std::to_string(i);
                ASSERT_EQ(VSIMkdir(osWorkerDir.c_str(), 0700), 0);
                for (int j = 0; j < 10; ++j)
                {
                    const auto osDir = osWorkerDir + "/" + std::to_string(j);
                    SCOPED_TRACE(osDir);
                    ASSERT_EQ(VSIMkdir(osDir.c_str(), 0700), 0);
                    ASSERT_NO_FATAL_FAILURE(
                        CheckCopy(m_poDriver, m_osRaster, osDir));
                    ASSERT_NO_FATAL_FAILURE(
                        CheckVector(m_poDriver, osDir + "/files",
                                    "MULTIPLE_LAYERS=SEPARATE_FILES", true));
                    ASSERT_NO_FATAL_FAILURE(
                        CheckVector(m_poDriver, osDir + "/groups.nc",
                                    "MULTIPLE_LAYERS=SEPARATE_GROUPS", false));
                    ASSERT_NO_FATAL_FAILURE(
                        CheckSentinel(osDir + "/sentinel.nc"));
                    ASSERT_NO_FATAL_FAILURE(CheckSimpleGeometry());
                }
            });
    }
    for (auto &oWorker : aoWorkers)
        oWorker.get();
}

}  // namespace
