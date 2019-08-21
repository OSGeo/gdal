/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGPSBabelWriteDataSource class.
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

#include <cstring>
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_error.h"
#include "cpl_spawn.h"

#include "ogr_gpsbabel.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                    OGRGPSBabelWriteDataSource()                      */
/************************************************************************/

OGRGPSBabelWriteDataSource::OGRGPSBabelWriteDataSource() :
    pszName(nullptr),
    pszGPSBabelDriverName(nullptr),
    pszFilename(nullptr),
    poGPXDS(nullptr)
{}

/************************************************************************/
/*                  ~OGRGPSBabelWriteDataSource()                       */
/************************************************************************/

OGRGPSBabelWriteDataSource::~OGRGPSBabelWriteDataSource()

{
    if (poGPXDS)
        GDALClose( poGPXDS );

    Convert();

    CPLFree(pszName);
    CPLFree(pszGPSBabelDriverName);
    CPLFree(pszFilename);
}

/************************************************************************/
/*                                Convert()                             */
/************************************************************************/

bool OGRGPSBabelWriteDataSource::Convert()
{
    int nRet = -1;
    if( !osTmpFileName.empty() && pszFilename != nullptr &&
         pszGPSBabelDriverName != nullptr )
    {
        if (OGRGPSBabelDataSource::IsSpecialFile(pszFilename))
        {
            /* Special file : don't try to open it */
            VSILFILE* tmpfp = VSIFOpenL(osTmpFileName.c_str(), "rb");
            if (tmpfp)
            {
                const char* const argv[] = {
                    "gpsbabel", "-i", "gpx", "-f", "-",
                    "-o", pszGPSBabelDriverName, "-F", pszFilename, nullptr };
                nRet = CPLSpawn(argv, tmpfp, nullptr, TRUE);

                VSIFCloseL(tmpfp);
                tmpfp = nullptr;
            }
        }
        else
        {
            VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
            if (fp == nullptr)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Cannot open file %s", pszFilename );
            }
            else
            {
                VSILFILE* tmpfp = VSIFOpenL(osTmpFileName.c_str(), "rb");
                if (tmpfp)
                {
                    const char* const argv[] = {
                        "gpsbabel", "-i", "gpx", "-f", "-",
                        "-o", pszGPSBabelDriverName, "-F", "-", nullptr };
                    nRet = CPLSpawn(argv, tmpfp, fp, TRUE);

                    VSIFCloseL(tmpfp);
                    tmpfp = nullptr;
                }

                VSIFCloseL(fp);
                fp = nullptr;
            }
        }

        VSIUnlink(osTmpFileName.c_str());
        osTmpFileName = "";
    }

    return nRet == 0;
}

/************************************************************************/
/*                                 Create()                             */
/************************************************************************/

int OGRGPSBabelWriteDataSource::Create( const char * pszNameIn,
                                        char **papszOptions )
{
    GDALDriver* poGPXDriver
        = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("GPX");
    if (poGPXDriver == nullptr)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GPX driver is necessary for GPSBabel write support" );
        return FALSE;
    }

    if (!STARTS_WITH_CI(pszNameIn, "GPSBABEL:"))
    {
        const char* pszOptionGPSBabelDriverName =
                CSLFetchNameValue(papszOptions, "GPSBABEL_DRIVER");
        if (pszOptionGPSBabelDriverName != nullptr)
            pszGPSBabelDriverName = CPLStrdup(pszOptionGPSBabelDriverName);
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "GPSBABEL_DRIVER dataset creation option expected" );
            return FALSE;
        }

        pszFilename = CPLStrdup(pszNameIn);
    }
    else
    {
        const char* pszSep = strchr(pszNameIn + 9, ':');
        if (pszSep == nullptr)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Wrong syntax. Expected GPSBabel:driver_name[,options]*:"
                      "file_name" );
            return FALSE;
        }

        pszGPSBabelDriverName = CPLStrdup(pszNameIn + 9);
        *(strchr(pszGPSBabelDriverName, ':')) = '\0';

        pszFilename = CPLStrdup(pszSep+1);
    }

    /* A bit of validation to avoid command line injection */
    if (!OGRGPSBabelDataSource::IsValidDriverName(pszGPSBabelDriverName))
        return FALSE;

    const char* pszOptionUseTempFile
        = CSLFetchNameValue(papszOptions, "USE_TEMPFILE");
    if (pszOptionUseTempFile == nullptr)
        pszOptionUseTempFile = CPLGetConfigOption("USE_TEMPFILE", nullptr);
    if (pszOptionUseTempFile && CPLTestBool(pszOptionUseTempFile))
        osTmpFileName = CPLGenerateTempFilename(nullptr);
    else
        osTmpFileName.Printf("/vsimem/ogrgpsbabeldatasource_%p", this);

    poGPXDS = poGPXDriver->Create( osTmpFileName.c_str(), 0, 0, 0,
                                   GDT_Unknown, papszOptions);
    if (poGPXDS == nullptr)
        return FALSE;

    pszName = CPLStrdup(pszNameIn);

    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRGPSBabelWriteDataSource::ICreateLayer( const char * pszLayerName,
                                                    OGRSpatialReference *poSRS,
                                                    OGRwkbGeometryType eType,
                                                    char ** papszOptions )
{
    if( poGPXDS )
        return poGPXDS->CreateLayer(pszLayerName, poSRS, eType, papszOptions);
    return nullptr;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGPSBabelWriteDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGPSBabelWriteDataSource::GetLayer( int iLayer )

{
    if( poGPXDS )
        return poGPXDS->GetLayer(iLayer);

    return nullptr;
}

/************************************************************************/
/*                         GetLayerCount()                              */
/************************************************************************/

int OGRGPSBabelWriteDataSource::GetLayerCount()

{
    if( poGPXDS )
        return poGPXDS->GetLayerCount();

    return 0;
}
