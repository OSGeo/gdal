/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB multidimensional support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2023, TileDB, Inc
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

#include "tiledbmultidim.h"

/************************************************************************/
/*              TileDBSingleArrayGroup::SanitizeNameForPath()           */
/************************************************************************/

/* static */
std::string TileDBSharedResource::SanitizeNameForPath(const std::string &osName)
{
    CPLString osSanitized(osName);
    // Reserved characters on Windows
    for (char ch : {'<', '>', ':', '"', '/', '\\', '|', '?', '*'})
        osSanitized.replaceAll(ch, '_');
    std::string osRet = std::move(osSanitized);
    return osRet;
}

/************************************************************************/
/*                    TileDBArrayGroup::Create()                        */
/************************************************************************/

std::shared_ptr<GDALGroup> TileDBArrayGroup::Create(
    const std::shared_ptr<TileDBSharedResource> &poSharedResource,
    const std::string &osArrayPath)
{
    auto poTileDBArray = std::make_unique<tiledb::Array>(
        poSharedResource->GetCtx(), osArrayPath, TILEDB_READ);
    auto schema = poTileDBArray->schema();
    const auto nAttributes = schema.attribute_num();
    const std::string osBaseName(CPLGetFilename(osArrayPath.c_str()));
    std::vector<std::shared_ptr<GDALMDArray>> apoArrays;
    if (nAttributes == 1)
    {
        auto poArray = TileDBArray::OpenFromDisk(poSharedResource, nullptr, "/",
                                                 osBaseName, std::string(),
                                                 osArrayPath, nullptr);
        if (!poArray)
            return nullptr;
        apoArrays.emplace_back(poArray);
    }
    else
    {
        for (uint32_t i = 0; i < nAttributes; ++i)
        {
            auto poArray = TileDBArray::OpenFromDisk(
                poSharedResource, nullptr, "/",
                osBaseName + "." + schema.attribute(i).name(),
                schema.attribute(i).name(), osArrayPath, nullptr);
            if (!poArray)
                return nullptr;
            apoArrays.emplace_back(poArray);
        }
    }
    return std::make_shared<TileDBArrayGroup>(apoArrays);
}

/************************************************************************/
/*                TileDBArrayGroup::GetMDArrayNames()                   */
/************************************************************************/

std::vector<std::string>
TileDBArrayGroup::GetMDArrayNames(CSLConstList /*papszOptions*/) const
{
    std::vector<std::string> aosNames;
    for (const auto &poArray : m_apoArrays)
        aosNames.emplace_back(poArray->GetName());
    return aosNames;
}

/************************************************************************/
/*                  TileDBArrayGroup::OpenMDArray()                     */
/************************************************************************/

std::shared_ptr<GDALMDArray>
TileDBArrayGroup::OpenMDArray(const std::string &osName,
                              CSLConstList /*papszOptions*/) const
{
    for (const auto &poArray : m_apoArrays)
    {
        if (poArray->GetName() == osName)
            return poArray;
    }
    return nullptr;
}

/************************************************************************/
/*                       OpenMultiDimensional()                         */
/************************************************************************/

GDALDataset *TileDBDataset::OpenMultiDimensional(GDALOpenInfo *poOpenInfo)
{
    const char *pszConfig =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_CONFIG");

    std::unique_ptr<tiledb::Context> pCtxt;
    if (pszConfig != nullptr)
    {
        tiledb::Config cfg(pszConfig);
        pCtxt.reset(new tiledb::Context(cfg));
    }
    else
    {
        pCtxt.reset(new tiledb::Context());
    }

    const std::string osPath =
        TileDBDataset::VSI_to_tiledb_uri(poOpenInfo->pszFilename);

    const auto eType = tiledb::Object::object(*(pCtxt.get()), osPath).type();

    auto poSharedResource = std::make_shared<TileDBSharedResource>(
        std::move(pCtxt), poOpenInfo->eAccess == GA_Update);

    poSharedResource->SetDumpStats(CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "STATS", "FALSE")));

    const char *pszTimestamp =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_TIMESTAMP");
    if (pszTimestamp)
        poSharedResource->SetTimestamp(
            std::strtoull(pszTimestamp, nullptr, 10));

    std::shared_ptr<GDALGroup> poRG;
    if (eType == tiledb::Object::Type::Array)
    {
        poRG = TileDBArrayGroup::Create(poSharedResource, osPath);
    }
    else
    {
        poRG = TileDBGroup::OpenFromDisk(poSharedResource, std::string(), "/",
                                         osPath);
    }
    if (!poRG)
        return nullptr;

    auto poDS = new TileDBMultiDimDataset(poRG);
    poDS->SetDescription(poOpenInfo->pszFilename);
    return poDS;
}

/************************************************************************/
/*                     CreateMultiDimensional()                         */
/************************************************************************/

GDALDataset *
TileDBDataset::CreateMultiDimensional(const char *pszFilename,
                                      CSLConstList /*papszRootGroupOptions*/,
                                      CSLConstList papszOptions)
{
    const char *pszConfig = CSLFetchNameValue(papszOptions, "TILEDB_CONFIG");

    std::unique_ptr<tiledb::Context> pCtxt;
    if (pszConfig != nullptr)
    {
        tiledb::Config cfg(pszConfig);
        pCtxt.reset(new tiledb::Context(cfg));
    }
    else
    {
        pCtxt.reset(new tiledb::Context());
    }

    const std::string osPath = TileDBDataset::VSI_to_tiledb_uri(pszFilename);

    auto poSharedResource =
        std::make_shared<TileDBSharedResource>(std::move(pCtxt), true);

    poSharedResource->SetDumpStats(
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "STATS", "FALSE")));

    const char *pszTimestamp =
        CSLFetchNameValue(papszOptions, "TILEDB_TIMESTAMP");
    if (pszTimestamp)
        poSharedResource->SetTimestamp(
            std::strtoull(pszTimestamp, nullptr, 10));

    auto poRG =
        TileDBGroup::CreateOnDisk(poSharedResource, std::string(), "/", osPath);
    if (!poRG)
        return nullptr;

    auto poDS = new TileDBMultiDimDataset(poRG);
    poDS->SetDescription(pszFilename);
    return poDS;
}
