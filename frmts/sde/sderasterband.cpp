#include "sderasterband.h"

/************************************************************************/
/*                           SDERasterBand()                            */
/************************************************************************/

SDERasterBand::SDERasterBand(   SDEDataset *poDS, 
                                int nBand, 
                                int nOverview, 
                                const SE_RASBANDINFO* band )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    poBand = band;
    eDataType = GetRasterDataType();
    
    nOverviews = 0;
	if (nOverview == -1) nOverview = 0; else nOverview = nOverview;

	hConstraint = NULL;
    hQuery = NULL;
    hStream = NULL;
    InitializeBand(nOverview);
    nBand = ComputeSDEBandNumber(band);
    
}

/************************************************************************/
/*                          ~SDERasterBand()                            */
/************************************************************************/
SDERasterBand::~SDERasterBand( void )

{
    SE_queryinfo_free(hQuery);
    SE_stream_free(hStream);
    SE_rasconstraint_free(hConstraint);
}


/************************************************************************/
/*                             GetColorTable()                          */
/************************************************************************/
GDALColorTable* SDERasterBand::GetColorTable(void) 
{
    

    if (SE_rasbandinfo_has_colormap(*poBand)) {
        GDALColorTable* poCT = ComputeColorTable();
        return poCT;
    } else {
        return NULL;
    }
}


/************************************************************************/
/*                             GetColorInterpretation()                 */
/************************************************************************/
GDALColorInterp SDERasterBand::GetColorInterpretation()
{
    if (SE_rasbandinfo_has_colormap(*poBand)) 
        return GCI_PaletteIndex;
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/
GDALRasterBand* SDERasterBand::GetOverview( int nOverview )
{
	SDEDataset *poGDS = (SDEDataset *) poDS;
    return new SDERasterBand(poGDS, nBand, nOverview, poBand);
    
} 
/************************************************************************/
/*                           GetOverviewCount()                         */
/************************************************************************/
int SDERasterBand::GetOverviewCount( void )
{
    long nSDEErr;
    BOOL bSkipLevel;
    
    if (nOverviews)
        return nOverviews;
    nSDEErr = SE_rasbandinfo_get_max_level(*poBand, 
                                           (long*)&nOverviews, 
                                           &bSkipLevel);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_band_size" );

    }
    
    CPLDebug("SDERASTER", "We have %d overviews", nOverviews);
    return nOverviews;
    
} 

/************************************************************************/
/*                             GetRasterDataType()                      */
/************************************************************************/
GDALDataType SDERasterBand::GetRasterDataType(void) 
{
    long nSDEErr;
    long nSDERasterType;
    
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

    bool bHasStats;
    bHasStats = SE_rasbandinfo_has_stats (*poBand);
    if (!bHasStats) 
        return GDALRasterBand::GetStatistics(    bApproxOK,
                                                    bForce,
                                                    pdfMin,
                                                    pdfMax,
                                                    pdfMean,
                                                    pdfStdDev);
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
    if (error) {
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
    if (error) {
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
    SDEDataset *poGDS = (SDEDataset *) poDS;

    SE_RASTILEINFO hTile;
    long nSDEErr;
    nSDEErr = SE_rastileinfo_create(&hTile);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rastileinfo_create" );
        return CE_Fatal;
    }

    hConstraint = InitializeConstraint( nBlockXOff, nBlockYOff );  
    if (!hConstraint)
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "ConstraintInfo initialization failed");   

    CPLErr error = QueryRaster(hConstraint);
    if (error != CE_None)
        return error;

    long level;
    nSDEErr = SE_rastileinfo_get_level(hTile, &level);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rastileinfo_get_level" );
        return CE_Fatal;
    }   
    
    CPLDebug ("SDERASTER", "nBlockXSize: %d nBlockYSize: %d "\
                           "nBlockXOff: %d nBlockYOff: %d",
                            nBlockXSize, nBlockYSize, 
                            nBlockXOff, nBlockYOff);
                            
    nSDEErr = SE_stream_get_raster_tile(hStream, hTile);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_get_raster_tile" );
        return CE_Fatal;
    }        

    long row, column;
    nSDEErr = SE_rastileinfo_get_rowcol(hTile, &row, &column);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rastileinfo_get_level" );
        return CE_Fatal;
    }     
    
    CPLDebug("SDERASTER", "row: %d column: %d", row, column); 

    long length;
    unsigned char* pixels;
    nSDEErr = SE_rastileinfo_get_pixel_data(hTile, (void**) &pixels, &length);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rastileinfo_get_pixel_data" );
        return CE_Fatal;
    }           
    CPLDebug("SDERASTER", "pixel data length: %d", length);
    
    memcpy( pImage, pixels, 
        nBlockSize * (GDALGetDataTypeSize(poGDS->eDataType) / 8) );
    nSDEErr = SE_FINISHED;

    SE_rastileinfo_free (hTile);

    return CE_None ;
}



/* ---------------------------------------------------------------------*/
/* Private Methods 

/************************************************************************/
/*                             ComputeColorTable()                      */
/************************************************************************/
GDALColorTable* SDERasterBand::ComputeColorTable(void) 
{

    SE_COLORMAP_TYPE eCMap_Type;
    SE_COLORMAP_DATA_TYPE eCMap_DataType;
    
    long nCMapEntries;
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
        return NULL;
    }                                            

    // Assign both the short and char pointers 
    // to the void*, and we'll switch and read based 
    // on the eCMap_DataType
    puszSDECMapData = (unsigned char*) phSDEColormapData;
    pushSDECMapData = (unsigned short*) phSDEColormapData;
    
    GDALColorTable* poCT = new GDALColorTable(GPI_RGB);
    
    int nEntries;
    int red, green, blue, alpha;
    
    CPLDebug("SDERASTER", "%d colormap entries specified", nCMapEntries);
    switch (eCMap_DataType) {
        case SE_COLORMAP_DATA_BYTE:
            switch (eCMap_Type){
                case SE_COLORMAP_RGB:
                    for (int i = 0; i < (nCMapEntries); i++) {
                        int j = i*3;
                        red = puszSDECMapData[j];
                        blue = puszSDECMapData[j+1];
                        green = puszSDECMapData[j+2];
                        GDALColorEntry sColor;
                        sColor.c1 = red;
                        sColor.c2 = green;
                        sColor.c3 = blue;
                        sColor.c4 = 255;
                        
                        // sColor is copied
                        poCT->SetColorEntry(i,&sColor);
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_BYTE "\
                                  "SE_COLORMAP_RGB Colormap Entry: %d %d %d", 
                                  red, blue, green);
                    }
                    break;
                case SE_COLORMAP_RGBA:
                    for (int i = 0; i < (nCMapEntries); i++) {
                        int j = i*4;
                        red = puszSDECMapData[j];
                        blue = puszSDECMapData[j+1];
                        green = puszSDECMapData[j+2];
                        alpha = puszSDECMapData[j+3];
                        GDALColorEntry sColor;
                        sColor.c1 = red;
                        sColor.c2 = green;
                        sColor.c3 = blue;
                        sColor.c4 = alpha;
                        
                        // sColor is copied
                        poCT->SetColorEntry(i,&sColor);
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_BYTE "\
                                  "SE_COLORMAP_RGBA Colormap Entry: %d %d %d %d", 
                                  red, blue, green, alpha);
                    }
                    break;               
            }
            break;
        case SE_COLORMAP_DATA_SHORT:
            switch (eCMap_Type) {
                case SE_COLORMAP_RGB:
                    for (int i = 0; i < (nCMapEntries); i++) {
                        int j = i*3;
                        red = pushSDECMapData[j];
                        blue = pushSDECMapData[j+1];
                        green = pushSDECMapData[j+2];
                        GDALColorEntry sColor;
                        sColor.c1 = red;
                        sColor.c2 = green;
                        sColor.c3 = blue;
                        sColor.c4 = 255;
                        
                        // sColor is copied
                        poCT->SetColorEntry(i,&sColor);
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_SHORT "\
                                  "SE_COLORMAP_RGB Colormap Entry: %d %d %d", 
                                  red, blue, green);
                    }
                    break;
                case SE_COLORMAP_RGBA:
                    for (int i = 0; i < (nCMapEntries); i++) {
                        int j = i*4;
                        red = pushSDECMapData[j];
                        blue = pushSDECMapData[j+1];
                        green = pushSDECMapData[j+2];
                        alpha = pushSDECMapData[j+3];
                        GDALColorEntry sColor;
                        sColor.c1 = red;
                        sColor.c2 = green;
                        sColor.c3 = blue;
                        sColor.c4 = alpha;
                        
                        // sColor is copied
                        poCT->SetColorEntry(i,&sColor);
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_SHORT "\
                                  "SE_COLORMAP_RGBA Colormap Entry: %d %d %d %d", 
                                  red, blue, green, alpha);
                    }
                    break;
            }
            break;
    }
    GDALColorEntry sColor;
    return poCT;
}


/************************************************************************/
/*                           ComputeSDEBandNumber()                     */
/************************************************************************/
int SDERasterBand::ComputeSDEBandNumber( const SE_RASBANDINFO* band ) 
{
    long nSDEErr;
    long pnBandNumber;
    nSDEErr = SE_rasbandinfo_get_band_number (*band, &pnBandNumber);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_band_number" );
        return 0;
    }
    return pnBandNumber;
}

/************************************************************************/
/*                           InitializeBand()                           */
/************************************************************************/
CPLErr SDERasterBand::InitializeBand( int nOverview )

{    

    SDEDataset *poGDS = (SDEDataset *) poDS;
    
    long nSDEErr;

    nSDEErr = SE_stream_create(poGDS->hConnection, &hStream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_create" );
        return CE_Fatal;
    }

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
                  
    nSDEErr = SE_stream_query_with_info(hStream, hQuery);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_query_with_info" );
        return CE_Fatal;
    }

    nSDEErr = SE_stream_execute (hStream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_execute" );
        return CE_Fatal;
    }
    nSDEErr = SE_stream_fetch (hStream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_fetch" );
        return CE_Fatal;
    }
    

    CPLErr error = QueryRaster(hConstraint);
    if (error != CE_None)
        return error;


    nSDEErr = SE_rasterattr_get_tile_size (poGDS->hAttributes, 
                                           (long*)&nBlockXSize, 
                                           (long*)&nBlockYSize);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasterattr_get_tile_size" );
        return CE_Fatal;
    }
    
	long offset_x, offset_y, num_bands;

	nSDEErr = SE_rasterattr_get_image_size_by_level (poGDS->hAttributes,
													 (long*)&nRasterXSize,
													 (long*)&nRasterYSize,
													 &offset_x,
													 &offset_y,
													 &num_bands,
													 nOverview);

    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasterattr_get_image_size_by_level" );
        return CE_Fatal;
    }
													 
    CPLDebug("SDERASTER", "Tile Sizes: %d %d", nBlockXSize, nBlockYSize);
    nBlockSize = nBlockXSize * nBlockYSize;

    return CE_None;
}

/************************************************************************/
/*                           InitializeConstraint()                     */
/************************************************************************/
SE_RASCONSTRAINT& SDERasterBand::InitializeConstraint( long nBlockXOff, 
                                                       long nBlockYOff) 
{
    SDEDataset *poGDS = (SDEDataset *) poDS;

    long nSDEErr;   
    
    if (!hConstraint) {
        nSDEErr = SE_rasconstraint_create(&hConstraint);
        if( nSDEErr != SE_SUCCESS )
        {
            IssueSDEError( nSDEErr, "SE_rasconstraint_create" );
        }
		
		nSDEErr = SE_rasconstraint_set_level(hConstraint, nOverview);
		if( nSDEErr != SE_SUCCESS )
		{
			IssueSDEError( nSDEErr, "SE_rasconstraint_create" );
		}		

        nSDEErr = SE_rasconstraint_set_bands(hConstraint, 1, (long*)&nBand);
        if( nSDEErr != SE_SUCCESS )
        {
            IssueSDEError( nSDEErr, "SE_rasconstraint_set_bands" );
        }
    }
    
    if (nBlockXSize != -1 && nBlockYSize != -1) { // we aren't initialized yet
        if (nBlockXSize >= 0 && nBlockYSize >= 0) { 
            if (nBlockXOff >= 0 &&  nBlockYOff >= 0) {
                long nMinX, nMinY, nMaxX, nMaxY;
                
                nMinX = nBlockXOff;
                nMinY = nBlockYOff;
                nMaxX = nBlockXOff;
                nMaxY = nBlockYOff;
                                                                            
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


                          
    nSDEErr = SE_stream_query_raster_tile(hStream, constraint);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_query_raster_tile" );
        return CE_Fatal;
    }
    
    nSDEErr = SE_stream_get_raster (hStream, 1, poGDS->hAttributes);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_fetch" );
        return CE_Fatal;
    }

    return CE_None;
}

//T:\>gdal_translate -of GTiff SDE:nakina.gis.iastate.edu,5151,,geoservwrite,EsrI4ever,sde_master.geoservwrite.century foo.tif  
//T:\>gdalinfo SDE:nakina.gis.iastate.edu,5151,,geoservwrite,EsrI4ever,sde_master.geoservwrite.century  
