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

#include <string.h>
#include "ogr_gpsbabel.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_error.h"

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
/*                                Open()                                */
/************************************************************************/

int OGRGPSBabelDataSource::Open( const char * pszDatasourceName, int bUpdateIn)

{
    if (bUpdateIn)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "OGR/GPSBabel driver does not support opening a file in update mode");
        return FALSE;
    }

    if (!EQUALN(pszDatasourceName, "GPSBABEL:", 9))
    {
        FILE* fp = VSIFOpenL(pszDatasourceName, "rb");
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

        VSIFCloseL(fp);

        if (pszGPSBabelDriverName == NULL)
        {
            return NULL;
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
        int i;
        for(i=0;pszGPSBabelDriverName[i] != '\0';i++)
        {
            char ch = pszGPSBabelDriverName[i];
            if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9') || ch == '_'))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid GPSBabel driver name");
                return FALSE;
            }
        }

        pszFilename = CPLStrdup(pszSep+1);
    }

    osTmpFileName.Printf("/vsimem/ogrgpsbabeldatasource_%p", this);

    int nRet = FALSE;
    if (strncmp(pszFilename, "/dev/", 5) == 0 ||
        strncmp(pszFilename, "usb:", 4) == 0 ||
        (strncmp(pszFilename, "COM", 3) == 0  && atoi(pszFilename + 3) > 0))
    {
        /* Special file : don't try to open it */
        const char* const argv[] = { "gpsbabel", "-i", pszGPSBabelDriverName,
                                    "-f", pszFilename , "-o", "gpx", "-F", "-", NULL };
        FILE* memfp = VSIFOpenL(osTmpFileName.c_str(), "w");
        nRet = ForkAndPipe(argv, NULL, memfp);
        VSIFCloseL(memfp);
        memfp = NULL;
    }
    else
    {
        FILE* fp = VSIFOpenL(pszFilename, "rb");
        if (fp == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot open file %s", pszFilename);
            return FALSE;
        }

        const char* const argv[] = { "gpsbabel", "-i", pszGPSBabelDriverName,
                                    "-f", "-" , "-o", "gpx", "-F", "-", NULL };
        FILE* memfp = VSIFOpenL(osTmpFileName.c_str(), "w");

        CPLPushErrorHandler(CPLQuietErrorHandler);
        nRet = ForkAndPipe(argv, fp, memfp);
        CPLPopErrorHandler();

        CPLErr nLastErrorType = CPLGetLastErrorType();
        int nLastErrorNo = CPLGetLastErrorNo();
        CPLString osLastErrorMsg = CPLGetLastErrorMsg();

        VSIFCloseL(memfp);
        memfp = NULL;

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
                const char* const argv[] = { "gpsbabel", "-i", pszGPSBabelDriverName,
                                            "-f", pszFilename , "-o", "gpx", "-F", "-", NULL };
                memfp = VSIFOpenL(osTmpFileName.c_str(), "w");
                nRet = ForkAndPipe(argv, NULL, memfp);
                VSIFCloseL(memfp);
                memfp = NULL;
            }
        }
    }


    if (nRet)
    {
        poGPXDS = OGRSFDriverRegistrar::Open(osTmpFileName.c_str());
        if (poGPXDS)
        {
            OGRLayer* poLayer;
            poLayer = poGPXDS->GetLayerByName("waypoints");
            if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                apoLayers[nLayers++] = poLayer;
            poLayer = poGPXDS->GetLayerByName("routes");
            if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                apoLayers[nLayers++] = poLayer;
            poLayer = poGPXDS->GetLayerByName("route_points");
            if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                apoLayers[nLayers++] = poLayer;
            poLayer = poGPXDS->GetLayerByName("tracks");
            if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                apoLayers[nLayers++] = poLayer;
            poLayer = poGPXDS->GetLayerByName("track_points");
            if (poLayer != NULL && poLayer->GetFeatureCount() != 0)
                apoLayers[nLayers++] = poLayer;
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
