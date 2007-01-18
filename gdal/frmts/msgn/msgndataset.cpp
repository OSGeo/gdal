/******************************************************************************
 * $Id$
 *
 * Project:  MSG Native Reader
 * Purpose:  All code for EUMETSAT Archive format reader
 * Author:   Frans van den Bergh, fvdbergh@csir.co.za
 *
 ******************************************************************************
 * Copyright (c) 2005, Frans van den Bergh <fvdbergh@csir.co.za>
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
 
#include "gdal_priv.h"
#include "ogr_spatialref.h"

#include "msg_reader_core.h"
using namespace msg_native_format;

CPL_CVSID("$Id$");

CPL_C_START
void   GDALRegister_MSGN(void);
CPL_C_END

class MSGNRasterBand;

/************************************************************************/
/* ==================================================================== */
/*                            MSGNDataset                               */
/* ==================================================================== */
/************************************************************************/

class MSGNDataset : public GDALDataset
{
    friend class MSGNRasterBand;

    FILE       *fp;
    GByte      abyHeader[1012];
    
    Msg_reader_core*    msg_reader_core;
    double adfGeoTransform[6];
    char   *pszProjection;

  public:
        MSGNDataset();
               ~MSGNDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr     GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
    
    static void double2hex(double a, char* s);
};

/************************************************************************/
/* ==================================================================== */
/*                            MSGNRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class MSGNRasterBand : public GDALRasterBand
{
    friend class MSGNDataset;
    
    unsigned int visir_packet_size;
    unsigned int visir_bytes_per_line;
    unsigned int interline_spacing;
    double  GetNoDataValue (int *pbSuccess=NULL) {
        if (pbSuccess) {
            *pbSuccess = 1;
        }
        return MSGN_NODATA_VALUE;  
    }
    
    static const unsigned short MSGN_NODATA_VALUE;
    
  public:

               MSGNRasterBand( MSGNDataset *, int );
    
    virtual CPLErr IReadBlock( int, int, void * );
};

const unsigned short MSGNRasterBand::MSGN_NODATA_VALUE = 0; 

/************************************************************************/
/*                           MSGNRasterBand()                            */
/************************************************************************/

MSGNRasterBand::MSGNRasterBand( MSGNDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_UInt16;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
    
    visir_packet_size = poDS->msg_reader_core->get_visir_packet_size();
    visir_bytes_per_line = poDS->msg_reader_core->get_visir_bytes_per_line();
    interline_spacing = poDS->msg_reader_core->get_interline_spacing();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MSGNRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    MSGNDataset *poGDS = (MSGNDataset *) poDS;
    char       *pszRecord;
    
    unsigned int packet_length =  visir_bytes_per_line + sizeof(SUB_VISIRLINE);
    unsigned int packet_offset = poGDS->msg_reader_core->get_f_data_offset() +
        interline_spacing*nBlockYOff  + (nBand-1)*visir_packet_size + 
        (visir_packet_size - packet_length);
        
    VSIFSeek( poGDS->fp, packet_offset, SEEK_SET );
    
    pszRecord = (char *) CPLMalloc(packet_length);
    size_t nread = VSIFRead( pszRecord, 1, packet_length, poGDS->fp );
    
    SUB_VISIRLINE* p = (SUB_VISIRLINE*) pszRecord;
    to_native(*p);
    
    if (p->lineValidity != 1) {
        for (int c=0; c < nBlockXSize; c++) {
            ((short int *)pImage)[c] = MSGN_NODATA_VALUE;
        }
    }
    
    if ( nread != packet_length || 
        (p->lineNumberInVisirGrid - poGDS->msg_reader_core->get_line_start()) != (unsigned int)nBlockYOff ) {
        CPLFree( pszRecord );

        CPLError( CE_Failure, CPLE_AppDefined, "MSGN Scanline corrupt." );
        
        return CE_Failure;
    }
    
    // unpack the 10-bit values into 16-bit unsigned short ints
    unsigned char *cptr = (unsigned char*)pszRecord + 
        (packet_length - visir_bytes_per_line);
    int bitsLeft = 8;
    unsigned short value = 0;
    for (int c=0; c < nBlockXSize; c++) {
        value = 0;
        for (int bit=0; bit < 10; bit++) {
            value <<= 1;
            if (*cptr & 128) {
                value |= 1;   
            }
            *cptr <<= 1;
            bitsLeft--; 
            if (bitsLeft == 0) {
                cptr++;
            bitsLeft = 8;
            }
        }
        ((short int *)pImage)[c] = value;
    }
    
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                             MSGNDataset                             */
/* ==================================================================== */
/************************************************************************/

MSGNDataset::MSGNDataset() {
    pszProjection = CPLStrdup("");
}

/************************************************************************/
/*                            ~MSGNDataset()                             */
/************************************************************************/

MSGNDataset::~MSGNDataset()

{
    if( fp != NULL )
        VSIFClose( fp );
        
    CPLFree(pszProjection);    
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

void MSGNDataset::double2hex(double a, char* out_str) {
    const char hexlut[] = "0123456789abcdef";
    char* p = (char*)&a;
    char* s = out_str;
    for(unsigned int i=0; i < sizeof(double); i++) {
        *s = hexlut[(*p >> 4) & 0x0f];
        s++;
        *s = hexlut[*p & 0x0f];
        s++;
        p++;
    }
    *s = 0;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MSGNDataset::GetGeoTransform( double * padfTransform )

{

    for (int i=0; i < 6; i++) {
        padfTransform[i] = adfGeoTransform[i];
    }

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *MSGNDataset::GetProjectionRef()

{
    return ( pszProjection );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MSGNDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Before trying MSGNOpen() we first verify that there is at        */
/*      least one "\n#keyword" type signature in the first chunk of     */
/*      the file.                                                       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 50 )
        return NULL;

    /* check if this is a "NATIVE" MSG format image */
    if( !EQUALN((char *)poOpenInfo->pabyHeader,
                "FormatName                  : NATIVE", 36) )
    {
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    MSGNDataset        *poDS;

    poDS = new MSGNDataset();

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    
/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    // first reset the file pointer, then hand over to the msg_reader_core
    VSIFSeek( poDS->fp, 0, SEEK_SET );
    
    poDS->msg_reader_core = new Msg_reader_core(poDS->fp);    

    poDS->nRasterXSize = poDS->msg_reader_core->get_columns();
    poDS->nRasterYSize = poDS->msg_reader_core->get_lines();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    unsigned int band_count = 1, i;
    unsigned char* bands = poDS->msg_reader_core->get_band_map();
    for ( i=0; i < MSG_NUM_CHANNELS-1; i++) {
        if (bands[i]) {
            poDS->SetBand( band_count, new MSGNRasterBand( poDS, band_count ));
            band_count++;
        }
    }
    
    double pixel_gsd_x = 1000 * poDS->msg_reader_core->get_col_dir_step();  // convert from km to m
    double pixel_gsd_y = 1000 * poDS->msg_reader_core->get_line_dir_step(); // convert from km to m
    double origin_x = -pixel_gsd_x * (-(Conversions::nlines / 2.0) + poDS->msg_reader_core->get_col_start()); 
    double origin_y = -pixel_gsd_y * ((Conversions::nlines / 2.0) - poDS->msg_reader_core->get_line_start()); 
    
    poDS->adfGeoTransform[0] = origin_x;
    poDS->adfGeoTransform[1] = -pixel_gsd_x;
    poDS->adfGeoTransform[2] = 0.0;
    
    poDS->adfGeoTransform[3] = origin_y;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = pixel_gsd_y;
    
    OGRSpatialReference oSRS;

    oSRS.SetProjCS("Geostationary projection (MSG)");
    oSRS.SetGEOS(  0, 35785831, 0, 0 );
    oSRS.SetGeogCS(
        "MSG Ellipsoid",
        "MSG_DATUM",
        "MSG_SPHEROID",
        Conversions::rpol * 1000.0,
        1 / ( 1 - Conversions::rpol/Conversions::req)
    );
        
    oSRS.exportToWkt( &(poDS->pszProjection) );
    
    CALIBRATION* cal = poDS->msg_reader_core->get_calibration_parameters();
    char tagname[30];
    char field[300];
    char hexvalue1[30];
    char hexvalue2[30];
    
    poDS->SetMetadataItem("Radiometric parameters format", "offset slopehex_offset hex_slope");
    for (i=0; i < MSG_NUM_CHANNELS; i++) {
        sprintf(tagname, "ch%02d_cal", i+1);
        
        MSGNDataset::double2hex(cal[i].cal_offset, hexvalue1);
        MSGNDataset::double2hex(cal[i].cal_slope, hexvalue2);
        
        sprintf(field, "%.12e %.12e %s %s", cal[i].cal_offset,cal[i].cal_slope, hexvalue1, hexvalue2);
        poDS->SetMetadataItem(tagname, field);
    }
    
    poDS->SetMetadataItem("Blackbody parameters format", "vc A B");
    for (i=4; i < MSG_NUM_CHANNELS-1; i++) {
        sprintf(tagname, "ch%02d_blackbody", i);
        sprintf(field, "%.4f %.4f %.4f",
            Msg_reader_core::Blackbody_LUT[i].vc,
            Msg_reader_core::Blackbody_LUT[i].A,
            Msg_reader_core::Blackbody_LUT[i].B
        );
        poDS->SetMetadataItem(tagname, field);
    }
    
    sprintf(field, "%04d%02d%02d/%02d:%02d",
        poDS->msg_reader_core->get_year(),
        poDS->msg_reader_core->get_month(),
        poDS->msg_reader_core->get_day(),
        poDS->msg_reader_core->get_hour(),
        poDS->msg_reader_core->get_minute()
    );
    poDS->SetMetadataItem("Date/Time", field);
    
    
    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_MSGN()                          */
/************************************************************************/

void GDALRegister_MSGN()

{
    GDALDriver *poDriver;

    if( GDALGetDriverByName( "MSGN" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "MSGN" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "EUMETSAT Archive native (.nat)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#MSGN" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "nat" );

        poDriver->pfnOpen = MSGNDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
