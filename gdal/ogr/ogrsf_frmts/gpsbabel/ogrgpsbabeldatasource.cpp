/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGPSBabelDataSource class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_error.h"
#include "cpl_spawn.h"
#include "ogr_gpsbabel.h"

#include <cstring>
#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/*                      OGRGPSBabelDataSource()                         */
/************************************************************************/

OGRGPSBabelDataSource::OGRGPSBabelDataSource()
{
}

/************************************************************************/
/*                     ~OGRGPSBabelDataSource()                         */
/************************************************************************/

OGRGPSBabelDataSource::~OGRGPSBabelDataSource()

{
    CPLFree(pszName);
    CPLFree(pszGPSBabelDriverName);
    CPLFree(pszFilename);

    OGRGPSBabelDataSource::CloseDependentDatasets();

    if (!osTmpFileName.empty())
        VSIUnlink(osTmpFileName.c_str());
}

/************************************************************************/
/*                     CloseDependentDatasets()                         */
/************************************************************************/

int OGRGPSBabelDataSource::CloseDependentDatasets()
{
    if( poGPXDS == nullptr )
        return FALSE;

    GDALClose( poGPXDS );
    poGPXDS = nullptr;
    return TRUE;
}

/************************************************************************/
/*                             GetArgv()                                */
/************************************************************************/

static char** GetArgv( int bExplicitFeatures, int bWaypoints, int bRoutes,
                       int bTracks, const char* pszGPSBabelDriverName,
                       const char* pszFilename )
{
    char** argv = CSLAddString(nullptr, "gpsbabel");
    if (bExplicitFeatures)
    {
        if (bWaypoints) argv = CSLAddString(argv, "-w");
        if (bRoutes) argv = CSLAddString(argv, "-r");
        if (bTracks) argv = CSLAddString(argv, "-t");
    }
    argv = CSLAddString(argv, "-i");
    argv = CSLAddString(argv, pszGPSBabelDriverName);
    argv = CSLAddString(argv, "-f");
    argv = CSLAddString(argv, pszFilename);
    argv = CSLAddString(argv, "-o");
    argv = CSLAddString(argv, "gpx,gpxver=1.1");
    argv = CSLAddString(argv, "-F");
    argv = CSLAddString(argv, "-");

    return argv;
}

/************************************************************************/
/*                         IsSpecialFile()                              */
/************************************************************************/

bool OGRGPSBabelDataSource::IsSpecialFile( const char* pszFilename )
{
    return
        STARTS_WITH(pszFilename, "/dev/") ||
        STARTS_WITH(pszFilename, "usb:") ||
        (STARTS_WITH(pszFilename, "COM")  && atoi(pszFilename + 3) > 0);
}

/************************************************************************/
/*                       IsValidDriverName()                            */
/************************************************************************/

bool
OGRGPSBabelDataSource::IsValidDriverName( const char* pszGPSBabelDriverName )
{
    for( int i = 0; pszGPSBabelDriverName[i] != '\0'; i++ )
    {
        char ch = pszGPSBabelDriverName[i];
        if( !((ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') ||
              ch == '_' || ch == '=' || ch == '.'  || ch == ',') )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid GPSBabel driver name");
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGPSBabelDataSource::Open( const char * pszDatasourceName,
                                 const char* pszGPSBabelDriverNameIn,
                                 char** papszOpenOptionsIn )

{
    if (!STARTS_WITH_CI(pszDatasourceName, "GPSBABEL:"))
    {
        CPLAssert(pszGPSBabelDriverNameIn);
        pszGPSBabelDriverName = CPLStrdup(pszGPSBabelDriverNameIn);
        pszFilename = CPLStrdup(pszDatasourceName);
    }
    else
    {
        if( CSLFetchNameValue(papszOpenOptionsIn, "FILENAME") )
            pszFilename = CPLStrdup(CSLFetchNameValue(papszOpenOptionsIn,
                                                      "FILENAME"));

        if( CSLFetchNameValue(papszOpenOptionsIn, "GPSBABEL_DRIVER") )
        {
            if( pszFilename == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Missing FILENAME");
                return FALSE;
            }

            pszGPSBabelDriverName
                = CPLStrdup(CSLFetchNameValue(papszOpenOptionsIn, "DRIVER"));

            /* A bit of validation to avoid command line injection */
            if (!IsValidDriverName(pszGPSBabelDriverName))
                return FALSE;
        }
    }

    pszName = CPLStrdup( pszDatasourceName );

    bool bExplicitFeatures = false;
    bool bWaypoints = true;
    bool bTracks = true;
    bool bRoutes = true;

    if (pszGPSBabelDriverName == nullptr)
    {
        const char* pszSep = strchr(pszDatasourceName + 9, ':');
        if (pszSep == nullptr)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Wrong syntax. Expected GPSBabel:driver_name:file_name");
            return FALSE;
        }

        pszGPSBabelDriverName = CPLStrdup(pszDatasourceName + 9);
        *(strchr(pszGPSBabelDriverName, ':')) = '\0';

        /* A bit of validation to avoid command line injection */
        if (!IsValidDriverName(pszGPSBabelDriverName))
            return FALSE;

        /* Parse optional features= option */
        if (STARTS_WITH_CI(pszSep+1, "features="))
        {
            const char* pszNextSep = strchr(pszSep+1, ':');
            if (pszNextSep == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Wrong syntax. Expected "
                         "GPSBabel:driver_name[,options]*:["
                         "features=waypoints,tracks,routes:]file_name");
                return FALSE;
            }

            char* pszFeatures = CPLStrdup(pszSep+1+9);
            *strchr(pszFeatures, ':') = 0;
            char** papszTokens = CSLTokenizeString(pszFeatures);
            char** papszIter = papszTokens;
            bool bErr = false;
            bExplicitFeatures = true;
            bWaypoints = false;
            bTracks = false;
            bRoutes = false;
            while(papszIter && *papszIter)
            {
                if (EQUAL(*papszIter, "waypoints"))
                    bWaypoints = true;
                else if (EQUAL(*papszIter, "tracks"))
                    bTracks = true;
                else if (EQUAL(*papszIter, "routes"))
                    bRoutes = true;
                else
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Wrong value for 'features' options");
                    bErr = true;
                }
                papszIter++;
            }
            CSLDestroy(papszTokens);
            CPLFree(pszFeatures);

            if (bErr)
                return FALSE;

            pszSep = pszNextSep;
        }

        if( pszFilename == nullptr )
            pszFilename = CPLStrdup(pszSep+1);
    }

    const char* pszOptionUseTempFile = CPLGetConfigOption("USE_TEMPFILE", nullptr);
    if (pszOptionUseTempFile && CPLTestBool(pszOptionUseTempFile))
        osTmpFileName = CPLGenerateTempFilename(nullptr);
    else
        osTmpFileName.Printf("/vsimem/ogrgpsbabeldatasource_%p", this);

    bool bRet = false;
    if (IsSpecialFile(pszFilename))
    {
        /* Special file : don't try to open it */
        char** argv = GetArgv(bExplicitFeatures, bWaypoints, bRoutes,
                              bTracks, pszGPSBabelDriverName, pszFilename);
        VSILFILE* tmpfp = VSIFOpenL(osTmpFileName.c_str(), "wb");
        bRet = (CPLSpawn(argv, nullptr, tmpfp, TRUE) == 0);
        VSIFCloseL(tmpfp);
        tmpfp = nullptr;
        CSLDestroy(argv);
        argv = nullptr;
    }
    else
    {
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (fp == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot open file %s", pszFilename);
            return FALSE;
        }

        char** argv = GetArgv(bExplicitFeatures, bWaypoints, bRoutes,
                              bTracks, pszGPSBabelDriverName, "-");

        VSILFILE* tmpfp = VSIFOpenL(osTmpFileName.c_str(), "wb");

        CPLPushErrorHandler(CPLQuietErrorHandler);
        bRet = (CPLSpawn(argv, fp, tmpfp, TRUE) == 0);
        CPLPopErrorHandler();

        CSLDestroy(argv);
        argv = nullptr;

        CPLErr nLastErrorType = CPLGetLastErrorType();
        CPLErrorNum nLastErrorNo = CPLGetLastErrorNo();
        CPLString osLastErrorMsg = CPLGetLastErrorMsg();

        VSIFCloseL(tmpfp);
        tmpfp = nullptr;

        VSIFCloseL(fp);
        fp = nullptr;

        if (!bRet)
        {
            if ( strstr(osLastErrorMsg.c_str(),
                        "This format cannot be used in piped commands") == nullptr)
            {
                CPLError( nLastErrorType, nLastErrorNo, "%s",
                          osLastErrorMsg.c_str());
            }
            else
            {
                VSIStatBuf sStatBuf;
                if (VSIStat(pszFilename, &sStatBuf) != 0)
                {
                    CPLError( CE_Failure, CPLE_NotSupported,
                              "Driver %s only supports real (non virtual) "
                              "files",
                              pszGPSBabelDriverName );
                    return FALSE;
                }

                /* Try without piping in */
                argv = GetArgv(bExplicitFeatures, bWaypoints, bRoutes,
                              bTracks, pszGPSBabelDriverName, pszFilename);
                tmpfp = VSIFOpenL(osTmpFileName.c_str(), "wb");
                bRet = (CPLSpawn(argv, nullptr, tmpfp, TRUE) == 0);
                VSIFCloseL(tmpfp);
                tmpfp = nullptr;

                CSLDestroy(argv);
                argv = nullptr;
            }
        }
    }

    if (bRet)
    {
        poGPXDS = static_cast<GDALDataset *>(
            GDALOpenEx( osTmpFileName.c_str(),
                        GDAL_OF_VECTOR, nullptr, nullptr, nullptr ) );
        if (poGPXDS)
        {
            if (bWaypoints)
            {
                OGRLayer* poLayer = poGPXDS->GetLayerByName("waypoints");
                if (poLayer != nullptr && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
            }

            if (bRoutes)
            {
                OGRLayer* poLayer = poGPXDS->GetLayerByName("routes");
                if (poLayer != nullptr && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
                poLayer = poGPXDS->GetLayerByName("route_points");
                if (poLayer != nullptr && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
            }

            if (bTracks)
            {
                OGRLayer* poLayer = poGPXDS->GetLayerByName("tracks");
                if (poLayer != nullptr && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
                poLayer = poGPXDS->GetLayerByName("track_points");
                if (poLayer != nullptr && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
            }
        }
    }

    return nLayers > 0;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGPSBabelDataSource::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGPSBabelDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;

    return apoLayers[iLayer];
}
