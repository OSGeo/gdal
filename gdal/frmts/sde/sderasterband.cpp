#include "sderasterband.h"

/************************************************************************/
/*                           SDERasterBand()                            */
/************************************************************************/

SDERasterBand::SDERasterBand( SDEDataset *poDS, int nBand, const SE_RASBANDINFO& band )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SDERasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    SDEDataset *poGDS = (SDEDataset *) poDS;
    char  *pszRecord;
    int   nRecordSize = nBlockXSize*5 + 9 + 2;
    int   i;



    return CE_None;
}

/************************************************************************/
/*                             GetStatistics()                          */
/************************************************************************/

CPLErr SDERasterBand::GetStatistics( int bApproxOK, int bForce,
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev ) 
{
    return CE_None;
}

/************************************************************************/
/*                             ComputeColorTable()                      */
/************************************************************************/
GDALColorTable* SDERasterBand::ComputeColorTable(const SE_RASBANDINFO& band) {

//    if (SE_rasbandinfo_has_colormap(band)) 
//        poCT = ComputeColorTable(band);    
    SE_COLORMAP_TYPE eCMap_Type;
    SE_COLORMAP_DATA_TYPE eCMap_DataType;
    
    long nCMapEntries;
    void * phSDEColormapData;
    
    unsigned char* puszSDECMapData;
    unsigned short* pushSDECMapData;
    
    long nSDEErr;
    
    nSDEErr = SE_rasbandinfo_get_colormap(  band,
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