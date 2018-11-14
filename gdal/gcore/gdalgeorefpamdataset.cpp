/******************************************************************************
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

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "gdal.h"

//! @cond Doxygen_Suppress
/************************************************************************/
/*                       GDALGeorefPamDataset()                         */
/************************************************************************/

GDALGeorefPamDataset::GDALGeorefPamDataset() :
    bGeoTransformValid(false),
    pszProjection(nullptr),
    nGCPCount(0),
    pasGCPList(nullptr),
    m_papszRPC(nullptr),
    m_bPixelIsPoint(false),
    m_nGeoTransformGeorefSrcIndex(-1),
    m_nGCPGeorefSrcIndex(-1),
    m_nProjectionGeorefSrcIndex(-1),
    m_nRPCGeorefSrcIndex(-1),
    m_nPixelIsPointGeorefSrcIndex(-1),
    m_bGotPAMGeorefSrcIndex(false),
    m_nPAMGeorefSrcIndex(0),
    m_bPAMLoaded(false),
    m_papszMainMD(nullptr)
{
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
    CSLDestroy(m_papszMainMD);
    CSLDestroy(m_papszRPC);
}

/************************************************************************/
/*                          GetMetadata()                               */
/************************************************************************/

char      **GDALGeorefPamDataset::GetMetadata( const char * pszDomain )
{
    if( pszDomain != nullptr && EQUAL(pszDomain, "RPC") )
    {
        const int nPAMIndex = GetPAMGeorefSrcIndex();
        if( nPAMIndex >= 0 &&
            ((m_papszRPC != nullptr && nPAMIndex < m_nRPCGeorefSrcIndex) ||
            m_nRPCGeorefSrcIndex < 0 || m_papszRPC == nullptr))
        {
            char** papszMD = GDALPamDataset::GetMetadata(pszDomain);
            if( papszMD )
                return papszMD;
        }
        return m_papszRPC;
    }

    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
    {
        if( m_papszMainMD )
            return m_papszMainMD;
        m_papszMainMD = CSLDuplicate(GDALPamDataset::GetMetadata(pszDomain));
        const int nPAMIndex = GetPAMGeorefSrcIndex();
        if( nPAMIndex >= 0 &&
            ((m_bPixelIsPoint && nPAMIndex < m_nPixelIsPointGeorefSrcIndex) ||
            m_nPixelIsPointGeorefSrcIndex < 0 || !m_bPixelIsPoint))
        {
            if( CSLFetchNameValue(m_papszMainMD, GDALMD_AREA_OR_POINT) != nullptr )
                return m_papszMainMD;
        }
        if( m_bPixelIsPoint )
        {
            m_papszMainMD = CSLSetNameValue(m_papszMainMD,
                                            GDALMD_AREA_OR_POINT,
                                            GDALMD_AOP_POINT);
        }
        else
        {
            m_papszMainMD = CSLSetNameValue(m_papszMainMD,
                                            GDALMD_AREA_OR_POINT, nullptr);
        }
        return m_papszMainMD;
    }

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char *GDALGeorefPamDataset::GetMetadataItem( const char * pszName,
                                                   const char * pszDomain )
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") || EQUAL(pszDomain, "RPC") )
    {
        return CSLFetchNameValue( GetMetadata(pszDomain), pszName );
    }
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                           TryLoadXML()                              */
/************************************************************************/

CPLErr GDALGeorefPamDataset::TryLoadXML(char **papszSiblingFiles)
{
    m_bPAMLoaded = true;
    CPLErr eErr = GDALPamDataset::TryLoadXML(papszSiblingFiles);
    CSLDestroy(m_papszMainMD);
    m_papszMainMD = nullptr;
    return eErr;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALGeorefPamDataset::SetMetadata( char ** papszMetadata,
                                           const char * pszDomain )
{
    if( m_bPAMLoaded && (pszDomain == nullptr || EQUAL(pszDomain, "")) )
    {
        CSLDestroy(m_papszMainMD);
        m_papszMainMD = CSLDuplicate(papszMetadata);
    }
    return GDALPamDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALGeorefPamDataset::SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain )
{
    if( m_bPAMLoaded && (pszDomain == nullptr || EQUAL(pszDomain, "")) )
    {
        m_papszMainMD = CSLSetNameValue( GetMetadata(), pszName, pszValue );
    }
    return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                            GetGCPCount()                             */
/*                                                                      */
/*      By default, we let PAM override the value stored                */
/*      inside our file, unless GDAL_GEOREF_SOURCES is defined.         */
/************************************************************************/

int GDALGeorefPamDataset::GetGCPCount()

{
    const int nPAMIndex = GetPAMGeorefSrcIndex();
    if( nPAMIndex >= 0 &&
        ((nGCPCount != 0 && nPAMIndex < m_nGCPGeorefSrcIndex) ||
         m_nGCPGeorefSrcIndex < 0 || nGCPCount == 0))
    {
        const int nPAMGCPCount = GDALPamDataset::GetGCPCount();
        if( nPAMGCPCount )
            return nPAMGCPCount;
    }

    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/*                                                                      */
/*      By default, we let PAM override the value stored                */
/*      inside our file, unless GDAL_GEOREF_SOURCES is defined.         */
/************************************************************************/

const char *GDALGeorefPamDataset::_GetGCPProjection()

{
    const int nPAMIndex = GetPAMGeorefSrcIndex();
    if( nPAMIndex >= 0 &&
        ((pszProjection != nullptr && nPAMIndex < m_nProjectionGeorefSrcIndex) ||
         m_nProjectionGeorefSrcIndex < 0 || pszProjection == nullptr) )
    {
        const char* pszPAMGCPProjection = GDALPamDataset::_GetGCPProjection();
        if( pszPAMGCPProjection != nullptr && strlen(pszPAMGCPProjection) > 0 )
            return pszPAMGCPProjection;
    }

    if( pszProjection != nullptr )
        return pszProjection;

    return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/*                                                                      */
/*      By default, we let PAM override the value stored                */
/*      inside our file, unless GDAL_GEOREF_SOURCES is defined.         */
/************************************************************************/

const GDAL_GCP *GDALGeorefPamDataset::GetGCPs()

{
    const int nPAMIndex = GetPAMGeorefSrcIndex();
    if( nPAMIndex >= 0 &&
        ((nGCPCount != 0 && nPAMIndex < m_nGCPGeorefSrcIndex) ||
         m_nGCPGeorefSrcIndex < 0 || nGCPCount == 0))
    {
        const GDAL_GCP* pasPAMGCPList = GDALPamDataset::GetGCPs();
        if( pasPAMGCPList )
            return pasPAMGCPList;
    }

    return pasGCPList;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/*                                                                      */
/*      By default, we let PAM override the value stored                */
/*      inside our file, unless GDAL_GEOREF_SOURCES is defined.         */
/************************************************************************/

const char *GDALGeorefPamDataset::_GetProjectionRef()

{
    if( GetGCPCount() > 0 )
        return "";

    const int nPAMIndex = GetPAMGeorefSrcIndex();
    if( nPAMIndex >= 0 &&
        ((pszProjection != nullptr && nPAMIndex < m_nProjectionGeorefSrcIndex) ||
         m_nProjectionGeorefSrcIndex < 0 || pszProjection == nullptr) )
    {
        const char* pszPAMProjection = GDALPamDataset::_GetProjectionRef();
        if( pszPAMProjection != nullptr && strlen(pszPAMProjection) > 0 )
            return pszPAMProjection;
    }

    if( pszProjection != nullptr )
        return pszProjection;

    return "";
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/*                                                                      */
/*      By default, we let PAM override the value stored                */
/*      inside our file, unless GDAL_GEOREF_SOURCES is defined.         */
/************************************************************************/

CPLErr GDALGeorefPamDataset::GetGeoTransform( double * padfTransform )

{
    const int nPAMIndex = GetPAMGeorefSrcIndex();
    if( nPAMIndex >= 0 &&
        ((bGeoTransformValid && nPAMIndex <= m_nGeoTransformGeorefSrcIndex) ||
          m_nGeoTransformGeorefSrcIndex < 0 || !bGeoTransformValid) )
    {
        if( GDALPamDataset::GetGeoTransform( padfTransform ) == CE_None )
        {
            m_nGeoTransformGeorefSrcIndex = nPAMIndex;
            return CE_None;
        }
    }

    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
        return( CE_None );
    }

    return CE_Failure;
}

/************************************************************************/
/*                     GetPAMGeorefSrcIndex()                           */
/*                                                                      */
/*      Get priority index of PAM (the lower, the more prioritary)      */
/************************************************************************/
int GDALGeorefPamDataset::GetPAMGeorefSrcIndex()
{
    if( !m_bGotPAMGeorefSrcIndex )
    {
        m_bGotPAMGeorefSrcIndex = true;
        const char* pszGeorefSources = CSLFetchNameValueDef( papszOpenOptions,
            "GEOREF_SOURCES",
            CPLGetConfigOption("GDAL_GEOREF_SOURCES", "PAM,OTHER") );
        char** papszTokens = CSLTokenizeString2(pszGeorefSources, ",", 0);
        m_nPAMGeorefSrcIndex = CSLFindString(papszTokens, "PAM");
        CSLDestroy(papszTokens);
    }
    return m_nPAMGeorefSrcIndex;
}
//! @endcond
