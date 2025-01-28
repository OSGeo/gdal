///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test GDALAlgorithm
// Author:   Even Rouault <even.rouault at spatialys.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_unit_test.h"

#include "cpl_error.h"
#include "cpl_string.h"
#include "gdalalgorithm.h"
#include "gdal_priv.h"

#include "test_data.h"

#include "gtest_include.h"

#include <limits>

namespace test_gdal_algorithm
{

struct test_gdal_algorithm : public ::testing::Test
{
};

TEST_F(test_gdal_algorithm, GDALAlgorithmArgTypeName)
{
    EXPECT_STREQ(GDALAlgorithmArgTypeName(GAAT_BOOLEAN), "boolean");
    EXPECT_STREQ(GDALAlgorithmArgTypeName(GAAT_STRING), "string");
    EXPECT_STREQ(GDALAlgorithmArgTypeName(GAAT_INTEGER), "integer");
    EXPECT_STREQ(GDALAlgorithmArgTypeName(GAAT_REAL), "real");
    EXPECT_STREQ(GDALAlgorithmArgTypeName(GAAT_DATASET), "dataset");
    EXPECT_STREQ(GDALAlgorithmArgTypeName(GAAT_STRING_LIST), "string_list");
    EXPECT_STREQ(GDALAlgorithmArgTypeName(GAAT_INTEGER_LIST), "integer_list");
    EXPECT_STREQ(GDALAlgorithmArgTypeName(GAAT_REAL_LIST), "real_list");
    EXPECT_STREQ(GDALAlgorithmArgTypeName(GAAT_DATASET_LIST), "dataset_list");
}

TEST_F(test_gdal_algorithm, GDALArgDatasetValueTypeName)
{
    EXPECT_STREQ(GDALArgDatasetValueTypeName(GDAL_OF_RASTER).c_str(), "raster");
    EXPECT_STREQ(GDALArgDatasetValueTypeName(GDAL_OF_VECTOR).c_str(), "vector");
    EXPECT_STREQ(GDALArgDatasetValueTypeName(GDAL_OF_MULTIDIM_RASTER).c_str(),
                 "multidimensional raster");
    EXPECT_STREQ(
        GDALArgDatasetValueTypeName(GDAL_OF_RASTER | GDAL_OF_VECTOR).c_str(),
        "raster or vector");
    EXPECT_STREQ(
        GDALArgDatasetValueTypeName(GDAL_OF_RASTER | GDAL_OF_MULTIDIM_RASTER)
            .c_str(),
        "raster or multidimensional raster");
    EXPECT_STREQ(GDALArgDatasetValueTypeName(GDAL_OF_RASTER | GDAL_OF_VECTOR |
                                             GDAL_OF_MULTIDIM_RASTER)
                     .c_str(),
                 "raster, vector or multidimensional raster");
    EXPECT_STREQ(
        GDALArgDatasetValueTypeName(GDAL_OF_VECTOR | GDAL_OF_MULTIDIM_RASTER)
            .c_str(),
        "vector or multidimensional raster");
}

TEST_F(test_gdal_algorithm, GDALAlgorithmArgDecl_SetMinCount)
{
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_EQ(GDALAlgorithmArgDecl("", 0, "", GAAT_BOOLEAN)
                      .SetMinCount(2)
                      .GetMinCount(),
                  0);
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
    EXPECT_EQ(GDALAlgorithmArgDecl("", 0, "", GAAT_STRING_LIST)
                  .SetMinCount(2)
                  .GetMinCount(),
              2);
}

TEST_F(test_gdal_algorithm, GDALAlgorithmArgDecl_SetMaxCount)
{
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_EQ(GDALAlgorithmArgDecl("", 0, "", GAAT_BOOLEAN)
                      .SetMaxCount(2)
                      .GetMaxCount(),
                  1);
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
    EXPECT_EQ(GDALAlgorithmArgDecl("", 0, "", GAAT_STRING_LIST)
                  .SetMaxCount(2)
                  .GetMaxCount(),
              2);
}

TEST_F(test_gdal_algorithm, GDALAlgorithmArg_Set)
{
    {
        bool val = false;
        auto arg = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_BOOLEAN), &val);
        arg.Set(true);
        EXPECT_EQ(arg.Get<bool>(), true);
        EXPECT_EQ(val, true);

        {
            bool val2 = false;
            auto arg2 = GDALAlgorithmArg(
                GDALAlgorithmArgDecl("", 0, "", GAAT_BOOLEAN), &val2);
            arg2.SetFrom(arg);
            EXPECT_EQ(arg2.Get<bool>(), true);
        }

        arg.Set(false);

        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(1);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<bool>(), false);

            CPLErrorReset();
            arg.Set(1.5);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<bool>(), false);

            CPLErrorReset();
            arg.Set("foo");
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<bool>(), false);

            CPLErrorReset();
            arg.Set(std::vector<std::string>());
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<bool>(), false);

            CPLErrorReset();
            arg.Set(std::vector<int>());
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<bool>(), false);

            CPLErrorReset();
            arg.Set(std::vector<double>());
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<bool>(), false);

            CPLErrorReset();
            arg.Set(std::vector<GDALArgDatasetValue>());
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<bool>(), false);

            auto poDS = std::unique_ptr<GDALDataset>(
                GetGDALDriverManager()->GetDriverByName("MEM")->Create(
                    "", 1, 1, 1, GDT_Byte, nullptr));
            CPLErrorReset();
            arg.Set(std::move(poDS));
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<bool>(), false);
        }

        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            int val2 = 1;
            auto arg2 = GDALAlgorithmArg(
                GDALAlgorithmArgDecl("", 0, "", GAAT_INTEGER), &val2);
            arg2.SetFrom(arg);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(val2, 1);
        }
    }
    {
        int val = 0;
        auto arg = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_INTEGER), &val);
        arg.Set(1);
        EXPECT_EQ(arg.Get<int>(), 1);
        EXPECT_EQ(val, 1);

        int val2 = 0;
        auto arg2 = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_INTEGER), &val2);
        arg2.SetFrom(arg);
        EXPECT_EQ(arg2.Get<int>(), 1);

        arg.Set(0);
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(true);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<int>(), 0);
        }
    }
    {
        double val = 0;
        auto arg = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_REAL).SetDefault(-1), &val);
        arg.Set(1.5);
        EXPECT_EQ(arg.Get<double>(), 1.5);
        EXPECT_EQ(val, 1.5);
        arg.Set(1);
        EXPECT_EQ(arg.Get<double>(), 1);

        double val2 = 0;
        auto arg2 = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_REAL).SetDefault(-1.5), &val2);
        arg2.SetFrom(arg);
        EXPECT_EQ(arg2.Get<double>(), 1);

        arg.Set(0);
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(true);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<double>(), 0);
        }
    }
    {
        std::string val;
        auto arg = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_STRING), &val);
        arg.Set("foo");
        EXPECT_STREQ(arg.Get<std::string>().c_str(), "foo");
        EXPECT_STREQ(val.c_str(), "foo");

        std::string val2;
        auto arg2 = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_STRING), &val2);
        arg2.SetFrom(arg);
        EXPECT_STREQ(arg2.Get<std::string>().c_str(), "foo");

        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(true);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_STREQ(arg.Get<std::string>().c_str(), "foo");
        }
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(static_cast<GDALDataset *>(nullptr));
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_STREQ(arg.Get<std::string>().c_str(), "foo");
        }
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.SetDatasetName("bar");
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_STREQ(arg.Get<std::string>().c_str(), "foo");
        }
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            GDALArgDatasetValue dsValue;
            arg.SetFrom(dsValue);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_STREQ(arg.Get<std::string>().c_str(), "foo");
        }
    }
    {
        std::string val;
        auto arg = GDALAlgorithmArg(GDALAlgorithmArgDecl("", 0, "", GAAT_STRING)
                                        .SetReadFromFileAtSyntaxAllowed()
                                        .SetRemoveSQLCommentsEnabled(),
                                    &val);
        EXPECT_TRUE(arg.Set("foo"));
        EXPECT_STREQ(val.c_str(), "foo");
    }
    {
        std::string osTmpFilename = VSIMemGenerateHiddenFilename("temp.sql");
        VSILFILE *fpTmp = VSIFOpenL(osTmpFilename.c_str(), "wb");
        VSIFPrintfL(fpTmp, "\xEF\xBB\xBF");  // UTF-8 BOM
        VSIFPrintfL(fpTmp, "-- this is a comment\n");
        VSIFPrintfL(fpTmp, "value");
        VSIFCloseL(fpTmp);

        std::string val;
        auto arg = GDALAlgorithmArg(GDALAlgorithmArgDecl("", 0, "", GAAT_STRING)
                                        .SetReadFromFileAtSyntaxAllowed()
                                        .SetRemoveSQLCommentsEnabled(),
                                    &val);
        EXPECT_TRUE(arg.Set(("@" + osTmpFilename).c_str()));
        EXPECT_STREQ(val.c_str(), "value");
        VSIUnlink(osTmpFilename.c_str());
    }
    {
        std::string val;
        auto arg = GDALAlgorithmArg(GDALAlgorithmArgDecl("", 0, "", GAAT_STRING)
                                        .SetReadFromFileAtSyntaxAllowed(),
                                    &val);
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        EXPECT_FALSE(arg.Set("@i_do_not_exist"));
    }
    {
        auto poMEMDS = std::unique_ptr<GDALDataset>(
            GetGDALDriverManager()->GetDriverByName("MEM")->Create(
                "", 1, 1, 1, GDT_Byte, nullptr));
        GDALArgDatasetValue val;
        auto arg = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_DATASET), &val);
        auto poMEMDSRaw = poMEMDS.get();

        arg.Set(poMEMDS.release());
        EXPECT_EQ(val.GetDatasetRef(), poMEMDSRaw);

        poMEMDS.reset(val.BorrowDataset());
        EXPECT_EQ(poMEMDS.get(), poMEMDSRaw);
        EXPECT_EQ(val.GetDatasetRef(), nullptr);

        EXPECT_TRUE(arg.Set(std::move(poMEMDS)));
        EXPECT_EQ(val.GetDatasetRef(), poMEMDSRaw);

        poMEMDSRaw->ReleaseRef();

        arg.SetDatasetName("foo");
        EXPECT_STREQ(val.GetName().c_str(), "foo");

        GDALArgDatasetValue val2;
        val2.Set("bar");
        arg.SetFrom(val2);
        EXPECT_STREQ(val.GetName().c_str(), "bar");

        auto arg2 = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_DATASET), &val2);
        val2.Set("baz");
        arg.SetFrom(arg2);
        EXPECT_STREQ(val.GetName().c_str(), "baz");

        val.SetInputFlags(GADV_NAME);
        val.SetOutputFlags(GADV_OBJECT);
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(static_cast<GDALDataset *>(nullptr));
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        }
    }
    {
        std::vector<std::string> val;
        auto arg = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_STRING_LIST), &val);
        const std::vector<std::string> expected{"foo", "bar"};
        arg.Set(expected);
        EXPECT_EQ(arg.Get<std::vector<std::string>>(), expected);
        EXPECT_EQ(val, expected);

        std::vector<std::string> val2;
        auto arg2 = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_STRING_LIST), &val2);
        arg2.SetFrom(arg);
        EXPECT_EQ(arg2.Get<std::vector<std::string>>(), expected);

        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(true);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<std::vector<std::string>>(), expected);
        }
    }
    {
        std::vector<int> val;
        auto arg = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_INTEGER_LIST), &val);
        const std::vector<int> expected{1, 2};
        arg.Set(expected);
        EXPECT_EQ(arg.Get<std::vector<int>>(), expected);
        EXPECT_EQ(val, expected);

        std::vector<int> val2;
        auto arg2 = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_INTEGER_LIST), &val2);
        arg2.SetFrom(arg);
        EXPECT_EQ(arg2.Get<std::vector<int>>(), expected);

        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(true);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<std::vector<int>>(), expected);
        }
    }
    {
        std::vector<double> val;
        auto arg = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_REAL_LIST), &val);
        const std::vector<double> expected{1.5, 2.5};
        arg.Set(expected);
        EXPECT_EQ(arg.Get<std::vector<double>>(), expected);
        EXPECT_EQ(val, expected);

        std::vector<double> val2;
        auto arg2 = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_REAL_LIST), &val2);
        arg2.SetFrom(arg);
        EXPECT_EQ(arg2.Get<std::vector<double>>(), expected);

        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(true);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<std::vector<double>>(), expected);
        }
    }
    {
        std::vector<GDALArgDatasetValue> val;
        auto arg = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_DATASET_LIST), &val);
        {
            std::vector<GDALArgDatasetValue> val2;
            val2.emplace_back(GDALArgDatasetValue("foo"));
            val2.emplace_back(GDALArgDatasetValue("bar"));
            arg.Set(std::move(val2));
            EXPECT_EQ(arg.Get<std::vector<GDALArgDatasetValue>>().size(), 2U);
            EXPECT_EQ(val.size(), 2U);
        }

        std::vector<GDALArgDatasetValue> val2;
        auto arg2 = GDALAlgorithmArg(
            GDALAlgorithmArgDecl("", 0, "", GAAT_DATASET_LIST), &val2);
        arg2.SetFrom(arg);
        EXPECT_EQ(arg2.Get<std::vector<GDALArgDatasetValue>>().size(), 2U);

        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            arg.Set(true);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
            EXPECT_EQ(arg.Get<std::vector<GDALArgDatasetValue>>().size(), 2U);
        }
    }
}

TEST_F(test_gdal_algorithm, RunValidationActions)
{
    int val = 0;
    auto arg = GDALInConstructionAlgorithmArg(
        nullptr, GDALAlgorithmArgDecl("", 0, "", GAAT_INTEGER), &val);
    arg.AddValidationAction([&arg]() { return arg.Get<int>() == 1; });
    EXPECT_TRUE(arg.Set(1));
    EXPECT_FALSE(arg.Set(2));
}

TEST_F(test_gdal_algorithm, SetIsCRSArg_wrong_type)
{
    int val = 0;
    auto arg = GDALInConstructionAlgorithmArg(
        nullptr, GDALAlgorithmArgDecl("", 0, "", GAAT_INTEGER), &val);
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        arg.SetIsCRSArg();
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

class MyAlgorithmWithDummyRun : public GDALAlgorithm
{
  public:
    MyAlgorithmWithDummyRun(const std::string &name = "test",
                            const std::string &description = "",
                            const std::string &url = "https://example.com")
        : GDALAlgorithm(name, description, url)
    {
    }

    bool RunImpl(GDALProgressFunc, void *) override
    {
        return true;
    }
};

TEST_F(test_gdal_algorithm, wrong_long_name_dash)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;

        MyAlgorithm()
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            AddArg("-", 0, "", &m_flag);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        }
    };

    MyAlgorithm alg;
    CPL_IGNORE_RET_VAL(alg.Run());
}

TEST_F(test_gdal_algorithm, wrong_long_name_contains_equal)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;

        MyAlgorithm()
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            AddArg("foo=bar", 0, "", &m_flag);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        }
    };

    MyAlgorithm alg;
}

TEST_F(test_gdal_algorithm, long_name_duplicated)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;

        MyAlgorithm()
        {
            AddArg("foo", 0, "", &m_flag);
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            AddArg("foo", 0, "", &m_flag);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        }
    };

    MyAlgorithm alg;
}

TEST_F(test_gdal_algorithm, wrong_short_name)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;

        MyAlgorithm()
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            AddArg("", '-', "", &m_flag);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        }
    };

    MyAlgorithm alg;
}

TEST_F(test_gdal_algorithm, short_name_duplicated)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;

        MyAlgorithm()
        {
            AddArg("", 'x', "", &m_flag);
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            AddArg("", 'x', "", &m_flag);
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        }
    };

    MyAlgorithm alg;
}

TEST_F(test_gdal_algorithm, GDALInConstructionAlgorithmArg_AddAlias)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;

        MyAlgorithm()
        {
            AddArg("flag", 'f', "boolean flag", &m_flag).AddAlias("alias");
        }
    };

    MyAlgorithm alg;
    alg.GetUsageForCLI(false);
    EXPECT_NE(alg.GetArg("flag"), nullptr);
    EXPECT_NE(alg.GetArg("--flag"), nullptr);
    EXPECT_NE(alg.GetArg("-f"), nullptr);
    EXPECT_NE(alg.GetArg("f"), nullptr);
    EXPECT_NE(alg.GetArg("alias"), nullptr);
    EXPECT_EQ(alg.GetArg("invalid"), nullptr);
    EXPECT_EQ(alg.GetArg("-"), nullptr);
}

TEST_F(test_gdal_algorithm, GDALInConstructionAlgorithmArg_AddAlias_redundant)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;
        bool m_flag2 = false;

        MyAlgorithm()
        {
            AddArg("flag", 'F', "boolean flag", &m_flag).AddAlias("alias");
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            CPLErrorReset();
            AddArg("flag2", '9', "boolean flag2", &m_flag2).AddAlias("alias");
            EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        }
    };

    MyAlgorithm alg;
    EXPECT_NE(alg.GetArg("alias"), nullptr);
}

TEST_F(test_gdal_algorithm, GDALInConstructionAlgorithmArg_AddHiddenAlias)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;

        MyAlgorithm()
        {
            AddArg("flag", 'f', "boolean flag", &m_flag)
                .AddHiddenAlias("hidden_alias");
        }
    };

    MyAlgorithm alg;
    EXPECT_NE(alg.GetArg("hidden_alias"), nullptr);
}

TEST_F(test_gdal_algorithm, GDALInConstructionAlgorithmArg_SetPositional)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        int m_val = 0;

        MyAlgorithm()
        {
            AddArg("option", 0, "option", &m_val).SetPositional();
        }
    };

    MyAlgorithm alg;
    EXPECT_TRUE(alg.GetArg("option")->IsPositional());
}

TEST_F(test_gdal_algorithm, GDALArgDatasetValue)
{
    {
        auto poDS = std::unique_ptr<GDALDataset>(
            GetGDALDriverManager()->GetDriverByName("MEM")->Create(
                "", 1, 1, 1, GDT_Byte, nullptr));
        auto poDSRaw = poDS.get();
        GDALArgDatasetValue value(poDS.release());
        EXPECT_EQ(value.GetDatasetRef(), poDSRaw);
        EXPECT_STREQ(value.GetName().c_str(), poDSRaw->GetDescription());

        GDALArgDatasetValue value2;
        value2 = std::move(value);
        EXPECT_STREQ(value2.GetName().c_str(), poDSRaw->GetDescription());

        poDSRaw->ReleaseRef();
    }
    {
        GDALArgDatasetValue value("foo");
        EXPECT_STREQ(value.GetName().c_str(), "foo");

        GDALArgDatasetValue value2;
        value2 = std::move(value);
        EXPECT_STREQ(value2.GetName().c_str(), "foo");
    }
}

TEST_F(test_gdal_algorithm, bool_flag)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;

        MyAlgorithm()
        {
            AddArg("flag", 'f', "boolean flag", &m_flag);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(true);
        alg.GetUsageForCLI(false);
        EXPECT_TRUE(alg.ParseCommandLineArguments({}));
        EXPECT_STREQ(alg.GetActualAlgorithm().GetName().c_str(), "test");
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--flag"}));
        EXPECT_TRUE(alg.m_flag);
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--flag=true"}));
        EXPECT_TRUE(alg.m_flag);
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--flag=false"}));
        EXPECT_FALSE(alg.m_flag);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--flag=invalid"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--flag", "--flag"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--invalid"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"-"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"-x"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"-xy"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, int)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        int m_val = 0;

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val);
        }
    };

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--val=5"}));
        EXPECT_EQ(alg.m_val, 5);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        // Missing value
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_EQ(alg.m_val, 0);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=invalid"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_EQ(alg.m_val, 0);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(
            alg.ParseCommandLineArguments({"--val=12345679812346798123456"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_EQ(alg.m_val, 0);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=1.5"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_EQ(alg.m_val, 0);
    }
}

TEST_F(test_gdal_algorithm, SetDisplayInJSONUsage)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        MyAlgorithm()
        {
            SetDisplayInJSONUsage(false);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);
        alg.GetUsageAsJSON();
    }
}

TEST_F(test_gdal_algorithm, int_with_default)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        int m_val = 0;

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val).SetDefault(3);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);
        alg.GetUsageAsJSON();
        EXPECT_TRUE(alg.ValidateArguments());
        EXPECT_EQ(alg.m_val, 3);
    }
}

TEST_F(test_gdal_algorithm, double)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        double m_val = 0;

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val);
        }
    };

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--val=1.5"}));
        EXPECT_EQ(alg.m_val, 1.5);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=invalid"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_EQ(alg.m_val, 0);
    }
}

TEST_F(test_gdal_algorithm, double_with_default)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        double m_val = 0;

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val).SetDefault(3.5);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);
        alg.GetUsageAsJSON();
        EXPECT_TRUE(alg.ValidateArguments());
        EXPECT_EQ(alg.m_val, 3.5);
    }
}

TEST_F(test_gdal_algorithm, string_with_default)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val).SetDefault("foo");
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);
        alg.GetUsageAsJSON();
        EXPECT_TRUE(alg.ValidateArguments());
        EXPECT_STREQ(alg.m_val.c_str(), "foo");
    }
}

TEST_F(test_gdal_algorithm, dataset)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        GDALArgDatasetValue m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val).SetRequired();
        }
    };

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments(
            {"--val=" GCORE_DATA_DIR "byte.tif"}));
        EXPECT_NE(alg.m_val.GetDatasetRef(), nullptr);
    }

    {
        MyAlgorithm alg;
        auto poDS = std::unique_ptr<GDALDataset>(
            GetGDALDriverManager()->GetDriverByName("MEM")->Create(
                "", 1, 1, 1, GDT_Byte, nullptr));
        auto poDSRaw = poDS.get();
        alg.GetArg("val")->Set(poDS.release());
        EXPECT_EQ(alg.m_val.GetDatasetRef(), poDSRaw);
        poDSRaw->ReleaseRef();
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(
            alg.ParseCommandLineArguments({"--val=i_do_not_exist.tif"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.Run());
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        GDALArgDatasetValue value;
        alg.GetArg("val")->SetFrom(value);
        EXPECT_FALSE(alg.Run());
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, input_update)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        GDALArgDatasetValue m_input{};
        GDALArgDatasetValue m_output{};
        bool m_update = false;

        MyAlgorithm()
        {
            AddInputDatasetArg(&m_input);
            AddUpdateArg(&m_update);
        }
    };

    {
        auto poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
        if (!poDriver)
        {
            GTEST_SKIP() << "GPKG support missing";
        }
        else
        {
            std::string osTmpFilename =
                VSIMemGenerateHiddenFilename("temp.gpkg");
            auto poDS = std::unique_ptr<GDALDataset>(poDriver->Create(
                osTmpFilename.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
            poDS->CreateLayer("foo");
            poDS.reset();

            MyAlgorithm alg;
            EXPECT_TRUE(!alg.GetUsageAsJSON().empty());
            EXPECT_TRUE(
                alg.ParseCommandLineArguments({"--update", osTmpFilename}));
            ASSERT_NE(alg.m_input.GetDatasetRef(), nullptr);
            EXPECT_EQ(alg.m_input.GetDatasetRef()->GetAccess(), GA_Update);

            alg.Finalize();

            VSIUnlink(osTmpFilename.c_str());
        }
    }
}

TEST_F(test_gdal_algorithm, same_input_output_dataset_sqlite)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        GDALArgDatasetValue m_input{};
        GDALArgDatasetValue m_output{};
        bool m_update = false;

        MyAlgorithm()
        {
            AddInputDatasetArg(&m_input);
            AddOutputDatasetArg(&m_output);
            m_output.SetInputFlags(GADV_NAME | GADV_OBJECT);
            AddUpdateArg(&m_update);
        }
    };

    {
        auto poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
        if (!poDriver)
        {
            GTEST_SKIP() << "GPKG support missing";
        }
        else
        {
            std::string osTmpFilename =
                VSIMemGenerateHiddenFilename("temp.gpkg");
            auto poDS = std::unique_ptr<GDALDataset>(poDriver->Create(
                osTmpFilename.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
            poDS->CreateLayer("foo");
            poDS.reset();

            MyAlgorithm alg;
            EXPECT_TRUE(alg.ParseCommandLineArguments(
                {"--update", osTmpFilename, osTmpFilename}));
            ASSERT_NE(alg.m_input.GetDatasetRef(), nullptr);
            EXPECT_NE(alg.m_output.GetDatasetRef(), nullptr);
            EXPECT_EQ(alg.m_input.GetDatasetRef(),
                      alg.m_output.GetDatasetRef());
            EXPECT_EQ(alg.m_input.GetDatasetRef()->GetAccess(), GA_Update);

            alg.Finalize();

            VSIUnlink(osTmpFilename.c_str());
        }
    }
}

TEST_F(test_gdal_algorithm, output_dataset_created_by_alg)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        GDALArgDatasetValue m_output{};

        MyAlgorithm()
        {
            AddOutputDatasetArg(&m_output);
            m_output.SetInputFlags(GADV_NAME);
            m_output.SetOutputFlags(GADV_OBJECT);
        }
    };

    MyAlgorithm alg;
    alg.GetUsageForCLI(false);
}

TEST_F(test_gdal_algorithm, string_choices)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val)
                .SetChoices("foo", "bar")
                .SetHiddenChoices("baz");
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({"--val=foo"}));
        EXPECT_STREQ(alg.m_val.c_str(), "foo");
    }

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({"--val=baz"}));
        EXPECT_STREQ(alg.m_val.c_str(), "baz");
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=invalid"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_TRUE(alg.m_val.empty());
    }
}

TEST_F(test_gdal_algorithm, vector_int)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<int> m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val);
        }
    };

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--val=5,6"}));
        auto expected = std::vector<int>{5, 6};
        EXPECT_EQ(alg.m_val, expected);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=1,foo"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_TRUE(alg.m_val.empty());
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(
            alg.ParseCommandLineArguments({"--val=1,12345679812346798123456"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_TRUE(alg.m_val.empty());
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=1", "--val=foo"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_TRUE(alg.m_val.empty());
    }
}

TEST_F(test_gdal_algorithm, vector_int_validation_fails)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<int> m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val)
                .AddValidationAction(
                    []()
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "validation failed");
                        return false;
                    });
        }
    };

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=5", "--val=6"}));
        EXPECT_STREQ(CPLGetLastErrorMsg(), "validation failed");
    }
}

TEST_F(test_gdal_algorithm, vector_double)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<double> m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val);
        }
    };

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--val=1.5,2.5"}));
        auto expected = std::vector<double>{1.5, 2.5};
        EXPECT_EQ(alg.m_val, expected);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=1,foo"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_TRUE(alg.m_val.empty());
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=1", "--val=foo"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_TRUE(alg.m_val.empty());
    }
}

TEST_F(test_gdal_algorithm, vector_double_validation_fails)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<double> m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val)
                .AddValidationAction(
                    []()
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "validation failed");
                        return false;
                    });
        }
    };

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=5", "--val=6"}));
        EXPECT_STREQ(CPLGetLastErrorMsg(), "validation failed");
    }
}

TEST_F(test_gdal_algorithm, vector_string)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val);
        }
    };

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--val=foo,bar"}));
        auto expected = std::vector<std::string>{"foo", "bar"};
        EXPECT_EQ(alg.m_val, expected);
    }
}

TEST_F(test_gdal_algorithm, vector_string_validation_fails)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val)
                .AddValidationAction(
                    []()
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "validation failed");
                        return false;
                    });
        }
    };

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=foo", "--val=bar"}));
        EXPECT_STREQ(CPLGetLastErrorMsg(), "validation failed");
    }
}

TEST_F(test_gdal_algorithm, vector_string_choices)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val).SetChoices("foo", "bar");
        }
    };

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--val=foo,bar"}));
        auto expected = std::vector<std::string>{"foo", "bar"};
        EXPECT_EQ(alg.m_val, expected);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=foo,invalid"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_TRUE(alg.m_val.empty());
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(
            alg.ParseCommandLineArguments({"--val=foo", "--val=invalid"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        EXPECT_TRUE(alg.m_val.empty());
    }
}

TEST_F(test_gdal_algorithm, vector_dataset)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<GDALArgDatasetValue> m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val);
        }
    };

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments(
            {"--val=" GCORE_DATA_DIR "byte.tif"}));
        ASSERT_EQ(alg.m_val.size(), 1U);
        EXPECT_NE(alg.m_val[0].GetDatasetRef(), nullptr);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=non_existing.tif"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
        ASSERT_EQ(alg.m_val.size(), 1U);
        EXPECT_EQ(alg.m_val[0].GetDatasetRef(), nullptr);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        alg.GetArg("val")->Set(std::vector<GDALArgDatasetValue>(1));
        EXPECT_FALSE(alg.Run());
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, vector_dataset_validation_fails)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<GDALArgDatasetValue> m_val{};

        MyAlgorithm()
        {
            AddArg("val", 0, "", &m_val)
                .AddValidationAction(
                    []()
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "validation failed");
                        return false;
                    });
        }
    };

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--val=foo", "--val=bar"}));
        EXPECT_STREQ(CPLGetLastErrorMsg(), "validation failed");
    }
}

TEST_F(test_gdal_algorithm, vector_input)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<GDALArgDatasetValue> m_input{};
        std::vector<std::string> m_oo{};
        std::vector<std::string> m_if{};
        bool m_update = false;

        MyAlgorithm()
        {
            AddInputDatasetArg(&m_input);
            AddOpenOptionsArg(&m_oo);
            AddInputFormatsArg(&m_if);
            AddUpdateArg(&m_update);
        }
    };

    {
        auto poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
        if (!poDriver)
        {
            GTEST_SKIP() << "GPKG support missing";
        }
        else
        {
            std::string osTmpFilename =
                VSIMemGenerateHiddenFilename("temp.gpkg");
            auto poDS = std::unique_ptr<GDALDataset>(poDriver->Create(
                osTmpFilename.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
            poDS->CreateLayer("foo");
            poDS.reset();

            MyAlgorithm alg;
            EXPECT_TRUE(alg.ParseCommandLineArguments(
                {"--update", "--oo=LIST_ALL_TABLES=YES", "--if=GPKG",
                 osTmpFilename}));
            ASSERT_EQ(alg.m_input.size(), 1U);
            ASSERT_NE(alg.m_input[0].GetDatasetRef(), nullptr);
            EXPECT_EQ(alg.m_input[0].GetDatasetRef()->GetAccess(), GA_Update);

            alg.Finalize();

            VSIUnlink(osTmpFilename.c_str());
        }
    }
}

TEST_F(test_gdal_algorithm, several_values)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_co{};

        MyAlgorithm()
        {
            AddArg("co", 0, "creation options", &m_co);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--co", "FOO=BAR"}));
        EXPECT_EQ(alg.m_co, std::vector<std::string>{"FOO=BAR"});
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--co=FOO=BAR"}));
        EXPECT_EQ(alg.m_co, std::vector<std::string>{"FOO=BAR"});
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--co=FOO=BAR,BAR=BAZ"}));
        auto expected = std::vector<std::string>{"FOO=BAR", "BAR=BAZ"};
        EXPECT_EQ(alg.m_co, expected);
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(
            alg.ParseCommandLineArguments({"--co=FOO=BAR", "--co", "BAR=BAZ"}));
        auto expected = std::vector<std::string>{"FOO=BAR", "BAR=BAZ"};
        EXPECT_EQ(alg.m_co, expected);
    }
}

TEST_F(test_gdal_algorithm, required_arg)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_arg{};

        MyAlgorithm()
        {
            AddArg("arg", 0, "required arg", &m_arg).SetRequired();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--arg", "foo"}));
        EXPECT_STREQ(alg.m_arg.c_str(), "foo");
    }
}

TEST_F(test_gdal_algorithm, single_positional_arg)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_value{};

        MyAlgorithm()
        {
            AddArg("input", 0, "input value", &m_value).SetPositional();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"my_input"}));
        EXPECT_TRUE(alg.GetArg("input")->IsExplicitlySet());
        EXPECT_STREQ(alg.GetArg("input")->Get<std::string>().c_str(),
                     "my_input");
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--input", "my_input"}));
        EXPECT_STREQ(alg.m_value.c_str(), "my_input");
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--input=my_input"}));
        EXPECT_STREQ(alg.m_value.c_str(), "my_input");
    }
}

TEST_F(test_gdal_algorithm, single_positional_arg_required)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_value{};

        MyAlgorithm()
        {
            AddArg("input", 0, "input value", &m_value)
                .SetPositional()
                .SetRequired();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--input=my_input"}));
        EXPECT_STREQ(alg.m_value.c_str(), "my_input");
    }
}

TEST_F(test_gdal_algorithm, two_positional_arg)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_input_value{};
        std::string m_output_value{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_value).SetPositional();
            AddArg("output", 'o', "output value", &m_output_value)
                .SetPositional();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"my_input"}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"-i", "my_input"}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"my_input", "my_output"}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
        EXPECT_STREQ(alg.m_output_value.c_str(), "my_output");
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments(
            {"--input", "my_input", "-o", "my_output"}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
        EXPECT_STREQ(alg.m_output_value.c_str(), "my_output");
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments(
            {"-o", "my_output", "--input", "my_input"}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
        EXPECT_STREQ(alg.m_output_value.c_str(), "my_output");
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(
            alg.ParseCommandLineArguments({"-o", "my_output", "my_input"}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
        EXPECT_STREQ(alg.m_output_value.c_str(), "my_output");
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(
            alg.ParseCommandLineArguments({"my_input", "-o", "my_output"}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
        EXPECT_STREQ(alg.m_output_value.c_str(), "my_output");
    }

    {
        MyAlgorithm alg;
        alg.GetArg("input")->Set("my_input");
        EXPECT_TRUE(alg.ParseCommandLineArguments({"my_output"}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
        EXPECT_STREQ(alg.m_output_value.c_str(), "my_output");
    }

    {
        MyAlgorithm alg;
        alg.GetArg("input")->Set("my_input");
        alg.GetArg("output")->Set("my_output");
        EXPECT_TRUE(alg.ParseCommandLineArguments({}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
        EXPECT_STREQ(alg.m_output_value.c_str(), "my_output");
    }

    {
        MyAlgorithm alg;
        alg.GetArg("input")->Set("my_input");
        alg.GetArg("output")->Set("my_output");
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"unexpected"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"foo", "bar", "baz"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, two_positional_arg_first_two_values)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<int> m_input_value{};
        std::string m_output_value{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_value)
                .SetPositional()
                .SetMinCount(2)
                .SetMaxCount(2)
                .SetDisplayHintAboutRepetition(false);
            AddArg("output", 'o', "output value", &m_output_value)
                .SetPositional();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({"1", "2", "baz"}));
        auto expected = std::vector<int>{1, 2};
        EXPECT_EQ(alg.m_input_value, expected);
        EXPECT_STREQ(alg.m_output_value.c_str(), "baz");
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"1"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"1", "foo"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, unlimited_input_single_output)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_input_values{};
        std::string m_output_value{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_values)
                .SetPositional();
            AddArg("output", 'o', "output value", &m_output_value)
                .SetPositional()
                .SetRequired();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(
            alg.ParseCommandLineArguments({"input1", "input2", "my_output"}));
        auto expected = std::vector<std::string>{"input1", "input2"};
        EXPECT_EQ(alg.m_input_values, expected);
        EXPECT_STREQ(alg.m_output_value.c_str(), "my_output");
    }
}

TEST_F(test_gdal_algorithm, single_input_unlimited_outputs)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_input_value{};
        std::vector<std::string> m_output_values{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_value)
                .SetPositional()
                .SetRequired();
            AddArg("output", 'o', "output value", &m_output_values)
                .SetPositional();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments(
            {"my_input", "my_output1", "my_output2"}));
        EXPECT_STREQ(alg.m_input_value.c_str(), "my_input");
        auto expected = std::vector<std::string>{"my_output1", "my_output2"};
        EXPECT_EQ(alg.m_output_values, expected);
    }
}

TEST_F(test_gdal_algorithm, min_max_count)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_arg{};

        MyAlgorithm()
        {
            AddArg("arg", 0, "arg", &m_arg)
                .SetRequired()
                .SetMinCount(2)
                .SetMaxCount(3);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--arg=foo"}));
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--arg=1,2,3,4"}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--arg=foo,bar"}));
        auto expected = std::vector<std::string>{"foo", "bar"};
        EXPECT_EQ(alg.m_arg, expected);
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments(
            {"--arg", "foo", "--arg", "bar", "--arg", "baz"}));
        auto expected = std::vector<std::string>{"foo", "bar", "baz"};
        EXPECT_EQ(alg.m_arg, expected);
    }
}

TEST_F(test_gdal_algorithm, min_max_count_equal)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_arg{};

        MyAlgorithm()
        {
            AddArg("arg", 0, "arg", &m_arg)
                .SetRequired()
                .SetMinCount(2)
                .SetMaxCount(2);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        alg.GetArg("arg")->Set(std::vector<std::string>{"foo"});
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ValidateArguments());
        EXPECT_STREQ(CPLGetLastErrorMsg(),
                     "test: 1 value(s) have been specified for argument 'arg', "
                     "whereas exactly 2 were expected.");
    }
}

TEST_F(test_gdal_algorithm, repeated_arg_allowed_false)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_arg{};

        MyAlgorithm()
        {
            AddArg("arg", 0, "arg", &m_arg)
                .SetRepeatedArgAllowed(false)
                .SetMinCount(2)
                .SetMaxCount(3);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--arg=foo,bar"}));
        auto expected = std::vector<std::string>{"foo", "bar"};
        EXPECT_EQ(alg.m_arg, expected);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--arg=foo", "--arg=bar"}));
    }
}

TEST_F(test_gdal_algorithm, ambiguous_positional_unlimited_and_then_varying)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_input_values{};
        std::vector<std::string> m_output_values{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_values)
                .SetPositional();
            AddArg("output", 'o', "output value", &m_output_values)
                .SetPositional()
                .SetMinCount(2)
                .SetMaxCount(3);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments(
            {"my_input", "my_output1", "my_output2"}));
        EXPECT_STREQ(
            CPLGetLastErrorMsg(),
            "test: Ambiguity in definition of positional argument 'output' "
            "given it has a varying number of values, but follows argument "
            "'input' which also has a varying number of values");
    }
}

TEST_F(test_gdal_algorithm,
       ambiguous_positional_unlimited_and_then_non_required)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_input_values{};
        std::string m_output_value{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_values)
                .SetPositional();
            AddArg("output", 'o', "output value", &m_output_value)
                .SetPositional();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments(
            {"my_input1", "my_input2", "my_output"}));
        EXPECT_STREQ(CPLGetLastErrorMsg(),
                     "test: Ambiguity in definition of positional argument "
                     "'output', given it is not required but follows argument "
                     "'input' which has a varying number of values");
    }
}

TEST_F(test_gdal_algorithm,
       ambiguous_positional_fixed_then_unlimited_then_fixed)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_input_value{};
        std::vector<std::string> m_something{};
        std::string m_output_value{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_value).SetPositional();
            AddArg("something", 0, "something", &m_something).SetPositional();
            AddArg("output", 'o', "output value", &m_output_value)
                .SetPositional();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments(
            {"my_input", "something", "my_output"}));
        // Actually this is not ambiguous here, but our parser does not support
        // that for now
        EXPECT_STREQ(
            CPLGetLastErrorMsg(),
            "test: Ambiguity in definition of positional arguments: arguments "
            "with varying number of values must be first or last one.");
    }
}

TEST_F(test_gdal_algorithm, positional_unlimited_and_then_2)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_input_values{};
        std::vector<std::string> m_output_values{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_values)
                .SetPositional();
            AddArg("output", 'o', "output value", &m_output_values)
                .SetPositional()
                .SetMinCount(2)
                .SetMaxCount(2);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({"my_input1", "my_input2",
                                                   "my_input3", "my_output1",
                                                   "my_output2"}));
        EXPECT_EQ(alg.m_input_values.size(), 3U);
        EXPECT_EQ(alg.m_output_values.size(), 2U);
    }

    {
        MyAlgorithm alg;

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"my_output1"}));
        EXPECT_STREQ(CPLGetLastErrorMsg(),
                     "test: Not enough positional values.");
    }
}

TEST_F(test_gdal_algorithm, positional_unlimited_validation_error_and_then_2)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_input_values{};
        std::vector<std::string> m_output_values{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_values)
                .SetPositional()
                .AddValidationAction(
                    []()
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "validation failed");
                        return false;
                    });
            AddArg("output", 'o', "output value", &m_output_values)
                .SetPositional()
                .SetMinCount(2)
                .SetMaxCount(2);
        }
    };

    {
        MyAlgorithm alg;

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"my_input1", "my_input2",
                                                    "my_input3", "my_output1",
                                                    "my_output2"}));
        EXPECT_STREQ(CPLGetLastErrorMsg(), "validation failed");
    }
}

TEST_F(test_gdal_algorithm,
       positional_unlimited_validation_error_and_then_required)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_input_values{};
        std::string m_output_value{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_values)
                .SetPositional()
                .SetChoices("foo");
            AddArg("output", 'o', "output value", &m_output_value)
                .SetPositional()
                .SetRequired();
        }
    };

    {
        MyAlgorithm alg;

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(
            alg.ParseCommandLineArguments({"foo", "bar", "my_output"}));
        EXPECT_STREQ(CPLGetLastErrorMsg(),
                     "test: Invalid value 'bar' for string argument 'input'. "
                     "Should be one among 'foo'.");
    }
}

TEST_F(test_gdal_algorithm,
       positional_required_and_then_unlimited_validation_error)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_input_value{};
        std::vector<std::string> m_output_values{};

        MyAlgorithm()
        {
            AddArg("input", 'i', "input value", &m_input_value)
                .SetPositional()
                .SetRequired();
            AddArg("output", 'o', "output values", &m_output_values)
                .SetPositional()
                .SetChoices("foo");
        }
    };

    {
        MyAlgorithm alg;

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(
            alg.ParseCommandLineArguments({"something", "foo", "bar"}));
        EXPECT_STREQ(CPLGetLastErrorMsg(),
                     "test: Invalid value 'bar' for string argument 'output'. "
                     "Should be one among 'foo'.");
    }
}

TEST_F(test_gdal_algorithm, packed_values_allowed_false)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_arg{};

        MyAlgorithm()
        {
            AddArg("arg", 0, "arg", &m_arg)
                .SetPackedValuesAllowed(false)
                .SetMinCount(2)
                .SetMaxCount(3);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--arg=foo", "--arg=bar"}));
        auto expected = std::vector<std::string>{"foo", "bar"};
        EXPECT_EQ(alg.m_arg, expected);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--arg=foo,bar"}));
    }
}

TEST_F(test_gdal_algorithm, actions)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;
        bool m_flagSpecified = false;

        MyAlgorithm()
        {
            AddArg("flag", 'f', "boolean flag", &m_flag)
                .AddAction([this]() { m_flagSpecified = true; });
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({"--flag"}));
        EXPECT_TRUE(alg.m_flag);
        EXPECT_TRUE(alg.m_flagSpecified);
    }
}

TEST_F(test_gdal_algorithm, various)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag = false;

        MyAlgorithm()
        {
            AddProgressArg();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({}));
        // Parse again
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        EXPECT_FALSE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"-h"}));
        EXPECT_TRUE(alg.IsHelpRequested());
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--help"}));
        EXPECT_TRUE(alg.IsHelpRequested());
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"help"}));
        EXPECT_TRUE(alg.IsHelpRequested());
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--json-usage"}));
        EXPECT_TRUE(alg.IsJSONUsageRequested());
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--progress"}));
        EXPECT_TRUE(alg.IsProgressBarRequested());
    }
}

TEST_F(test_gdal_algorithm, mutually_exclusive)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_flag1 = false;
        bool m_flag2 = false;
        bool m_flag3 = false;

        MyAlgorithm()
        {
            AddArg("flag1", 0, "", &m_flag1)
                .SetMutualExclusionGroup("my_group");
            AddArg("flag2", 0, "", &m_flag2)
                .SetMutualExclusionGroup("my_group");
            AddArg("flag3", 0, "", &m_flag3)
                .SetMutualExclusionGroup("my_group");
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--flag1"}));
    }

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--flag2"}));
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--flag1", "--flag2"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, invalid_input_format)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_if{};

        MyAlgorithm()
        {
            AddInputFormatsArg(&m_if).AddMetadataItem(
                GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR});
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--if=I_DO_NOT_EXIST"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--if=GTIFF"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, arg_layer_name_single)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::string m_layerName{};

        MyAlgorithm()
        {
            AddLayerNameArg(&m_layerName);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments({"-l", "foo"}));
        EXPECT_STREQ(alg.m_layerName.c_str(), "foo");
    }
}

TEST_F(test_gdal_algorithm, arg_layer_name_multiple)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_layerNames{};

        MyAlgorithm()
        {
            AddLayerNameArg(&m_layerNames);
        }
    };

    {
        MyAlgorithm alg;
        EXPECT_TRUE(alg.ParseCommandLineArguments({"-l", "foo", "-l", "bar"}));
        EXPECT_EQ(alg.m_layerNames.size(), 2U);
    }
}

TEST_F(test_gdal_algorithm, arg_co)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_co{};

        MyAlgorithm()
        {
            AddCreationOptionsArg(&m_co);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments(
            {"--co", "foo=bar", "--co", "bar=baz"}));
        EXPECT_EQ(alg.m_co.size(), 2U);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--co", "foo"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, arg_lco)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        std::vector<std::string> m_lco{};

        MyAlgorithm()
        {
            AddLayerCreationOptionsArg(&m_lco);
        }
    };

    {
        MyAlgorithm alg;
        alg.GetUsageForCLI(false);

        EXPECT_TRUE(alg.ParseCommandLineArguments(
            {"--lco", "foo=bar", "--lco", "bar=baz"}));
        EXPECT_EQ(alg.m_lco.size(), 2U);
    }

    {
        MyAlgorithm alg;
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--lco", "foo"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, SetHiddenForCLI)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_b = false;

        MyAlgorithm()
        {
            AddArg("flag", 0, "", &m_b)
                .SetHiddenForCLI()
                .SetCategory(GAAC_ESOTERIC);
        }
    };

    MyAlgorithm alg;
    alg.GetUsageForCLI(false);
}

TEST_F(test_gdal_algorithm, SetOnlyForCLI)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        bool m_b = false;

        MyAlgorithm()
        {
            AddArg("flag", 0, "", &m_b)
                .SetOnlyForCLI()
                .SetCategory("my category");
            m_longDescription = "long description";
        }
    };

    MyAlgorithm alg;
    alg.GetUsageForCLI(false);
}

TEST_F(test_gdal_algorithm, SetSkipIfAlreadySet)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        int m_val = 0;

        MyAlgorithm()
        {
            AddArg("option", 0, "option", &m_val).SetPositional();
        }
    };

    {
        MyAlgorithm alg;
        alg.GetArg("option")->Set(1);
        alg.GetArg("option")->SetSkipIfAlreadySet();
        EXPECT_TRUE(alg.ParseCommandLineArguments({"--option=1"}));
    }

    {
        MyAlgorithm alg;
        alg.GetArg("option")->Set(1);
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"--option=1"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }
}

TEST_F(test_gdal_algorithm, alg_with_aliases)
{
    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        int m_val = 0;

        MyAlgorithm()
        {
            m_aliases.push_back("one_alias");
            m_aliases.push_back(GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR);
            m_aliases.push_back("hidden_alias");
        }
    };

    MyAlgorithm alg;
    alg.GetUsageForCLI(false);
    EXPECT_EQ(alg.GetAliases().size(), 3U);
}

TEST_F(test_gdal_algorithm, subalgorithms)
{
    bool hasRun = false;

    class SubAlgorithm : public GDALAlgorithm
    {
      public:
        bool &m_bHasRun;
        bool m_flag = false;

        SubAlgorithm(bool &lHasRun)
            : GDALAlgorithm("subalg", "", "https://example.com"),
              m_bHasRun(lHasRun)
        {
            AddProgressArg();
            m_aliases.push_back("one_alias");
            m_aliases.push_back(GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR);
            m_aliases.push_back("hidden_alias");
        }

        bool RunImpl(GDALProgressFunc, void *) override
        {
            m_bHasRun = true;
            return true;
        }
    };

    class MyAlgorithm : public MyAlgorithmWithDummyRun
    {
      public:
        MyAlgorithm(bool &lHasRun)
        {
            GDALAlgorithmRegistry::AlgInfo info;
            info.m_name = "subalg";
            info.m_creationFunc = [&lHasRun]()
            { return std::make_unique<SubAlgorithm>(lHasRun); };
            RegisterSubAlgorithm(info);
            // RegisterSubAlgorithm(SubAlgorithm);
        }
    };

    {
        MyAlgorithm alg(hasRun);
        alg.GetUsageForCLI(false);

        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg(hasRun);
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        CPLErrorReset();
        EXPECT_FALSE(alg.ParseCommandLineArguments({"invalid_subcommand"}));
        EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    }

    {
        MyAlgorithm alg(hasRun);
        alg.SetCallPath(std::vector<std::string>{"main"});
        EXPECT_TRUE(alg.ParseCommandLineArguments({"subalg"}));
        EXPECT_STREQ(alg.GetActualAlgorithm().GetName().c_str(), "subalg");
        EXPECT_TRUE(alg.ValidateArguments());
        EXPECT_TRUE(alg.Run());
        EXPECT_TRUE(hasRun);
        EXPECT_TRUE(alg.Finalize());
        alg.GetUsageForCLI(false);
    }

    {
        MyAlgorithm alg(hasRun);
        EXPECT_TRUE(alg.ParseCommandLineArguments({"subalg", "-h"}));
        EXPECT_TRUE(alg.IsHelpRequested());
        EXPECT_TRUE(alg.ValidateArguments());
        alg.GetUsageForCLI(false);
    }

    {
        MyAlgorithm alg(hasRun);
        EXPECT_TRUE(alg.ParseCommandLineArguments({"subalg", "--progress"}));
        EXPECT_TRUE(alg.IsProgressBarRequested());
        EXPECT_TRUE(alg.ValidateArguments());
        alg.GetUsageForCLI(false);
    }
}

class MyRedundantRasterAlgorithm : public MyAlgorithmWithDummyRun
{
  public:
    static constexpr const char *NAME = "raster";
    static constexpr const char *DESCRIPTION =
        "redundant with existing raster!!!";
    static constexpr const char *HELP_URL = "";

    static std::vector<std::string> GetAliases()
    {
        return {};
    }
};

class MyAlgorithmWithAlias : public MyAlgorithmWithDummyRun
{
  public:
    static constexpr const char *NAME = "MyAlgorithmWithAlias";
    static constexpr const char *DESCRIPTION = "";
    static constexpr const char *HELP_URL = "";

    static std::vector<std::string> GetAliases()
    {
        return {"alias", GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR,
                "hidden_alias"};
    }
};

class MyAlgorithmWithRedundantAlias : public MyAlgorithmWithDummyRun
{
  public:
    static constexpr const char *NAME = "MyAlgorithmWithRedundantAlias";
    static constexpr const char *DESCRIPTION = "";
    static constexpr const char *HELP_URL = "";

    static std::vector<std::string> GetAliases()
    {
        return {"alias"};
    }
};

class MyAlgorithmWithRedundantHiddenAlias : public MyAlgorithmWithDummyRun
{
  public:
    static constexpr const char *NAME = "MyAlgorithmWithRedundantHiddenAlias";
    static constexpr const char *DESCRIPTION = "";
    static constexpr const char *HELP_URL = "";

    static std::vector<std::string> GetAliases()
    {
        return {GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR, "hidden_alias"};
    }
};

TEST_F(test_gdal_algorithm, GDALGlobalAlgorithmRegistry)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    EXPECT_NE(singleton.GetInfo("raster"), nullptr);
    EXPECT_EQ(singleton.GetInfo("not_existing"), nullptr);
    auto alg = singleton.Instantiate("raster");
    ASSERT_NE(alg, nullptr);
    EXPECT_TRUE(!alg->GetUsageAsJSON().empty());

    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        EXPECT_FALSE(singleton.Register<MyRedundantRasterAlgorithm>());
    }

    EXPECT_TRUE(singleton.Register<MyAlgorithmWithAlias>());
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        EXPECT_FALSE(singleton.Register<MyAlgorithmWithRedundantAlias>());
    }
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        EXPECT_FALSE(singleton.Register<MyAlgorithmWithRedundantHiddenAlias>());
    }
}

TEST_F(test_gdal_algorithm, vector_pipeline_GetUsageForCLI)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    auto vector = singleton.Instantiate("vector");
    ASSERT_NE(vector, nullptr);
    auto pipeline = vector->InstantiateSubAlgorithm("pipeline");
    ASSERT_NE(pipeline, nullptr);
    pipeline->GetUsageForCLI(false);
    pipeline->GetUsageForCLI(true);
}

TEST_F(test_gdal_algorithm, raster_pipeline_GetUsageForCLI)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    auto raster = singleton.Instantiate("raster");
    ASSERT_NE(raster, nullptr);
    auto pipeline = raster->InstantiateSubAlgorithm("pipeline");
    ASSERT_NE(pipeline, nullptr);
    pipeline->GetUsageForCLI(false);
    pipeline->GetUsageForCLI(true);
}

TEST_F(test_gdal_algorithm, registry_c_api)
{
    auto reg = GDALGetGlobalAlgorithmRegistry();
    ASSERT_NE(reg, nullptr);
    char **names = GDALAlgorithmRegistryGetAlgNames(reg);
    EXPECT_GE(CSLCount(names), 2);
    CSLDestroy(names);
    auto alg = GDALAlgorithmRegistryInstantiateAlg(reg, "raster");
    ASSERT_NE(alg, nullptr);
    EXPECT_EQ(GDALAlgorithmRegistryInstantiateAlg(reg, "not_existing"),
              nullptr);
    GDALAlgorithmRelease(alg);
    GDALAlgorithmRegistryRelease(reg);
}

TEST_F(test_gdal_algorithm, algorithm_c_api)
{
    class MyAlgorithm : public GDALAlgorithm
    {
      public:
        bool m_flag = false;
        std::string m_str{};
        int m_int = 0;
        double m_double = 0;
        std::vector<std::string> m_strlist{};
        std::vector<int> m_intlist{};
        std::vector<double> m_doublelist{};
        GDALArgDatasetValue m_dsValue{};

        bool m_hasParsedCommandLinearguments = false;
        bool m_hasRun = false;
        bool m_hasFinalized = false;

        MyAlgorithm()
            : GDALAlgorithm("test", "description", "http://example.com")
        {
            m_longDescription = "long description";
            AddArg("flag", 'f', "boolean flag", &m_flag);
            AddArg("str", 0, "str", &m_str);
            AddArg("int", 0, "int", &m_int);
            AddArg("double", 0, "double", &m_double);
            AddArg("strlist", 0, "strlist", &m_strlist);
            AddArg("doublelist", 0, "doublelist", &m_doublelist);
            AddArg("intlist", 0, "intlist", &m_intlist);
            AddArg("dataset", 0, "dataset", &m_dsValue);
        }

        bool
        ParseCommandLineArguments(const std::vector<std::string> &args) override
        {
            m_hasParsedCommandLinearguments = true;
            return GDALAlgorithm::ParseCommandLineArguments(args);
        }

        bool RunImpl(GDALProgressFunc, void *) override
        {
            m_hasRun = true;
            return true;
        }

        bool Finalize() override
        {
            m_hasFinalized = true;
            return GDALAlgorithm::Finalize();
        }
    };

    auto hAlg =
        std::make_unique<GDALAlgorithmHS>(std::make_unique<MyAlgorithm>());
    MyAlgorithm *pAlg = cpl::down_cast<MyAlgorithm *>(hAlg->ptr);
    EXPECT_STREQ(GDALAlgorithmGetName(hAlg.get()), "test");
    EXPECT_STREQ(GDALAlgorithmGetDescription(hAlg.get()), "description");
    EXPECT_STREQ(GDALAlgorithmGetLongDescription(hAlg.get()),
                 "long description");
    EXPECT_STREQ(GDALAlgorithmGetHelpFullURL(hAlg.get()), "http://example.com");
    EXPECT_FALSE(GDALAlgorithmHasSubAlgorithms(hAlg.get()));
    EXPECT_EQ(GDALAlgorithmGetSubAlgorithmNames(hAlg.get()), nullptr);
    EXPECT_EQ(GDALAlgorithmInstantiateSubAlgorithm(hAlg.get(), "not_existing"),
              nullptr);
    EXPECT_TRUE(GDALAlgorithmParseCommandLineArguments(
        hAlg.get(), CPLStringList(std::vector<std::string>({"-f"})).List()));
    EXPECT_TRUE(pAlg->m_hasParsedCommandLinearguments);
    EXPECT_TRUE(GDALAlgorithmRun(hAlg.get(), nullptr, nullptr));
    EXPECT_TRUE(pAlg->m_hasRun);
    EXPECT_TRUE(GDALAlgorithmFinalize(hAlg.get()));
    EXPECT_TRUE(pAlg->m_hasFinalized);
    char *jsonUsage = GDALAlgorithmGetUsageAsJSON(hAlg.get());
    EXPECT_NE(jsonUsage, nullptr);
    CPLFree(jsonUsage);

    char **argNames = GDALAlgorithmGetArgNames(hAlg.get());
    ASSERT_NE(argNames, nullptr);
    EXPECT_EQ(CSLCount(argNames), 13);
    CSLDestroy(argNames);

    EXPECT_EQ(GDALAlgorithmGetArg(hAlg.get(), "non_existing"), nullptr);
    {
        auto hArg = GDALAlgorithmGetArg(hAlg.get(), "flag");
        ASSERT_NE(hArg, nullptr);
        GDALAlgorithmArgSetAsBoolean(hArg, true);
        EXPECT_TRUE(GDALAlgorithmArgGetAsBoolean(hArg));
        GDALAlgorithmArgRelease(hArg);
    }
    {
        auto hArg = GDALAlgorithmGetArg(hAlg.get(), "str");
        ASSERT_NE(hArg, nullptr);
        GDALAlgorithmArgSetAsString(hArg, "foo");
        EXPECT_STREQ(GDALAlgorithmArgGetAsString(hArg), "foo");
        GDALAlgorithmArgRelease(hArg);
    }
    {
        auto hArg = GDALAlgorithmGetArg(hAlg.get(), "int");
        ASSERT_NE(hArg, nullptr);
        GDALAlgorithmArgSetAsInteger(hArg, 2);
        EXPECT_EQ(GDALAlgorithmArgGetAsInteger(hArg), 2);
        GDALAlgorithmArgRelease(hArg);
    }
    {
        auto hArg = GDALAlgorithmGetArg(hAlg.get(), "double");
        ASSERT_NE(hArg, nullptr);
        GDALAlgorithmArgSetAsDouble(hArg, 2.5);
        EXPECT_EQ(GDALAlgorithmArgGetAsDouble(hArg), 2.5);
        GDALAlgorithmArgRelease(hArg);
    }
    {
        auto hArg = GDALAlgorithmGetArg(hAlg.get(), "strlist");
        ASSERT_NE(hArg, nullptr);
        const CPLStringList list(std::vector<std::string>({"foo", "bar"}));
        GDALAlgorithmArgSetAsStringList(hArg, list.List());
        char **ret = GDALAlgorithmArgGetAsStringList(hArg);
        EXPECT_EQ(CSLCount(ret), 2);
        CSLDestroy(ret);
        GDALAlgorithmArgRelease(hArg);
    }
    {
        auto hArg = GDALAlgorithmGetArg(hAlg.get(), "intlist");
        ASSERT_NE(hArg, nullptr);
        std::vector<int> vals{2, 3};
        GDALAlgorithmArgSetAsIntegerList(hArg, vals.size(), vals.data());
        size_t nCount = 0;
        const int *ret = GDALAlgorithmArgGetAsIntegerList(hArg, &nCount);
        ASSERT_EQ(nCount, 2);
        ASSERT_NE(ret, nullptr);
        EXPECT_EQ(ret[0], 2);
        EXPECT_EQ(ret[1], 3);
        GDALAlgorithmArgRelease(hArg);
    }
    {
        auto hArg = GDALAlgorithmGetArg(hAlg.get(), "doublelist");
        ASSERT_NE(hArg, nullptr);
        std::vector<double> vals{2.5, 3.5};
        GDALAlgorithmArgSetAsDoubleList(hArg, vals.size(), vals.data());
        size_t nCount = 0;
        const double *ret = GDALAlgorithmArgGetAsDoubleList(hArg, &nCount);
        ASSERT_EQ(nCount, 2);
        ASSERT_NE(ret, nullptr);
        EXPECT_EQ(ret[0], 2.5);
        EXPECT_EQ(ret[1], 3.5);
        GDALAlgorithmArgRelease(hArg);
    }
    {
        auto hArg = GDALAlgorithmGetArg(hAlg.get(), "dataset");
        ASSERT_NE(hArg, nullptr);
        GDALArgDatasetValueH hVal = GDALArgDatasetValueCreate();
        EXPECT_EQ(GDALArgDatasetValueGetType(hVal),
                  GDAL_OF_RASTER | GDAL_OF_VECTOR | GDAL_OF_MULTIDIM_RASTER);
        EXPECT_EQ(GDALArgDatasetValueGetInputFlags(hVal),
                  GADV_NAME | GADV_OBJECT);
        EXPECT_EQ(GDALArgDatasetValueGetOutputFlags(hVal), GADV_OBJECT);
        GDALArgDatasetValueSetName(hVal, "foo");

        {
            auto poDS = std::unique_ptr<GDALDataset>(
                GetGDALDriverManager()->GetDriverByName("MEM")->Create(
                    "", 1, 1, 1, GDT_Byte, nullptr));
            GDALArgDatasetValueSetDataset(hVal, poDS.release());
        }

        GDALAlgorithmArgSetAsDatasetValue(hArg, hVal);
        GDALArgDatasetValueRelease(hVal);

        hVal = GDALAlgorithmArgGetAsDatasetValue(hArg);
        ASSERT_NE(hVal, nullptr);
        auto hDS = GDALArgDatasetValueGetDatasetRef(hVal);
        EXPECT_NE(hDS, nullptr);
        {
            auto hDS2 = GDALArgDatasetValueGetDatasetIncreaseRefCount(hVal);
            EXPECT_EQ(hDS2, hDS);
            GDALReleaseDataset(hDS2);
        }
        GDALArgDatasetValueRelease(hVal);

        GDALAlgorithmArgSetDataset(hArg, nullptr);

        hVal = GDALAlgorithmArgGetAsDatasetValue(hArg);
        ASSERT_NE(hVal, nullptr);
        EXPECT_EQ(GDALArgDatasetValueGetDatasetRef(hVal), nullptr);
        GDALArgDatasetValueRelease(hVal);

        {
            auto poDS = std::unique_ptr<GDALDataset>(
                GetGDALDriverManager()->GetDriverByName("MEM")->Create(
                    "", 1, 1, 1, GDT_Byte, nullptr));
            GDALAlgorithmArgSetDataset(hArg, poDS.release());
        }

        hVal = GDALAlgorithmArgGetAsDatasetValue(hArg);
        ASSERT_NE(hVal, nullptr);
        EXPECT_NE(GDALArgDatasetValueGetDatasetRef(hVal), nullptr);
        GDALArgDatasetValueRelease(hVal);

        GDALAlgorithmArgRelease(hArg);
    }
}

TEST_F(test_gdal_algorithm, DispatcherGetUsageForCLI)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    {
        auto info = singleton.Instantiate("info");
        info->GetUsageForCLI(false);
    }
    {
        auto info = singleton.Instantiate("info");
        EXPECT_TRUE(info->ParseCommandLineArguments(
            std::vector<std::string>{GCORE_DATA_DIR "byte.tif"}));
        info->GetUsageForCLI(false);
    }
    {
        auto poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
        if (!poDriver)
        {
            GTEST_SKIP() << "GPKG support missing";
        }
        else
        {
            std::string osTmpFilename =
                VSIMemGenerateHiddenFilename("temp.gpkg");
            auto poDS = std::unique_ptr<GDALDataset>(poDriver->Create(
                osTmpFilename.c_str(), 1, 1, 1, GDT_Byte, nullptr));
            double adfGT[] = {1, 1, 0, 1, 0, -1};
            poDS->SetGeoTransform(adfGT);
            poDS->CreateLayer("foo");
            poDS.reset();

            auto info = singleton.Instantiate("info");
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            EXPECT_FALSE(info->ParseCommandLineArguments(
                std::vector<std::string>{osTmpFilename.c_str()}));
            info->GetUsageForCLI(false);

            VSIUnlink(osTmpFilename.c_str());
        }
    }
}

TEST_F(test_gdal_algorithm, raster_edit_failures_dataset_0_0)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    auto raster = singleton.Instantiate("raster");
    ASSERT_NE(raster, nullptr);
    auto edit = raster->InstantiateSubAlgorithm("edit");
    ASSERT_NE(edit, nullptr);

    class MyDataset : public GDALDataset
    {
      public:
        MyDataset()
        {
            nRasterXSize = 0;
            nRasterYSize = 0;
            eAccess = GA_Update;
        }
    };

    auto datasetArg = edit->GetArg("dataset");
    ASSERT_NE(datasetArg, nullptr);
    datasetArg->Get<GDALArgDatasetValue>().Set(std::make_unique<MyDataset>());

    auto extentArg = edit->GetArg("bbox");
    ASSERT_NE(extentArg, nullptr);
    extentArg->Set(std::vector<double>{2, 49, 3, 50});

    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
    CPLErrorReset();
    EXPECT_FALSE(edit->Run());
    EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    EXPECT_STREQ(CPLGetLastErrorMsg(),
                 "edit: Cannot set extent because dataset has 0x0 dimension");
}

TEST_F(test_gdal_algorithm, raster_edit_failures_set_spatial_ref_none)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    auto raster = singleton.Instantiate("raster");
    ASSERT_NE(raster, nullptr);
    auto edit = raster->InstantiateSubAlgorithm("edit");
    ASSERT_NE(edit, nullptr);

    class MyDataset : public GDALDataset
    {
      public:
        MyDataset()
        {
            eAccess = GA_Update;
        }

        CPLErr SetSpatialRef(const OGRSpatialReference *) override
        {
            return CE_Failure;
        }
    };

    auto datasetArg = edit->GetArg("dataset");
    ASSERT_NE(datasetArg, nullptr);
    datasetArg->Get<GDALArgDatasetValue>().Set(std::make_unique<MyDataset>());

    auto crsArg = edit->GetArg("crs");
    ASSERT_NE(crsArg, nullptr);
    crsArg->Set("none");

    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
    CPLErrorReset();
    EXPECT_FALSE(edit->Run());
    EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    EXPECT_STREQ(CPLGetLastErrorMsg(), "edit: SetSpatialRef(none) failed");
}

TEST_F(test_gdal_algorithm, raster_edit_failures_set_spatial_ref_regular)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    auto raster = singleton.Instantiate("raster");
    ASSERT_NE(raster, nullptr);
    auto edit = raster->InstantiateSubAlgorithm("edit");
    ASSERT_NE(edit, nullptr);

    class MyDataset : public GDALDataset
    {
      public:
        MyDataset()
        {
            eAccess = GA_Update;
        }

        CPLErr SetSpatialRef(const OGRSpatialReference *) override
        {
            return CE_Failure;
        }
    };

    auto datasetArg = edit->GetArg("dataset");
    ASSERT_NE(datasetArg, nullptr);
    datasetArg->Get<GDALArgDatasetValue>().Set(std::make_unique<MyDataset>());

    auto crsArg = edit->GetArg("crs");
    ASSERT_NE(crsArg, nullptr);
    crsArg->Set("EPSG:32632");

    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
    CPLErrorReset();
    EXPECT_FALSE(edit->Run());
    EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    EXPECT_STREQ(CPLGetLastErrorMsg(),
                 "edit: SetSpatialRef(EPSG:32632) failed");
}

TEST_F(test_gdal_algorithm, raster_edit_failures_set_geo_transform)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    auto raster = singleton.Instantiate("raster");
    ASSERT_NE(raster, nullptr);
    auto edit = raster->InstantiateSubAlgorithm("edit");
    ASSERT_NE(edit, nullptr);

    class MyDataset : public GDALDataset
    {
      public:
        MyDataset()
        {
            eAccess = GA_Update;
        }

        CPLErr SetGeoTransform(double *) override
        {
            return CE_Failure;
        }
    };

    auto datasetArg = edit->GetArg("dataset");
    ASSERT_NE(datasetArg, nullptr);
    datasetArg->Get<GDALArgDatasetValue>().Set(std::make_unique<MyDataset>());

    auto extentArg = edit->GetArg("bbox");
    ASSERT_NE(extentArg, nullptr);
    extentArg->Set(std::vector<double>{2, 49, 3, 50});

    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
    CPLErrorReset();
    EXPECT_FALSE(edit->Run());
    EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    EXPECT_STREQ(CPLGetLastErrorMsg(), "edit: Setting extent failed");
}

TEST_F(test_gdal_algorithm, raster_edit_failures_set_metadata)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    auto raster = singleton.Instantiate("raster");
    ASSERT_NE(raster, nullptr);
    auto edit = raster->InstantiateSubAlgorithm("edit");
    ASSERT_NE(edit, nullptr);

    class MyDataset : public GDALDataset
    {
      public:
        MyDataset()
        {
            eAccess = GA_Update;
        }

        CPLErr SetMetadataItem(const char *, const char *,
                               const char *) override
        {
            return CE_Failure;
        }
    };

    auto datasetArg = edit->GetArg("dataset");
    ASSERT_NE(datasetArg, nullptr);
    datasetArg->Get<GDALArgDatasetValue>().Set(std::make_unique<MyDataset>());

    auto extentArg = edit->GetArg("metadata");
    ASSERT_NE(extentArg, nullptr);
    extentArg->Set(std::vector<std::string>{"foo=bar"});

    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
    CPLErrorReset();
    EXPECT_FALSE(edit->Run());
    EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    EXPECT_STREQ(CPLGetLastErrorMsg(),
                 "edit: SetMetadataItem('foo', 'bar') failed");
}

TEST_F(test_gdal_algorithm, raster_edit_failures_unset_metadata)
{
    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    auto raster = singleton.Instantiate("raster");
    ASSERT_NE(raster, nullptr);
    auto edit = raster->InstantiateSubAlgorithm("edit");
    ASSERT_NE(edit, nullptr);

    class MyDataset : public GDALDataset
    {
      public:
        MyDataset()
        {
            eAccess = GA_Update;
        }

        CPLErr SetMetadataItem(const char *, const char *,
                               const char *) override
        {
            return CE_Failure;
        }
    };

    auto datasetArg = edit->GetArg("dataset");
    ASSERT_NE(datasetArg, nullptr);
    datasetArg->Get<GDALArgDatasetValue>().Set(std::make_unique<MyDataset>());

    auto extentArg = edit->GetArg("unset-metadata");
    ASSERT_NE(extentArg, nullptr);
    extentArg->Set(std::vector<std::string>{"foo"});

    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
    CPLErrorReset();
    EXPECT_FALSE(edit->Run());
    EXPECT_EQ(CPLGetLastErrorType(), CE_Failure);
    EXPECT_STREQ(CPLGetLastErrorMsg(),
                 "edit: SetMetadataItem('foo', NULL) failed");
}

}  // namespace test_gdal_algorithm
