/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR C API "Spy"
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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

#include "cpl_port.h"
#include "cpl_multiproc.h"
#include "ograpispy.h"

#include <cstdio>
#include <map>
#include <set>

#include "cpl_string.h"
#include "gdal.h"
#include "ogr_geometry.h"
#include "ogr_feature.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

#ifdef OGRAPISPY_ENABLED

int bOGRAPISpyEnabled = FALSE;
static CPLMutex* hMutex = nullptr;
static CPLString osSnapshotPath;
static CPLString osSpyFile;
static FILE* fpSpyFile = nullptr;
extern "C" int CPL_DLL GDALIsInGlobalDestructor(void);

// Keep in sync with cpl_conv.cpp
void OGRAPISPYCPLSetConfigOption(const char*, const char*);
void OGRAPISPYCPLSetThreadLocalConfigOption(const char*, const char*);

namespace
{

class LayerDescription
{
  public:
    int iLayer = -1;

    LayerDescription() = default;
    explicit LayerDescription( int iLayerIn ): iLayer(iLayerIn) {}
};

class DatasetDescription
{
  public:
    int iDS = -1;
    std::map<OGRLayerH, LayerDescription> oMapLayer{};

    DatasetDescription() = default;
    explicit DatasetDescription( int iDSIn ) : iDS(iDSIn) {}
    DatasetDescription& operator=(DatasetDescription&&) = default;
    ~DatasetDescription();
};

class FeatureDefnDescription
{
  public:
    OGRFeatureDefnH hFDefn = nullptr;
    int iUniqueNumber = -1;
    std::map<OGRFieldDefnH, int> oMapFieldDefn{};
    std::map<OGRGeomFieldDefnH, int> oMapGeomFieldDefn{};

    FeatureDefnDescription() = default;
    FeatureDefnDescription( OGRFeatureDefnH hFDefnIn, int iUniqueNumberIn ):
        hFDefn(hFDefnIn), iUniqueNumber(iUniqueNumberIn) {}

    FeatureDefnDescription(const FeatureDefnDescription&) = default;
    FeatureDefnDescription& operator=(const FeatureDefnDescription&) = default;

    void Free();
};

}  // namespace

static std::map<GDALDatasetH, DatasetDescription> oMapDS;
static std::set<int> oSetDSIndex;
static std::map<OGRLayerH, CPLString> oGlobalMapLayer;
static OGRLayerH hLayerGetNextFeature = nullptr;
static OGRLayerH hLayerGetLayerDefn = nullptr;
static bool bDeferGetFieldCount = false;
static int nGetNextFeatureCalls = 0;
static std::set<CPLString> aoSetCreatedDS;
static std::map<OGRFeatureDefnH, FeatureDefnDescription> oMapFDefn;
static std::map<OGRGeomFieldDefnH, CPLString> oGlobalMapGeomFieldDefn;
static std::map<OGRFieldDefnH, CPLString> oGlobalMapFieldDefn;

void FeatureDefnDescription::Free()
{
    {
        std::map<OGRGeomFieldDefnH, int>::iterator oIter =
            oMapGeomFieldDefn.begin();
        for( ; oIter != oMapGeomFieldDefn.end(); ++oIter )
            oGlobalMapGeomFieldDefn.erase(oIter->first);
    }
    {
        std::map<OGRFieldDefnH, int>::iterator oIter =
            oMapFieldDefn.begin();
        for( ; oIter != oMapFieldDefn.end(); ++oIter )
            oGlobalMapFieldDefn.erase(oIter->first);
    }
}

DatasetDescription::~DatasetDescription()
{
    if( !GDALIsInGlobalDestructor() )
    {
        std::map<OGRLayerH, LayerDescription>::iterator oIter =
            oMapLayer.begin();
        for( ; oIter != oMapLayer.end(); ++oIter )
            oGlobalMapLayer.erase(oIter->first);
    }
}

void OGRAPISpyDestroyMutex()
{
    if( hMutex )
    {
        CPLDestroyMutex(hMutex);
        hMutex = nullptr;

        aoSetCreatedDS.clear();
        oMapFDefn.clear();
        oGlobalMapGeomFieldDefn.clear();
        oGlobalMapFieldDefn.clear();
    }
}

static void OGRAPISpyFileReopen()
{
    if( fpSpyFile == nullptr )
    {
        fpSpyFile = fopen(osSpyFile, "ab");
        if( fpSpyFile == nullptr )
            fpSpyFile = stderr;
    }
}

static void OGRAPISpyFileClose()
{
    if( fpSpyFile != stdout && fpSpyFile != stderr )
    {
        fclose(fpSpyFile);
        fpSpyFile = nullptr;
    }
}

static bool OGRAPISpyEnabled()
{
    if( bOGRAPISpyEnabled < 0 )
        return false;

    const char* pszSpyFile = CPLGetConfigOption("OGR_API_SPY_FILE", nullptr);
    bOGRAPISpyEnabled = pszSpyFile != nullptr;
    if( !bOGRAPISpyEnabled )
    {
        osSpyFile.resize(0);
        aoSetCreatedDS.clear();
        return false;
    }
    if( !osSpyFile.empty() )
        return true;

    CPLMutexHolderD(&hMutex);
    if( !osSpyFile.empty() )
        return true;

    osSpyFile = pszSpyFile;

    const char* pszSnapshotPath =
        CPLGetConfigOption("OGR_API_SPY_SNAPSHOT_PATH", ".");
    if( EQUAL(pszSnapshotPath, "NO") )
        osSnapshotPath = "";
    else
        osSnapshotPath = pszSnapshotPath;

    if( EQUAL(pszSpyFile, "stdout") )
        fpSpyFile = stdout;
    else if( EQUAL(pszSpyFile, "stderr") )
        fpSpyFile = stderr;
    else
        fpSpyFile = fopen(pszSpyFile, "wb");
    if( fpSpyFile == nullptr )
        fpSpyFile = stderr;

    fprintf(fpSpyFile,
            "# This file is generated by the OGR_API_SPY mechanism.\n");
    fprintf(fpSpyFile, "import os\n");
    fprintf(fpSpyFile, "import shutil\n");
    fprintf(fpSpyFile, "from osgeo import gdal\n");
    fprintf(fpSpyFile, "from osgeo import ogr\n");
    fprintf(fpSpyFile, "from osgeo import osr\n");
    // To make pyflakes happy in case it is unused later.
    fprintf(fpSpyFile, "os.access\n");
    fprintf(fpSpyFile, "shutil.copy\n");  // Same here.
    fprintf(fpSpyFile, "\n");

    return true;
}

static CPLString OGRAPISpyGetOptions( char** papszOptions )
{
    if( papszOptions == nullptr )
    {
        return "[]";
    }

    CPLString options = "[";
    for( char** papszIter = papszOptions; *papszIter != nullptr; papszIter++ )
    {
        if( papszIter != papszOptions )
            options += ", ";
        options += "'";
        options += *papszIter;
        options += "'";
    }
    options += "]";

    return options;
}

static CPLString OGRAPISpyGetString( const char* pszStr )
{
    if( pszStr == nullptr )
        return "None";
    CPLString osRet = "'";
    while( *pszStr )
    {
        if( *pszStr == '\'' )
            osRet += "\\'";
        else if( *pszStr == '\\' )
            osRet += "\\\\";
        else
            osRet += *pszStr;
        pszStr++;
    }
    osRet += "'";
    return osRet;
}

static CPLString OGRAPISpyGetDSVar( GDALDatasetH hDS )
{
    if( hDS && oMapDS.find(hDS) == oMapDS.end() )
    {
        int i = 1;
        while( oSetDSIndex.find(i) != oSetDSIndex.end() )
            i ++;
        oMapDS[hDS] = DatasetDescription(i);
        oSetDSIndex.insert(i);
    }
    return CPLSPrintf("ds%d", hDS ? oMapDS[hDS].iDS : 0);
}

static CPLString OGRAPISpyGetLayerVar( OGRLayerH hLayer )
{
    return oGlobalMapLayer[hLayer];
}

static CPLString OGRAPISpyGetAndRegisterLayerVar( GDALDatasetH hDS,
                                                  OGRLayerH hLayer )
{
    DatasetDescription& dd = oMapDS[hDS];
    if( hLayer && dd.oMapLayer.find(hLayer) == dd.oMapLayer.end() )
    {
        const int i = static_cast<int>(dd.oMapLayer.size()) + 1;
        dd.oMapLayer[hLayer] = LayerDescription(i);
        oGlobalMapLayer[hLayer] =
            OGRAPISpyGetDSVar(hDS) + "_" + CPLSPrintf("lyr%d", i);
    }

    return OGRAPISpyGetDSVar(hDS) + "_" +
           CPLSPrintf("lyr%d", hLayer ? dd.oMapLayer[hLayer].iLayer : 0);
}

static CPLString OGRAPISpyGetSRS( OGRSpatialReferenceH hSpatialRef )
{
    if( hSpatialRef == nullptr )
        return "None";

    char* pszWKT = nullptr;
    OGRSpatialReference::FromHandle(hSpatialRef)->exportToWkt(&pszWKT);
    const char* pszRet =
        CPLSPrintf(R"(osr.SpatialReference("""%s"""))", pszWKT);
    CPLFree(pszWKT);
    return pszRet;
}

static CPLString OGRAPISpyGetGeom( OGRGeometryH hGeom )
{
    if( hGeom == nullptr )
        return "None";

    char* pszWKT = nullptr;
    OGRGeometry::FromHandle(hGeom)->exportToWkt(&pszWKT);
    const char* pszRet = CPLSPrintf("ogr.CreateGeometryFromWkt('%s')", pszWKT);
    CPLFree(pszWKT);
    return pszRet;
}

#define casePrefixOgrDot(x)  case x: return "ogr." #x;

static CPLString OGRAPISpyGetGeomType( OGRwkbGeometryType eType )
{
    switch( eType )
    {
        casePrefixOgrDot(wkbUnknown)
        casePrefixOgrDot(wkbPoint)
        casePrefixOgrDot(wkbLineString)
        casePrefixOgrDot(wkbPolygon)
        casePrefixOgrDot(wkbMultiPoint)
        casePrefixOgrDot(wkbMultiLineString)
        casePrefixOgrDot(wkbMultiPolygon)
        casePrefixOgrDot(wkbGeometryCollection)
        casePrefixOgrDot(wkbCircularString)
        casePrefixOgrDot(wkbCompoundCurve)
        casePrefixOgrDot(wkbCurvePolygon)
        casePrefixOgrDot(wkbMultiCurve)
        casePrefixOgrDot(wkbMultiSurface)
        casePrefixOgrDot(wkbCurve)
        casePrefixOgrDot(wkbSurface)
        casePrefixOgrDot(wkbNone)
        casePrefixOgrDot(wkbLinearRing)
        casePrefixOgrDot(wkbCircularStringZ)
        casePrefixOgrDot(wkbCompoundCurveZ)
        casePrefixOgrDot(wkbCurvePolygonZ)
        casePrefixOgrDot(wkbMultiCurveZ)
        casePrefixOgrDot(wkbMultiSurfaceZ)
        casePrefixOgrDot(wkbCurveZ)
        casePrefixOgrDot(wkbSurfaceZ)
        casePrefixOgrDot(wkbPoint25D)
        casePrefixOgrDot(wkbLineString25D)
        casePrefixOgrDot(wkbPolygon25D)
        casePrefixOgrDot(wkbMultiPoint25D)
        casePrefixOgrDot(wkbMultiLineString25D)
        casePrefixOgrDot(wkbMultiPolygon25D)
        casePrefixOgrDot(wkbGeometryCollection25D)
        casePrefixOgrDot(wkbPolyhedralSurface)
        casePrefixOgrDot(wkbTIN)
        casePrefixOgrDot(wkbTriangle)
        casePrefixOgrDot(wkbPolyhedralSurfaceZ)
        casePrefixOgrDot(wkbTINZ)
        casePrefixOgrDot(wkbTriangleZ)
        casePrefixOgrDot(wkbPointM)
        casePrefixOgrDot(wkbLineStringM)
        casePrefixOgrDot(wkbPolygonM)
        casePrefixOgrDot(wkbMultiPointM)
        casePrefixOgrDot(wkbMultiLineStringM)
        casePrefixOgrDot(wkbMultiPolygonM)
        casePrefixOgrDot(wkbGeometryCollectionM)
        casePrefixOgrDot(wkbCircularStringM)
        casePrefixOgrDot(wkbCompoundCurveM)
        casePrefixOgrDot(wkbCurvePolygonM)
        casePrefixOgrDot(wkbMultiCurveM)
        casePrefixOgrDot(wkbMultiSurfaceM)
        casePrefixOgrDot(wkbCurveM)
        casePrefixOgrDot(wkbSurfaceM)
        casePrefixOgrDot(wkbPolyhedralSurfaceM)
        casePrefixOgrDot(wkbTINM)
        casePrefixOgrDot(wkbTriangleM)
        casePrefixOgrDot(wkbPointZM)
        casePrefixOgrDot(wkbLineStringZM)
        casePrefixOgrDot(wkbPolygonZM)
        casePrefixOgrDot(wkbMultiPointZM)
        casePrefixOgrDot(wkbMultiLineStringZM)
        casePrefixOgrDot(wkbMultiPolygonZM)
        casePrefixOgrDot(wkbGeometryCollectionZM)
        casePrefixOgrDot(wkbCircularStringZM)
        casePrefixOgrDot(wkbCompoundCurveZM)
        casePrefixOgrDot(wkbCurvePolygonZM)
        casePrefixOgrDot(wkbMultiCurveZM)
        casePrefixOgrDot(wkbMultiSurfaceZM)
        casePrefixOgrDot(wkbCurveZM)
        casePrefixOgrDot(wkbSurfaceZM)
        casePrefixOgrDot(wkbPolyhedralSurfaceZM)
        casePrefixOgrDot(wkbTriangleZM)
        casePrefixOgrDot(wkbTINZM)

    }
    return "error";
}

static CPLString OGRAPISpyGetFieldType(OGRFieldType eType)
{
    switch( eType )
    {
        casePrefixOgrDot(OFTInteger)
        casePrefixOgrDot(OFTInteger64)
        casePrefixOgrDot(OFTIntegerList)
        casePrefixOgrDot(OFTInteger64List)
        casePrefixOgrDot(OFTReal)
        casePrefixOgrDot(OFTRealList)
        casePrefixOgrDot(OFTString)
        casePrefixOgrDot(OFTStringList)
        casePrefixOgrDot(OFTWideString)
        casePrefixOgrDot(OFTWideStringList)
        casePrefixOgrDot(OFTBinary)
        casePrefixOgrDot(OFTDate)
        casePrefixOgrDot(OFTTime)
        casePrefixOgrDot(OFTDateTime)
    }
    return "error";
}

#undef casePrefixOgrDot

static CPLString OGRAPISpyGetFeatureDefnVar( OGRFeatureDefnH hFDefn )
{
    std::map<OGRFeatureDefnH, FeatureDefnDescription>::iterator oIter =
        oMapFDefn.find(hFDefn);
    int i = 0;
    if( oIter == oMapFDefn.end() )
    {
        i = static_cast<int>(oMapFDefn.size()) + 1;
        oMapFDefn[hFDefn] = FeatureDefnDescription(hFDefn, i);

        // So that we can check when they are no longer used.
        OGRFeatureDefn::FromHandle(hFDefn)->Reference();
    }
    else
    {
        i = oIter->second.iUniqueNumber;
    }
    return CPLSPrintf("fdefn%d", i);
}

static void OGRAPISpyFlushDefered()
{
    OGRAPISpyFileReopen();
    if( hLayerGetLayerDefn != nullptr )
    {
        OGRFeatureDefnH hDefn =
            OGRFeatureDefn::ToHandle(
                OGRLayer::FromHandle(hLayerGetLayerDefn)->
                    GetLayerDefn());
        fprintf(fpSpyFile, "%s = %s.GetLayerDefn()\n",
            OGRAPISpyGetFeatureDefnVar(hDefn).c_str(),
            OGRAPISpyGetLayerVar(hLayerGetLayerDefn).c_str());

        if( bDeferGetFieldCount )
        {
            fprintf(fpSpyFile, "%s.GetFieldCount()\n",
                    OGRAPISpyGetFeatureDefnVar(hDefn).c_str());
            bDeferGetFieldCount = false;
        }

        hLayerGetLayerDefn = nullptr;
    }

    if( nGetNextFeatureCalls == 1)
    {
        fprintf(fpSpyFile, "%s.GetNextFeature()\n",
            OGRAPISpyGetLayerVar(hLayerGetNextFeature).c_str());
        hLayerGetNextFeature = nullptr;
        nGetNextFeatureCalls = 0;
    }
    else if( nGetNextFeatureCalls > 0)
    {
        fprintf(fpSpyFile, "for i in range(%d):\n", nGetNextFeatureCalls);
        fprintf(fpSpyFile, "    %s.GetNextFeature()\n",
            OGRAPISpyGetLayerVar(hLayerGetNextFeature).c_str());
        hLayerGetNextFeature = nullptr;
        nGetNextFeatureCalls = 0;
    }
}

int OGRAPISpyOpenTakeSnapshot( const char* pszName, int bUpdate )
{
    if( !OGRAPISpyEnabled() || !bUpdate || osSnapshotPath.empty() ||
        aoSetCreatedDS.find(pszName) != aoSetCreatedDS.end() )
        return -1;
    OGRAPISpyFlushDefered();

    VSIStatBufL sStat;
    if( VSIStatL( pszName, &sStat ) == 0 )
    {
        bOGRAPISpyEnabled = -1;
        GDALDatasetH hDS =
            GDALOpenEx(pszName, GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
        char** papszFileList = hDS ? GDALDataset::FromHandle(hDS)->GetFileList() : nullptr;
        GDALClose(hDS);
        bOGRAPISpyEnabled = true;
        if( papszFileList )
        {
            int i = 1;
            CPLString osBaseDir;
            CPLString osSrcDir;
            CPLString osWorkingDir;
            while( true )
            {
                osBaseDir =
                    CPLFormFilename(osSnapshotPath,
                                    CPLSPrintf("snapshot_%d", i), nullptr );
                if( VSIStatL( osBaseDir, &sStat ) != 0 )
                    break;
                i++;
            }
            VSIMkdir( osSnapshotPath, 0777 );
            VSIMkdir( osBaseDir, 0777 );
            osSrcDir = CPLFormFilename( osBaseDir, "source", nullptr );
            VSIMkdir( osSrcDir, 0777 );
            osWorkingDir = CPLFormFilename( osBaseDir, "working", nullptr );
            VSIMkdir( osWorkingDir, 0777 );

            OGRAPISpyFileReopen();
            fprintf(fpSpyFile, "# Take snapshot of %s\n", pszName);
            fprintf(fpSpyFile, "try:\n");
            fprintf(fpSpyFile, "    shutil.rmtree('%s')\n",
                    osWorkingDir.c_str());
            fprintf(fpSpyFile, "except:\n");
            fprintf(fpSpyFile, "    pass\n");
            fprintf(fpSpyFile, "os.mkdir('%s')\n", osWorkingDir.c_str());
            for( char** papszIter = papszFileList; *papszIter; papszIter++ )
            {
                CPLString osSnapshotSrcFile = CPLFormFilename(
                        osSrcDir, CPLGetFilename(*papszIter), nullptr);
                CPLString osSnapshotWorkingFile = CPLFormFilename(
                        osWorkingDir, CPLGetFilename(*papszIter), nullptr);
                CPLCopyFile( osSnapshotSrcFile, *papszIter );
                CPLCopyFile( osSnapshotWorkingFile, *papszIter );
                fprintf(fpSpyFile, "shutil.copy('%s', '%s')\n",
                        osSnapshotSrcFile.c_str(),
                        osSnapshotWorkingFile.c_str());
            }
            CSLDestroy(papszFileList);
            return i;
        }
    }
    return -1;
}

void OGRAPISpyOpen( const char* pszName, int bUpdate, int iSnapshot,
                    GDALDatasetH* phDS )
{
    if( !OGRAPISpyEnabled() ) return;
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();

    CPLString osName;
    if( iSnapshot > 0 )
    {
        CPLString osBaseDir =
            CPLFormFilename(osSnapshotPath,
                            CPLSPrintf("snapshot_%d", iSnapshot), nullptr );
        CPLString osWorkingDir = CPLFormFilename( osBaseDir, "working", nullptr );
        osName = CPLFormFilename(osWorkingDir, CPLGetFilename(pszName), nullptr);
        pszName = osName.c_str();

        if( *phDS != nullptr )
        {
            bOGRAPISpyEnabled = -1;
            GDALClose( GDALDataset::FromHandle(*phDS) );
            *phDS = GDALOpenEx(pszName,
                               GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                               nullptr, nullptr, nullptr);
            bOGRAPISpyEnabled = true;
        }
    }

    OGRAPISpyFileReopen();
    if( *phDS != nullptr )
        fprintf(fpSpyFile,
                "%s = ",
                OGRAPISpyGetDSVar(*phDS).c_str());
    if( bUpdate )
        fprintf(fpSpyFile, "gdal.OpenEx(%s, gdal.OF_VECTOR | gdal.OF_UPDATE)\n",
            OGRAPISpyGetString(pszName).c_str());
    else
        fprintf(fpSpyFile, "gdal.OpenEx(%s, gdal.OF_VECTOR)\n",
                OGRAPISpyGetString(pszName).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpyPreClose( GDALDatasetH hDS )
{
    if( !OGRAPISpyEnabled() ) return;
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "ds%d = None\n", oMapDS[hDS].iDS);
    oSetDSIndex.erase(oMapDS[hDS].iDS);
    oMapDS.erase(hDS);
    OGRAPISpyFileClose();
}

void OGRAPISpyPostClose()
{
    if( !GDALIsInGlobalDestructor() )
    {
        if( !OGRAPISpyEnabled() ) return;
        CPLMutexHolderD(&hMutex);
        std::map<OGRFeatureDefnH, FeatureDefnDescription>::iterator oIter =
            oMapFDefn.begin();
        std::vector<OGRFeatureDefnH> oArray;
        for( ; oIter != oMapFDefn.end(); ++oIter )
        {
            FeatureDefnDescription& featureDefnDescription = oIter->second;
            if( OGRFeatureDefn::FromHandle(featureDefnDescription.hFDefn)->
                    GetReferenceCount() == 1 )
            {
                oArray.push_back(featureDefnDescription.hFDefn);
            }
        }
        for( auto& hFDefn: oArray )
        {
            FeatureDefnDescription& featureDefnDescription =
                oMapFDefn[hFDefn];
            OGRFeatureDefn::FromHandle(featureDefnDescription.hFDefn)->Release();
            featureDefnDescription.Free();
            oMapFDefn.erase(hFDefn);
        }
    }
}

void OGRAPISpyCreateDataSource( OGRSFDriverH hDriver, const char* pszName,
                                char** papszOptions, OGRDataSourceH hDS )
{
    if( !OGRAPISpyEnabled() ) return;
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    if( hDS != nullptr )
        fprintf(fpSpyFile, "%s = ", OGRAPISpyGetDSVar(hDS).c_str());
    fprintf(fpSpyFile,
            "ogr.GetDriverByName('%s').CreateDataSource(%s, options=%s)\n",
            GDALGetDriverShortName(reinterpret_cast<GDALDriverH>(hDriver)),
            OGRAPISpyGetString(pszName).c_str(),
            OGRAPISpyGetOptions(papszOptions).c_str());
    if( hDS != nullptr )
    {
        aoSetCreatedDS.insert(pszName);
    }
    OGRAPISpyFileClose();
}

void OGRAPISpyDeleteDataSource( OGRSFDriverH hDriver, const char* pszName )
{
    if( !OGRAPISpyEnabled() ) return;
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "ogr.GetDriverByName('%s').DeleteDataSource(%s)\n",
            GDALGetDriverShortName(reinterpret_cast<GDALDriverH>(hDriver)),
            OGRAPISpyGetString(pszName).c_str());
    aoSetCreatedDS.erase(pszName);
    OGRAPISpyFileClose();
}

void OGRAPISpy_DS_GetLayer( GDALDatasetH hDS, int iLayer, OGRLayerH hLayer )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    if( hLayer != nullptr )
        fprintf(fpSpyFile, "%s = ",
            OGRAPISpyGetAndRegisterLayerVar(hDS, hLayer).c_str());
    fprintf(fpSpyFile, "%s.GetLayer(%d)\n",
            OGRAPISpyGetDSVar(hDS).c_str(),
            iLayer);
    OGRAPISpyFileClose();
}

void OGRAPISpy_DS_GetLayerCount( GDALDatasetH hDS )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.GetLayerCount()\n", OGRAPISpyGetDSVar(hDS).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_DS_GetLayerByName( GDALDatasetH hDS, const char* pszLayerName,
                                  OGRLayerH hLayer )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    if( hLayer != nullptr )
        fprintf(fpSpyFile, "%s = ",
            OGRAPISpyGetAndRegisterLayerVar(hDS, hLayer).c_str());
    fprintf(fpSpyFile, "%s.GetLayerByName(%s)\n",
            OGRAPISpyGetDSVar(hDS).c_str(),
            OGRAPISpyGetString(pszLayerName).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_DS_ExecuteSQL( GDALDatasetH hDS,
                              const char *pszStatement,
                              OGRGeometryH hSpatialFilter,
                              const char *pszDialect,
                              OGRLayerH hLayer )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    if( hLayer != nullptr )
        fprintf(fpSpyFile, "%s = ",
            OGRAPISpyGetAndRegisterLayerVar(hDS, hLayer).c_str());
    fprintf(fpSpyFile, "%s.ExecuteSQL(%s, %s, %s)\n",
            OGRAPISpyGetDSVar(hDS).c_str(),
            OGRAPISpyGetString(pszStatement).c_str(),
            OGRAPISpyGetGeom(hSpatialFilter).c_str(),
            OGRAPISpyGetString(pszDialect).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_DS_ReleaseResultSet( GDALDatasetH hDS, OGRLayerH hLayer )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.ReleaseResultSet(%s)\n",
            OGRAPISpyGetDSVar(hDS).c_str(),
            (hLayer) ? OGRAPISpyGetLayerVar(hLayer).c_str() : "None");

    DatasetDescription& dd = oMapDS[hDS];
    dd.oMapLayer.erase(hLayer);
    oGlobalMapLayer.erase(hLayer);

    OGRAPISpyFileClose();
}

void OGRAPISpy_DS_CreateLayer( GDALDatasetH hDS,
                               const char * pszName,
                               OGRSpatialReferenceH hSpatialRef,
                               OGRwkbGeometryType eType,
                               char ** papszOptions,
                               OGRLayerH hLayer )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    if( hLayer != nullptr )
        fprintf(fpSpyFile, "%s = ",
            OGRAPISpyGetAndRegisterLayerVar(hDS, hLayer).c_str());
    fprintf(fpSpyFile,
            "%s.CreateLayer(%s, srs=%s, geom_type=%s, options=%s)\n",
            OGRAPISpyGetDSVar(hDS).c_str(),
            OGRAPISpyGetString(pszName).c_str(),
            OGRAPISpyGetSRS(hSpatialRef).c_str(),
            OGRAPISpyGetGeomType(eType).c_str(),
            OGRAPISpyGetOptions(papszOptions).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_DS_DeleteLayer( GDALDatasetH hDS, int iLayer )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.DeleteLayer(%d)\n",
            OGRAPISpyGetDSVar(hDS).c_str(), iLayer);
    // Should perhaps remove from the maps.
    OGRAPISpyFileClose();
}

void OGRAPISpy_Dataset_StartTransaction( GDALDatasetH hDS, int bForce )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.StartTransaction(%d)\n",
            OGRAPISpyGetDSVar(hDS).c_str(),
                              bForce);
    OGRAPISpyFileClose();
}

void OGRAPISpy_Dataset_CommitTransaction( GDALDatasetH hDS )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.CommitTransaction()\n",
            OGRAPISpyGetDSVar(hDS).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_Dataset_RollbackTransaction( GDALDatasetH hDS )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.RollbackTransaction()\n",
            OGRAPISpyGetDSVar(hDS).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_GetFeatureCount( OGRLayerH hLayer, int bForce )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.GetFeatureCount(force=%d)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), bForce);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_GetExtent( OGRLayerH hLayer, int bForce )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.GetExtent(force=%d)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), bForce);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_GetExtentEx( OGRLayerH hLayer, int iGeomField, int bForce )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.GetExtent(geom_field=%d, force=%d)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), iGeomField, bForce);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_SetAttributeFilter( OGRLayerH hLayer, const char* pszFilter )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.SetAttributeFilter(%s)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(),
            OGRAPISpyGetString(pszFilter).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_GetFeature( OGRLayerH hLayer, GIntBig nFeatureId )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.GetFeature(" CPL_FRMT_GIB ")\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), nFeatureId);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_SetNextByIndex( OGRLayerH hLayer, GIntBig nIndex )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.SetNextByIndex(" CPL_FRMT_GIB ")\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), nIndex);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_GetNextFeature( OGRLayerH hLayer )
{
    CPLMutexHolderD(&hMutex);
    if( hLayerGetNextFeature != hLayer )
    {
        OGRAPISpyFlushDefered();
        OGRAPISpyFileClose();
    }
    hLayerGetNextFeature = hLayer;
    nGetNextFeatureCalls++;
}

static void OGRAPISpyDumpFeature( OGRFeatureH hFeat )
{
    OGRFeature* poFeature = OGRFeature::FromHandle(hFeat);

    fprintf(fpSpyFile, "f = ogr.Feature(%s)\n",
            OGRAPISpyGetFeatureDefnVar(OGRFeatureDefn::ToHandle(poFeature->
                   GetDefnRef())).c_str());
    if( poFeature->GetFID() != -1 )
        fprintf(fpSpyFile, "f.SetFID(" CPL_FRMT_GIB ")\n", poFeature->GetFID());
    for( int i = 0; i < poFeature->GetFieldCount(); i++ )
    {
        if( poFeature->IsFieldNull(i) )
        {
            fprintf(fpSpyFile, "f.SetFieldNull(%d)\n", i);
        }
        else if( poFeature->IsFieldSet(i) )
        {
            switch( poFeature->GetFieldDefnRef(i)->GetType())
            {
                case OFTInteger:
                    fprintf(fpSpyFile, "f.SetField(%d, %d)\n", i,
                    poFeature->GetFieldAsInteger(i));
                    break;
                case OFTReal:
                    fprintf(fpSpyFile,
                            "%s", CPLSPrintf("f.SetField(%d, %.16g)\n", i,
                    poFeature->GetFieldAsDouble(i)));
                    break;
                case OFTString:
                    fprintf(fpSpyFile, "f.SetField(%d, %s)\n", i,
                    OGRAPISpyGetString(poFeature->GetFieldAsString(i)).c_str());
                    break;
                default:
                    fprintf(fpSpyFile, "f.SetField(%d, %s) #FIXME\n", i,
                    OGRAPISpyGetString(poFeature->GetFieldAsString(i)).c_str());
                    break;
            }
        }
    }
    for( int i = 0; i < poFeature->GetGeomFieldCount(); i++ )
    {
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom != nullptr )
        {
            fprintf(fpSpyFile, "f.SetGeomField(%d, %s)\n",
                    i,
                    OGRAPISpyGetGeom(OGRGeometry::ToHandle(poGeom)).c_str());
        }
    }
    const char* pszStyleString = poFeature->GetStyleString();
    if( pszStyleString != nullptr )
        fprintf(fpSpyFile, "f.SetStyleString(%s)\n",
                OGRAPISpyGetString(pszStyleString).c_str() );
}

void OGRAPISpy_L_SetFeature( OGRLayerH hLayer, OGRFeatureH hFeat )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    OGRAPISpyDumpFeature(hFeat);
    fprintf(fpSpyFile, "%s.SetFeature(f)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str());
    // In case layer defn is changed afterwards.
    fprintf(fpSpyFile, "f = None\n");
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_CreateFeature( OGRLayerH hLayer, OGRFeatureH hFeat )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    OGRAPISpyDumpFeature(hFeat);
    fprintf(fpSpyFile, "%s.CreateFeature(f)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str());
    // In case layer defn is changed afterwards.
    fprintf(fpSpyFile, "f = None\n");
    OGRAPISpyFileClose();
}

static void OGRAPISpyDumpFieldDefn( OGRFieldDefn* poFieldDefn )
{
    CPLMutexHolderD(&hMutex);
    fprintf(fpSpyFile, "fd = ogr.FieldDefn(%s, %s)\n",
            OGRAPISpyGetString(poFieldDefn->GetNameRef()).c_str(),
            OGRAPISpyGetFieldType(poFieldDefn->GetType()).c_str());
    if( poFieldDefn->GetWidth() > 0 )
        fprintf(fpSpyFile, "fd.SetWidth(%d)\n", poFieldDefn->GetWidth() );
    if( poFieldDefn->GetPrecision() > 0 )
        fprintf(fpSpyFile, "fd.SetPrecision(%d)\n",
                poFieldDefn->GetPrecision() );
    if( !poFieldDefn->IsNullable() )
        fprintf(fpSpyFile, "fd.SetNullable(0)\n");
    if( poFieldDefn->GetDefault() != nullptr )
        fprintf(fpSpyFile, "fd.SetDefault(%s)\n",
                OGRAPISpyGetString(poFieldDefn->GetDefault()).c_str());
}

void OGRAPISpy_L_CreateField( OGRLayerH hLayer, OGRFieldDefnH hField,
                              int bApproxOK )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    OGRFieldDefn* poFieldDefn = OGRFieldDefn::FromHandle(hField);
    OGRAPISpyDumpFieldDefn(poFieldDefn);
    fprintf(fpSpyFile, "%s.CreateField(fd, approx_ok=%d)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), bApproxOK);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_DeleteField( OGRLayerH hLayer, int iField )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.DeleteField(%d)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), iField);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_ReorderFields( OGRLayerH hLayer, int* panMap )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    OGRLayer* poLayer = OGRLayer::FromHandle(hLayer);
    fprintf(fpSpyFile, "%s.ReorderFields([",
            OGRAPISpyGetLayerVar(hLayer).c_str());
    for( int i = 0; i < poLayer->GetLayerDefn()->GetFieldCount(); i++ )
    {
        if( i > 0 ) fprintf(fpSpyFile, ", ");
        fprintf(fpSpyFile, "%d", panMap[i]);
    }
    fprintf(fpSpyFile, "])\n");
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_ReorderField( OGRLayerH hLayer, int iOldFieldPos,
                               int iNewFieldPos )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.ReorderField(%d, %d)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), iOldFieldPos, iNewFieldPos);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_AlterFieldDefn( OGRLayerH hLayer, int iField,
                                 OGRFieldDefnH hNewFieldDefn, int nFlags )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    OGRFieldDefn* poFieldDefn = OGRFieldDefn::FromHandle(hNewFieldDefn);
    OGRAPISpyDumpFieldDefn(poFieldDefn);
    fprintf(fpSpyFile, "%s.AlterFieldDefn(%d, fd, %d)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), iField, nFlags);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_CreateGeomField( OGRLayerH hLayer, OGRGeomFieldDefnH hField,
                                  int bApproxOK )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    OGRGeomFieldDefn* poGeomFieldDefn = OGRGeomFieldDefn::FromHandle(hField);

    fprintf(fpSpyFile, "geom_fd = ogr.GeomFieldDefn(%s, %s)\n",
            OGRAPISpyGetString(poGeomFieldDefn->GetNameRef()).c_str(),
            OGRAPISpyGetGeomType(poGeomFieldDefn->GetType()).c_str());
    if( poGeomFieldDefn->GetSpatialRef() != nullptr )
        fprintf(
            fpSpyFile, "geom_fd.SetSpatialRef(%s)\n",
            OGRAPISpyGetSRS(
                OGRSpatialReference::ToHandle
                    (poGeomFieldDefn->GetSpatialRef())).c_str() );
    if( !poGeomFieldDefn->IsNullable() )
        fprintf(fpSpyFile, "geom_fd.SetNullable(0)\n");
    fprintf(fpSpyFile, "%s.CreateGeomField(geom_fd, approx_ok=%d)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), bApproxOK);
    OGRAPISpyFileClose();
}

static void OGRAPISpy_L_Op( OGRLayerH hLayer, const char* pszMethod )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.%s()\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), pszMethod);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_StartTransaction( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "StartTransaction"); }
void OGRAPISpy_L_CommitTransaction( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "CommitTransaction"); }
void OGRAPISpy_L_RollbackTransaction( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "RollbackTransaction"); }

void OGRAPISpy_L_GetLayerDefn( OGRLayerH hLayer )
{
    if( hLayer != hLayerGetLayerDefn )
    {
        OGRAPISpyFlushDefered();
        hLayerGetLayerDefn = hLayer;
        OGRAPISpyFileClose();
    }
}

void OGRAPISpy_L_GetSpatialRef( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "GetSpatialRef"); }
void OGRAPISpy_L_GetSpatialFilter( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "GetSpatialFilter"); }
void OGRAPISpy_L_ResetReading( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "ResetReading"); }
void OGRAPISpy_L_SyncToDisk( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "SyncToDisk"); }
void OGRAPISpy_L_GetFIDColumn( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "GetFIDColumn"); }
void OGRAPISpy_L_GetGeometryColumn( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "GetGeometryColumn"); }
void OGRAPISpy_L_GetName( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "GetName"); }
void OGRAPISpy_L_GetGeomType( OGRLayerH hLayer )
    { OGRAPISpy_L_Op(hLayer, "GetGeomType"); }

void OGRAPISpy_L_FindFieldIndex( OGRLayerH hLayer, const char *pszFieldName,
                                 int bExactMatch )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.FindFieldIndex(%s, %d)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(),
            OGRAPISpyGetString(pszFieldName).c_str(), bExactMatch);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_TestCapability( OGRLayerH hLayer, const char* pszCap )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.TestCapability(%s)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(),
            OGRAPISpyGetString(pszCap).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_SetSpatialFilter( OGRLayerH hLayer, OGRGeometryH hGeom )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.SetSpatialFilter(%s)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(),
            OGRAPISpyGetGeom(hGeom).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_SetSpatialFilterEx( OGRLayerH hLayer, int iGeomField,
                                     OGRGeometryH hGeom )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.SetSpatialFilter(%d, %s)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(),
            iGeomField,
            OGRAPISpyGetGeom(hGeom).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_SetSpatialFilterRect( OGRLayerH hLayer,
                                       double dfMinX, double dfMinY,
                                       double dfMaxX, double dfMaxY )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s",
            CPLSPrintf("%s.SetSpatialFilterRect(%.16g, %.16g, %.16g, %.16g)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(),
            dfMinX, dfMinY, dfMaxX, dfMaxY));
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_SetSpatialFilterRectEx( OGRLayerH hLayer, int iGeomField,
                                         double dfMinX, double dfMinY,
                                         double dfMaxX, double dfMaxY )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s", CPLSPrintf("%s.SetSpatialFilterRect(%d, "
            "%.16g, %.16g, %.16g, %.16g)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(),
            iGeomField,
            dfMinX, dfMinY, dfMaxX, dfMaxY));
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_DeleteFeature( OGRLayerH hLayer, GIntBig nFID )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.DeleteFeature(" CPL_FRMT_GIB ")\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(), nFID);
    OGRAPISpyFileClose();
}

void OGRAPISpy_L_SetIgnoredFields( OGRLayerH hLayer,
                                   const char** papszIgnoredFields )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.SetIgnoredFields(%s)\n",
            OGRAPISpyGetLayerVar(hLayer).c_str(),
            OGRAPISpyGetOptions(
                const_cast<char **>(papszIgnoredFields)).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_FD_GetGeomType( OGRFeatureDefnH hDefn )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.GetGeomType()\n",
            OGRAPISpyGetFeatureDefnVar(hDefn).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_FD_GetFieldCount( OGRFeatureDefnH hDefn )
{
    CPLMutexHolderD(&hMutex);
    if( hLayerGetLayerDefn != nullptr &&
        OGRFeatureDefn::ToHandle(
            OGRLayer::FromHandle(hLayerGetLayerDefn)->GetLayerDefn()) ==
        hDefn )
    {
        bDeferGetFieldCount = true;
    }
    else
    {
        OGRAPISpyFlushDefered();
        fprintf(fpSpyFile, "%s.GetFieldCount()\n",
                OGRAPISpyGetFeatureDefnVar(hDefn).c_str());
        OGRAPISpyFileClose();
    }
}

void OGRAPISpy_FD_GetFieldDefn( OGRFeatureDefnH hDefn, int iField,
                                OGRFieldDefnH hField )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s_fielddefn%d = %s.GetFieldDefn(%d)\n",
            OGRAPISpyGetFeatureDefnVar(hDefn).c_str(),
            iField,
            OGRAPISpyGetFeatureDefnVar(hDefn).c_str(),
            iField);

    std::map<OGRFieldDefnH, CPLString>::iterator oIter =
                            oGlobalMapFieldDefn.find(hField);
    if( oIter == oGlobalMapFieldDefn.end() )
    {
        oMapFDefn[hDefn].oMapFieldDefn[hField] = iField;
        oGlobalMapFieldDefn[hField] =
            CPLSPrintf("%s_fielddefn%d",
                       OGRAPISpyGetFeatureDefnVar(hDefn).c_str(),
                       iField);
    }

    OGRAPISpyFileClose();
}

void OGRAPISpy_FD_GetFieldIndex( OGRFeatureDefnH hDefn,
                                 const char* pszFieldName )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.GetFieldIndex(%s)\n",
            OGRAPISpyGetFeatureDefnVar(hDefn).c_str(),
            OGRAPISpyGetString(pszFieldName).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_Fld_GetXXXX( OGRFieldDefnH hField, const char* pszOp )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.%s()\n",
            oGlobalMapFieldDefn[hField].c_str(), pszOp);
    OGRAPISpyFileClose();
}

void OGRAPISpy_FD_GetGeomFieldCount( OGRFeatureDefnH hDefn )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.GetGeomFieldCount()\n",
            OGRAPISpyGetFeatureDefnVar(hDefn).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_FD_GetGeomFieldDefn( OGRFeatureDefnH hDefn, int iGeomField,
                                    OGRGeomFieldDefnH hGeomField )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s_geomfielddefn%d = %s.GetGeomFieldDefn(%d)\n",
            OGRAPISpyGetFeatureDefnVar(hDefn).c_str(),
            iGeomField,
            OGRAPISpyGetFeatureDefnVar(hDefn).c_str(),
            iGeomField);

    std::map<OGRGeomFieldDefnH, CPLString>::iterator oIter =
                            oGlobalMapGeomFieldDefn.find(hGeomField);
    if( oIter == oGlobalMapGeomFieldDefn.end() )
    {
        oMapFDefn[hDefn].oMapGeomFieldDefn[hGeomField] = iGeomField;
        oGlobalMapGeomFieldDefn[hGeomField] =
            CPLSPrintf("%s_geomfielddefn%d",
                       OGRAPISpyGetFeatureDefnVar(hDefn).c_str(),
                       iGeomField);
    }

    OGRAPISpyFileClose();
}

void OGRAPISpy_FD_GetGeomFieldIndex( OGRFeatureDefnH hDefn,
                                     const char* pszFieldName )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.GetGeomFieldIndex(%s)\n",
            OGRAPISpyGetFeatureDefnVar(hDefn).c_str(),
            OGRAPISpyGetString(pszFieldName).c_str());
    OGRAPISpyFileClose();
}

void OGRAPISpy_GFld_GetXXXX( OGRGeomFieldDefnH hGeomField, const char* pszOp )
{
    CPLMutexHolderD(&hMutex);
    OGRAPISpyFlushDefered();
    fprintf(fpSpyFile, "%s.%s()\n",
            oGlobalMapGeomFieldDefn[hGeomField].c_str(), pszOp);
    OGRAPISpyFileClose();
}

void OGRAPISPYCPLSetConfigOption(const char* pszKey, const char* pszValue )
{
    if( STARTS_WITH(pszKey, "OGR_API_SPY_") || STARTS_WITH(pszKey, "__") )
        return;
    if( !OGRAPISpyEnabled() )
        return;
    OGRAPISpyFlushDefered();
    if( pszValue )
    {
        fprintf(fpSpyFile, "gdal.SetConfigOption(%s, %s)\n",
                OGRAPISpyGetString(pszKey).c_str(),
                OGRAPISpyGetString(pszValue).c_str());
    }
    else
    {
        fprintf(fpSpyFile, "gdal.SetConfigOption(%s, None)\n",
                OGRAPISpyGetString(pszKey).c_str());
    }
    OGRAPISpyFileClose();
}

void OGRAPISPYCPLSetThreadLocalConfigOption(const char* pszKey, const char* pszValue )
{
    if( STARTS_WITH(pszKey, "OGR_API_SPY_") || STARTS_WITH(pszKey, "__") )
        return;
    if( !OGRAPISpyEnabled() )
        return;
    OGRAPISpyFlushDefered();
    if( pszValue )
    {
        fprintf(fpSpyFile, "gdal.SetConfigOption(%s, %s) # SetThreadLocalConfigOption actually\n",
                OGRAPISpyGetString(pszKey).c_str(),
                OGRAPISpyGetString(pszValue).c_str());
    }
    else
    {
        fprintf(fpSpyFile, "gdal.SetConfigOption(%s, None) # SetThreadLocalConfigOption actually\n",
                OGRAPISpyGetString(pszKey).c_str());
    }
    OGRAPISpyFileClose();
}

#endif  // OGRAPISPY_ENABLED
