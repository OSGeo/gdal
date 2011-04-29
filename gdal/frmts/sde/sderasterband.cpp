/******************************************************************************
 * $Id: sdedataset.cpp 10804 2007-02-08 23:24:59Z hobu $
 *
 * Project:  ESRI ArcSDE Raster reader
 * Purpose:  Rasterband implementaion for ESRI ArcSDE Rasters
 * Author:   Howard Butler, hobu@hobu.net
 *
 * This work was sponsored by the Geological Survey of Canada, Natural
 * Resources Canada. http://gsc.nrcan.gc.ca/
 *
 ******************************************************************************
 * Copyright (c) 2007, Howard Butler <hobu@hobu.net>
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


#include "sderasterband.h"



/************************************************************************/
/*  SDERasterBand implements a GDAL RasterBand for ArcSDE.  This class  */
/*  carries around a pointer to SDE's internal band representation      */
/*  is of type SE_RASBANDINFO*.  SDERasterBand provides the following   */
/*  capabilities:                                                       */
/*                                                                      */
/*      -- Statistics support - uses SDE's internal band statistics     */
/*      -- Colortable - translates SDE's internal colortable to GDAL's  */
/*      -- Block reading through IReadBlock                             */
/*      -- Overview support                                             */
/*      -- NODATA support                                               */
/*                                                                      */
/*  Instantiating a SDERasterBand is rather expensive because of all    */
/*  of the round trips to the database the SDE C API must make to       */
/*  calculate band information.  This overhead hit is also taken in     */
/*  the case of grabbing an overview, because information between       */
/*  bands is not shared.  It might be possible in the future to do      */
/*  do so, but it would likely make things rather complicated.          */
/*  In particular, the stream, constraint, and queryinfo SDE objects    */
/*  could be passed around from band to overview band without having    */
/*  to be instantiated every time.  Stream creation has an especially   */
/*  large overhead.                                                     */
/*                                                                      */
/*  Once the band or overview band is established, querying raster      */
/*  blocks does not carry much more network overhead than that requied  */
/*  to actually download the bytes.                                     */
/*                                                                      */
/*  Overview of internal methods:                                       */
/*      -- InitializeBand - does most of the work of construction       */
/*                          of the band and communication with SDE.     */
/*                          Calls InitializeConstraint and              */
/*                          IntializeQuery.                             */
/*      -- InitializeQuery -    Initializes a SDE queryinfo object      */
/*                              that contains information about which   */
/*                              tables we are querying from.            */
/*      -- InitializeConstraint -   Specifies block constraints (which  */
/*                                  are initially set to none in        */
/*                                  InitializeBand) as well as which    */
/*                                  band for SDE to query from.         */
/*      -- MorphESRIRasterType -    translates SDE's raster type to GDAL*/
/*      -- MorphESRIRasterDepth -   calculates the bit depth from SDE   */
/*      -- ComputeColorTable -  does the work of getting and            */
/*                              translating the SDE colortable to GDAL. */
/*      -- ComputeSDEBandNumber -   returns the band # for SDE's        */
/*                                  internal representation of the band.*/
/*      -- QueryRaster -    Does the work of setting the constraint     */
/*                          and preparing for querying tiles from SDE.  */
/*                                                                      */
/************************************************************************/


/************************************************************************/
/*                           SDERasterBand()                            */
/************************************************************************/

SDERasterBand::SDERasterBand(   SDEDataset *poDS, 
                                int nBand, 
                                int nOverview, 
                                const SE_RASBANDINFO* band )

{
    // Carry some of the data we were given at construction.  
    // If we were passed -1 for an overview at construction, reset it 
    // to 0 to ensure we get the zero'th level from SDE.
    // The SE_RASBANDINFO* we were given is actually owned by the 
    // dataset.  We want it around for convenience.
    this->poDS = poDS;
    this->nBand = nBand;
    this->nOverview = nOverview;
    this->poBand = band;
    
    // Initialize our SDE opaque object pointers to NULL.
    // The nOverviews private data member will be updated when 
    // GetOverviewCount is called and subsequently returned immediately in 
    // later calls if it has been set to anything other than 0.
    this->hConstraint = NULL;
    this->hQuery = NULL;
    this->poColorTable = NULL;
    
    if (this->nOverview == -1 || this->nOverview == 0)
        this->nOverviews = GetOverviewCount();
    else
        this->nOverviews = 0;

    if (nOverview == -1) {
        this->papoOverviews = (GDALRasterBand**)  CPLMalloc( nOverviews * sizeof(GDALRasterBand*) );
    }
    else {
        this->papoOverviews = NULL;
    }
    this->eDataType = GetRasterDataType();
    
    // nSDERasterType is set by GetRasterDataType
    this->dfDepth = MorphESRIRasterDepth(nSDERasterType);
    InitializeBand(this->nOverview);

    
}

/************************************************************************/
/*                          ~SDERasterBand()                            */
/************************************************************************/
SDERasterBand::~SDERasterBand( void )

{ 

    if (hQuery)
        SE_queryinfo_free(hQuery);

    if (hConstraint)
        SE_rasconstraint_free(hConstraint);

    if (papoOverviews)
        for (int i=0; i < nOverviews; i++)
            delete papoOverviews[i];
        CPLFree(papoOverviews);
    
    if (poColorTable != NULL)
        delete poColorTable;
}


/************************************************************************/
/*                             GetColorTable()                          */
/************************************************************************/
GDALColorTable* SDERasterBand::GetColorTable(void) 
{
    
    if (SE_rasbandinfo_has_colormap(*poBand)) {
        if (poColorTable == NULL)
            ComputeColorTable();
        return poColorTable;
    } else {
        return NULL;
    }
}


/************************************************************************/
/*                             GetColorInterpretation()                 */
/************************************************************************/
GDALColorInterp SDERasterBand::GetColorInterpretation()
{
    // Only return Paletted images when SDE has a colormap.  Otherwise,
    // just return gray, even in the instance where we have 3 or 4 band, 
    // imagery.  Let the client be smart instead of trying to do too much.
    if (SE_rasbandinfo_has_colormap(*poBand)) 
        return GCI_PaletteIndex;
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/
GDALRasterBand* SDERasterBand::GetOverview( int nOverviewValue )
{

    if (papoOverviews) {
        return papoOverviews[nOverviewValue];
    }
    else
        return NULL;

    
}

/************************************************************************/
/*                           GetOverviewCount()                         */
/************************************************************************/
int SDERasterBand::GetOverviewCount( void )
{
    // grab our existing overview count if we have already gotten it,
    // otherwise request it from SDE and set our member data with it.
    long nSDEErr;
    BOOL bSkipLevel;
    LONG nOvRet;
    
    // return nothing if we were an overview band
    if (nOverview != -1)
        return 0;

    nSDEErr = SE_rasbandinfo_get_max_level(*poBand, &nOvRet, &bSkipLevel);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_band_size" );
    }
    
    nOverviews = nOvRet;

    return nOverviews;
} 

/************************************************************************/
/*                             GetRasterDataType()                      */
/************************************************************************/
GDALDataType SDERasterBand::GetRasterDataType(void) 
{
    // Always ask SDE what it thinks our type is.
    LONG nSDEErr;
    
    nSDEErr = SE_rasbandinfo_get_pixel_type(*poBand, &nSDERasterType);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_pixel_type" );
        return GDT_Byte;
    }

    return MorphESRIRasterType(nSDERasterType);
}

/************************************************************************/
/*                             GetStatistics()                          */
/************************************************************************/

CPLErr SDERasterBand::GetStatistics( int bApproxOK, int bForce,
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev ) 
{
    // if SDE hasn't already cached our statistics, we'll depend on the 
    // GDALRasterBands's method for getting them.
    bool bHasStats;
    bHasStats = SE_rasbandinfo_has_stats (*poBand);
    if (!bHasStats) 
        return GDALRasterBand::GetStatistics(    bApproxOK,
                                                    bForce,
                                                    pdfMin,
                                                    pdfMax,
                                                    pdfMean,
                                                    pdfStdDev);

    // bForce has no effect currently.  We always go to SDE to get our 
    // stats if SDE has them.
    
    // bApproxOK has no effect currently.  If we're getting stats from 
    // SDE, we're hoping SDE calculates them in the way we want.
    long nSDEErr;
    nSDEErr = SE_rasbandinfo_get_stats_min(*poBand, pdfMin);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_stats_min" );
        return CE_Fatal;
    }  

    nSDEErr = SE_rasbandinfo_get_stats_max(*poBand, pdfMax);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_stats_max" );
        return CE_Fatal;
    } 

    nSDEErr = SE_rasbandinfo_get_stats_mean(*poBand, pdfMean);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_stats_mean" );
        return CE_Fatal;
    }

    nSDEErr = SE_rasbandinfo_get_stats_stddev(*poBand, pdfStdDev);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_stats_stddev" );
        return CE_Fatal;
    } 
    return CE_None;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double SDERasterBand::GetMinimum(int *pbSuccess) 
{
    double dfMin, dfMax, dfMean, dfStdDev;
    CPLErr error = GetStatistics( TRUE, TRUE, 
                                  &dfMin,
                                  &dfMax, 
                                  &dfMean, 
                                  &dfStdDev );
    if (error == CE_None) {
        *pbSuccess = TRUE;
        return dfMin;
    }
    *pbSuccess = FALSE;
    return 0.0;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double SDERasterBand::GetMaximum(int *pbSuccess) 
{
    double dfMin, dfMax, dfMean, dfStdDev;
    CPLErr error = GetStatistics( TRUE, TRUE, 
                                  &dfMin,
                                  &dfMax, 
                                  &dfMean, 
                                  &dfStdDev );
    if (error == CE_None) {
        *pbSuccess = TRUE;
        return dfMax;
    }
    *pbSuccess = FALSE;
    return 0.0;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SDERasterBand::IReadBlock( int nBlockXOff, 
                                  int nBlockYOff,
                                  void * pImage )

{
    // grab our Dataset to limit the casting we have to do.
    SDEDataset *poGDS = (SDEDataset *) poDS;


    // SDE manages the acquisition of raster data in "TileInfo" objects.  
    // The hTile is the only heap-allocated object in this method, and 
    // we should make sure to delete it at the end.  Once we get the 
    // pixel data, we'll memcopy it back on to the pImage pointer.

    SE_RASTILEINFO hTile;
    long nSDEErr;
    nSDEErr = SE_rastileinfo_create(&hTile);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rastileinfo_create" );
        return CE_Fatal;
    }

    hConstraint = InitializeConstraint( (long*) &nBlockXOff, (long*) &nBlockYOff );  
    if (!hConstraint)
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "ConstraintInfo initialization failed");   

    CPLErr error = QueryRaster(hConstraint);
    if (error != CE_None)
        return error;

    LONG level;
    nSDEErr = SE_rastileinfo_get_level(hTile, &level);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rastileinfo_get_level" );
        return CE_Fatal;
    }   

    nSDEErr = SE_stream_get_raster_tile(poGDS->hStream, hTile);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_get_raster_tile" );
        return CE_Fatal;
    }        

    LONG row, column;
    nSDEErr = SE_rastileinfo_get_rowcol(hTile, &row, &column);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rastileinfo_get_level" );
        return CE_Fatal;
    }     

    LONG length;
    unsigned char* pixels;
    nSDEErr = SE_rastileinfo_get_pixel_data(hTile, (void**) &pixels, &length);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rastileinfo_get_pixel_data" );
        return CE_Fatal;
    }           

    int bits_per_pixel = static_cast<int>(dfDepth * 8 + 0.0001);
    int block_size = (nBlockXSize * bits_per_pixel + 7) / 8 * nBlockYSize;
    int bitmap_size = (nBlockXSize * nBlockYSize + 7) / 8;


    if (length == 0) {
        // ArcSDE says the block has no data in it.
        // Write 0's and be done with it
        memset( pImage, 0, 
                nBlockXSize*nBlockYSize*GDALGetDataTypeSize(eDataType)/8);
        return CE_None;
    }
    if ((length == block_size) || (length == (block_size + bitmap_size))) {
    if (bits_per_pixel >= 8) {
        memcpy(pImage, pixels, block_size);
    } else {
        GByte *p = reinterpret_cast<GByte*>(pImage);
        int bit_mask = (2 << bits_per_pixel) - 1;
        int i = 0;
        for (int y = 0; y < nBlockYSize; ++y) {
        for (int x = 0; x < nBlockXSize; ++x) {
            *p++ = (pixels[i >> 3] >> (i & 7)) & bit_mask;
            i += bits_per_pixel;
        }
        i = (i + 7) / 8 * 8;
        }
    }
    } else {

            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Bit size calculation failed... "\
                      "SDE's length:%d With bitmap length: %d Without bitmap length: %d", 
                      length, block_size + bitmap_size, block_size );
            return CE_Fatal;
        }

    SE_rastileinfo_free (hTile);

    return CE_None ;
}



/* ---------------------------------------------------------------------*/
/* Private Methods                                                      */

/************************************************************************/
/*                             ComputeColorTable()                      */
/************************************************************************/
void SDERasterBand::ComputeColorTable(void) 
{

    SE_COLORMAP_TYPE eCMap_Type;
    SE_COLORMAP_DATA_TYPE eCMap_DataType;
    
    LONG nCMapEntries;
    void * phSDEColormapData;
    
    unsigned char* puszSDECMapData;
    unsigned short* pushSDECMapData;
    
    long nSDEErr;
    
    nSDEErr = SE_rasbandinfo_get_colormap(  *poBand,
                                            &eCMap_Type,
                                            &eCMap_DataType,
                                            &nCMapEntries,
                                            &phSDEColormapData);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_colormap" );
    }                                            

    // Assign both the short and char pointers 
    // to the void*, and we'll switch and read based 
    // on the eCMap_DataType
    puszSDECMapData = (unsigned char*) phSDEColormapData;
    pushSDECMapData = (unsigned short*) phSDEColormapData;
    
    poColorTable = new GDALColorTable(GPI_RGB);
    
    int red, green, blue, alpha;
    
    CPLDebug("SDERASTER", "%d colormap entries specified", nCMapEntries);
    switch (eCMap_DataType) {
        case SE_COLORMAP_DATA_BYTE:
            switch (eCMap_Type){
                case SE_COLORMAP_RGB:
                    for (int i = 0; i < (nCMapEntries); i++) {
                        int j = i*3;
                        red = puszSDECMapData[j];
                        green = puszSDECMapData[j+1];
                        blue = puszSDECMapData[j+2];
                        GDALColorEntry sColor;
                        sColor.c1 = red;
                        sColor.c2 = green;
                        sColor.c3 = blue;
                        sColor.c4 = 255;
                        
                        // sColor is copied
                        poColorTable->SetColorEntry(i,&sColor);
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_BYTE "\
                                  "SE_COLORMAP_RGB Colormap Entry: %d %d %d", 
                                  red, blue, green);
                    }
                    break;
                case SE_COLORMAP_RGBA:
                    for (int i = 0; i < (nCMapEntries); i++) {
                        int j = i*4;
                        red = puszSDECMapData[j];
                        green = puszSDECMapData[j+1];
                        blue = puszSDECMapData[j+2];
                        alpha = puszSDECMapData[j+3];
                        GDALColorEntry sColor;
                        sColor.c1 = red;
                        sColor.c2 = green;
                        sColor.c3 = blue;
                        sColor.c4 = alpha;
                        
                        // sColor is copied
                        poColorTable->SetColorEntry(i,&sColor);
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_BYTE "\
                                  "SE_COLORMAP_RGBA Colormap Entry: %d %d %d %d", 
                                  red, blue, green, alpha);
                    }
                    break;  
                case SE_COLORMAP_NONE:
                    break;                 
            }
            break;
        case SE_COLORMAP_DATA_SHORT:
            switch (eCMap_Type) {
                case SE_COLORMAP_RGB:
                    for (int i = 0; i < (nCMapEntries); i++) {
                        int j = i*3;
                        red = pushSDECMapData[j];
                        green = pushSDECMapData[j+1];
                        blue = pushSDECMapData[j+2];
                        GDALColorEntry sColor;
                        sColor.c1 = red;
                        sColor.c2 = green;
                        sColor.c3 = blue;
                        sColor.c4 = 255;
                        
                        // sColor is copied
                        poColorTable->SetColorEntry(i,&sColor);
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_SHORT "\
                                  "SE_COLORMAP_RGB Colormap Entry: %d %d %d", 
                                  red, blue, green);
                    }
                    break;
                case SE_COLORMAP_RGBA:
                    for (int i = 0; i < (nCMapEntries); i++) {
                        int j = i*4;
                        red = pushSDECMapData[j];
                        green = pushSDECMapData[j+1];
                        blue = pushSDECMapData[j+2];
                        alpha = pushSDECMapData[j+3];
                        GDALColorEntry sColor;
                        sColor.c1 = red;
                        sColor.c2 = green;
                        sColor.c3 = blue;
                        sColor.c4 = alpha;
                        
                        // sColor is copied
                        poColorTable->SetColorEntry(i,&sColor);
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_SHORT "\
                                  "SE_COLORMAP_RGBA Colormap Entry: %d %d %d %d", 
                                  red, blue, green, alpha);
                    }
                    break;
                case SE_COLORMAP_NONE:
                    break;    
            }
            break;
    }
    SE_rasbandinfo_free_colormap(phSDEColormapData);
}


/************************************************************************/
/*                           InitializeBand()                           */
/************************************************************************/
CPLErr SDERasterBand::InitializeBand( int nOverview )

{    

    SDEDataset *poGDS = (SDEDataset *) poDS;
    
    long nSDEErr;


    hConstraint = InitializeConstraint( NULL, NULL );  
    if (!hConstraint)
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "ConstraintInfo initialization failed");   

    if (!hQuery) {
        hQuery = InitializeQuery();
        if (!hQuery)
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "QueryInfo initialization failed");
    }

    nSDEErr = SE_stream_close(poGDS->hStream, 1);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_close" );
        return CE_Fatal;
    }
                      
    nSDEErr = SE_stream_query_with_info(poGDS->hStream, hQuery);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_query_with_info" );
        return CE_Fatal;
    }

    nSDEErr = SE_stream_execute (poGDS->hStream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_execute" );
        return CE_Fatal;
    }
    nSDEErr = SE_stream_fetch (poGDS->hStream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_fetch" );
        return CE_Fatal;
    }
    

    CPLErr error = QueryRaster(hConstraint);
    if (error != CE_None)
        return error;

    LONG nBXRet, nBYRet;
    nSDEErr = SE_rasterattr_get_tile_size (poGDS->hAttributes, 
                                           &nBXRet, &nBYRet);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasterattr_get_tile_size" );
        return CE_Fatal;
    }

    nBlockXSize = nBXRet;
    nBlockYSize = nBYRet;
    
    LONG offset_x, offset_y, num_bands, nXSRet, nYSRet;
    
    nSDEErr = SE_rasterattr_get_image_size_by_level (poGDS->hAttributes,
                                                     &nXSRet, &nYSRet,
                                                     &offset_x,
                                                     &offset_y,
                                                     &num_bands,
                                                     (nOverview == -1) ? (0): (nOverview));

    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasterattr_get_image_size_by_level" );
        return CE_Fatal;
    }

    nRasterXSize = nXSRet;
    nRasterYSize = nYSRet;

    nBlockSize = nBlockXSize * nBlockYSize;

    // We're the base level
    if (nOverview == -1) {
        for (int i = 0; i<this->nOverviews; i++) {
            papoOverviews[i]= new SDERasterBand(poGDS, nBand, i, poBand);

        }
    }
    return CE_None;
}

/************************************************************************/
/*                           InitializeConstraint()                     */
/************************************************************************/
SE_RASCONSTRAINT& SDERasterBand::InitializeConstraint( long* nBlockXOff, 
                                                       long* nBlockYOff) 
{

    long nSDEErr;   
    
    if (!hConstraint) {
        nSDEErr = SE_rasconstraint_create(&hConstraint);
        if( nSDEErr != SE_SUCCESS )
        {
            IssueSDEError( nSDEErr, "SE_rasconstraint_create" );
        }
        
        nSDEErr = SE_rasconstraint_set_level(hConstraint, (nOverview == -1) ? (0): (nOverview));
        if( nSDEErr != SE_SUCCESS )
        {
            IssueSDEError( nSDEErr, "SE_rasconstraint_create" );
        }       

        LONG nBandIn = nBand;
        nSDEErr = SE_rasconstraint_set_bands(hConstraint, 1, &nBandIn);
        if( nSDEErr != SE_SUCCESS )
        {
            IssueSDEError( nSDEErr, "SE_rasconstraint_set_bands" );
        }
        nSDEErr = SE_rasconstraint_set_interleave(hConstraint, SE_RASTER_INTERLEAVE_BSQ);
        if( nSDEErr != SE_SUCCESS )
        {
            IssueSDEError( nSDEErr, "SE_rasconstraint_set_interleave" );
        }

    }
    
    if (nBlockXSize != -1 && nBlockYSize != -1) { // we aren't initialized yet
        if (nBlockXSize >= 0 && nBlockYSize >= 0) { 
            if (*nBlockXOff >= 0 &&  *nBlockYOff >= 0) {
                long nMinX, nMinY, nMaxX, nMaxY;
                
                nMinX = *nBlockXOff;
                nMinY = *nBlockYOff;
                nMaxX = *nBlockXOff;
                nMaxY = *nBlockYOff;
                                                                            
                nSDEErr = SE_rasconstraint_set_envelope (hConstraint,
                                                        nMinX,
                                                        nMinY,
                                                        nMaxX,
                                                        nMaxY);
                if( nSDEErr != SE_SUCCESS )
                {
                    IssueSDEError( nSDEErr, "SE_rasconstraint_set_envelope" );
                }
            }
        }
    }
    return hConstraint;
}

/************************************************************************/
/*                           InitializeQuery()                          */
/************************************************************************/
SE_QUERYINFO& SDERasterBand::InitializeQuery( void ) 
{
    SDEDataset *poGDS = (SDEDataset *) poDS;
    long nSDEErr;

    nSDEErr = SE_queryinfo_create(&hQuery);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_create" );
    }
    
    nSDEErr = SE_queryinfo_set_tables(hQuery, 
                                      1, 
                                      (const char**) &(poGDS->pszLayerName), 
                                      NULL);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_set_tables" );
    }

    nSDEErr = SE_queryinfo_set_where_clause(hQuery, (const char*) "");
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_set_where" );
    }

    nSDEErr = SE_queryinfo_set_columns(hQuery, 
                                       1, 
                                       (const char**) &(poGDS->pszColumnName));
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_set_where" );
    }
    return hQuery;        
}



/************************************************************************/
/*                             MorphESRIRasterDepth()                   */
/************************************************************************/
double SDERasterBand::MorphESRIRasterDepth(int gtype) {
    
    switch (gtype) {
        case SE_PIXEL_TYPE_1BIT:
            return 0.125;
        case SE_PIXEL_TYPE_4BIT:
            return 0.5;
        case SE_PIXEL_TYPE_8BIT_U:
            return 1.0;
        case SE_PIXEL_TYPE_8BIT_S:
            return 1.0;
        case SE_PIXEL_TYPE_16BIT_U:
            return 2.0;
        case SE_PIXEL_TYPE_16BIT_S:
            return 2.0;
        case SE_PIXEL_TYPE_32BIT_U:
            return 4.0;
        case SE_PIXEL_TYPE_32BIT_S:
            return 4.0;
        case SE_PIXEL_TYPE_32BIT_REAL:
            return 4.0;
        case SE_PIXEL_TYPE_64BIT_REAL:
            return 8.0;
        default:
            return 2.0;
        }
}

/************************************************************************/
/*                             MorphESRIRasterType()                    */
/************************************************************************/
GDALDataType SDERasterBand::MorphESRIRasterType(int gtype) {
    
    switch (gtype) {
        case SE_PIXEL_TYPE_1BIT:
            return GDT_Byte;
        case SE_PIXEL_TYPE_4BIT:
            return GDT_Byte;
        case SE_PIXEL_TYPE_8BIT_U:
            return GDT_Byte;
        case SE_PIXEL_TYPE_8BIT_S:
            return GDT_Byte;
        case SE_PIXEL_TYPE_16BIT_U:
            return GDT_UInt16;
        case SE_PIXEL_TYPE_16BIT_S:
            return GDT_Int16;
        case SE_PIXEL_TYPE_32BIT_U:
            return GDT_UInt32;
        case SE_PIXEL_TYPE_32BIT_S:
            return GDT_Int32;
        case SE_PIXEL_TYPE_32BIT_REAL:
            return GDT_Float32;
        case SE_PIXEL_TYPE_64BIT_REAL:
            return GDT_Float64;
        default:
            return GDT_UInt16;
        }
}

/************************************************************************/
/*                           QueryRaster()                              */
/************************************************************************/
CPLErr SDERasterBand::QueryRaster( SE_RASCONSTRAINT& constraint ) 
{

    SDEDataset *poGDS = (SDEDataset *) poDS;
    
    long nSDEErr;


                          
    nSDEErr = SE_stream_query_raster_tile(poGDS->hStream, constraint);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_query_raster_tile" );
        return CE_Fatal;
    }
    
    nSDEErr = SE_stream_get_raster (poGDS->hStream, 1, poGDS->hAttributes);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_fetch" );
        return CE_Fatal;
    }

    return CE_None;
}

//T:\>gdal_translate -of GTiff SDE:nakina.gis.iastate.edu,5151,,geoservwrite,EsrI4ever,sde_master.geoservwrite.century foo.tif  
//T:\>gdalinfo SDE:nakina.gis.iastate.edu,5151,,geoservwrite,EsrI4ever,sde_master.geoservwrite.century  
