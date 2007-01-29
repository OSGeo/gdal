#include "sderasterband.h"

/************************************************************************/
/*                           SDERasterBand()                            */
/************************************************************************/

SDERasterBand::SDERasterBand( SDEDataset *poDS, int nBand, 
                              const SE_RASBANDINFO* band )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    poBand = band;
    eDataType = GetRasterDataType();

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
    
    nOverviews = 0;
    

    
}

int SDERasterBand::GetOverviewCount(void)
{
    long nSDEErr;
    BOOL bSkipLevel;
    
    if (nOverviews)
        return nOverviews;
    nSDEErr = SE_rasbandinfo_get_max_level(*poBand, (long*)&nOverviews, &bSkipLevel);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_band_size" );

    }
    
    CPLDebug("SDERASTER", "We have %d overviews", nOverviews);
    return 0;
    return nOverviews;
    
}

CPLErr SDERasterBand::InitializeBand( void )

{    

    SDEDataset *poGDS = (SDEDataset *) poDS;
    SE_RASTILEINFO hTile;
    SE_RASCONSTRAINT hConstraint;
    
    long nSDEErr;
    nSDEErr = SE_rastileinfo_create(&hTile);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rastileinfo_create" );
        return CE_Fatal;
    }
    
    nSDEErr = SE_rasconstraint_create(&hConstraint);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasconstraint_create" );
        return CE_Fatal;
    }
    
    nSDEErr = SE_rasconstraint_set_level(hConstraint, 0);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasconstraint_create" );
        return CE_Fatal;
    }

    long pnBandNumber;
    nSDEErr = SE_rasbandinfo_get_band_number (*poBand, &pnBandNumber);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_band_number" );
        return CE_Fatal;
    }
    
    nSDEErr = SE_rasconstraint_set_bands(hConstraint, 1, &pnBandNumber);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasconstraint_set_bands" );
        return CE_Fatal;
    }
    
    SE_QUERYINFO query;

    nSDEErr = SE_queryinfo_create(&query);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_create" );
        return CE_Fatal;
    }
        
    nSDEErr = SE_queryinfo_set_tables(query, 1, (const char**) &(poGDS->pszLayerName), NULL);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_set_tables" );
        return CE_Fatal;
    }

    nSDEErr = SE_queryinfo_set_where_clause(query, (const char*) "");
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_set_where" );
        return CE_Fatal;
    }

    nSDEErr = SE_queryinfo_set_columns(query, 1, (const char**) &(poGDS->pszColumnName));
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_set_where" );
        return CE_Fatal;
    }

    SE_STREAM stream;
    nSDEErr = SE_stream_create(*(poGDS->hConnection), &stream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_create" );
        return CE_Fatal;
    }

    nSDEErr = SE_stream_query_with_info(stream, query);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_query_with_info" );
        return CE_Fatal;
    }

    nSDEErr = SE_stream_execute (stream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_execute" );
        return CE_Fatal;
    }
    nSDEErr = SE_stream_fetch (stream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_fetch" );
        return CE_Fatal;
    }

    nSDEErr = SE_stream_get_raster (stream, 1, *(poGDS->hAttributes));
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_fetch" );
        return CE_Fatal;
    }

    
    nSDEErr = SE_stream_query_raster_tile(stream, hConstraint);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_query_raster_tile" );
        return CE_Fatal;
    }
    

    SE_rasconstraint_free (hConstraint);
    SE_rastileinfo_free (hTile);
 
    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SDERasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    SDEDataset *poGDS = (SDEDataset *) poDS;

    return InitializeBand();
    return CE_None;
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
/*                             ComputeColorTable()                      */
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
GDALColorInterp SDERasterBand::GetColorInterpretation()
{
    if (SE_rasbandinfo_has_colormap(*poBand)) 
        return GCI_PaletteIndex;
    else
        return GCI_GrayIndex;
}
    
    
/************************************************************************/
/*                             ComputeColorTable()                      */
/************************************************************************/
GDALColorTable* SDERasterBand::ComputeColorTable(void) 
{

//    if (SE_rasbandinfo_has_colormap(band)) 
//        poCT = ComputeColorTable(band);    
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
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_BYTE SE_COLORMAP_RGB Colormap Entry: %d %d %d", red, blue, green);
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
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_BYTE SE_COLORMAP_RGBA Colormap Entry: %d %d %d %d", red, blue, green, alpha);
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
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_SHORT  SE_COLORMAP_RGB Colormap Entry: %d %d %d", red, blue, green);
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
                        CPLDebug ("SDERASTER", "SE_COLORMAP_DATA_SHORT SE_COLORMAP_RGBA Colormap Entry: %d %d %d %d", red, blue, green, alpha);
                    }
                    break;
            }
            break;
    }
    GDALColorEntry sColor;
    return poCT;
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