///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test SWQ (SQL WHERE Query) features.
// Author:   Even Rouault <even.rouault at spatialys.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
/*
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

#include "gdal_unit_test.h"

#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_swq.h"

#include "gtest_include.h"

namespace
{

struct test_ogr_swq : public ::testing::Test
{
};

}  // namespace

TEST_F(test_ogr_swq, basic)
{
    std::vector<swq_expr_node> nodes = {
        swq_expr_node(),
        swq_expr_node(1),
        swq_expr_node(2),
        swq_expr_node(1.5),
        swq_expr_node(2.5),
        swq_expr_node(static_cast<GIntBig>(4000) * 1000 * 1000),
        swq_expr_node(static_cast<GIntBig>(4000) * 1000 * 1000 + 1),
        swq_expr_node(static_cast<const char *>(nullptr)),
        swq_expr_node("a"),
        swq_expr_node("b"),
        swq_expr_node(SWQ_OR),
        swq_expr_node(SWQ_NOT),
        swq_expr_node(static_cast<OGRGeometry *>(nullptr)),
        swq_expr_node(std::make_unique<OGRPoint>(1, 2).get()),
        swq_expr_node(std::make_unique<OGRPoint>(1, 3).get()),
    };
    {
        auto node = swq_expr_node(SWQ_NOT);
        node.PushSubExpression(new swq_expr_node(1));
        nodes.emplace_back(node);
    }
    {
        auto node = swq_expr_node(SWQ_NOT);
        node.PushSubExpression(new swq_expr_node(2));
        nodes.emplace_back(node);
    }
    {
        auto node = swq_expr_node();
        node.eNodeType = SNT_COLUMN;
        node.field_index = 0;
        node.table_index = 0;
        nodes.emplace_back(node);
    }
    {
        auto node = swq_expr_node();
        node.eNodeType = SNT_COLUMN;
        node.field_index = 0;
        node.table_index = 0;
        node.table_name = CPLStrdup("foo");
        nodes.emplace_back(node);
    }
    {
        auto node = swq_expr_node();
        node.eNodeType = SNT_COLUMN;
        node.field_index = 0;
        node.table_index = 0;
        node.table_name = CPLStrdup("bar");
        nodes.emplace_back(node);
    }
    {
        auto node = swq_expr_node();
        node.eNodeType = SNT_COLUMN;
        node.field_index = 1;
        node.table_index = 0;
        nodes.emplace_back(node);
    }
    {
        auto node = swq_expr_node();
        node.eNodeType = SNT_COLUMN;
        node.field_index = 0;
        node.table_index = 1;
        nodes.emplace_back(node);
    }

    for (const auto &node1 : nodes)
    {
        for (const auto &node2 : nodes)
        {
            if (&node1 == &node2)
            {
                EXPECT_TRUE(node1 == node1);
                EXPECT_TRUE(node1 == swq_expr_node(node1));
            }
            else
            {
                EXPECT_FALSE(node1 == node2);
                EXPECT_FALSE(node2 == node1);

                {
                    swq_expr_node copy(node1);
                    copy = node2;
                    EXPECT_TRUE(copy == node2);
                }
                {
                    swq_expr_node copy1(node1);
                    swq_expr_node copy2(node2);
                    copy1 = std::move(copy2);
                    EXPECT_TRUE(copy1 == node2);
                }
            }
        }
    }
}

class PushNotOperationDownToStackFixture
    : public test_ogr_swq,
      public ::testing::WithParamInterface<
          std::tuple<const char *, const char *>>
{
  public:
    static std::vector<std::tuple<const char *, const char *>> GetTupleValues()
    {
        return {
            std::make_tuple("NOT(1 = 2)", "1 <> 2"),
            std::make_tuple("NOT(1 <> 2)", "1 = 2"),
            std::make_tuple("NOT(1 >= 2)", "1 < 2"),
            std::make_tuple("NOT(1 > 2)", "1 <= 2"),
            std::make_tuple("NOT(1 <= 2)", "1 > 2"),
            std::make_tuple("NOT(1 < 2)", "1 >= 2"),
            std::make_tuple("NOT(NOT(1))", "1"),
            std::make_tuple("NOT(1 AND 2)", "(NOT (1)) OR (NOT (2))"),
            std::make_tuple("NOT(1 OR 2)", "(NOT (1)) AND (NOT (2))"),
            std::make_tuple("3 AND NOT(1 OR 2)",
                            "3 AND ((NOT (1)) AND (NOT (2)))"),
            std::make_tuple("NOT(NOT(1 = 2) OR 2)", "(1 = 2) AND (NOT (2))"),
            std::make_tuple("1", "1"),
        };
    }
};

TEST_P(PushNotOperationDownToStackFixture, test)
{
    const char *pszInput = std::get<0>(GetParam());
    const char *pszExpected = std::get<1>(GetParam());

    swq_expr_node *poNode = nullptr;
    swq_expr_compile(pszInput, 0, nullptr, nullptr, true, nullptr, &poNode);
    ASSERT_TRUE(poNode);
    poNode->PushNotOperationDownToStack();
    char *pszStr = poNode->Unparse(nullptr, '"');
    std::string osStr = pszStr ? pszStr : "";
    CPLFree(pszStr);
    EXPECT_STREQ(osStr.c_str(), pszExpected);
    delete poNode;
}

INSTANTIATE_TEST_SUITE_P(
    test_ogr_swq, PushNotOperationDownToStackFixture,
    ::testing::ValuesIn(PushNotOperationDownToStackFixture::GetTupleValues()),
    [](const ::testing::TestParamInfo<
        PushNotOperationDownToStackFixture::ParamType> &l_info)
    {
        CPLString osStr = std::get<0>(l_info.param);
        osStr.replaceAll(' ', '_');
        osStr.replaceAll('(', '_');
        osStr.replaceAll(')', '_');
        osStr.replaceAll("<>", "NE");
        osStr.replaceAll(">=", "GE");
        osStr.replaceAll(">", "GT");
        osStr.replaceAll("<=", "LE");
        osStr.replaceAll("<", "LT");
        osStr.replaceAll('=', "EQ");
        osStr.replaceAll("__", '_');
        if (osStr.back() == '_')
            osStr.pop_back();
        return osStr;
    });

TEST_F(test_ogr_swq, select_unparse)
{
    {
        swq_select select;
        const char *pszSQL = "SELECT a FROM FOO";
        EXPECT_EQ(select.preparse(pszSQL), CE_None);
        char *ret = select.Unparse();
        EXPECT_STREQ(ret, pszSQL);
        CPLFree(ret);
    }
    {
        swq_select select;
        const char *pszSQL =
            "SELECT DISTINCT a, \"a b\" AS renamed, AVG(x.a) AS avg, MIN(a), "
            "MAX(\"a b\"), SUM(a), AVG(a), COUNT(a), COUNT(DISTINCT a) "
            "FROM 'foo'.\"FOO BAR\" AS x "
            "JOIN 'bar'.BAR AS y ON FOO.x = BAR.y "
            "WHERE 1 ORDER BY a, \"a b\" DESC "
            "LIMIT 1 OFFSET 2";
        EXPECT_EQ(select.preparse(pszSQL), CE_None);
        char *ret = select.Unparse();
        EXPECT_STREQ(ret, pszSQL);
        CPLFree(ret);
    }
}
