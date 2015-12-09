/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTSimpleSource, VRTFuncSource and 
 *           VRTAveragedSource.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "vrtdataset.h"
#include "gdal_proxy.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

/* See #5459 */
#ifdef isnan
#define HAS_ISNAN_MACRO
#endif
#include <algorithm>
#if defined(HAS_ISNAN_MACRO) && !defined(isnan)
#define isnan std::isnan
#endif

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                             VRTSource                                */
/* ==================================================================== */
/************************************************************************/

VRTSource::~VRTSource()
{
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTSource::GetFileList(CPL_UNUSED char*** ppapszFileList,
                            CPL_UNUSED int *pnSize,
                            CPL_UNUSED int *pnMaxSize,
                            CPL_UNUSED CPLHashSet* hSetFiles)
{
}

/************************************************************************/
/* ==================================================================== */
/*                          VRTSimpleSource                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          VRTSimpleSource()                           */
/************************************************************************/

VRTSimpleSource::VRTSimpleSource() :
    m_dfSrcXOff(0.0),
    m_dfSrcYOff(0.0),
    m_dfSrcXSize(0.0),
    m_dfSrcYSize(0.0),
    m_dfDstXOff(0.0),
    m_dfDstYOff(0.0),
    m_dfDstXSize(0.0),
    m_dfDstYSize(0.0)
{
    m_poRasterBand = NULL;
    m_poMaskBandMainBand = NULL;
    m_bNoDataSet = FALSE;
    m_dfNoDataValue = VRT_NODATA_UNSET;
    m_bRelativeToVRTOri = -1;
    m_nMaxValue = 0;
}

/************************************************************************/
/*                          VRTSimpleSource()                           */
/************************************************************************/

VRTSimpleSource::VRTSimpleSource(const VRTSimpleSource* poSrcSource,
                                 double dfXDstRatio, double dfYDstRatio)
{
    m_poRasterBand = poSrcSource->m_poRasterBand;
    m_poMaskBandMainBand = poSrcSource->m_poMaskBandMainBand;
    m_bNoDataSet = poSrcSource->m_bNoDataSet;
    m_dfNoDataValue = poSrcSource->m_dfNoDataValue;
    m_dfSrcXOff = poSrcSource->m_dfSrcXOff;
    m_dfSrcYOff = poSrcSource->m_dfSrcYOff;
    m_dfSrcXSize = poSrcSource->m_dfSrcXSize;
    m_dfSrcYSize = poSrcSource->m_dfSrcYSize;
    m_dfDstXOff = poSrcSource->m_dfDstXOff * dfXDstRatio;
    m_dfDstYOff = poSrcSource->m_dfDstYOff * dfYDstRatio;
    m_dfDstXSize = poSrcSource->m_dfDstXSize * dfXDstRatio;
    m_dfDstYSize = poSrcSource->m_dfDstYSize * dfYDstRatio;
    m_bRelativeToVRTOri = -1;
    m_nMaxValue = poSrcSource->m_nMaxValue;
}

/************************************************************************/
/*                          ~VRTSimpleSource()                          */
/************************************************************************/

VRTSimpleSource::~VRTSimpleSource()

{
    // We use bRelativeToVRTOri to know if the file has been opened from
    // XMLInit(), and thus we are sure that no other code has a direct reference
    // to the dataset
    if( m_poMaskBandMainBand != NULL )
    {
        if (m_poMaskBandMainBand->GetDataset() != NULL )
        {
            if( m_poMaskBandMainBand->GetDataset()->GetShared() || m_bRelativeToVRTOri >= 0 )
                GDALClose( (GDALDatasetH) m_poMaskBandMainBand->GetDataset() );
            else
                m_poMaskBandMainBand->GetDataset()->Dereference();
        }
    }
    else if( m_poRasterBand != NULL && m_poRasterBand->GetDataset() != NULL )
    {
        if( m_poRasterBand->GetDataset()->GetShared() || m_bRelativeToVRTOri >= 0 )
            GDALClose( (GDALDatasetH) m_poRasterBand->GetDataset() );
        else
            m_poRasterBand->GetDataset()->Dereference();
    }
}

/************************************************************************/
/*                    UnsetPreservedRelativeFilenames()                 */
/************************************************************************/

void VRTSimpleSource::UnsetPreservedRelativeFilenames()
{
    m_bRelativeToVRTOri = -1;
    m_osSourceFileNameOri = "";
}

/************************************************************************/
/*                             SetSrcBand()                             */
/************************************************************************/

void VRTSimpleSource::SetSrcBand( GDALRasterBand *poNewSrcBand )

{
    m_poRasterBand = poNewSrcBand;
}


/************************************************************************/
/*                          SetSrcMaskBand()                            */
/************************************************************************/

/* poSrcBand is not the mask band, but the band from which the mask band is taken */
void VRTSimpleSource::SetSrcMaskBand( GDALRasterBand *poNewSrcBand )

{
    m_poRasterBand = poNewSrcBand->GetMaskBand();
    m_poMaskBandMainBand = poNewSrcBand;
}

/************************************************************************/
/*                            SetSrcWindow()                            */
/************************************************************************/

void VRTSimpleSource::SetSrcWindow( double dfNewXOff, double dfNewYOff, 
                                    double dfNewXSize, double dfNewYSize )

{
    m_dfSrcXOff = dfNewXOff;
    m_dfSrcYOff = dfNewYOff;
    m_dfSrcXSize = dfNewXSize;
    m_dfSrcYSize = dfNewYSize;
}

/************************************************************************/
/*                            SetDstWindow()                            */
/************************************************************************/

void VRTSimpleSource::SetDstWindow( double dfNewXOff, double dfNewYOff, 
                                    double dfNewXSize, double dfNewYSize )

{
    m_dfDstXOff = dfNewXOff;
    m_dfDstYOff = dfNewYOff;
    m_dfDstXSize = dfNewXSize;
    m_dfDstYSize = dfNewYSize;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

void VRTSimpleSource::SetNoDataValue( double dfNewNoDataValue )

{
    if( dfNewNoDataValue == VRT_NODATA_UNSET )
    {
        m_bNoDataSet = FALSE;
        m_dfNoDataValue = VRT_NODATA_UNSET;
    }
    else
    {
        m_bNoDataSet = TRUE;
        m_dfNoDataValue = dfNewNoDataValue;
    }
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

static const char* const apszSpecialSyntax[] = { "HDF5:\"{FILENAME}\":{ANY}",
                                            "HDF5:{FILENAME}:{ANY}",
                                            "NETCDF:\"{FILENAME}\":{ANY}",
                                            "NETCDF:{FILENAME}:{ANY}",
                                            "NITF_IM:{ANY}:{FILENAME}",
                                            "PDF:{ANY}:{FILENAME}",
                                            "RASTERLITE:{FILENAME},{ANY}" };

CPLXMLNode *VRTSimpleSource::SerializeToXML( const char *pszVRTPath )

{
    CPLXMLNode      *psSrc;
    int              bRelativeToVRT;
    const char      *pszRelativePath;
    int              nBlockXSize, nBlockYSize;

    if( m_poRasterBand == NULL )
        return NULL;

    GDALDataset     *poDS;

    if (m_poMaskBandMainBand)
    {
        poDS = m_poMaskBandMainBand->GetDataset();
        if( poDS == NULL || m_poMaskBandMainBand->GetBand() < 1 )
            return NULL;
    }
    else
    {
        poDS = m_poRasterBand->GetDataset();
        if( poDS == NULL || m_poRasterBand->GetBand() < 1 )
            return NULL;
    }

    psSrc = CPLCreateXMLNode( NULL, CXT_Element, "SimpleSource" );

    if( m_osResampling.size() )
    {
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psSrc, CXT_Attribute, "resampling" ), 
                CXT_Text, m_osResampling.c_str() );
    }

    VSIStatBufL sStat;
    CPLString osTmp;
    if( m_bRelativeToVRTOri >= 0 )
    {
        pszRelativePath = m_osSourceFileNameOri;
        bRelativeToVRT = m_bRelativeToVRTOri;
    }
    else if ( strstr(poDS->GetDescription(), "/vsicurl/http") != NULL ||
         strstr(poDS->GetDescription(), "/vsicurl/ftp") != NULL )
    {
        /* Testing the existence of remote resources can be excruciating */
        /* slow, so let's just suppose they exist */
        pszRelativePath = poDS->GetDescription();
        bRelativeToVRT = FALSE;
    }
    /* If this isn't actually a file, don't even try to know if it is */
    /* a relative path. It can't be !, and unfortunately */
    /* CPLIsFilenameRelative() can only work with strings that are filenames */
    /* To be clear NITF_TOC_ENTRY:CADRG_JOG-A_250K_1_0:some_path isn't a relative */
    /* file path */
    else if( VSIStatExL( poDS->GetDescription(), &sStat, VSI_STAT_EXISTS_FLAG ) != 0 )
    {
        pszRelativePath = poDS->GetDescription();
        bRelativeToVRT = FALSE;
        for( size_t i = 0; i < sizeof(apszSpecialSyntax) / sizeof(apszSpecialSyntax[0]); i ++)
        {
            const char* pszSyntax = apszSpecialSyntax[i];
            CPLString osPrefix(pszSyntax);
            osPrefix.resize(strchr(pszSyntax, ':') - pszSyntax + 1);
            if( pszSyntax[osPrefix.size()] == '"' )
                osPrefix += '"';
            if( EQUALN(pszRelativePath, osPrefix, osPrefix.size()) )
            {
                if( STARTS_WITH_CI(pszSyntax + osPrefix.size(), "{ANY}") )
                {
                    const char* pszLastPart = strrchr(pszRelativePath, ':') + 1;
                    /* CSV:z:/foo.xyz */
                    if( (pszLastPart[0] == '/' || pszLastPart[0] == '\\') &&
                        pszLastPart - pszRelativePath >= 3 && pszLastPart[-3] == ':' )
                        pszLastPart -= 2;
                    CPLString osPrefixFilename = pszRelativePath;
                    osPrefixFilename.resize(pszLastPart - pszRelativePath);
                    pszRelativePath =
                        CPLExtractRelativePath( pszVRTPath, pszLastPart,
                                    &bRelativeToVRT );
                    osTmp = osPrefixFilename + pszRelativePath;
                    pszRelativePath = osTmp.c_str();
                }
                else if( STARTS_WITH_CI(pszSyntax + osPrefix.size(), "{FILENAME}") )
                {
                    CPLString osFilename(pszRelativePath + osPrefix.size());
                    size_t nPos = 0;
                    if( osFilename.size() >= 3 && osFilename[1] == ':' &&
                        (osFilename[2] == '\\' || osFilename[2] == '/') )
                        nPos = 2;
                    nPos = osFilename.find(pszSyntax[osPrefix.size() + strlen("{FILENAME}")], nPos);
                    if( nPos != std::string::npos )
                    {
                        CPLString osSuffix = osFilename.substr(nPos);
                        osFilename.resize(nPos);
                        pszRelativePath =
                            CPLExtractRelativePath( pszVRTPath, osFilename,
                                        &bRelativeToVRT );
                        osTmp = osPrefix + pszRelativePath + osSuffix;
                        pszRelativePath = osTmp.c_str();
                    }
                }
                break;
            }
        }
    }
    else
    {
        pszRelativePath =
            CPLExtractRelativePath( pszVRTPath, poDS->GetDescription(),
                                    &bRelativeToVRT );
    }
    
    CPLSetXMLValue( psSrc, "SourceFilename", pszRelativePath );
    
    CPLCreateXMLNode( 
        CPLCreateXMLNode( CPLGetXMLNode( psSrc, "SourceFilename" ), 
                          CXT_Attribute, "relativeToVRT" ), 
        CXT_Text, bRelativeToVRT ? "1" : "0" );
    
    if( !CSLTestBoolean(CPLGetConfigOption("VRT_SHARED_SOURCE", "TRUE")) )
    {
        CPLCreateXMLNode( 
            CPLCreateXMLNode( CPLGetXMLNode( psSrc, "SourceFilename" ), 
                              CXT_Attribute, "shared" ), 
                              CXT_Text, "0" );
    }

    char** papszOpenOptions = poDS->GetOpenOptions();
    GDALSerializeOpenOptionsToXML(psSrc, papszOpenOptions);

    if (m_poMaskBandMainBand)
        CPLSetXMLValue( psSrc, "SourceBand",
                        CPLSPrintf("mask,%d",m_poMaskBandMainBand->GetBand()) );
    else
        CPLSetXMLValue( psSrc, "SourceBand",
                        CPLSPrintf("%d",m_poRasterBand->GetBand()) );

    /* Write a few additional useful properties of the dataset */
    /* so that we can use a proxy dataset when re-opening. See XMLInit() */
    /* below */
    CPLSetXMLValue( psSrc, "SourceProperties.#RasterXSize", 
                    CPLSPrintf("%d",m_poRasterBand->GetXSize()) );
    CPLSetXMLValue( psSrc, "SourceProperties.#RasterYSize", 
                    CPLSPrintf("%d",m_poRasterBand->GetYSize()) );
    CPLSetXMLValue( psSrc, "SourceProperties.#DataType", 
                GDALGetDataTypeName( m_poRasterBand->GetRasterDataType() ) );
    m_poRasterBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    CPLSetXMLValue( psSrc, "SourceProperties.#BlockXSize", 
                    CPLSPrintf("%d",nBlockXSize) );
    CPLSetXMLValue( psSrc, "SourceProperties.#BlockYSize", 
                    CPLSPrintf("%d",nBlockYSize) );

    if( m_dfSrcXOff != -1 
        || m_dfSrcYOff != -1 
        || m_dfSrcXSize != -1 
        || m_dfSrcYSize != -1 )
    {
        CPLSetXMLValue( psSrc, "SrcRect.#xOff", 
                        CPLSPrintf( "%g", m_dfSrcXOff ) );
        CPLSetXMLValue( psSrc, "SrcRect.#yOff", 
                        CPLSPrintf( "%g", m_dfSrcYOff ) );
        CPLSetXMLValue( psSrc, "SrcRect.#xSize", 
                        CPLSPrintf( "%g", m_dfSrcXSize ) );
        CPLSetXMLValue( psSrc, "SrcRect.#ySize", 
                        CPLSPrintf( "%g", m_dfSrcYSize ) );
    }

    if( m_dfDstXOff != -1 
        || m_dfDstYOff != -1 
        || m_dfDstXSize != -1 
        || m_dfDstYSize != -1 )
    {
        CPLSetXMLValue( psSrc, "DstRect.#xOff", CPLSPrintf( "%g", m_dfDstXOff ) );
        CPLSetXMLValue( psSrc, "DstRect.#yOff", CPLSPrintf( "%g", m_dfDstYOff ) );
        CPLSetXMLValue( psSrc, "DstRect.#xSize",CPLSPrintf( "%g", m_dfDstXSize ));
        CPLSetXMLValue( psSrc, "DstRect.#ySize",CPLSPrintf( "%g", m_dfDstYSize ));
    }

    return psSrc;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTSimpleSource::XMLInit( CPLXMLNode *psSrc, const char *pszVRTPath )

{
    m_osResampling = CPLGetXMLValue( psSrc, "resampling", "");

/* -------------------------------------------------------------------- */
/*      Prepare filename.                                               */
/* -------------------------------------------------------------------- */
    char *pszSrcDSName = NULL;
    CPLXMLNode* psSourceFileNameNode = CPLGetXMLNode(psSrc,"SourceFilename");
    const char *pszFilename = 
        psSourceFileNameNode ? CPLGetXMLValue(psSourceFileNameNode,NULL, NULL) : NULL;

    if( pszFilename == NULL )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Missing <SourceFilename> element in VRTRasterBand." );
        return CE_Failure;
    }
    
    // Backup original filename and relativeToVRT so as to be able to
    // serialize them identically again (#5985)
    m_osSourceFileNameOri = pszFilename;
    m_bRelativeToVRTOri = atoi(CPLGetXMLValue( psSourceFileNameNode, "relativetoVRT", "0"));
    const char* pszShared = CPLGetXMLValue( psSourceFileNameNode, "shared", NULL);
    int bShared = FALSE;
    if( pszShared != NULL )
        bShared = CSLTestBoolean(pszShared);
    else
        bShared = CSLTestBoolean(CPLGetConfigOption("VRT_SHARED_SOURCE", "TRUE"));

    if( pszVRTPath != NULL
        && m_bRelativeToVRTOri )
    {
        int bDone = FALSE;
        for( size_t i = 0; i < sizeof(apszSpecialSyntax) / sizeof(apszSpecialSyntax[0]); i ++)
        {
            const char* pszSyntax = apszSpecialSyntax[i];
            CPLString osPrefix(pszSyntax);
            osPrefix.resize(strchr(pszSyntax, ':') - pszSyntax + 1);
            if( pszSyntax[osPrefix.size()] == '"' )
                osPrefix += '"';
            if( EQUALN(pszFilename, osPrefix, osPrefix.size()) )
            {
                if( STARTS_WITH_CI(pszSyntax + osPrefix.size(), "{ANY}") )
                {
                    const char* pszLastPart = strrchr(pszFilename, ':') + 1;
                    /* CSV:z:/foo.xyz */
                    if( (pszLastPart[0] == '/' || pszLastPart[0] == '\\') &&
                        pszLastPart - pszFilename >= 3 && pszLastPart[-3] == ':' )
                        pszLastPart -= 2;
                    CPLString osPrefixFilename = pszFilename;
                    osPrefixFilename.resize(pszLastPart - pszFilename);
                    pszSrcDSName = CPLStrdup( (osPrefixFilename +
                        CPLProjectRelativeFilename( pszVRTPath, pszLastPart )).c_str() );
                    bDone = TRUE;
                }
                else if( STARTS_WITH_CI(pszSyntax + osPrefix.size(), "{FILENAME}") )
                {
                    CPLString osFilename(pszFilename + osPrefix.size());
                    size_t nPos = 0;
                    if( osFilename.size() >= 3 && osFilename[1] == ':' &&
                        (osFilename[2] == '\\' || osFilename[2] == '/') )
                        nPos = 2;
                    nPos = osFilename.find(pszSyntax[osPrefix.size() + strlen("{FILENAME}")], nPos);
                    if( nPos != std::string::npos )
                    {
                        CPLString osSuffix = osFilename.substr(nPos);
                        osFilename.resize(nPos);
                        pszSrcDSName = CPLStrdup( (osPrefix +
                            CPLProjectRelativeFilename( pszVRTPath, osFilename ) + osSuffix).c_str() );
                        bDone = TRUE;
                    }
                }
                break;
            }
        }
        if( !bDone )
        {
            pszSrcDSName = CPLStrdup(
                CPLProjectRelativeFilename( pszVRTPath, pszFilename ) );
        }
    }
    else
        pszSrcDSName = CPLStrdup( pszFilename );

    const char* pszSourceBand = CPLGetXMLValue(psSrc,"SourceBand","1");
    int nSrcBand = 0;
    int bGetMaskBand = FALSE;
    if (STARTS_WITH_CI(pszSourceBand, "mask"))
    {
        bGetMaskBand = TRUE;
        if (pszSourceBand[4] == ',')
            nSrcBand = atoi(pszSourceBand + 5);
        else
            nSrcBand = 1;
    }
    else
        nSrcBand = atoi(pszSourceBand);
    if (!GDALCheckBandCount(nSrcBand, 0))
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Invalid <SourceBand> element in VRTRasterBand." );
        CPLFree( pszSrcDSName );
        return CE_Failure;
    }

    /* Newly generated VRT will have RasterXSize, RasterYSize, DataType, */
    /* BlockXSize, BlockYSize tags, so that we don't have actually to */
    /* open the real dataset immediately, but we can use a proxy dataset */
    /* instead. This is particularly useful when dealing with huge VRT */
    /* For example, a VRT with the world coverage of DTED0 (25594 files) */
    CPLXMLNode* psSrcProperties = CPLGetXMLNode(psSrc,"SourceProperties");
    int nRasterXSize = 0, nRasterYSize =0;
    GDALDataType eDataType = (GDALDataType)-1;
    int nBlockXSize = 0, nBlockYSize = 0;
    if (psSrcProperties)
    {
        nRasterXSize = atoi(CPLGetXMLValue(psSrcProperties,"RasterXSize","0"));
        nRasterYSize = atoi(CPLGetXMLValue(psSrcProperties,"RasterYSize","0"));
        const char *pszDataType = CPLGetXMLValue(psSrcProperties, "DataType", NULL);
        if( pszDataType != NULL )
        {
            for( int iType = 0; iType < GDT_TypeCount; iType++ )
            {
                const char *pszThisName = GDALGetDataTypeName((GDALDataType)iType);

                if( pszThisName != NULL && EQUAL(pszDataType,pszThisName) )
                {
                    eDataType = (GDALDataType) iType;
                    break;
                }
            }
        }
        nBlockXSize = atoi(CPLGetXMLValue(psSrcProperties,"BlockXSize","0"));
        nBlockYSize = atoi(CPLGetXMLValue(psSrcProperties,"BlockYSize","0"));
    }

    char** papszOpenOptions = GDALDeserializeOpenOptionsFromXML(psSrc);
    if( strstr(pszSrcDSName,"<VRTDataset") != NULL )
        papszOpenOptions = CSLSetNameValue(papszOpenOptions, "ROOT_PATH", pszVRTPath);

    GDALDataset *poSrcDS;
    if (nRasterXSize == 0 || nRasterYSize == 0 || eDataType == (GDALDataType)-1 ||
        nBlockXSize == 0 || nBlockYSize == 0)
    {
        /* -------------------------------------------------------------------- */
        /*      Open the file (shared).                                         */
        /* -------------------------------------------------------------------- */
        int nOpenFlags = GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR;
        if( bShared )
            nOpenFlags |= GDAL_OF_SHARED;
        poSrcDS = (GDALDataset *) GDALOpenEx(
                    pszSrcDSName, nOpenFlags, NULL,
                    (const char* const* )papszOpenOptions, NULL );
    }
    else
    {
        /* -------------------------------------------------------------------- */
        /*      Create a proxy dataset                                          */
        /* -------------------------------------------------------------------- */
        int i;
        GDALProxyPoolDataset* proxyDS = new GDALProxyPoolDataset(pszSrcDSName, nRasterXSize, nRasterYSize, GA_ReadOnly, bShared);
        proxyDS->SetOpenOptions(papszOpenOptions);
        poSrcDS = proxyDS;

        /* Only the information of rasterBand nSrcBand will be accurate */
        /* but that's OK since we only use that band afterwards */
        for(i=1;i<=nSrcBand;i++)
            proxyDS->AddSrcBandDescription(eDataType, nBlockXSize, nBlockYSize);
        if (bGetMaskBand)
            ((GDALProxyPoolRasterBand*)proxyDS->GetRasterBand(nSrcBand))->AddSrcMaskBandDescription(eDataType, nBlockXSize, nBlockYSize);
    }
    
    CSLDestroy(papszOpenOptions);

    CPLFree( pszSrcDSName );
    
    if( poSrcDS == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Get the raster band.                                            */
/* -------------------------------------------------------------------- */
    
    m_poRasterBand = poSrcDS->GetRasterBand(nSrcBand);
    if( m_poRasterBand == NULL )
    {
        if( poSrcDS->GetShared() )
            GDALClose( (GDALDatasetH) poSrcDS );
        return CE_Failure;
    }
    if (bGetMaskBand)
    {
        m_poMaskBandMainBand = m_poRasterBand;
        m_poRasterBand = m_poRasterBand->GetMaskBand();
        if( m_poRasterBand == NULL )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Set characteristics.                                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode* psSrcRect = CPLGetXMLNode(psSrc,"SrcRect");
    if (psSrcRect)
    {
        m_dfSrcXOff = CPLAtof(CPLGetXMLValue(psSrcRect,"xOff","-1"));
        m_dfSrcYOff = CPLAtof(CPLGetXMLValue(psSrcRect,"yOff","-1"));
        m_dfSrcXSize = CPLAtof(CPLGetXMLValue(psSrcRect,"xSize","-1"));
        m_dfSrcYSize = CPLAtof(CPLGetXMLValue(psSrcRect,"ySize","-1"));
    }
    else
    {
        m_dfSrcXOff = m_dfSrcYOff = m_dfSrcXSize = m_dfSrcYSize = -1;
    }

    CPLXMLNode* psDstRect = CPLGetXMLNode(psSrc,"DstRect");
    if (psDstRect)
    {
        m_dfDstXOff = CPLAtof(CPLGetXMLValue(psDstRect,"xOff","-1"));
        m_dfDstYOff = CPLAtof(CPLGetXMLValue(psDstRect,"yOff","-1"));
        m_dfDstXSize = CPLAtof(CPLGetXMLValue(psDstRect,"xSize","-1"));
        m_dfDstYSize = CPLAtof(CPLGetXMLValue(psDstRect,"ySize","-1"));
    }
    else
    {
        m_dfDstXOff = m_dfDstYOff = m_dfDstXSize = m_dfDstYSize = -1;
    }

    return CE_None;
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTSimpleSource::GetFileList(char*** ppapszFileList, int *pnSize,
                                  int *pnMaxSize, CPLHashSet* hSetFiles)
{
    const char* pszFilename;
    if (m_poRasterBand != NULL && m_poRasterBand->GetDataset() != NULL &&
        (pszFilename = m_poRasterBand->GetDataset()->GetDescription()) != NULL)
    {
/* -------------------------------------------------------------------- */
/*      Is the filename even a real filesystem object?                  */
/* -------------------------------------------------------------------- */
        if ( strstr(pszFilename, "/vsicurl/http") != NULL ||
             strstr(pszFilename, "/vsicurl/ftp") != NULL )
        {
            /* Testing the existence of remote resources can be excruciating */
            /* slow, so let's just suppose they exist */
        }
        else
        {
            VSIStatBufL  sStat;
            if( VSIStatExL( pszFilename, &sStat, VSI_STAT_EXISTS_FLAG ) != 0 )
                return;
        }
            
/* -------------------------------------------------------------------- */
/*      Is it already in the list ?                                     */
/* -------------------------------------------------------------------- */
        if( CPLHashSetLookup(hSetFiles, pszFilename) != NULL )
            return;
        
/* -------------------------------------------------------------------- */
/*      Grow array if necessary                                         */
/* -------------------------------------------------------------------- */
        if (*pnSize + 1 >= *pnMaxSize)
        {
            *pnMaxSize = 2 + 2 * (*pnMaxSize);
            *ppapszFileList = (char **) CPLRealloc(
                        *ppapszFileList, sizeof(char*)  * (*pnMaxSize) );
        }
            
/* -------------------------------------------------------------------- */
/*      Add the string to the list                                      */
/* -------------------------------------------------------------------- */
        (*ppapszFileList)[*pnSize] = CPLStrdup(pszFilename);
        (*ppapszFileList)[(*pnSize + 1)] = NULL;
        CPLHashSetInsert(hSetFiles, (*ppapszFileList)[*pnSize]);
        
        (*pnSize) ++;
    }
}

/************************************************************************/
/*                             GetBand()                                */
/************************************************************************/

GDALRasterBand* VRTSimpleSource::GetBand()
{
    return m_poMaskBandMainBand ? NULL : m_poRasterBand;
}

/************************************************************************/
/*                       IsSameExceptBandNumber()                       */
/************************************************************************/

int VRTSimpleSource::IsSameExceptBandNumber(VRTSimpleSource* poOtherSource)
{
    return m_dfSrcXOff == poOtherSource->m_dfSrcXOff &&
           m_dfSrcYOff == poOtherSource->m_dfSrcYOff &&
           m_dfSrcXSize == poOtherSource->m_dfSrcXSize &&
           m_dfSrcYSize == poOtherSource->m_dfSrcYSize &&
           m_dfDstXOff == poOtherSource->m_dfDstXOff &&
           m_dfDstYOff == poOtherSource->m_dfDstYOff &&
           m_dfDstXSize == poOtherSource->m_dfDstXSize &&
           m_dfDstYSize == poOtherSource->m_dfDstYSize &&
           m_bNoDataSet == poOtherSource->m_bNoDataSet &&
           m_dfNoDataValue == poOtherSource->m_dfNoDataValue &&
           GetBand() != NULL && poOtherSource->GetBand() != NULL &&
           GetBand()->GetDataset() != NULL &&
           poOtherSource->GetBand()->GetDataset() != NULL &&
           EQUAL(GetBand()->GetDataset()->GetDescription(),
                 poOtherSource->GetBand()->GetDataset()->GetDescription());
}

/************************************************************************/
/*                              SrcToDst()                              */
/*                                                                      */
/*      Note: this is a no-op if the dst window is -1,-1,-1,-1.         */
/************************************************************************/

void VRTSimpleSource::SrcToDst( double dfX, double dfY,
                                double &dfXOut, double &dfYOut )

{
    dfXOut = ((dfX - m_dfSrcXOff) / m_dfSrcXSize) * m_dfDstXSize + m_dfDstXOff;
    dfYOut = ((dfY - m_dfSrcYOff) / m_dfSrcYSize) * m_dfDstYSize + m_dfDstYOff;
}

/************************************************************************/
/*                              DstToSrc()                              */
/*                                                                      */
/*      Note: this is a no-op if the dst window is -1,-1,-1,-1.         */
/************************************************************************/

void VRTSimpleSource::DstToSrc( double dfX, double dfY,
                                double &dfXOut, double &dfYOut )

{
    dfXOut = ((dfX - m_dfDstXOff) / m_dfDstXSize) * m_dfSrcXSize + m_dfSrcXOff;
    dfYOut = ((dfY - m_dfDstYOff) / m_dfDstYSize) * m_dfSrcYSize + m_dfSrcYOff;
}

/************************************************************************/
/*                          GetSrcDstWindow()                           */
/************************************************************************/

int 
VRTSimpleSource::GetSrcDstWindow( int nXOff, int nYOff, int nXSize, int nYSize,
                                  int nBufXSize, int nBufYSize,
                                  double *pdfReqXOff, double *pdfReqYOff,
                                  double *pdfReqXSize, double *pdfReqYSize,
                                  int *pnReqXOff, int *pnReqYOff,
                                  int *pnReqXSize, int *pnReqYSize,
                                  int *pnOutXOff, int *pnOutYOff, 
                                  int *pnOutXSize, int *pnOutYSize )

{
    const bool bDstWinSet = m_dfDstXOff != -1 || m_dfDstXSize != -1 
        || m_dfDstYOff != -1 || m_dfDstYSize != -1;

#ifdef DEBUG
    const bool bSrcWinSet = m_dfSrcXOff != -1 || m_dfSrcXSize != -1 
        || m_dfSrcYOff != -1 || m_dfSrcYSize != -1;

    if( bSrcWinSet != bDstWinSet )
    {
        return FALSE;
    }
#endif

/* -------------------------------------------------------------------- */
/*      If the input window completely misses the portion of the        */
/*      virtual dataset provided by this source we have nothing to do.  */
/* -------------------------------------------------------------------- */
    if( bDstWinSet )
    {
        if( nXOff >= m_dfDstXOff + m_dfDstXSize
            || nYOff >= m_dfDstYOff + m_dfDstYSize
            || nXOff + nXSize < m_dfDstXOff
            || nYOff + nYSize < m_dfDstYOff )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      This request window corresponds to the whole output buffer.     */
/* -------------------------------------------------------------------- */
    *pnOutXOff = 0;
    *pnOutYOff = 0;
    *pnOutXSize = nBufXSize;
    *pnOutYSize = nBufYSize;

/* -------------------------------------------------------------------- */
/*      If the input window extents outside the portion of the on       */
/*      the virtual file that this source can set, then clip down       */
/*      the requested window.                                           */
/* -------------------------------------------------------------------- */
    int bModifiedX = FALSE, bModifiedY = FALSE;
    double dfRXOff = nXOff;
    double dfRYOff = nYOff;
    double dfRXSize = nXSize;
    double dfRYSize = nYSize;


    if( bDstWinSet )
    {
        if( dfRXOff < m_dfDstXOff )
        {
            dfRXSize = dfRXSize + dfRXOff - m_dfDstXOff;
            dfRXOff = m_dfDstXOff;
            bModifiedX = TRUE;
        }

        if( dfRYOff < m_dfDstYOff )
        {
            dfRYSize = dfRYSize + dfRYOff - m_dfDstYOff;
            dfRYOff = m_dfDstYOff;
            bModifiedY = TRUE;
        }

        if( dfRXOff + dfRXSize > m_dfDstXOff + m_dfDstXSize )
        {
            dfRXSize = m_dfDstXOff + m_dfDstXSize - dfRXOff;
            bModifiedX = TRUE;
        }

        if( dfRYOff + dfRYSize > m_dfDstYOff + m_dfDstYSize )
        {
            dfRYSize = m_dfDstYOff + m_dfDstYSize - dfRYOff;
            bModifiedY = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate requested region in virtual file into the source      */
/*      band coordinates.                                               */
/* -------------------------------------------------------------------- */
    const double      dfScaleX = m_dfSrcXSize / m_dfDstXSize;
    const double      dfScaleY = m_dfSrcYSize / m_dfDstYSize;
    
    *pdfReqXOff = (dfRXOff - m_dfDstXOff) * dfScaleX + m_dfSrcXOff;
    *pdfReqYOff = (dfRYOff - m_dfDstYOff) * dfScaleY + m_dfSrcYOff;
    *pdfReqXSize = dfRXSize * dfScaleX;
    *pdfReqYSize = dfRYSize * dfScaleY;

/* -------------------------------------------------------------------- */
/*      Clamp within the bounds of the available source data.           */
/* -------------------------------------------------------------------- */
    if( *pdfReqXOff < 0 )
    {
        *pdfReqXSize += *pdfReqXOff;
        *pdfReqXOff = 0;
        bModifiedX = TRUE;
    }
    if( *pdfReqYOff < 0 )
    {
        *pdfReqYSize += *pdfReqYOff;
        *pdfReqYOff = 0;
        bModifiedY = TRUE;
    }

    *pnReqXOff = (int) floor(*pdfReqXOff);
    *pnReqYOff = (int) floor(*pdfReqYOff);

    *pnReqXSize = (int) floor(*pdfReqXSize + 0.5);
    *pnReqYSize = (int) floor(*pdfReqYSize + 0.5);

/* -------------------------------------------------------------------- */
/*      Clamp within the bounds of the available source data.           */
/* -------------------------------------------------------------------- */

    if( *pnReqXSize == 0 )
        *pnReqXSize = 1;
    if( *pnReqYSize == 0 )
        *pnReqYSize = 1;

    if( *pnReqXOff + *pnReqXSize > m_poRasterBand->GetXSize() )
    {
        *pnReqXSize = m_poRasterBand->GetXSize() - *pnReqXOff;
        bModifiedX = TRUE;
    }
    if( *pdfReqXOff + *pdfReqXSize > m_poRasterBand->GetXSize() )
    {
        *pdfReqXSize = m_poRasterBand->GetXSize() - *pdfReqXOff;
        bModifiedX = TRUE;
    }

    if( *pnReqYOff + *pnReqYSize > m_poRasterBand->GetYSize() )
    {
        *pnReqYSize = m_poRasterBand->GetYSize() - *pnReqYOff;
        bModifiedY = TRUE;
    }
    if( *pdfReqYOff + *pdfReqYSize > m_poRasterBand->GetYSize() )
    {
        *pdfReqYSize = m_poRasterBand->GetYSize() - *pdfReqYOff;
        bModifiedY = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Don't do anything if the requesting region is completely off    */
/*      the source image.                                               */
/* -------------------------------------------------------------------- */
    if( *pnReqXOff >= m_poRasterBand->GetXSize()
        || *pnReqYOff >= m_poRasterBand->GetYSize()
        || *pnReqXSize <= 0 || *pnReqYSize <= 0 )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we haven't had to modify the source rectangle, then the      */
/*      destination rectangle must be the whole region.                 */
/* -------------------------------------------------------------------- */
    if( !bModifiedX && !bModifiedY )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Now transform this possibly reduced request back into the       */
/*      destination buffer coordinates in case the output region is     */
/*      less than the whole buffer.                                     */
/* -------------------------------------------------------------------- */
    double dfDstULX, dfDstULY, dfDstLRX, dfDstLRY;
    double dfScaleWinToBufX, dfScaleWinToBufY;

    SrcToDst( *pdfReqXOff, *pdfReqYOff, dfDstULX, dfDstULY );
    SrcToDst( *pdfReqXOff + *pdfReqXSize, *pdfReqYOff + *pdfReqYSize, 
              dfDstLRX, dfDstLRY );
    //CPLDebug("VRT", "dfDstULX=%g dfDstULY=%g dfDstLRX=%g dfDstLRY=%g",
    //         dfDstULX, dfDstULY, dfDstLRX, dfDstLRY);

    if( bModifiedX )
    {
        dfScaleWinToBufX = nBufXSize / (double) nXSize;

        *pnOutXOff = (int) ((dfDstULX - nXOff) * dfScaleWinToBufX+0.001);
        *pnOutXSize = (int) ceil((dfDstLRX - nXOff) * dfScaleWinToBufX-0.001) 
            - *pnOutXOff;

        *pnOutXOff = MAX(0,*pnOutXOff);
        if( *pnOutXOff + *pnOutXSize > nBufXSize )
            *pnOutXSize = nBufXSize - *pnOutXOff;
    }

    if( bModifiedY )
    {
        dfScaleWinToBufY = nBufYSize / (double) nYSize;

        *pnOutYOff = (int) ((dfDstULY - nYOff) * dfScaleWinToBufY+0.001);
        *pnOutYSize = (int) ceil((dfDstLRY - nYOff) * dfScaleWinToBufY-0.001) 
            - *pnOutYOff;

        *pnOutYOff = MAX(0,*pnOutYOff);
        if( *pnOutYOff + *pnOutYSize > nBufYSize )
            *pnOutYSize = nBufYSize - *pnOutYOff;
    }

    if( *pnOutXSize < 1 || *pnOutYSize < 1 )
        return FALSE;
    else
        return TRUE;
}

/************************************************************************/
/*                          NeedMaxValAdjustment()                      */
/************************************************************************/

int VRTSimpleSource::NeedMaxValAdjustment() const
{
    if( m_nMaxValue )
    {
        const char* pszNBITS = m_poRasterBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
        int nBits = (pszNBITS) ? atoi(pszNBITS) : 0;
        int nBandMaxValue = (1 << nBits) - 1;
        if( nBandMaxValue == 0 || nBandMaxValue > m_nMaxValue )
        {
            return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr 
VRTSimpleSource::RasterIO( int nXOff, int nYOff, int nXSize, int nYSize,
                           void *pData, int nBufXSize, int nBufYSize, 
                           GDALDataType eBufType, 
                           GSpacing nPixelSpace,
                           GSpacing nLineSpace,
                           GDALRasterIOExtraArg* psExtraArgIn )

{
    GDALRasterIOExtraArg sExtraArg;

    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    GDALRasterIOExtraArg* psExtraArg = &sExtraArg;

    // The window we will actually request from the source raster band.
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

    if( !GetSrcDstWindow( nXOff, nYOff, nXSize, nYSize,
                          nBufXSize, nBufYSize, 
                          &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                          &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                          &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) )
    {
        return CE_None;
    }
    /*CPLDebug("VRT", "nXOff=%d, nYOff=%d, nXSize=%d, nYSize=%d, nBufXSize=%d, nBufYSize=%d, "
                    "dfReqXOff=%g, dfReqYOff=%g, dfReqXSize=%g, dfReqYSize=%g, "
                    "nOutXOff=%d, nOutYOff=%d, nOutXSize=%d, nOutYSize=%d",
                    nXOff, nYOff, nXSize, nYSize,
                    nBufXSize, nBufYSize, 
                    dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize,
                    nOutXOff, nOutYOff, nOutXSize, nOutYSize);*/

/* -------------------------------------------------------------------- */
/*      Actually perform the IO request.                                */
/* -------------------------------------------------------------------- */
    CPLErr eErr;

    if( m_osResampling.size() )
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }
    else if( psExtraArgIn != NULL )
    {
        psExtraArg->eResampleAlg = psExtraArgIn->eResampleAlg;
    }
    psExtraArg->bFloatingPointWindowValidity = TRUE;
    psExtraArg->dfXOff = dfReqXOff;
    psExtraArg->dfYOff = dfReqYOff;
    psExtraArg->dfXSize = dfReqXSize;
    psExtraArg->dfYSize = dfReqYSize;

    GByte* pabyOut = ((unsigned char *) pData) 
                                + nOutXOff * nPixelSpace
                                + (GPtrDiff_t)nOutYOff * nLineSpace;
    eErr = 
        m_poRasterBand->RasterIO( GF_Read, 
                                nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                                pabyOut,
                                nOutXSize, nOutYSize, 
                                eBufType, nPixelSpace, nLineSpace, psExtraArg );

    if( NeedMaxValAdjustment() )
    {
        for(int j=0;j<nOutYSize;j++)
        {
            for(int i=0;i<nOutXSize;i++)
            {
                int nVal;
                GDALCopyWords(pabyOut + j * nLineSpace + i * nPixelSpace,
                                eBufType, 0,
                                &nVal, GDT_Int32, 0,
                                1);
                if( nVal > m_nMaxValue )
                    nVal = m_nMaxValue;
                GDALCopyWords(&nVal, GDT_Int32, 0,
                                pabyOut + j * nLineSpace + i * nPixelSpace,
                                eBufType, 0,
                                1);
            }
        }
    }

    return eErr;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTSimpleSource::GetMinimum( int nXSize, int nYSize, int *pbSuccess )
{
    // The window we will actually request from the source raster band.
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

    if( !GetSrcDstWindow( 0, 0, nXSize, nYSize,
                          nXSize, nYSize,
                          &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                          &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                          &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) ||
        nReqXOff != 0 || nReqYOff != 0 ||
        nReqXSize != m_poRasterBand->GetXSize() ||
        nReqYSize != m_poRasterBand->GetYSize())
    {
        *pbSuccess = FALSE;
        return 0;
    }

    double dfVal = m_poRasterBand->GetMinimum(pbSuccess);
    if( NeedMaxValAdjustment() && dfVal > m_nMaxValue )
        dfVal = m_nMaxValue;
    return dfVal;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTSimpleSource::GetMaximum( int nXSize, int nYSize, int *pbSuccess )
{
    // The window we will actually request from the source raster band.
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

    if( !GetSrcDstWindow( 0, 0, nXSize, nYSize,
                          nXSize, nYSize,
                          &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                          &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                          &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) ||
        nReqXOff != 0 || nReqYOff != 0 ||
        nReqXSize != m_poRasterBand->GetXSize() ||
        nReqYSize != m_poRasterBand->GetYSize())
    {
        *pbSuccess = FALSE;
        return 0;
    }

    double dfVal = m_poRasterBand->GetMaximum(pbSuccess);
    if( NeedMaxValAdjustment() && dfVal > m_nMaxValue )
        dfVal = m_nMaxValue;
    return dfVal;
}

/************************************************************************/
/*                       ComputeRasterMinMax()                          */
/************************************************************************/

CPLErr VRTSimpleSource::ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK, double* adfMinMax )
{
    // The window we will actually request from the source raster band.
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

    if( !GetSrcDstWindow( 0, 0, nXSize, nYSize,
                          nXSize, nYSize,
                          &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                          &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                          &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) ||
        nReqXOff != 0 || nReqYOff != 0 ||
        nReqXSize != m_poRasterBand->GetXSize() ||
        nReqYSize != m_poRasterBand->GetYSize())
    {
        return CE_Failure;
    }

    CPLErr eErr = m_poRasterBand->ComputeRasterMinMax(bApproxOK, adfMinMax);
    if( NeedMaxValAdjustment() )
    {
        if( adfMinMax[0] > m_nMaxValue )
            adfMinMax[0] = m_nMaxValue;
        if( adfMinMax[1] > m_nMaxValue )
            adfMinMax[1] = m_nMaxValue;
    }
    return eErr;
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

CPLErr VRTSimpleSource::ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK, 
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    // The window we will actually request from the source raster band.
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

    if( NeedMaxValAdjustment() ||
        !GetSrcDstWindow( 0, 0, nXSize, nYSize,
                          nXSize, nYSize,
                          &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                          &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                          &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) ||
        nReqXOff != 0 || nReqYOff != 0 ||
        nReqXSize != m_poRasterBand->GetXSize() ||
        nReqYSize != m_poRasterBand->GetYSize())
    {
        return CE_Failure;
    }

    return m_poRasterBand->ComputeStatistics(bApproxOK, pdfMin, pdfMax,
                                           pdfMean, pdfStdDev,
                                           pfnProgress, pProgressData);
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTSimpleSource::GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress, void *pProgressData )
{
    // The window we will actually request from the source raster band.
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

    if( NeedMaxValAdjustment() ||
        !GetSrcDstWindow( 0, 0, nXSize, nYSize,
                          nXSize, nYSize,
                          &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                          &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                          &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) ||
        nReqXOff != 0 || nReqYOff != 0 ||
        nReqXSize != m_poRasterBand->GetXSize() ||
        nReqYSize != m_poRasterBand->GetYSize())
    {
        return CE_Failure;
    }

    return m_poRasterBand->GetHistogram( dfMin, dfMax, nBuckets,
                                       panHistogram,
                                       bIncludeOutOfRange, bApproxOK,
                                       pfnProgress, pProgressData );
}

/************************************************************************/
/*                          DatasetRasterIO()                           */
/************************************************************************/

CPLErr VRTSimpleSource::DatasetRasterIO(
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArgIn)
{
    if (!EQUAL(GetType(), "SimpleSource"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DatasetRasterIO() not implemented for %s", GetType());
        return CE_Failure;
    }

    GDALRasterIOExtraArg sExtraArg;

    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    GDALRasterIOExtraArg* psExtraArg = &sExtraArg;

    // The window we will actually request from the source raster band.
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

    if( !GetSrcDstWindow( nXOff, nYOff, nXSize, nYSize,
                          nBufXSize, nBufYSize,
                          &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                          &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                          &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) )
    {
        return CE_None;
    }

    GDALDataset* poDS = m_poRasterBand->GetDataset();
    if (poDS == NULL)
        return CE_Failure;

    if( m_osResampling.size() )
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }
    else if( psExtraArgIn != NULL )
    {
        psExtraArg->eResampleAlg = psExtraArgIn->eResampleAlg;
    }
    psExtraArg->bFloatingPointWindowValidity = TRUE;
    psExtraArg->dfXOff = dfReqXOff;
    psExtraArg->dfYOff = dfReqYOff;
    psExtraArg->dfXSize = dfReqXSize;
    psExtraArg->dfYSize = dfReqYSize;

    GByte* pabyOut = ((unsigned char *) pData)
                           + nOutXOff * nPixelSpace
                           + (GPtrDiff_t)nOutYOff * nLineSpace;
    CPLErr eErr = poDS->RasterIO( GF_Read,
                           nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                           pabyOut,
                           nOutXSize, nOutYSize,
                           eBufType, nBandCount, panBandMap,
                           nPixelSpace, nLineSpace, nBandSpace, psExtraArg );

    if( NeedMaxValAdjustment() )
    {
        for(int k=0;k<nBandCount;k++)
        {
            for(int j=0;j<nOutYSize;j++)
            {
                for(int i=0;i<nOutXSize;i++)
                {
                    int nVal;
                    GDALCopyWords(pabyOut + k * nBandSpace + j * nLineSpace + i * nPixelSpace,
                                eBufType, 0,
                                &nVal, GDT_Int32, 0,
                                1);
                    if( nVal > m_nMaxValue )
                        nVal = m_nMaxValue;
                    GDALCopyWords(&nVal, GDT_Int32, 0,
                                pabyOut + k * nBandSpace + j * nLineSpace + i * nPixelSpace,
                                eBufType, 0,
                                1);
                }
            }
        }
    }

    return eErr;
}

/************************************************************************/
/*                          SetResampling()                             */
/************************************************************************/

void VRTSimpleSource::SetResampling( const char* pszResampling )
{
    m_osResampling = (pszResampling) ? pszResampling : "";
}

/************************************************************************/
/* ==================================================================== */
/*                         VRTAveragedSource                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         VRTAveragedSource()                          */
/************************************************************************/

VRTAveragedSource::VRTAveragedSource()
{
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTAveragedSource::SerializeToXML( const char *pszVRTPath )

{
    CPLXMLNode *psSrc = VRTSimpleSource::SerializeToXML( pszVRTPath );

    if( psSrc == NULL )
        return NULL;

    CPLFree( psSrc->pszValue );
    psSrc->pszValue = CPLStrdup( "AveragedSource" );

    return psSrc;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr 
VRTAveragedSource::RasterIO( int nXOff, int nYOff, int nXSize, int nYSize,
                           void *pData, int nBufXSize, int nBufYSize, 
                           GDALDataType eBufType, 
                           GSpacing nPixelSpace,
                           GSpacing nLineSpace,
                           GDALRasterIOExtraArg* psExtraArgIn )

{
    GDALRasterIOExtraArg sExtraArg;

    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    GDALRasterIOExtraArg* psExtraArg = &sExtraArg;

    // The window we will actually request from the source raster band.
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

    if( !GetSrcDstWindow( nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, 
                          &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                          &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                          &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer to whole the full resolution        */
/*      data from the area of interest.                                 */
/* -------------------------------------------------------------------- */
    float *pafSrc;

    pafSrc = (float *) VSI_MALLOC3_VERBOSE(sizeof(float), nReqXSize, nReqYSize);
    if( pafSrc == NULL )
    {
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Load it.                                                        */
/* -------------------------------------------------------------------- */
    CPLErr eErr;

    if( m_osResampling.size() )
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }
    else if( psExtraArgIn != NULL )
    {
        psExtraArg->eResampleAlg = psExtraArgIn->eResampleAlg;
    }

    psExtraArg->bFloatingPointWindowValidity = TRUE;
    psExtraArg->dfXOff = dfReqXOff;
    psExtraArg->dfYOff = dfReqYOff;
    psExtraArg->dfXSize = dfReqXSize;
    psExtraArg->dfYSize = dfReqYSize;

    eErr = m_poRasterBand->RasterIO( GF_Read, 
                                   nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                                   pafSrc, nReqXSize, nReqYSize, GDT_Float32, 
                                   0, 0, psExtraArg );

    if( eErr != CE_None )
    {
        VSIFree( pafSrc );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Do the averaging.                                               */
/* -------------------------------------------------------------------- */
    for( int iBufLine = nOutYOff; iBufLine < nOutYOff + nOutYSize; iBufLine++ )
    {
        double  dfYDst;

        dfYDst = (iBufLine / (double) nBufYSize) * nYSize + nYOff;
        
        for( int iBufPixel = nOutXOff; 
             iBufPixel < nOutXOff + nOutXSize; 
             iBufPixel++ )
        {
            double dfXDst;
            double dfXSrcStart, dfXSrcEnd, dfYSrcStart, dfYSrcEnd;
            int    iXSrcStart, iYSrcStart, iXSrcEnd, iYSrcEnd;

            dfXDst = (iBufPixel / (double) nBufXSize) * nXSize + nXOff;

            // Compute the source image rectangle needed for this pixel.
            DstToSrc( dfXDst, dfYDst, dfXSrcStart, dfYSrcStart );
            DstToSrc( dfXDst+1.0, dfYDst+1.0, dfXSrcEnd, dfYSrcEnd );

            // Convert to integers, assuming that the center of the source
            // pixel must be in our rect to get included.
            if (dfXSrcEnd >= dfXSrcStart + 1)
            {
                iXSrcStart = (int) floor(dfXSrcStart+0.5);
                iXSrcEnd = (int) floor(dfXSrcEnd+0.5);
            }
            else
            {
                /* If the resampling factor is less than 100%, the distance */
                /* between the source pixel is < 1, so we stick to nearest */
                /* neighbour */
                iXSrcStart = (int) floor(dfXSrcStart);
                iXSrcEnd = iXSrcStart + 1;
            }
            if (dfYSrcEnd >= dfYSrcStart + 1)
            {
                iYSrcStart = (int) floor(dfYSrcStart+0.5);
                iYSrcEnd = (int) floor(dfYSrcEnd+0.5);
            }
            else
            {
                iYSrcStart = (int) floor(dfYSrcStart);
                iYSrcEnd = iYSrcStart + 1;
            }

            // Transform into the coordinate system of the source *buffer*
            iXSrcStart -= nReqXOff;
            iYSrcStart -= nReqYOff;
            iXSrcEnd -= nReqXOff;
            iYSrcEnd -= nReqYOff;

            double dfSum = 0.0;
            int    nPixelCount = 0;
            
            for( int iY = iYSrcStart; iY < iYSrcEnd; iY++ )
            {
                if( iY < 0 || iY >= nReqYSize )
                    continue;

                for( int iX = iXSrcStart; iX < iXSrcEnd; iX++ )
                {
                    if( iX < 0 || iX >= nReqXSize )
                        continue;

                    float fSampledValue = pafSrc[iX + iY * nReqXSize];
                    if (CPLIsNan(fSampledValue))
                        continue;

                    if( m_bNoDataSet && ARE_REAL_EQUAL(fSampledValue, m_dfNoDataValue))
                        continue;

                    nPixelCount++;
                    dfSum += pafSrc[iX + iY * nReqXSize];
                }
            }

            if( nPixelCount == 0 )
                continue;

            // Compute output value.
            float dfOutputValue = (float) (dfSum / nPixelCount);

            // Put it in the output buffer.
            GByte *pDstLocation;

            pDstLocation = ((GByte *)pData) 
                + nPixelSpace * iBufPixel
                + (GPtrDiff_t)nLineSpace * iBufLine;

            if( eBufType == GDT_Byte )
                *pDstLocation = (GByte) MIN(255,MAX(0,dfOutputValue + 0.5));
            else
                GDALCopyWords( &dfOutputValue, GDT_Float32, 4, 
                               pDstLocation, eBufType, 8, 1 );
        }
    }

    VSIFree( pafSrc );

    return CE_None;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTAveragedSource::GetMinimum( CPL_UNUSED int nXSize,
                                      CPL_UNUSED int nYSize,
                                      CPL_UNUSED int *pbSuccess )
{
    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTAveragedSource::GetMaximum( CPL_UNUSED int nXSize,
                                      CPL_UNUSED int nYSize,
                                      CPL_UNUSED int *pbSuccess )
{
    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                       ComputeRasterMinMax()                          */
/************************************************************************/

CPLErr VRTAveragedSource::ComputeRasterMinMax( CPL_UNUSED int nXSize,
                                               CPL_UNUSED int nYSize,
                                               CPL_UNUSED int bApproxOK,
                                               CPL_UNUSED double* adfMinMax )
{
    return CE_Failure;
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

CPLErr VRTAveragedSource::ComputeStatistics( CPL_UNUSED int nXSize,
                                             CPL_UNUSED int nYSize,
                                             CPL_UNUSED int bApproxOK,
                                             CPL_UNUSED double *pdfMin,
                                             CPL_UNUSED double *pdfMax,
                                             CPL_UNUSED double *pdfMean,
                                             CPL_UNUSED double *pdfStdDev,
                                             CPL_UNUSED GDALProgressFunc pfnProgress,
                                             CPL_UNUSED void *pProgressData )
{
    return CE_Failure;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTAveragedSource::GetHistogram( CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED double dfMin,
                                        CPL_UNUSED double dfMax,
                                        CPL_UNUSED int nBuckets,
                                        CPL_UNUSED GUIntBig * panHistogram,
                                        CPL_UNUSED int bIncludeOutOfRange,
                                        CPL_UNUSED int bApproxOK,
                                        CPL_UNUSED GDALProgressFunc pfnProgress,
                                        CPL_UNUSED void *pProgressData )
{
    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                          VRTComplexSource                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          VRTComplexSource()                          */
/************************************************************************/

VRTComplexSource::VRTComplexSource()

{
    m_eScalingType = VRT_SCALING_NONE;
    m_dfScaleOff = 0.0;
    m_dfScaleRatio = 1.0;
    
    m_padfLUTInputs = NULL;
    m_padfLUTOutputs = NULL;
    m_nLUTItemCount = 0;
    m_nColorTableComponent = 0;

    m_bSrcMinMaxDefined = FALSE;
    m_dfSrcMin = 0.0;
    m_dfSrcMax = 0.0;
    m_dfDstMin = 0.0;
    m_dfDstMax = 0.0;
    m_dfExponent = 1.0;
}

VRTComplexSource::VRTComplexSource(const VRTComplexSource* poSrcSource,
                                   double dfXDstRatio, double dfYDstRatio) :
                        VRTSimpleSource(poSrcSource, dfXDstRatio, dfYDstRatio)
{
    m_eScalingType = poSrcSource->m_eScalingType;
    m_dfScaleOff = poSrcSource->m_dfScaleOff;
    m_dfScaleRatio = poSrcSource->m_dfScaleRatio;

    m_nLUTItemCount = poSrcSource->m_nLUTItemCount;
    if( m_nLUTItemCount )
    {
        m_padfLUTInputs = (double*)CPLMalloc(sizeof(double) * m_nLUTItemCount);
        memcpy(m_padfLUTInputs, poSrcSource->m_padfLUTInputs, sizeof(double) * m_nLUTItemCount);
        m_padfLUTOutputs = (double*)CPLMalloc(sizeof(double) * m_nLUTItemCount);
        memcpy(m_padfLUTOutputs, poSrcSource->m_padfLUTOutputs, sizeof(double) * m_nLUTItemCount);
    }
    else
    {
        m_padfLUTInputs = NULL;
        m_padfLUTOutputs = NULL;
    }
    m_nColorTableComponent = poSrcSource->m_nColorTableComponent;

    m_bSrcMinMaxDefined = poSrcSource->m_bSrcMinMaxDefined;
    m_dfSrcMin = poSrcSource->m_dfSrcMin;
    m_dfSrcMax = poSrcSource->m_dfSrcMax;
    m_dfDstMin = poSrcSource->m_dfDstMin;
    m_dfDstMax = poSrcSource->m_dfDstMax;
    m_dfExponent = poSrcSource->m_dfExponent;
}

VRTComplexSource::~VRTComplexSource()
{
    if (m_padfLUTInputs)
        VSIFree( m_padfLUTInputs );
    if (m_padfLUTOutputs)
        VSIFree( m_padfLUTOutputs );
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTComplexSource::SerializeToXML( const char *pszVRTPath )

{
    CPLXMLNode *psSrc = VRTSimpleSource::SerializeToXML( pszVRTPath );

    if( psSrc == NULL )
        return NULL;

    CPLFree( psSrc->pszValue );
    psSrc->pszValue = CPLStrdup( "ComplexSource" );

    if( m_bNoDataSet )
    {
        if (CPLIsNan(m_dfNoDataValue))
            CPLSetXMLValue( psSrc, "NODATA", "nan");
        else
            CPLSetXMLValue( psSrc, "NODATA", 
                            CPLSPrintf("%.16g", m_dfNoDataValue) );
    }
        
    switch( m_eScalingType )
    {
        case VRT_SCALING_NONE:
            break;

        case VRT_SCALING_LINEAR:
        {
            CPLSetXMLValue( psSrc, "ScaleOffset", 
                            CPLSPrintf("%g", m_dfScaleOff) );
            CPLSetXMLValue( psSrc, "ScaleRatio", 
                            CPLSPrintf("%g", m_dfScaleRatio) );
            break;
        }

        case VRT_SCALING_EXPONENTIAL:
        {
            CPLSetXMLValue( psSrc, "Exponent", 
                            CPLSPrintf("%g", m_dfExponent) );
            CPLSetXMLValue( psSrc, "SrcMin", 
                            CPLSPrintf("%g", m_dfSrcMin) );
            CPLSetXMLValue( psSrc, "SrcMax", 
                            CPLSPrintf("%g", m_dfSrcMax) );
            CPLSetXMLValue( psSrc, "DstMin", 
                            CPLSPrintf("%g", m_dfDstMin) );
            CPLSetXMLValue( psSrc, "DstMax", 
                            CPLSPrintf("%g", m_dfDstMax) );
            break;
        }
    }

    if ( m_nLUTItemCount )
    {
        CPLString osLUT = CPLString().Printf("%g:%g", m_padfLUTInputs[0], m_padfLUTOutputs[0]);
        int i;
        for ( i = 1; i < m_nLUTItemCount; i++ )
            osLUT += CPLString().Printf(",%g:%g", m_padfLUTInputs[i], m_padfLUTOutputs[i]);
        CPLSetXMLValue( psSrc, "LUT", osLUT );
    }

    if ( m_nColorTableComponent )
    {
        CPLSetXMLValue( psSrc, "ColorTableComponent", 
                        CPLSPrintf("%d", m_nColorTableComponent) );
    }

    return psSrc;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTComplexSource::XMLInit( CPLXMLNode *psSrc, const char *pszVRTPath )

{
    CPLErr eErr;

/* -------------------------------------------------------------------- */
/*      Do base initialization.                                         */
/* -------------------------------------------------------------------- */
    eErr = VRTSimpleSource::XMLInit( psSrc, pszVRTPath );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Complex parameters.                                             */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue(psSrc, "ScaleOffset", NULL) != NULL 
        || CPLGetXMLValue(psSrc, "ScaleRatio", NULL) != NULL )
    {
        m_eScalingType = VRT_SCALING_LINEAR;
        m_dfScaleOff = CPLAtof(CPLGetXMLValue(psSrc, "ScaleOffset", "0") );
        m_dfScaleRatio = CPLAtof(CPLGetXMLValue(psSrc, "ScaleRatio", "1") );
    }
    else if( CPLGetXMLValue(psSrc, "Exponent", NULL) != NULL &&
             CPLGetXMLValue(psSrc, "DstMin", NULL) != NULL && 
             CPLGetXMLValue(psSrc, "DstMax", NULL) != NULL )
    {
        m_eScalingType = VRT_SCALING_EXPONENTIAL;
        m_dfExponent = CPLAtof(CPLGetXMLValue(psSrc, "Exponent", "1.0") );

        if( CPLGetXMLValue(psSrc, "SrcMin", NULL) != NULL 
         && CPLGetXMLValue(psSrc, "SrcMax", NULL) != NULL )
        {
            m_dfSrcMin = CPLAtof(CPLGetXMLValue(psSrc, "SrcMin", "0.0") );
            m_dfSrcMax = CPLAtof(CPLGetXMLValue(psSrc, "SrcMax", "0.0") );
            m_bSrcMinMaxDefined = TRUE;
        }

        m_dfDstMin = CPLAtof(CPLGetXMLValue(psSrc, "DstMin", "0.0") );
        m_dfDstMax = CPLAtof(CPLGetXMLValue(psSrc, "DstMax", "0.0") );
    }

    if( CPLGetXMLValue(psSrc, "NODATA", NULL) != NULL )
    {
        m_bNoDataSet = TRUE;
        m_dfNoDataValue = CPLAtofM(CPLGetXMLValue(psSrc, "NODATA", "0"));
    }

    if( CPLGetXMLValue(psSrc, "LUT", NULL) != NULL )
    {
        int nIndex;
        char **papszValues = CSLTokenizeString2(CPLGetXMLValue(psSrc, "LUT", ""), ",:", CSLT_ALLOWEMPTYTOKENS);

        if (m_nLUTItemCount)
        {
            if (m_padfLUTInputs)
            {
                VSIFree( m_padfLUTInputs );
                m_padfLUTInputs = NULL;
            }
            if (m_padfLUTOutputs)
            {
                VSIFree( m_padfLUTOutputs );
                m_padfLUTOutputs = NULL;
            }
            m_nLUTItemCount = 0;
        }

        m_nLUTItemCount = CSLCount(papszValues) / 2;

        m_padfLUTInputs = (double *) VSIMalloc2(m_nLUTItemCount, sizeof(double));
        if ( !m_padfLUTInputs )
        {
            CSLDestroy(papszValues);
            m_nLUTItemCount = 0;
            return CE_Failure;
        }

        m_padfLUTOutputs = (double *) VSIMalloc2(m_nLUTItemCount, sizeof(double));
        if ( !m_padfLUTOutputs )
        {
            CSLDestroy(papszValues);
            VSIFree( m_padfLUTInputs );
            m_padfLUTInputs = NULL;
            m_nLUTItemCount = 0;
            return CE_Failure;
        }
        
        for ( nIndex = 0; nIndex < m_nLUTItemCount; nIndex++ )
        {
            m_padfLUTInputs[nIndex] = CPLAtof( papszValues[nIndex * 2] );
            m_padfLUTOutputs[nIndex] = CPLAtof( papszValues[nIndex * 2 + 1] );

	    // Enforce the requirement that the LUT input array is monotonically non-decreasing.
	    if ( nIndex > 0 && m_padfLUTInputs[nIndex] < m_padfLUTInputs[nIndex - 1] )
	    {
		CSLDestroy(papszValues);
		VSIFree( m_padfLUTInputs );
		VSIFree( m_padfLUTOutputs );
		m_padfLUTInputs = NULL;
		m_padfLUTOutputs = NULL;
		m_nLUTItemCount = 0;
		return CE_Failure;
	    }
        }
        
        CSLDestroy(papszValues);
    }
    
    if( CPLGetXMLValue(psSrc, "ColorTableComponent", NULL) != NULL )
    {
        m_nColorTableComponent = atoi(CPLGetXMLValue(psSrc, "ColorTableComponent", "0"));
    }

    return CE_None;
}

/************************************************************************/
/*                              LookupValue()                           */
/************************************************************************/

double
VRTComplexSource::LookupValue( double dfInput )
{
    // Find the index of the first element in the LUT input array that
    // is not smaller than the input value.
    int i = static_cast<int>(std::lower_bound(m_padfLUTInputs, m_padfLUTInputs + m_nLUTItemCount, dfInput) - m_padfLUTInputs);

    if (i == 0)
	return m_padfLUTOutputs[0];

    // If the index is beyond the end of the LUT input array, the input
    // value is larger than all the values in the array.
    if (i == m_nLUTItemCount)
	return m_padfLUTOutputs[m_nLUTItemCount - 1];

    if (m_padfLUTInputs[i] == dfInput)
	return m_padfLUTOutputs[i];

    // Otherwise, interpolate.
    return m_padfLUTOutputs[i - 1] + (dfInput - m_padfLUTInputs[i - 1]) *
	((m_padfLUTOutputs[i] - m_padfLUTOutputs[i - 1]) / (m_padfLUTInputs[i] - m_padfLUTInputs[i - 1]));
}

/************************************************************************/
/*                         SetLinearScaling()                           */
/************************************************************************/

void VRTComplexSource::SetLinearScaling(double dfOffset, double dfScale)
{
    m_eScalingType = VRT_SCALING_LINEAR;
    m_dfScaleOff = dfOffset;
    m_dfScaleRatio = dfScale;
}

/************************************************************************/
/*                         SetPowerScaling()                           */
/************************************************************************/

void VRTComplexSource::SetPowerScaling(double dfExponentIn,
                                       double dfSrcMinIn,
                                       double dfSrcMaxIn,
                                       double dfDstMinIn,
                                       double dfDstMaxIn)
{
    m_eScalingType = VRT_SCALING_EXPONENTIAL;
    m_dfExponent = dfExponentIn;
    m_dfSrcMin = dfSrcMinIn;
    m_dfSrcMax = dfSrcMaxIn;
    m_dfDstMin = dfDstMinIn;
    m_dfDstMax = dfDstMaxIn;
    m_bSrcMinMaxDefined = TRUE;
}

/************************************************************************/
/*                    SetColorTableComponent()                          */
/************************************************************************/

void VRTComplexSource::SetColorTableComponent(int nComponent)
{
    m_nColorTableComponent = nComponent;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr 
VRTComplexSource::RasterIO( int nXOff, int nYOff, int nXSize, int nYSize,
                            void *pData, int nBufXSize, int nBufYSize, 
                            GDALDataType eBufType, 
                            GSpacing nPixelSpace,
                            GSpacing nLineSpace,
                            GDALRasterIOExtraArg* psExtraArgIn)
    
{
    GDALRasterIOExtraArg sExtraArg;

    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    GDALRasterIOExtraArg* psExtraArg = &sExtraArg;

    // The window we will actually request from the source raster band.
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

    if( !GetSrcDstWindow( nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, 
                          &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                          &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize,
                          &nOutXOff, &nOutYOff, &nOutXSize, &nOutYSize ) )
        return CE_None;

    if( m_osResampling.size() )
    {
        psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
    }
    else if( psExtraArgIn != NULL )
    {
        psExtraArg->eResampleAlg = psExtraArgIn->eResampleAlg;
    }
    psExtraArg->bFloatingPointWindowValidity = TRUE;
    psExtraArg->dfXOff = dfReqXOff;
    psExtraArg->dfYOff = dfReqYOff;
    psExtraArg->dfXSize = dfReqXSize;
    psExtraArg->dfYSize = dfReqYSize;

    CPLErr eErr = RasterIOInternal(nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                       ((GByte *)pData)
                            + nPixelSpace * nOutXOff
                            + (GPtrDiff_t)nLineSpace * nOutYOff,
                       nOutXSize, nOutYSize,
                       eBufType,
                       nPixelSpace, nLineSpace, psExtraArg );

    return eErr;
}

/************************************************************************/
/*                          RasterIOInternal()                          */
/************************************************************************/

/* nReqXOff, nReqYOff, nReqXSize, nReqYSize are expressed in source band */
/* referential */
CPLErr VRTComplexSource::RasterIOInternal( int nReqXOff, int nReqYOff,
                                      int nReqXSize, int nReqYSize,
                                      void *pData, int nOutXSize, int nOutYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace,
                                      GSpacing nLineSpace,
                                      GDALRasterIOExtraArg* psExtraArg )
{
/* -------------------------------------------------------------------- */
/*      Read into a temporary buffer.                                   */
/* -------------------------------------------------------------------- */
    float *pafData;
    CPLErr eErr;
    GDALColorTable* poColorTable = NULL;
    const int bIsComplex = GDALDataTypeIsComplex(eBufType);
    const GDALDataType eWrkDataType = (bIsComplex) ? GDT_CFloat32 : GDT_Float32;
    const int nWordSize = GDALGetDataTypeSize(eWrkDataType) / 8;
    const int bNoDataSetIsNan = m_bNoDataSet && CPLIsNan(m_dfNoDataValue);
    const int bNoDataSetAndNotNan = m_bNoDataSet && !CPLIsNan(m_dfNoDataValue);

    if( m_eScalingType == VRT_SCALING_LINEAR &&
        m_bNoDataSet == FALSE && m_dfScaleRatio == 0)
    {
/* -------------------------------------------------------------------- */
/*      Optimization when writing a constant value                      */
/*      (used by the -addalpha option of gdalbuildvrt)                  */
/* -------------------------------------------------------------------- */
        pafData = NULL;
    }
    else
    {
        pafData = (float *) VSI_MALLOC3_VERBOSE(nOutXSize,nOutYSize,nWordSize);
        if (pafData == NULL)
        {
            return CE_Failure;
        }

        GDALRIOResampleAlg     eResampleAlgBack = psExtraArg->eResampleAlg;
        if( m_osResampling.size() )
        {
            psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(m_osResampling);
        }

        eErr = m_poRasterBand->RasterIO( GF_Read, 
                                       nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                                       pafData, nOutXSize, nOutYSize, eWrkDataType,
                                       nWordSize, nWordSize * (GSpacing)nOutXSize, psExtraArg );
        if( m_osResampling.size() )
            psExtraArg->eResampleAlg = eResampleAlgBack;

        if( eErr != CE_None )
        {
            CPLFree( pafData );
            return eErr;
        }

        if (m_nColorTableComponent != 0)
        {
            poColorTable = m_poRasterBand->GetColorTable();
            if (poColorTable == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Source band has no color table.");
                CPLFree( pafData );
                return CE_Failure;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Selectively copy into output buffer with nodata masking,        */
/*      and/or scaling.                                                 */
/* -------------------------------------------------------------------- */
    int iX, iY;

    for( iY = 0; iY < nOutYSize; iY++ )
    {
        for( iX = 0; iX < nOutXSize; iX++ )
        {
            GByte *pDstLocation;

            pDstLocation = ((GByte *)pData)
                + nPixelSpace * iX
                + (GPtrDiff_t)nLineSpace * iY;

            if (pafData && !bIsComplex)
            {
                float fResult = pafData[iX + iY * nOutXSize];
                if( bNoDataSetIsNan && CPLIsNan(fResult) )
                    continue;
                if( bNoDataSetAndNotNan && ARE_REAL_EQUAL(fResult, m_dfNoDataValue) )
                    continue;

                if (m_nColorTableComponent)
                {
                    const GDALColorEntry* poEntry = poColorTable->GetColorEntry((int)fResult);
                    if (poEntry)
                    {
                        if (m_nColorTableComponent == 1)
                            fResult = poEntry->c1;
                        else if (m_nColorTableComponent == 2)
                            fResult = poEntry->c2;
                        else if (m_nColorTableComponent == 3)
                            fResult = poEntry->c3;
                        else if (m_nColorTableComponent == 4)
                            fResult = poEntry->c4;
                    }
                    else
                    {
                        static int bHasWarned = FALSE;
                        if (!bHasWarned)
                        {
                            bHasWarned = TRUE;
                            CPLError(CE_Failure, CPLE_AppDefined,
                                    "No entry %d.", (int)fResult);
                        }
                        continue;
                    }
                }

                if( m_eScalingType == VRT_SCALING_LINEAR )
                    fResult = (float) (fResult * m_dfScaleRatio + m_dfScaleOff);
                else if( m_eScalingType == VRT_SCALING_EXPONENTIAL )
                {
                    if( !m_bSrcMinMaxDefined )
                    {
                        int bSuccessMin = FALSE, bSuccessMax = FALSE;
                        double adfMinMax[2];
                        adfMinMax[0] = m_poRasterBand->GetMinimum(&bSuccessMin);
                        adfMinMax[1] = m_poRasterBand->GetMaximum(&bSuccessMax);
                        if( (bSuccessMin && bSuccessMax) ||
                            m_poRasterBand->ComputeRasterMinMax( TRUE, adfMinMax ) == CE_None )
                        {
                            m_dfSrcMin = adfMinMax[0];
                            m_dfSrcMax = adfMinMax[1];
                            m_bSrcMinMaxDefined = TRUE;
                        }
                        else
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                    "Cannot determine source min/max value");
                            return CE_Failure;
                        }
                    }

                    double dfPowVal =
                        (fResult - m_dfSrcMin) / (m_dfSrcMax - m_dfSrcMin);
                    if( dfPowVal < 0.0 )
                        dfPowVal = 0.0;
                    else if( dfPowVal > 1.0 )
                        dfPowVal = 1.0;
                    fResult = static_cast<float>((m_dfDstMax - m_dfDstMin) * pow( dfPowVal, m_dfExponent ) + m_dfDstMin);
                }

                if (m_nLUTItemCount)
                    fResult = static_cast<float>(LookupValue( fResult ));

                if( m_nMaxValue != 0 && fResult > m_nMaxValue )
                    fResult = static_cast<float>(m_nMaxValue);

                if( eBufType == GDT_Byte )
                    *pDstLocation = (GByte) MIN(255,MAX(0,fResult + 0.5));
                else
                    GDALCopyWords( &fResult, GDT_Float32, 0,
                                pDstLocation, eBufType, 0, 1 );
            }
            else if (pafData && bIsComplex)
            {
                float afResult[2];
                afResult[0] = pafData[2 * (iX + iY * nOutXSize)];
                afResult[1] = pafData[2 * (iX + iY * nOutXSize) + 1];

                /* Do not use color table */

                if( m_eScalingType == VRT_SCALING_LINEAR )
                {
                    afResult[0] = (float) (afResult[0] * m_dfScaleRatio + m_dfScaleOff);
                    afResult[1] = (float) (afResult[1] * m_dfScaleRatio + m_dfScaleOff);
                }

                /* Do not use LUT */

                if( eBufType == GDT_Byte )
                    *pDstLocation = (GByte) MIN(255,MAX(0,afResult[0] + 0.5));
                else
                    GDALCopyWords( afResult, GDT_CFloat32, 0,
                                   pDstLocation, eBufType, 0, 1 );
            }
            else
            {
                float fResult = static_cast<float>(m_dfScaleOff);

                if (m_nLUTItemCount)
                    fResult = static_cast<float>(LookupValue( fResult ));

                if( m_nMaxValue != 0 && fResult > m_nMaxValue )
                    fResult = static_cast<float>(m_nMaxValue);

                if( eBufType == GDT_Byte )
                    *pDstLocation = (GByte) MIN(255,MAX(0,fResult + 0.5));
                else
                    GDALCopyWords( &fResult, GDT_Float32, 0,
                                pDstLocation, eBufType, 0, 1 );
            }

        }
    }

    CPLFree( pafData );

    return CE_None;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTComplexSource::GetMinimum( int nXSize, int nYSize, int *pbSuccess )
{
    if (m_dfScaleOff == 0.0 && m_dfScaleRatio == 1.0 &&
        m_nLUTItemCount == 0 && m_nColorTableComponent == 0)
    {
        return VRTSimpleSource::GetMinimum(nXSize, nYSize, pbSuccess);
    }

    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTComplexSource::GetMaximum( int nXSize, int nYSize, int *pbSuccess )
{
    if (m_dfScaleOff == 0.0 && m_dfScaleRatio == 1.0 &&
        m_nLUTItemCount == 0 && m_nColorTableComponent == 0)
    {
        return VRTSimpleSource::GetMaximum(nXSize, nYSize, pbSuccess);
    }

    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                       ComputeRasterMinMax()                          */
/************************************************************************/

CPLErr VRTComplexSource::ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK, double* adfMinMax )
{
    if (m_dfScaleOff == 0.0 && m_dfScaleRatio == 1.0 &&
        m_nLUTItemCount == 0 && m_nColorTableComponent == 0)
    {
        return VRTSimpleSource::ComputeRasterMinMax(nXSize, nYSize, bApproxOK, adfMinMax);
    }

    return CE_Failure;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTComplexSource::GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress, void *pProgressData )
{
    if (m_dfScaleOff == 0.0 && m_dfScaleRatio == 1.0 &&
        m_nLUTItemCount == 0 && m_nColorTableComponent == 0)
    {
        return VRTSimpleSource::GetHistogram(nXSize, nYSize,
                                             dfMin, dfMax, nBuckets,
                                             panHistogram,
                                             bIncludeOutOfRange, bApproxOK,
                                             pfnProgress, pProgressData);
    }

    return CE_Failure;
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

CPLErr VRTComplexSource::ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK, 
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    if (m_dfScaleOff == 0.0 && m_dfScaleRatio == 1.0 &&
        m_nLUTItemCount == 0 && m_nColorTableComponent == 0)
    {
        return VRTSimpleSource::ComputeStatistics(nXSize, nYSize, bApproxOK, pdfMin, pdfMax,
                                           pdfMean, pdfStdDev,
                                           pfnProgress, pProgressData);
    }

    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                          VRTFuncSource                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           VRTFuncSource()                            */
/************************************************************************/

VRTFuncSource::VRTFuncSource()

{
    pfnReadFunc = NULL;
    pCBData = NULL;
    fNoDataValue = (float) VRT_NODATA_UNSET;
    eType = GDT_Byte;
}

/************************************************************************/
/*                           ~VRTFuncSource()                           */
/************************************************************************/

VRTFuncSource::~VRTFuncSource()

{
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTFuncSource::SerializeToXML( CPL_UNUSED const char * pszVRTPath )
{
    return NULL;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr
VRTFuncSource::RasterIO( int nXOff, int nYOff, int nXSize, int nYSize,
                         void *pData, int nBufXSize, int nBufYSize,
                         GDALDataType eBufType,
                         GSpacing nPixelSpace,
                         GSpacing nLineSpace,
                         CPL_UNUSED GDALRasterIOExtraArg* psExtraArg )
{
    if( nPixelSpace*8 == GDALGetDataTypeSize( eBufType )
        && nLineSpace == nPixelSpace * nXSize
        && nBufXSize == nXSize && nBufYSize == nYSize
        && eBufType == eType )
    {
        return pfnReadFunc( pCBData,
                            nXOff, nYOff, nXSize, nYSize,
                            pData );
    }
    else
    {
        printf( "%d,%d  %d,%d, %d,%d %d,%d %d,%d\n",
                (int)nPixelSpace*8, GDALGetDataTypeSize(eBufType),
                (int)nLineSpace, (int)nPixelSpace * nXSize,
                nBufXSize, nXSize,
                nBufYSize, nYSize,
                (int) eBufType, (int) eType );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRTFuncSource::RasterIO() - Irregular request." );
        return CE_Failure;
    }
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTFuncSource::GetMinimum( CPL_UNUSED int nXSize,
                                  CPL_UNUSED int nYSize,
                                  CPL_UNUSED CPL_UNUSED int *pbSuccess )
{
    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTFuncSource::GetMaximum( CPL_UNUSED int nXSize,
                                  CPL_UNUSED int nYSize,
                                  CPL_UNUSED int *pbSuccess )
{
    *pbSuccess = FALSE;
    return 0;
}

/************************************************************************/
/*                       ComputeRasterMinMax()                          */
/************************************************************************/

CPLErr VRTFuncSource::ComputeRasterMinMax( CPL_UNUSED int nXSize,
                                           CPL_UNUSED int nYSize,
                                           CPL_UNUSED int bApproxOK,
                                           CPL_UNUSED double* adfMinMax )
{
    return CE_Failure;
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

CPLErr VRTFuncSource::ComputeStatistics( CPL_UNUSED int nXSize,
                                         CPL_UNUSED int nYSize,
                                         CPL_UNUSED int bApproxOK,
                                         CPL_UNUSED double *pdfMin,
                                         CPL_UNUSED double *pdfMax,
                                         CPL_UNUSED double *pdfMean,
                                         CPL_UNUSED double *pdfStdDev,
                                         CPL_UNUSED GDALProgressFunc pfnProgress,
                                         CPL_UNUSED void *pProgressData )
{
    return CE_Failure;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTFuncSource::GetHistogram( CPL_UNUSED int nXSize,
                                    CPL_UNUSED int nYSize,
                                    CPL_UNUSED double dfMin,
                                    CPL_UNUSED double dfMax,
                                    CPL_UNUSED int nBuckets,
                                    CPL_UNUSED GUIntBig * panHistogram,
                                    CPL_UNUSED int bIncludeOutOfRange,
                                    CPL_UNUSED int bApproxOK,
                                    CPL_UNUSED GDALProgressFunc pfnProgress,
                                    CPL_UNUSED void *pProgressData )
{
    return CE_Failure;
}

/************************************************************************/
/*                        VRTParseCoreSources()                         */
/************************************************************************/

VRTSource *VRTParseCoreSources( CPLXMLNode *psChild, const char *pszVRTPath )

{
    VRTSource * poSource;

    if( EQUAL(psChild->pszValue,"AveragedSource") 
        || (EQUAL(psChild->pszValue,"SimpleSource")
            && STARTS_WITH_CI(CPLGetXMLValue(psChild, "Resampling", "Nearest"), "Aver")) )
    {
        poSource = new VRTAveragedSource();
    }
    else if( EQUAL(psChild->pszValue,"SimpleSource") )
    {
        poSource = new VRTSimpleSource();
    }
    else if( EQUAL(psChild->pszValue,"ComplexSource") )
    {
        poSource = new VRTComplexSource();
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "VRTParseCoreSources() - Unknown source : %s", psChild->pszValue );
        return NULL;
    }

    if ( poSource->XMLInit( psChild, pszVRTPath ) == CE_None )
        return poSource;

    delete poSource;
    return NULL;
}
