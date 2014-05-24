/******************************************************************************
 * $Id$
 *
 * Project:  MSG Native Reader
 * Purpose:  All code for EUMETSAT Archive format reader
 * Author:   Frans van den Bergh, fvdbergh@csir.co.za
 *
 ******************************************************************************
 * Copyright (c) 2005, Frans van den Bergh <fvdbergh@csir.co.za>
 * Copyright (c) 2008-2009, Even Rouault <even dot rouault at mines-paris dot org>
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

typedef enum {
    MODE_VISIR,     // Visible and Infrared bands (1 through 11) in 10-bit raw mode
    MODE_HRV,       // Pan band (band 11) only, in 10-bit raw mode
    MODE_RAD     // Black-body temperature (K) for thermal bands only (4-10), 64-bit float
    } open_mode_type;

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

    Msg_reader_core*    msg_reader_core;
    double adfGeoTransform[6];
    char   *pszProjection;

  public:
        MSGNDataset();
               ~MSGNDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr     GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();

};

/************************************************************************/
/* ==================================================================== */
/*                            MSGNRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class MSGNRasterBand : public GDALRasterBand
{
    friend class MSGNDataset;

    unsigned int packet_size;
    unsigned int bytes_per_line;
    unsigned int interline_spacing;
    unsigned int orig_band_no;      // The name of the band
    unsigned int band_in_file;      // The effective index of the band in the file
    open_mode_type open_mode;

    double  GetNoDataValue (int *pbSuccess=NULL) {
        if (pbSuccess) {
            *pbSuccess = 1;
        }
        return MSGN_NODATA_VALUE;
    }

    double MSGN_NODATA_VALUE;

    char band_description[30];

  public:

        MSGNRasterBand( MSGNDataset *, int , open_mode_type mode, int orig_band_no, int band_in_file);

    virtual CPLErr IReadBlock( int, int, void * );
    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum(int *pbSuccess = NULL );
    virtual const char* GetDescription() const { return band_description; }
};


/************************************************************************/
/*                           MSGNRasterBand()                            */
/************************************************************************/

MSGNRasterBand::MSGNRasterBand( MSGNDataset *poDS, int nBand , open_mode_type mode, int orig_band_no, int band_in_file)

{
    this->poDS = poDS;
    this->nBand = nBand;                // GDAL's band number, i.e. always starts at 1
    this->open_mode = mode;
    this->orig_band_no = orig_band_no;
    this->band_in_file = band_in_file;

    sprintf(band_description, "band %02d", orig_band_no);

    if (mode != MODE_RAD) {
        eDataType = GDT_UInt16;
        MSGN_NODATA_VALUE = 0;
    } else {
        eDataType = GDT_Float64;
        MSGN_NODATA_VALUE = -1000;
    }

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    if (mode != MODE_HRV) {
        packet_size = poDS->msg_reader_core->get_visir_packet_size();
        bytes_per_line = poDS->msg_reader_core->get_visir_bytes_per_line();
    } else {
        packet_size = poDS->msg_reader_core->get_hrv_packet_size();
        bytes_per_line = poDS->msg_reader_core->get_hrv_bytes_per_line();
    }

    interline_spacing = poDS->msg_reader_core->get_interline_spacing();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MSGNRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    MSGNDataset *poGDS = (MSGNDataset *) poDS;

    // invert y position
    int i_nBlockYOff = poDS->GetRasterYSize() - 1 - nBlockYOff;

    char       *pszRecord;

    unsigned int data_length =  bytes_per_line + sizeof(SUB_VISIRLINE);
    unsigned int data_offset = 0;

    if (open_mode != MODE_HRV) {
        data_offset = poGDS->msg_reader_core->get_f_data_offset() +
            interline_spacing*i_nBlockYOff  + (band_in_file-1)*packet_size +
            (packet_size - data_length);
    } else {
        data_offset = poGDS->msg_reader_core->get_f_data_offset() +
            interline_spacing*(int(i_nBlockYOff/3) + 1) -
            packet_size*(3 - (i_nBlockYOff % 3)) + (packet_size - data_length);
    }

    VSIFSeek( poGDS->fp, data_offset, SEEK_SET );

    pszRecord = (char *) CPLMalloc(data_length);
    size_t nread = VSIFRead( pszRecord, 1, data_length, poGDS->fp );

    SUB_VISIRLINE* p = (SUB_VISIRLINE*) pszRecord;
    to_native(*p);

    if (p->lineValidity != 1) {
        for (int c=0; c < nBlockXSize; c++) {
            if (open_mode != MODE_RAD) {
                ((GUInt16 *)pImage)[c] = (GUInt16)MSGN_NODATA_VALUE;
            } else {
                ((double *)pImage)[c] = MSGN_NODATA_VALUE;
            }
        }
    }

    if ( nread != data_length ||
        ( open_mode != MODE_HRV && (p->lineNumberInVisirGrid - poGDS->msg_reader_core->get_line_start()) != (unsigned int)i_nBlockYOff )
       ) { // no sophisticated checking for HRV at the moment
        CPLFree( pszRecord );

        CPLError( CE_Failure, CPLE_AppDefined, "MSGN Scanline corrupt." );

        return CE_Failure;
    }

    // unpack the 10-bit values into 16-bit unsigned short ints
    unsigned char *cptr = (unsigned char*)pszRecord +
        (data_length - bytes_per_line);
    int bitsLeft = 8;
    unsigned short value = 0;

    if (open_mode != MODE_RAD) {
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
            ((GUInt16 *)pImage)[nBlockXSize-1 - c] = value;
        }
    } else {
        // radiance mode
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
            double dvalue = double(value);
            double bbvalue = MSGN_NODATA_VALUE;
            bbvalue = dvalue * poGDS->msg_reader_core->get_calibration_parameters()[orig_band_no-1].cal_slope +
                poGDS->msg_reader_core->get_calibration_parameters()[orig_band_no-1].cal_offset;

            ((double *)pImage)[nBlockXSize-1 -c] = bbvalue;
        }
    }
    CPLFree( pszRecord );
    return CE_None;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/
double MSGNRasterBand::GetMinimum( int *pbSuccess ) {
    if (pbSuccess) {
        *pbSuccess = 1;
    }
    return open_mode != MODE_RAD ? 1 : GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/
double MSGNRasterBand::GetMaximum(int *pbSuccess ) {
    if (pbSuccess) {
        *pbSuccess = 1;
    }
    return open_mode != MODE_RAD ? 1023 : GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/* ==================================================================== */
/*                             MSGNDataset                             */
/* ==================================================================== */
/************************************************************************/

MSGNDataset::MSGNDataset() {
    pszProjection = CPLStrdup("");
    msg_reader_core = 0;
}

/************************************************************************/
/*                            ~MSGNDataset()                             */
/************************************************************************/

MSGNDataset::~MSGNDataset()

{
    if( fp != NULL )
        VSIFClose( fp );

    if (msg_reader_core) {
        delete msg_reader_core;
    }

    CPLFree(pszProjection);
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
    open_mode_type open_mode = MODE_VISIR;
    GDALOpenInfo* open_info = poOpenInfo;

    if (!poOpenInfo->bStatOK) {
        if ( EQUALN(poOpenInfo->pszFilename, "HRV:", 4) ) {
            open_info = new GDALOpenInfo(&poOpenInfo->pszFilename[4], poOpenInfo->eAccess);
            open_mode = MODE_HRV;
        } else
        if ( EQUALN(poOpenInfo->pszFilename, "RAD:", 4 ) ) {
            open_info = new GDALOpenInfo(&poOpenInfo->pszFilename[4], poOpenInfo->eAccess);
            open_mode = MODE_RAD;
        }
    }

/* -------------------------------------------------------------------- */
/*      Before trying MSGNOpen() we first verify that there is at        */
/*      least one "\n#keyword" type signature in the first chunk of     */
/*      the file.                                                       */
/* -------------------------------------------------------------------- */
    if( open_info->fpL == NULL || open_info->nHeaderBytes < 50 )
        return NULL;

    /* check if this is a "NATIVE" MSG format image */
    if( !EQUALN((char *)open_info->pabyHeader,
                "FormatName                  : NATIVE", 36) )
    {
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The MSGN driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    MSGNDataset        *poDS;
    FILE* fp = VSIFOpen( open_info->pszFilename, "rb" );
    if( fp == NULL )
        return NULL;

    poDS = new MSGNDataset();

    poDS->fp = fp;

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    // first reset the file pointer, then hand over to the msg_reader_core
    VSIFSeek( poDS->fp, 0, SEEK_SET );

    poDS->msg_reader_core = new Msg_reader_core(poDS->fp);

    if (!poDS->msg_reader_core->get_open_success()) {
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = poDS->msg_reader_core->get_columns();
    poDS->nRasterYSize = poDS->msg_reader_core->get_lines();

    if (open_mode == MODE_HRV) {
        poDS->nRasterXSize *= 3;
        poDS->nRasterYSize *= 3;
    }


/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    unsigned int i;
    unsigned int band_count = 1;
    unsigned int missing_band_count = 0;
    unsigned char* bands = poDS->msg_reader_core->get_band_map();
    unsigned char band_map[MSG_NUM_CHANNELS+1];   // map GDAL band numbers to MSG channels
    for (i=0; i < MSG_NUM_CHANNELS; i++) {
        if (bands[i]) {
            bool ok_to_add = false;
            switch (open_mode) {
                case MODE_VISIR:
                    ok_to_add = i < MSG_NUM_CHANNELS - 1;
                    break;
                case MODE_RAD:
                    ok_to_add = (i <= 2) || (Msg_reader_core::Blackbody_LUT[i+1].B != 0);
                    break;
                case MODE_HRV:
                    ok_to_add = i == MSG_NUM_CHANNELS - 1;
                    break;
            }
            if (ok_to_add) {
                poDS->SetBand( band_count, new MSGNRasterBand( poDS, band_count, open_mode, i+1, i+1 - missing_band_count));
                band_map[band_count] = (unsigned char) (i+1);
                band_count++;
            }
        } else {
            missing_band_count++;
        }
    }

    double pixel_gsd_x;
    double pixel_gsd_y;
    double origin_x;
    double origin_y;

    if (open_mode != MODE_HRV) {
        pixel_gsd_x = 1000 * poDS->msg_reader_core->get_col_dir_step();  // convert from km to m
        pixel_gsd_y = 1000 * poDS->msg_reader_core->get_line_dir_step(); // convert from km to m
        origin_x = -pixel_gsd_x * (-(Conversions::nlines / 2.0) + poDS->msg_reader_core->get_col_start());
        origin_y = -pixel_gsd_y * ((Conversions::nlines / 2.0) - poDS->msg_reader_core->get_line_start());
    } else {
        pixel_gsd_x = 1000 * poDS->msg_reader_core->get_col_dir_step() / 3.0;  // convert from km to m, approximate for HRV
        pixel_gsd_y = 1000 * poDS->msg_reader_core->get_line_dir_step() / 3.0; // convert from km to m, approximate for HRV
        origin_x = -pixel_gsd_x * (-(3*Conversions::nlines / 2.0) + 3*poDS->msg_reader_core->get_col_start());
        origin_y = -pixel_gsd_y * ((3*Conversions::nlines / 2.0) - 3*poDS->msg_reader_core->get_line_start());
    }

    poDS->adfGeoTransform[0] = origin_x;
    poDS->adfGeoTransform[1] = pixel_gsd_x;
    poDS->adfGeoTransform[2] = 0.0;

    poDS->adfGeoTransform[3] = origin_y;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -pixel_gsd_y;

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

    poDS->SetMetadataItem("Radiometric parameters format", "offset slope");
    for (i=1; i < band_count; i++) {
        sprintf(tagname, "ch%02d_cal", band_map[i]);
        sprintf(field, "%.12e %.12e", cal[band_map[i]-1].cal_offset, cal[band_map[i]-1].cal_slope);
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

    sprintf(field, "%d %d",
         poDS->msg_reader_core->get_line_start(),
         poDS->msg_reader_core->get_col_start()
    );
    poDS->SetMetadataItem("Origin", field);


    if (open_info != poOpenInfo) {
        delete open_info;
    }

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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "EUMETSAT Archive native (.nat)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_msgn.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "nat" );

        poDriver->pfnOpen = MSGNDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
