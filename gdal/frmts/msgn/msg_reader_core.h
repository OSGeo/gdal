/******************************************************************************
 * $Id$
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

#ifndef MSG_READER_CORE_H
#define MSG_READER_CORE_H

#include "msg_basic_types.h"
#include <stdio.h>

namespace msg_native_format {

const unsigned int MSG_NUM_CHANNELS = 12;

typedef struct {
    double vc;
    double A;
    double B;
} Blackbody_lut_type;

typedef enum {
    VIS0_6  = 2,
    VIS0_8  = 4,
    NIR1_6  = 8,
    IR3_9   = 16,
    IR6_2   = 32,
    IR7_3   = 64,
    IR8_7   = 128,
    IR9_7   = 256,
    IR10_8  = 512,
    IR12_0  = 1024,
    IR13_4  = 2048,
    HRV     = 4096
} Msg_channel_names;

class Msg_reader_core {
public:
    Msg_reader_core(const char* fname);
    Msg_reader_core(FILE* fp);
    virtual ~Msg_reader_core(void) {};

    bool get_open_success(void) { return _open_success; }

    #ifndef GDAL_SUPPORT
    virtual void radiance_to_blackbody(int using_chan_no = 0) = 0;   // can override which channel's parameters to use
    virtual double* get_data(int chan_no=0) = 0;
    #endif

    unsigned int get_lines(void) { return _lines; }
    unsigned int get_columns(void) { return _columns; }

    void get_pixel_geo_coordinates(unsigned int line, unsigned int column, double& longitude, double& latitude); // x and y relative to this image, not full disc image
    void get_pixel_geo_coordinates(double line, double column, double& longitude, double& latitude); // x and y relative to this image, not full disc image
    double compute_pixel_area_sqkm(double line, double column);

    static const Blackbody_lut_type Blackbody_LUT[MSG_NUM_CHANNELS+1];

    unsigned int get_year(void) { return _year; }
    unsigned int get_month(void) { return _month; }
    unsigned int get_day(void) { return _day; }
    unsigned int get_hour(void) { return _hour; }
    unsigned int get_minute(void) { return _minute; }

    unsigned int get_line_start(void) { return _line_start; }
    unsigned int get_col_start(void) { return _col_start; }

    float get_col_dir_step(void) { return _col_dir_step; }
    float get_line_dir_step(void) { return _line_dir_step; }

    unsigned int get_f_data_offset(void) { return _f_data_offset; }
    unsigned int get_visir_bytes_per_line(void) { return _visir_bytes_per_line; }
    unsigned int get_visir_packet_size(void) { return _visir_packet_size; }
    unsigned int get_hrv_bytes_per_line(void) { return _hrv_bytes_per_line; }
    unsigned int get_hrv_packet_size(void) { return _hrv_packet_size; }
    unsigned int get_interline_spacing(void) { return _interline_spacing; }

    unsigned char* get_band_map(void) { return _bands; }

    CALIBRATION*  get_calibration_parameters(void) { return _calibration; }

private:
    void read_metadata_block(FILE* fp);

protected:

    int _chan_to_idx(Msg_channel_names channel);

    unsigned int    _lines;
    unsigned int    _columns;

    unsigned int    _line_start;
    unsigned int    _col_start;

    float           _col_dir_step;
    float           _line_dir_step;

    MAIN_PROD_HEADER        _main_header;
    SECONDARY_PROD_HEADER   _sec_header;
    CALIBRATION             _calibration[MSG_NUM_CHANNELS];

    unsigned int _f_data_offset;
    unsigned int _f_data_size;
    unsigned int _f_header_offset;
    unsigned int _f_header_size;

    unsigned int _visir_bytes_per_line;   // packed length of a VISIR line, without headers
    unsigned int _visir_packet_size;      // effectively, the spacing between lines of consecutive bands in bytes
    unsigned int _hrv_bytes_per_line;
    unsigned int _hrv_packet_size;
    unsigned int _interline_spacing;

    unsigned char _bands[MSG_NUM_CHANNELS];

    unsigned int _year;
    unsigned int _month;
    unsigned int _day;
    unsigned int _hour;
    unsigned int _minute;

    bool    _open_success;
};

}// namespace msg_native_format

#endif

