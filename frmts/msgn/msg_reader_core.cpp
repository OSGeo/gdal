/******************************************************************************
 *
 * Project:  MSG Native Reader
 * Purpose:  Base class for reading in the headers of MSG native images
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

#include "cpl_port.h"
#include "cpl_error.h"

#include "msg_reader_core.h"
#include "msg_basic_types.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef DEBUG
#ifdef GDAL_SUPPORT
#undef DEBUG
#endif
#endif

#ifdef GDAL_SUPPORT
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

#else
#define VSIFSeek(fp, pos, ref)    CPL_IGNORE_RET_VAL(fseek(fp, pos, ref))
#define VSIFRead(p, bs, nb, fp)   CPL_IGNORE_RET_VAL(fread(p, bs, nb, fp))
#endif

namespace msg_native_format {

const Blackbody_lut_type Msg_reader_core::Blackbody_LUT[MSG_NUM_CHANNELS+1] = {
    {0,0,0},  // dummy channel
    {0,0,0},  // N/A
    {0,0,0},  // N/A
    {0,0,0},  // N/A
    {2569.094, 0.9959, 3.471},
    {1598.566, 0.9963, 2.219},
    {1362.142, 0.9991, 0.485},
    {1149.083, 0.9996, 0.181},
    {1034.345, 0.9999, 0.060},
    { 930.659, 0.9983, 0.627},
    { 839.661, 0.9988, 0.397},
    { 752.381, 0.9981, 0.576},
    {0,0,0}   // N/A
};

static
void PhDataInit(PH_DATA *data)
{
  data->name[0] = '\0';
  data->value[0] = '\0';
}

static
void SecondaryProdHeaderInit(SECONDARY_PROD_HEADER *header)
{
  PhDataInit(&header->abid);
  PhDataInit(&header->smod);
  PhDataInit(&header->apxs);
  PhDataInit(&header->avpa);
  PhDataInit(&header->lscd);
  PhDataInit(&header->lmap);
  PhDataInit(&header->qdlc);
  PhDataInit(&header->qdlp);
  PhDataInit(&header->qqai);
  PhDataInit(&header->selectedBandIds);
  PhDataInit(&header->southLineSelectedRectangle);
  PhDataInit(&header->northLineSelectedRectangle);
  PhDataInit(&header->eastColumnSelectedRectangle);
  PhDataInit(&header->westColumnSelectedRectangle);
}

Msg_reader_core::Msg_reader_core( const char* fname ) :
    _lines(0),
    _columns(0),
    _line_start(0),
    _col_start(0),
    _col_dir_step(0.0f),
    _line_dir_step(0.0f),
    _f_data_offset(0),
    _f_data_size(0),
    _f_header_offset(0),
    _f_header_size(0),
    _visir_bytes_per_line(0),
    _visir_packet_size(0),
    _hrv_bytes_per_line(0),
    _hrv_packet_size(0),
    _interline_spacing(0),
    _year(0),
    _month(0),
    _day(0),
    _hour(0),
    _minute(0),
    _open_success(false)
{
    memset(&_main_header, 0, sizeof(_main_header));
    memset(&_sec_header, 0, sizeof(_sec_header));
    SecondaryProdHeaderInit(&_sec_header);
    for (size_t i=0; i < MSG_NUM_CHANNELS; ++i) {
      _calibration[i].cal_slope = 0.0;
      _calibration[i].cal_offset = 0.0;
    }

    VSILFILE* fin = VSIFOpenL(fname, "rb");
    if( !fin )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Could not open file %s", fname);
        return;
    }
    read_metadata_block(fin);
    VSIFCloseL(fin);
}

Msg_reader_core::Msg_reader_core( VSILFILE* fp ) :
    _lines(0),
    _columns(0),
    _line_start(0),
    _col_start(0),
    _col_dir_step(0.0f),
    _line_dir_step(0.0f),
    _f_data_offset(0),
    _f_data_size(0),
    _f_header_offset(0),
    _f_header_size(0),
    _visir_bytes_per_line(0),
    _visir_packet_size(0),
    _hrv_bytes_per_line(0),
    _hrv_packet_size(0),
    _interline_spacing(0),
    _year(0),
    _month(0),
    _day(0),
    _hour(0),
    _minute(0),
    _open_success(false)
{
    memset(&_main_header, 0, sizeof(_main_header));
    memset(&_sec_header, 0, sizeof(_sec_header));

    SecondaryProdHeaderInit(&_sec_header);
    for (size_t i=0; i < MSG_NUM_CHANNELS; ++i) {
      _calibration[i].cal_slope = 0.0;
      _calibration[i].cal_offset = 0.0;
    }

    read_metadata_block(fp);
}

void Msg_reader_core::read_metadata_block(VSILFILE* fin) {
    _open_success = true;

    unsigned int i;

    CPL_IGNORE_RET_VAL(VSIFReadL(&_main_header, sizeof(_main_header), 1, fin));
    CPL_IGNORE_RET_VAL(VSIFReadL(&_sec_header, sizeof(_sec_header), 1, fin));

#ifdef DEBUG
    // print out all the fields in the header
    PH_DATA* hd = (PH_DATA*)&_main_header;
    for (int i=0; i < 6; i++) {
        to_string(*hd);
        printf("[%02d] %s %s", i, hd->name, hd->value);/*ok*/
        hd++;
    }
    PH_DATA_ID* hdi = (PH_DATA_ID*)&_main_header.dataSetIdentification;

    for (i=0; i < 5; i++) {
        printf("%s %s %s", hdi->name, hdi->size, hdi->address);/*ok*/
        hdi++;
    }
    hd = (PH_DATA*)(&_main_header.totalFileSize);
    for (int i=0; i < 19; i++) {
        to_string(*hd);
        printf("[%02d] %s %s", i, hd->name, hd->value);/*ok*/
        hd++;
    }
#endif // DEBUG

    // extract data & header positions

    for (i=0; i < 5; i++) {
        PH_DATA_ID* hdi = (PH_DATA_ID*)&_main_header.dataSetIdentification[i];
        if (STARTS_WITH(hdi->name, "15Header")) {
            sscanf(hdi->size, "%u", &_f_header_size);
            sscanf(hdi->address, "%u", &_f_header_offset);
        } else
            if (STARTS_WITH(hdi->name, "15Data")) {
            sscanf(hdi->size, "%u", &_f_data_size);
            sscanf(hdi->address, "%u", &_f_data_offset);
        }
    }
#ifdef DEBUG
    printf("Data: %u %u\n", _f_data_offset, _f_data_size);/*ok*/
    printf("Header: %u %u\n", _f_header_offset, _f_header_size);/*ok*/
#endif // DEBUG

    unsigned int lines;
    sscanf(_sec_header.northLineSelectedRectangle.value, "%u", &_lines);
    sscanf(_sec_header.southLineSelectedRectangle.value, "%u", &lines);
    _line_start = lines;
    if( lines > 0 && _lines >= lines - 1 )
        _lines -= lines - 1;

    unsigned int cols;
    sscanf(_sec_header.westColumnSelectedRectangle.value, "%u", &_columns);
    sscanf(_sec_header.eastColumnSelectedRectangle.value, "%u", &cols);
    _col_start = cols;
    if( cols > 0 && _columns >= cols - 1 )
        _columns -= cols - 1;

#ifdef DEBUG
    printf("lines = %u, cols = %u\n", _lines, _columns);/*ok*/
    int records_per_line = 0;
#endif // DEBUG

    for (i=0; i < MSG_NUM_CHANNELS; i++) {
        if (_sec_header.selectedBandIds.value[i] == 'X') {
            _bands[i] = 1;
#ifdef DEBUG
            records_per_line += (i == (MSG_NUM_CHANNELS-1)) ? 3 : 1;
#endif // DEBUG
        } else {
            _bands[i] = 0;
        }
    }

#ifdef DEBUG
    printf("reading a total of %d records per line\n", records_per_line);/*ok*/
#endif // DEBUG

    // extract time fields, assume that SNIT is the correct field:
    sscanf(_main_header.snit.value +  0, "%04u", &_year);
    sscanf(_main_header.snit.value +  4, "%02u", &_month);
    sscanf(_main_header.snit.value +  6, "%02u", &_day);
    sscanf(_main_header.snit.value +  8, "%02u", &_hour);
    sscanf(_main_header.snit.value + 10, "%02u", &_minute);

    // read radiometric block
    RADIOMETRIC_PROCESSING_RECORD rad;
    off_t offset = RADIOMETRICPROCESSING_RECORD_OFFSET + _f_header_offset + sizeof(GP_PK_HEADER) + sizeof(GP_PK_SH1) + 1;
    CPL_IGNORE_RET_VAL(VSIFSeekL(fin, offset, SEEK_SET));
    CPL_IGNORE_RET_VAL(VSIFReadL(&rad, sizeof(RADIOMETRIC_PROCESSING_RECORD), 1, fin));
    to_native(rad);
    memcpy((void*)_calibration, (void*)&rad.level1_5ImageCalibration,sizeof(_calibration));

#ifdef DEBUG
    for (unsigned int i=0; i < MSG_NUM_CHANNELS; i++) {
        if (_calibration[i].cal_slope < 0 || _calibration[i].cal_slope > 0.4)
        {
            printf("Warning: calibration slope (%f) out of nominal range. MSG reader probably broken\n", _calibration[i].cal_slope);/*ok*/
        }
        if (_calibration[i].cal_offset > 0 || _calibration[i].cal_offset < -20) {
            printf("Warning: calibration offset (%f) out of nominal range. MSG reader probably broken\n",/*ok*/ _calibration[i].cal_offset);
        }
    }
#endif

    // read image description block
    IMAGE_DESCRIPTION_RECORD idr;
    offset = RADIOMETRICPROCESSING_RECORD_OFFSET  - IMAGEDESCRIPTION_RECORD_LENGTH + _f_header_offset + sizeof(GP_PK_HEADER) + sizeof(GP_PK_SH1) + 1;
    CPL_IGNORE_RET_VAL(VSIFSeekL(fin, offset, SEEK_SET));
    CPL_IGNORE_RET_VAL(VSIFReadL(&idr, sizeof(IMAGE_DESCRIPTION_RECORD), 1, fin));
    to_native(idr);
    _line_dir_step = idr.referencegrid_visir.lineDirGridStep;
    _col_dir_step = idr.referencegrid_visir.columnDirGridStep;

    // Rather convoluted, but this code is required to compute the real data block sizes
    // It does this by reading in the first line of every band, to get to the packet size field
    GP_PK_HEADER gp_header;
    GP_PK_SH1    sub_header;
    SUB_VISIRLINE visir_line;

    CPL_IGNORE_RET_VAL(VSIFSeekL(fin, _f_data_offset, SEEK_SET));

    _hrv_packet_size = 0;
    _interline_spacing = 0;
    visir_line.channelId = 0;

    int scanned_bands[MSG_NUM_CHANNELS];
    int band_count = 0;
    for (i=0; i < MSG_NUM_CHANNELS; i++) {
        scanned_bands[i] = _bands[i];
        band_count += _bands[i];
    }

    do {
        if( VSIFReadL(&gp_header, sizeof(GP_PK_HEADER), 1, fin) != 1 ||
            VSIFReadL(&sub_header, sizeof(GP_PK_SH1), 1, fin) != 1 ||
            VSIFReadL(&visir_line, sizeof(SUB_VISIRLINE), 1, fin) != 1 )
        {
            _open_success = false;
            break;
        }
        to_native(visir_line);
        to_native(gp_header);

        // skip over the actual line data
        CPL_IGNORE_RET_VAL(VSIFSeekL(fin,
            gp_header.packetLength - (sizeof(GP_PK_SH1) + sizeof(SUB_VISIRLINE) - 1),
            SEEK_CUR
        ));

        if (visir_line.channelId == 0 || visir_line.channelId > MSG_NUM_CHANNELS) {
            _open_success = false;
            break;
        }

        if (scanned_bands[visir_line.channelId - 1]) {
            scanned_bands[visir_line.channelId - 1] = 0;
            band_count--;

            if (visir_line.channelId != 12) { // not the HRV channel
                _visir_bytes_per_line = gp_header.packetLength - (unsigned int)(sizeof(GP_PK_SH1) + sizeof(SUB_VISIRLINE) - 1);
                _visir_packet_size = gp_header.packetLength + (unsigned int)sizeof(GP_PK_HEADER) + 1;
                _interline_spacing += _visir_packet_size;
            } else {
                _hrv_bytes_per_line = gp_header.packetLength - (unsigned int)(sizeof(GP_PK_SH1) + sizeof(SUB_VISIRLINE) - 1);
                _hrv_packet_size = gp_header.packetLength + (unsigned int)sizeof(GP_PK_HEADER) + 1;
                _interline_spacing +=  3*_hrv_packet_size;
                CPL_IGNORE_RET_VAL(VSIFSeekL(fin, 2*gp_header.packetLength, SEEK_CUR ));
            }
        }
    } while (band_count > 0);
}

#ifndef GDAL_SUPPORT

int Msg_reader_core::_chan_to_idx(Msg_channel_names channel) {
    unsigned int idx = 0;
    while (idx < MSG_NUM_CHANNELS) {
        if ( (1 << (idx+1)) == (int)channel ) {
            return idx;
        }
        idx++;
    }
    return 0;
}

void Msg_reader_core::get_pixel_geo_coordinates(unsigned int line, unsigned int column, double& longitude, double& latitude) const {
    Conversions::convert_pixel_to_geo((unsigned int)(line + _line_start), (unsigned int)(column + _col_start), longitude, latitude);
}

void Msg_reader_core::get_pixel_geo_coordinates(double line, double column, double& longitude, double& latitude) {
    Conversions::convert_pixel_to_geo(line + _line_start, column + _col_start, longitude, latitude);
}

double Msg_reader_core::compute_pixel_area_sqkm(double line, double column) {
    return Conversions::compute_pixel_area_sqkm(line + _line_start, column + _col_start);
}

#endif // GDAL_SUPPORT

} // namespace msg_native_format
