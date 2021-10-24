/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  STACIT (Spatio-Temporal Asset Catalog ITems) driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_json.h"
#include "vrtdataset.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <limits>
#include <map>
#include <string>

CPL_CVSID("$Id$")

namespace {

struct AssetItem
{
    std::string osFilename{};
    std::string osDateTime{};
    int nXSize = 0;
    int nYSize = 0;
    double dfXMin = 0;
    double dfYMin = 0;
    double dfXMax = 0;
    double dfYMax = 0;
};

struct AssetSetByProjection
{
    std::string osProjUserString{};
    std::vector<AssetItem> assets{};
};

struct Asset
{
    std::string osName{};
    CPLJSONArray eoBands{};
    std::map<std::string, AssetSetByProjection> assets{};
};

struct Collection
{
    std::string osName{};
    std::map<std::string, Asset> assets{};
};
} // namespace

/************************************************************************/
/*                            STACITDataset                             */
/************************************************************************/

class STACITDataset final: public VRTDataset
{
        bool Open(GDALOpenInfo* poOpenInfo);
        bool SetupDataset(GDALOpenInfo* poOpenInfo,
                          std::map<std::string, Collection>& oMapCollection);
        bool SetSubdatasets(
                    const std::string& osFilename,
                    const std::map<std::string, Collection>& oMapCollection);
public:
        STACITDataset();

        static int Identify(GDALOpenInfo* poOpenInfo);
        static GDALDataset* OpenStatic(GDALOpenInfo* poOpenInfo);
};

/************************************************************************/
/*                          STACITDataset()                             */
/************************************************************************/

STACITDataset::STACITDataset():
    VRTDataset(0,0)
{
    SetWritable(false);
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int STACITDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if( STARTS_WITH(poOpenInfo->pszFilename, "STACIT:") )
    {
        return true;
    }

    if( poOpenInfo->nHeaderBytes == 0 )
    {
        return false;
    }

    for( int i = 0; i < 2; i++ )
    {
        // TryToIngest() may reallocate pabyHeader, so do not move this
        // before the loop.
        const char* pszHeader =
            reinterpret_cast<const char*>(poOpenInfo->pabyHeader);
        if( strstr(pszHeader, "\"stac_version\"") != nullptr &&
            strstr(pszHeader, "\"proj:transform\"") != nullptr )
        {
            return true;
        }

        if( i == 0 )
        {
            // Should be enough for a STACIT .json file
            poOpenInfo->TryToIngest(32768);
        }
    }

    return false;
}

/************************************************************************/
/*                        SanitizeCRSValue()                            */
/************************************************************************/

static std::string SanitizeCRSValue(const std::string& v)
{
    std::string ret;
    bool lastWasAlphaNum = true;
    for( char ch: v )
    {
        if( !isalnum(static_cast<int>(ch)) )
        {
            if( lastWasAlphaNum )
                ret += '_';
            lastWasAlphaNum = false;
        }
        else
        {
            ret += ch;
            lastWasAlphaNum = true;
        }
    }
    if( !ret.empty() && ret.back() == '_' )
        ret.resize(ret.size() - 1);
    return ret;
}

/************************************************************************/
/*                            ParseAsset()                              */
/************************************************************************/

static void ParseAsset(const CPLJSONObject& jAsset,
                       const CPLJSONObject& oProperties,
                       const std::string& osCollection,
                       const std::string& osFilteredCRS,
                       std::map<std::string, Collection>& oMapCollection)
{
    // Skip assets that are obviously not images
    const auto osType = jAsset["type"].ToString();
    if( osType == "application/json" ||
        osType == "application/xml" ||
        osType == "text/plain" )
    {
        return;
    }

    // Skip assets whose role is obviously non georeferenced
    const auto oRoles = jAsset.GetArray("roles");
    if( oRoles.IsValid() )
    {
        for( const auto& oRole: oRoles )
        {
            const auto osRole = oRole.ToString();
            if( osRole == "thumbnail" || osRole == "info" ||
                osRole == "metadata" )
            {
                return;
            }
        }
    }

    const auto osAssetName = jAsset.GetName();

    const auto osHref = jAsset["href"].ToString();
    if( osHref.empty() )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Missing href on asset %s", osAssetName.c_str());
        return;
    }

    const auto GetAssetOrFeatureProperty = [&oProperties, &jAsset](const char* pszName)
    {
        auto obj = jAsset[pszName];
        if( obj.IsValid() )
            return obj;
        return oProperties[pszName];
    };

    auto oProjEPSG = GetAssetOrFeatureProperty("proj:epsg");
    if( !oProjEPSG.IsValid() )
    {
        CPLDebug("STACIT",
                 "Skipping asset %s that lacks the 'proj:epsg' member",
                 osAssetName.c_str());
        return;
    }
    std::string osProjUserString;
    if( oProjEPSG.GetType() != CPLJSONObject::Type::Null )
    {
        osProjUserString = "EPSG:" + oProjEPSG.ToString();
    }
    else
    {
        auto oProjWKT2 = GetAssetOrFeatureProperty("proj:wkt2");
        if( oProjWKT2.IsValid() &&
            oProjWKT2.GetType() == CPLJSONObject::Type::String )
        {
            osProjUserString = oProjWKT2.ToString();
        }
        else
        {
            auto oProjPROJJSON = GetAssetOrFeatureProperty("proj:projjson");
            if( oProjPROJJSON.IsValid() &&
                oProjPROJJSON.GetType() == CPLJSONObject::Type::Object )
            {
                osProjUserString = oProjPROJJSON.ToString();
            }
            else
            {
                CPLDebug("STACIT",
                         "Skipping asset %s that lacks a valid CRS member",
                         osAssetName.c_str());
                return;
            }
        }
    }

    if( !osFilteredCRS.empty() &&
        osFilteredCRS != SanitizeCRSValue(osProjUserString) )
    {
        return;
    }

    AssetItem item;
    item.osFilename = osHref;
    item.osDateTime = oProperties["datetime"].ToString();

    // Figure out item bounds and width/height
    auto oProjBBOX = GetAssetOrFeatureProperty("proj:bbox").ToArray();
    auto oProjShape = GetAssetOrFeatureProperty("proj:shape").ToArray();
    auto oProjTransform = GetAssetOrFeatureProperty("proj:transform").ToArray();
    const bool bIsBBOXValid = oProjBBOX.IsValid() && oProjBBOX.Size() == 4;
    const bool bIsShapeValid = oProjShape.IsValid() && oProjShape.Size() == 2;
    const bool bIsTransformValid = oProjTransform.IsValid() && (oProjTransform.Size() == 6 || oProjTransform.Size() == 9);
    std::vector<double> bbox;
    if( bIsBBOXValid )
    {
        for( const auto& oItem: oProjBBOX )
            bbox.push_back(oItem.ToDouble());
        CPLAssert(bbox.size() == 4);
    }
    std::vector<int> shape;
    if( bIsShapeValid )
    {
        for( const auto& oItem: oProjShape )
            shape.push_back(oItem.ToInteger());
        CPLAssert(shape.size() == 2);
    }
    std::vector<double> transform;
    if( bIsTransformValid )
    {
        for( const auto& oItem: oProjTransform )
            transform.push_back(oItem.ToDouble());
        CPLAssert(transform.size() == 6 || transform.size() == 9);
        if( transform[0] <= 0 ||
            transform[1] != 0 ||
            transform[3] != 0 ||
            transform[4] >= 0 ||
            (transform.size() == 9 &&
             (transform[6] != 0 ||
             transform[7] != 0 ||
             transform[8] != 1)) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Skipping asset %s because its proj:transform is "
                     "not of the form [xres,0,xoffset,0,yres<0,yoffset[,0,0,1]]",
                     osAssetName.c_str());
            return;
        }
    }

    if( bIsBBOXValid && bIsShapeValid )
    {
        item.nXSize = shape[1];
        item.nYSize = shape[0];
        item.dfXMin = bbox[0];
        item.dfYMin = bbox[1];
        item.dfXMax = bbox[2];
        item.dfYMax = bbox[3];
    }
    else if( bIsBBOXValid && bIsTransformValid )
    {
        item.dfXMin = bbox[0];
        item.dfYMin = bbox[1];
        item.dfXMax = bbox[2];
        item.dfYMax = bbox[3];
        if( item.dfXMin != transform[2] ||
            item.dfYMax != transform[5] )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Skipping asset %s because the origin of "
                     "proj:transform and proj:bbox are not consistent",
                     osAssetName.c_str());
            return;
        }
        double dfXSize = (item.dfXMax - item.dfXMin) / transform[0];
        double dfYSize = (item.dfYMax - item.dfYMin) / -transform[4];
        if( !(dfXSize < INT_MAX && dfYSize < INT_MAX) )
            return;
        item.nXSize = static_cast<int>(dfXSize);
        item.nYSize = static_cast<int>(dfYSize);
    }
    else if( bIsShapeValid && bIsTransformValid )
    {
        item.nXSize = shape[1];
        item.nYSize = shape[0];
        item.dfXMin = transform[2];
        item.dfYMax = transform[5];
        item.dfXMax = item.dfXMin + item.nXSize * transform[0];
        item.dfYMin = item.dfYMax + item.nYSize * transform[4];
    }
    else
    {
        CPLDebug("STACIT",
                 "Skipping asset %s that lacks at least 2 members among "
                 "proj:bbox, proj:shape and proj:transform",
                 osAssetName.c_str());
        return;
    }

    if( item.nXSize <= 0 || item.nYSize <= 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Skipping asset %s because the size is invalid",
                 osAssetName.c_str());
        return;
    }

    // Create/fetch collection
    if( oMapCollection.find(osCollection) == oMapCollection.end() )
    {
        Collection collection;
        collection.osName = osCollection;
        oMapCollection[osCollection] = collection;
    }
    auto& collection = oMapCollection[osCollection];

    // Create/fetch asset in collection
    if( collection.assets.find(osAssetName) == collection.assets.end() )
    {
        Asset asset;
        asset.osName = osAssetName;
        asset.eoBands = jAsset.GetArray("eo:bands");

        collection.assets[osAssetName] = asset;
    }
    auto& asset = collection.assets[osAssetName];

    // Create/fetch projection in asset
    if( asset.assets.find(osProjUserString) == asset.assets.end() )
    {
        AssetSetByProjection assetByProj;
        assetByProj.osProjUserString = osProjUserString;
        asset.assets[osProjUserString] = assetByProj;
    }
    auto& assets = asset.assets[osProjUserString];

    // Add item
    assets.assets.emplace_back(item);
}

/************************************************************************/
/*                           SetupDataset()                             */
/************************************************************************/

bool STACITDataset::SetupDataset(GDALOpenInfo* poOpenInfo,
                                 std::map<std::string, Collection>& oMapCollection)
{
    auto& collection = oMapCollection.begin()->second;
    auto& asset = collection.assets.begin()->second;
    auto& assetByProj = asset.assets.begin()->second;
    auto& items = assetByProj.assets;

    // Compute global bounds and resolution
    double dfXMin = std::numeric_limits<double>::max();
    double dfYMin = std::numeric_limits<double>::max();
    double dfXMax = -std::numeric_limits<double>::max();
    double dfYMax = -std::numeric_limits<double>::max();
    double dfXRes = 0;
    double dfYRes = 0;
    const char* pszResolution = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "RESOLUTION", "AVERAGE");
    for( const auto& assetItem: items )
    {
        dfXMin = std::min(dfXMin, assetItem.dfXMin);
        dfYMin = std::min(dfYMin, assetItem.dfYMin);
        dfXMax = std::max(dfXMax, assetItem.dfXMax);
        dfYMax = std::max(dfYMax, assetItem.dfYMax);
        const double dfThisXRes =
            (assetItem.dfXMax - assetItem.dfXMin) / assetItem.nXSize;
        const double dfThisYRes =
            (assetItem.dfYMax - assetItem.dfYMin) / assetItem.nYSize;
#ifdef DEBUG_VERBOSE
        CPLDebug("STACIT", "%s -> resx=%f resy=%f",
                 assetItem.osFilename.c_str(), dfThisXRes, dfThisYRes);
#endif
        if( dfXRes != 0 && EQUAL(pszResolution, "HIGHEST") )
        {
            dfXRes = std::min(dfXRes, dfThisXRes);
            dfYRes = std::min(dfYRes, dfThisYRes);
        }
        else if( dfXRes != 0 && EQUAL(pszResolution, "LOWEST") )
        {
            dfXRes = std::max(dfXRes, dfThisXRes);
            dfYRes = std::max(dfYRes, dfThisYRes);
        }
        else
        {
            dfXRes += dfThisXRes;
            dfYRes += dfThisYRes;
        }
    }
    if( EQUAL(pszResolution, "AVERAGE") )
    {
        dfXRes /= static_cast<int>(items.size());
        dfYRes /= static_cast<int>(items.size());
    }

    // Set raster size
    double dfXSize = std::round((dfXMax - dfXMin) / dfXRes);
    double dfYSize = std::round((dfYMax - dfYMin) / dfYRes);
    if( dfXSize <= 0 ||
        dfYSize <= 0 ||
        dfXSize > INT_MAX ||
        dfYSize > INT_MAX )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid computed dataset dimensions");
        return false;
    }
    nRasterXSize = static_cast<int>(dfXSize);
    nRasterYSize = static_cast<int>(dfYSize);

    // Set geotransform
    double adfGeoTransform[6];
    adfGeoTransform[0] = dfXMin;
    adfGeoTransform[1] = dfXRes;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = dfYMax;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = -dfYRes;
    SetGeoTransform(adfGeoTransform);

    // Set SRS
    OGRSpatialReference oSRS;
    if( oSRS.SetFromUserInput(assetByProj.osProjUserString.c_str(), OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) == OGRERR_NONE )
    {
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        SetSpatialRef(&oSRS);
    }

    // Open of the items to find the number of bands, their data type
    // and nodata.
    CPLString osFirstItemName(items.front().osFilename);
    if( osFirstItemName.find("http") == 0 )
        osFirstItemName = "/vsicurl/" + osFirstItemName;
    auto poItemDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(osFirstItemName));
    if( !poItemDS )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot open %s to retrieve band characteristics",
                 osFirstItemName.c_str());
        return false;
    }

    // Sort by ascending datetime
    std::sort(items.begin(), items.end(),
              [](const AssetItem& a, const AssetItem& b)
        {
            if( !a.osDateTime.empty() && !b.osDateTime.empty() )
                return a.osDateTime < b.osDateTime;
            return &a < &b;
        }
    );

    // Create VRT bands and add sources
    for(int i = 0; i < poItemDS->GetRasterCount(); i++ )
    {
        auto poItemBand = poItemDS->GetRasterBand(i+1);
        AddBand( poItemBand->GetRasterDataType(), nullptr );
        auto poVRTBand = cpl::down_cast<VRTSourcedRasterBand *>(GetRasterBand(i+1));
        int bHasNoData = FALSE;
        const double dfNoData = poItemBand->GetNoDataValue(&bHasNoData);
        if( bHasNoData )
            poVRTBand->SetNoDataValue(dfNoData);

        const auto eInterp = poItemBand->GetColorInterpretation();
        if( eInterp != GCI_Undefined )
            poVRTBand->SetColorInterpretation(eInterp);

        // Set band properties
        if( asset.eoBands.IsValid() &&
            asset.eoBands.Size() == poItemDS->GetRasterCount() )
        {
            const auto& eoBand = asset.eoBands[i];
            const auto osBandName = eoBand["name"].ToString();
            if( !osBandName.empty() )
                poVRTBand->SetDescription(osBandName.c_str());

            if( eInterp != GCI_Undefined )
            {
                const auto osCommonName = eoBand["common_name"].ToString();
                if( osCommonName == "red" )
                    poVRTBand->SetColorInterpretation(GCI_RedBand);
                else if( osCommonName == "green" )
                    poVRTBand->SetColorInterpretation(GCI_GreenBand);
                else if( osCommonName == "blue" )
                    poVRTBand->SetColorInterpretation(GCI_BlueBand);
                else if( osCommonName == "alpha" )
                    poVRTBand->SetColorInterpretation(GCI_AlphaBand);
            }

            for( const auto& eoBandChild: eoBand.GetChildren() )
            {
                const auto osChildName = eoBandChild.GetName();
                if( osChildName != "name" && osChildName != "common_name" )
                {
                    poVRTBand->SetMetadataItem(osChildName.c_str(),
                                               eoBandChild.ToString().c_str());
                }
            }
        }

        // Add items as VRT sources
        for( const auto& assetItem: items )
        {
            CPLString osItemName(assetItem.osFilename);
            if( osItemName.find("http") == 0 )
                osItemName = "/vsicurl/" + osItemName;
            const double dfDstXOff = (assetItem.dfXMin - dfXMin) / dfXRes;
            const double dfDstXSize = (assetItem.dfXMax - assetItem.dfXMin ) / dfXRes;
            const double dfDstYOff = (dfYMax - assetItem.dfYMax) / dfYRes;
            const double dfDstYSize = (assetItem.dfYMax - assetItem.dfYMin ) / dfYRes;
            if( !bHasNoData )
            {
                poVRTBand->AddSimpleSource( osItemName.c_str(), i+1,
                                            0,
                                            0,
                                            assetItem.nXSize,
                                            assetItem.nYSize,
                                            dfDstXOff,
                                            dfDstYOff,
                                            dfDstXSize,
                                            dfDstYSize );
            }
            else
            {
                poVRTBand->AddComplexSource( osItemName.c_str(), i+1,
                                            0,
                                            0,
                                            assetItem.nXSize,
                                            assetItem.nYSize,
                                            dfDstXOff,
                                            dfDstYOff,
                                            dfDstXSize,
                                            dfDstYSize,
                                            0.0, // offset
                                            1.0, // scale
                                            dfNoData );
            }
        }

        const char* apszOptions[] = { "EMIT_ERROR_IF_GEOS_NOT_AVAILABLE=NO",
                                      nullptr };
        poVRTBand->RemoveCoveredSources(apszOptions);
    }
    return true;
}

/************************************************************************/
/*                         SetSubdatasets()                             */
/************************************************************************/

bool STACITDataset::SetSubdatasets(
                        const std::string& osFilename,
                        const std::map<std::string, Collection>& oMapCollection)
{
    CPLStringList aosSubdatasets;
    int nCount = 1;
    for( const auto& collectionKV: oMapCollection )
    {
        for( const auto& assetKV: collectionKV.second.assets )
        {
            std::string osCollectionAssetArg;
            if( oMapCollection.size() > 1 )
                osCollectionAssetArg += "collection=" + collectionKV.first + ",";
            osCollectionAssetArg += "asset=" + assetKV.first;

            std::string osCollectionAssetText;
            if( oMapCollection.size() > 1 )
                osCollectionAssetText += "Collection " + collectionKV.first + ", ";
            osCollectionAssetText += "Asset " + assetKV.first;

            if( assetKV.second.assets.size() == 1 )
            {
                aosSubdatasets.AddString(
                    CPLSPrintf("SUBDATASET_%d_NAME=STACIT:\"%s\":%s",
                               nCount,
                               osFilename.c_str(),
                               osCollectionAssetArg.c_str()));
                aosSubdatasets.AddString(
                    CPLSPrintf("SUBDATASET_%d_DESC=%s of %s",
                               nCount,
                               osCollectionAssetText.c_str(),
                               osFilename.c_str()));
                nCount ++;
            }
            else
            {
                for( const auto& assetsByProjKV: assetKV.second.assets )
                {
                    aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_NAME=STACIT:\"%s\":%s,crs=%s",
                                   nCount,
                                   osFilename.c_str(),
                                   osCollectionAssetArg.c_str(),
                                   SanitizeCRSValue(assetsByProjKV.first).c_str()));
                    aosSubdatasets.AddString(
                        CPLSPrintf("SUBDATASET_%d_DESC=%s of %s in CRS %s",
                                   nCount,
                                   osCollectionAssetText.c_str(),
                                   osFilename.c_str(),
                                   assetsByProjKV.first.c_str()));
                    nCount ++;
                }
            }
        }
    }
    GDALDataset::SetMetadata(aosSubdatasets.List(), "SUBDATASETS");
    return true;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

bool STACITDataset::Open(GDALOpenInfo* poOpenInfo)
{
    CPLString osFilename(poOpenInfo->pszFilename);
    std::string osFilteredCollection =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "COLLECTION", "");
    std::string osFilteredAsset =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "ASSET", "");
    std::string osFilteredCRS =
        SanitizeCRSValue(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "CRS", ""));
    if( STARTS_WITH(poOpenInfo->pszFilename, "STACIT:") )
    {
        const CPLStringList aosTokens(
            CSLTokenizeString2(poOpenInfo->pszFilename, ":", CSLT_HONOURSTRINGS ));
        if( aosTokens.size() != 2 && aosTokens.size() != 3 )
            return false;
        osFilename = aosTokens[1];
        if( aosTokens.size() >= 3 )
        {
            const CPLStringList aosFilters(
                CSLTokenizeString2(aosTokens[2], ",", 0));
            osFilteredCollection = aosFilters.FetchNameValueDef("collection",
                                                osFilteredCollection.c_str());
            osFilteredAsset = aosFilters.FetchNameValueDef("asset",
                                                osFilteredAsset.c_str());
            osFilteredCRS = aosFilters.FetchNameValueDef("crs",
                                                osFilteredCRS.c_str());
        }
    }

    std::map<std::string, Collection> oMapCollection;
    GIntBig nItemIter = 0;
    GIntBig nMaxItems = CPLAtoGIntBig(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "MAX_ITEMS", "1000"));

    if( CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAX_ITEMS") == nullptr )
    {
        // If the URL includes a limit parameter, and it's larger than our
        // default MAX_ITEMS value, then increase the later to the former.
        const auto nPos = osFilename.ifind("&limit=");
        if( nPos != std::string::npos )
        {
            const auto nLimit = CPLAtoGIntBig(
                osFilename.substr(nPos + strlen("&limit=")).c_str());
            nMaxItems = std::max(nMaxItems, nLimit);
        }
    }

    auto osCurFilename = osFilename;
    do
    {
        CPLJSONDocument oDoc;
        if( STARTS_WITH(osCurFilename, "http://") ||
            STARTS_WITH(osCurFilename, "https://") )
        {
            if( !oDoc.LoadUrl(osCurFilename, nullptr) )
                return false;
        }
        else
        {
            if( !oDoc.Load(osCurFilename) )
                return false;
        }
        const auto oRoot = oDoc.GetRoot();
        const auto oFeatures = oRoot.GetArray("features");
        if( !oFeatures.IsValid() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing features");
            return false;
        }
        for( const auto& oFeature: oFeatures )
        {
            nItemIter ++;
            if( nItemIter > nMaxItems )
            {
                break;
            }

            auto oStacExtensions = oFeature.GetArray("stac_extensions");
            if( !oStacExtensions.IsValid() )
            {
                CPLDebug("STACIT", "Skipping Feature that lacks stac_extensions");
                continue;
            }
            bool bProjExtensionFound = false;
            for( const auto& oStacExtension: oStacExtensions )
            {
                if( oStacExtension.ToString() == "proj" ||
                    oStacExtension.ToString().find(
                        "https://stac-extensions.github.io/projection/") == 0 )
                {
                    bProjExtensionFound = true;
                    break;
                }
            }
            if( !bProjExtensionFound )
            {
                CPLDebug("STACIT",
                         "Skipping Feature that lacks the 'proj' STAC extension");
                continue;
            }

            auto jAssets = oFeature["assets"];
            if( !jAssets.IsValid() ||
                jAssets.GetType() != CPLJSONObject::Type::Object )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Missing assets on a Feature");
                continue;
            }

            auto oProperties = oFeature["properties"];
            if( !oProperties.IsValid() ||
                oProperties.GetType() != CPLJSONObject::Type::Object )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Missing properties on a Feature");
                continue;
            }

            const auto osCollection = oFeature["collection"].ToString();
            if( !osFilteredCollection.empty() && osFilteredCollection != osCollection )
                continue;

            for( const auto& jAsset: jAssets.GetChildren() )
            {
                const auto osAssetName = jAsset.GetName();
                if( !osFilteredAsset.empty() && osFilteredAsset != osAssetName )
                    continue;

                ParseAsset(jAsset, oProperties, osCollection,
                           osFilteredCRS,
                           oMapCollection);
            }
        }
        if( nItemIter >= nMaxItems )
        {
            if( CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                  "MAX_ITEMS") == nullptr )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Maximum number of items (" CPL_FRMT_GIB
                         ") allowed to be retrieved has been hit",
                         nMaxItems);
            }
            else
            {
                CPLDebug("STACIT",
                         "Maximum number of items (" CPL_FRMT_GIB
                         ") allowed to be retrieved has been hit",
                         nMaxItems);
            }
            break;
        }

        // Follow next link
        const auto oLinks = oRoot.GetArray("links");
        if( !oLinks.IsValid() )
            break;
        std::string osNewFilename;
        for( const auto& oLink: oLinks )
        {
            if( oLink["rel"].ToString() == "next" )
            {
                osNewFilename = oLink["href"].ToString();
                break;
            }
        }
        if( !osNewFilename.empty() && osNewFilename != osCurFilename )
            osCurFilename = osNewFilename;
        else
            osCurFilename.clear();
    }
    while( !osCurFilename.empty() );

    if( oMapCollection.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No compatible asset found");
        return false;
    }

    if( oMapCollection.size() > 1 ||
        oMapCollection.begin()->second.assets.size() > 1 ||
        oMapCollection.begin()->second.assets.begin()->second.assets.size() > 1 )
    {
        // If there's more than one asset type or more than one SRS, expose
        // subdatasets.
        return SetSubdatasets(osFilename, oMapCollection);
    }
    else
    {
        return SetupDataset(poOpenInfo, oMapCollection);
    }
}

/************************************************************************/
/*                            OpenStatic()                              */
/************************************************************************/

GDALDataset* STACITDataset::OpenStatic(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) )
        return nullptr;
    auto poDS = cpl::make_unique<STACITDataset>();
    if( !poDS->Open(poOpenInfo) )
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                       GDALRegister_STACIT()                          */
/************************************************************************/

void GDALRegister_STACIT()

{
    if( GDALGetDriverByName( "STACIT" ) != nullptr )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "STACIT" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Spatio-Temporal Asset Catalog Items" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/stacit.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='MAX_ITEMS' type='int' default='1000' "
                        "description='Maximum number of items fetched. 0=unlimited'/>"
"   <Option name='COLLECTION' type='string' "
                        "description='Name of collection to filter items'/>"
"   <Option name='ASSET' type='string' "
                        "description='Name of asset to filter items'/>"
"   <Option name='CRS' type='string' "
                        "description='Name of CRS to filter items'/>"
"   <Option name='RESOLUTION' type='string-select' default='AVERAGE' "
            "description='Strategy to use to determine dataset resolution'>"
"       <Value>AVERAGE</Value>"
"       <Value>HIGHEST</Value>"
"       <Value>LOWEST</Value>"
"   </Option>"
"</OpenOptionList>" );

    poDriver->pfnOpen = STACITDataset::OpenStatic;
    poDriver->pfnIdentify = STACITDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
