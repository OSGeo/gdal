/******************************************************************************
 * $Id$
 *
 * Project:  IRIS Reader
 * Purpose:  All code for IRIS format Reader
 * Author:   Roger Veciana, rveciana@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Roger Veciana <rveciana@gmail.com>
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

#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include <sstream>


CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_IRIS(void);
CPL_C_END

#define ARRAY_ELEMENT_COUNT(x) ((sizeof(x))/sizeof(x[0]))

/************************************************************************/
/* ==================================================================== */
/*                                  IRISDataset                         */
/* ==================================================================== */
/************************************************************************/

class IRISRasterBand;

class IRISDataset : public GDALPamDataset
{
    friend class IRISRasterBand;

    VSILFILE              *fp;
    GByte                 abyHeader[640];
    int                   bNoDataSet;
    double                dfNoDataValue;
    static const char* const   aszProductNames[];
    static const char* const   aszDataTypeCodes[];
    static const char* const   aszDataTypes[];   
    static const char* const   aszProjections[];   
    unsigned short        nProductCode;
    unsigned short        nDataTypeCode;
    unsigned char         nProjectionCode;
    float                 fNyquistVelocity;
    char*                 pszSRS_WKT;
    public:
        IRISDataset();
        ~IRISDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
 
};

const char* const IRISDataset::aszProductNames[]= {
    "", "PPI", "RHI", "CAPPI", "CROSS", "TOPS", "TRACK", "RAIN1", "RAINN",
    "VVP", "VIL", "SHEAR", "WARN", "CATCH", "RTI", "RAW", "MAX", "USER",
    "USERV", "OTHER", "STATUS", "SLINE", "WIND", "BEAM", "TEXT", "FCAST",
    "NDOP", "IMAGE", "COMP", "TDWR", "GAGE", "DWELL", "SRI", "BASE", "HMAX"};

const char* const IRISDataset::aszDataTypeCodes[]={
    "XHDR", "DBT" ,"dBZ", "VEL", "WIDTH", "ZDR", "ORAIN", "dBZC", "DBT2",
    "dBZ2", "VEL2", "WIDTH2", "ZDR2", "RAINRATE2", "KDP", "KDP2", "PHIDP",
    "VELC", "SQI", "RHOHV", "RHOHV2", "dBZC2", "VELC2", "SQI2", "PHIDP2",
    "LDRH", "LDRH2", "LDRV", "LDRV2", "FLAGS", "FLAGS2", "FLOAT32", "HEIGHT",
    "VIL2", "NULL", "SHEAR", "DIVERGE2", "FLIQUID2", "USER", "OTHER", "DEFORM2",
    "VVEL2", "HVEL2", "HDIR2", "AXDIL2", "TIME2", "RHOH", "RHOH2", "RHOV",
    "RHOV2", "PHIH", "PHIH2", "PHIV", "PHIV2", "USER2", "HCLASS", "HCLASS2",
    "ZDRC", "ZDRC2", "TEMPERATURE16", "VIR16", "DBTV8", "DBTV16", "DBZV8",
    "DBZV16", "SNR8", "SNR16", "ALBEDO8", "ALBEDO16", "VILD16", "TURB16"};
const char* const IRISDataset::aszDataTypes[]={
    "Extended Headers","Total H power (1 byte)","Clutter Corrected H reflectivity (1 byte)",
    "Velocity (1 byte)","Width (1 byte)","Differential reflectivity (1 byte)",
    "Old Rainfall rate (stored as dBZ)","Fully corrected reflectivity (1 byte)",
    "Uncorrected reflectivity (2 byte)","Corrected reflectivity (2 byte)",
    "Velocity (2 byte)","Width (2 byte)","Differential reflectivity (2 byte)",
    "Rainfall rate (2 byte)","Kdp (specific differential phase)(1 byte)",
    "Kdp (specific differential phase)(2 byte)","PHIdp (differential phase)(1 byte)",
    "Corrected Velocity (1 byte)","SQI (1 byte)","RhoHV(0) (1 byte)","RhoHV(0) (2 byte)",
    "Fully corrected reflectivity (2 byte)","Corrected Velocity (2 byte)","SQI (2 byte)",
    "PHIdp (differential phase)(2 byte)","LDR H to V (1 byte)","LDR H to V (2 byte)",
    "LDR V to H (1 byte)","LDR V to H (2 byte)","Individual flag bits for each bin","",
    "Test of floating format", "Height (1/10 km) (1 byte)", "Linear liquid (.001mm) (2 byte)",
    "Data type is not applicable", "Wind Shear (1 byte)", "Divergence (.001 10**-4) (2-byte)",
    "Floated liquid (2 byte)", "User type, unspecified data (1 byte)",
    "Unspecified data, no color legend", "Deformation (.001 10**-4) (2-byte)",
    "Vertical velocity (.01 m/s) (2-byte)", "Horizontal velocity (.01 m/s) (2-byte)",
    "Horizontal wind direction (.1 degree) (2-byte)", "Axis of Dillitation (.1 degree) (2-byte)",
    "Time of data (seconds) (2-byte)", "Rho H to V (1 byte)", "Rho H to V (2 byte)",
    "Rho V to H (1 byte)", "Rho V to H (2 byte)", "Phi H to V (1 byte)", "Phi H to V (2 byte)",
    "Phi V to H (1 byte)", "Phi V to H (2 byte)", "User type, unspecified data (2 byte)",
    "Hydrometeor class (1 byte)", "Hydrometeor class (2 byte)", "Corrected Differential reflectivity (1 byte)",
    "Corrected Differential reflectivity (2 byte)", "Temperature (2 byte)",
    "Vertically Integrated Reflectivity (2 byte)", "Total V Power (1 byte)", "Total V Power (2 byte)",
    "Clutter Corrected V Reflectivity (1 byte)", "Clutter Corrected V Reflectivity (2 byte)",
    "Signal to Noise ratio (1 byte)", "Signal to Noise ratio (2 byte)", "Albedo (1 byte)",
    "Albedo (2 byte)", "VIL Density (2 byte)", "Turbulence (2 byte)"};
const char* const IRISDataset::aszProjections[]={
    "Azimutal equidistant","Mercator","Polar Stereographic","UTM",
    "Prespective from geosync","Equidistant cylindrical","Gnomonic",
    "Gauss conformal","Lambert conformal conic"};

/************************************************************************/
/* ==================================================================== */
/*                            IRISRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class IRISRasterBand : public GDALPamRasterBand
{
    friend class IRISDataset;

 
    unsigned char*        pszRecord;
    int                   bBufferAllocFailed;

    public:
        IRISRasterBand( IRISDataset *, int );
        ~IRISRasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );

    virtual double          GetNoDataValue( int * );
    virtual CPLErr          SetNoDataValue( double );
};


/************************************************************************/
/*                           IRISRasterBand()                           */
/************************************************************************/

IRISRasterBand::IRISRasterBand( IRISDataset *poDS, int nBand )
{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_Float32;
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
    pszRecord = NULL;
    bBufferAllocFailed = FALSE;
}

IRISRasterBand::~IRISRasterBand()
{
    VSIFree(pszRecord);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr IRISRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    IRISDataset *poGDS = (IRISDataset *) poDS;

    //printf("hola %d %s\n",poGDS->dataTypeCode,poGDS->aszDataTypeCodes[poGDS->dataTypeCode]);
    //Every product type has it's own size. TODO: Move it like dataType
    int nDataLength = 1;
    if(poGDS->nDataTypeCode == 2){nDataLength=1;}
    else if(poGDS->nDataTypeCode == 37){nDataLength=2;}
    else if(poGDS->nDataTypeCode == 33){nDataLength=2;}
    else if(poGDS->nDataTypeCode == 32){nDataLength=1;}

    int i;
    //We allocate space for storing a record:
    if (pszRecord == NULL)
    {
        if (bBufferAllocFailed)
            return CE_Failure;

        pszRecord = (unsigned char *) VSIMalloc(nBlockXSize*nDataLength);

        if (pszRecord == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate scanline buffer");
            bBufferAllocFailed = TRUE;
            return CE_Failure;
        }
    }

    //Prepare to read (640 is the header size in bytes) and read (the y axis in the IRIS files in the inverse direction)
    VSIFSeekL( poGDS->fp, 640 + nBlockXSize*nDataLength*(poGDS->GetRasterYSize()-1-nBlockYOff), SEEK_SET );

    VSIFReadL( pszRecord, 1, nBlockXSize*nDataLength, poGDS->fp );

    
    //If datatype is dbZ:
    //See point 3.3.5 at page 3.42 of the manual
    if(poGDS->nDataTypeCode == 2){
        float fVal;
        for (i=0;i<nBlockXSize;i++){
            fVal = (((float) *(pszRecord+i*nDataLength)) -64)/2.0;
            if (fVal == 95.5)
                fVal = -9999;
            ((float *) pImage)[i] = fVal;
        }
    //Fliquid2 (Rain1 & Rainn products)
    //See point 3.3.11 at page 3.43 of the manual
    } else if(poGDS->nDataTypeCode == 37){
        unsigned short nVal, nExp, nMantissa;
        float fVal2=0;
        for (i=0;i<nBlockXSize;i++){
            nVal = CPL_LSBUINT16PTR(pszRecord+i*nDataLength);
            nExp = nVal>>12;
            nMantissa = nVal - (nExp<<12);
            if (nVal == 65535)
                fVal2 = -9999;
            else if (nExp == 0)
                fVal2 = (float) nMantissa / 1000.0;
            else
                fVal2 = (float)((nMantissa+4096)<<(nExp-1))/1000.0;
            ((float *) pImage)[i] = fVal2;
        }
    //VIL2 (VIL products)
    //See point 3.3.41 at page 3.54 of the manual
    } else if(poGDS->nDataTypeCode == 33){ 
        float fVal;
        for (i=0;i<nBlockXSize;i++){
            fVal = (float) CPL_LSBUINT16PTR(pszRecord+i*nDataLength);
            if (fVal == 65535)
                ((float *) pImage)[i] = -9999;
            else if (fVal == 0)
                ((float *) pImage)[i] = -1;
            else
                ((float *) pImage)[i] = (fVal-1)/1000;
        }
    //HEIGTH (TOPS products)
    //See point 3.3.14 at page 3.46 of the manual
    } else if(poGDS->nDataTypeCode == 32){ 
        unsigned char nVal;
        for (i=0;i<nBlockXSize;i++){
            nVal =  *(pszRecord+i*nDataLength) ;
            if (nVal == 255)
                ((float *) pImage)[i] = -9999;
            else if (nVal == 0)
                ((float *) pImage)[i] = -1;
            else
                ((float *) pImage)[i] = ((float) nVal - 1) / 10;
        }		
    //VEL (Velocity 1-Byte in PPI & others)
    //See point 3.3.37 at page 3.53 of the manual
    } else if(poGDS->nDataTypeCode == 3){
          float fVal;
        for (i=0;i<nBlockXSize;i++){
            fVal = (float) *(pszRecord+i*nDataLength);
            if (fVal == 0)
                fVal = -9997; 
            else if(fVal == 1)
                fVal = -9998; 
            else if(fVal == 255)
                fVal = -9999; 
            else
                fVal = poGDS->fNyquistVelocity * (fVal - 128)/127;
            ((float *) pImage)[i] = fVal;     
        }       
    }

    return CE_None;
} 

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr IRISRasterBand::SetNoDataValue( double dfNoData )

{
    IRISDataset *poGDS = (IRISDataset *) poDS;
   // if( poGDS->bNoDataSet && poGDS->dfNoDataValue == dfNoData )
   //   return CE_None;


    poGDS->bNoDataSet = TRUE;
    poGDS->dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double IRISRasterBand::GetNoDataValue( int * pbSuccess )

{
    IRISDataset *poGDS = (IRISDataset *) poDS;


    if( poGDS->bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return poGDS->dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}


/************************************************************************/
/* ==================================================================== */
/*                              IRISDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            IRISDataset()                             */
/************************************************************************/

IRISDataset::IRISDataset() : fp(NULL), pszSRS_WKT(NULL)

{
}

/************************************************************************/
/*                           ~IRISDataset()                             */
/************************************************************************/

IRISDataset::~IRISDataset()

{
    FlushCache();
    if( fp != NULL )
        VSIFCloseL( fp );
    CPLFree( pszSRS_WKT );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr IRISDataset::GetGeoTransform( double * padfTransform )

{
    float fRadarLocX, fRadarLocY, fScaleX, fScaleY;

    fRadarLocX = float (CPL_LSBSINT32PTR (abyHeader + 112 + 12 )) / 1000;
    fRadarLocY = float (CPL_LSBSINT32PTR (abyHeader + 116 + 12 )) / 1000;
 
    fScaleX = float (CPL_LSBSINT32PTR (abyHeader + 88 + 12 )) / 100;
    fScaleY = float (CPL_LSBSINT32PTR (abyHeader + 92 + 12 )) / 100;


    padfTransform[0] = -1*(fRadarLocX*fScaleX);
    padfTransform[3] = fRadarLocY*fScaleY;
    padfTransform[1] = fScaleX;
    padfTransform[2] = 0.0;
        
    padfTransform[4] = 0.0;
    padfTransform[5] = -1*fScaleY;

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *IRISDataset::GetProjectionRef(){
    if (pszSRS_WKT != NULL)
        return pszSRS_WKT;
    float fEquatorialRadius = float( (CPL_LSBUINT32PTR (abyHeader+220+320+12)))/100; //They give it in cm
    if(fEquatorialRadius == 0) // if Radius is 0, change to 6371000 Point 3.2.27 pag 3-15
        fEquatorialRadius = 6371000; 
    float fFlattening = float( (CPL_LSBUINT32PTR (abyHeader+224+320+12)))/1000000; //Point 3.2.27 pag 3-15

    float fCenterLon = 360 * float((CPL_LSBUINT32PTR (abyHeader+112+320+12))) / 4294967295LL;
    float fCenterLat = 360 * float((CPL_LSBUINT32PTR (abyHeader+108+320+12))) / 4294967295LL;

    OGRSpatialReference oSRSLatLon, oSRSOut;
    

    //The center coordinates are given in LatLon and the defined ellipsoid. Necessary to calculate false northing.
    oSRSLatLon.SetGeogCS("unnamed ellipse",
                        "unknown", 
                        "unnamed", 
                        fEquatorialRadius, fFlattening, 
                        "Greenwich", 0.0, 
                        "degree", 0.0174532925199433);

    ////MERCATOR PROJECTION
    if(EQUAL(aszProjections[nProjectionCode],"Mercator")){

        OGRCoordinateTransformation *poTransform = NULL;
        oSRSOut.SetGeogCS("unnamed ellipse",
                        "unknown", 
                        "unnamed", 
                        fEquatorialRadius, fFlattening, 
                        "Greenwich", 0.0, 
                        "degree", 0.0174532925199433);
    
        oSRSOut.SetMercator(fCenterLat,fCenterLon,1,0,0);

        double dfX, dfY;
        poTransform = OGRCreateCoordinateTransformation( &oSRSLatLon,
                                                  &oSRSOut );
        dfX = fCenterLon ;
        dfY =  fCenterLat ;
          
        if( poTransform == NULL || !poTransform->Transform( 1, &dfX, &dfY ) )
             CPLError( CE_Failure, CPLE_None, 
             "Transformation Failed\n" );
        oSRSOut.SetMercator(fCenterLat,fCenterLon,1,0,-1*dfY);
        oSRSOut.exportToWkt(&pszSRS_WKT) ;
        OGRCoordinateTransformation::DestroyCT( poTransform );
    }else if(EQUAL(aszProjections[nProjectionCode],"Azimutal equidistant")){

        oSRSOut.SetGeogCS("unnamed ellipse",
                        "unknown", 
                        "unnamed", 
                        fEquatorialRadius, fFlattening, 
                        "Greenwich", 0.0, 
                        "degree", 0.0174532925199433);
        oSRSOut.SetAE(fCenterLat,fCenterLon,0,0);
        oSRSOut.exportToWkt(&pszSRS_WKT) ;
    }

    return pszSRS_WKT;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int IRISDataset::Identify( GDALOpenInfo * poOpenInfo )

{

/* -------------------------------------------------------------------- */
/*      Confirm that the file is an IRIS file                           */
/* -------------------------------------------------------------------- */
    //Si no el posem, peta al fer el translate, quan s'obre Identify des de GDALIdentifyDriver
    if( poOpenInfo->nHeaderBytes < 640 )
        return FALSE;


    short nId1 = CPL_LSBSINT16PTR(poOpenInfo->pabyHeader);
    short nId2 = CPL_LSBSINT16PTR(poOpenInfo->pabyHeader+12);
    unsigned short nType = CPL_LSBUINT16PTR (poOpenInfo->pabyHeader+24);

    /*Check if the two headers are 27 (product hdr) & 26 (product configuration), and the product type is in the range 1 -> 34*/
    if( !(nId1 == 27 && nId2 == 26 && nType > 0 && nType < 35) )
        return FALSE;
     
    return TRUE;
}

/************************************************************************/
/*                             FillString()                             */
/************************************************************************/

static void FillString(char* szBuffer, size_t nBufferSize, void* pSrcBuffer)
{
    for(size_t i = 0; i < nBufferSize - 1; i++)
        szBuffer[i] = ((char*)pSrcBuffer)[i];
    szBuffer[nBufferSize-1] = '\0';
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *IRISDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo))
        return NULL;
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The IRIS driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    IRISDataset *poDS;

    poDS = new IRISDataset();

    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if (poDS->fp == NULL)
    {
        delete poDS;
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFReadL( poDS->abyHeader, 1, 640, poDS->fp );
    int nXSize = CPL_LSBSINT32PTR(poDS->abyHeader+100+12);
    int nYSize = CPL_LSBSINT32PTR(poDS->abyHeader+104+12);
    int nNumBands = CPL_LSBSINT32PTR(poDS->abyHeader+108+12);

    poDS->nRasterXSize = nXSize;

    poDS->nRasterYSize = nYSize;
    if  (poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Invalid dimensions : %d x %d", 
                  poDS->nRasterXSize, poDS->nRasterYSize); 
        delete poDS;
        return NULL;
    }
    
    if( !GDALCheckBandCount(nNumBands, TRUE) )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for (int iBandNum = 1; iBandNum <= nNumBands; iBandNum++) {
        poDS->SetBand( iBandNum, new IRISRasterBand( poDS, iBandNum ));
        poDS->GetRasterBand(iBandNum)->SetNoDataValue(-9999);
    }
    
/* -------------------------------------------------------------------- */
/*      Setting the Metadata                                            */
/* -------------------------------------------------------------------- */
    //See point 3.2.26 at page 3.12 of the manual
    poDS->nProductCode = CPL_LSBUINT16PTR (poDS->abyHeader+12+12);
    poDS->SetMetadataItem( "PRODUCT_ID", CPLString().Printf("%d", poDS->nProductCode ));
    if( poDS->nProductCode >= ARRAY_ELEMENT_COUNT(poDS->aszProductNames) )
    {
        delete poDS;
        return NULL;
    }
    
    poDS->SetMetadataItem( "PRODUCT",poDS->aszProductNames[poDS->nProductCode]);
    
    poDS->nDataTypeCode = CPL_LSBUINT16PTR (poDS->abyHeader+130+12);
    if( poDS->nDataTypeCode >= ARRAY_ELEMENT_COUNT(poDS->aszDataTypeCodes) )
    {
        delete poDS;
        return NULL;
    }
    poDS->SetMetadataItem( "DATA_TYPE_CODE",poDS->aszDataTypeCodes[poDS->nDataTypeCode]);

    if( poDS->nDataTypeCode >= ARRAY_ELEMENT_COUNT(poDS->aszDataTypes) )
    {
        delete poDS;
        return NULL;
    }
    poDS->SetMetadataItem( "DATA_TYPE",poDS->aszDataTypes[poDS->nDataTypeCode]);

    unsigned short nDataTypeInputCode = CPL_LSBUINT16PTR (poDS->abyHeader+144+12);
    if( nDataTypeInputCode >= ARRAY_ELEMENT_COUNT(poDS->aszDataTypeCodes) )
    {
        delete poDS;
        return NULL;
    }
    poDS->SetMetadataItem( "DATA_TYPE_INPUT_CODE",poDS->aszDataTypeCodes[nDataTypeInputCode]);

    unsigned short nDataTypeInput = CPL_LSBUINT16PTR (poDS->abyHeader+144+12);
    if( nDataTypeInput >= ARRAY_ELEMENT_COUNT(poDS->aszDataTypes) )
    {
        delete poDS;
        return NULL;
    }
    poDS->SetMetadataItem( "DATA_TYPE_INPUT",poDS->aszDataTypes[nDataTypeInput]);

    poDS->nProjectionCode = * (unsigned char *) (poDS->abyHeader+146+12);
    if( poDS->nProjectionCode >= ARRAY_ELEMENT_COUNT(poDS->aszProjections) )
    {
        delete poDS;
        return NULL;
    }

    ////TIMES
    int nSeconds = CPL_LSBSINT32PTR(poDS->abyHeader+20+12);
    
    int nHour =  (nSeconds - (nSeconds%3600)) /3600;
    int nMinute = ((nSeconds - nHour * 3600) - (nSeconds - nHour * 3600)%60)/ 60;
    int nSecond = nSeconds - nHour * 3600 - nMinute * 60;
    
    short nYear = CPL_LSBSINT16PTR(poDS->abyHeader+26+12);
    short nMonth = CPL_LSBSINT16PTR(poDS->abyHeader+28+12);
    short nDay = CPL_LSBSINT16PTR(poDS->abyHeader+30+12);

    poDS->SetMetadataItem( "TIME_PRODUCT_GENERATED", CPLString().Printf("%d-%02d-%02d %02d:%02d:%02d", nYear, nMonth, nDay, nHour, nMinute, nSecond ) );


    nSeconds = CPL_LSBSINT32PTR(poDS->abyHeader+32+12);
    
    nHour =  (nSeconds - (nSeconds%3600)) /3600;
    nMinute = ((nSeconds - nHour * 3600) - (nSeconds - nHour * 3600)%60)/ 60;
    nSecond = nSeconds - nHour * 3600 - nMinute * 60;
    
    nYear = CPL_LSBSINT16PTR(poDS->abyHeader+26+12);
    nMonth = CPL_LSBSINT16PTR(poDS->abyHeader+28+12);
    nDay = CPL_LSBSINT16PTR(poDS->abyHeader+30+12);

    poDS->SetMetadataItem( "TIME_INPUT_INGEST_SWEEP", CPLString().Printf("%d-%02d-%02d %02d:%02d:%02d", nYear, nMonth, nDay, nHour, nMinute, nSecond ) );

    ///Site and task information

    char szSiteName[17] = ""; //Must have one extra char for string end!
    char szVersionName[9] = "";

    FillString(szSiteName, sizeof(szSiteName), poDS->abyHeader+320+12);
    FillString(szVersionName, sizeof(szVersionName), poDS->abyHeader+16+320+12);
    poDS->SetMetadataItem( "PRODUCT_SITE_NAME",szSiteName);
    poDS->SetMetadataItem( "PRODUCT_SITE_IRIS_VERSION",szVersionName);

    FillString(szSiteName, sizeof(szSiteName), poDS->abyHeader+90+320+12);
    FillString(szVersionName, sizeof(szVersionName), poDS->abyHeader+24+320+12);
    poDS->SetMetadataItem( "INGEST_SITE_NAME",szSiteName);
    poDS->SetMetadataItem( "INGEST_SITE_IRIS_VERSION",szVersionName);

    FillString(szSiteName, sizeof(szSiteName), poDS->abyHeader+74+320+12);
    poDS->SetMetadataItem( "INGEST_HARDWARE_NAME",szSiteName);

    char szConfigFile[13] = "";
    FillString(szConfigFile, sizeof(szConfigFile), poDS->abyHeader+62+12);
    poDS->SetMetadataItem( "PRODUCT_CONFIGURATION_NAME",szConfigFile);

    char szTaskName[13] = "";
    FillString(szTaskName, sizeof(szTaskName), poDS->abyHeader+74+12);
    poDS->SetMetadataItem( "TASK_NAME",szTaskName);
   
    short nRadarHeight = CPL_LSBSINT16PTR(poDS->abyHeader+284+320+12);
    poDS->SetMetadataItem( "RADAR_HEIGHT",CPLString().Printf("%d m",nRadarHeight));
    short nGroundHeight = CPL_LSBSINT16PTR(poDS->abyHeader+118+320+12);
    poDS->SetMetadataItem( "GROUND_HEIGHT",CPLString().Printf("%d m",nRadarHeight-nGroundHeight)); //Ground height over the sea level

    unsigned short nFlags = CPL_LSBUINT16PTR (poDS->abyHeader+86+12);
    //Get eleventh bit
    nFlags=nFlags<<4;
    nFlags=nFlags>>15;
    if (nFlags == 1){
        poDS->SetMetadataItem( "COMPOSITED_PRODUCT","YES");
        unsigned int compositedMask = CPL_LSBUINT32PTR (poDS->abyHeader+232+320+12);
        poDS->SetMetadataItem( "COMPOSITED_PRODUCT_MASK",CPLString().Printf("0x%08x",compositedMask));
    } else{
        poDS->SetMetadataItem( "COMPOSITED_PRODUCT","NO");
    }

    //Wave values
    poDS->SetMetadataItem( "PRF",CPLString().Printf("%d Hz",CPL_LSBSINT32PTR(poDS->abyHeader+120+320+12))); 
    poDS->SetMetadataItem( "WAVELENGTH",CPLString().Printf("%4.2f cm",(float) CPL_LSBSINT32PTR(poDS->abyHeader+148+320+12)/100)); 
    unsigned short nPolarizationType = CPL_LSBUINT16PTR (poDS->abyHeader+172+320+12);
    float fNyquist = (CPL_LSBSINT32PTR(poDS->abyHeader+120+320+12))*((float) CPL_LSBSINT32PTR(poDS->abyHeader+148+320+12)/10000)/4; //See section 3.3.37 & 3.2.54
    if (nPolarizationType == 1)
        fNyquist = fNyquist * 2;
    else if(nPolarizationType == 2)
        fNyquist = fNyquist * 3;
    else if(nPolarizationType == 3)
        fNyquist = fNyquist * 4;
    poDS->fNyquistVelocity = fNyquist;
    poDS->SetMetadataItem( "NYQUIST_VELOCITY",CPLString().Printf("%.2f m/s",fNyquist)); 

    ///Product dependent metadata (stored in 80 bytes fromm 162 bytes at the product header) See point 3.2.30 at page 3.19 of the manual
    //See point 3.2.25 at page 3.12 of the manual
    if (EQUAL(poDS->aszProductNames[poDS->nProductCode],"PPI")){
        //Degrees = 360 * (Binary Angle)*2^N
        //float fElevation = 360 * float((CPL_LSBUINT16PTR (poDS->abyHeader+164+12))) / 65536;
        float fElevation = 360 * float((CPL_LSBSINT16PTR (poDS->abyHeader+164+12))) / 65536;

        poDS->SetMetadataItem( "PPI_ELEVATION_ANGLE",CPLString().Printf("%f",fElevation));
        if (EQUAL(poDS->aszDataTypeCodes[poDS->nDataTypeCode],"dBZ"))
            poDS->SetMetadataItem( "DATA_TYPE_UNITS","dBZ");
        else
            poDS->SetMetadataItem( "DATA_TYPE_UNITS","m/s");
        //See point 3.2.2 at page 3.2 of the manual
    } else if (EQUAL(poDS->aszProductNames[poDS->nProductCode],"CAPPI")){
        float fElevation = ((float) CPL_LSBSINT32PTR(poDS->abyHeader+4+164+12))/100;
        poDS->SetMetadataItem( "CAPPI_HEIGHT",CPLString().Printf("%.1f m",fElevation));
        float fAzimuthSmoothingForShear = 360 * float((CPL_LSBUINT16PTR (poDS->abyHeader+10+164+12))) / 65536;
        poDS->SetMetadataItem( "AZIMUTH_SMOOTHING_FOR_SHEAR" ,CPLString().Printf("%.1f", fAzimuthSmoothingForShear));
        unsigned int  nMaxAgeVVPCorrection = CPL_LSBUINT32PTR (poDS->abyHeader+24+164+12);
        poDS->SetMetadataItem( "MAX_AGE_FOR_SHEAR_VVP_CORRECTION" ,CPLString().Printf("%d s", nMaxAgeVVPCorrection));
        if (EQUAL(poDS->aszDataTypeCodes[poDS->nDataTypeCode],"dBZ"))
            poDS->SetMetadataItem( "DATA_TYPE_UNITS","dBZ");
        else
            poDS->SetMetadataItem( "DATA_TYPE_UNITS","m/s");
        //See point 3.2.32 at page 3.19 of the manual
    } else if (EQUAL(poDS->aszProductNames[poDS->nProductCode],"RAIN1") || EQUAL(poDS->aszProductNames[poDS->nProductCode],"RAINN")){
        short nNumProducts = CPL_LSBSINT16PTR(poDS->abyHeader+170+320+12);
        poDS->SetMetadataItem( "NUM_FILES_USED",CPLString().Printf("%d",nNumProducts));
    
        float fMinZAcum= (float)((CPL_LSBUINT32PTR (poDS->abyHeader+164+12))-32768)/1000;
        poDS->SetMetadataItem( "MINIMUM_Z_TO_ACUMULATE",CPLString().Printf("%f",fMinZAcum));

        unsigned short nSecondsOfAccumulation = CPL_LSBUINT16PTR (poDS->abyHeader+6+164+12);
        poDS->SetMetadataItem( "SECONDS_OF_ACCUMULATION",CPLString().Printf("%d s",nSecondsOfAccumulation));

        unsigned int nSpanInputFiles = CPL_LSBUINT32PTR (poDS->abyHeader+24+164+12);
        poDS->SetMetadataItem( "SPAN_OF_INPUT_FILES",CPLString().Printf("%d s",nSpanInputFiles));
        poDS->SetMetadataItem( "DATA_TYPE_UNITS","mm");

        char szInputProductName[13] = "";
        for(int k=0; k<12;k++)
            szInputProductName[k] = * (char *) (poDS->abyHeader+k+12+164+12);
        poDS->SetMetadataItem( "INPUT_PRODUCT_NAME",CPLString().Printf("%s",szInputProductName));        
    
        if (EQUAL(poDS->aszProductNames[poDS->nProductCode],"RAINN"))
             poDS->SetMetadataItem( "NUM_HOURS_ACCUMULATE",CPLString().Printf("%d",CPL_LSBUINT16PTR (poDS->abyHeader+10+164+12)));   
        
    //See point 3.2.73 at page 3.36 of the manual
    } else if (EQUAL(poDS->aszProductNames[poDS->nProductCode],"VIL")){
        float fBottomHeigthInterval = (float) CPL_LSBSINT32PTR(poDS->abyHeader+4+164+12) / 100;
        poDS->SetMetadataItem( "BOTTOM_OF_HEIGTH_INTERVAL",CPLString().Printf("%.1f m",fBottomHeigthInterval)); 
        float fTopHeigthInterval = (float) CPL_LSBSINT32PTR(poDS->abyHeader+8+164+12) / 100;
        poDS->SetMetadataItem( "TOP_OF_HEIGTH_INTERVAL",CPLString().Printf("%.1f m",fTopHeigthInterval));   
        poDS->SetMetadataItem( "VIL_DENSITY_NOT_AVAILABLE_VALUE","-1");
        poDS->SetMetadataItem( "DATA_TYPE_UNITS","mm");
    //See point 3.2.68 at page 3.36 of the manual
    } else if (EQUAL(poDS->aszProductNames[poDS->nProductCode],"TOPS")){
        float fZThreshold = (float) CPL_LSBSINT16PTR(poDS->abyHeader+4+164+12) / 16;
        poDS->SetMetadataItem( "Z_THRESHOLD",CPLString().Printf("%.1f dBZ",fZThreshold));
        poDS->SetMetadataItem( "ECHO_TOPS_NOT_AVAILABLE_VALUE","-1");
        poDS->SetMetadataItem( "DATA_TYPE_UNITS","km");
    //See point 3.2.20 at page 3.10 of the manual
    } else if (EQUAL(poDS->aszProductNames[poDS->nProductCode],"MAX")){
        float fBottomInterval = (float) CPL_LSBSINT32PTR(poDS->abyHeader+4+164+12) / 100;
        poDS->SetMetadataItem( "BOTTOM_OF_INTERVAL",CPLString().Printf("%.1f m",fBottomInterval));
        float fTopInterval = (float) CPL_LSBSINT32PTR(poDS->abyHeader+8+164+12) / 100;
        poDS->SetMetadataItem( "TOP_OF_INTERVAL",CPLString().Printf("%.1f m",fTopInterval));
        int nNumPixelsSidePanels = CPL_LSBSINT32PTR(poDS->abyHeader+12+164+12); 
        poDS->SetMetadataItem( "NUM_PIXELS_SIDE_PANELS",CPLString().Printf("%d",nNumPixelsSidePanels));         
        short nHorizontalSmootherSidePanels = CPL_LSBSINT16PTR(poDS->abyHeader+16+164+12); 
        poDS->SetMetadataItem( "HORIZONTAL_SMOOTHER_SIDE_PANELS",CPLString().Printf("%d",nHorizontalSmootherSidePanels));   
        short nVerticalSmootherSidePanels = CPL_LSBSINT16PTR(poDS->abyHeader+18+164+12); 
        poDS->SetMetadataItem( "VERTICAL_SMOOTHER_SIDE_PANELS",CPLString().Printf("%d",nVerticalSmootherSidePanels));
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_IRIS()                         */
/************************************************************************/

void GDALRegister_IRIS()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "IRIS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "IRIS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "IRIS data (.PPI, .CAPPi etc)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#IRIS" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ppi" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = IRISDataset::Open;
        poDriver->pfnIdentify = IRISDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
