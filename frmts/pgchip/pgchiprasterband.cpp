/******************************************************************************
 *
 * File :    pgchiprasterband.cpp
 * Project:  PGCHIP Driver
 * Purpose:  GDALRasterBand code for POSTGIS CHIP/GDAL Driver 
 * Author:   Benjamin Simon, noumayoss@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Benjamin Simon, noumayoss@gmail.com
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
 ******************************************************************************
 * 
 * Revision 1.1  2005/08/29 bsimon
 * New
 *
 */

/************************************************************************/
/* ==================================================================== */
/*                            PGCHIPRasterBand                          */
/* ==================================================================== */
/************************************************************************/

#include "pgchip.h"


/************************************************************************/
/*                           PGCHIPRasterBand()                         */
/************************************************************************/

PGCHIPRasterBand::PGCHIPRasterBand( PGCHIPDataset *poDS, int nBand ){

    this->poDS = poDS;
    this->nBand = nBand;
    
    if( poDS->nBitDepth == 16 )
        eDataType = GDT_UInt16;
    else
        eDataType = GDT_Byte;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
    
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PGCHIPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage ){

    char    szCommand[1024];
    PGCHIPDataset *poGDS = (PGCHIPDataset *) poDS;
    PGconn      *hPGConn;
    PGresult    *hResult;
        
    int        chipDataSize;
    int        bandSize;
    int        nPixelSize;
    int        nPixelOffset;
    int        nXSize;
    int        i;
    
    hPGConn = poGDS->hPGConn;
        
    // Must start on the very left
    CPLAssert( nBlockXOff == 0 );
           
    
    if( poGDS->nBitDepth == 16 )
        nPixelSize = 2;
    else
        nPixelSize = 1;
    
    nPixelOffset = poGDS->nBands * nPixelSize;
    nXSize = GetXSize();
    bandSize = nPixelSize * nXSize;
    int sizePalette = 0;        
    
    if(poGDS->PGCHIP->future[2] == PGCHIP_COLOR_TYPE_PALETTE){
        sizePalette = (int)poGDS->PGCHIP->compression * sizeof(pgchip_color);
    }
    
    // Determine size of whole Image Data
    chipDataSize = poGDS->PGCHIP->size - sizeof(CHIP) - sizePalette;
    
    
/* -------------------------------------------------------------------- */
/*      Reading Chip (first pas only)                                   */
/* -------------------------------------------------------------------- */
    
    if(poGDS->PGCHIP->data == NULL){
    
        hResult = PQexec(hPGConn, "BEGIN");
    
        if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
        {
                                   
            PQclear( hResult );
            sprintf( szCommand,"SELECT raster FROM %s",poGDS->pszName);
            
            hResult = PQexec(hPGConn,szCommand);
        
            char *chipData =  PQgetvalue(hResult,0,0);        
            poGDS->PGCHIP->data = (char *) CPLMalloc(chipDataSize);
            
            char *data = chipData + (sizePalette + sizeof(CHIP))*2;
                    
            // Reading whole data
            for(i=0 ; i<chipDataSize ; i++){
                ((unsigned char *)poGDS->PGCHIP->data)[i] = parse_hex( &data[i*2]) ;
            }
            
            PQclear( hResult );
            
        }
        else {
            CPLError( CE_Failure, CPLE_AppDefined, "%s", PQerrorMessage(hPGConn) );
            PQclear( hResult );
        }
                
        hResult = PQexec(hPGConn, "COMMIT");
        PQclear( hResult );
        
     }
     
/* -------------------------------------------------------------------- */
/*      Extracting band from pointer                                    */
/* -------------------------------------------------------------------- */
     
     if(poGDS->PGCHIP->data){
     
               
        if( nPixelSize == 1 ){
        
            char *bufferData = ((char *)poGDS->PGCHIP->data) + (nBlockYOff * poGDS->nBands * bandSize) + ((nBand-1) * nPixelSize);
                
            for(i = 0; i < nXSize; i++ ){
                ((char *) pImage)[i] = bufferData[i*nPixelOffset];
            }
        }
        else {
        
            GUInt16 *bufferData = (GUInt16 *)(((char *)poGDS->PGCHIP->data) + (nBlockYOff * poGDS->nBands * bandSize) + ((nBand-1) * nPixelSize));
             
            for(i = 0; i < nXSize; i++ ){
                
                ((GUInt16 *) pImage)[i] = bufferData[i*poGDS->nBands];
                
            }
        }
     
     }  
         
    return CE_None;
}



/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/
GDALColorInterp PGCHIPRasterBand::GetColorInterpretation(){

    PGCHIPDataset	*poGDS = (PGCHIPDataset *) poDS;

    if( poGDS->nColorType == PGCHIP_COLOR_TYPE_GRAY ){
        return GCI_GrayIndex;
    }
    else  if( poGDS->nColorType == PGCHIP_COLOR_TYPE_PALETTE ){
        return GCI_PaletteIndex;    
    }
    else if(poGDS->nColorType == PGCHIP_COLOR_TYPE_RGB_ALPHA){
        if( nBand == 1 )
            return GCI_RedBand;
        else if( nBand == 2 )
            return GCI_GreenBand;
        else if( nBand == 3 )
            return GCI_BlueBand;
        else 
            return GCI_AlphaBand;
    }
    
    return GCI_GrayIndex;
}


/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *PGCHIPRasterBand::GetColorTable(){

    PGCHIPDataset	*poGDS = (PGCHIPDataset *) poDS;

    if( nBand == 1 )
        return poGDS->poColorTable;
    else
        return NULL;
}
