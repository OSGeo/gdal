/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGPSBabelDataSource class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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

#include "ogr_gpsbabel.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_error.h"

#include <string.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                      OGRGPSBabelDataSource()                         */
/************************************************************************/

OGRGPSBabelDataSource::OGRGPSBabelDataSource()

{
    nLayers = 0;
    pszName = NULL;
    pszGPSBabelDriverName = NULL;
    pszFilename = NULL;
    poGPXDS = NULL;
}

/************************************************************************/
/*                     ~OGRGPSBabelDataSource()                         */
/************************************************************************/

OGRGPSBabelDataSource::~OGRGPSBabelDataSource()

{
    CPLFree(pszName);
    CPLFree(pszGPSBabelDriverName);
    CPLFree(pszFilename);
    
    if (poGPXDS)
        OGRDataSource::DestroyDataSource(poGPXDS);
    
    if (osTmpFileName.size() > 0)
        VSIUnlink(osTmpFileName.c_str());
}

/************************************************************************/
/*                             GetArgv()                                */
/************************************************************************/

static char** GetArgv(int bExplicitFeatures, int bWaypoints, int bRoutes,
                      int bTracks, const char* pszGPSBabelDriverName,
                      const char* pszFilename)
{
    char** argv = NULL;
    argv = CSLAddString(argv, "gpsbabel");
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

int OGRGPSBabelDataSource::IsSpecialFile(const char* pszFilename)
{
    return (strncmp(pszFilename, "/dev/", 5) == 0 ||
            strncmp(pszFilename, "usb:", 4) == 0 ||
            (strncmp(pszFilename, "COM", 3) == 0  && atoi(pszFilename + 3) > 0));
}

/************************************************************************/
/*                       IsValidDriverName()                            */
/************************************************************************/

int OGRGPSBabelDataSource::IsValidDriverName(const char* pszGPSBabelDriverName)
{
    int i;
    for(i=0;pszGPSBabelDriverName[i] != '\0';i++)
    {
        char ch = pszGPSBabelDriverName[i];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '=' || ch == '.'  || ch == ','))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Invalid GPSBabel driver name");
            return FALSE;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGPSBabelDataSource::Open( const char * pszDatasourceName, int bUpdateIn)

{
    int bExplicitFeatures = FALSE;
    int bWaypoints = TRUE, bTracks = TRUE, bRoutes = TRUE;
    if (bUpdateIn)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "OGR/GPSBabel driver does not support opening a file in update mode");
        return FALSE;
    }

    if (!EQUALN(pszDatasourceName, "GPSBABEL:", 9))
    {
        VSILFILE* fp = VSIFOpenL(pszDatasourceName, "rb");
        if (fp == NULL)
            return FALSE;

        char szHeader[1024 + 1];
        memset(szHeader, 0, 1024+1);
        VSIFReadL(szHeader, 1, 1024, fp);
        if (memcmp(szHeader, "MsRcd", 5) == 0)
            pszGPSBabelDriverName = CPLStrdup("mapsource");
        else if (memcmp(szHeader, "MsRcf", 5) == 0)
            pszGPSBabelDriverName = CPLStrdup("gdb");
        else if (strstr(szHeader, "<osm") != NULL)
            pszGPSBabelDriverName = CPLStrdup("osm");
        else if (strstr(szHeader, "$GPGSA") != NULL ||
                 strstr(szHeader, "$GPGGA") != NULL)
            pszGPSBabelDriverName = CPLStrdup("nmea");
        else if (EQUALN(szHeader, "OziExplorer",11))
            pszGPSBabelDriverName = CPLStrdup("ozi");
        else if (strstr(szHeader, "Grid") && strstr(szHeader, "Datum") && strstr(szHeader, "Header"))
            pszGPSBabelDriverName = CPLStrdup("garmin_txt");
        else if (szHeader[0] == 13 && szHeader[10] == 'M' && szHeader[11] == 'S' &&
                 (szHeader[12] >= '0' && szHeader[12] <= '9') &&
                 (szHeader[13] >= '0' && szHeader[13] <= '9') &&
                 szHeader[12] * 10 + szHeader[13] >= 30 &&
                 (szHeader[14] == 1 || szHeader[14] == 2) && szHeader[15] == 0 &&
                 szHeader[16] == 0 && szHeader[17] == 0)
            pszGPSBabelDriverName = CPLStrdup("mapsend");
        else if (strstr(szHeader, "$PMGNWPL") != NULL ||
                 strstr(szHeader, "$PMGNRTE") != NULL)
            pszGPSBabelDriverName = CPLStrdup("magellan");

        VSIFCloseL(fp);

        if (pszGPSBabelDriverName == NULL)
        {
            return FALSE;
        }

        pszFilename = CPLStrdup(pszDatasourceName);
    }

    pszName = CPLStrdup( pszDatasourceName );

    if (pszGPSBabelDriverName == NULL)
    {
        const char* pszSep = strchr(pszDatasourceName + 9, ':');
        if (pszSep == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Wrong syntax. Expected GPSBabel:driver_name:file_name");
            return FALSE;
        }

        pszGPSBabelDriverName = CPLStrdup(pszDatasourceName + 9);
        *(strchr(pszGPSBabelDriverName, ':')) = '\0';

        /* A bit of validation to avoid command line injection */
        if (!IsValidDriverName(pszGPSBabelDriverName))
            return FALSE;

        /* Parse optionnal features= option */
        if (EQUALN(pszSep+1, "features=", 9))
        {
            const char* pszNextSep = strchr(pszSep+1, ':');
            if (pszNextSep == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Wrong syntax. Expected GPSBabel:driver_name[,options]*:[features=waypoints,tracks,routes:]file_name");
                return FALSE;
            }

            char* pszFeatures = CPLStrdup(pszSep+1+9);
            *strchr(pszFeatures, ':') = 0;
            char** papszTokens = CSLTokenizeString(pszFeatures);
            char** papszIter = papszTokens;
            int bErr = FALSE;
            bExplicitFeatures = TRUE;
            bWaypoints = bTracks = bRoutes = FALSE;
            while(papszIter && *papszIter)
            {
                if (EQUAL(*papszIter, "waypoints"))
                    bWaypoints = TRUE;
                else if (EQUAL(*papszIter, "tracks"))
                    bTracks = TRUE;
                else if (EQUAL(*papszIter, "routes"))
                    bRoutes = TRUE;
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Wrong value for 'features' options");
                    bErr = TRUE;
                }
                papszIter ++;
            }
            CSLDestroy(papszTokens);
            CPLFree(pszFeatures);

            if (bErr)
                return FALSE;

            pszSep = pszNextSep;
        }

        pszFilename = CPLStrdup(pszSep+1);
    }

    const char* pszOptionUseTempFile = CPLGetConfigOption("USE_TEMPFILE", NULL);
    if (pszOptionUseTempFile && CSLTestBoolean(pszOptionUseTempFile))
        osTmpFileName = CPLGenerateTempFilename(NULL);
    else
        osTmpFileName.Printf("/vsimem/ogrgpsbabeldatasource_%p", this);

    int nRet = FALSE;
    if (IsSpecialFile(pszFilename))
    {
        /* Special file : don't try to open it */
        char** argv = GetArgv(bExplicitFeatures, bWaypoints, bRoutes,
                              bTracks, pszGPSBabelDriverName, pszFilename);
        VSILFILE* tmpfp = VSIFOpenL(osTmpFileName.c_str(), "wb");
        nRet = ForkAndPipe(argv, NULL, tmpfp);
        VSIFCloseL(tmpfp);
        tmpfp = NULL;
        CSLDestroy(argv);
        argv = NULL;
    }
    else
    {
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (fp == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot open file %s", pszFilename);
            return FALSE;
        }

        char** argv = GetArgv(bExplicitFeatures, bWaypoints, bRoutes,
                              bTracks, pszGPSBabelDriverName, "-");

        VSILFILE* tmpfp = VSIFOpenL(osTmpFileName.c_str(), "wb");

        CPLPushErrorHandler(CPLQuietErrorHandler);
        nRet = ForkAndPipe(argv, fp, tmpfp);
        CPLPopErrorHandler();

        CSLDestroy(argv);
        argv = NULL;

        CPLErr nLastErrorType = CPLGetLastErrorType();
        int nLastErrorNo = CPLGetLastErrorNo();
        CPLString osLastErrorMsg = CPLGetLastErrorMsg();

        VSIFCloseL(tmpfp);
        tmpfp = NULL;

        VSIFCloseL(fp);
        fp = NULL;

        if (!nRet)
        {
            if (strstr(osLastErrorMsg.c_str(), "This format cannot be used in piped commands") == NULL)
            {
                CPLError(nLastErrorType, nLastErrorNo, "%s", osLastErrorMsg.c_str());
            }
            else
            {
                VSIStatBuf sStatBuf;
                if (VSIStat(pszFilename, &sStatBuf) != 0)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                            "Driver %s only supports real (non virtual) files", pszGPSBabelDriverName);
                    return FALSE;
                }

                /* Try without piping in */
                argv = GetArgv(bExplicitFeatures, bWaypoints, bRoutes,
                              bTracks, pszGPSBabelDriverName, pszFilename);
                tmpfp = VSIFOpenL(osTmpFileName.c_str(), "wb");
                nRet = ForkAndPipe(argv, NULL, tmpfp);
                VSIFCloseL(tmpfp);
                tmpfp = NULL;

                CSLDestroy(argv);
                argv = NULL;
            }
        }
    }


    if (nRet)
    {
        poGPXDS = OGRSFDriverRegistrar::Open(osTmpFileName.c_str());
        if (poGPXDS)
        {
            OGRLayer* poLayer;

            if (bWaypoints)
            {
                poLayer = poGPXDS->GetLayerByName("waypoints");
                if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
            }

            if (bRoutes)
            {
                poLayer = poGPXDS->GetLayerByName("routes");
                if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
                poLayer = poGPXDS->GetLayerByName("route_points");
                if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
            }

            if (bTracks)
            {
                poLayer = poGPXDS->GetLayerByName("tracks");
                if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
                poLayer = poGPXDS->GetLayerByName("track_points");
                if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                    apoLayers[nLayers++] = poLayer;
            }
        }
    }

    return nLayers > 0;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGPSBabelDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGPSBabelDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return apoLayers[iLayer];
}
