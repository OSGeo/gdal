/******************************************************************************
 * $Id: jpipkakdataset.cpp 2008-10-01 nbarker $
 *
 * Project:  jpip read driver
 * Purpose:  GDAL bindings for JPIP.  
 * Author:   Norman Barker, ITT VIS, norman.barker@gmail.com
 *
 ******************************************************************************
 * ITT Visual Information Systems grants you use of this code, under the 
 * following license:
 * 
 * Copyright (c) 2000-2007, ITT Visual Information Solutions 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions: 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software. 

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
**/

#include "jpipkakdataset.h"


/* 
** The following are for testing premature stream termination support.
** This is a mechanism to test handling of failed or incomplete reads 
** from the server, and is not normally active.  For this reason we
** don't worry about the non-threadsafe nature of the debug support
** variables below.
*/

#ifdef DEBUG
#  define PST_DEBUG 1
#endif

#ifdef PST_DEBUG
static int nPSTTargetInstance = -1;
static int nPSTThisInstance = -1;
static int nPSTTargetOffset = -1;
#endif

/************************************************************************/
/* ==================================================================== */
/*                     Set up messaging services                        */
/* ==================================================================== */
/************************************************************************/

class jpipkak_kdu_cpl_error_message : public kdu_message 
{
public: // Member classes
    jpipkak_kdu_cpl_error_message( CPLErr eErrClass ) 
    {
        m_eErrClass = eErrClass;
        m_pszError = NULL;
    }

    void put_text(const char *string)
    {
        if( m_pszError == NULL )
            m_pszError = CPLStrdup( string );
        else
        {
            m_pszError = (char *) 
                CPLRealloc(m_pszError, strlen(m_pszError) + strlen(string)+1 );
            strcat( m_pszError, string );
        }
    }

    class JP2KAKException
    {
    };

    void flush(bool end_of_message=false) 
    {
        if( m_pszError == NULL )
            return;
        if( m_pszError[strlen(m_pszError)-1] == '\n' )
            m_pszError[strlen(m_pszError)-1] = '\0';

        CPLError( m_eErrClass, CPLE_AppDefined, "%s", m_pszError );
        CPLFree( m_pszError );
        m_pszError = NULL;

        if( end_of_message && m_eErrClass == CE_Failure )
        {
            throw new JP2KAKException();
        }
    }

private:
    CPLErr m_eErrClass;
    char *m_pszError;
};

/************************************************************************/
/* ==================================================================== */
/*                            JPIPKAKRasterBand                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         JPIPKAKRasterBand()                          */
/************************************************************************/

JPIPKAKRasterBand::JPIPKAKRasterBand( int nBand, int nDiscardLevels,
                                      kdu_codestream *oCodeStream,
                                      int nResCount,
                                      JPIPKAKDataset *poBaseDSIn )

{
    this->nBand = nBand;
    poBaseDS = poBaseDSIn;

    eDataType = poBaseDSIn->eDT;

    this->nDiscardLevels = nDiscardLevels;
    this->oCodeStream = oCodeStream;

    oCodeStream->apply_input_restrictions( 0, 0, nDiscardLevels, 0, NULL );
    oCodeStream->get_dims( 0, band_dims );

    nRasterXSize = band_dims.size.x;
    nRasterYSize = band_dims.size.y;

/* -------------------------------------------------------------------- */
/*      Use a 2048x128 "virtual" block size unless the file is small.    */
/* -------------------------------------------------------------------- */
    if( nRasterXSize >= 2048 )
        nBlockXSize = 2048;
    else
        nBlockXSize = nRasterXSize;
    
    if( nRasterYSize >= 256 )
        nBlockYSize = 128;
    else
        nBlockYSize = nRasterYSize;

/* -------------------------------------------------------------------- */
/*      Figure out the color interpretation for this band.              */
/* -------------------------------------------------------------------- */
    
    eInterp = GCI_Undefined;
        
/* -------------------------------------------------------------------- */
/*      Do we have any overviews?  Only check if we are the full res    */
/*      image.                                                          */
/* -------------------------------------------------------------------- */
    nOverviewCount = 0;
    papoOverviewBand = 0;

    if( nDiscardLevels == 0 )
    {
        int  nXSize = nRasterXSize, nYSize = nRasterYSize;

        for( int nDiscard = 1; nDiscard < nResCount; nDiscard++ )
        {
            kdu_dims  dims;

            nXSize = (nXSize+1) / 2;
            nYSize = (nYSize+1) / 2;

            if( (nXSize+nYSize) < 128 || nXSize < 4 || nYSize < 4 )
                continue; /* skip super reduced resolution layers */

            oCodeStream->apply_input_restrictions( 0, 0, nDiscard, 0, NULL );
            oCodeStream->get_dims( 0, dims );

            if( (dims.size.x == nXSize || dims.size.x == nXSize-1)
                && (dims.size.y == nYSize || dims.size.y == nYSize-1) )
            {
                nOverviewCount++;
                papoOverviewBand = (JPIPKAKRasterBand **) 
                    CPLRealloc( papoOverviewBand, 
                                sizeof(void*) * nOverviewCount );
                papoOverviewBand[nOverviewCount-1] = 
                    new JPIPKAKRasterBand( nBand, nDiscard, oCodeStream, 0,
                                           poBaseDS );
            }
            else
            {
                CPLDebug( "GDAL", "Discard %dx%d JPEG2000 overview layer,\n"
                          "expected %dx%d.", 
                          dims.size.x, dims.size.y, nXSize, nYSize );
            }
        }
    }
}

/************************************************************************/
/*                         ~JPIPKAKRasterBand()                          */
/************************************************************************/

JPIPKAKRasterBand::~JPIPKAKRasterBand()

{
    for( int i = 0; i < nOverviewCount; i++ )
        delete papoOverviewBand[i];

    CPLFree( papoOverviewBand );
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int JPIPKAKRasterBand::GetOverviewCount()

{
    return nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *JPIPKAKRasterBand::GetOverview( int iOverviewIndex )

{
    if( iOverviewIndex < 0 || iOverviewIndex >= nOverviewCount )
        return NULL;
    else
        return papoOverviewBand[iOverviewIndex];
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPIPKAKRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    CPLDebug( "JPIPKAK", "IReadBlock(%d,%d) on band %d.", 
              nBlockXOff, nBlockYOff, nBand );

/* -------------------------------------------------------------------- */
/*      Fix the buffer layer.                                           */
/* -------------------------------------------------------------------- */
    int nPixelSpace = GDALGetDataTypeSize(eDataType) / 8;
    int nLineSpace = nPixelSpace * nBlockXSize;
    int nBandSpace = nLineSpace * nBlockYSize;

/* -------------------------------------------------------------------- */
/*      Zoom up file window based on overview level so we are           */
/*      referring to the full res image.                                */
/* -------------------------------------------------------------------- */
    int nZoom = 1 << nDiscardLevels;

    int xOff = nBlockXOff * nBlockXSize * nZoom;
    int yOff = nBlockYOff * nBlockYSize * nZoom;
    int xSize = nBlockXSize * nZoom;
    int ySize = nBlockYSize * nZoom;

    int nBufXSize = nBlockXSize;
    int nBufYSize = nBlockYSize;

/* -------------------------------------------------------------------- */
/*      Make adjustments for partial blocks on right and bottom.        */
/* -------------------------------------------------------------------- */
    if( xOff + xSize > poBaseDS->GetRasterXSize() )
    {
        xSize = poBaseDS->GetRasterXSize() - xOff;
        nBufXSize= MAX(xSize/nZoom,1);
    }
    
    if( yOff + ySize > poBaseDS->GetRasterYSize() )
    {
        ySize = poBaseDS->GetRasterYSize() - yOff;
        nBufYSize = MAX(ySize/nZoom,1);
    }

/* -------------------------------------------------------------------- */
/*      Start the reader and run till complete.                         */
/* -------------------------------------------------------------------- */
    GDALAsyncReader* ario = poBaseDS->
        BeginAsyncReader(xOff, yOff, xSize, ySize,
                         pImage, nBufXSize,nBufYSize,
                         eDataType, 1, &nBand, 
                         nPixelSpace, nLineSpace, nBandSpace, NULL);

    if( ario == NULL )
        return CE_Failure;

    int nXBufOff; // absolute x image offset
    int nYBufOff; // abolute y image offset
    int nXBufSize;
    int nYBufSize;

    GDALAsyncStatusType status;

    do 
    {
        status = ario->GetNextUpdatedRegion(-1.0, 
                                            &nXBufOff, &nYBufOff, 
                                            &nXBufSize, &nYBufSize );
    } while (status != GARIO_ERROR && status != GARIO_COMPLETE );

    poBaseDS->EndAsyncReader(ario);

    if (status == GARIO_ERROR)
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr 
JPIPKAKRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType, 
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg)
    
{
/* -------------------------------------------------------------------- */
/*      We need various criteria to skip out to block based methods.    */
/* -------------------------------------------------------------------- */
    if( poBaseDS->TestUseBlockIO( nXOff, nYOff, nXSize, nYSize, 
                                  nBufXSize, nBufYSize,
                                  eBufType, 1, &nBand ) )
        return GDALPamRasterBand::IRasterIO( 
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, 
            nPixelSpace, nLineSpace, psExtraArg );

/* -------------------------------------------------------------------- */
/*      Otherwise do this as a single uncached async rasterio.          */
/* -------------------------------------------------------------------- */
    GDALAsyncReader* ario = 
        poBaseDS->BeginAsyncReader(nXOff, nYOff, nXSize, nYSize,
                                   pData, nBufXSize, nBufYSize, eBufType, 
                                   1, &nBand,
                                   nPixelSpace, nLineSpace, 0, NULL);
    
    if( ario == NULL )
        return CE_Failure;
    
    GDALAsyncStatusType status;

    do 
    {
        int nXBufOff,nYBufOff,nXBufSize,nYBufSize;

        status = ario->GetNextUpdatedRegion(-1.0, 
                                            &nXBufOff, &nYBufOff, 
                                            &nXBufSize, &nYBufSize );
    } while (status != GARIO_ERROR && status != GARIO_COMPLETE );

    poBaseDS->EndAsyncReader(ario);

    if (status == GARIO_ERROR)
        return CE_Failure;
    else
        return CE_None;
}

/*****************************************/
/*         JPIPKAKDataset()              */
/*****************************************/
JPIPKAKDataset::JPIPKAKDataset()         
{
    pszPath = NULL;
    pszCid = NULL;
    pszProjection = NULL;
	
    poCache = NULL;
    poCodestream = NULL;
    poDecompressor = NULL;

    nPos = 0;
    nVBASLen = 0;
    nVBASFirstByte = 0;

    nClassId = 0;
    nCodestream = 0;
    nDatabins = 0;
    bWindowDone = FALSE;
    bGeoTransformValid = FALSE;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;

    bHighThreadRunning = 0;
    bLowThreadRunning = 0;
    bHighThreadFinished = 0;
    bLowThreadFinished = 0;
    nHighThreadByteCount = 0;
    nLowThreadByteCount = 0;

    pGlobalMutex = CPLCreateMutex();
    CPLReleaseMutex(pGlobalMutex);
}

/*****************************************/
/*         ~JPIPKAKDataset()             */
/*****************************************/
JPIPKAKDataset::~JPIPKAKDataset()
{
    CPLHTTPCleanup();

    Deinitialize();

    CPLFree(pszProjection);
    pszProjection = NULL;

    CPLFree(pszPath);

    if (nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
}

/************************************************************************/
/*                            Deinitialize()                            */
/*                                                                      */
/*      Cleanup stuff that we will rebuild during a                     */
/*      reinitialization.                                               */
/************************************************************************/

void JPIPKAKDataset::Deinitialize()

{
    CPLFree(pszCid);
    pszCid = NULL;

    // frees decompressor as well
    if (poCodestream)
    {
        poCodestream->destroy();
        delete poCodestream;
        poCodestream = NULL;
    }

    delete poDecompressor;
    poDecompressor = NULL;

    delete poCache;
    poCache = NULL;

    bNeedReinitialize = TRUE;
}

/*****************************************/
/*         Initialize()                  */
/*****************************************/
int JPIPKAKDataset::Initialize(const char* pszDatasetName, int bReinitializing )
{
    // set up message handlers
    jpipkak_kdu_cpl_error_message oErrHandler( CE_Failure );
    jpipkak_kdu_cpl_error_message oWarningHandler( CE_Warning );
    kdu_customize_warnings(new jpipkak_kdu_cpl_error_message( CE_Warning ) );
    kdu_customize_errors(new jpipkak_kdu_cpl_error_message( CE_Failure ) );

    // create necessary http headers
    CPLString osHeaders = "HEADERS=Accept: jpp-stream";
    CPLString osPersistent;

    osPersistent.Printf( "PERSISTENT=JPIPKAK:%p", this );

    char *apszOptions[] = { 
        (char *) osHeaders.c_str(),
        (char *) osPersistent.c_str(),
        NULL 
    };

    // Setup url to have http in place of jpip protocol indicator.
    CPLString osURL = "http";
    osURL += (pszDatasetName + 4);
    
    CPLAssert( strncmp(pszDatasetName,"jpip",4) == 0 );

    // make initial request to the server for a session, we are going to 
    // assume that the jpip communication is stateful, rather than one-shot
    // stateless requests append pszUrl with jpip request parameters for a 
    // stateful session (multi-shot communications)
    // "cnew=http&type=jpp-stream&stream=0&tid=0&len="
    CPLString osRequest;
    osRequest.Printf("%s?%s%i", osURL.c_str(),
                     "cnew=http&type=jpp-stream&stream=0&tid=0&len=", 2000);
	
    CPLHTTPResult *psResult = CPLHTTPFetch(osRequest, apszOptions);
	
    if ( psResult == NULL)
        return FALSE;

    if( psResult->nDataLen == 0 || psResult->pabyData == NULL  )
    {

        CPLError(CE_Failure, CPLE_AppDefined, 
                 "No data was returned from the given URL" );
        CPLHTTPDestroyResult( psResult );
        return FALSE;
    }

    if (psResult->nStatus != 0) 
    {
        CPLHTTPDestroyResult( psResult );
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Curl reports error: %d: %s", psResult->nStatus, psResult->pszErrBuf );
        return FALSE;        
    }

    // parse the response headers, and the initial data until we get to the 
    // codestream definition
    char** pszHdrs = psResult->papszHeaders;
    const char* pszCnew = CSLFetchNameValue(pszHdrs, "JPIP-cnew");

    if( pszCnew == NULL )
    {
        if( psResult->pszContentType != NULL 
            && EQUALN(psResult->pszContentType,"text/html",9) )
            CPLDebug( "JPIPKAK", "%s", 
                      psResult->pabyData );

        CPLHTTPDestroyResult( psResult );
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Unable to parse required cnew and tid response headers" );

        return FALSE;    
    }

    // parse cnew response
    // JPIP-cnew:
    // cid=DC69DF980A641A4BBDEB50E484A66578,path=MyPath,transport=http
    char **papszTokens = CSLTokenizeString2( pszCnew, ",", CSLT_HONOURSTRINGS);
    for (int i = 0; i < CSLCount(papszTokens); i++)
    {
        // looking for cid, path
        if (EQUALN(papszTokens[i], "cid", 3))
        {
            char *pszKey = NULL;
            const char *pszValue = CPLParseNameValue(papszTokens[i], &pszKey );
            pszCid = CPLStrdup(pszValue);
            CPLFree( pszKey );
        }

        if (EQUALN(papszTokens[i], "path", 4))
        {
            char *pszKey = NULL;
            const char *pszValue = CPLParseNameValue(papszTokens[i], &pszKey );
            pszPath = CPLStrdup(pszValue);
            CPLFree( pszKey );
        }
    }

    CSLDestroy(papszTokens);

    if( pszPath == NULL || pszCid == NULL )
    {
        CPLHTTPDestroyResult(psResult);
        CPLError(CE_Failure, CPLE_AppDefined, "Error parsing path and cid from cnew - %s", pszCnew);
        return FALSE;
    }

    // ok, good to go with jpip, get to the codestream before returning 
    // successful initialisation of the driver
    try
    {
        poCache = new kdu_cache();
        poCodestream = new kdu_codestream();
        poDecompressor = new kdu_region_decompressor();

        int bFinished = FALSE;
        int bError = FALSE;
        bFinished = ReadFromInput(psResult->pabyData, psResult->nDataLen,
                                  bError );
        CPLHTTPDestroyResult(psResult);

        // continue making requests in the main thread to get all the available 
        // metadata for data bin 0, and reach the codestream

        // format the new request
        // and set as pszRequestUrl;
        // get the protocol from the original request
        size_t found = osRequest.find_first_of("/");
        CPLString osProtocol = osRequest.substr(0, found + 2);
        osRequest.erase(0, found + 2);
        // find context path
        found = osRequest.find_first_of("/");
        osRequest.erase(found);
			
        osRequestUrl.Printf("%s%s/%s?cid=%s&stream=0&len=%i", osProtocol.c_str(), osRequest.c_str(), pszPath, pszCid, 2000);

        while (!bFinished && !bError )
        {
            CPLHTTPResult *psResult = CPLHTTPFetch(osRequestUrl, apszOptions);
            bFinished = ReadFromInput(psResult->pabyData, psResult->nDataLen,
                                      bError );
            CPLHTTPDestroyResult(psResult);
        }

        if( bError )
            return FALSE;

        // clean up osRequest, remove variable len= parameter
        size_t pos = osRequestUrl.find_last_of("&");	
        osRequestUrl.erase(pos);

        // create codestream
        poCache->set_read_scope(KDU_MAIN_HEADER_DATABIN, 0, 0);
        poCodestream->create(poCache);
        poCodestream->set_persistent();

/* -------------------------------------------------------------------- */
/*      If this is a reinitialization then we can hop out at this       */
/*      point.  The rest of the stuff was already done, and             */
/*      hopefully the configuration is unchanged.                       */
/* -------------------------------------------------------------------- */
        if( bReinitializing )
            return TRUE;

/* -------------------------------------------------------------------- */
/*      Collect GDAL raster configuration information.                  */
/* -------------------------------------------------------------------- */
        kdu_channel_mapping oChannels;
        oChannels.configure(*poCodestream);
        kdu_coords* ref_expansion = new kdu_coords(1, 1);
					
        // get available resolutions, image width / height etc.
        kdu_dims view_dims = poDecompressor->
            get_rendered_image_dims(*poCodestream, &oChannels, -1, 0,
                                    *ref_expansion, *ref_expansion, 
                                    KDU_WANT_OUTPUT_COMPONENTS);

        nRasterXSize = view_dims.size.x;
        nRasterYSize = view_dims.size.y;

        // Establish the datatype - we will use the same datatype for
        // all bands based on the first.  This really doesn't do something
        // great for >16 bit images.
        if( poCodestream->get_bit_depth(0) > 8 
            && poCodestream->get_signed(0) )
        {
            eDT = GDT_Int16;
        }
        else if( poCodestream->get_bit_depth(0) > 8
                 && !poCodestream->get_signed(0) )
        {
            eDT = GDT_UInt16;
        }
        else
            eDT = GDT_Byte;

        if( poCodestream->get_bit_depth(0) % 8 != 8
            && poCodestream->get_bit_depth(0) < 16 )
            SetMetadataItem( 
                "NBITS", 
                CPLString().Printf("%d",poCodestream->get_bit_depth(0)), 
                "IMAGE_STRUCTURE" );

        // TODO add color interpretation

        // calculate overviews
        siz_params* siz_in = poCodestream->access_siz();
        kdu_params* cod_in = siz_in->access_cluster("COD");

        delete ref_expansion;

        siz_in->get("Scomponents", 0, 0, nComps);
        siz_in->get("Sprecision", 0, 0, nBitDepth);

        cod_in->get("Clayers", 0, 0, nQualityLayers);
        cod_in->get("Clevels", 0, 0, nResLevels);

        bYCC=TRUE;
        cod_in->get("Cycc", 0, 0, bYCC);

    }
    catch(...)
    {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Trapped Kakadu exception attempting to initialize JPIP access." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      YCC images are always processed as 3 bands.                     */
/* -------------------------------------------------------------------- */
    if( bYCC )
        nBands = 3;
    else
        nBands = nComps;

/* -------------------------------------------------------------------- */
/*      Setup band objects.                                             */
/* -------------------------------------------------------------------- */
    int iBand;
    
    for( iBand = 1; iBand <= nBands; iBand++ )
    {
        JPIPKAKRasterBand *poBand = 
            new JPIPKAKRasterBand(iBand,0,poCodestream,nResLevels,
                                  this );
	
        SetBand( iBand, poBand );
    }

    // set specific metadata items
    CPLString osNQualityLayers;
    osNQualityLayers.Printf("%i", nQualityLayers);
    CPLString osNResolutionLevels;
    osNResolutionLevels.Printf("%i", nResLevels);
    CPLString osNComps;
    osNComps.Printf("%i", nComps);
    CPLString osBitDepth;
    osBitDepth.Printf("%i", nBitDepth);

    SetMetadataItem("JPIP_NQUALITYLAYERS", osNQualityLayers.c_str(), "JPIP");
    SetMetadataItem("JPIP_NRESOLUTIONLEVELS", osNResolutionLevels.c_str(), "JPIP");
    SetMetadataItem("JPIP_NCOMPS", osNComps.c_str(), "JPIP");
    SetMetadataItem("JPIP_SPRECISION", osBitDepth.c_str(), "JPIP");

    if( bYCC )
        SetMetadataItem("JPIP_YCC", "YES", "JPIP");
    else
        SetMetadataItem("JPIP_YCC", "NO", "JPIP");
	
/* ==================================================================== */
/*      Parse geojp2, or gmljp2, we will assume that the core           */
/*      metadata  of gml or a geojp2 uuid have been sent in the         */
/*      initial metadata response.                                      */
/*      If the server has used placeholder boxes for this               */
/*      information then the image will be interpreted as x,y           */
/* ==================================================================== */
    GDALJP2Metadata oJP2Geo;
    int nLen = poCache->get_databin_length(KDU_META_DATABIN, nCodestream, 0);

    if( nLen == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Unable to open stream to parse metadata boxes" );
        return FALSE;
    }

    // create in memory file using vsimem
    CPLString osFileBoxName;
    osFileBoxName.Printf("/vsimem/jpip/%s.dat", pszCid);
    VSILFILE *fpLL = VSIFOpenL(osFileBoxName.c_str(), "w+");
    poCache->set_read_scope(KDU_META_DATABIN, nCodestream, 0);
    kdu_byte* pabyBuffer = (kdu_byte *)CPLMalloc(nLen);
    poCache->read(pabyBuffer, nLen);
    VSIFWriteL(pabyBuffer, nLen, 1, fpLL);
    CPLFree( pabyBuffer );

    VSIFFlushL(fpLL);
    VSIFSeekL(fpLL, 0, SEEK_SET);

    nPamFlags |= GPF_NOSAVE;

    try
    {
        oJP2Geo.ReadBoxes(fpLL);
        // parse gml first, followed by geojp2 as a fallback
        if (oJP2Geo.ParseGMLCoverageDesc() || oJP2Geo.ParseJP2GeoTIFF())
        {
            pszProjection = CPLStrdup(oJP2Geo.pszProjection);
            bGeoTransformValid = TRUE;

            memcpy(adfGeoTransform, oJP2Geo.adfGeoTransform, 
                   sizeof(double) * 6 );
            nGCPCount = oJP2Geo.nGCPCount;
            pasGCPList = oJP2Geo.pasGCPList;

            oJP2Geo.pasGCPList = NULL;
            oJP2Geo.nGCPCount = 0;
						
            int iBox;

            for( iBox = 0; 
                 oJP2Geo.papszGMLMetadata
                     && oJP2Geo.papszGMLMetadata[iBox] != NULL; 
                 iBox++ )
            {
                char *pszName = NULL;
                const char *pszXML = 
                    CPLParseNameValue( oJP2Geo.papszGMLMetadata[iBox], 
                                       &pszName );
                CPLString osDomain;
                char *apszMDList[2];

                osDomain.Printf( "xml:%s", pszName );
                apszMDList[0] = (char *) pszXML;
                apszMDList[1] = NULL;

                GDALPamDataset::SetMetadata( apszMDList, osDomain );
                CPLFree( pszName );
            }
        }
        else
        {
            // treat as cartesian, no geo metadata
            CPLError(CE_Warning, CPLE_AppDefined, 
                     "Parsed metadata boxes from jpip stream, geographic metadata not found - is the server using placeholders for this data?" );
						
        }
    }
    catch(...)
    {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Unable to parse geographic metadata boxes from jpip stream" );
    }

    VSIFCloseL(fpLL);
    VSIUnlink( osFileBoxName.c_str());

    bNeedReinitialize = FALSE;

    return TRUE;
}

/******************************************/
/*           ReadVBAS()                   */
/******************************************/
long JPIPKAKDataset::ReadVBAS(GByte* pabyData, int nLen )
{
    int c = -1;
    long val = 0;
    nVBASLen = 0;

    while ((c & 0x80) != 0)
    {
        if (nVBASLen >= 9)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VBAS Length not supported");
            return -1;
        }

        if (nPos > nLen)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "EOF reached before completing VBAS");
            return -1;
        }

#ifdef PST_DEBUG
        if( nPSTThisInstance == nPSTTargetInstance 
            && nPos >= nPSTTargetOffset )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Artificial PST EOF reached before completing VBAS");
            return -1;
        }
#endif

        c = pabyData[nPos];
        nPos++;

        val = (val << 7) | (long)(c & 0x7F);

        if (nVBASLen == 0)
            nVBASFirstByte = c;

        nVBASLen++;
    }

    return val;
}

/******************************************/
/*            ReadSegment()               */
/******************************************/
JPIPDataSegment* JPIPKAKDataset::ReadSegment(GByte* pabyData, int nLen,
                                             int& bError )
{
    long nId = ReadVBAS(pabyData, nLen);
    bError = FALSE;

    if (nId < 0)
    {
        bError = TRUE;
        return NULL;
    }
    else
    {
        JPIPDataSegment* segment = new JPIPDataSegment();
        segment->SetId(nId);

        if (nVBASFirstByte == 0)
        {
            segment->SetEOR(TRUE);
            segment->SetId(pabyData[nPos]);
        }
        else
        {
            segment->SetEOR(FALSE);
            nId &= ~(0x70 << ((nVBASLen -1) * 7));
            segment->SetId(nId);
            segment->SetFinal((nVBASFirstByte & 0x10) != 0);

            int m = (nVBASFirstByte & 0x7F) >> 5;

            if (m == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid Bin-ID value format");
                bError = TRUE;
                return NULL;
            }
            else if (m >= 2) {
                nClassId = ReadVBAS(pabyData, nLen);
                if (m > 2)
                {
                    nCodestream = ReadVBAS(pabyData, nLen);
                    if( nCodestream < 0 )
                    {
                        bError = TRUE;
                        return NULL;
                    }
                }
            }

            long nNextVal;

            segment->SetClassId(nClassId);
            segment->SetCodestreamIdx(nCodestream);
            
            nNextVal = ReadVBAS(pabyData, nLen);
            if( nNextVal == -1 )
            {
                bError = TRUE;
                return NULL;
            }
            else
                segment->SetOffset( nNextVal );

            nNextVal = ReadVBAS(pabyData, nLen);
            if( nNextVal == -1 )
            {
                bError = TRUE;
                return NULL;
            }
            else
                segment->SetLen(nNextVal);
        }

        if ((segment->GetLen() > 0) && (!segment->IsEOR()))
        {
            GByte* pabyDataSegment = (GByte *) CPLCalloc(1,segment->GetLen());
			
            // copy data from input array pabyData to the data segment
            memcpy(pabyDataSegment, 
                   pabyData + nPos,
                   segment->GetLen());

            segment->SetData(pabyDataSegment);
        }

        nPos += segment->GetLen();

        if (!segment->IsEOR())
            nDatabins++;

        if ((segment->GetId() == JPIPKAKDataset::JPIP_EOR_WINDOW_DONE) && (segment->IsEOR()))
            bWindowDone = TRUE;

        return segment;
    }
}

/******************************************/
/*           KakaduClassId()              */
/******************************************/
int JPIPKAKDataset::KakaduClassId(int nClassId)
{
    if (nClassId == 0)
        return KDU_PRECINCT_DATABIN;
    else if (nClassId == 2)
        return KDU_TILE_HEADER_DATABIN;
    else if (nClassId == 6)
        return KDU_MAIN_HEADER_DATABIN;
    else if (nClassId == 8)  
        return KDU_META_DATABIN;
    else if (nClassId == 4)
        return KDU_TILE_DATABIN;
    else
        return -1;
}

/******************************************/
/*            ReadFromInput()             */
/******************************************/
int JPIPKAKDataset::ReadFromInput(GByte* pabyData, int nLen, int &bError )
{
    int res = FALSE;
    bError = FALSE;

    if (nLen <= 0 )
        return FALSE;

#ifdef PST_DEBUG
    nPSTThisInstance++;
    if( CPLGetConfigOption( "PST_OFFSET", NULL ) != NULL )
    {
        nPSTTargetOffset = atoi(CPLGetConfigOption("PST_OFFSET","0"));
        nPSTTargetInstance = 0;
    }
    if( CPLGetConfigOption( "PST_INSTANCE", NULL ) != NULL )
    {
        nPSTTargetInstance = atoi(CPLGetConfigOption("PST_INSTANCE","0"));
    }
    
    if( nPSTTargetOffset != -1 && nPSTThisInstance == 0 )
    {
        CPLDebug( "JPIPKAK", "Premature Stream Termination Activated, PST_OFFSET=%d, PST_INSTANCE=%d",
                  nPSTTargetOffset, nPSTTargetInstance );
    }
    if( nPSTTargetOffset != -1 
        && nPSTThisInstance == nPSTTargetInstance )
    {
        CPLDebug( "JPIPKAK", "Premature Stream Termination in force for this input instance, PST_OFFSET=%d, data length=%d", 
                  nPSTTargetOffset, nLen );
    }
#endif // def PST_DEBUG 
    
    // parse the data stream, reading the vbas and adding to the kakadu cache
    // we could parse all the boxes by hand, and just add data to the kakadu cache
    // we will do it the easy way and retrieve the metadata through the kakadu query api
    
    nPos = 0;
    JPIPDataSegment* pSegment = NULL;
    
    while ((pSegment = ReadSegment(pabyData, nLen, bError)) != NULL)
    {
	
        if (pSegment->IsEOR())
        {		
            if ((pSegment->GetId() == JPIPKAKDataset::JPIP_EOR_IMAGE_DONE) || 
                (pSegment->GetId() == JPIPKAKDataset::JPIP_EOR_WINDOW_DONE))
                res = TRUE;
            
            delete pSegment;
            break;
        }
        else
        {
            // add data to kakadu
            //CPLDebug("JPIPKAK", "Parsed JPIP Segment class=%i stream=%i id=%i offset=%i len=%i isFinal=%i isEOR=%i", pSegment->GetClassId(), pSegment->GetCodestreamIdx(), pSegment->GetId(), pSegment->GetOffset(), pSegment->GetLen(), pSegment->IsFinal(), pSegment->IsEOR());
            poCache->add_to_databin(KakaduClassId(pSegment->GetClassId()), pSegment->GetCodestreamIdx(),
                                    pSegment->GetId(), pSegment->GetData(), pSegment->GetOffset(), pSegment->GetLen(), pSegment->IsFinal());
            
            delete pSegment;
        }
    }

    return res;
}


/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *JPIPKAKDataset::GetProjectionRef()

{
    if( pszProjection && *pszProjection )
        return( pszProjection );
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JPIPKAKDataset::GetGeoTransform( double * padfTransform )

{
    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    
        return CE_None;
    }
    else
        return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int JPIPKAKDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *JPIPKAKDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *JPIPKAKDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JPIPKAKDataset::IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType, 
                                  int nBandCount, int *panBandMap,
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GSpacing nBandSpace,
                                  GDALRasterIOExtraArg* psExtraArg)

{
/* -------------------------------------------------------------------- */
/*      We need various criteria to skip out to block based methods.    */
/* -------------------------------------------------------------------- */
    if( TestUseBlockIO( nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                        eBufType, nBandCount, panBandMap ) )
        return GDALPamDataset::IRasterIO( 
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, 
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace, psExtraArg );

/* -------------------------------------------------------------------- */
/*      Otherwise do this as a single uncached async rasterio.          */
/* -------------------------------------------------------------------- */
    GDALAsyncReader* ario = 
        BeginAsyncReader(nXOff, nYOff, nXSize, nYSize,
                         pData, nBufXSize, nBufYSize, eBufType, 
                         nBandCount, panBandMap, 
                         nPixelSpace, nLineSpace, nBandSpace, NULL);

    if( ario == NULL )
        return CE_Failure;
    
    GDALAsyncStatusType status;

    do 
    {
        int nXBufOff,nYBufOff,nXBufSize,nYBufSize;

        status = ario->GetNextUpdatedRegion(-1.0, 
                                            &nXBufOff, &nYBufOff, 
                                            &nXBufSize, &nYBufSize );
    } while (status != GARIO_ERROR && status != GARIO_COMPLETE );

    EndAsyncReader(ario);

    if (status == GARIO_ERROR)
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                           TestUseBlockIO()                           */
/************************************************************************/

int 
JPIPKAKDataset::TestUseBlockIO( CPL_UNUSED int nXOff, CPL_UNUSED int nYOff,
                                int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize,
                                CPL_UNUSED GDALDataType eDataType, 
                                int nBandCount, int *panBandList )

{
/* -------------------------------------------------------------------- */
/*      Due to limitations in DirectRasterIO() we can only handle       */
/*      it no duplicates in the band list.                              */
/* -------------------------------------------------------------------- */
    int i, j; 
    
    for( i = 0; i < nBandCount; i++ )
    {
        for( j = i+1; j < nBandCount; j++ )
            if( panBandList[j] == panBandList[i] )
                return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      The rest of the rules are io strategy stuff, and use            */
/*      configuration checks.                                           */
/* -------------------------------------------------------------------- */
    int bUseBlockedIO = bForceCachedIO;

    if( nYSize == 1 || nXSize * ((double) nYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( nBufYSize == 1 || nBufXSize * ((double) nBufYSize) < 100.0 )
        bUseBlockedIO = TRUE;

    if( bUseBlockedIO
        && CSLTestBoolean( CPLGetConfigOption( "GDAL_ONE_BIG_READ", "NO") ) )
        bUseBlockedIO = FALSE;

    return bUseBlockedIO;
}

/*************************************************************************/
/*                     BeginAsyncReader()                              */
/*************************************************************************/

GDALAsyncReader* 
JPIPKAKDataset::BeginAsyncReader(int xOff, int yOff,
                                   int xSize, int ySize, 
                                   void *pBuf,
                                   int bufXSize, int bufYSize,
                                   GDALDataType bufType,
                                   int nBandCount, int* pBandMap,
                                   int nPixelSpace, int nLineSpace,
                                   int nBandSpace,
                                   char **papszOptions)
{
    CPLDebug( "JPIP", "BeginAsyncReadeR(%d,%d,%d,%d -> %dx%d)",
              xOff, yOff, xSize, ySize, bufXSize, bufYSize );

/* -------------------------------------------------------------------- */
/*      Recreate the code stream access if needed.                      */
/* -------------------------------------------------------------------- */
    if( bNeedReinitialize )
    {
        CPLDebug( "JPIPKAK", "\n\nReinitializing after error! ******\n" );

        Deinitialize();
        if( !Initialize(GetDescription(),TRUE) )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Provide default packing if needed.                              */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize(bufType) / 8;
    if( nLineSpace == 0 )
        nLineSpace = nPixelSpace * bufXSize;
    if( nBandSpace == 0 )
        nBandSpace = nLineSpace * bufYSize;
    
/* -------------------------------------------------------------------- */
/*      check we have sensible values for windowing.                    */
/* -------------------------------------------------------------------- */
    if (xOff > GetRasterXSize()
        || yOff > GetRasterYSize()
        || (xOff + xSize) > GetRasterXSize()
        || (yOff + ySize) > GetRasterYSize() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Requested window (%d,%d %dx%d) off dataset.",
                  xOff, yOff, xSize, ySize );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Record request information.                                     */
/* -------------------------------------------------------------------- */
    JPIPKAKAsyncReader* ario = new JPIPKAKAsyncReader();
    ario->poDS = this;
    ario->nBufXSize = bufXSize;
    ario->nBufYSize = bufYSize;
    ario->eBufType = bufType;
    ario->nBandCount = nBandCount;
    ario->nXOff = xOff;
    ario->nYOff = yOff;
    ario->nXSize = xSize;
    ario->nYSize = ySize;

    ario->panBandMap = new int[nBandCount];
    if (pBandMap)
    {
        for (int i = 0; i < nBandCount; i++)
            ario->panBandMap[i] = pBandMap[i];
    }
    else
    {
        for (int i = 0; i < nBandCount; i++)
            ario->panBandMap[i] = i+1;
    }

/* -------------------------------------------------------------------- */
/*      If the buffer type is of other than images type, we need to     */
/*      allocate a private buffer the same type as the image which      */
/*      will be converted later.                                        */
/* -------------------------------------------------------------------- */
    if( bufType != eDT )
    {
        ario->nPixelSpace = GDALGetDataTypeSize(eDT) / 8;
        ario->nLineSpace = ario->nPixelSpace * bufXSize;
        ario->nBandSpace = ario->nLineSpace * bufYSize;

        ario->nAppPixelSpace = nPixelSpace;
        ario->nAppLineSpace = nLineSpace;
        ario->nAppBandSpace = nBandSpace;

        ario->pBuf = VSIMalloc3(bufXSize,bufYSize,ario->nPixelSpace*nBandCount);
        if( ario->pBuf == NULL )
        {
            delete ario;
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Failed to allocate %d byte work buffer.",
                      bufXSize * bufYSize * ario->nPixelSpace );
            return NULL;
        }

        ario->pAppBuf = pBuf;
    }
    else
    {
        ario->pBuf = pBuf;
        ario->pAppBuf = pBuf;

        ario->nAppPixelSpace = ario->nPixelSpace = nPixelSpace;
        ario->nAppLineSpace = ario->nLineSpace = nLineSpace;
        ario->nAppBandSpace = ario->nBandSpace = nBandSpace;
    }

    // parse options
    const char* pszLevel = CSLFetchNameValue(papszOptions, "LEVEL");
    const char* pszLayers = CSLFetchNameValue(papszOptions, "LAYERS");
    const char* pszPriority = CSLFetchNameValue(papszOptions, "PRIORITY");
	
    if (pszLayers)
        ario->nQualityLayers = atoi(pszLayers);
    else
        ario->nQualityLayers = nQualityLayers;

    if (pszPriority)
    {
        if (EQUAL(pszPriority, "0"))
            ario->bHighPriority = 0;
        else
            ario->bHighPriority = 1;
    }
    else
        ario->bHighPriority = 1;

/* -------------------------------------------------------------------- */
/*      Select an appropriate level based on the ratio of buffer        */
/*      size to full resolution image.  We aim for the next             */
/*      resolution *lower* than we might expect for the target          */
/*      buffer unless it falls on a power of two.  This is because      */
/*      the region decompressor only seems to support upsampling        */
/*      via the numerator/denominator magic.                            */
/* -------------------------------------------------------------------- */
    if (pszLevel)
        ario->nLevel = atoi(pszLevel);
    else
    {
        int nRXSize = xSize, nRYSize = ySize;
        ario->nLevel = 0;

        while( ario->nLevel < nResLevels
               && (nRXSize > bufXSize || nRYSize > bufYSize) )
        {
            nRXSize = (nRXSize+1) / 2;
            nRYSize = (nRYSize+1) / 2;
            ario->nLevel++;
        }
    }

    ario->Start();

    return ario;
}

/************************************************************************/
/*                  EndAsyncReader()                                  */
/************************************************************************/

void JPIPKAKDataset::EndAsyncReader(GDALAsyncReader *poARIO)
{
    delete poARIO;
}


/*****************************************/
/*             Open()                    */
/*****************************************/
GDALDataset *JPIPKAKDataset::Open(GDALOpenInfo * poOpenInfo)
{
    // test jpip and jpips, assuming jpip is using http as the transport layer
    // jpip = http, jpips = https (note SSL is allowed, but jpips is not in the ISO spec)
    if (EQUALN(poOpenInfo->pszFilename,"jpip://", 7)
        || EQUALN(poOpenInfo->pszFilename,"jpips://",8))
    {
        // perform the initial connection
        // using cpl_http for the connection
        if  (CPLHTTPEnabled() == TRUE)
        {
            JPIPKAKDataset *poDS;
            poDS = new JPIPKAKDataset();
            if (poDS->Initialize(poOpenInfo->pszFilename,FALSE))
            {
                poDS->SetDescription( poOpenInfo->pszFilename );
                return poDS;
            }
            else
            {
                delete poDS;
                return NULL;
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open %s within JPIPKAK driver CPL HTTP not enabled.\n",
                      poOpenInfo->pszFilename );
            return NULL;
        }
    }
    else
        return NULL;
}

/************************************************************************/
/*                        GDALRegister_JPIPKAK()			*/
/************************************************************************/

void GDALRegister_JPIPKAK()
{
    GDALDriver *poDriver;
	
    if (! GDAL_CHECK_VERSION("JPIPKAK driver"))
        return;

    if( GDALGetDriverByName( "JPIPKAK" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "JPIPKAK" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "JPIP (based on Kakadu)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_jpipkak.html" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jpp-stream" );

        poDriver->pfnOpen = JPIPKAKDataset::Open;
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

/************************************************************************/
/*                         JPIPKAKAsyncReader                         */
/************************************************************************/
JPIPKAKAsyncReader::JPIPKAKAsyncReader()
{
    panBandMap = NULL;
    pAppBuf = pBuf = NULL;
    nDataRead = 0;
}

/************************************************************************/
/*                        ~JPIPKAKAsyncReader                         */
/************************************************************************/
JPIPKAKAsyncReader::~JPIPKAKAsyncReader()
{
    Stop();

    // don't own the buffer
    delete [] panBandMap;

    if( pAppBuf != pBuf )
        CPLFree( pBuf );
}

/************************************************************************/
/*                               Start()                                */
/************************************************************************/

void JPIPKAKAsyncReader::Start()
{
    JPIPKAKDataset *poJDS = (JPIPKAKDataset*)poDS;

    // stop the currently running thread
    // start making requests to the server to the server
    nDataRead = 0;
    bComplete = 0;

    // check a thread is not already running
    if (((bHighPriority) && (poJDS->bHighThreadRunning)) || 
        ((!bHighPriority) && (poJDS->bLowThreadRunning)))
        CPLError(CE_Failure, CPLE_AppDefined, "JPIPKAKAsyncReader supports at most two concurrent server communication threads");
    else
    {
        // Ensure we are working against full res
        ((JPIPKAKDataset*)poDS)->poCodestream->
            apply_input_restrictions( 0, 0, 0, 0, NULL );

        // calculate the url the worker function is going to retrieve
        // calculate the kakadu adjust image size
        channels.configure(*(((JPIPKAKDataset*)poDS)->poCodestream));

        // find current canvas width and height in the cache and check we don't
        // exceed this in our process request
        kdu_dims view_dims;	
        kdu_coords ref_expansion;
        ref_expansion.x = 1;
        ref_expansion.y = 1;

        view_dims = ((JPIPKAKDataset*)poDS)->poDecompressor->
            get_rendered_image_dims(*((JPIPKAKDataset*)poDS)->poCodestream, &channels, 
                                    -1, nLevel, 
                                    ref_expansion );

        kdu_coords* view_siz = view_dims.access_size();
		
        // Establish the decimation implied by our resolution level.
        int nRes = 1;
        if (nLevel > 0)
            nRes = 2 << (nLevel - 1);

        // setup expansion to account for the difference between 
        // the selected level and the buffer resolution.
        exp_numerator.x = nBufXSize;
        exp_numerator.y = nBufYSize;

        exp_denominator.x = (int) ceil(nXSize / (double) nRes);
        exp_denominator.y = (int) ceil(nYSize / (double) nRes);

        // formulate jpip parameters and adjust offsets for current level
        int fx = view_siz->x / nRes;
        int fy = view_siz->y / nRes;

        rr_win.pos.x = (int) ceil(nXOff / (1.0 * nRes)); // roffx
        rr_win.pos.y = (int) ceil(nYOff / (1.0 * nRes)); // roffy
        rr_win.size.x = (int) ceil(nXSize / (1.0 * nRes)); // rsizx
        rr_win.size.y = (int) ceil(nYSize / (1.0 * nRes)); // rsizy

        if ( rr_win.pos.x + rr_win.size.x > fx)
            rr_win.size.x = fx - rr_win.pos.x;
        if ( rr_win.pos.y + rr_win.size.y > fy)
            rr_win.size.y = fy - rr_win.pos.y;

        CPLString jpipUrl;
        CPLString comps;

        if( poJDS->bYCC )
        {
            comps = "0,1,2";
        }
        else
        {
            for (int i = 0; i < nBandCount; i++)
                comps.Printf("%s%i,", comps.c_str(), panBandMap[i]-1);

            comps.erase(comps.length() -1);
        }
	
        jpipUrl.Printf("%s&type=jpp-stream&roff=%i,%i&rsiz=%i,%i&fsiz=%i,%i,closest&quality=%i&comps=%s", 
                       ((JPIPKAKDataset*)poDS)->osRequestUrl.c_str(),
                       rr_win.pos.x, rr_win.pos.y,
                       rr_win.size.x, rr_win.size.y,
                       fx, fy,
                       nQualityLayers, comps.c_str());

        JPIPRequest* pRequest = new JPIPRequest();
        pRequest->bPriority = bHighPriority;
        pRequest->osRequest = jpipUrl;
        pRequest->poARIO = this;

        if( bHighPriority )
            poJDS->bHighThreadFinished = 0;
        else
            poJDS->bLowThreadFinished = 0;
        
        //CPLDebug("JPIPKAKAsyncReader", "THREADING TURNED OFF");
        if (CPLCreateThread(JPIPWorkerFunc, pRequest) == -1)
            CPLError(CE_Failure, CPLE_AppDefined, 
                     "Unable to create worker jpip  thread" );
        // run in main thread as a test
        //JPIPWorkerFunc(pRequest);
    }
}

/************************************************************************/
/*                                Stop()                                */
/************************************************************************/
void JPIPKAKAsyncReader::Stop()
{
    JPIPKAKDataset *poJDS = (JPIPKAKDataset*)poDS;

    bComplete = 1;
    if (poJDS->pGlobalMutex)
    {
        if (((bHighPriority) && (!poJDS->bHighThreadFinished)) || 
            ((!bHighPriority) && (!poJDS->bLowThreadFinished)))
        {
            CPLDebug( "JPIPKAK", "JPIPKAKAsyncReader::Stop() requested." );

            // stop the thread
            if (bHighPriority)
            {
                CPLAcquireMutex(poJDS->pGlobalMutex, 100.0);
                poJDS->bHighThreadRunning = 0;
                CPLReleaseMutex(poJDS->pGlobalMutex);
		
                while (!poJDS->bHighThreadFinished)
                    CPLSleep(0.1);
            }
            else
            {
                CPLAcquireMutex(poJDS->pGlobalMutex, 100.0);
                poJDS->bLowThreadRunning = 0;
                CPLReleaseMutex(poJDS->pGlobalMutex);
		
                while (!poJDS->bLowThreadFinished)
                {
                    CPLSleep(0.1);
                }
            }
            CPLDebug( "JPIPKAK", "JPIPKAKAsyncReader::Stop() confirmed." );
        }
    }
}

/************************************************************************/
/*                        GetNextUpdatedRegion()                        */
/************************************************************************/

GDALAsyncStatusType 
JPIPKAKAsyncReader::GetNextUpdatedRegion(double dfTimeout,
                                         int* pnxbufoff,
                                         int* pnybufoff,
                                         int* pnxbufsize,
                                         int* pnybufsize)
{
    GDALAsyncStatusType result = GARIO_ERROR;
    JPIPKAKDataset *poJDS = (JPIPKAKDataset*)poDS;

    long nSize = 0;
    // take a snapshot of the volatile variables
    if (bHighPriority)
    {
        const long s = poJDS->nHighThreadByteCount - nDataRead;
        nSize = s;
    }
    else
    {
        const long s = poJDS->nLowThreadByteCount - nDataRead;
        nSize = s;
    }

/* -------------------------------------------------------------------- */
/*      Wait for new data to return if required.                        */
/* -------------------------------------------------------------------- */
    if ((nSize == 0) && dfTimeout != 0 )
    {
        // poll for change in cache size
        clock_t end_wait = 0;

        end_wait = clock() + (int) (dfTimeout * CLOCKS_PER_SEC); 
        while ((nSize == 0) && ((bHighPriority && poJDS->bHighThreadRunning) ||
                                (!bHighPriority && poJDS->bLowThreadRunning)))
        {
            if (end_wait)
                if (clock() > end_wait && dfTimeout >= 0 )
                    break;

            CPLSleep(0.1);

            if (bHighPriority)
            {
                const long s = poJDS->nHighThreadByteCount - nDataRead;
                nSize = s;
            }
            else
            {
                const long s = poJDS->nLowThreadByteCount - nDataRead;
                nSize = s;
            }
        }
    }

    // if there is no pending data and we don't want to wait.
    if( nSize == 0 )
    {
        *pnxbufoff = 0;
        *pnybufoff = 0;
        *pnxbufsize = 0;
        *pnybufsize = 0;		

        // Indicate an error if the thread finished prematurely
        if( (bHighPriority 
             && !poJDS->bHighThreadRunning 
             && poJDS->bHighThreadFinished)
            || (!bHighPriority 
                && !poJDS->bLowThreadRunning 
                && poJDS->bLowThreadFinished) )
        {
            if( osErrorMsg != "" )
                CPLError( CE_Failure, CPLE_AppDefined,
                          "%s", osErrorMsg.c_str() );
            else
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Working thread failed without complete data. (%d,%d,%d)",
                          bHighPriority, 
                          poJDS->bHighThreadRunning,
                          poJDS->bHighThreadFinished );
            poJDS->bNeedReinitialize = TRUE;
            return GARIO_ERROR;
        }

        // Otherwise there is still pending data to wait for.
        return GARIO_PENDING;
    }

/* -------------------------------------------------------------------- */
/*      Establish the canvas region with the expansion factor           */
/*      applied, and compute region from the original window cut        */
/*      down to the target canvas.                                      */
/* -------------------------------------------------------------------- */
    kdu_dims view_dims, region;
    int nBytesPerPixel = GDALGetDataTypeSize(poJDS->eDT) / 8;
    int nPrecision = 0;

    try
    {
        // Ensure we are working against full res
        poJDS->poCodestream->apply_input_restrictions( 0, 0, 0, 0, NULL );

        view_dims = poJDS->poDecompressor->get_rendered_image_dims(
            *poJDS->poCodestream, &channels, 
            -1, nLevel, exp_numerator, exp_denominator );

        double x_ratio, y_ratio;
        x_ratio = view_dims.size.x / (double) poDS->GetRasterXSize();
        y_ratio = view_dims.size.y / (double) poDS->GetRasterYSize();

        region = rr_win;

        region.pos.x = (int) ceil(region.pos.x * x_ratio);
        region.pos.y = (int) ceil(region.pos.y * y_ratio);
        region.size.x = (int) ceil(region.size.x * x_ratio);
        region.size.y = (int) ceil(region.size.y * y_ratio);

        region.size.x = MIN(region.size.x,nBufXSize);
        region.size.y = MIN(region.size.y,nBufYSize);
        
        if( region.pos.x + region.size.x > view_dims.size.x )
            region.size.x = view_dims.size.x - region.pos.x;
        if( region.pos.y + region.size.y > view_dims.size.y )
            region.size.y = view_dims.size.y - region.pos.y;
        
        region.pos += view_dims.pos;
    
        CPLAssert( nBytesPerPixel == 1 || nBytesPerPixel == 2  );

        if( poJDS->poCodestream->get_bit_depth(0) > 16 )
            nPrecision = 16;
    }
    catch(...)
    {
        // The error handler should already have posted an error message.
        return GARIO_ERROR;
    }

/* ==================================================================== */
/*      Now we process the available cached jpeg2000 data into          */
/*      imagery.  The kdu_region_decompressor only seems to support     */
/*      reading back one or three components at a time, we may need     */
/*      to do severalp processing passes to get the bands we            */
/*      want. We try to do groups of three were possible, and handle    */
/*      the rest one band at a time.                                    */
/* ==================================================================== */
    int nBandsCompleted = 0;

    while( nBandsCompleted < nBandCount )
    {
/* -------------------------------------------------------------------- */
/*      Set up channel list requested.                                  */
/* -------------------------------------------------------------------- */
        std::vector<int> component_indices;
        unsigned int i;

        if( nBandCount - nBandsCompleted >= 3 )
        {
            CPLDebug( "JPIPKAK", "process bands %d,%d,%d", 
                      panBandMap[nBandsCompleted],
                      panBandMap[nBandsCompleted+1],
                      panBandMap[nBandsCompleted+2] );

            component_indices.push_back( panBandMap[nBandsCompleted++]-1 );
            component_indices.push_back( panBandMap[nBandsCompleted++]-1 );
            component_indices.push_back( panBandMap[nBandsCompleted++]-1 );
        }
        else
        {
            CPLDebug( "JPIPKAK", "process band %d", 
                      panBandMap[nBandsCompleted] );
            component_indices.push_back( panBandMap[nBandsCompleted++]-1 );
        }

/* -------------------------------------------------------------------- */
/*      Apply region, channel and overview level restrictions.          */
/* -------------------------------------------------------------------- */
        kdu_dims region_pass = region;

        poJDS->poCodestream->apply_input_restrictions(
            component_indices.size(), &(component_indices[0]), 
            nLevel, nQualityLayers, &region_pass, 
            KDU_WANT_CODESTREAM_COMPONENTS);

        channels.configure(*(poJDS->poCodestream));

        for( i=0; i < component_indices.size(); i++ )
            channels.source_components[i] = component_indices[i];

        kdu_dims incomplete_region = region_pass;
        kdu_coords origin = region_pass.pos;
        
        int bIsDecompressing = FALSE;
		
        CPLAcquireMutex(poJDS->pGlobalMutex, 100.0);

        try
        {

            bIsDecompressing = poJDS->poDecompressor->start(
                *(poJDS->poCodestream), 
                &channels, -1, nLevel, nQualityLayers, 
                region_pass, exp_numerator, exp_denominator, TRUE);	
        
            *pnxbufoff = 0;
            *pnybufoff = 0;
            *pnxbufsize = region_pass.access_size()->x;
            *pnybufsize = region_pass.access_size()->y;
        
            // Setup channel buffers
            std::vector<kdu_byte*> channel_bufs;

            for( i=0; i < component_indices.size(); i++ )
                channel_bufs.push_back( 
                    ((kdu_byte *) pBuf) 
                    + (i+nBandsCompleted-component_indices.size()) * nBandSpace );

            int pixel_gap = nPixelSpace / nBytesPerPixel;
            int row_gap = nLineSpace / nBytesPerPixel;

            while ((bIsDecompressing == 1) || (incomplete_region.area() != 0))
            {
                if( nBytesPerPixel == 1 )
                {
                    bIsDecompressing = poJDS->poDecompressor->
                        process(&(channel_bufs[0]), false, 
                                pixel_gap, origin, row_gap, 1000000, 0, 
                                incomplete_region, region_pass,
                                0, false );
                }
                else if( nBytesPerPixel == 2 )
                {
                    bIsDecompressing = poJDS->poDecompressor->
                        process((kdu_uint16**) &(channel_bufs[0]), false,
                                pixel_gap, origin, row_gap, 1000000, 0, 
                                incomplete_region, region_pass,
                                nPrecision, false );
                }
            
                CPLDebug( "JPIPKAK",
                          "processed=%d,%d %dx%d   - incomplete=%d,%d %dx%d",
                          region_pass.pos.x, region_pass.pos.y, 
                          region_pass.size.x, region_pass.size.y,
                          incomplete_region.pos.x, incomplete_region.pos.y,
                          incomplete_region.size.x, incomplete_region.size.y );
            }

            poJDS->poDecompressor->finish();
            CPLReleaseMutex(poJDS->pGlobalMutex);

        }
        catch(...)
        {
            poJDS->poDecompressor->finish();
            CPLReleaseMutex(poJDS->pGlobalMutex);
            // The error handler should already have posted an error message.
            return GARIO_ERROR;
        }
    } // nBandsCompleted < nBandCount


/* -------------------------------------------------------------------- */
/*      If the application buffer is of a different type than our       */
/*      band we need to copy into the application buffer at this        */
/*      point.                                                          */
/*                                                                      */
/*      We could optimize to only update affected area, but that is     */
/*      always the whole area for the JPIP driver it seems.             */
/* -------------------------------------------------------------------- */
    if( pAppBuf != pBuf )
    {
        int iY, iBand;
        GByte *pabySrc = (GByte *) pBuf;
        GByte *pabyDst = (GByte *) pAppBuf;
        
        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            for( iY = 0; iY < nBufYSize; iY++ )
            {
                GDALCopyWords( pabySrc + nLineSpace * iY + nBandSpace * iBand,
                               poJDS->eDT, nPixelSpace,
                               pabyDst + nAppLineSpace*iY + nAppBandSpace*iBand,
                               eBufType, nAppPixelSpace, 
                               nBufXSize );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      has there been any more data read while were have been processing?*/
/* -------------------------------------------------------------------- */
    long size = 0;
    if (bHighPriority)
    {
        const long x = poJDS->nHighThreadByteCount - nDataRead;
        size = x;
    }
    else
    {
        const long x = poJDS->nLowThreadByteCount - nDataRead;
        size = x;
    }
    
    if ((bComplete) && (nSize == size))
        result = GARIO_COMPLETE;
    else
        result = GARIO_UPDATE;
    
    nDataRead += nSize;

    if( result == GARIO_ERROR )
        poJDS->bNeedReinitialize = TRUE;
    
    return result;	
}

/************************************************************************/
/*                          JPIPDataSegment()                           */
/************************************************************************/
JPIPDataSegment::JPIPDataSegment()
{
    nId = 0;
    nAux = 0;
    nClassId = 0;
    nCodestream = 0;
    nOffset = 0;
    nLen = 0;
    pabyData = NULL;
    bIsFinal = FALSE;
    bIsEOR = FALSE;
}

/************************************************************************/
/*                          ~JPIPDataSegment()                          */
/************************************************************************/
JPIPDataSegment::~JPIPDataSegment()
{
    CPLFree(pabyData);
}

/************************************************************************/
/*                           JPIPWorkerFunc()                           */
/************************************************************************/
static void JPIPWorkerFunc(void *req)
{
    int nCurrentTransmissionLength = 2000;
    int nMinimumTransmissionLength = 2000;

    JPIPRequest *pRequest = (JPIPRequest *)req;
    JPIPKAKDataset *poJDS = 
        (JPIPKAKDataset*)(pRequest->poARIO->GetGDALDataset());
	
    int bPriority = pRequest->bPriority;

    CPLAcquireMutex(poJDS->pGlobalMutex, 100.0);

    CPLDebug( "JPIPKAK", "working thread starting." );
    // set the running status
    if (bPriority)
    {
        poJDS->bHighThreadRunning = 1;
        poJDS->bHighThreadFinished = 0;
    }
    else
    {
        poJDS->bLowThreadRunning = 1;
        poJDS->bLowThreadFinished = 0;
    }

    CPLReleaseMutex(poJDS->pGlobalMutex);

    CPLString osHeaders = "HEADERS=";
    osHeaders += "Accept: jpp-stream";
    CPLString osPersistent;

    osPersistent.Printf( "PERSISTENT=JPIPKAK:%p", poJDS );

    char *apszOptions[] = { 
        (char *) osHeaders.c_str(),
        (char *) osPersistent.c_str(),
        NULL 
    };

    while (TRUE)
    {
        // modulate the len= parameter to use the currently available bandwidth
        long nStart = clock();

        if (((bPriority) && (!poJDS->bHighThreadRunning)) || ((!bPriority) && (!poJDS->bLowThreadRunning)))
            break;

        // make jpip requests
        // adjust transmission length
        CPLString osCurrentRequest;
        osCurrentRequest.Printf("%s&len=%i", pRequest->osRequest.c_str(), nCurrentTransmissionLength);
        CPLHTTPResult *psResult = CPLHTTPFetch(osCurrentRequest, apszOptions);
        if (psResult->nDataLen == 0)
        {
            CPLAcquireMutex(poJDS->pGlobalMutex, 100.0);
            if( psResult->pszErrBuf )
                pRequest->poARIO->
                    osErrorMsg.Printf( "zero data returned from server, timeout?\n%s", psResult->pszErrBuf );
            else
                pRequest->poARIO->
                    osErrorMsg =  "zero data returned from server, timeout?";

            // status is not being set, always zero in cpl_http
            CPLDebug("JPIPWorkerFunc", "zero data returned from server");
            CPLReleaseMutex(poJDS->pGlobalMutex);
            break;
        }

        if( psResult->pszContentType != NULL )
            CPLDebug( "JPIPKAK", "Content-type: %s", psResult->pszContentType );

        if( psResult->pszContentType != NULL 
            && strstr(psResult->pszContentType,"html") != NULL )
        {
            CPLDebug( "JPIPKAK", "%s", psResult->pabyData );
        }

        int bytes = psResult->nDataLen;
        long nEnd = clock();
		
        if ((nEnd - nStart) > 0)
            nCurrentTransmissionLength = (int) MAX(bytes / ((1.0 * (nEnd - nStart)) / CLOCKS_PER_SEC), nMinimumTransmissionLength);
		

        CPLAcquireMutex(poJDS->pGlobalMutex, 100.0);

        int bError;
        int bComplete = ((JPIPKAKDataset*)pRequest->poARIO->GetGDALDataset())->ReadFromInput(psResult->pabyData, psResult->nDataLen, bError);
        if (bPriority)
            poJDS->nHighThreadByteCount += psResult->nDataLen;
        else
            poJDS->nLowThreadByteCount += psResult->nDataLen;

        pRequest->poARIO->SetComplete(bComplete);
		
        CPLReleaseMutex(poJDS->pGlobalMutex);
        CPLHTTPDestroyResult(psResult);

        if (bComplete || bError )
            break;
    }

    CPLAcquireMutex(poJDS->pGlobalMutex, 100.0);

    CPLDebug( "JPIPKAK", "Worker shutting down." );

    if (bPriority)
    {
        poJDS->bHighThreadRunning = 0;
        poJDS->bHighThreadFinished = 1;
    }
    else
    {
        poJDS->bLowThreadRunning = 0;
        poJDS->bLowThreadFinished = 1;
    }

    CPLReleaseMutex(poJDS->pGlobalMutex);

    // end of thread
    delete pRequest;
}

