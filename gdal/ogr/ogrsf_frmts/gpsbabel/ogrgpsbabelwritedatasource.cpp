/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGPSBabelWriteDataSource class.
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
#include "cpl_spawn.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                    OGRGPSBabelWriteDataSource()                      */
/************************************************************************/

OGRGPSBabelWriteDataSource::OGRGPSBabelWriteDataSource()

{
    pszName = NULL;
    pszGPSBabelDriverName = NULL;
    pszFilename = NULL;
    poGPXDS = NULL;
}

/************************************************************************/
/*                  ~OGRGPSBabelWriteDataSource()                       */
/************************************************************************/

OGRGPSBabelWriteDataSource::~OGRGPSBabelWriteDataSource()

{
    if (poGPXDS)
        OGRDataSource::DestroyDataSource(poGPXDS);

    Convert();

    CPLFree(pszName);
    CPLFree(pszGPSBabelDriverName);
    CPLFree(pszFilename);
}

/************************************************************************/
/*                                Convert()                             */
/************************************************************************/

int OGRGPSBabelWriteDataSource::Convert()
{
    int nRet = -1;
    if (osTmpFileName.size() > 0 && pszFilename != NULL && pszGPSBabelDriverName != NULL)
    {
        if (OGRGPSBabelDataSource::IsSpecialFile(pszFilename))
        {
            /* Special file : don't try to open it */
            const char* const argv[] = { "gpsbabel", "-i", "gpx", "-f", "-",
                                          "-o", pszGPSBabelDriverName, "-F", pszFilename, NULL };
            VSILFILE* tmpfp = VSIFOpenL(osTmpFileName.c_str(), "rb");
            if (tmpfp)
            {
                nRet = CPLSpawn(argv, tmpfp, NULL, TRUE);

                VSIFCloseL(tmpfp);
                tmpfp = NULL;
            }
        }
        else
        {
            VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
            if (fp == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot open file %s", pszFilename);
            }
            else
            {
                const char* const argv[] = { "gpsbabel", "-i", "gpx", "-f", "-",
                                            "-o", pszGPSBabelDriverName, "-F", "-", NULL };
                VSILFILE* tmpfp = VSIFOpenL(osTmpFileName.c_str(), "rb");
                if (tmpfp)
                {
                    nRet = CPLSpawn(argv, tmpfp, fp, TRUE);

                    VSIFCloseL(tmpfp);
                    tmpfp = NULL;
                }

                VSIFCloseL(fp);
                fp = NULL;
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

int OGRGPSBabelWriteDataSource::Create( const char * pszName,
                                        char **papszOptions )
{
    OGRSFDriver* poGPXDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("GPX");
    if (poGPXDriver == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GPX driver is necessary for GPSBabel write support");
        return FALSE;
    }

    if (!EQUALN(pszName, "GPSBABEL:", 9))
    {
        const char* pszOptionGPSBabelDriverName =
                CSLFetchNameValue(papszOptions, "GPSBABEL_DRIVER");
        if (pszOptionGPSBabelDriverName != NULL)
            pszGPSBabelDriverName = CPLStrdup(pszOptionGPSBabelDriverName);
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "GPSBABEL_DRIVER dataset creation option expected");
            return FALSE;
        }

        pszFilename = CPLStrdup(pszName);
    }
    else
    {
        const char* pszSep = strchr(pszName + 9, ':');
        if (pszSep == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Wrong syntax. Expected GPSBabel:driver_name[,options]*:file_name");
            return FALSE;
        }

        pszGPSBabelDriverName = CPLStrdup(pszName + 9);
        *(strchr(pszGPSBabelDriverName, ':')) = '\0';

        pszFilename = CPLStrdup(pszSep+1);
    }

    /* A bit of validation to avoid command line injection */
    if (!OGRGPSBabelDataSource::IsValidDriverName(pszGPSBabelDriverName))
        return FALSE;

    const char* pszOptionUseTempFile = CSLFetchNameValue(papszOptions, "USE_TEMPFILE");
    if (pszOptionUseTempFile == NULL)
        pszOptionUseTempFile = CPLGetConfigOption("USE_TEMPFILE", NULL);
    if (pszOptionUseTempFile && CSLTestBoolean(pszOptionUseTempFile))
        osTmpFileName = CPLGenerateTempFilename(NULL);
    else
        osTmpFileName.Printf("/vsimem/ogrgpsbabeldatasource_%p", this);

    poGPXDS = poGPXDriver->CreateDataSource(osTmpFileName.c_str(), papszOptions);
    if (poGPXDS == NULL)
        return FALSE;

    this->pszName = CPLStrdup(pszName);

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer * OGRGPSBabelWriteDataSource::CreateLayer( const char * pszLayerName,
                                                    OGRSpatialReference *poSRS,
                                                    OGRwkbGeometryType eType,
                                                    char ** papszOptions )
{
    if (poGPXDS)
        return poGPXDS->CreateLayer(pszLayerName, poSRS, eType, papszOptions);
    return NULL;
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
    if (poGPXDS)
        return poGPXDS->GetLayer(iLayer);
    return NULL;
}

/************************************************************************/
/*                         GetLayerCount()                              */
/************************************************************************/

int OGRGPSBabelWriteDataSource::GetLayerCount()

{
    if (poGPXDS)
        return poGPXDS->GetLayerCount();
    return 0;
}
