/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTDriver
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_alg_priv.h"
#include "gdal_frmts.h"

CPL_CVSID("$Id$")

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                             VRTDriver()                              */
/************************************************************************/

VRTDriver::VRTDriver() :
    papszSourceParsers(nullptr)
{
#if 0
    pDeserializerData = GDALRegisterTransformDeserializer(
        "WarpedOverviewTransformer",
        VRTWarpedOverviewTransform,
        VRTDeserializeWarpedOverviewTransformer );
#endif
}

/************************************************************************/
/*                             ~VRTDriver()                             */
/************************************************************************/

VRTDriver::~VRTDriver()

{
    CSLDestroy( papszSourceParsers );
    VRTDerivedRasterBand::Cleanup();
#if 0
    if(  pDeserializerData )
    {
        GDALUnregisterTransformDeserializer( pDeserializerData );
    }
#endif
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **VRTDriver::GetMetadataDomainList()
{
    return BuildMetadataDomainList( GDALDriver::GetMetadataDomainList(),
                                    TRUE,
                                    "SourceParsers", nullptr );
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **VRTDriver::GetMetadata( const char *pszDomain )

{
    if( pszDomain && EQUAL(pszDomain,"SourceParsers") )
        return papszSourceParsers;

    return GDALDriver::GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTDriver::SetMetadata( char **papszMetadata, const char *pszDomain )

{
    if( pszDomain && EQUAL(pszDomain,"SourceParsers") )
    {
        m_oMapSourceParser.clear();
        CSLDestroy( papszSourceParsers );
        papszSourceParsers = CSLDuplicate( papszMetadata );
        return CE_None;
    }

    return GDALDriver::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          AddSourceParser()                           */
/************************************************************************/

void VRTDriver::AddSourceParser( const char *pszElementName,
                                 VRTSourceParser pfnParser )

{
    m_oMapSourceParser[pszElementName] = pfnParser;

    // Below won't work on architectures with "capability pointers"

    char szPtrValue[128] = { '\0' };
    void* ptr;
    CPL_STATIC_ASSERT(sizeof(pfnParser) == sizeof(void*));
    memcpy(&ptr, &pfnParser, sizeof(void*));
    int nRet = CPLPrintPointer( szPtrValue,
                                ptr,
                                sizeof(szPtrValue) );
    szPtrValue[nRet] = 0;

    papszSourceParsers = CSLSetNameValue( papszSourceParsers,
                                          pszElementName, szPtrValue );
}

/************************************************************************/
/*                            ParseSource()                             */
/************************************************************************/

VRTSource *VRTDriver::ParseSource( CPLXMLNode *psSrc, const char *pszVRTPath,
                                   std::map<CPLString, GDALDataset*>& oMapSharedSources )

{

    if( psSrc == nullptr || psSrc->eType != CXT_Element )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Corrupt or empty VRT source XML document." );
        return nullptr;
    }

    if( !m_oMapSourceParser.empty() )
    {
        auto oIter = m_oMapSourceParser.find( psSrc->pszValue );
        if( oIter != m_oMapSourceParser.end() )
        {
            return oIter->second( psSrc, pszVRTPath, oMapSharedSources );
        }
        return nullptr;
    }

    // Below won't work on architectures with "capability pointers"

    const char *pszParserFunc
        = CSLFetchNameValue( papszSourceParsers, psSrc->pszValue );
    if( pszParserFunc == nullptr )
        return nullptr;

    VRTSourceParser pfnParser;
    CPL_STATIC_ASSERT(sizeof(pfnParser) == sizeof(void*));
    void* ptr = CPLScanPointer( pszParserFunc,
                        static_cast<int>(strlen(pszParserFunc)) );
    memcpy(&pfnParser, &ptr, sizeof(void*));

    if( pfnParser == nullptr )
        return nullptr;

    return pfnParser( psSrc, pszVRTPath, oMapSharedSources );
}

/************************************************************************/
/*                           VRTCreateCopy()                            */
/************************************************************************/

static GDALDataset *
VRTCreateCopy( const char * pszFilename,
               GDALDataset *poSrcDS,
               int /* bStrict */,
               char ** /* papszOptions */,
               GDALProgressFunc /* pfnProgress */,
               void * /* pProgressData */ )
{
    CPLAssert( nullptr != poSrcDS );

/* -------------------------------------------------------------------- */
/*      If the source dataset is a virtual dataset then just write      */
/*      it to disk as a special case to avoid extra layers of           */
/*      indirection.                                                    */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetDriver() != nullptr &&
        EQUAL(poSrcDS->GetDriver()->GetDescription(),"VRT") )
    {

    /* -------------------------------------------------------------------- */
    /*      Convert tree to a single block of XML text.                     */
    /* -------------------------------------------------------------------- */
        char *pszVRTPath = CPLStrdup(CPLGetPath(pszFilename));
        static_cast<VRTDataset *>(
            poSrcDS )->UnsetPreservedRelativeFilenames();
        CPLXMLNode *psDSTree = static_cast<VRTDataset *>(
            poSrcDS )->SerializeToXML( pszVRTPath );

        char *pszXML = CPLSerializeXMLTree( psDSTree );

        CPLDestroyXMLNode( psDSTree );

        CPLFree( pszVRTPath );

    /* -------------------------------------------------------------------- */
    /*      Write to disk.                                                  */
    /* -------------------------------------------------------------------- */
        GDALDataset* pCopyDS = nullptr;

        if( 0 != strlen( pszFilename ) )
        {
            VSILFILE *fpVRT = VSIFOpenL( pszFilename, "wb" );
            if( fpVRT == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot create %s", pszFilename);
                CPLFree( pszXML );
                return nullptr;
            }

            bool bRet = VSIFWriteL( pszXML, strlen(pszXML), 1, fpVRT ) > 0;
            if( VSIFCloseL( fpVRT ) != 0 )
                bRet = false;

            if( bRet )
                pCopyDS = GDALDataset::Open( pszFilename,
                    GDAL_OF_RASTER | GDAL_OF_MULTIDIM_RASTER | GDAL_OF_UPDATE);
        }
        else
        {
            /* No destination file is given, so pass serialized XML directly. */
            pCopyDS = GDALDataset::Open( pszXML,
                GDAL_OF_RASTER | GDAL_OF_MULTIDIM_RASTER | GDAL_OF_UPDATE);
        }

        CPLFree( pszXML );

        return pCopyDS;
    }

/* -------------------------------------------------------------------- */
/*      Multidimensional raster ?                                       */
/* -------------------------------------------------------------------- */
    auto poSrcGroup = poSrcDS->GetRootGroup();
    if( poSrcGroup != nullptr )
    {
        auto poDstDS = std::unique_ptr<GDALDataset>(
            VRTDataset::CreateMultiDimensional(pszFilename,
                                   nullptr,
                                   nullptr));
        if( !poDstDS )
            return nullptr;
        auto poDstGroup = poDstDS->GetRootGroup();
        if( !poDstGroup )
            return nullptr;
        if( GDALDriver::DefaultCreateCopyMultiDimensional(poSrcDS,
                                              poDstDS.get(),
                                              false,
                                              nullptr,
                                              nullptr,
                                              nullptr) != CE_None )
            return nullptr;
        return poDstDS.release();
    }

/* -------------------------------------------------------------------- */
/*      Create the virtual dataset.                                     */
/* -------------------------------------------------------------------- */
    VRTDataset *poVRTDS = static_cast<VRTDataset *>(
        VRTDataset::Create( pszFilename,
                            poSrcDS->GetRasterXSize(),
                            poSrcDS->GetRasterYSize(),
                            0, GDT_Byte, nullptr ) );
    if( poVRTDS == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Do we have a geotransform?                                      */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = { 0.0 };

    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        poVRTDS->SetGeoTransform( adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Copy projection                                                 */
/* -------------------------------------------------------------------- */
    poVRTDS->SetSpatialRef( poSrcDS->GetSpatialRef() );

/* -------------------------------------------------------------------- */
/*      Emit dataset level metadata.                                    */
/* -------------------------------------------------------------------- */
    poVRTDS->SetMetadata( poSrcDS->GetMetadata() );

/* -------------------------------------------------------------------- */
/*      Copy any special domains that should be transportable.          */
/* -------------------------------------------------------------------- */
    char **papszMD = poSrcDS->GetMetadata( "RPC" );
    if( papszMD )
        poVRTDS->SetMetadata( papszMD, "RPC" );

    papszMD = poSrcDS->GetMetadata( "IMD" );
    if( papszMD )
        poVRTDS->SetMetadata( papszMD, "IMD" );

    papszMD = poSrcDS->GetMetadata( "GEOLOCATION" );
    if( papszMD )
        poVRTDS->SetMetadata( papszMD, "GEOLOCATION" );

    {
        const char* pszInterleave = poSrcDS->GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE");
        if (pszInterleave)
        {
            poVRTDS->SetMetadataItem("INTERLEAVE", pszInterleave, "IMAGE_STRUCTURE");
        }
    }
    {
        const char* pszCompression = poSrcDS->GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE");
        if( pszCompression )
        {
            poVRTDS->SetMetadataItem("COMPRESSION", pszCompression, "IMAGE_STRUCTURE");
        }
    }

/* -------------------------------------------------------------------- */
/*      GCPs                                                            */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetGCPCount() > 0 )
    {
        poVRTDS->SetGCPs( poSrcDS->GetGCPCount(),
                          poSrcDS->GetGCPs(),
                          poSrcDS->GetGCPSpatialRef() );
    }

/* -------------------------------------------------------------------- */
/*      Loop over all the bands.                                        */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Create the band with the appropriate band type.                 */
/* -------------------------------------------------------------------- */
        CPLStringList aosAddBandOptions;
        int nSrcBlockXSize, nSrcBlockYSize;
        poSrcBand->GetBlockSize(&nSrcBlockXSize, &nSrcBlockYSize);
        aosAddBandOptions.SetNameValue("BLOCKXSIZE", CPLSPrintf("%d", nSrcBlockXSize));
        aosAddBandOptions.SetNameValue("BLOCKYSIZE", CPLSPrintf("%d", nSrcBlockYSize));
        poVRTDS->AddBand( poSrcBand->GetRasterDataType(), aosAddBandOptions );

        VRTSourcedRasterBand *poVRTBand
            = static_cast<VRTSourcedRasterBand *>(
                poVRTDS->GetRasterBand( iBand+1 ) );

/* -------------------------------------------------------------------- */
/*      Setup source mapping.                                           */
/* -------------------------------------------------------------------- */
        poVRTBand->AddSimpleSource( poSrcBand );

/* -------------------------------------------------------------------- */
/*      Emit various band level metadata.                               */
/* -------------------------------------------------------------------- */
        poVRTBand->CopyCommonInfoFrom( poSrcBand );

        const char* pszCompression = poSrcBand->GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE");
        if( pszCompression )
        {
            poVRTBand->SetMetadataItem("COMPRESSION", pszCompression, "IMAGE_STRUCTURE");
        }

/* -------------------------------------------------------------------- */
/*      Add specific mask band.                                         */
/* -------------------------------------------------------------------- */
        if( (poSrcBand->GetMaskFlags()
              & (GMF_PER_DATASET | GMF_ALL_VALID | GMF_NODATA)) == 0)
        {
            VRTSourcedRasterBand* poVRTMaskBand = new VRTSourcedRasterBand(
                poVRTDS, 0,
                poSrcBand->GetMaskBand()->GetRasterDataType(),
                poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize());
            poVRTMaskBand->AddMaskBandSource( poSrcBand );
            poVRTBand->SetMaskBand( poVRTMaskBand );
        }
    }

/* -------------------------------------------------------------------- */
/*      Add dataset mask band                                           */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetRasterCount() != 0 &&
        poSrcDS->GetRasterBand(1) != nullptr &&
        poSrcDS->GetRasterBand(1)->GetMaskFlags() == GMF_PER_DATASET )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(1);
        VRTSourcedRasterBand* poVRTMaskBand = new VRTSourcedRasterBand(
            poVRTDS, 0,
            poSrcBand->GetMaskBand()->GetRasterDataType(),
            poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize() );
        poVRTMaskBand->AddMaskBandSource( poSrcBand );
        poVRTDS->SetMaskBand( poVRTMaskBand );
    }

    if( strcmp(pszFilename, "") != 0 )
    {
        CPLErrorReset();
        poVRTDS->FlushCache(true);
        if( CPLGetLastErrorType() != CE_None )
        {
            delete poVRTDS;
            poVRTDS = nullptr;
        }
    }

    return poVRTDS;
}

/************************************************************************/
/*                          GDALRegister_VRT()                          */
/************************************************************************/

void GDALRegister_VRT()

{
    if( GDALGetDriverByName( "VRT" ) != nullptr )
        return;

    // First register the pixel functions
    GDALRegisterDefaultPixelFunc();

    VRTDriver *poDriver = new VRTDriver();

    poDriver->SetDescription( "VRT" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIDIM_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Virtual Raster" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "vrt" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/vrt.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32 Int64 UInt64 "
                               "Float32 Float64 "
                               "CInt16 CInt32 CFloat32 CFloat64" );

    poDriver->pfnOpen = VRTDataset::Open;
    poDriver->pfnCreateCopy = VRTCreateCopy;
    poDriver->pfnCreate = VRTDataset::Create;
    poDriver->pfnCreateMultiDimensional = VRTDataset::CreateMultiDimensional;
    poDriver->pfnIdentify = VRTDataset::Identify;
    poDriver->pfnDelete = VRTDataset::Delete;

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='ROOT_PATH' type='string' description='Root path to evaluate "
"relative paths inside the VRT. Mainly useful for inlined VRT, or in-memory "
"VRT, where their own directory does not make sense'/>"
"</OpenOptionList>" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_COORDINATE_EPOCH, "YES" );

    poDriver->AddSourceParser( "SimpleSource", VRTParseCoreSources );
    poDriver->AddSourceParser( "ComplexSource", VRTParseCoreSources );
    poDriver->AddSourceParser( "AveragedSource", VRTParseCoreSources );
    poDriver->AddSourceParser( "KernelFilteredSource", VRTParseFilterSources );

    GetGDALDriverManager()->RegisterDriver( poDriver );
}

/*! @endcond */
