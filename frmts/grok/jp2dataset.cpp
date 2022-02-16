/******************************************************************************
 *
 * Project:  JPEG2000 base driver
 * Purpose:  JPEG2000 base driver
 * Author:   Aaron Boxer, <boxerab at protonmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2015, European Union (European Environment Agency)
 * Copyright (c) 2022, Grok Image Compression
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

#include "jp2dataset.h"



/************************************************************************/
/*                           PreloadBlocks()                            */
/************************************************************************/
int JP2Dataset::PreloadBlocks(JP2RasterBand* poBand,
                                      int nXOff, int nYOff, int nXSize, int nYSize,
                                      int nBandCount, int *panBandMap)
{
    int bRet = TRUE;
    int nXStart = nXOff / poBand->nBlockXSize;
    int nXEnd = (nXOff + nXSize - 1) / poBand->nBlockXSize;
    int nYStart = nYOff / poBand->nBlockYSize;
    int nYEnd = (nYOff + nYSize - 1) / poBand->nBlockYSize;
    GIntBig nReqMem = (GIntBig)(nXEnd - nXStart + 1) * (nYEnd - nYStart + 1) *
                      poBand->nBlockXSize * poBand->nBlockYSize * (GDALGetDataTypeSize(poBand->eDataType) / 8);

    int nMaxThreads = GetNumThreads();
    if( !bUseSetDecodeArea && nMaxThreads > 1 )
    {
        if( nReqMem > GDALGetCacheMax64() / (nBandCount == 0 ? 1 : nBandCount) )
            return FALSE;

        JobStruct oJob;
        m_nBlocksToLoad = 0;
        try
        {
            for(int nBlockXOff = nXStart; nBlockXOff <= nXEnd; ++nBlockXOff)
            {
                for(int nBlockYOff = nYStart; nBlockYOff <= nYEnd; ++nBlockYOff)
                {
                    GDALRasterBlock* poBlock = poBand->TryGetLockedBlockRef(nBlockXOff,nBlockYOff);
                    if (poBlock != nullptr)
                    {
                        poBlock->DropLock();
                        continue;
                    }
                    oJob.oPairs.push_back( std::pair<int,int>(nBlockXOff, nBlockYOff) );
                    m_nBlocksToLoad ++;
                }
            }
        }
        catch( const std::bad_alloc& )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory error");
            m_nBlocksToLoad = 0;
            return -1;
        }

        if( m_nBlocksToLoad > 1 )
        {
            const int l_nThreads = std::min(m_nBlocksToLoad, nMaxThreads);
            CPLJoinableThread** pahThreads = (CPLJoinableThread**) VSI_CALLOC_VERBOSE( sizeof(CPLJoinableThread*), l_nThreads );
            if( pahThreads == nullptr )
            {
                m_nBlocksToLoad = 0;
                return -1;
            }
            int i;

            CPLDebug("JP2DATASET", "%d blocks to load (%d threads)", m_nBlocksToLoad, l_nThreads);

            oJob.poGDS = this;
            oJob.nBand = poBand->GetBand();
            oJob.nCurPair = -1;
            if( nBandCount > 0 )
            {
                oJob.nBandCount = nBandCount;
                oJob.panBandMap = panBandMap;
            }
            else
            {
                if( nReqMem <= GDALGetCacheMax64() / nBands )
                {
                    oJob.nBandCount = nBands;
                    oJob.panBandMap = nullptr;
                }
                else
                {
                    bRet = FALSE;
                    oJob.nBandCount = 1;
                    oJob.panBandMap = &oJob.nBand;
                }
            }
            oJob.bSuccess = true;

            /* Flushes all dirty blocks from cache to disk to avoid them */
            /* to be flushed randomly, and simultaneously, from our worker threads, */
            /* which might cause races in the output driver. */
            /* This is a workaround to a design defect of the block cache */
            GDALRasterBlock::FlushDirtyBlocks();

            for(i=0;i<l_nThreads;i++)
            {
                pahThreads[i] = CPLCreateJoinableThread(ReadBlockInThread, &oJob);
                if( pahThreads[i] == nullptr )
                    oJob.bSuccess = false;
            }
            TemporarilyDropReadWriteLock();
            for(i=0;i<l_nThreads;i++)
                CPLJoinThread( pahThreads[i] );
            ReacquireReadWriteLock();
            CPLFree(pahThreads);
            if( !oJob.bSuccess )
            {
                m_nBlocksToLoad = 0;
                return -1;
            }
            m_nBlocksToLoad = 0;
        }
    }

    return bRet;
}
void JP2Dataset::ReadBlockInThread(void* userdata)
{
    int nPair;
    JobStruct* poJob = (JobStruct*) userdata;

    JP2Dataset* poGDS = poJob->poGDS;
    int nBand = poJob->nBand;
    int nPairs = (int)poJob->oPairs.size();
    int nBandCount = poJob->nBandCount;
    int* panBandMap = poJob->panBandMap;
    VSILFILE* fp = VSIFOpenL(poGDS->m_osFilename.c_str(), "rb");
    if( fp == nullptr )
    {
        CPLDebug("JP2Dataset", "Cannot open %s", poGDS->m_osFilename.c_str());
        poJob->bSuccess = false;
        //VSIFree(pDummy);
        return;
    }

    while( (nPair = CPLAtomicInc(&(poJob->nCurPair))) < nPairs &&
            poJob->bSuccess )
    {
        int nBlockXOff = poJob->oPairs[nPair].first;
        int nBlockYOff = poJob->oPairs[nPair].second;
        poGDS->AcquireMutex();
        GDALRasterBlock* poBlock = poGDS->GetRasterBand(nBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
        poGDS->ReleaseMutex();
        if (poBlock == nullptr)
        {
            poJob->bSuccess = false;
            break;
        }

        void* pDstBuffer = poBlock->GetDataRef();
        if( poGDS->ReadBlock(nBand, fp, nBlockXOff, nBlockYOff, pDstBuffer,
                             nBandCount, panBandMap) != CE_None )
        {
            poJob->bSuccess = false;
        }

        poBlock->DropLock();
    }

    VSIFCloseL(fp);
    //VSIFree(pDummy);
}

int JP2Dataset::GetNumThreads()
{
    if( nThreads >= 1 )
        return nThreads;

    const char* pszThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    if (EQUAL(pszThreads, "ALL_CPUS"))
        nThreads = CPLGetNumCPUs();
    else
        nThreads = atoi(pszThreads);
    if (nThreads > 128)
        nThreads = 128;
    if (nThreads <= 0)
        nThreads = 1;
    return nThreads;
}
GDALColorInterp JP2Dataset::GetColorInterpretation(int nBand) {
    if( nBand == nAlphaIndex + 1 )
        return GCI_AlphaBand;

    if (nBands <= 2 && eColorSpace == JP2_CLRSPC_GRAY)
        return GCI_GrayIndex;
    else if (eColorSpace == JP2_CLRSPC_SRGB ||
             eColorSpace == JP2_CLRSPC_SYCC)
    {
        if( nBand == nRedIndex + 1 )
            return GCI_RedBand;
        if( nBand == nGreenIndex + 1 )
            return GCI_GreenBand;
        if( nBand == nBlueIndex + 1 )
            return GCI_BlueBand;
    }

    return GCI_Undefined;
}

/************************************************************************/
/*                           WriteBox()                                 */
/************************************************************************/

void JP2Dataset::WriteBox(VSILFILE* fp, GDALJP2Box* poBox)
{
    GUInt32   nLBox;
    GUInt32   nTBox;

    if( poBox == nullptr )
        return;

    nLBox = (int) poBox->GetDataLength() + 8;
    nLBox = CPL_MSBWORD32( nLBox );

    memcpy(&nTBox, poBox->GetType(), 4);

    VSIFWriteL( &nLBox, 4, 1, fp );
    VSIFWriteL( &nTBox, 4, 1, fp );
    VSIFWriteL(poBox->GetWritableData(), 1, (int) poBox->GetDataLength(), fp);
}

/************************************************************************/
/*                         WriteGDALMetadataBox()                       */
/************************************************************************/

void JP2Dataset::WriteGDALMetadataBox( VSILFILE* fp,
                                               GDALDataset* poSrcDS,
                                               char** papszOptions )
{
    GDALJP2Box* poBox = GDALJP2Metadata::CreateGDALMultiDomainMetadataXMLBox(
        poSrcDS, CPLFetchBool(papszOptions, "MAIN_MD_DOMAIN_ONLY", false));
    if( poBox )
        WriteBox(fp, poBox);
    delete poBox;
}

/************************************************************************/
/*                         WriteXMLBoxes()                              */
/************************************************************************/

void JP2Dataset::WriteXMLBoxes( VSILFILE* fp, GDALDataset* poSrcDS,
                                         CPL_UNUSED char** papszOptions )
{
    int nBoxes = 0;
    GDALJP2Box** papoBoxes = GDALJP2Metadata::CreateXMLBoxes(poSrcDS, &nBoxes);
    for(int i=0;i<nBoxes;i++)
    {
        WriteBox(fp, papoBoxes[i]);
        delete papoBoxes[i];
    }
    CPLFree(papoBoxes);
}

/************************************************************************/
/*                           WriteXMPBox()                              */
/************************************************************************/

void JP2Dataset::WriteXMPBox ( VSILFILE* fp, GDALDataset* poSrcDS,
                                       CPL_UNUSED char** papszOptions )
{
    GDALJP2Box* poBox = GDALJP2Metadata::CreateXMPBox(poSrcDS);
    if( poBox )
        WriteBox(fp, poBox);
    delete poBox;
}

/************************************************************************/
/*                           SetGeoTransform()                          */
/************************************************************************/

CPLErr JP2Dataset::SetGeoTransform( double *padfGeoTransform )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        memcpy(adfGeoTransform, padfGeoTransform, 6* sizeof(double));
        bGeoTransformValid = !(
            adfGeoTransform[0] == 0.0 && adfGeoTransform[1] == 1.0 &&
            adfGeoTransform[2] == 0.0 && adfGeoTransform[3] == 0.0 &&
            adfGeoTransform[4] == 0.0 && adfGeoTransform[5] == 1.0);
        return CE_None;
    }
    else
        return GDALJP2AbstractDataset::SetGeoTransform(padfGeoTransform);
}
/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr JP2Dataset::SetSpatialRef( const OGRSpatialReference * poSRS )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        m_oSRS.Clear();
        if( poSRS )
            m_oSRS = *poSRS;
        return CE_None;
    }
    else
        return GDALJP2AbstractDataset::SetSpatialRef(poSRS);
}

/************************************************************************/
/*                           SetGCPs()                                  */
/************************************************************************/

CPLErr JP2Dataset::SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                                    const OGRSpatialReference* poSRS )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        if( nGCPCount > 0 )
        {
            GDALDeinitGCPs( nGCPCount, pasGCPList );
            CPLFree( pasGCPList );
        }

        m_oSRS.Clear();
        if( poSRS )
            m_oSRS = *poSRS;

        nGCPCount = nGCPCountIn;
        pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPListIn );

        return CE_None;
    }
    else
        return GDALJP2AbstractDataset::SetGCPs(nGCPCountIn, pasGCPListIn,
                                               poSRS);
}


/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr  JP2Dataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg)
{
    if( eRWFlag != GF_Read )
        return CE_Failure;

    if( nBandCount < 1 )
        return CE_Failure;

    JP2RasterBand* poBand = (JP2RasterBand*) GetRasterBand(panBandMap[0]);

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */

    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && poBand->GetOverviewCount() > 0 )
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO( eRWFlag,
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
    }

    bEnoughMemoryToLoadOtherBands = PreloadBlocks(poBand, nXOff, nYOff, nXSize, nYSize, nBandCount, panBandMap);

    CPLErr eErr = GDALPamDataset::IRasterIO(   eRWFlag,
                                        nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize,
                                        eBufType,
                                        nBandCount, panBandMap,
                                        nPixelSpace, nLineSpace, nBandSpace,
                                        psExtraArg );

    bEnoughMemoryToLoadOtherBands = TRUE;
    return eErr;
}
/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr JP2Dataset::SetMetadata( char ** papszMetadata,
                                        const char * pszDomain )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        if( pszDomain == nullptr || EQUAL(pszDomain, "") )
        {
            CSLDestroy(m_papszMainMD);
            m_papszMainMD = CSLDuplicate(papszMetadata);
        }
        return GDALDataset::SetMetadata(papszMetadata, pszDomain);
    }
    return GDALJP2AbstractDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr JP2Dataset::SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        if( pszDomain == nullptr || EQUAL(pszDomain, "") )
        {
            m_papszMainMD = CSLSetNameValue( GetMetadata(), pszName, pszValue );
        }
        return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
    }
    return GDALJP2AbstractDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}



/************************************************************************/
/*                        FindCodeStream()                   */
/************************************************************************/
static const unsigned char jpc_header[] = {0xff,0x4f,0xff,0x51}; // SOC + RSIZ markers
static const unsigned char jp2_box_jp[] = {0x6a,0x50,0x20,0x20}; /* 'jP  ' */
vsi_l_offset JP2Dataset::FindCodeStream( VSILFILE* fp,
                                               vsi_l_offset* pnLength )
{
    vsi_l_offset nCodeStreamStart = 0;
    vsi_l_offset nCodeStreamLength = 0;

    VSIFSeekL(fp, 0, SEEK_SET);
    GByte abyHeader[16];
    VSIFReadL(abyHeader, 1, 16, fp);

    if (memcmp( abyHeader, jpc_header, sizeof(jpc_header) ) == 0)
    {
        VSIFSeekL(fp, 0, SEEK_END);
        nCodeStreamLength = VSIFTellL(fp);
    }
    else if (memcmp( abyHeader + 4, jp2_box_jp, sizeof(jp2_box_jp) ) == 0)
    {
        /* Find offset of first jp2c box */
        GDALJP2Box oBox( fp );
        if( oBox.ReadFirst() )
        {
            while( strlen(oBox.GetType()) > 0 )
            {
                if( EQUAL(oBox.GetType(),"jp2c") )
                {
                    nCodeStreamStart = VSIFTellL(fp);
                    nCodeStreamLength = oBox.GetDataLength();
                    break;
                }

                if (!oBox.ReadNext())
                    break;
            }
        }
    }
    *pnLength = nCodeStreamLength;
    return nCodeStreamStart;
}
/************************************************************************/
/*                            Identify()                                */
/************************************************************************/
int JP2Dataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes >= 16
        && (memcmp( poOpenInfo->pabyHeader, jpc_header,
                    sizeof(jpc_header) ) == 0
            || memcmp( poOpenInfo->pabyHeader + 4, jp2_box_jp,
                    sizeof(jp2_box_jp) ) == 0
           ) )
        return TRUE;

    else
        return FALSE;
}

void JP2Dataset::getBandInfo(int nBand,
					int &nBlockXSize,
					int &nBlockYSize,
					GDALDataType &eDataType,
					int &nBlocksPerRow){
    JP2RasterBand* poBand = (JP2RasterBand*) GetRasterBand(nBand);
    nBlockXSize = poBand->nBlockXSize;
    nBlockYSize = poBand->nBlockYSize;
    eDataType = poBand->eDataType;
    nBlocksPerRow = poBand->nBlocksPerRow;
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int JP2Dataset::CloseDependentDatasets()
{
    int bRet = GDALJP2AbstractDataset::CloseDependentDatasets();
    if ( papoOverviewDS )
    {
        for( int i = 0; i < nOverviewCount; i++ )
            delete papoOverviewDS[i];
        CPLFree( papoOverviewDS );
        papoOverviewDS = nullptr;
        bRet = TRUE;
    }
    return bRet;
}


/************************************************************************/
/*                           WriteIPRBox()                              */
/************************************************************************/

void JP2Dataset::WriteIPRBox ( VSILFILE* fp, GDALDataset* poSrcDS,
                                       CPL_UNUSED char** papszOptions )
{
    GDALJP2Box* poBox = GDALJP2Metadata::CreateIPRBox(poSrcDS);
    if( poBox )
        WriteBox(fp, poBox);
    delete poBox;
}


/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr JP2Dataset::IBuildOverviews( const char *pszResampling,
                                       int nOverviews, int *panOverviewList,
                                       int nListBands, int *panBandList,
                                       GDALProgressFunc pfnProgress,
                                       void *pProgressData )

{
    // In order for building external overviews to work properly, we
    // discard any concept of internal overviews when the user
    // first requests to build external overviews.
    for( int i = 0; i < nOverviewCount; i++ )
    {
        delete papoOverviewDS[i];
    }
    CPLFree(papoOverviewDS);
    papoOverviewDS = nullptr;
    nOverviewCount = 0;

    return GDALPamDataset::IBuildOverviews(pszResampling,
                                           nOverviews, panOverviewList,
                                           nListBands, panBandList,
                                           pfnProgress, pProgressData);
}

int JP2Dataset::FloorPowerOfTwo(int nVal)
{
    int nBits = 0;
    while( nVal > 1 )
    {
        nBits ++;
        nVal >>= 1;
    }
    return 1 << nBits;
}

GDALDriver* JP2Dataset::CreateDriver(const char* driverVersion,
									const char* driverName ,
									const char* driverLongName,
									const char* driverHelp)
{
    if( !GDAL_CHECK_VERSION( driverVersion ) )
        return nullptr;

    if( GDALGetDriverByName( driverName ) != nullptr )
        return nullptr;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( driverName );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, driverLongName );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, driverHelp );
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jp2" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jp2" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "jp2 j2k" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='1BIT_ALPHA_PROMOTION' type='boolean' description='Whether a 1-bit alpha channel should be promoted to 8-bit' default='YES'/>"
"   <Option name='OPEN_REMOTE_GML' type='boolean' description='Whether to load remote vector layers referenced by a link in a GMLJP2 v2 box' default='NO'/>"
"   <Option name='GEOREF_SOURCES' type='string' description='Comma separated list made with values INTERNAL/GMLJP2/GEOJP2/WORLDFILE/PAM/NONE that describe the priority order for georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
"   <Option name='USE_TILE_AS_BLOCK' type='boolean' description='Whether to always use the JPEG-2000 block size as the GDAL block size' default='NO'/>"
"</OpenOptionList>" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='CODEC' type='string-select' default='according to file extension. If unknown, default to J2K'>"
"       <Value>JP2</Value>"
"       <Value>J2K</Value>"
"   </Option>"
"   <Option name='GeoJP2' type='boolean' description='Whether to emit a GeoJP2 box' default='YES'/>"
"   <Option name='GMLJP2' type='boolean' description='Whether to emit a GMLJP2 v1 box' default='YES'/>"
"   <Option name='GMLJP2V2_DEF' type='string' description='Definition file to describe how a GMLJP2 v2 box should be generated. If set to YES, a minimal instance will be created'/>"
"   <Option name='QUALITY' type='string' description='Single quality value or comma separated list of increasing quality values for several layers, each in the 0-100 range' default='25'/>"
"   <Option name='REVERSIBLE' type='boolean' description='True if the compression is reversible' default='false'/>"
"   <Option name='RESOLUTIONS' type='int' description='Number of resolutions.' min='1' max='30'/>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width' default='1024'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile Height' default='1024'/>"
"   <Option name='PROGRESSION' type='string-select' default='LRCP'>"
"       <Value>LRCP</Value>"
"       <Value>RLCP</Value>"
"       <Value>RPCL</Value>"
"       <Value>PCRL</Value>"
"       <Value>CPRL</Value>"
"   </Option>"
"   <Option name='SOP' type='boolean' description='True to insert SOP markers' default='false'/>"
"   <Option name='EPH' type='boolean' description='True to insert EPH markers' default='false'/>"
"   <Option name='YCBCR420' type='boolean' description='if RGB must be resampled to YCbCr 4:2:0' default='false'/>"
"   <Option name='YCC' type='boolean' description='if RGB must be transformed to YCC color space (lossless MCT transform)' default='YES'/>"
"   <Option name='NBITS' type='int' description='Bits (precision) for sub-byte files (1-7), sub-uint16 (9-15), sub-uint32 (17-31)'/>"
"   <Option name='1BIT_ALPHA' type='boolean' description='Whether to encode the alpha channel as a 1-bit channel' default='NO'/>"
"   <Option name='ALPHA' type='boolean' description='Whether to force encoding last channel as alpha channel' default='NO'/>"
"   <Option name='PROFILE' type='string-select' description='Which codestream profile to use' default='AUTO'>"
"       <Value>AUTO</Value>"
"       <Value>UNRESTRICTED</Value>"
"       <Value>PROFILE_1</Value>"
"   </Option>"
"   <Option name='INSPIRE_TG' type='boolean' description='Whether to use features that comply with Inspire Orthoimagery Technical Guidelines' default='NO'/>"
"   <Option name='JPX' type='boolean' description='Whether to advertise JPX features when a GMLJP2 box is written (or use JPX branding if GMLJP2 v2)' default='YES'/>"
"   <Option name='GEOBOXES_AFTER_JP2C' type='boolean' description='Whether to place GeoJP2/GMLJP2 boxes after the code-stream' default='NO'/>"
"   <Option name='PRECINCTS' type='string' description='Precincts size as a string of the form {w,h},{w,h},... with power-of-two values'/>"
"   <Option name='TILEPARTS' type='string-select' description='Whether to generate tile-parts and according to which criterion' default='DISABLED'>"
"       <Value>DISABLED</Value>"
"       <Value>RESOLUTIONS</Value>"
"       <Value>LAYERS</Value>"
"       <Value>COMPONENTS</Value>"
"   </Option>"
"   <Option name='CODEBLOCK_WIDTH' type='int' description='Codeblock width' default='64' min='4' max='1024'/>"
"   <Option name='CODEBLOCK_HEIGHT' type='int' description='Codeblock height' default='64' min='4' max='1024'/>"
"   <Option name='CT_COMPONENTS' type='int' min='3' max='4' description='If there is one color table, number of color table components to write. Autodetected if not specified.'/>"
"   <Option name='WRITE_METADATA' type='boolean' description='Whether metadata should be written, in a dedicated JP2 XML box' default='NO'/>"
"   <Option name='MAIN_MD_DOMAIN_ONLY' type='boolean' description='(Only if WRITE_METADATA=YES) Whether only metadata from the main domain should be written' default='NO'/>"
"   <Option name='USE_SRC_CODESTREAM' type='boolean' description='When source dataset is JPEG2000, whether to reuse the codestream of the source dataset unmodified' default='NO'/>"
"   <Option name='CODEBLOCK_STYLE' type='string' description='Comma-separated combination of BYPASS, RESET, TERMALL, VSC, PREDICTABLE, SEGSYM or value between 0 and 63'/>"
"   <Option name='PLT' type='boolean' description='True to insert PLT marker segments' default='false'/>"
"   <Option name='TLM' type='boolean' description='True to insert TLM marker segments' default='false'/>"
"</CreationOptionList>"  );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->pfnIdentify = JP2Dataset::Identify;

    return poDriver;
}

/************************************************************************/
/*                      ~JP2RasterBand()                        */
/************************************************************************/

JP2RasterBand::~JP2RasterBand()
{
    delete poCT;
}

/************************************************************************/
/*                        JP2kRasterBand()                       */
/************************************************************************/

JP2RasterBand::JP2RasterBand( JP2Dataset *poDSIn, int nBandIn,
							  GDALDataType eDataTypeIn, int nBits,
							  int bPromoteTo8BitIn,
							  int nBlockXSizeIn, int nBlockYSizeIn )

{
    this->eDataType = eDataTypeIn;
    this->nBlockXSize = nBlockXSizeIn;
    this->nBlockYSize = nBlockYSizeIn;
    this->bPromoteTo8Bit = bPromoteTo8BitIn;
    poCT = nullptr;

    if( (nBits % 8) != 0 )
        GDALRasterBand::SetMetadataItem("NBITS",
                        CPLString().Printf("%d",nBits),
                        "IMAGE_STRUCTURE" );
    GDALRasterBand::SetMetadataItem("COMPRESSION", "JPEG2000",
                    "IMAGE_STRUCTURE" );
    this->poDS = poDSIn;
    this->nBand = nBandIn;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JP2RasterBand::GetColorInterpretation()
{
    if( poCT )
        return GCI_PaletteIndex;

    JP2Dataset *poGDS = (JP2Dataset *) poDS;

    return poGDS->GetColorInterpretation(nBand);
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JP2RasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                          void * pImage )
{
    JP2Dataset *poGDS = (JP2Dataset *) poDS;

#ifdef DEBUG_VERBOSE
    int nXOff = nBlockXOff * nBlockXSize;
    int nYOff = nBlockYOff * nBlockYSize;
    int nXSize = std::min( nBlockXSize, nRasterXSize - nXOff );
    int nYSize = std::min( nBlockYSize, nRasterYSize - nYOff );
    if( poGDS->iLevel == 0 )
    {
        CPLDebug("JP2DATASET",
                 "ds.GetRasterBand(%d).ReadRaster(%d,%d,%d,%d)",
                 nBand, nXOff, nYOff, nXSize, nYSize);
    }
    else
    {
        CPLDebug("JP2DATASET",
                 "ds.GetRasterBand(%d).GetOverview(%d).ReadRaster(%d,%d,%d,%d)",
                 nBand, poGDS->iLevel - 1, nXOff, nYOff, nXSize, nYSize);
    }
#endif

    if ( poGDS->bEnoughMemoryToLoadOtherBands )
        return poGDS->ReadBlock(nBand, poGDS->fp, nBlockXOff, nBlockYOff, pImage,
                                poGDS->nBands, nullptr);
    else
        return poGDS->ReadBlock(nBand, poGDS->fp, nBlockXOff, nBlockYOff, pImage,
                                1, &nBand);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JP2RasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                         int nXOff, int nYOff, int nXSize, int nYSize,
                                         void * pData, int nBufXSize, int nBufYSize,
                                         GDALDataType eBufType,
                                         GSpacing nPixelSpace, GSpacing nLineSpace,
                                         GDALRasterIOExtraArg* psExtraArg )
{
	JP2Dataset *poGDS = (JP2Dataset *) poDS;

    if( eRWFlag != GF_Read )
        return CE_Failure;

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 )
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO( eRWFlag,
                                    nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nPixelSpace, nLineSpace,
                                    psExtraArg,
                                    &bTried );
        if( bTried )
            return eErr;
    }

    int nRet = poGDS->PreloadBlocks(this, nXOff, nYOff, nXSize, nYSize, 0, nullptr);
    if( nRet < 0 )
        return CE_Failure;
    poGDS->bEnoughMemoryToLoadOtherBands = nRet;

    CPLErr eErr = GDALPamRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg );

    // cppcheck-suppress redundantAssignment
    poGDS->bEnoughMemoryToLoadOtherBands = TRUE;
    return eErr;
}


/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int JP2RasterBand::GetOverviewCount()
{
    JP2Dataset *poGDS = cpl::down_cast<JP2Dataset*>(poDS);
    if( !poGDS->AreOverviewsEnabled() )
        return 0;

    if( GDALPamRasterBand::GetOverviewCount() > 0 )
        return GDALPamRasterBand::GetOverviewCount();

    return poGDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand* JP2RasterBand::GetOverview(int iOvrLevel)
{
    if( GDALPamRasterBand::GetOverviewCount() > 0 )
        return GDALPamRasterBand::GetOverview(iOvrLevel);

    JP2Dataset *poGDS = (JP2Dataset *) poDS;
    if (iOvrLevel < 0 || iOvrLevel >= poGDS->nOverviewCount)
        return nullptr;

    return poGDS->papoOverviewDS[iOvrLevel]->GetRasterBand(nBand);
}
