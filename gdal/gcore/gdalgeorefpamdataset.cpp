/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  GDALPamDataset with internal storage for georeferencing, with
 *           priority for PAM over internal georeferencing
 * Author:   Even Rouault <even dot rouault at mines-paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdalgeorefpamdataset.h"

/************************************************************************/
/*                       GDALGeorefPamDataset()                         */
/************************************************************************/

GDALGeorefPamDataset::GDALGeorefPamDataset()
{
    pszProjection = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                       ~GDALGeorefPamDataset()                        */
/************************************************************************/

GDALGeorefPamDataset::~GDALGeorefPamDataset()
{
    CPLFree( pszProjection );

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
}


/************************************************************************/
/*                            GetGCPCount()                             */
/*                                                                      */
/*      We let PAM coordinate system override the one stored inside     */
/*      our file.                                                       */
/************************************************************************/

int GDALGeorefPamDataset::GetGCPCount()

{
    int nPamGCPCount = GDALPamDataset::GetGCPCount();
    if( nGCPCount > 0 && nPamGCPCount == 0)
        return nGCPCount;
    else
        return nPamGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/*                                                                      */
/*      We let PAM coordinate system override the one stored inside     */
/*      our file.                                                       */
/************************************************************************/

const char *GDALGeorefPamDataset::GetGCPProjection()

{
    const char* pszPamPrj = GDALPamDataset::GetGCPProjection();
    if( pszProjection != NULL && strlen(pszPamPrj) == 0 )
        return pszProjection;
    else
        return pszPamPrj;
}

/************************************************************************/
/*                               GetGCP()                               */
/*                                                                      */
/*      We let PAM coordinate system override the one stored inside     */
/*      our file.                                                       */
/************************************************************************/

const GDAL_GCP *GDALGeorefPamDataset::GetGCPs()

{
    int nPamGCPCount = GDALPamDataset::GetGCPCount();
    if( nGCPCount > 0 && nPamGCPCount == 0)
        return pasGCPList;
    else
        return GDALPamDataset::GetGCPs();
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/*                                                                      */
/*      We let PAM coordinate system override the one stored inside     */
/*      our file.                                                       */
/************************************************************************/

const char *GDALGeorefPamDataset::GetProjectionRef() 

{
    const char* pszPamPrj = GDALPamDataset::GetProjectionRef();

    if( GetGCPCount() > 0 )
        return "";

    if( pszProjection != NULL && strlen(pszPamPrj) == 0 )
        return pszProjection;
    else
        return pszPamPrj;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/*                                                                      */
/*      Let the PAM geotransform override the native one if it is       */
/*      available.                                                      */
/************************************************************************/

CPLErr GDALGeorefPamDataset::GetGeoTransform( double * padfTransform )

{
    CPLErr eErr = GDALPamDataset::GetGeoTransform( padfTransform );

    if( eErr != CE_None && bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
        return( CE_None );
    }
    else
        return eErr;
}
