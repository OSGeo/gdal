/******************************************************************************
 * $Id$
 *
 * Project:  APP ENVISAT Support
 * Purpose:  Low Level Envisat file access (read/write) API.
 * Author:   Antonio Valentino <antonio.valentino@tiscali.it>
 *
 ******************************************************************************
 * Copyright (c) 2011, Antonio Valentino
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

#include "cpl_string.h"
#include "records.h"

CPL_CVSID("$Id$")

/* --- ASAR record descriptors --------------------------------------------- */
static const EnvisatFieldDescr ASAR_ANTENNA_ELEV_PATT_ADSR[] = {
    {"ZERO_DOPPLER_TIME",                                  0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"BEAM_ID",                                           13, EDT_Char,        3},
    {"ELEVATION_PATTERN.SLANT_RANGE_TIME",                16, EDT_Float32,    11},
    {"ELEVATION_PATTERN.ELEVATION_ANGLES",                60, EDT_Float32,    11},
    {"ELEVATION_PATTERN.ANTENNA_PATTERN",                104, EDT_Float32,    11},
    /*{"SPARE_1",                                        148, EDT_UByte,      14},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr ASAR_CHIRP_PARAMS_ADSR[] = {
    {"ZERO_DOPPLER_TIME",                                  0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"BEAM_ID",                                           13, EDT_Char,        3},
    {"POLAR",                                             16, EDT_Char,        3},
    {"CHIRP_WIDTH",                                       19, EDT_Float32,     1},
    {"CHIRP_SIDELOBE",                                    23, EDT_Float32,     1},
    {"CHIRP_ISLR",                                        27, EDT_Float32,     1},
    {"CHIRP_PEAK_LOC",                                    31, EDT_Float32,     1},
    {"CHIRP_POWER",                                       35, EDT_Float32,     1},
    {"ELEV_CORR_FACTOR",                                  39, EDT_Float32,     1},
    /*{"SPARE_1",                                         43, EDT_UByte,      16},*/
    {"CAL_PULSE_INFO.1.MAX_CAL",                          59, EDT_Float32,     3},
    {"CAL_PULSE_INFO.1.AVG_CAL",                          71, EDT_Float32,     3},
    {"CAL_PULSE_INFO.1.AVG_VAL_1A",                       83, EDT_Float32,     1},
    {"CAL_PULSE_INFO.1.PHS_CAL",                          87, EDT_Float32,     4},
    {"CAL_PULSE_INFO.2.MAX_CAL",                         103, EDT_Float32,     3},
    {"CAL_PULSE_INFO.2.AVG_CAL",                         115, EDT_Float32,     3},
    {"CAL_PULSE_INFO.2.AVG_VAL_1A",                      127, EDT_Float32,     1},
    {"CAL_PULSE_INFO.2.PHS_CAL",                         131, EDT_Float32,     4},
    {"CAL_PULSE_INFO.3.MAX_CAL",                         147, EDT_Float32,     3},
    {"CAL_PULSE_INFO.3.AVG_CAL",                         159, EDT_Float32,     3},
    {"CAL_PULSE_INFO.3.AVG_VAL_1A",                      171, EDT_Float32,     1},
    {"CAL_PULSE_INFO.3.PHS_CAL",                         175, EDT_Float32,     4},
    {"CAL_PULSE_INFO.4.MAX_CAL",                         191, EDT_Float32,     3},
    {"CAL_PULSE_INFO.4.AVG_CAL",                         203, EDT_Float32,     3},
    {"CAL_PULSE_INFO.4.AVG_VAL_1A",                      215, EDT_Float32,     1},
    {"CAL_PULSE_INFO.4.PHS_CAL",                         219, EDT_Float32,     4},
    {"CAL_PULSE_INFO.5.MAX_CAL",                         235, EDT_Float32,     3},
    {"CAL_PULSE_INFO.5.AVG_CAL",                         247, EDT_Float32,     3},
    {"CAL_PULSE_INFO.5.AVG_VAL_1A",                      259, EDT_Float32,     1},
    {"CAL_PULSE_INFO.5.PHS_CAL",                         263, EDT_Float32,     4},
    {"CAL_PULSE_INFO.6.MAX_CAL",                         279, EDT_Float32,     3},
    {"CAL_PULSE_INFO.6.AVG_CAL",                         291, EDT_Float32,     3},
    {"CAL_PULSE_INFO.6.AVG_VAL_1A",                      303, EDT_Float32,     1},
    {"CAL_PULSE_INFO.6.PHS_CAL",                         307, EDT_Float32,     4},
    {"CAL_PULSE_INFO.7.MAX_CAL",                         323, EDT_Float32,     3},
    {"CAL_PULSE_INFO.7.AVG_CAL",                         335, EDT_Float32,     3},
    {"CAL_PULSE_INFO.7.AVG_VAL_1A",                      347, EDT_Float32,     1},
    {"CAL_PULSE_INFO.7.PHS_CAL",                         351, EDT_Float32,     4},
    {"CAL_PULSE_INFO.8.MAX_CAL",                         367, EDT_Float32,     3},
    {"CAL_PULSE_INFO.8.AVG_CAL",                         379, EDT_Float32,     3},
    {"CAL_PULSE_INFO.8.AVG_VAL_1A",                      391, EDT_Float32,     1},
    {"CAL_PULSE_INFO.8.PHS_CAL",                         395, EDT_Float32,     4},
    {"CAL_PULSE_INFO.9.MAX_CAL",                         411, EDT_Float32,     3},
    {"CAL_PULSE_INFO.9.AVG_CAL",                         423, EDT_Float32,     3},
    {"CAL_PULSE_INFO.9.AVG_VAL_1A",                      435, EDT_Float32,     1},
    {"CAL_PULSE_INFO.9.PHS_CAL",                         439, EDT_Float32,     4},
    {"CAL_PULSE_INFO.10.MAX_CAL",                        455, EDT_Float32,     3},
    {"CAL_PULSE_INFO.10.AVG_CAL",                        467, EDT_Float32,     3},
    {"CAL_PULSE_INFO.10.AVG_VAL_1A",                     479, EDT_Float32,     1},
    {"CAL_PULSE_INFO.10.PHS_CAL",                        483, EDT_Float32,     4},
    {"CAL_PULSE_INFO.11.MAX_CAL",                        499, EDT_Float32,     3},
    {"CAL_PULSE_INFO.11.AVG_CAL",                        511, EDT_Float32,     3},
    {"CAL_PULSE_INFO.11.AVG_VAL_1A",                     523, EDT_Float32,     1},
    {"CAL_PULSE_INFO.11.PHS_CAL",                        527, EDT_Float32,     4},
    {"CAL_PULSE_INFO.12.MAX_CAL",                        543, EDT_Float32,     3},
    {"CAL_PULSE_INFO.12.AVG_CAL",                        555, EDT_Float32,     3},
    {"CAL_PULSE_INFO.12.AVG_VAL_1A",                     567, EDT_Float32,     1},
    {"CAL_PULSE_INFO.12.PHS_CAL",                        571, EDT_Float32,     4},
    {"CAL_PULSE_INFO.13.MAX_CAL",                        587, EDT_Float32,     3},
    {"CAL_PULSE_INFO.13.AVG_CAL",                        599, EDT_Float32,     3},
    {"CAL_PULSE_INFO.13.AVG_VAL_1A",                     611, EDT_Float32,     1},
    {"CAL_PULSE_INFO.13.PHS_CAL",                        615, EDT_Float32,     4},
    {"CAL_PULSE_INFO.14.MAX_CAL",                        631, EDT_Float32,     3},
    {"CAL_PULSE_INFO.14.AVG_CAL",                        643, EDT_Float32,     3},
    {"CAL_PULSE_INFO.14.AVG_VAL_1A",                     655, EDT_Float32,     1},
    {"CAL_PULSE_INFO.14.PHS_CAL",                        659, EDT_Float32,     4},
    {"CAL_PULSE_INFO.15.MAX_CAL",                        675, EDT_Float32,     3},
    {"CAL_PULSE_INFO.15.AVG_CAL",                        687, EDT_Float32,     3},
    {"CAL_PULSE_INFO.15.AVG_VAL_1A",                     699, EDT_Float32,     1},
    {"CAL_PULSE_INFO.15.PHS_CAL",                        703, EDT_Float32,     4},
    {"CAL_PULSE_INFO.16.MAX_CAL",                        719, EDT_Float32,     3},
    {"CAL_PULSE_INFO.16.AVG_CAL",                        731, EDT_Float32,     3},
    {"CAL_PULSE_INFO.16.AVG_VAL_1A",                     743, EDT_Float32,     1},
    {"CAL_PULSE_INFO.16.PHS_CAL",                        747, EDT_Float32,     4},
    {"CAL_PULSE_INFO.17.MAX_CAL",                        763, EDT_Float32,     3},
    {"CAL_PULSE_INFO.17.AVG_CAL",                        775, EDT_Float32,     3},
    {"CAL_PULSE_INFO.17.AVG_VAL_1A",                     787, EDT_Float32,     1},
    {"CAL_PULSE_INFO.17.PHS_CAL",                        791, EDT_Float32,     4},
    {"CAL_PULSE_INFO.18.MAX_CAL",                        807, EDT_Float32,     3},
    {"CAL_PULSE_INFO.18.AVG_CAL",                        819, EDT_Float32,     3},
    {"CAL_PULSE_INFO.18.AVG_VAL_1A",                     831, EDT_Float32,     1},
    {"CAL_PULSE_INFO.18.PHS_CAL",                        835, EDT_Float32,     4},
    {"CAL_PULSE_INFO.19.MAX_CAL",                        851, EDT_Float32,     3},
    {"CAL_PULSE_INFO.19.AVG_CAL",                        863, EDT_Float32,     3},
    {"CAL_PULSE_INFO.19.AVG_VAL_1A",                     875, EDT_Float32,     1},
    {"CAL_PULSE_INFO.19.PHS_CAL",                        879, EDT_Float32,     4},
    {"CAL_PULSE_INFO.20.MAX_CAL",                        895, EDT_Float32,     3},
    {"CAL_PULSE_INFO.20.AVG_CAL",                        907, EDT_Float32,     3},
    {"CAL_PULSE_INFO.20.AVG_VAL_1A",                     919, EDT_Float32,     1},
    {"CAL_PULSE_INFO.20.PHS_CAL",                        923, EDT_Float32,     4},
    {"CAL_PULSE_INFO.21.MAX_CAL",                        939, EDT_Float32,     3},
    {"CAL_PULSE_INFO.21.AVG_CAL",                        951, EDT_Float32,     3},
    {"CAL_PULSE_INFO.21.AVG_VAL_1A",                     963, EDT_Float32,     1},
    {"CAL_PULSE_INFO.21.PHS_CAL",                        967, EDT_Float32,     4},
    {"CAL_PULSE_INFO.22.MAX_CAL",                        983, EDT_Float32,     3},
    {"CAL_PULSE_INFO.22.AVG_CAL",                        995, EDT_Float32,     3},
    {"CAL_PULSE_INFO.22.AVG_VAL_1A",                    1007, EDT_Float32,     1},
    {"CAL_PULSE_INFO.22.PHS_CAL",                       1011, EDT_Float32,     4},
    {"CAL_PULSE_INFO.23.MAX_CAL",                       1027, EDT_Float32,     3},
    {"CAL_PULSE_INFO.23.AVG_CAL",                       1039, EDT_Float32,     3},
    {"CAL_PULSE_INFO.23.AVG_VAL_1A",                    1051, EDT_Float32,     1},
    {"CAL_PULSE_INFO.23.PHS_CAL",                       1055, EDT_Float32,     4},
    {"CAL_PULSE_INFO.24.MAX_CAL",                       1071, EDT_Float32,     3},
    {"CAL_PULSE_INFO.24.AVG_CAL",                       1083, EDT_Float32,     3},
    {"CAL_PULSE_INFO.24.AVG_VAL_1A",                    1095, EDT_Float32,     1},
    {"CAL_PULSE_INFO.24.PHS_CAL",                       1099, EDT_Float32,     4},
    {"CAL_PULSE_INFO.25.MAX_CAL",                       1115, EDT_Float32,     3},
    {"CAL_PULSE_INFO.25.AVG_CAL",                       1127, EDT_Float32,     3},
    {"CAL_PULSE_INFO.25.AVG_VAL_1A",                    1139, EDT_Float32,     1},
    {"CAL_PULSE_INFO.25.PHS_CAL",                       1143, EDT_Float32,     4},
    {"CAL_PULSE_INFO.26.MAX_CAL",                       1159, EDT_Float32,     3},
    {"CAL_PULSE_INFO.26.AVG_CAL",                       1171, EDT_Float32,     3},
    {"CAL_PULSE_INFO.26.AVG_VAL_1A",                    1183, EDT_Float32,     1},
    {"CAL_PULSE_INFO.26.PHS_CAL",                       1187, EDT_Float32,     4},
    {"CAL_PULSE_INFO.27.MAX_CAL",                       1203, EDT_Float32,     3},
    {"CAL_PULSE_INFO.27.AVG_CAL",                       1215, EDT_Float32,     3},
    {"CAL_PULSE_INFO.27.AVG_VAL_1A",                    1227, EDT_Float32,     1},
    {"CAL_PULSE_INFO.27.PHS_CAL",                       1231, EDT_Float32,     4},
    {"CAL_PULSE_INFO.28.MAX_CAL",                       1247, EDT_Float32,     3},
    {"CAL_PULSE_INFO.28.AVG_CAL",                       1259, EDT_Float32,     3},
    {"CAL_PULSE_INFO.28.AVG_VAL_1A",                    1271, EDT_Float32,     1},
    {"CAL_PULSE_INFO.28.PHS_CAL",                       1275, EDT_Float32,     4},
    {"CAL_PULSE_INFO.29.MAX_CAL",                       1291, EDT_Float32,     3},
    {"CAL_PULSE_INFO.29.AVG_CAL",                       1303, EDT_Float32,     3},
    {"CAL_PULSE_INFO.29.AVG_VAL_1A",                    1315, EDT_Float32,     1},
    {"CAL_PULSE_INFO.29.PHS_CAL",                       1319, EDT_Float32,     4},
    {"CAL_PULSE_INFO.30.MAX_CAL",                       1335, EDT_Float32,     3},
    {"CAL_PULSE_INFO.30.AVG_CAL",                       1347, EDT_Float32,     3},
    {"CAL_PULSE_INFO.30.AVG_VAL_1A",                    1359, EDT_Float32,     1},
    {"CAL_PULSE_INFO.30.PHS_CAL",                       1363, EDT_Float32,     4},
    {"CAL_PULSE_INFO.31.MAX_CAL",                       1379, EDT_Float32,     3},
    {"CAL_PULSE_INFO.31.AVG_CAL",                       1391, EDT_Float32,     3},
    {"CAL_PULSE_INFO.31.AVG_VAL_1A",                    1403, EDT_Float32,     1},
    {"CAL_PULSE_INFO.31.PHS_CAL",                       1407, EDT_Float32,     4},
    {"CAL_PULSE_INFO.32.MAX_CAL",                       1423, EDT_Float32,     3},
    {"CAL_PULSE_INFO.32.AVG_CAL",                       1435, EDT_Float32,     3},
    {"CAL_PULSE_INFO.32.AVG_VAL_1A",                    1447, EDT_Float32,     1},
    {"CAL_PULSE_INFO.32.PHS_CAL",                       1451, EDT_Float32,     4},
    /*{"SPARE_2",                                       1467, EDT_UByte,      16},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr ASAR_DOP_CENTROID_COEFFS_ADSR[] = {
    {"ZERO_DOPPLER_TIME",                                  0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"SLANT_RANGE_TIME",                                  13, EDT_Float32,     1},
    {"DOP_COEF",                                          17, EDT_Float32,     5},
    {"DOP_CONF",                                          37, EDT_Float32,     1},
    {"DOP_CONF_BELOW_THRESH_FLAG",                        41, EDT_UByte,       1},
    {"DELTA_DOPP_COEFF",                                  42, EDT_Int16,       5},
    /*{"SPARE_1",                                         52, EDT_UByte,       3},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

#if 0  /* Unused */
static const EnvisatFieldDescr ASAR_GEOLOCATION_GRID_ADSR[] = {
    {"FIRST_ZERO_DOPPLER_TIME",                            0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"LINE_NUM",                                          13, EDT_UInt32,      1},
    {"NUM_LINES",                                         17, EDT_UInt32,      1},
    {"SUB_SAT_TRACK",                                     21, EDT_Float32,     1},
    {"FIRST_LINE_TIE_POINTS.SAMP_NUMBERS",                25, EDT_UInt32,     11},
    {"FIRST_LINE_TIE_POINTS.SLANT_RANGE_TIMES",           69, EDT_Float32,    11},
    {"FIRST_LINE_TIE_POINTS.ANGLES",                     113, EDT_Float32,    11},
    {"FIRST_LINE_TIE_POINTS.LATS",                       157, EDT_Int32,      11},
    {"FIRST_LINE_TIE_POINTS.LONGS",                      201, EDT_Int32,      11},
    /*{"SPARE_1",                                        245, EDT_UByte,      22},*/
    {"LAST_ZERO_DOPPLER_TIME",                           267, EDT_MJD,         1},
    {"LAST_LINE_TIE_POINTS.SAMP_NUMBERS",                279, EDT_UInt32,     11},
    {"LAST_LINE_TIE_POINTS.SLANT_RANGE_TIMES",           323, EDT_Float32,    11},
    {"LAST_LINE_TIE_POINTS.ANGLES",                      367, EDT_Float32,    11},
    {"LAST_LINE_TIE_POINTS.LATS",                        411, EDT_Int32,      11},
    {"LAST_LINE_TIE_POINTS.LONGS",                       455, EDT_Int32,      11},
    /*{"SPARE_2",                                        499, EDT_UByte,      22},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};
#endif  /* Unused */

static const EnvisatFieldDescr ASAR_MAIN_PROCESSING_PARAMS_ADSR[] = {
    {"FIRST_ZERO_DOPPLER_TIME",                            0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"LAST_ZERO_DOPPLER_TIME",                            13, EDT_MJD,         1},
    {"WORK_ORDER_ID",                                     25, EDT_Char,       12},
    {"TIME_DIFF",                                         37, EDT_Float32,     1},
    {"SWATH_ID",                                          41, EDT_Char,        3},
    {"RANGE_SPACING",                                     44, EDT_Float32,     1},
    {"AZIMUTH_SPACING",                                   48, EDT_Float32,     1},
    {"LINE_TIME_INTERVAL",                                52, EDT_Float32,     1},
    {"NUM_OUTPUT_LINES",                                  56, EDT_UInt32,      1},
    {"NUM_SAMPLES_PER_LINE",                              60, EDT_UInt32,      1},
    {"DATA_TYPE",                                         64, EDT_Char,        5},
    /*{"SPARE_1",                                         69, EDT_UByte,      51},*/
    {"DATA_ANALYSIS_FLAG",                               120, EDT_UByte,       1},
    {"ANT_ELEV_CORR_FLAG",                               121, EDT_UByte,       1},
    {"CHIRP_EXTRACT_FLAG",                               122, EDT_UByte,       1},
    {"SRGR_FLAG",                                        123, EDT_UByte,       1},
    {"DOP_CEN_FLAG",                                     124, EDT_UByte,       1},
    {"DOP_AMB_FLAG",                                     125, EDT_UByte,       1},
    {"RANGE_SPREAD_COMP_FLAG",                           126, EDT_UByte,       1},
    {"DETECTED_FLAG",                                    127, EDT_UByte,       1},
    {"LOOK_SUM_FLAG",                                    128, EDT_UByte,       1},
    {"RMS_EQUAL_FLAG",                                   129, EDT_UByte,       1},
    {"ANT_SCAL_FLAG",                                    130, EDT_UByte,       1},
    {"VGA_COM_ECHO_FLAG",                                131, EDT_UByte,       1},
    {"VGA_COM_PULSE_2_FLAG",                             132, EDT_UByte,       1},
    {"VGA_COM_PULSE_ZERO_FLAG",                          133, EDT_UByte,       1},
    {"INV_FILT_COMP_FLAG",                               134, EDT_UByte,       1},
    /*{"SPARE_2",                                        135, EDT_UByte,       6},*/
    {"RAW_DATA_ANALYSIS.1.NUM_GAPS",                     141, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.1.NUM_MISSING_LINES",            145, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.1.RANGE_SAMP_SKIP",              149, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.1.RANGE_LINES_SKIP",             153, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.1.CALC_I_BIAS",                  157, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_Q_BIAS",                  161, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_I_STD_DEV",               165, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_Q_STD_DEV",               169, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_GAIN",                    173, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_QUAD",                    177, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.I_BIAS_MAX",                   181, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.I_BIAS_MIN",                   185, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.Q_BIAS_MAX",                   189, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.Q_BIAS_MIN",                   193, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.GAIN_MIN",                     197, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.GAIN_MAX",                     201, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.QUAD_MIN",                     205, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.QUAD_MAX",                     209, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.I_BIAS_FLAG",                  213, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.1.Q_BIAS_FLAG",                  214, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.1.GAIN_FLAG",                    215, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.1.QUAD_FLAG",                    216, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.1.USED_I_BIAS",                  217, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.USED_Q_BIAS",                  221, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.USED_GAIN",                    225, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.USED_QUAD",                    229, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.NUM_GAPS",                     233, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.2.NUM_MISSING_LINES",            237, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.2.RANGE_SAMP_SKIP",              241, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.2.RANGE_LINES_SKIP",             245, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.2.CALC_I_BIAS",                  249, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_Q_BIAS",                  253, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_I_STD_DEV",               257, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_Q_STD_DEV",               261, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_GAIN",                    265, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_QUAD",                    269, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.I_BIAS_MAX",                   273, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.I_BIAS_MIN",                   277, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.Q_BIAS_MAX",                   281, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.Q_BIAS_MIN",                   285, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.GAIN_MIN",                     289, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.GAIN_MAX",                     293, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.QUAD_MIN",                     297, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.QUAD_MAX",                     301, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.I_BIAS_FLAG",                  305, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.2.Q_BIAS_FLAG",                  306, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.2.GAIN_FLAG",                    307, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.2.QUAD_FLAG",                    308, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.2.USED_I_BIAS",                  309, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.USED_Q_BIAS",                  313, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.USED_GAIN",                    317, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.USED_QUAD",                    321, EDT_Float32,     1},
    /*{"SPARE_3",                                        325, EDT_UByte,      32},*/
    {"START_TIME.1.FIRST_OBT",                           357, EDT_UInt32,      2},
    {"START_TIME.1.FIRST_MJD",                           365, EDT_MJD,         1},
    {"START_TIME.2.FIRST_OBT",                           377, EDT_UInt32,      2},
    {"START_TIME.2.FIRST_MJD",                           385, EDT_MJD,         1},
    {"PARAMETER_CODES.FIRST_SWST_CODE",                  397, EDT_UInt16,      5},
    {"PARAMETER_CODES.LAST_SWST_CODE",                   407, EDT_UInt16,      5},
    {"PARAMETER_CODES.PRI_CODE",                         417, EDT_UInt16,      5},
    {"PARAMETER_CODES.TX_PULSE_LEN_CODE",                427, EDT_UInt16,      5},
    {"PARAMETER_CODES.TX_BW_CODE",                       437, EDT_UInt16,      5},
    {"PARAMETER_CODES.ECHO_WIN_LEN_CODE",                447, EDT_UInt16,      5},
    {"PARAMETER_CODES.UP_CODE",                          457, EDT_UInt16,      5},
    {"PARAMETER_CODES.DOWN_CODE",                        467, EDT_UInt16,      5},
    {"PARAMETER_CODES.RESAMP_CODE",                      477, EDT_UInt16,      5},
    {"PARAMETER_CODES.BEAM_ADJ_CODE",                    487, EDT_UInt16,      5},
    {"PARAMETER_CODES.BEAM_SET_NUM_CODE",                497, EDT_UInt16,      5},
    {"PARAMETER_CODES.TX_MONITOR_CODE",                  507, EDT_UInt16,      5},
    /*{"SPARE_4",                                        517, EDT_UByte,      60},*/
    {"ERROR_COUNTERS.NUM_ERR_SWST",                      577, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_PRI",                       581, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_TX_PULSE_LEN",              585, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_TX_PULSE_BW",               589, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_ECHO_WIN_LEN",              593, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_UP",                        597, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_DOWN",                      601, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_RESAMP",                    605, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_BEAM_ADJ",                  609, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_BEAM_SET_NUM",              613, EDT_UInt32,      1},
    /*{"SPARE_5",                                        617, EDT_UByte,      26},*/
    {"IMAGE_PARAMETERS.FIRST_SWST_VALUE",                643, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.LAST_SWST_VALUE",                 663, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.SWST_CHANGES",                    683, EDT_UInt32,      5},
    {"IMAGE_PARAMETERS.PRF_VALUE",                       703, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.TX_PULSE_LEN_VALUE",              723, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.TX_PULSE_BW_VALUE",               743, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.ECHO_WIN_LEN_VALUE",              763, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.UP_VALUE",                        783, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.DOWN_VALUE",                      803, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.RESAMP_VALUE",                    823, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.BEAM_ADJ_VALUE",                  843, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.BEAM_SET_VALUE",                  863, EDT_UInt16,      5},
    {"IMAGE_PARAMETERS.TX_MONITOR_VALUE",                873, EDT_Float32,     5},
    /*{"SPARE_6",                                        893, EDT_UByte,      82},*/
    {"FIRST_PROC_RANGE_SAMP",                            975, EDT_UInt32,      1},
    {"RANGE_REF",                                        979, EDT_Float32,     1},
    {"RANGE_SAMP_RATE",                                  983, EDT_Float32,     1},
    {"RADAR_FREQ",                                       987, EDT_Float32,     1},
    {"NUM_LOOKS_RANGE",                                  991, EDT_UInt16,      1},
    {"FILTER_WINDOW",                                    993, EDT_Char,        7},
    {"WINDOW_COEF_RANGE",                               1000, EDT_Float32,     1},
    {"BANDWIDTH.LOOK_BW_RANGE",                         1004, EDT_Float32,     5},
    {"BANDWIDTH.TOT_BW_RANGE",                          1024, EDT_Float32,     5},
    {"NOMINAL_CHIRP.1.NOM_CHIRP_AMP",                   1044, EDT_Float32,     4},
    {"NOMINAL_CHIRP.1.NOM_CHIRP_PHS",                   1060, EDT_Float32,     4},
    {"NOMINAL_CHIRP.2.NOM_CHIRP_AMP",                   1076, EDT_Float32,     4},
    {"NOMINAL_CHIRP.2.NOM_CHIRP_PHS",                   1092, EDT_Float32,     4},
    {"NOMINAL_CHIRP.3.NOM_CHIRP_AMP",                   1108, EDT_Float32,     4},
    {"NOMINAL_CHIRP.3.NOM_CHIRP_PHS",                   1124, EDT_Float32,     4},
    {"NOMINAL_CHIRP.4.NOM_CHIRP_AMP",                   1140, EDT_Float32,     4},
    {"NOMINAL_CHIRP.4.NOM_CHIRP_PHS",                   1156, EDT_Float32,     4},
    {"NOMINAL_CHIRP.5.NOM_CHIRP_AMP",                   1172, EDT_Float32,     4},
    {"NOMINAL_CHIRP.5.NOM_CHIRP_PHS",                   1188, EDT_Float32,     4},
    /*{"SPARE_7",                                       1204, EDT_UByte,      60},*/
    {"NUM_LINES_PROC",                                  1264, EDT_UInt32,      1},
    {"NUM_LOOK_AZ",                                     1268, EDT_UInt16,      1},
    {"LOOK_BW_AZ",                                      1270, EDT_Float32,     1},
    {"TO_BW_AZ",                                        1274, EDT_Float32,     1},
    {"FILTER_AZ",                                       1278, EDT_Char,        7},
    {"FILTER_COEF_AZ",                                  1285, EDT_Float32,     1},
    {"AZ_FM_RATE",                                      1289, EDT_Float32,     3},
    {"AX_FM_ORIGIN",                                    1301, EDT_Float32,     1},
    {"DOP_AMB_CONF",                                    1305, EDT_Float32,     1},
    /*{"SPARE_8",                                       1309, EDT_UByte,      68},*/
    {"CALIBRATION_FACTORS.1.PROC_SCALING_FACT",         1377, EDT_Float32,     1},
    {"CALIBRATION_FACTORS.1.EXT_CAL_FACT",              1381, EDT_Float32,     1},
    {"CALIBRATION_FACTORS.2.PROC_SCALING_FACT",         1385, EDT_Float32,     1},
    {"CALIBRATION_FACTORS.2.EXT_CAL_FACT",              1389, EDT_Float32,     1},
    {"NOISE_ESTIMATION.NOISE_POWER_CORR",               1393, EDT_Float32,     5},
    {"NOISE_ESTIMATION.NUM_NOISE_LINES",                1413, EDT_UInt32,      5},
    /*{"SPARE_9",                                       1433, EDT_UByte,      76},*/
    {"OUTPUT_STATISTICS.1.OUT_MEAN",                    1509, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.1.OUT_IMAG_MEAN",               1513, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.1.OUT_STD_DEV",                 1517, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.1.OUT_IMAG_STD_DEV",            1521, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.2.OUT_MEAN",                    1525, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.2.OUT_IMAG_MEAN",               1529, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.2.OUT_STD_DEV",                 1533, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.2.OUT_IMAG_STD_DEV",            1537, EDT_Float32,     1},
    /*{"SPARE_10",                                      1541, EDT_UByte,      52},*/
    {"ECHO_COMP",                                       1593, EDT_Char,        4},
    {"ECHO_COMP_RATIO",                                 1597, EDT_Char,        3},
    {"INIT_CAL_COMP",                                   1600, EDT_Char,        4},
    {"INIT_CAL_RATIO",                                  1604, EDT_Char,        3},
    {"PER_CAL_COMP",                                    1607, EDT_Char,        4},
    {"PER_CAL_RATIO",                                   1611, EDT_Char,        3},
    {"NOISE_COMP",                                      1614, EDT_Char,        4},
    {"NOISE_COMP_RATIO",                                1618, EDT_Char,        3},
    /*{"SPARE_11",                                      1621, EDT_UByte,      64},*/
    {"BEAM_MERGE_SL_RANGE",                             1685, EDT_UInt32,      4},
    {"BEAM_MERGE_ALG_PARAM",                            1701, EDT_Float32,     4},
    {"LINES_PER_BURST",                                 1717, EDT_UInt32,      5},
    /*{"SPARE_12",                                      1737, EDT_UByte,      28},*/
    {"ORBIT_STATE_VECTORS.1.STATE_VECT_TIME_1",         1765, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.1.X_POS_1",                   1777, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.Y_POS_1",                   1781, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.Z_POS_1",                   1785, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.X_VEL_1",                   1789, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.Y_VEL_1",                   1793, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.Z_VEL_1",                   1797, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.STATE_VECT_TIME_1",         1801, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.2.X_POS_1",                   1813, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.Y_POS_1",                   1817, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.Z_POS_1",                   1821, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.X_VEL_1",                   1825, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.Y_VEL_1",                   1829, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.Z_VEL_1",                   1833, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.STATE_VECT_TIME_1",         1837, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.3.X_POS_1",                   1849, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.Y_POS_1",                   1853, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.Z_POS_1",                   1857, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.X_VEL_1",                   1861, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.Y_VEL_1",                   1865, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.Z_VEL_1",                   1869, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.STATE_VECT_TIME_1",         1873, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.4.X_POS_1",                   1885, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.Y_POS_1",                   1889, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.Z_POS_1",                   1893, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.X_VEL_1",                   1897, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.Y_VEL_1",                   1901, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.Z_VEL_1",                   1905, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.STATE_VECT_TIME_1",         1909, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.5.X_POS_1",                   1921, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.Y_POS_1",                   1925, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.Z_POS_1",                   1929, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.X_VEL_1",                   1933, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.Y_VEL_1",                   1937, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.Z_VEL_1",                   1941, EDT_Int32,       1},
    /*{"SPARE_13",                                      1945, EDT_UByte,      64},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr ASAR_MAP_PROJECTION_GADS[] = {
    {"MAP_DESCRIPTOR",                                     0, EDT_Char,       32},
    {"SAMPLES",                                           32, EDT_UInt32,      1},
    {"LINES",                                             36, EDT_UInt32,      1},
    {"SAMPLE_SPACING",                                    40, EDT_Float32,     1},
    {"LINE_SPACING",                                      44, EDT_Float32,     1},
    {"ORIENTATION",                                       48, EDT_Float32,     1},
    /*{"SPARE_1",                                         52, EDT_UByte,      40},*/
    {"HEADING",                                           92, EDT_Float32,     1},
    {"ELLIPSOID_NAME",                                    96, EDT_Char,       32},
    {"SEMI_MAJOR",                                       128, EDT_Float32,     1},
    {"SEMI_MINOR",                                       132, EDT_Float32,     1},
    {"SHIFT_DX",                                         136, EDT_Float32,     1},
    {"SHIFT_DY",                                         140, EDT_Float32,     1},
    {"SHIFT_DZ",                                         144, EDT_Float32,     1},
    {"AVG_HEIGHT",                                       148, EDT_Float32,     1},
    /*{"SPARE_2",                                        152, EDT_UByte,      12},*/
    {"PROJECTION_DESCRIPTION",                           164, EDT_Char,       32},
    {"UTM_DESCRIPTOR",                                   196, EDT_Char,       32},
    {"UTM_ZONE",                                         228, EDT_Char,        4},
    {"UTM_ORIGIN_EASTING",                               232, EDT_Float32,     1},
    {"UTM_ORIGIN_NORTHING",                              236, EDT_Float32,     1},
    {"UTM_CENTER_LONG",                                  240, EDT_Int32,       1},
    {"UTM_CENTER_LAT",                                   244, EDT_Int32,       1},
    {"UTM_PARA1",                                        248, EDT_Float32,     1},
    {"UTM_PARA2",                                        252, EDT_Float32,     1},
    {"UTM_SCALE",                                        256, EDT_Float32,     1},
    {"UPS_DESCRIPTOR",                                   260, EDT_Char,       32},
    {"UPS_CENTER_LONG",                                  292, EDT_Int32,       1},
    {"UPS_CENTER_LAT",                                   296, EDT_Int32,       1},
    {"UPS_SCALE",                                        300, EDT_Float32,     1},
    {"NSP_DESCRIPTOR",                                   304, EDT_Char,       32},
    {"ORIGIN_EASTING",                                   336, EDT_Float32,     1},
    {"ORIGIN_NORTHING",                                  340, EDT_Float32,     1},
    {"CENTER_LONG",                                      344, EDT_Int32,       1},
    {"CENTER_LAT",                                       348, EDT_Int32,       1},
    {"STANDARD_PARALLEL_PARAMETERS.PARA1",               352, EDT_Float32,     1},
    {"STANDARD_PARALLEL_PARAMETERS.PARA2",               356, EDT_Float32,     1},
    {"STANDARD_PARALLEL_PARAMETERS.PARA3",               360, EDT_Float32,     1},
    {"STANDARD_PARALLEL_PARAMETERS.PARA4",               364, EDT_Float32,     1},
    {"CENTRAL_MERIDIAN_PARAMETERS.CENTRAL_M1",           368, EDT_Float32,     1},
    {"CENTRAL_MERIDIAN_PARAMETERS.CENTRAL_M2",           372, EDT_Float32,     1},
    {"CENTRAL_MERIDIAN_PARAMETERS.CENTRAL_M3",           376, EDT_Float32,     1},
    /*{"PROJECTION_PARAMETERS.SPARE_3",                  380, EDT_UByte,      16},*/
    {"POSITION_NORTHINGS_EASTINGS.TL_NORTHING",          396, EDT_Float32,     1},
    {"POSITION_NORTHINGS_EASTINGS.TL_EASTING",           400, EDT_Float32,     1},
    {"POSITION_NORTHINGS_EASTINGS.TR_NORTHING",          404, EDT_Float32,     1},
    {"POSITION_NORTHINGS_EASTINGS.TR_EASTING",           408, EDT_Float32,     1},
    {"POSITION_NORTHINGS_EASTINGS.BR_NORTHING",          412, EDT_Float32,     1},
    {"POSITION_NORTHINGS_EASTINGS.BR_EASTING",           416, EDT_Float32,     1},
    {"POSITION_NORTHINGS_EASTINGS.BL_NORTHING",          420, EDT_Float32,     1},
    {"POSITION_NORTHINGS_EASTINGS.BL_EASTING",           424, EDT_Float32,     1},
    {"POSITION_LAT_LONG.TL_LAT",                         428, EDT_Int32,       1},
    {"POSITION_LAT_LONG.TL_LONG",                        432, EDT_Int32,       1},
    {"POSITION_LAT_LONG.TR_LAT",                         436, EDT_Int32,       1},
    {"POSITION_LAT_LONG.TR_LONG",                        440, EDT_Int32,       1},
    {"POSITION_LAT_LONG.BR_LAT",                         444, EDT_Int32,       1},
    {"POSITION_LAT_LONG.BR_LONG",                        448, EDT_Int32,       1},
    {"POSITION_LAT_LONG.BL_LAT",                         452, EDT_Int32,       1},
    {"POSITION_LAT_LONG.BL_LONG",                        456, EDT_Int32,       1},
    /*{"SPARE_4",                                        460, EDT_UByte,      32},*/
    {"IMAGE_TO_MAP_COEFS",                               492, EDT_Float32,     8},
    {"MAP_TO_IMAGE_COEFS",                               524, EDT_Float32,     8},
    /*{"SPARE_5",                                        556, EDT_UByte,      35},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr ASAR_SQ_ADSR[] = {
    {"ZERO_DOPPLER_TIME",                                  0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"INPUT_MEAN_FLAG",                                   13, EDT_UByte,       1},
    {"INPUT_STD_DEV_FLAG",                                14, EDT_UByte,       1},
    {"INPUT_GAPS_FLAG",                                   15, EDT_UByte,       1},
    {"INPUT_MISSING_LINES_FLAG",                          16, EDT_UByte,       1},
    {"DOP_CEN_FLAG",                                      17, EDT_UByte,       1},
    {"DOP_AMB_FLAG",                                      18, EDT_UByte,       1},
    {"OUTPUT_MEAN_FLAG",                                  19, EDT_UByte,       1},
    {"OUTPUT_STD_DEV_FLAG",                               20, EDT_UByte,       1},
    {"CHIRP_FLAG",                                        21, EDT_UByte,       1},
    {"MISSING_DATA_SETS_FLAG",                            22, EDT_UByte,       1},
    {"INVALID_DOWNLINK_FLAG",                             23, EDT_UByte,       1},
    /*{"SPARE_1",                                         24, EDT_UByte,       7},*/
    {"THRESH_CHIRP_BROADENING",                           31, EDT_Float32,     1},
    {"THRESH_CHIRP_SIDELOBE",                             35, EDT_Float32,     1},
    {"THRESH_CHIRP_ISLR",                                 39, EDT_Float32,     1},
    {"THRESH_INPUT_MEAN",                                 43, EDT_Float32,     1},
    {"EXP_INPUT_MEAN",                                    47, EDT_Float32,     1},
    {"THRESH_INPUT_STD_DEV",                              51, EDT_Float32,     1},
    {"EXP_INPUT_STD_DEV",                                 55, EDT_Float32,     1},
    {"THRESH_DOP_CEN",                                    59, EDT_Float32,     1},
    {"THRESH_DOP_AMB",                                    63, EDT_Float32,     1},
    {"THRESH_OUTPUT_MEAN",                                67, EDT_Float32,     1},
    {"EXP_OUTPUT_MEAN",                                   71, EDT_Float32,     1},
    {"THRESH_OUTPUT_STD_DEV",                             75, EDT_Float32,     1},
    {"EXP_OUTPUT_STD_DEV",                                79, EDT_Float32,     1},
    {"THRESH_INPUT_MISSING_LINES",                        83, EDT_Float32,     1},
    {"THRESH_INPUT_GAPS",                                 87, EDT_Float32,     1},
    {"LINES_PER_GAPS",                                    91, EDT_UInt32,      1},
    /*{"SPARE_2",                                         95, EDT_UByte,      15},*/
    {"INPUT_MEAN",                                       110, EDT_Float32,     2},
    {"INPUT_STD_DEV",                                    118, EDT_Float32,     2},
    {"NUM_GAPS",                                         126, EDT_Float32,     1},
    {"NUM_MISSING_LINES",                                130, EDT_Float32,     1},
    {"OUTPUT_MEAN",                                      134, EDT_Float32,     2},
    {"OUTPUT_STD_DEV",                                   142, EDT_Float32,     2},
    {"TOT_ERRORS",                                       150, EDT_UInt32,      1},
    /*{"SPARE_3",                                        154, EDT_UByte,      16},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr ASAR_SR_GR_ADSR[] = {
    {"ZERO_DOPPLER_TIME",                                  0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"SLANT_RANGE_TIME",                                  13, EDT_Float32,     1},
    {"GROUND_RANGE_ORIGIN",                               17, EDT_Float32,     1},
    {"SRGR_COEFF",                                        21, EDT_Float32,     5},
    /*{"SPARE_1",                                         41, EDT_UByte,      14},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

#if 0  /* Unused */
static const EnvisatFieldDescr ASAR_GEOLOCATION_ADSR[] = {
    {"ZERO_DOPPLER_TIME",                                  0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"CENTER_LAT",                                        13, EDT_Int32,       1},
    {"CENTER_LONG",                                       17, EDT_Int32,       1},
    /*{"SPARE_1",                                         21, EDT_UByte,       4},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};
#endif  /* Unused */

static const EnvisatFieldDescr ASAR_PROCESSING_PARAMS_ADSR[] = {
    {"FIRST_ZERO_DOPPLER_TIME",                            0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"LAST_ZERO_DOPPLER_TIME",                            13, EDT_MJD,         1},
    {"WORK_ORDER_ID",                                     25, EDT_Char,       12},
    {"TIME_DIFF",                                         37, EDT_Float32,     1},
    {"SWATH_ID",                                          41, EDT_Char,        3},
    {"RANGE_SPACING",                                     44, EDT_Float32,     1},
    {"AZIMUTH_SPACING",                                   48, EDT_Float32,     1},
    {"LINE_TIME_INTERVAL",                                52, EDT_Float32,     1},
    {"NUM_OUTPUT_LINES",                                  56, EDT_UInt32,      1},
    {"NUM_SAMPLES_PER_LINE",                              60, EDT_UInt32,      1},
    {"DATA_TYPE",                                         64, EDT_Char,        5},
    /*{"SPARE_1",                                         69, EDT_UByte,      51},*/
    {"DATA_ANALYSIS_FLAG",                               120, EDT_UByte,       1},
    {"ANT_ELEV_CORR_FLAG",                               121, EDT_UByte,       1},
    {"CHIRP_EXTRACT_FLAG",                               122, EDT_UByte,       1},
    {"SRGR_FLAG",                                        123, EDT_UByte,       1},
    {"DOP_CEN_FLAG",                                     124, EDT_UByte,       1},
    {"DOP_AMB_FLAG",                                     125, EDT_UByte,       1},
    {"RANGE_SPREAD_COMP_FLAG",                           126, EDT_UByte,       1},
    {"DETECTED_FLAG",                                    127, EDT_UByte,       1},
    {"LOOK_SUM_FLAG",                                    128, EDT_UByte,       1},
    {"RMS_EQUAL_FLAG",                                   129, EDT_UByte,       1},
    {"ANT_SCAL_FLAG",                                    130, EDT_UByte,       1},
    /*{"SPARE_2",                                        131, EDT_UByte,      10},*/
    {"RAW_DATA_ANALYSIS.1.NUM_GAPS",                     141, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.1.NUM_MISSING_LINES",            145, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.1.RANGE_SAMP_SKIP",              149, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.1.RANGE_LINES_SKIP",             153, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.1.CALC_I_BIAS",                  157, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_Q_BIAS",                  161, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_I_STD_DEV",               165, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_Q_STD_DEV",               169, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_GAIN",                    173, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.CALC_QUAD",                    177, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.I_BIAS_MAX",                   181, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.I_BIAS_MIN",                   185, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.Q_BIAS_MAX",                   189, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.Q_BIAS_MIN",                   193, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.GAIN_MIN",                     197, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.GAIN_MAX",                     201, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.QUAD_MIN",                     205, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.QUAD_MAX",                     209, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.I_BIAS_FLAG",                  213, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.1.Q_BIAS_FLAG",                  214, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.1.GAIN_FLAG",                    215, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.1.QUAD_FLAG",                    216, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.1.USED_I_BIAS",                  217, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.USED_Q_BIAS",                  221, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.USED_GAIN",                    225, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.1.USED_QUAD",                    229, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.NUM_GAPS",                     233, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.2.NUM_MISSING_LINES",            237, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.2.RANGE_SAMP_SKIP",              241, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.2.RANGE_LINES_SKIP",             245, EDT_UInt32,      1},
    {"RAW_DATA_ANALYSIS.2.CALC_I_BIAS",                  249, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_Q_BIAS",                  253, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_I_STD_DEV",               257, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_Q_STD_DEV",               261, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_GAIN",                    265, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.CALC_QUAD",                    269, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.I_BIAS_MAX",                   273, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.I_BIAS_MIN",                   277, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.Q_BIAS_MAX",                   281, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.Q_BIAS_MIN",                   285, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.GAIN_MIN",                     289, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.GAIN_MAX",                     293, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.QUAD_MIN",                     297, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.QUAD_MAX",                     301, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.I_BIAS_FLAG",                  305, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.2.Q_BIAS_FLAG",                  306, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.2.GAIN_FLAG",                    307, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.2.QUAD_FLAG",                    308, EDT_UByte,       1},
    {"RAW_DATA_ANALYSIS.2.USED_I_BIAS",                  309, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.USED_Q_BIAS",                  313, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.USED_GAIN",                    317, EDT_Float32,     1},
    {"RAW_DATA_ANALYSIS.2.USED_QUAD",                    321, EDT_Float32,     1},
    /*{"SPARE_3",                                        325, EDT_UByte,      32},*/
    {"START_TIME.1.FIRST_OBT",                           357, EDT_UInt32,      2},
    {"START_TIME.1.FIRST_MJD",                           365, EDT_MJD,         1},
    {"START_TIME.2.FIRST_OBT",                           377, EDT_UInt32,      2},
    {"START_TIME.2.FIRST_MJD",                           385, EDT_MJD,         1},
    {"PARAMETER_CODES.SWST_CODE",                        397, EDT_UInt16,      5},
    {"PARAMETER_CODES.LAST_SWST_CODE",                   407, EDT_UInt16,      5},
    {"PARAMETER_CODES.PRI_CODE",                         417, EDT_UInt16,      5},
    {"PARAMETER_CODES.TX_PULSE_LEN_CODE",                427, EDT_UInt16,      5},
    {"PARAMETER_CODES.TX_BW_CODE",                       437, EDT_UInt16,      5},
    {"PARAMETER_CODES.ECHO_WIN_LEN_CODE",                447, EDT_UInt16,      5},
    {"PARAMETER_CODES.UP_CODE",                          457, EDT_UInt16,      5},
    {"PARAMETER_CODES.DOWN_CODE",                        467, EDT_UInt16,      5},
    {"PARAMETER_CODES.RESAMP_CODE",                      477, EDT_UInt16,      5},
    {"PARAMETER_CODES.BEAM_ADJ_CODE",                    487, EDT_UInt16,      5},
    {"PARAMETER_CODES.BEAM_SET_NUM_CODE",                497, EDT_UInt16,      5},
    {"PARAMETER_CODES.TX_MONITOR_CODE",                  507, EDT_UInt16,      5},
    /*{"SPARE_4",                                        517, EDT_UByte,      60},*/
    {"ERROR_COUNTERS.NUM_ERR_SWST",                      577, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_PRI",                       581, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_TX_PULSE_LEN",              585, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_TX_PULSE_BW",               589, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_ECHO_WIN_LEN",              593, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_UP",                        597, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_DOWN",                      601, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_RESAMP",                    605, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_BEAM_ADJ",                  609, EDT_UInt32,      1},
    {"ERROR_COUNTERS.NUM_ERR_BEAM_SET_NUM",              613, EDT_UInt32,      1},
    /*{"SPARE_5",                                        617, EDT_UByte,      26},*/
    {"IMAGE_PARAMETERS.SWST_VALUE",                      643, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.LAST_SWST_VALUE",                 663, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.SWST_CHANGES",                    683, EDT_UInt32,      5},
    {"IMAGE_PARAMETERS.PRF_VALUE",                       703, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.TX_PULSE_LEN_VALUE",              723, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.TX_PULSE_BW_VALUE",               743, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.ECHO_WIN_LEN_VALUE",              763, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.UP_VALUE",                        783, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.DOWN_VALUE",                      803, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.RESAMP_VALUE",                    823, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.BEAM_ADJ_VALUE",                  843, EDT_Float32,     5},
    {"IMAGE_PARAMETERS.BEAM_SET_VALUE",                  863, EDT_UInt16,      5},
    {"IMAGE_PARAMETERS.TX_MONITOR_VALUE",                873, EDT_Float32,     5},
    /*{"SPARE_6",                                        893, EDT_UByte,      82},*/
    {"FIRST_PROC_RANGE_SAMP",                            975, EDT_UInt32,      1},
    {"RANGE_REF",                                        979, EDT_Float32,     1},
    {"RANGE_SAMP_RATE",                                  983, EDT_Float32,     1},
    {"RADAR_FREQ",                                       987, EDT_Float32,     1},
    {"NUM_LOOKS_RANGE",                                  991, EDT_UInt16,      1},
    {"FILTER_RANGE",                                     993, EDT_Char,        7},
    {"FILTER_COEF_RANGE",                               1000, EDT_Float32,     1},
    {"BANDWIDTH.LOOK_BW_RANGE",                         1004, EDT_Float32,     5},
    {"BANDWIDTH.TOT_BW_RANGE",                          1024, EDT_Float32,     5},
    {"NOMINAL_CHIRP.1.NOM_CHIRP_AMP",                   1044, EDT_Float32,     4},
    {"NOMINAL_CHIRP.1.NOM_CHIRP_PHS",                   1060, EDT_Float32,     4},
    {"NOMINAL_CHIRP.2.NOM_CHIRP_AMP",                   1076, EDT_Float32,     4},
    {"NOMINAL_CHIRP.2.NOM_CHIRP_PHS",                   1092, EDT_Float32,     4},
    {"NOMINAL_CHIRP.3.NOM_CHIRP_AMP",                   1108, EDT_Float32,     4},
    {"NOMINAL_CHIRP.3.NOM_CHIRP_PHS",                   1124, EDT_Float32,     4},
    {"NOMINAL_CHIRP.4.NOM_CHIRP_AMP",                   1140, EDT_Float32,     4},
    {"NOMINAL_CHIRP.4.NOM_CHIRP_PHS",                   1156, EDT_Float32,     4},
    {"NOMINAL_CHIRP.5.NOM_CHIRP_AMP",                   1172, EDT_Float32,     4},
    {"NOMINAL_CHIRP.5.NOM_CHIRP_PHS",                   1188, EDT_Float32,     4},
    /*{"SPARE_7",                                       1204, EDT_UByte,      60},*/
    {"NUM_LINES_PROC",                                  1264, EDT_UInt32,      1},
    {"NUM_LOOK_AZ",                                     1268, EDT_UInt16,      1},
    {"LOOK_BW_AZ",                                      1270, EDT_Float32,     1},
    {"TO_BW_AZ",                                        1274, EDT_Float32,     1},
    {"FILTER_AZ",                                       1278, EDT_Char,        7},
    {"FILTER_COEF_AZ",                                  1285, EDT_Float32,     1},
    {"AZ_FM_RATE",                                      1289, EDT_Float32,     3},
    {"AX_FM_ORIGIN",                                    1301, EDT_Float32,     1},
    {"DOP_AMB_CONF",                                    1305, EDT_Float32,     1},
    /*{"SPARE_8",                                       1309, EDT_UByte,      68},*/
    {"CALIBRATION_FACTORS.1.PROC_SCALING_FACT",         1377, EDT_Float32,     1},
    {"CALIBRATION_FACTORS.1.EXT_CAL_FACT",              1381, EDT_Float32,     1},
    {"CALIBRATION_FACTORS.2.PROC_SCALING_FACT",         1385, EDT_Float32,     1},
    {"CALIBRATION_FACTORS.2.EXT_CAL_FACT",              1389, EDT_Float32,     1},
    {"NOISE_ESTIMATION.NOISE_POWER_CORR",               1393, EDT_Float32,     5},
    {"NOISE_ESTIMATION.NUM_NOISE_LINES",                1413, EDT_UInt32,      5},
    /*{"SPARE_9",                                       1433, EDT_UByte,      76},*/
    {"OUTPUT_STATISTICS.1.OUT_MEAN",                    1509, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.1.OUT_IMAG_MEAN",               1513, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.1.OUT_STD_DEV",                 1517, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.1.OUT_IMAG_STD_DEV",            1521, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.2.OUT_MEAN",                    1525, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.2.OUT_IMAG_MEAN",               1529, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.2.OUT_STD_DEV",                 1533, EDT_Float32,     1},
    {"OUTPUT_STATISTICS.2.OUT_IMAG_STD_DEV",            1537, EDT_Float32,     1},
    /*{"SPARE_10",                                      1541, EDT_UByte,      52},*/
    {"ECHO_COMP",                                       1593, EDT_Char,        4},
    {"ECHO_COMP_RATIO",                                 1597, EDT_Char,        3},
    {"INIT_CAL_COMP",                                   1600, EDT_Char,        4},
    {"INIT_CAL_RATIO",                                  1604, EDT_Char,        3},
    {"PER_CAL_COMP",                                    1607, EDT_Char,        4},
    {"PER_CAL_RATIO",                                   1611, EDT_Char,        3},
    {"NOISE_COMP",                                      1614, EDT_Char,        4},
    {"NOISE_COMP_RATIO",                                1618, EDT_Char,        3},
    /*{"SPARE_11",                                      1621, EDT_UByte,      64},*/
    {"BEAM_OVERLAP",                                    1685, EDT_UInt32,      4},
    {"LINES_PER_BURST",                                 1701, EDT_UInt32,      5},
    /*{"SPARE_12",                                      1721, EDT_UByte,      44},*/
    {"ORBIT_STATE_VECTORS.1.STATE_VECT_TIME_1",         1765, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.1.X_POS_1",                   1777, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.Y_POS_1",                   1781, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.Z_POS_1",                   1785, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.X_VEL_1",                   1789, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.Y_VEL_1",                   1793, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.1.Z_VEL_1",                   1797, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.STATE_VECT_TIME_1",         1801, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.2.X_POS_1",                   1813, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.Y_POS_1",                   1817, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.Z_POS_1",                   1821, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.X_VEL_1",                   1825, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.Y_VEL_1",                   1829, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.2.Z_VEL_1",                   1833, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.STATE_VECT_TIME_1",         1837, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.3.X_POS_1",                   1849, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.Y_POS_1",                   1853, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.Z_POS_1",                   1857, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.X_VEL_1",                   1861, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.Y_VEL_1",                   1865, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.3.Z_VEL_1",                   1869, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.STATE_VECT_TIME_1",         1873, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.4.X_POS_1",                   1885, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.Y_POS_1",                   1889, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.Z_POS_1",                   1893, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.X_VEL_1",                   1897, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.Y_VEL_1",                   1901, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.4.Z_VEL_1",                   1905, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.STATE_VECT_TIME_1",         1909, EDT_MJD,         1},
    {"ORBIT_STATE_VECTORS.5.X_POS_1",                   1921, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.Y_POS_1",                   1925, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.Z_POS_1",                   1929, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.X_VEL_1",                   1933, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.Y_VEL_1",                   1937, EDT_Int32,       1},
    {"ORBIT_STATE_VECTORS.5.Z_VEL_1",                   1941, EDT_Int32,       1},
    /*{"SPARE_13",                                      1945, EDT_UByte,      64},*/
    {"SLANT_RANGE_TIME",                                2009, EDT_Float32,     1},
    {"DOP_COEF",                                        2013, EDT_Float32,     5},
    {"DOP_CONF",                                        2033, EDT_Float32,     1},
    /*{"SPARE_14",                                      2037, EDT_UByte,      14},*/
    {"CHIRP_WIDTH",                                     2051, EDT_Float32,     1},
    {"CHIRP_SIDELOBE",                                  2055, EDT_Float32,     1},
    {"CHIRP_ISLR",                                      2059, EDT_Float32,     1},
    {"CHIRP_PEAK_LOC",                                  2063, EDT_Float32,     1},
    {"CHIRP_POWER",                                     2067, EDT_Float32,     1},
    {"ELEV_CORR_FACTOR",                                2071, EDT_Float32,     1},
    /*{"SPARE_15",                                      2075, EDT_UByte,      16},*/
    {"CAL_INFO.1.MAX_CAL",                              2091, EDT_Float32,     3},
    {"CAL_INFO.1.AVG_CAL",                              2103, EDT_Float32,     3},
    {"CAL_INFO.1.AVG_VAL_1A",                           2115, EDT_Float32,     1},
    {"CAL_INFO.1.PHS_CAL",                              2119, EDT_Float32,     4},
    {"CAL_INFO.2.MAX_CAL",                              2135, EDT_Float32,     3},
    {"CAL_INFO.2.AVG_CAL",                              2147, EDT_Float32,     3},
    {"CAL_INFO.2.AVG_VAL_1A",                           2159, EDT_Float32,     1},
    {"CAL_INFO.2.PHS_CAL",                              2163, EDT_Float32,     4},
    {"CAL_INFO.3.MAX_CAL",                              2179, EDT_Float32,     3},
    {"CAL_INFO.3.AVG_CAL",                              2191, EDT_Float32,     3},
    {"CAL_INFO.3.AVG_VAL_1A",                           2203, EDT_Float32,     1},
    {"CAL_INFO.3.PHS_CAL",                              2207, EDT_Float32,     4},
    {"CAL_INFO.4.MAX_CAL",                              2223, EDT_Float32,     3},
    {"CAL_INFO.4.AVG_CAL",                              2235, EDT_Float32,     3},
    {"CAL_INFO.4.AVG_VAL_1A",                           2247, EDT_Float32,     1},
    {"CAL_INFO.4.PHS_CAL",                              2251, EDT_Float32,     4},
    {"CAL_INFO.5.MAX_CAL",                              2267, EDT_Float32,     3},
    {"CAL_INFO.5.AVG_CAL",                              2279, EDT_Float32,     3},
    {"CAL_INFO.5.AVG_VAL_1A",                           2291, EDT_Float32,     1},
    {"CAL_INFO.5.PHS_CAL",                              2295, EDT_Float32,     4},
    {"CAL_INFO.6.MAX_CAL",                              2311, EDT_Float32,     3},
    {"CAL_INFO.6.AVG_CAL",                              2323, EDT_Float32,     3},
    {"CAL_INFO.6.AVG_VAL_1A",                           2335, EDT_Float32,     1},
    {"CAL_INFO.6.PHS_CAL",                              2339, EDT_Float32,     4},
    {"CAL_INFO.7.MAX_CAL",                              2355, EDT_Float32,     3},
    {"CAL_INFO.7.AVG_CAL",                              2367, EDT_Float32,     3},
    {"CAL_INFO.7.AVG_VAL_1A",                           2379, EDT_Float32,     1},
    {"CAL_INFO.7.PHS_CAL",                              2383, EDT_Float32,     4},
    {"CAL_INFO.8.MAX_CAL",                              2399, EDT_Float32,     3},
    {"CAL_INFO.8.AVG_CAL",                              2411, EDT_Float32,     3},
    {"CAL_INFO.8.AVG_VAL_1A",                           2423, EDT_Float32,     1},
    {"CAL_INFO.8.PHS_CAL",                              2427, EDT_Float32,     4},
    {"CAL_INFO.9.MAX_CAL",                              2443, EDT_Float32,     3},
    {"CAL_INFO.9.AVG_CAL",                              2455, EDT_Float32,     3},
    {"CAL_INFO.9.AVG_VAL_1A",                           2467, EDT_Float32,     1},
    {"CAL_INFO.9.PHS_CAL",                              2471, EDT_Float32,     4},
    {"CAL_INFO.10.MAX_CAL",                             2487, EDT_Float32,     3},
    {"CAL_INFO.10.AVG_CAL",                             2499, EDT_Float32,     3},
    {"CAL_INFO.10.AVG_VAL_1A",                          2511, EDT_Float32,     1},
    {"CAL_INFO.10.PHS_CAL",                             2515, EDT_Float32,     4},
    {"CAL_INFO.11.MAX_CAL",                             2531, EDT_Float32,     3},
    {"CAL_INFO.11.AVG_CAL",                             2543, EDT_Float32,     3},
    {"CAL_INFO.11.AVG_VAL_1A",                          2555, EDT_Float32,     1},
    {"CAL_INFO.11.PHS_CAL",                             2559, EDT_Float32,     4},
    {"CAL_INFO.12.MAX_CAL",                             2575, EDT_Float32,     3},
    {"CAL_INFO.12.AVG_CAL",                             2587, EDT_Float32,     3},
    {"CAL_INFO.12.AVG_VAL_1A",                          2599, EDT_Float32,     1},
    {"CAL_INFO.12.PHS_CAL",                             2603, EDT_Float32,     4},
    {"CAL_INFO.13.MAX_CAL",                             2619, EDT_Float32,     3},
    {"CAL_INFO.13.AVG_CAL",                             2631, EDT_Float32,     3},
    {"CAL_INFO.13.AVG_VAL_1A",                          2643, EDT_Float32,     1},
    {"CAL_INFO.13.PHS_CAL",                             2647, EDT_Float32,     4},
    {"CAL_INFO.14.MAX_CAL",                             2663, EDT_Float32,     3},
    {"CAL_INFO.14.AVG_CAL",                             2675, EDT_Float32,     3},
    {"CAL_INFO.14.AVG_VAL_1A",                          2687, EDT_Float32,     1},
    {"CAL_INFO.14.PHS_CAL",                             2691, EDT_Float32,     4},
    {"CAL_INFO.15.MAX_CAL",                             2707, EDT_Float32,     3},
    {"CAL_INFO.15.AVG_CAL",                             2719, EDT_Float32,     3},
    {"CAL_INFO.15.AVG_VAL_1A",                          2731, EDT_Float32,     1},
    {"CAL_INFO.15.PHS_CAL",                             2735, EDT_Float32,     4},
    {"CAL_INFO.16.MAX_CAL",                             2751, EDT_Float32,     3},
    {"CAL_INFO.16.AVG_CAL",                             2763, EDT_Float32,     3},
    {"CAL_INFO.16.AVG_VAL_1A",                          2775, EDT_Float32,     1},
    {"CAL_INFO.16.PHS_CAL",                             2779, EDT_Float32,     4},
    {"CAL_INFO.17.MAX_CAL",                             2795, EDT_Float32,     3},
    {"CAL_INFO.17.AVG_CAL",                             2807, EDT_Float32,     3},
    {"CAL_INFO.17.AVG_VAL_1A",                          2819, EDT_Float32,     1},
    {"CAL_INFO.17.PHS_CAL",                             2823, EDT_Float32,     4},
    {"CAL_INFO.18.MAX_CAL",                             2839, EDT_Float32,     3},
    {"CAL_INFO.18.AVG_CAL",                             2851, EDT_Float32,     3},
    {"CAL_INFO.18.AVG_VAL_1A",                          2863, EDT_Float32,     1},
    {"CAL_INFO.18.PHS_CAL",                             2867, EDT_Float32,     4},
    {"CAL_INFO.19.MAX_CAL",                             2883, EDT_Float32,     3},
    {"CAL_INFO.19.AVG_CAL",                             2895, EDT_Float32,     3},
    {"CAL_INFO.19.AVG_VAL_1A",                          2907, EDT_Float32,     1},
    {"CAL_INFO.19.PHS_CAL",                             2911, EDT_Float32,     4},
    {"CAL_INFO.20.MAX_CAL",                             2927, EDT_Float32,     3},
    {"CAL_INFO.20.AVG_CAL",                             2939, EDT_Float32,     3},
    {"CAL_INFO.20.AVG_VAL_1A",                          2951, EDT_Float32,     1},
    {"CAL_INFO.20.PHS_CAL",                             2955, EDT_Float32,     4},
    {"CAL_INFO.21.MAX_CAL",                             2971, EDT_Float32,     3},
    {"CAL_INFO.21.AVG_CAL",                             2983, EDT_Float32,     3},
    {"CAL_INFO.21.AVG_VAL_1A",                          2995, EDT_Float32,     1},
    {"CAL_INFO.21.PHS_CAL",                             2999, EDT_Float32,     4},
    {"CAL_INFO.22.MAX_CAL",                             3015, EDT_Float32,     3},
    {"CAL_INFO.22.AVG_CAL",                             3027, EDT_Float32,     3},
    {"CAL_INFO.22.AVG_VAL_1A",                          3039, EDT_Float32,     1},
    {"CAL_INFO.22.PHS_CAL",                             3043, EDT_Float32,     4},
    {"CAL_INFO.23.MAX_CAL",                             3059, EDT_Float32,     3},
    {"CAL_INFO.23.AVG_CAL",                             3071, EDT_Float32,     3},
    {"CAL_INFO.23.AVG_VAL_1A",                          3083, EDT_Float32,     1},
    {"CAL_INFO.23.PHS_CAL",                             3087, EDT_Float32,     4},
    {"CAL_INFO.24.MAX_CAL",                             3103, EDT_Float32,     3},
    {"CAL_INFO.24.AVG_CAL",                             3115, EDT_Float32,     3},
    {"CAL_INFO.24.AVG_VAL_1A",                          3127, EDT_Float32,     1},
    {"CAL_INFO.24.PHS_CAL",                             3131, EDT_Float32,     4},
    {"CAL_INFO.25.MAX_CAL",                             3147, EDT_Float32,     3},
    {"CAL_INFO.25.AVG_CAL",                             3159, EDT_Float32,     3},
    {"CAL_INFO.25.AVG_VAL_1A",                          3171, EDT_Float32,     1},
    {"CAL_INFO.25.PHS_CAL",                             3175, EDT_Float32,     4},
    {"CAL_INFO.26.MAX_CAL",                             3191, EDT_Float32,     3},
    {"CAL_INFO.26.AVG_CAL",                             3203, EDT_Float32,     3},
    {"CAL_INFO.26.AVG_VAL_1A",                          3215, EDT_Float32,     1},
    {"CAL_INFO.26.PHS_CAL",                             3219, EDT_Float32,     4},
    {"CAL_INFO.27.MAX_CAL",                             3235, EDT_Float32,     3},
    {"CAL_INFO.27.AVG_CAL",                             3247, EDT_Float32,     3},
    {"CAL_INFO.27.AVG_VAL_1A",                          3259, EDT_Float32,     1},
    {"CAL_INFO.27.PHS_CAL",                             3263, EDT_Float32,     4},
    {"CAL_INFO.28.MAX_CAL",                             3279, EDT_Float32,     3},
    {"CAL_INFO.28.AVG_CAL",                             3291, EDT_Float32,     3},
    {"CAL_INFO.28.AVG_VAL_1A",                          3303, EDT_Float32,     1},
    {"CAL_INFO.28.PHS_CAL",                             3307, EDT_Float32,     4},
    {"CAL_INFO.29.MAX_CAL",                             3323, EDT_Float32,     3},
    {"CAL_INFO.29.AVG_CAL",                             3335, EDT_Float32,     3},
    {"CAL_INFO.29.AVG_VAL_1A",                          3347, EDT_Float32,     1},
    {"CAL_INFO.29.PHS_CAL",                             3351, EDT_Float32,     4},
    {"CAL_INFO.30.MAX_CAL",                             3367, EDT_Float32,     3},
    {"CAL_INFO.30.AVG_CAL",                             3379, EDT_Float32,     3},
    {"CAL_INFO.30.AVG_VAL_1A",                          3391, EDT_Float32,     1},
    {"CAL_INFO.30.PHS_CAL",                             3395, EDT_Float32,     4},
    {"CAL_INFO.31.MAX_CAL",                             3411, EDT_Float32,     3},
    {"CAL_INFO.31.AVG_CAL",                             3423, EDT_Float32,     3},
    {"CAL_INFO.31.AVG_VAL_1A",                          3435, EDT_Float32,     1},
    {"CAL_INFO.31.PHS_CAL",                             3439, EDT_Float32,     4},
    {"CAL_INFO.32.MAX_CAL",                             3455, EDT_Float32,     3},
    {"CAL_INFO.32.AVG_CAL",                             3467, EDT_Float32,     3},
    {"CAL_INFO.32.AVG_VAL_1A",                          3479, EDT_Float32,     1},
    {"CAL_INFO.32.PHS_CAL",                             3483, EDT_Float32,     4},
    /*{"SPARE_16",                                      3499, EDT_UByte,      16},*/
    {"FIRST_LINE_TIME",                                 3515, EDT_MJD,         1},
    {"FIRST_LINE_TIE_POINTS.RANGE_SAMP_NUMS_FIRST",     3527, EDT_UInt32,      3},
    {"FIRST_LINE_TIE_POINTS.SLANT_RANGE_TIMES_FIRST",   3539, EDT_Float32,     3},
    {"FIRST_LINE_TIE_POINTS.INC_ANGLES_FIRST",          3551, EDT_Float32,     3},
    {"FIRST_LINE_TIE_POINTS.LATS_FIRST",                3563, EDT_Int32,       3},
    {"FIRST_LINE_TIE_POINTS.LONGS_FIRST",               3575, EDT_Int32,       3},
    {"MID_LINE_TIME",                                   3587, EDT_MJD,         1},
    {"MID_RANGE_LINE_NUMS",                             3599, EDT_UInt32,      1},
    {"MID_LINE_TIE_POINTS.RANGE_SAMP_NUMS_MID",         3603, EDT_UInt32,      3},
    {"MID_LINE_TIE_POINTS.SLANT_RANGE_TIMES_MID",       3615, EDT_Float32,     3},
    {"MID_LINE_TIE_POINTS.INC_ANGLES_MID",              3627, EDT_Float32,     3},
    {"MID_LINE_TIE_POINTS.LATS_MID",                    3639, EDT_Int32,       3},
    {"MID_LINE_TIE_POINTS.LONGS_MID",                   3651, EDT_Int32,       3},
    {"LAST_LINE_TIME",                                  3663, EDT_MJD,         1},
    {"LAST_LINE_NUM",                                   3675, EDT_UInt32,      1},
    {"LAST_LINE_TIE_POINTS.RANGE_SAMP_NUMS_LAST",       3679, EDT_UInt32,      3},
    {"LAST_LINE_TIE_POINTS.SLANT_RANGE_TIMES_LAST",     3691, EDT_Float32,     3},
    {"LAST_LINE_TIE_POINTS.INC_ANGLES_LAST",            3703, EDT_Float32,     3},
    {"LAST_LINE_TIE_POINTS.LATS_LAST",                  3715, EDT_Int32,       3},
    {"LAST_LINE_TIE_POINTS.LONGS_LAST",                 3727, EDT_Int32,       3},
    {"SWST_OFFSET",                                     3739, EDT_Float32,     1},
    {"GROUND_RANGE_BIAS",                               3743, EDT_Float32,     1},
    {"ELEV_ANGLE_BIAS",                                 3747, EDT_Float32,     1},
    {"IMAGETTE_RANGE_LEN",                              3751, EDT_Float32,     1},
    {"IMAGETTE_AZ_LEN",                                 3755, EDT_Float32,     1},
    {"IMAGETTE_RANGE_RES",                              3759, EDT_Float32,     1},
    {"GROUND_RES",                                      3763, EDT_Float32,     1},
    {"IMAGETTE_AZ_RES",                                 3767, EDT_Float32,     1},
    {"PLATFORM_ALT",                                    3771, EDT_Float32,     1},
    {"PLATFORM_VEL",                                    3775, EDT_Float32,     1},
    {"SLANT_RANGE",                                     3779, EDT_Float32,     1},
    {"CW_DRIFT",                                        3783, EDT_Float32,     1},
    {"WAVE_SUBCYCLE",                                   3787, EDT_UInt16,      1},
    {"EARTH_RADIUS",                                    3789, EDT_Float32,     1},
    {"SAT_HEIGHT",                                      3793, EDT_Float32,     1},
    {"FIRST_SAMPLE_SLANT_RANGE",                        3797, EDT_Float32,     1},
    /*{"SPARE_17",                                      3801, EDT_UByte,      12},*/
    {"ELEVATION_PATTERN.SLANT_RANGE_TIME",              3813, EDT_Float32,    11},
    {"ELEVATION_PATTERN.ELEVATION_ANGLES",              3857, EDT_Float32,    11},
    {"ELEVATION_PATTERN.ANTENNA_PATTERN",               3901, EDT_Float32,    11},
    /*{"SPARE_18",                                      3945, EDT_UByte,      14},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr ASAR_WAVE_SQ_ADSR[] = {
    {"ZERO_DOPPLER_TIME",                                  0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"INPUT_MEAN_FLAG",                                   13, EDT_UByte,       1},
    {"INPUT_STD_DEV_FLAG",                                14, EDT_UByte,       1},
    {"INPUT_GAPS_FLAG",                                   15, EDT_UByte,       1},
    {"INPUT_MISSING_LINES_FLAG",                          16, EDT_UByte,       1},
    {"DOP_CEN_FLAG",                                      17, EDT_UByte,       1},
    {"DOP_AMB_FLAG",                                      18, EDT_UByte,       1},
    {"OUTPUT_MEAN_FLAG",                                  19, EDT_UByte,       1},
    {"OUTPUT_STD_DEV_FLAG",                               20, EDT_UByte,       1},
    {"CHIRP_FLAG",                                        21, EDT_UByte,       1},
    {"MISSING_DATA_SETS_FLAG",                            22, EDT_UByte,       1},
    {"INVALID_DOWNLINK_FLAG",                             23, EDT_UByte,       1},
    /*{"SPARE_1",                                         24, EDT_UByte,       7},*/
    {"THRESH_CHIRP_BROADENING",                           31, EDT_Float32,     1},
    {"THRESH_CHIRP_SIDELOBE",                             35, EDT_Float32,     1},
    {"THRESH_CHIRP_ISLR",                                 39, EDT_Float32,     1},
    {"THRESH_INPUT_MEAN",                                 43, EDT_Float32,     1},
    {"EXP_INPUT_MEAN",                                    47, EDT_Float32,     1},
    {"THRESH_INPUT_STD_DEV",                              51, EDT_Float32,     1},
    {"EXP_INPUT_STD_DEV",                                 55, EDT_Float32,     1},
    {"THRESH_DOP_CEN",                                    59, EDT_Float32,     1},
    {"THRESH_DOP_AMB",                                    63, EDT_Float32,     1},
    {"THRESH_OUTPUT_MEAN",                                67, EDT_Float32,     1},
    {"EXP_OUTPUT_MEAN",                                   71, EDT_Float32,     1},
    {"THRESH_OUTPUT_STD_DEV",                             75, EDT_Float32,     1},
    {"EXP_OUTPUT_STD_DEV",                                79, EDT_Float32,     1},
    {"THRESH_INPUT_MISSING_LINES",                        83, EDT_Float32,     1},
    {"THRESH_INPUT_GAPS",                                 87, EDT_Float32,     1},
    {"LINES_PER_GAPS",                                    91, EDT_UInt32,      1},
    /*{"SPARE_2",                                         95, EDT_UByte,      15},*/
    {"INPUT_MEAN",                                       110, EDT_Float32,     2},
    {"INPUT_STD_DEV",                                    118, EDT_Float32,     2},
    {"NUM_GAPS",                                         126, EDT_Float32,     1},
    {"NUM_MISSING_LINES",                                130, EDT_Float32,     1},
    {"OUTPUT_MEAN",                                      134, EDT_Float32,     2},
    {"OUTPUT_STD_DEV",                                   142, EDT_Float32,     2},
    {"TOT_ERRORS",                                       150, EDT_UInt32,      1},
    /*{"SPARE_3",                                        154, EDT_UByte,      16},*/
    {"LAND_FLAG",                                        170, EDT_UByte,       1},
    {"LOOK_CONF_FLAG",                                   171, EDT_UByte,       1},
    {"INTER_LOOK_CONF_FLAG",                             172, EDT_UByte,       1},
    {"AZ_CUTOFF_FLAG",                                   173, EDT_UByte,       1},
    {"AZ_CUTOFF_ITERATION_FLAG",                         174, EDT_UByte,       1},
    {"PHASE_FLAG",                                       175, EDT_UByte,       1},
    /*{"SPARE_4",                                        176, EDT_UByte,       4},*/
    {"LOOK_CONF_THRESH",                                 180, EDT_Float32,     2},
    {"INTER_LOOK_CONF_THRESH",                           188, EDT_Float32,     1},
    {"AZ_CUTOFF_THRESH",                                 192, EDT_Float32,     1},
    {"AZ_CUTOFF_ITERATIONS_THRESH",                      196, EDT_UInt32,      1},
    {"PHASE_PEAK_THRESH",                                200, EDT_Float32,     1},
    {"PHASE_CROSS_THRESH",                               204, EDT_Float32,     1},
    /*{"SPARE_5",                                        208, EDT_UByte,      12},*/
    {"LOOK_CONF",                                        220, EDT_Float32,     1},
    {"INTER_LOOK_CONF",                                  224, EDT_Float32,     1},
    {"AZ_CUTOFF",                                        228, EDT_Float32,     1},
    {"PHASE_PEAK_CONF",                                  232, EDT_Float32,     1},
    {"PHASE_CROSS_CONF",                                 236, EDT_Float32,     1},
    /*{"SPARE_6",                                        240, EDT_UByte,      12},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

/* --- MERIS record descriptors -------------------------------------------- */
static const EnvisatFieldDescr MERIS_1P_QUALITY_ADSR[] = {
    {"DSR_TIME",                                           0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"RANGE_FLAG",                                        13, EDT_UInt16,      5},
    {"RANGE_BLIND_FLAG",                                  23, EDT_UInt16,      5},
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr MERIS_1P_SCALING_FACTOR_GADS[] = {
    {"SCALING_FACTOR_ALT",                                 0, EDT_Float32,     1},
    {"SCALING_FACTOR_ROUGH",                               4, EDT_Float32,     1},
    {"SCALING_FACTOR_ZON_WIND",                            8, EDT_Float32,     1},
    {"SCALING_FACTOR_MERR_WIND",                          12, EDT_Float32,     1},
    {"SCALING_FACTOR_ATM_PRES",                           16, EDT_Float32,     1},
    {"SCALING_FACTOR_OZONE",                              20, EDT_Float32,     1},
    {"SCALING_FACTOR_REL_HUM",                            24, EDT_Float32,     1},
    {"SCALING_FACTOR_RAD",                                28, EDT_Float32,    15},
    {"GAIN_SETTINGS",                                     88, EDT_UByte,      80},
    {"SAMPLING_RATE",                                    168, EDT_UInt32,      1},
    {"SUN_SPECTRAL_FLUX",                                172, EDT_Float32,    15},
    /*{"SPARE_1",                                        232, EDT_UByte,      60},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr MERIS_2P_QUALITY_ADSR[] = {
    {"DSR_TIME",                                           0, EDT_MJD,         1},
    {"ATTACH_FLAG",                                       12, EDT_UByte,       1},
    {"PERC_WATER_ABS_AERO",                               13, EDT_UByte,       1},
    {"PERC_WATER",                                        14, EDT_UByte,       1},
    {"PERC_DDV_LAND",                                     15, EDT_UByte,       1},
    {"PERC_LAND",                                         16, EDT_UByte,       1},
    {"PERC_CLOUD",                                        17, EDT_UByte,       1},
    {"PERC_LOW_POLY_PRESS",                               18, EDT_UByte,       1},
    {"PERC_LOW_NEURAL_PRESS",                             19, EDT_UByte,       1},
    {"PERC_OUT_RAN_INP_WVAPOUR",                          20, EDT_UByte,       1},
    {"PER_OUT_RAN_OUTP_WVAPOUR",                          21, EDT_UByte,       1},
    {"PERC_OUT_RANGE_INP_CL",                             22, EDT_UByte,       1},
    {"PERC_OUT_RAN_OUTP_CL",                              23, EDT_UByte,       1},
    {"PERC_IN_RAN_INP_LAND",                              24, EDT_UByte,       1},
    {"PERC_OUT_RAN_OUTP_LAND",                            25, EDT_UByte,       1},
    {"PERC_OUT_RAN_INP_OCEAN",                            26, EDT_UByte,       1},
    {"PERC_OUT_RAN_OUTP_OCEAN",                           27, EDT_UByte,       1},
    {"PERC_OUT_RAN_INP_CASE1",                            28, EDT_UByte,       1},
    {"PERC_OUT_RAN_OUTP_CASE1",                           29, EDT_UByte,       1},
    {"PERC_OUT_RAN_INP_CASE2",                            30, EDT_UByte,       1},
    {"PERC_OUT_RAN_OUTP_CASE2",                           31, EDT_UByte,       1},
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr MERIS_2P_SCALING_FACTOR_GADS[] = {
    {"SCALING_FACTOR_ALT",                                 0, EDT_Float32,     1},
    {"SCALING_FACTOR_ROUGH",                               4, EDT_Float32,     1},
    {"SCALING_FACTOR_ZON_WIND",                            8, EDT_Float32,     1},
    {"SCALING_FACTOR_MERR_WIND",                          12, EDT_Float32,     1},
    {"SCALING_FACTOR_ATM_PRES",                           16, EDT_Float32,     1},
    {"SCALING_FACTOR_OZONE",                              20, EDT_Float32,     1},
    {"SCALING_FACTOR_REL_HUMID",                          24, EDT_Float32,     1},
    {"SCALING_FACTOR_REFLEC",                             28, EDT_Float32,    13},
    {"SCALING_FACTOR_ALGAL_PIG_IND",                      80, EDT_Float32,     1},
    {"SCALING_FACTOR_YELLOW_SUBS",                        84, EDT_Float32,     1},
    {"SCALING_FACTOR_SUSP_SED",                           88, EDT_Float32,     1},
    {"SCALING_FACTOR_AERO_EPSILON",                       92, EDT_Float32,     1},
    {"SCALING_FACTOR_AER_OPT_THICK",                      96, EDT_Float32,     1},
    {"SCALING_FACTOR_CL_OPT_THICK",                      100, EDT_Float32,     1},
    {"SCALING_FACTOR_SURF_PRES",                         104, EDT_Float32,     1},
    {"SCALING_FACTOR_WVAPOUR",                           108, EDT_Float32,     1},
    {"SCALING_FACTOR_PHOTOSYN_RAD",                      112, EDT_Float32,     1},
    {"SCALING_FACTOR_TOA_VEG",                           116, EDT_Float32,     1},
    {"SCALING_FACTOR_BOA_VEG",                           120, EDT_Float32,     1},
    {"SCALING_FACTOR_CLOUD_ALBEDO",                      124, EDT_Float32,     1},
    {"SCALING_FACTOR_CLOUD_TOP_PRESS",                   128, EDT_Float32,     1},
    {"OFF_REFLEC",                                       132, EDT_Float32,    13},
    {"OFFSET_ALGAL",                                     184, EDT_Float32,     1},
    {"OFFSET_YELLOW_SUBS",                               188, EDT_Float32,     1},
    {"OFFSET_TOTAL_SUSP",                                192, EDT_Float32,     1},
    {"OFFSET_AERO_EPSILON",                              196, EDT_Float32,     1},
    {"OFFSET_AER_OPT_THICK",                             200, EDT_Float32,     1},
    {"OFFSET_CL_OPT_THICK",                              204, EDT_Float32,     1},
    {"OFFSET_SURF_PRES",                                 208, EDT_Float32,     1},
    {"OFFSET_WVAPOUR",                                   212, EDT_Float32,     1},
    {"OFFSET_PHOTOSYN_RAD",                              216, EDT_Float32,     1},
    {"OFFSET_TOA_VEG",                                   220, EDT_Float32,     1},
    {"OFFSET_BOA_VEG",                                   224, EDT_Float32,     1},
    {"OFFSET_CLOUD_ALBEDO",                              228, EDT_Float32,     1},
    {"OFFSET_CLOUD_TOP_PRESS",                           232, EDT_Float32,     1},
    {"GAIN_SETTINGS",                                    236, EDT_UByte,      80},
    {"SAMPLING_RATE",                                    316, EDT_UInt32,      1},
    {"SUN_SPECTRAL_FLUX",                                320, EDT_Float32,    15},
    {"SCALING_FACTOR_RECT_REFL_NIR",                     380, EDT_Float32,     1},
    {"OFFSET_RECT_REFL_NIR",                             384, EDT_Float32,     1},
    {"SCALING_FACTOR_RECT_REFL_RED",                     388, EDT_Float32,     1},
    {"OFFSET_RECT_REFL_RED",                             392, EDT_Float32,     1},
    /*{"SPARE_1",                                        396, EDT_UByte,      44},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr MERIS_2P_C_SCALING_FACTOR_GADS[] = {
    {"SCALING_FACTOR_CLOUD_OPT_THICK",                     0, EDT_Float32,     1},
    {"SCALING_FACTOR_CLOUD_TOP_PRESS",                     4, EDT_Float32,     1},
    {"SCALING_FACTOR_WVAPOUR",                             8, EDT_Float32,     1},
    {"OFFSET_CL_OPT_THICK",                               12, EDT_Float32,     1},
    {"OFFSET_CLOUD_TOP_PRESS",                            16, EDT_Float32,     1},
    {"OFFSET_WVAPOUR",                                    20, EDT_Float32,     1},
    /*{"SPARE_1",                                         24, EDT_UByte,      52},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};

static const EnvisatFieldDescr MERIS_2P_V_SCALING_FACTOR_GADS[] = {
    {"SCALING_FACTOR_TOA_VEGETATION_INDEX",                0, EDT_Float32,     1},
    {"SCALING_FACTOR_BOA_VEGETATION_INDEX",                4, EDT_Float32,     1},
    {"OFFSET_TOA_VEGETAYION_INDEX",                        8, EDT_Float32,     1},
    {"OFFSET_BOA_VEGETAYION_INDEX",                       12, EDT_Float32,     1},
    /*{"SPARE_1",                                         16, EDT_UByte,      60},*/
    {NULL,                                                 0, EDT_Unknown,     0}
};


static const EnvisatRecordDescr aASAR_Records[] =  {
    {"MDS1 ANTENNA ELEV PATT ADS", ASAR_ANTENNA_ELEV_PATT_ADSR},
    {"MDS2 ANTENNA ELEV PATT ADS", ASAR_ANTENNA_ELEV_PATT_ADSR},
    {"CHIRP PARAMS ADS", ASAR_CHIRP_PARAMS_ADSR},
    {"DOP CENTROID COEFFS ADS", ASAR_DOP_CENTROID_COEFFS_ADSR},
    /*{"GEOLOCATION GRID ADS", ASAR_GEOLOCATION_GRID_ADSR},*/
    {"MAIN PROCESSING PARAMS ADS", ASAR_MAIN_PROCESSING_PARAMS_ADSR},
    {"MAP PROJECTION GADS", ASAR_MAP_PROJECTION_GADS},
    {"MDS1 SQ ADS", ASAR_SQ_ADSR},
    {"MDS2 SQ ADS", ASAR_SQ_ADSR},
    {"SR GR ADS", ASAR_SR_GR_ADSR},
    /* WAVE */
    /*{"GEOLOCATION ADS", ASAR_GEOLOCATION_ADSR},*/
    {"PROCESSING PARAMS ADS", ASAR_PROCESSING_PARAMS_ADSR},
    {"SQ ADS", ASAR_WAVE_SQ_ADSR},
    {NULL, NULL}
};

static const EnvisatRecordDescr aMERIS_1P_Records[] =  {
    {"Quality ADS", MERIS_1P_QUALITY_ADSR},
    {"Scaling Factor GADS", MERIS_1P_SCALING_FACTOR_GADS},
    {NULL, NULL}
};

static const EnvisatRecordDescr aMERIS_2P_Records[] =  {
    {"Quality ADS", MERIS_2P_QUALITY_ADSR},
    {"Scaling Factor GADS", MERIS_2P_SCALING_FACTOR_GADS},
    {NULL, NULL}
};

static const EnvisatRecordDescr aMERIS_2P_C_Records[] =  {
    {"Quality ADS", MERIS_2P_QUALITY_ADSR},
    {"Scaling Factor GADS", MERIS_2P_C_SCALING_FACTOR_GADS},
    {NULL, NULL}
};

static const EnvisatRecordDescr aMERIS_2P_V_Records[] =  {
    {"Quality ADS", MERIS_2P_QUALITY_ADSR},
    {"Scaling Factor GADS", MERIS_2P_V_SCALING_FACTOR_GADS},
    {NULL, NULL}
};

const EnvisatRecordDescr* EnvisatFile_GetRecordDescriptor(
                        const char* pszProduct, const char* pszDataset)
{
    const EnvisatRecordDescr *paRecords = NULL;
    const EnvisatRecordDescr *pRecordDescr = NULL;
    int nLen;

    if( STARTS_WITH_CI(pszProduct, "ASA") )
        paRecords = aASAR_Records;
    else if( STARTS_WITH_CI(pszProduct, "MER") )
    {
        if ( STARTS_WITH_CI(pszProduct + 6, "C_2P") )
            paRecords = aMERIS_2P_C_Records;
        else if ( STARTS_WITH_CI(pszProduct + 6, "V_2P") )
            paRecords = aMERIS_2P_V_Records;
        else if ( STARTS_WITH_CI(pszProduct + 8, "1P") )
            paRecords = aMERIS_1P_Records;
        else if ( STARTS_WITH_CI(pszProduct + 8, "2P") )
            paRecords = aMERIS_2P_Records;
        else
            return NULL;
    }
    else if( STARTS_WITH_CI(pszProduct, "SAR") )
        /* ERS products in ENVISAT format have the same records of ASAR ones */
        paRecords = aASAR_Records;
    else
        return NULL;

    /* strip trailing spaces */
    for( nLen = (int)strlen(pszDataset); nLen && pszDataset[nLen-1] == ' '; --nLen );

    pRecordDescr = paRecords;
    while ( pRecordDescr->szName != NULL )
    {
        if ( EQUALN(pRecordDescr->szName, pszDataset, nLen) )
            return pRecordDescr;
        else
            ++pRecordDescr;
    }

    return NULL;
}

CPLErr EnvisatFile_GetFieldAsString(const void *pRecord, int nRecLen,
                        const EnvisatFieldDescr* pField, char *szBuf, size_t nBufLen)
{
    int ret;
    int i, nOffset = 0;
    const GByte *pData;

    if ( pField->nOffset >= nRecLen )
    {
        CPLDebug( "EnvisatDataset",
                  "Field offset (%d) is greater than the record length (%d).",
                  pField->nOffset, nRecLen );

        return CE_Failure;
    }

    pData = (const GByte*)pRecord + pField->nOffset;

    szBuf[0] = '\0';

    switch (pField->eType)
    {
        case EDT_Char:
            memcpy((void*)szBuf, pData, pField->nCount);
            szBuf[pField->nCount] = '\0';
            break;
        case EDT_UByte:
        case EDT_SByte:
            for (i = 0; i < pField->nCount; ++i)
            {
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                ret = snprintf(szBuf + nOffset, nBufLen -nOffset, "%d",
                                   ((const char*)pData)[i]);
                if( ret < 0 || ret >= (int)nBufLen - nOffset )
                    return CE_Failure;
                nOffset += ret;
            }
            break;
        case EDT_Int16:
            for (i = 0; i < pField->nCount; ++i)
            {
                GInt16 nVal;
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                memcpy(&nVal, pData + i * sizeof(nVal), sizeof(nVal));
                ret = snprintf(szBuf + nOffset, nBufLen -nOffset, "%d",
                                   CPL_MSBWORD16(nVal));
                if( ret < 0 || ret >= (int)nBufLen -nOffset )
                    return CE_Failure;
                nOffset += ret;
            }
            break;
        case EDT_UInt16:
            for (i = 0; i < pField->nCount; ++i)
            {
                GUInt16 nVal;
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                memcpy(&nVal, pData + i * sizeof(nVal), sizeof(nVal));
                ret = snprintf(szBuf + nOffset, nBufLen -nOffset,"%u",
                                   CPL_MSBWORD16(nVal));
                if( ret < 0 || ret >= (int)nBufLen -nOffset )
                    return CE_Failure;
                nOffset += ret;
            }
            break;
        case EDT_Int32:
            for (i = 0; i < pField->nCount; ++i)
            {
                GInt32 nVal;
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                memcpy(&nVal, pData + i * sizeof(nVal), sizeof(nVal));
                ret = snprintf(szBuf + nOffset, nBufLen -nOffset,"%d",
                                   CPL_MSBWORD32(nVal));
                if( ret < 0 || ret >= (int)nBufLen -nOffset )
                    return CE_Failure;
                nOffset += ret;
            }
            break;
        case EDT_UInt32:
            for (i = 0; i < pField->nCount; ++i)
            {
                GUInt32 nVal;
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                memcpy(&nVal, pData + i * sizeof(nVal), sizeof(nVal));
                ret = snprintf(szBuf + nOffset, nBufLen -nOffset,"%u",
                                   CPL_MSBWORD32(nVal));
                if( ret < 0 || ret >= (int)nBufLen -nOffset )
                    return CE_Failure;
                nOffset += ret;
            }
            break;
        case EDT_Float32:
            for (i = 0; i < pField->nCount; ++i)
            {
                float fValue;
                memcpy(&fValue, pData + i * sizeof(fValue), sizeof(fValue));
#ifdef CPL_LSB
                CPL_SWAP32PTR( &fValue );
#endif

                if (i > 0)
                    szBuf[nOffset++] = ' ';
                ret = CPLsnprintf(szBuf + nOffset, nBufLen -nOffset,"%f", fValue);
                if( ret < 0 || ret >= (int)nBufLen -nOffset )
                    return CE_Failure;
                nOffset += ret;
            }
            break;
        case EDT_Float64:
            for (i = 0; i < pField->nCount; ++i)
            {
                double dfValue;
                memcpy(&dfValue, pData + i * sizeof(dfValue), sizeof(dfValue));
#ifdef CPL_LSB
                CPL_SWAPDOUBLE( &dfValue );
#endif
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                ret = CPLsnprintf(szBuf + nOffset, nBufLen -nOffset,"%f", dfValue);
                if( ret < 0 || ret >= (int)nBufLen -nOffset )
                    return CE_Failure;
                nOffset += ret;
            }
            break;
/*
        case EDT_CInt16:
            for (i = 0; i < pField->nCount; ++i)
            {
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                nOffset += sprintf(szBuf + nOffset, "(%d, %d)",
                                CPL_MSBWORD16(((const GInt16*)pData)[2 * i]),
                                CPL_MSBWORD16(((const GInt16*)pData)[2 * i+1]));
            }
            break;
        case EDT_CInt32:
            for (i = 0; i < pField->nCount; ++i)
            {
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                nOffset += sprintf(szBuf + nOffset, "(%d, %d)",
                                CPL_MSBWORD32(((const GInt32*)pData)[2 * i]),
                                CPL_MSBWORD32(((const GInt32*)pData)[2 * i+1]));
            }
            break;
        case EDT_CFloat32:
            for (i = 0; i < pField->nCount; ++i)
            {
                float fReal = ((const float*)pData)[2 * i];
                float fImag = ((const float*)pData)[2 * i + 1];
#ifdef CPL_LSB
                CPL_SWAP32PTR( &fReal );
                CPL_SWAP32PTR( &fImag );
#endif
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                nOffset += CPLsprintf(szBuf + nOffset, "(%f, %f)", fReal, fImag);
            }
            break;
        case EDT_CFloat64:
            for (i = 0; i < pField->nCount; ++i)
            {
                double dfReal = ((const double*)pData)[2 * i];
                double dfImag = ((const double*)pData)[2 * i + 1];
#ifdef CPL_LSB
                CPL_SWAPDOUBLE( &dfReal );
                CPL_SWAPDOUBLE( &dfImag );
#endif
                if (i > 0)
                    szBuf[nOffset++] = ' ';
                nOffset += CPLsprintf(szBuf + nOffset, "(%f, %f)", dfReal, dfImag);
            }
            break;
*/
        case EDT_MJD:
            CPLAssert(pField->nCount == 1);
            {
                GInt32 days;
                GUInt32 seconds, microseconds;

                days = CPL_MSBWORD32(((const GInt32*)pData)[0]);
                seconds = CPL_MSBWORD32(((const GUInt32*)pData)[1]);
                microseconds = CPL_MSBWORD32(((const GUInt32*)pData)[2]);

                ret = snprintf(szBuf, nBufLen, "%d, %u, %u", days, seconds, microseconds);
                if( ret < 0 || ret >= (int)nBufLen )
                    return CE_Failure;
            }
            break;
        default:
            CPLDebug( "EnvisatDataset",
                      "Unabe to convert '%s' field to string: "
                      "unexpected data type '%d'.",
                      pField->szName, pField->eType );
            return CE_Failure;
    }

    return CE_None;
}
