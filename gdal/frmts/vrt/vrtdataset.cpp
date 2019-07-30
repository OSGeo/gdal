/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTDataset
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <typeinfo>
#include "gdal_proxy.h"


/*! @cond Doxygen_Suppress */

CPL_CVSID("$Id$")

/************************************************************************/
/*                            VRTDataset()                             */
/************************************************************************/

VRTDataset::VRTDataset( int nXSize, int nYSize ) :
    m_bGeoTransformSet(FALSE),
    m_nGCPCount(0),
    m_pasGCPList(nullptr),
    m_bNeedsFlush(FALSE),
    m_bWritable(TRUE),
    m_pszVRTPath(nullptr),
    m_poMaskBand(nullptr),
    m_bCompatibleForDatasetIO(-1),
    m_papszXMLVRTMetadata(nullptr)
{
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;

    GDALRegister_VRT();

    poDriver = reinterpret_cast<GDALDriver *>( GDALGetDriverByName( "VRT" ) );
}

/*! @endcond */

/************************************************************************/
/*                              VRTCreate()                             */
/************************************************************************/

/**
 * @see VRTDataset::VRTDataset()
 */

VRTDatasetH CPL_STDCALL VRTCreate(int nXSize, int nYSize)

{
    return new VRTDataset(nXSize, nYSize);
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                            ~VRTDataset()                            */
/************************************************************************/

VRTDataset::~VRTDataset()

{
    VRTDataset::FlushCache();
    if( m_poSRS )
        m_poSRS->Release();
    if( m_poGCP_SRS )
        m_poGCP_SRS->Release();
    if( m_nGCPCount > 0 )
    {
        GDALDeinitGCPs( m_nGCPCount, m_pasGCPList );
        CPLFree( m_pasGCPList );
    }
    CPLFree( m_pszVRTPath );

    delete m_poMaskBand;

    for(size_t i=0;i<m_apoOverviews.size();i++)
        delete m_apoOverviews[i];
    for(size_t i=0;i<m_apoOverviewsBak.size();i++)
        delete m_apoOverviewsBak[i];
    CSLDestroy( m_papszXMLVRTMetadata );
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void VRTDataset::FlushCache()

{
    if( m_poRootGroup )
        m_poRootGroup->Serialize();
    else
        VRTFlushCacheStruct<VRTDataset>::FlushCache(*this);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void VRTWarpedDataset::FlushCache()

{
    VRTFlushCacheStruct<VRTWarpedDataset>::FlushCache(*this);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void VRTPansharpenedDataset::FlushCache()

{
    VRTFlushCacheStruct<VRTPansharpenedDataset>::FlushCache(*this);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

template<class T> void VRTFlushCacheStruct<T>::FlushCache(T& obj)
{
    obj.GDALDataset::FlushCache();

    if( !obj.m_bNeedsFlush || obj.m_bWritable == FALSE)
        return;

    obj.m_bNeedsFlush = FALSE;

    // We don't write to disk if there is no filename.  This is a
    // memory only dataset.
    if( strlen(obj.GetDescription()) == 0
        || STARTS_WITH_CI(obj.GetDescription(), "<VRTDataset") )
        return;

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    VSILFILE *fpVRT = VSIFOpenL( obj.GetDescription(), "w" );
    if( fpVRT == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Failed to write .vrt file in FlushCache()." );
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Convert tree to a single block of XML text.                     */
    /* -------------------------------------------------------------------- */
    const char* pszDescription = obj.GetDescription();
    char *l_pszVRTPath = CPLStrdup(
        pszDescription[0] && !STARTS_WITH(pszDescription, "<VRTDataset") ?
            CPLGetPath(pszDescription): "" );
    CPLXMLNode *psDSTree = obj.T::SerializeToXML( l_pszVRTPath );
    char *pszXML = CPLSerializeXMLTree( psDSTree );

    CPLDestroyXMLNode( psDSTree );

    CPLFree( l_pszVRTPath );
    bool bOK = true;
    if( pszXML )
    {
        /* ------------------------------------------------------------------ */
        /*      Write to disk.                                                */
        /* ------------------------------------------------------------------ */
        bOK &=
            VSIFWriteL( pszXML, 1, strlen(pszXML), fpVRT )
            == strlen(pszXML);
        CPLFree(pszXML);
    }
    if( VSIFCloseL( fpVRT ) != 0 )
        bOK = false;
    if( !bOK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Failed to write .vrt file in FlushCache()." );
        return;
    }
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char** VRTDataset::GetMetadata( const char *pszDomain )
{
    if( pszDomain != nullptr && EQUAL(pszDomain, "xml:VRT") )
    {
        /* ------------------------------------------------------------------ */
        /*      Convert tree to a single block of XML text.                   */
        /* ------------------------------------------------------------------ */
        const char* pszDescription = GetDescription();
        char *l_pszVRTPath = CPLStrdup(
            pszDescription[0] && !STARTS_WITH(pszDescription, "<VRTDataset") ?
                CPLGetPath(pszDescription): "" );
        CPLXMLNode *psDSTree = SerializeToXML( l_pszVRTPath );
        char *pszXML = CPLSerializeXMLTree( psDSTree );

        CPLDestroyXMLNode( psDSTree );

        CPLFree( l_pszVRTPath );

        CSLDestroy(m_papszXMLVRTMetadata);
        m_papszXMLVRTMetadata = static_cast<char**>(CPLMalloc(2 * sizeof(char*)));
        m_papszXMLVRTMetadata[0] = pszXML;
        m_papszXMLVRTMetadata[1] = nullptr;
        return m_papszXMLVRTMetadata;
    }

    return GDALDataset::GetMetadata(pszDomain);
}

/*! @endcond */

/************************************************************************/
/*                            VRTFlushCache()                           */
/************************************************************************/

/**
 * @see VRTDataset::FlushCache()
 */

void CPL_STDCALL VRTFlushCache( VRTDatasetH hDataset )
{
    VALIDATE_POINTER0( hDataset, "VRTFlushCache" );

    reinterpret_cast<VRTDataset *>( hDataset )->FlushCache();
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTDataset::SerializeToXML( const char *pszVRTPathIn )

{
    if( m_poRootGroup )
        return m_poRootGroup->SerializeToXML(pszVRTPathIn);

    /* -------------------------------------------------------------------- */
    /*      Setup root node and attributes.                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psDSTree = CPLCreateXMLNode( nullptr, CXT_Element, "VRTDataset" );

    char szNumber[128] = { '\0' };
    snprintf( szNumber, sizeof(szNumber), "%d", GetRasterXSize() );
    CPLSetXMLValue( psDSTree, "#rasterXSize", szNumber );

    snprintf( szNumber, sizeof(szNumber), "%d", GetRasterYSize() );
    CPLSetXMLValue( psDSTree, "#rasterYSize", szNumber );

 /* -------------------------------------------------------------------- */
 /*      SRS                                                             */
 /* -------------------------------------------------------------------- */
    if( m_poSRS && !m_poSRS->IsEmpty() )
    {
        char* pszWKT = nullptr;
        m_poSRS->exportToWkt(&pszWKT);
        CPLXMLNode* psSRSNode = CPLCreateXMLElementAndValue( psDSTree, "SRS", pszWKT );
        CPLFree(pszWKT);
        const auto& mapping = m_poSRS->GetDataAxisToSRSAxisMapping();
        CPLString osMapping;
        for( size_t i = 0; i < mapping.size(); ++i )
        {
            if( !osMapping.empty() )
                osMapping += ",";
            osMapping += CPLSPrintf("%d", mapping[i]);
        }
        CPLAddXMLAttributeAndValue(psSRSNode, "dataAxisToSRSAxisMapping",
                                   osMapping.c_str());
    }

 /* -------------------------------------------------------------------- */
 /*      Geotransform.                                                   */
 /* -------------------------------------------------------------------- */
    if( m_bGeoTransformSet )
    {
        CPLSetXMLValue(
            psDSTree, "GeoTransform",
            CPLSPrintf( "%24.16e,%24.16e,%24.16e,%24.16e,%24.16e,%24.16e",
                        m_adfGeoTransform[0],
                        m_adfGeoTransform[1],
                        m_adfGeoTransform[2],
                        m_adfGeoTransform[3],
                        m_adfGeoTransform[4],
                        m_adfGeoTransform[5] ) );
    }

/* -------------------------------------------------------------------- */
/*      Metadata                                                        */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMD = oMDMD.Serialize();
    if( psMD != nullptr )
    {
        CPLAddXMLChild( psDSTree, psMD );
    }

 /* -------------------------------------------------------------------- */
 /*      GCPs                                                            */
 /* -------------------------------------------------------------------- */
    if( m_nGCPCount > 0 )
    {
        GDALSerializeGCPListToXML( psDSTree,
                                   m_pasGCPList,
                                   m_nGCPCount,
                                   m_poGCP_SRS );
    }

    /* -------------------------------------------------------------------- */
    /*      Serialize bands.                                                */
    /* -------------------------------------------------------------------- */
    CPLXMLNode* psLastChild = psDSTree->psChild;
    for( ; psLastChild != nullptr && psLastChild->psNext;
                                    psLastChild = psLastChild->psNext )
    {
    }
    CPLAssert(psLastChild); // we have at least rasterXSize
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        CPLXMLNode *psBandTree =
            reinterpret_cast<VRTRasterBand *>(
                papoBands[iBand])->SerializeToXML( pszVRTPathIn );

        if( psBandTree != nullptr )
        {
            psLastChild->psNext = psBandTree;
            psLastChild = psBandTree;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Serialize dataset mask band.                                    */
    /* -------------------------------------------------------------------- */
    if( m_poMaskBand )
    {
        CPLXMLNode *psBandTree =
            m_poMaskBand->SerializeToXML(pszVRTPathIn);

        if( psBandTree != nullptr )
        {
            CPLXMLNode *psMaskBandElement
                = CPLCreateXMLNode( psDSTree, CXT_Element, "MaskBand" );
            CPLAddXMLChild( psMaskBandElement, psBandTree );
        }
    }

    return psDSTree;
}

/*! @endcond */
/************************************************************************/
/*                          VRTSerializeToXML()                         */
/************************************************************************/

/**
 * @see VRTDataset::SerializeToXML()
 */

CPLXMLNode * CPL_STDCALL VRTSerializeToXML( VRTDatasetH hDataset,
                                            const char *pszVRTPath )
{
    VALIDATE_POINTER1( hDataset, "VRTSerializeToXML", nullptr );

    return reinterpret_cast<VRTDataset *>(
        hDataset )->SerializeToXML(pszVRTPath);
}
/*! @cond Doxygen_Suppress */


/************************************************************************/
/*                             InitBand()                               */
/************************************************************************/

VRTRasterBand* VRTDataset::InitBand(const char* pszSubclass, int nBand,
                                    bool bAllowPansharpened)
{
    VRTRasterBand  *poBand = nullptr;
    if( EQUAL(pszSubclass,"VRTSourcedRasterBand") )
        poBand = new VRTSourcedRasterBand( this, nBand );
    else if( EQUAL(pszSubclass, "VRTDerivedRasterBand") )
        poBand = new VRTDerivedRasterBand( this, nBand );
    else if( EQUAL(pszSubclass, "VRTRawRasterBand") )
        poBand = new VRTRawRasterBand( this, nBand );
    else if( EQUAL(pszSubclass, "VRTWarpedRasterBand") &&
                dynamic_cast<VRTWarpedDataset*>(this) != nullptr )
        poBand = new VRTWarpedRasterBand( this, nBand );
    else if( bAllowPansharpened &&
             EQUAL(pszSubclass, "VRTPansharpenedRasterBand") &&
                dynamic_cast<VRTPansharpenedDataset*>(this) != nullptr )
        poBand = new VRTPansharpenedRasterBand( this, nBand);
    else
        CPLError( CE_Failure, CPLE_AppDefined,
                    "VRTRasterBand of unrecognized subclass '%s'.",
                    pszSubclass );
    return poBand;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTDataset::XMLInit( CPLXMLNode *psTree, const char *pszVRTPathIn )

{
    if( pszVRTPathIn != nullptr )
        m_pszVRTPath = CPLStrdup(pszVRTPathIn);

/* -------------------------------------------------------------------- */
/*      Check for an SRS node.                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode* psSRSNode = CPLGetXMLNode(psTree, "SRS");
    if( psSRSNode )
    {
        if( m_poSRS )
            m_poSRS->Release();
        m_poSRS = new OGRSpatialReference();
        m_poSRS->SetFromUserInput( CPLGetXMLValue(psSRSNode, nullptr, "") );
        const char* pszMapping =
            CPLGetXMLValue(psSRSNode, "dataAxisToSRSAxisMapping", nullptr);
        if( pszMapping )
        {
            char** papszTokens = CSLTokenizeStringComplex( pszMapping, ",", FALSE, FALSE);
            std::vector<int> anMapping;
            for( int i = 0; papszTokens && papszTokens[i]; i++ )
            {
                anMapping.push_back(atoi(papszTokens[i]));
            }
            CSLDestroy(papszTokens);
            m_poSRS->SetDataAxisToSRSAxisMapping(anMapping);
        }
        else
        {
            m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for a GeoTransform node.                                  */
/* -------------------------------------------------------------------- */
    if( strlen(CPLGetXMLValue(psTree, "GeoTransform", "")) > 0 )
    {
        const char *pszGT = CPLGetXMLValue(psTree, "GeoTransform", "");
        char **papszTokens
            = CSLTokenizeStringComplex( pszGT, ",", FALSE, FALSE );
        if( CSLCount(papszTokens) != 6 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "GeoTransform node does not have expected six values.");
        }
        else
        {
            for( int iTA = 0; iTA < 6; iTA++ )
                m_adfGeoTransform[iTA] = CPLAtof(papszTokens[iTA]);
            m_bGeoTransformSet = TRUE;
        }

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Check for GCPs.                                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode( psTree, "GCPList" );

    if( psGCPList != nullptr )
    {
        GDALDeserializeGCPListFromXML( psGCPList,
                                       &m_pasGCPList,
                                       &m_nGCPCount,
                                       &m_poGCP_SRS );
    }

/* -------------------------------------------------------------------- */
/*      Apply any dataset level metadata.                               */
/* -------------------------------------------------------------------- */
    oMDMD.XMLInit( psTree, TRUE );

/* -------------------------------------------------------------------- */
/*      Create dataset mask band.                                       */
/* -------------------------------------------------------------------- */

    /* Parse dataset mask band first */
    CPLXMLNode* psMaskBandNode = CPLGetXMLNode(psTree, "MaskBand");

    CPLXMLNode *psChild = nullptr;
    if( psMaskBandNode )
        psChild = psMaskBandNode->psChild;
    else
        psChild = nullptr;

    for( ; psChild != nullptr; psChild=psChild->psNext )
    {
        if( psChild->eType == CXT_Element
            && EQUAL(psChild->pszValue,"VRTRasterBand") )
        {
            const char *pszSubclass = CPLGetXMLValue( psChild, "subclass",
                                                      "VRTSourcedRasterBand" );

            VRTRasterBand  *poBand = InitBand(pszSubclass, 0, false);
            if( poBand != nullptr
                && poBand->XMLInit( psChild, pszVRTPathIn, this,
                                    m_oMapSharedSources ) == CE_None )
            {
                SetMaskBand(poBand);
                break;
            }
            else
            {
                delete poBand;
                return CE_Failure;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int l_nBands = 0;
    for( psChild=psTree->psChild; psChild != nullptr; psChild=psChild->psNext )
    {
        if( psChild->eType == CXT_Element
            && EQUAL(psChild->pszValue,"VRTRasterBand") )
        {
            const char *pszSubclass = CPLGetXMLValue( psChild, "subclass",
                                                      "VRTSourcedRasterBand" );

            VRTRasterBand  *poBand = InitBand(pszSubclass, l_nBands+1, true);
            if( poBand != nullptr
                && poBand->XMLInit( psChild, pszVRTPathIn, this,
                                    m_oMapSharedSources ) == CE_None )
            {
                l_nBands ++;
                SetBand( l_nBands, poBand );
            }
            else
            {
                delete poBand;
                return CE_Failure;
            }
        }
    }

    CPLXMLNode* psGroup = CPLGetXMLNode(psTree, "Group");
    if( psGroup )
    {
        const char* pszName = CPLGetXMLValue(psGroup, "name", nullptr);
        if( pszName == nullptr || !EQUAL(pszName, "/") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing name or not equal to '/'");
            return CE_Failure;
        }

        m_poRootGroup = std::make_shared<VRTGroup>(std::string(), "/");
        m_poRootGroup->SetIsRootGroup();
        if( !m_poRootGroup->XMLInit( m_poRootGroup, m_poRootGroup,
                                     psGroup, pszVRTPathIn ) )
        {
            return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int VRTDataset::GetGCPCount()

{
    return m_nGCPCount;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *VRTDataset::GetGCPs()

{
    return m_pasGCPList;
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr VRTDataset::SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                             const OGRSpatialReference* poGCP_SRS )

{
    if( m_poGCP_SRS )
        m_poGCP_SRS->Release();
    if( m_nGCPCount > 0 )
    {
        GDALDeinitGCPs( m_nGCPCount, m_pasGCPList );
        CPLFree( m_pasGCPList );
    }

    m_poGCP_SRS = poGCP_SRS ? poGCP_SRS->Clone(): nullptr;

    m_nGCPCount = nGCPCountIn;

    m_pasGCPList = GDALDuplicateGCPs( nGCPCountIn, pasGCPListIn );

    m_bNeedsFlush = TRUE;

    return CE_None;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr VRTDataset::SetSpatialRef(const OGRSpatialReference* poSRS)

{
    if( m_poSRS )
        m_poSRS->Release();
    if( poSRS )
        m_poSRS = poSRS->Clone();
    else
        m_poSRS = nullptr;

    m_bNeedsFlush = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr VRTDataset::SetGeoTransform( double *padfGeoTransformIn )

{
    memcpy( m_adfGeoTransform, padfGeoTransformIn, sizeof(double) * 6 );
    m_bGeoTransformSet = TRUE;

    m_bNeedsFlush = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr VRTDataset::GetGeoTransform( double * padfGeoTransform )

{
    memcpy( padfGeoTransform, m_adfGeoTransform, sizeof(double) * 6 );

    return m_bGeoTransformSet ? CE_None : CE_Failure;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTDataset::SetMetadata( char **papszMetadata,
                                const char *pszDomain )

{
    SetNeedsFlush();

    return GDALDataset::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr VRTDataset::SetMetadataItem( const char *pszName,
                                    const char *pszValue,
                                    const char *pszDomain )

{
    SetNeedsFlush();

    return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int VRTDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes > 20
         && strstr((const char *)poOpenInfo->pabyHeader,"<VRTDataset") != nullptr )
        return TRUE;

    if( strstr(poOpenInfo->pszFilename,"<VRTDataset") != nullptr )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *VRTDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Does this appear to be a virtual dataset definition XML         */
/*      file?                                                           */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Try to read the whole file into memory.                         */
/* -------------------------------------------------------------------- */
    char *pszXML = nullptr;
    VSILFILE *fp = poOpenInfo->fpL;

    char *pszVRTPath = nullptr;
    if( fp != nullptr )
    {
        poOpenInfo->fpL = nullptr;

        GByte* pabyOut = nullptr;
        if( !VSIIngestFile( fp, poOpenInfo->pszFilename, &pabyOut,
                            nullptr, INT_MAX - 1 ) )
        {
            CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return nullptr;
        }
        pszXML = reinterpret_cast<char*>(pabyOut);

        char* pszCurDir = CPLGetCurrentDir();
        const char *currentVrtFilename
            = CPLProjectRelativeFilename(pszCurDir, poOpenInfo->pszFilename);
        CPLString osInitialCurrentVrtFilename(currentVrtFilename);
        CPLFree(pszCurDir);

#if defined(HAVE_READLINK) && defined(HAVE_LSTAT)
        char filenameBuffer[2048];

        while( true ) {
            VSIStatBuf statBuffer;
            int lstatCode = lstat( currentVrtFilename, &statBuffer );
            if( lstatCode == -1 ) {
                if( errno == ENOENT )
                {
                    // File could be a virtual file, let later checks handle it.
                    break;
                }
                else
                {
                    CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                    CPLFree( pszXML );
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Failed to lstat %s: %s",
                              currentVrtFilename,
                              VSIStrerror(errno) );
                    return nullptr;
                }
            }

            if( !VSI_ISLNK(statBuffer.st_mode) ) {
                break;
            }

            const int bufferSize = static_cast<int>(
                readlink( currentVrtFilename,
                          filenameBuffer,
                          sizeof(filenameBuffer) ) );
            if( bufferSize != -1 )
            {
                filenameBuffer[std::min(bufferSize, static_cast<int>(
                    sizeof(filenameBuffer) ) - 1)] = 0;
                // The filename in filenameBuffer might be a relative path
                // from the linkfile resolve it before looping
                currentVrtFilename = CPLProjectRelativeFilename(
                    CPLGetDirname( currentVrtFilename ), filenameBuffer);
            }
            else
            {
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                CPLFree( pszXML );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to read filename from symlink %s: %s",
                          currentVrtFilename,
                          VSIStrerror(errno) );
                return nullptr;
            }
        }
#endif  // HAVE_READLINK && HAVE_LSTAT

        if( osInitialCurrentVrtFilename == currentVrtFilename )
            pszVRTPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
        else
            pszVRTPath = CPLStrdup(CPLGetPath(currentVrtFilename));

        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    }
/* -------------------------------------------------------------------- */
/*      Or use the filename as the XML input.                           */
/* -------------------------------------------------------------------- */
    else
    {
        pszXML = CPLStrdup( poOpenInfo->pszFilename );
    }

    if( CSLFetchNameValue(poOpenInfo->papszOpenOptions, "ROOT_PATH") != nullptr )
    {
        CPLFree(pszVRTPath);
        pszVRTPath = CPLStrdup(
            CSLFetchNameValue( poOpenInfo->papszOpenOptions, "ROOT_PATH" ) );
    }

/* -------------------------------------------------------------------- */
/*      Turn the XML representation into a VRTDataset.                  */
/* -------------------------------------------------------------------- */
    VRTDataset *poDS = reinterpret_cast<VRTDataset *>(
        OpenXML( pszXML, pszVRTPath, poOpenInfo->eAccess ) );

    if( poDS != nullptr )
        poDS->m_bNeedsFlush = FALSE;

    if( poDS != nullptr )
    {
        if( poDS->GetRasterCount() == 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) == 0 &&
            strstr(pszXML, "VRTPansharpenedDataset") == nullptr )
        {
            delete poDS;
            poDS = nullptr;
        }
        else if( poDS->GetRootGroup() == nullptr &&
            (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
            (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) != 0 )
        {
            delete poDS;
            poDS = nullptr;
        }
    }

    CPLFree( pszXML );
    CPLFree( pszVRTPath );

/* -------------------------------------------------------------------- */
/*      Initialize info for later overview discovery.                   */
/* -------------------------------------------------------------------- */
    if( fp != nullptr && poDS != nullptr )
    {
        poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
        if( poOpenInfo->AreSiblingFilesLoaded() )
            poDS->oOvManager.TransferSiblingFiles(
                poOpenInfo->StealSiblingFiles() );
    }

    if( poDS && poDS->eAccess == GA_Update &&
        poDS->m_poRootGroup && !STARTS_WITH_CI(poOpenInfo->pszFilename, "<VRT") )
    {
        poDS->m_poRootGroup->SetFilename(poOpenInfo->pszFilename);
    }

    return poDS;
}

/************************************************************************/
/*                              OpenXML()                               */
/*                                                                      */
/*      Create an open VRTDataset from a supplied XML representation    */
/*      of the dataset.                                                 */
/************************************************************************/

GDALDataset *VRTDataset::OpenXML( const char *pszXML, const char *pszVRTPath,
                                  GDALAccess eAccess)

{
 /* -------------------------------------------------------------------- */
 /*      Parse the XML.                                                  */
 /* -------------------------------------------------------------------- */
    CPLXMLTreeCloser psTree(CPLParseXMLString( pszXML ));
    if( psTree == nullptr )
        return nullptr;

    CPLXMLNode *psRoot = CPLGetXMLNode( psTree.get(), "=VRTDataset" );
    if( psRoot == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Missing VRTDataset element." );
        return nullptr;
    }

    const char* pszSubClass = CPLGetXMLValue(psRoot, "subClass", "");

    const bool bIsPansharpened =
        strcmp(pszSubClass, "VRTPansharpenedDataset" ) == 0;

    if( !bIsPansharpened &&
        CPLGetXMLNode( psRoot, "Group" ) == nullptr &&
        (CPLGetXMLNode( psRoot, "rasterXSize" ) == nullptr
        || CPLGetXMLNode( psRoot, "rasterYSize" ) == nullptr
        || CPLGetXMLNode( psRoot, "VRTRasterBand" ) == nullptr) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Missing one of rasterXSize, rasterYSize or bands on"
                  " VRTDataset." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create the new virtual dataset object.                          */
/* -------------------------------------------------------------------- */
    const int nXSize = atoi(CPLGetXMLValue(psRoot, "rasterXSize","0"));
    const int nYSize = atoi(CPLGetXMLValue(psRoot, "rasterYSize","0"));

    if( !bIsPansharpened &&
        CPLGetXMLNode( psRoot, "VRTRasterBand" ) != nullptr &&
        !GDALCheckDatasetDimensions( nXSize, nYSize ) )
    {
        return nullptr;
    }

    VRTDataset *poDS = nullptr;
    if( strcmp(pszSubClass, "VRTWarpedDataset") == 0 )
        poDS = new VRTWarpedDataset( nXSize, nYSize );
    else if( bIsPansharpened )
        poDS = new VRTPansharpenedDataset( nXSize, nYSize );
    else
    {
        poDS = new VRTDataset( nXSize, nYSize );
        poDS->eAccess = eAccess;
    }

    if( poDS->XMLInit( psRoot, pszVRTPath ) != CE_None )
    {
        delete poDS;
        poDS = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */

    return poDS;
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr VRTDataset::AddBand( GDALDataType eType, char **papszOptions )

{
    m_bNeedsFlush = TRUE;

/* ==================================================================== */
/*      Handle a new raw band.                                          */
/* ==================================================================== */
    const char *pszSubClass = CSLFetchNameValue(papszOptions, "subclass");

    if( pszSubClass != nullptr && EQUAL(pszSubClass,"VRTRawRasterBand") )
    {
        const int nWordDataSize = GDALGetDataTypeSizeBytes( eType );

/* -------------------------------------------------------------------- */
/*      Collect required information.                                   */
/* -------------------------------------------------------------------- */
        const char* pszImageOffset =
            CSLFetchNameValueDef(papszOptions, "ImageOffset", "0");
        vsi_l_offset nImageOffset = CPLScanUIntBig(
            pszImageOffset, static_cast<int>(strlen(pszImageOffset)) );

        int nPixelOffset = nWordDataSize;
        const char* pszPixelOffset =
                            CSLFetchNameValue(papszOptions, "PixelOffset");
        if( pszPixelOffset != nullptr )
            nPixelOffset = atoi(pszPixelOffset);

        int nLineOffset;
        const char* pszLineOffset = 
                                CSLFetchNameValue(papszOptions, "LineOffset");
        if( pszLineOffset != nullptr )
            nLineOffset = atoi(pszLineOffset);
        else
        {
            if( nPixelOffset > INT_MAX / GetRasterXSize() ||
                nPixelOffset < INT_MIN / GetRasterXSize() )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Int overflow");
                return CE_Failure;
            }
            nLineOffset = nPixelOffset * GetRasterXSize();
        }

        const char *pszByteOrder =
                                CSLFetchNameValue(papszOptions, "ByteOrder");

        const char *pszFilename =
                            CSLFetchNameValue(papszOptions, "SourceFilename");
        if( pszFilename == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "AddBand() requires a SourceFilename option for "
                      "VRTRawRasterBands." );
            return CE_Failure;
        }

        const bool bRelativeToVRT =
            CPLFetchBool( papszOptions, "relativeToVRT", false );

/* -------------------------------------------------------------------- */
/*      Create and initialize the band.                                 */
/* -------------------------------------------------------------------- */

        VRTRawRasterBand *poBand =
            new VRTRawRasterBand( this, GetRasterCount() + 1, eType );

        char* l_pszVRTPath = CPLStrdup(CPLGetPath(GetDescription()));
        if( EQUAL(l_pszVRTPath, "") )
        {
            CPLFree(l_pszVRTPath);
            l_pszVRTPath = nullptr;
        }

        const CPLErr eErr =
            poBand->SetRawLink( pszFilename, l_pszVRTPath, bRelativeToVRT,
                                nImageOffset, nPixelOffset, nLineOffset,
                                pszByteOrder );
        CPLFree(l_pszVRTPath);
        if( eErr != CE_None )
        {
            delete poBand;
            return eErr;
        }

        SetBand( GetRasterCount() + 1, poBand );

        return CE_None;
    }

/* ==================================================================== */
/*      Handle a new "sourced" band.                                    */
/* ==================================================================== */
    else
    {
        VRTSourcedRasterBand *poBand = nullptr;

        /* ---- Check for our sourced band 'derived' subclass ---- */
        if(pszSubClass != nullptr && EQUAL(pszSubClass,"VRTDerivedRasterBand")) {

            /* We'll need a pointer to the subclass in case we need */
            /* to set the new band's pixel function below. */
            VRTDerivedRasterBand* poDerivedBand = new VRTDerivedRasterBand(
                this, GetRasterCount() + 1, eType,
                GetRasterXSize(), GetRasterYSize());

            /* Set the pixel function options it provided. */
            const char* pszFuncName =
                CSLFetchNameValue(papszOptions, "PixelFunctionType");
            if( pszFuncName != nullptr )
                poDerivedBand->SetPixelFunctionName(pszFuncName);

            const char* pszLanguage =
                CSLFetchNameValue(papszOptions, "PixelFunctionLanguage");
            if( pszLanguage != nullptr )
                poDerivedBand->SetPixelFunctionLanguage(pszLanguage);

            const char* pszTransferTypeName =
                CSLFetchNameValue(papszOptions, "SourceTransferType");
            if( pszTransferTypeName != nullptr )
            {
                const GDALDataType eTransferType =
                    GDALGetDataTypeByName(pszTransferTypeName);
                if( eTransferType == GDT_Unknown )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "invalid SourceTransferType: \"%s\".",
                              pszTransferTypeName);
                    delete poDerivedBand;
                    return CE_Failure;
                }
                poDerivedBand->SetSourceTransferType(eTransferType);
            }

            /* We're done with the derived band specific stuff, so */
            /* we can assigned the base class pointer now. */
            poBand = poDerivedBand;
        }
        else {
            /* ---- Standard sourced band ---- */
            poBand = new VRTSourcedRasterBand(
                this, GetRasterCount() + 1, eType,
                GetRasterXSize(), GetRasterYSize());
        }

        SetBand( GetRasterCount() + 1, poBand );

        for( int i=0; papszOptions != nullptr && papszOptions[i] != nullptr; i++ )
        {
            if( STARTS_WITH_CI(papszOptions[i], "AddFuncSource=") )
            {
                char **papszTokens
                    = CSLTokenizeStringComplex( papszOptions[i]+14,
                                                ",", TRUE, FALSE );
                if( CSLCount(papszTokens) < 1 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "AddFuncSource(): required argument missing." );
                    // TODO: How should this error be handled?  Return
                    // CE_Failure?
                }

                VRTImageReadFunc pfnReadFunc = nullptr;
                sscanf( papszTokens[0], "%p", &pfnReadFunc );

                void *pCBData = nullptr;
                if( CSLCount(papszTokens) > 1 )
                    sscanf( papszTokens[1], "%p", &pCBData );

                const double dfNoDataValue =
                    ( CSLCount(papszTokens) > 2 ) ?
                    CPLAtof( papszTokens[2] ) : VRT_NODATA_UNSET;

                poBand->AddFuncSource( pfnReadFunc, pCBData, dfNoDataValue );

                CSLDestroy( papszTokens );
            }
        }

        return CE_None;
    }
}
/*! @endcond */
/************************************************************************/
/*                              VRTAddBand()                            */
/************************************************************************/

/**
 * @see VRTDataset::VRTAddBand().
 *
 * @note The return type of this function is int, but the actual values
 * returned are of type CPLErr.
 */

int CPL_STDCALL VRTAddBand( VRTDatasetH hDataset, GDALDataType eType,
                            char **papszOptions )

{
    VALIDATE_POINTER1( hDataset, "VRTAddBand", 0 );

    return reinterpret_cast<VRTDataset *>(
        hDataset )->AddBand( eType, papszOptions );
}
/*! @cond Doxygen_Suppress */
/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *
VRTDataset::Create( const char * pszName,
                    int nXSize, int nYSize, int nBands,
                    GDALDataType eType, char ** papszOptions )

{
    if( STARTS_WITH_CI(pszName, "<VRTDataset") )
    {
        GDALDataset *poDS = OpenXML( pszName, nullptr, GA_Update );
        if(  poDS != nullptr )
            poDS->SetDescription( "<FromXML>" );
        return poDS;
    }

    const char *pszSubclass = CSLFetchNameValue( papszOptions,
                                                 "SUBCLASS" );

    VRTDataset *poDS = nullptr;

    if( pszSubclass == nullptr || EQUAL(pszSubclass,"VRTDataset") )
        poDS = new VRTDataset( nXSize, nYSize );
    else if( EQUAL(pszSubclass,"VRTWarpedDataset") )
    {
        poDS = new VRTWarpedDataset( nXSize, nYSize );
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "SUBCLASS=%s not recognised.",
                  pszSubclass );
        return nullptr;
    }
    poDS->eAccess = GA_Update;

    poDS->SetDescription( pszName );

    for( int iBand = 0; iBand < nBands; iBand++ )
        poDS->AddBand( eType, nullptr );

    poDS->m_bNeedsFlush = TRUE;

    poDS->oOvManager.Initialize( poDS, pszName );

    return poDS;
}


/************************************************************************/
/*                     CreateMultiDimensional()                         */
/************************************************************************/

GDALDataset * VRTDataset::CreateMultiDimensional( const char * pszFilename,
                                                  CSLConstList /*papszRootGroupOptions*/,
                                                  CSLConstList /*papszOptions*/ )
{
    VRTDataset *poDS = new VRTDataset( 0, 0 );
    poDS->eAccess = GA_Update;
    poDS->SetDescription(pszFilename);
    poDS->m_poRootGroup = std::make_shared<VRTGroup>(std::string(), "/");
    poDS->m_poRootGroup->SetIsRootGroup();
    poDS->m_poRootGroup->SetFilename(pszFilename);
    poDS->m_poRootGroup->SetDirty();

    return poDS;
}


/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** VRTDataset::GetFileList()
{
    char** papszFileList = GDALDataset::GetFileList();

    int nSize = CSLCount(papszFileList);
    int nMaxSize = nSize;

    // Do not need an element deallocator as each string points to an
    // element of the papszFileList.
    CPLHashSet* hSetFiles = CPLHashSetNew(CPLHashSetHashStr,
                                          CPLHashSetEqualStr,
                                          nullptr);

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
      reinterpret_cast<VRTRasterBand *>(
          papoBands[iBand])->GetFileList(
              &papszFileList, &nSize, &nMaxSize, hSetFiles );
    }

    CPLHashSetDestroy(hSetFiles);

    return papszFileList;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

/* We implement Delete() to avoid that the default implementation */
/* in GDALDriver::Delete() destroys the source files listed by GetFileList(),*/
/* which would be an undesired effect... */
CPLErr VRTDataset::Delete( const char * pszFilename )
{
    GDALDriverH hDriver = GDALIdentifyDriver(pszFilename, nullptr);

    if( !hDriver || !EQUAL( GDALGetDriverShortName(hDriver), "VRT" ) )
        return CE_Failure;

    if( strstr(pszFilename, "<VRTDataset") == nullptr &&
        VSIUnlink( pszFilename ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Deleting %s failed:\n%s",
                  pszFilename,
                  VSIStrerror(errno) );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          CreateMaskBand()                            */
/************************************************************************/

CPLErr VRTDataset::CreateMaskBand( int )
{
    if( m_poMaskBand != nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This VRT dataset has already a mask band");
        return CE_Failure;
    }

    SetMaskBand(new VRTSourcedRasterBand( this, 0 ));

    return CE_None;
}

/************************************************************************/
/*                           SetMaskBand()                              */
/************************************************************************/

void VRTDataset::SetMaskBand(VRTRasterBand* poMaskBandIn)
{
    delete m_poMaskBand;
    m_poMaskBand = poMaskBandIn;
    m_poMaskBand->SetIsMaskBand();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTDataset::CloseDependentDatasets()
{
    /* We need to call it before removing the sources, otherwise */
    /* we would remove them from the serizalized VRT */
    FlushCache();

    int bHasDroppedRef = GDALDataset::CloseDependentDatasets();

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        bHasDroppedRef |= reinterpret_cast<VRTRasterBand *>(
            papoBands[iBand] )->CloseDependentDatasets();
    }

    return bHasDroppedRef;
}

/************************************************************************/
/*                      CheckCompatibleForDatasetIO()                   */
/************************************************************************/

/* We will return TRUE only if all the bands are VRTSourcedRasterBands */
/* made of identical sources, that are strictly VRTSimpleSource, and that */
/* the band number of each source is the band number of the */
/* VRTSourcedRasterBand. */

int VRTDataset::CheckCompatibleForDatasetIO()
{
    int nSources = 0;
    VRTSource **papoSources = nullptr;
    CPLString osResampling;

    if( m_bCompatibleForDatasetIO >= 0 )
    {
        return m_bCompatibleForDatasetIO;
    }

    for(int iBand = 0; iBand < nBands; iBand++)
    {
        if( !reinterpret_cast<VRTRasterBand *>(
                papoBands[iBand] )->IsSourcedRasterBand() )
            return FALSE;

        VRTSourcedRasterBand* poBand
            = reinterpret_cast<VRTSourcedRasterBand*>( papoBands[iBand] );

        // Do not allow VRTDerivedRasterBand for example
        if( typeid(*poBand) != typeid(VRTSourcedRasterBand) )
            return FALSE;

        if( iBand == 0 )
        {
            nSources = poBand->nSources;
            papoSources = poBand->papoSources;
            for(int iSource = 0; iSource < nSources; iSource++)
            {
                if( !papoSources[iSource]->IsSimpleSource() )
                    return FALSE;

                VRTSimpleSource* poSource = reinterpret_cast<VRTSimpleSource *>(
                    papoSources[iSource] );
                if( !EQUAL(poSource->GetType(), "SimpleSource") )
                    return FALSE;

                GDALRasterBand *srcband = poSource->GetBand();
                if( srcband == nullptr )
                    return FALSE;
                if( srcband->GetDataset() == nullptr )
                    return FALSE;
                if( srcband->GetDataset()->GetRasterCount() <= iBand )
                    return FALSE;
                if( srcband->GetDataset()->GetRasterBand(iBand + 1) != srcband )
                    return FALSE;
                osResampling = poSource->GetResampling();
            }
        }
        else if( nSources != poBand->nSources )
        {
            return FALSE;
        }
        else
        {
            for(int iSource = 0; iSource < nSources; iSource++)
            {
                if( !poBand->papoSources[iSource]->IsSimpleSource() )
                    return FALSE;
                VRTSimpleSource* poRefSource
                    = reinterpret_cast<VRTSimpleSource *>(
                        papoSources[iSource] );

                VRTSimpleSource* poSource = reinterpret_cast<VRTSimpleSource *>(
                    poBand->papoSources[iSource] );
                if( !EQUAL(poSource->GetType(), "SimpleSource") )
                    return FALSE;
                if( !poSource->IsSameExceptBandNumber(poRefSource) )
                    return FALSE;

                GDALRasterBand *srcband = poSource->GetBand();
                if( srcband == nullptr )
                    return FALSE;
                if( srcband->GetDataset() == nullptr )
                    return FALSE;
                if( srcband->GetDataset()->GetRasterCount() <= iBand )
                    return FALSE;
                if( srcband->GetDataset()->GetRasterBand(iBand + 1) != srcband )
                    return FALSE;
                if( osResampling.compare(poSource->GetResampling()) != 0 )
                    return FALSE;
            }
        }
    }

    return nSources != 0;
}


/************************************************************************/
/*                      ExpandProxyBands()                              */
/************************************************************************/
/* In ProxyPoolDatasets, by default only one band is initialized. When using
 * VRTDataset::IRasterIO and CheckCompatibleForDatasetIO is True, we need to have
 * all bands initialized (but only for the last band in the VRTDataset). This function
 * assumes CheckCompatibleForDatasetIO() has already been run and returned succesfull.
 */
void VRTDataset::ExpandProxyBands()
{
    VRTSourcedRasterBand * poLastBand = reinterpret_cast<VRTSourcedRasterBand*>(papoBands[nBands - 1]);

    CPLAssert(poLastBand != nullptr); // CheckCompatibleForDatasetIO()

    int nSources = poLastBand->nSources;

    for (int iSource = 0; iSource < nSources; iSource++)
    {
        VRTSimpleSource* poSource = reinterpret_cast<VRTSimpleSource *>(poLastBand->papoSources[iSource]);

        CPLAssert(poSource != nullptr); // CheckCompatibleForDatasetIO()

        GDALProxyPoolDataset * dataset = dynamic_cast<GDALProxyPoolDataset *>(poSource->GetBand()->GetDataset());

        if (dataset == nullptr)
        {
            continue; // only GDALProxyPoolDataset needs to be expanded
        }

        if (dataset->GetBands()[0] != nullptr)
        {
            continue; // first band already set, so just assume all the others are set as well
        }

        for (int iBand = 1; iBand <= nBands - 1; iBand++ )
        {
            VRTSourcedRasterBand * srcband = reinterpret_cast<VRTSourcedRasterBand *>(papoBands[iBand - 1]);
            VRTSimpleSource* src = reinterpret_cast<VRTSimpleSource *>(srcband->papoSources[iSource]);
            GDALRasterBand * rasterband = src->GetBand();

            int nBlockXSize, nBlockYSize;

            rasterband->GetBlockSize(&nBlockXSize, &nBlockYSize);

            dataset->AddSrcBand(iBand, rasterband->GetRasterDataType(), nBlockXSize, nBlockYSize);
        }
    }
}




/************************************************************************/
/*                         GetSingleSimpleSource()                      */
/*                                                                      */
/* Returns a non-NULL dataset if the VRT is made of a single source     */
/* that is a simple source, in its full extent, and with all of its     */
/* bands. Basically something produced by :                             */
/*   gdal_translate src dst.vrt -of VRT (-a_srs / -a_ullr)              */
/************************************************************************/

GDALDataset* VRTDataset::GetSingleSimpleSource()
{
    if( !CheckCompatibleForDatasetIO() )
        return nullptr;

    VRTSourcedRasterBand* poVRTBand
        = reinterpret_cast<VRTSourcedRasterBand *>( papoBands[0] );
    if( poVRTBand->nSources != 1 )
        return nullptr;

    VRTSimpleSource* poSource = reinterpret_cast<VRTSimpleSource *>(
        poVRTBand->papoSources[0] );

    GDALRasterBand* poBand = poSource->GetBand();
    if( poBand == nullptr )
        return nullptr;

    GDALDataset* poSrcDS = poBand->GetDataset();
    if( poSrcDS == nullptr )
        return nullptr;

    /* Check that it uses the full source dataset */
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;
    if( !poSource->GetSrcDstWindow(
           0, 0,
           poSrcDS->GetRasterXSize(),
           poSrcDS->GetRasterYSize(),
           poSrcDS->GetRasterXSize(),
           poSrcDS->GetRasterYSize(),
           &dfReqXOff, &dfReqYOff,
           &dfReqXSize, &dfReqYSize,
           &nReqXOff, &nReqYOff,
           &nReqXSize, &nReqYSize,
           &nOutXOff, &nOutYOff,
           &nOutXSize, &nOutYSize ) )
        return nullptr;

    if( nReqXOff != 0 || nReqYOff != 0 ||
        nReqXSize != poSrcDS->GetRasterXSize() ||
        nReqYSize != poSrcDS->GetRasterYSize() )
        return nullptr;

    if( nOutXOff != 0 || nOutYOff != 0 ||
        nOutXSize != poSrcDS->GetRasterXSize() ||
        nOutYSize != poSrcDS->GetRasterYSize() )
        return nullptr;

    return poSrcDS;
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

CPLErr VRTDataset::AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eDT,
                               int nBandCount, int *panBandList,
                               char **papszOptions )
{
    if( !CheckCompatibleForDatasetIO() )
        return CE_None;

    VRTSourcedRasterBand* poVRTBand
        = reinterpret_cast<VRTSourcedRasterBand *>( papoBands[0] );
    if( poVRTBand->nSources != 1 )
        return CE_None;

    VRTSimpleSource* poSource = reinterpret_cast<VRTSimpleSource *>(
        poVRTBand->papoSources[0] );

    GDALRasterBand* poBand = poSource->GetBand();
    if( poBand == nullptr )
        return CE_None;

    GDALDataset* poSrcDS = poBand->GetDataset();
    if( poSrcDS == nullptr )
        return CE_None;

    /* Find source window and buffer size */
    double dfReqXOff = 0.0;
    double dfReqYOff = 0.0;
    double dfReqXSize = 0.0;
    double dfReqYSize = 0.0;
    int nReqXOff = 0;
    int nReqYOff = 0;
    int nReqXSize = 0;
    int nReqYSize = 0;
    int nOutXOff = 0;
    int nOutYOff = 0;
    int nOutXSize = 0;
    int nOutYSize = 0;
    if( !poSource->GetSrcDstWindow(
           nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
           &dfReqXOff, &dfReqYOff,
           &dfReqXSize, &dfReqYSize,
           &nReqXOff, &nReqYOff,
           &nReqXSize, &nReqYSize,
           &nOutXOff, &nOutYOff,
           &nOutXSize, &nOutYSize ) )
        return CE_None;

    return poSrcDS->AdviseRead(nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                               nOutXSize, nOutYSize,
                               eDT, nBandCount, panBandList, papszOptions);
}

/************************************************************************/
/*                              IRasterIO()                             */
/************************************************************************/

CPLErr VRTDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg )
{
    bool bLocalCompatibleForDatasetIO = CPL_TO_BOOL(CheckCompatibleForDatasetIO());
    if( bLocalCompatibleForDatasetIO && eRWFlag == GF_Read &&
        (nBufXSize < nXSize || nBufYSize < nYSize) )
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO( eRWFlag,
                                                 nXOff, nYOff, nXSize, nYSize,
                                                 pData, nBufXSize, nBufYSize,
                                                 eBufType,
                                                 nBandCount, panBandMap,
                                                 nPixelSpace, nLineSpace,
                                                 nBandSpace,
                                                 psExtraArg,
                                                 &bTried );
        if( bTried )
            return eErr;

        for(int iBand = 0; iBand < nBands; iBand++)
        {
            VRTSourcedRasterBand* poBand
                = reinterpret_cast<VRTSourcedRasterBand *>( papoBands[iBand] );

            // If there are overviews, let VRTSourcedRasterBand::IRasterIO()
            // do the job.
            if( poBand->GetOverviewCount() != 0 )
            {
                bLocalCompatibleForDatasetIO = false;
                break;
            }
        }
    }

    // If resampling with non-nearest neighbour, we need to be careful
    // if the VRT band exposes a nodata value, but the sources do not have it
    if( bLocalCompatibleForDatasetIO && eRWFlag == GF_Read &&
        (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
    {
        for( int iBandIndex=0; iBandIndex<nBandCount; iBandIndex++ )
        {
            VRTSourcedRasterBand* poBand =
                reinterpret_cast<VRTSourcedRasterBand*>(
                    GetRasterBand(panBandMap[iBandIndex]) );
            int bHasNoData = FALSE;
            const double dfNoDataValue = poBand->GetNoDataValue(&bHasNoData);
            if( bHasNoData )
            {
                for( int i = 0; i < poBand->nSources; i++ )
                {
                    VRTSimpleSource* poSource
                        = reinterpret_cast<VRTSimpleSource *>(
                            poBand->papoSources[i] );
                    int bSrcHasNoData = FALSE;
                    const double dfSrcNoData
                        = poSource->GetBand()->GetNoDataValue(&bSrcHasNoData);
                    if( !bSrcHasNoData || dfSrcNoData != dfNoDataValue )
                    {
                        bLocalCompatibleForDatasetIO = false;
                        break;
                    }
                }
                if( !bLocalCompatibleForDatasetIO )
                    break;
            }
        }
    }

    if( bLocalCompatibleForDatasetIO && eRWFlag == GF_Read )
    {

        // Make sure the expand the last band before using them below
        ExpandProxyBands();

        for(int iBandIndex=0; iBandIndex<nBandCount; iBandIndex++)
        {
            VRTSourcedRasterBand* poBand
                = reinterpret_cast<VRTSourcedRasterBand *>(
                    GetRasterBand( panBandMap[iBandIndex] ) );

            /* Dirty little trick to initialize the buffer without doing */
            /* any real I/O */
            const int nSavedSources = poBand->nSources;
            poBand->nSources = 0;

            GByte *pabyBandData
                = reinterpret_cast<GByte *>( pData ) + iBandIndex * nBandSpace;

            poBand->IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                                pabyBandData, nBufXSize, nBufYSize,
                                eBufType,
                                nPixelSpace, nLineSpace, psExtraArg);

            poBand->nSources = nSavedSources;
        }

        CPLErr eErr = CE_None;
        GDALProgressFunc  pfnProgressGlobal = psExtraArg->pfnProgress;
        void *pProgressDataGlobal = psExtraArg->pProgressData;

        // Use the last band, because when sources reference a GDALProxyDataset,
        // they don't necessary instantiate all underlying rasterbands.
        VRTSourcedRasterBand* poBand = reinterpret_cast<VRTSourcedRasterBand *>(
            papoBands[nBands - 1] );
        for( int iSource = 0;
             eErr == CE_None && iSource < poBand->nSources;
             iSource++ )
        {
            psExtraArg->pfnProgress = GDALScaledProgress;
            psExtraArg->pProgressData =
                GDALCreateScaledProgress(
                    1.0 * iSource / poBand->nSources,
                    1.0 * (iSource + 1) / poBand->nSources,
                    pfnProgressGlobal,
                    pProgressDataGlobal );

            VRTSimpleSource* poSource = reinterpret_cast<VRTSimpleSource *>(
                poBand->papoSources[iSource] );

            eErr = poSource->DatasetRasterIO( poBand->GetRasterDataType(),
                                              nXOff, nYOff, nXSize, nYSize,
                                              pData, nBufXSize, nBufYSize,
                                              eBufType,
                                              nBandCount, panBandMap,
                                              nPixelSpace, nLineSpace,
                                              nBandSpace,
                                              psExtraArg );

            GDALDestroyScaledProgress( psExtraArg->pProgressData );
        }

        psExtraArg->pfnProgress = pfnProgressGlobal;
        psExtraArg->pProgressData = pProgressDataGlobal;

        return eErr;
    }

    return GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                   pData, nBufXSize, nBufYSize,
                                   eBufType,
                                   nBandCount, panBandMap,
                                   nPixelSpace, nLineSpace, nBandSpace,
                                   psExtraArg );
}

/************************************************************************/
/*                  UnsetPreservedRelativeFilenames()                   */
/************************************************************************/

void VRTDataset::UnsetPreservedRelativeFilenames()
{
    for(int iBand = 0; iBand < nBands; iBand++)
    {
        if( !reinterpret_cast<VRTRasterBand *>(
               papoBands[iBand] )->IsSourcedRasterBand() )
            continue;

        VRTSourcedRasterBand* poBand
            = reinterpret_cast<VRTSourcedRasterBand *>( papoBands[iBand] );
        const int nSources = poBand->nSources;
        VRTSource** papoSources = poBand->papoSources;
        for(int iSource = 0; iSource < nSources; iSource++)
        {
            if( !papoSources[iSource]->IsSimpleSource() )
                continue;

            VRTSimpleSource* poSource = reinterpret_cast<VRTSimpleSource *>(
                papoSources[iSource] );
            poSource->UnsetPreservedRelativeFilenames();
        }
    }
}

/************************************************************************/
/*                        BuildVirtualOverviews()                       */
/************************************************************************/

void VRTDataset::BuildVirtualOverviews()
{
    // Currently we expose virtual overviews only if the dataset is made of
    // a single SimpleSource/ComplexSource, in each band.
    // And if the underlying sources have overviews of course
    if( !m_apoOverviews.empty() || !m_apoOverviewsBak.empty() )
        return;

    int nOverviews = 0;
    GDALRasterBand* poFirstBand = nullptr;
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        if( !reinterpret_cast<VRTRasterBand *>(
               papoBands[iBand] )->IsSourcedRasterBand())
            return;

        VRTSourcedRasterBand* poVRTBand
            = reinterpret_cast<VRTSourcedRasterBand *>( papoBands[iBand] );
        if( poVRTBand->nSources != 1 )
            return;
        if( !poVRTBand->papoSources[0]->IsSimpleSource() )
            return;

        VRTSimpleSource* poSource
            = reinterpret_cast<VRTSimpleSource *>( poVRTBand->papoSources[0] );
        if( !EQUAL(poSource->GetType(), "SimpleSource") &&
            !EQUAL(poSource->GetType(), "ComplexSource") )
            return;
        GDALRasterBand* poSrcBand = poSource->GetBand();
        if( poSrcBand == nullptr )
            return;

        // To prevent recursion
        m_apoOverviewsBak.push_back(nullptr);
        const int nOvrCount = poSrcBand->GetOverviewCount();
        m_apoOverviewsBak.resize(0);

        if( nOvrCount == 0 )
            return;
        if( iBand == 0 )
        {
            if( poSrcBand->GetXSize() == 0 || poSrcBand->GetYSize() == 0 )
                return;
            poFirstBand = poSrcBand;
            nOverviews = nOvrCount;
        }
        else if( nOvrCount < nOverviews )
            nOverviews = nOvrCount;
    }

    for( int j = 0; j < nOverviews; j++)
    {
        auto poOvrBand = poFirstBand->GetOverview(j);
        if( !poOvrBand )
            return;
        const double dfXRatio = static_cast<double>(
            poOvrBand->GetXSize() ) / poFirstBand->GetXSize();
        const double dfYRatio = static_cast<double>(
            poOvrBand->GetYSize() ) / poFirstBand->GetYSize();
        const int nOvrXSize = static_cast<int>(0.5 + nRasterXSize * dfXRatio);
        const int nOvrYSize = static_cast<int>(0.5 + nRasterYSize * dfYRatio);
        if( nOvrXSize < 128 || nOvrYSize < 128 )
            break;
        VRTDataset* poOvrVDS = new VRTDataset(nOvrXSize, nOvrYSize);
        m_apoOverviews.push_back(poOvrVDS);

        for( int i = 0; i < nBands; i++ )
        {
            VRTSourcedRasterBand* poVRTBand
                = reinterpret_cast<VRTSourcedRasterBand *>(
                    GetRasterBand(i+1) );
            VRTSourcedRasterBand* poOvrVRTBand = new VRTSourcedRasterBand(
                poOvrVDS,
                poOvrVDS->GetRasterCount() + 1,
                poVRTBand->GetRasterDataType(),
                nOvrXSize, nOvrYSize);
            poOvrVDS->SetBand( poOvrVDS->GetRasterCount() + 1, poOvrVRTBand );

            VRTSimpleSource* poSrcSource = reinterpret_cast<VRTSimpleSource *>(
                poVRTBand->papoSources[0] );
            VRTSimpleSource* poNewSource = nullptr;
            if( EQUAL(poSrcSource->GetType(), "SimpleSource") )
            {
                poNewSource =
                    new VRTSimpleSource(poSrcSource, dfXRatio, dfYRatio);
            }
            else if( EQUAL(poSrcSource->GetType(), "ComplexSource") )
            {
              poNewSource = new VRTComplexSource(
                  reinterpret_cast<VRTComplexSource *>( poSrcSource ),
                  dfXRatio, dfYRatio );
            }
            else
            {
                CPLAssert(false);
            }
            if( poNewSource )
            {
                if( poNewSource->GetBand()->GetDataset() )
                    poNewSource->GetBand()->GetDataset()->Reference();
                poOvrVRTBand->AddSource(poNewSource);
            }
        }
    }
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr
VRTDataset::IBuildOverviews( const char *pszResampling,
                             int nOverviews,
                             int *panOverviewList,
                             int nListBands,
                             int *panBandList,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData )
{
    // Make implicit overviews invisible, but do not destroy them in case they
    // are already used.  Should the client do that?  Behaviour might undefined
    // in GDAL API?
    if( !m_apoOverviews.empty() )
    {
        m_apoOverviewsBak = m_apoOverviews;
        m_apoOverviews.resize(0);
    }
    else
    {
        // Add a dummy overview so that GDALDataset::IBuildOverviews()
        // doesn't manage to get a virtual implicit overview.
        m_apoOverviews.push_back(nullptr);
    }

    return GDALDataset::IBuildOverviews( pszResampling,
                                         nOverviews,
                                         panOverviewList,
                                         nListBands,
                                         panBandList,
                                         pfnProgress,
                                         pProgressData );
}

/*! @endcond */
