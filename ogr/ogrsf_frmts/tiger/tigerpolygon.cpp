/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPolygon, providing access to .RTA files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_tiger.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

static const TigerFieldInfo rtA_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "STATECU",    'L', 'N', OFTInteger,   26,  27,   2,       1,   1,     1 },
  { "COUNTYCU",   'L', 'N', OFTInteger,   28,  30,   3,       1,   1,     1 },

  { "TRACT",      'L', 'N', OFTInteger,   31,  36,   6,       1,   1,     1 },
  { "BLOCK",      'L', 'N', OFTInteger,   37,  40,   4,       1,   1,     1 },
  { "BLOCKSUFCU", 'L', 'A', OFTString,    41,  41,   1,       1,   1,     1 },

  { "RS_A1",      'L', 'A', OFTString,    42,  42,   1,       1,   1,     1 },
  { "AIANHHFPCU", 'L', 'N', OFTInteger,   43,  47,   5,       1,   1,     1 },
  { "AIANHHCU",   'L', 'N', OFTInteger,   48,  51,   4,       1,   1,     1 },
  { "AIHHTLICU",  'L', 'A', OFTString,    52,  52,   1,       1,   1,     1 },
  { "ANRCCU",     'L', 'N', OFTInteger,   53,  57,   5,       1,   1,     1 },
  { "AITSCECU",   'L', 'N', OFTInteger,   58,  60,   3,       1,   1,     1 },
  { "AITSCU",     'L', 'N', OFTInteger,   61,  65,   5,       1,   1,     1 },
  { "CONCITCU",   'L', 'N', OFTInteger,   66,  70,   5,       1,   1,     1 },
  { "COUSUBCU",   'L', 'N', OFTInteger,   71,  75,   5,       1,   1,     1 },
  { "SUBMCDCU",   'L', 'N', OFTInteger,   76,  80,   5,       1,   1,     1 },
  { "PLACECU",    'L', 'N', OFTInteger,   81,  85,   5,       1,   1,     1 },
  { "SDELMCU",    'L', 'A', OFTString,    86,  90,   5,       1,   1,     1 },
  { "SDSECCU",    'L', 'A', OFTString,    91,  95,   5,       1,   1,     1 },
  { "SDUNICU",    'L', 'A', OFTString,    96, 100,   5,       1,   1,     1 },
  { "MSACMSACU",  'L', 'N', OFTInteger,  101, 104,   4,       1,   1,     1 },
  { "PMSACU",     'L', 'N', OFTInteger,  105, 108,   4,       1,   1,     1 },
  { "NECMACU",    'L', 'N', OFTInteger,  109, 112,   4,       1,   1,     1 },
  { "CDCU",       'R', 'N', OFTInteger,  113, 114,   2,       1,   1,     1 },
  { "RS_A2",      'L', 'A', OFTString,   115, 119,   5,       1,   1,     1 },
  { "RS_A3",      'R', 'A', OFTString,   120, 122,   3,       1,   1,     1 },
  { "RS_A4",      'R', 'A', OFTString,   123, 128,   6,       1,   1,     1 },
  { "RS_A5",      'R', 'A', OFTString,   129, 131,   3,       1,   1,     1 },
  { "RS_A6",      'R', 'A', OFTString,   132, 134,   3,       1,   1,     1 },
  { "RS_A7",      'R', 'A', OFTString,   135, 139,   5,       1,   1,     1 },
  { "RS_A8",      'R', 'A', OFTString,   140, 145,   6,       1,   1,     1 },
  { "RS_A9",      'L', 'A', OFTString,   146, 151,   6,       1,   1,     1 },
  { "RS_A10",     'L', 'A', OFTString,   152, 157,   6,       1,   1,     1 },
  { "RS_A11",     'L', 'A', OFTString,   158, 163,   6,       1,   1,     1 },
  { "RS_A12",     'L', 'A', OFTString,   164, 169,   6,       1,   1,     1 },
  { "RS_A13",     'L', 'A', OFTString,   170, 175,   6,       1,   1,     1 },
  { "RS_A14",     'L', 'A', OFTString,   176, 181,   6,       1,   1,     1 },
  { "RS_A15",     'L', 'A', OFTString,   182, 186,   5,       1,   1,     1 },
  { "RS_A16",     'L', 'A', OFTString,   187, 187,   1,       1,   1,     1 },
  { "RS_A17",     'L', 'A', OFTString,   188, 193,   6,       1,   1,     1 },
  { "RS_A18",     'L', 'A', OFTString,   194, 199,   6,       1,   1,     1 },
  { "RS_A19",     'L', 'A', OFTString,   200, 210,  11,       1,   1,     1 },
};
static const TigerRecordInfo rtA_2002_info =
  {
    rtA_2002_fields,
    sizeof(rtA_2002_fields) / sizeof(TigerFieldInfo),
    210
  };


static const TigerFieldInfo rtA_2003_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "STATECU",    'L', 'N', OFTInteger,   26,  27,   2,       1,   1,     1 },
  { "COUNTYCU",   'L', 'N', OFTInteger,   28,  30,   3,       1,   1,     1 },

  { "TRACT",      'L', 'N', OFTInteger,   31,  36,   6,       1,   1,     1 },
  { "BLOCK",      'L', 'N', OFTInteger,   37,  40,   4,       1,   1,     1 },
  { "BLOCKSUFCU", 'L', 'A', OFTString,    41,  41,   1,       1,   1,     1 },

  { "RS_A1",      'L', 'A', OFTString,    42,  42,   1,       1,   1,     1 },
  { "AIANHHFPCU", 'L', 'N', OFTInteger,   43,  47,   5,       1,   1,     1 },
  { "AIANHHCU",   'L', 'N', OFTInteger,   48,  51,   4,       1,   1,     1 },
  { "AIHHTLICU",  'L', 'A', OFTString,    52,  52,   1,       1,   1,     1 },
  { "ANRCCU",     'L', 'N', OFTInteger,   53,  57,   5,       1,   1,     1 },
  { "AITSCECU",   'L', 'N', OFTInteger,   58,  60,   3,       1,   1,     1 },
  { "AITSCU",     'L', 'N', OFTInteger,   61,  65,   5,       1,   1,     1 },
  { "CONCITCU",   'L', 'N', OFTInteger,   66,  70,   5,       1,   1,     1 },
  { "COUSUBCU",   'L', 'N', OFTInteger,   71,  75,   5,       1,   1,     1 },
  { "SUBMCDCU",   'L', 'N', OFTInteger,   76,  80,   5,       1,   1,     1 },
  { "PLACECU",    'L', 'N', OFTInteger,   81,  85,   5,       1,   1,     1 },
  { "SDELMCU",    'L', 'A', OFTString,    86,  90,   5,       1,   1,     1 },
  { "SDSECCU",    'L', 'A', OFTString,    91,  95,   5,       1,   1,     1 },
  { "SDUNICU",    'L', 'A', OFTString,    96, 100,   5,       1,   1,     1 },
  { "RS_A20",     'L', 'A', OFTString,   101, 104,   4,       1,   1,     1 },
  { "RS_A21",     'L', 'A', OFTString,   105, 108,   4,       1,   1,     1 },
  { "RS_A22",     'L', 'A', OFTString,   109, 112,   4,       1,   1,     1 },
  { "CDCU",       'R', 'N', OFTInteger,  113, 114,   2,       1,   1,     1 },
  { "ZCTA5CU",    'L', 'A', OFTString,   115, 119,   5,       1,   1,     1 },
  { "ZCTA3CU",    'R', 'A', OFTString,   120, 122,   3,       1,   1,     1 },
  { "RS_A4",      'R', 'A', OFTString,   123, 128,   6,       1,   1,     1 },
  { "RS_A5",      'R', 'A', OFTString,   129, 131,   3,       1,   1,     1 },
  { "RS_A6",      'R', 'A', OFTString,   132, 134,   3,       1,   1,     1 },
  { "RS_A7",      'R', 'A', OFTString,   135, 139,   5,       1,   1,     1 },
  { "RS_A8",      'R', 'A', OFTString,   140, 145,   6,       1,   1,     1 },
  { "RS_A9",      'L', 'A', OFTString,   146, 151,   6,       1,   1,     1 },
  { "CBSACU",     'L', 'A', OFTInteger,  152, 156,   5,       1,   1,     1 },
  { "CSACU",      'L', 'A', OFTInteger,  157, 159,   3,       1,   1,     1 },
  { "NECTACU",    'L', 'A', OFTInteger,  160, 164,   5,       1,   1,     1 },
  { "CNECTACU",   'L', 'A', OFTInteger,  165, 167,   3,       1,   1,     1 },
  { "METDIVCU",   'L', 'A', OFTInteger,  168, 172,   5,       1,   1,     1 },
  { "NECTADIVCU", 'L', 'A', OFTInteger,  173, 177,   5,       1,   1,     1 },
  { "RS_A14",     'L', 'A', OFTString,   178, 181,   4,       1,   1,     1 },
  { "RS_A15",     'L', 'A', OFTString,   182, 186,   5,       1,   1,     1 },
  { "RS_A16",     'L', 'A', OFTString,   187, 187,   1,       1,   1,     1 },
  { "RS_A17",     'L', 'A', OFTString,   188, 193,   6,       1,   1,     1 },
  { "RS_A18",     'L', 'A', OFTString,   194, 199,   6,       1,   1,     1 },
  { "RS_A19",     'L', 'A', OFTString,   200, 210,  11,       1,   1,     1 },
};
static const TigerRecordInfo rtA_2003_info =
  {
    rtA_2003_fields,
    sizeof(rtA_2003_fields) / sizeof(TigerFieldInfo),
    210
  };


static const TigerFieldInfo rtA_2004_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "STATECU",    'L', 'N', OFTInteger,   26,  27,   2,       1,   1,     1 },
  { "COUNTYCU",   'L', 'N', OFTInteger,   28,  30,   3,       1,   1,     1 },

  { "TRACT",      'L', 'N', OFTInteger,   31,  36,   6,       1,   1,     1 },
  { "BLOCK",      'L', 'N', OFTInteger,   37,  40,   4,       1,   1,     1 },
  { "BLOCKSUFCU", 'L', 'A', OFTString,    41,  41,   1,       1,   1,     1 },

  { "RS_A1",      'L', 'A', OFTString,    42,  42,   1,       1,   1,     1 },
  { "AIANHHFPCU", 'L', 'N', OFTInteger,   43,  47,   5,       1,   1,     1 },
  { "AIANHHCU",   'L', 'N', OFTInteger,   48,  51,   4,       1,   1,     1 },
  { "AIHHTLICU",  'L', 'A', OFTString,    52,  52,   1,       1,   1,     1 },
  { "ANRCCU",     'L', 'N', OFTInteger,   53,  57,   5,       1,   1,     1 },
  { "AITSCECU",   'L', 'N', OFTInteger,   58,  60,   3,       1,   1,     1 },
  { "AITSCU",     'L', 'N', OFTInteger,   61,  65,   5,       1,   1,     1 },
  { "CONCITCU",   'L', 'N', OFTInteger,   66,  70,   5,       1,   1,     1 },
  { "COUSUBCU",   'L', 'N', OFTInteger,   71,  75,   5,       1,   1,     1 },
  { "SUBMCDCU",   'L', 'N', OFTInteger,   76,  80,   5,       1,   1,     1 },
  { "PLACECU",    'L', 'N', OFTInteger,   81,  85,   5,       1,   1,     1 },
  { "SDELMCU",    'L', 'A', OFTString,    86,  90,   5,       1,   1,     1 },
  { "SDSECCU",    'L', 'A', OFTString,    91,  95,   5,       1,   1,     1 },
  { "SDUNICU",    'L', 'A', OFTString,    96, 100,   5,       1,   1,     1 },
  { "RS_A20",     'L', 'A', OFTString,   101, 104,   4,       1,   1,     1 },
  { "RS_A21",     'L', 'A', OFTString,   105, 108,   4,       1,   1,     1 },
  { "RS_A22",     'L', 'A', OFTString,   109, 112,   4,       1,   1,     1 },
  { "CDCU",       'R', 'N', OFTInteger,  113, 114,   2,       1,   1,     1 },
  { "ZCTA5CU",    'L', 'A', OFTString,   115, 119,   5,       1,   1,     1 },
  { "ZCTA3CU",    'R', 'A', OFTString,   120, 122,   3,       1,   1,     1 },
  { "RS_A4",      'R', 'A', OFTString,   123, 128,   6,       1,   1,     1 },
  { "RS_A5",      'R', 'A', OFTString,   129, 131,   3,       1,   1,     1 },
  { "RS_A6",      'R', 'A', OFTString,   132, 134,   3,       1,   1,     1 },
  { "RS_A7",      'R', 'A', OFTString,   135, 139,   5,       1,   1,     1 },
  { "RS_A8",      'R', 'A', OFTString,   140, 145,   6,       1,   1,     1 },
  { "RS_A9",      'L', 'A', OFTString,   146, 151,   6,       1,   1,     1 },
  { "CBSACU",     'L', 'A', OFTInteger,  152, 156,   5,       1,   1,     1 },
  { "CSACU",      'L', 'A', OFTInteger,  157, 159,   3,       1,   1,     1 },
  { "NECTACU",    'L', 'A', OFTInteger,  160, 164,   5,       1,   1,     1 },
  { "CNECTACU",   'L', 'A', OFTInteger,  165, 167,   3,       1,   1,     1 },
  { "METDIVCU",   'L', 'A', OFTInteger,  168, 172,   5,       1,   1,     1 },
  { "NECTADIVCU", 'L', 'A', OFTInteger,  173, 177,   5,       1,   1,     1 },
  { "RS_A14",     'L', 'A', OFTString,   178, 181,   4,       1,   1,     1 },
  { "UACU",       'L', 'N', OFTInteger,  182, 186,   5,       1,   1,     1 },
  { "URCU",       'L', 'A', OFTString,   187, 187,   1,       1,   1,     1 },
  { "RS_A17",     'L', 'A', OFTString,   188, 193,   6,       1,   1,     1 },
  { "RS_A18",     'L', 'A', OFTString,   194, 199,   6,       1,   1,     1 },
  { "RS_A19",     'L', 'A', OFTString,   200, 210,  11,       1,   1,     1 },
};
static const TigerRecordInfo rtA_2004_info =
  {
    rtA_2004_fields,
    sizeof(rtA_2004_fields) / sizeof(TigerFieldInfo),
    210
  };


static const TigerFieldInfo rtA_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTString,     6,  10,   5,       1,   1,     1 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "FAIR",       'L', 'N', OFTInteger,   26,  30,   5,       1,   1,     1 },
  { "FMCD",       'L', 'N', OFTInteger,   31,  35,   5,       1,   1,     1 },
  { "FPL",        'L', 'N', OFTInteger,   36,  40,   5,       1,   1,     1 },
  { "CTBNA90",    'L', 'N', OFTInteger,   41,  46,   6,       1,   1,     1 },
  { "BLK90",      'L', 'A', OFTString,    47,  50,   4,       1,   1,     1 },
  { "CD106",      'L', 'N', OFTInteger,   51,  52,   2,       1,   1,     1 },
  { "CD108",      'L', 'N', OFTInteger,   53,  54,   2,       1,   1,     1 },
  { "SDELM",      'L', 'A', OFTString,    55,  59,   5,       1,   1,     1 },
  { "SDSEC",      'L', 'N', OFTString,    65,  69,   5,       1,   1,     1 },
  { "SDUNI",      'L', 'A', OFTString,    70,  74,   5,       1,   1,     1 },
  { "TAZ",        'R', 'A', OFTString,    75,  80,   6,       1,   1,     1 },
  { "UA",         'L', 'N', OFTInteger,   81,  84,   4,       1,   1,     1 },
  { "URBFLAG",    'L', 'A', OFTString,    85,  85,   1,       1,   1,     1 },
  { "CTPP",       'L', 'A', OFTString,    86,  89,   4,       1,   1,     1 },
  { "STATE90",    'L', 'N', OFTInteger,   90,  91,   2,       1,   1,     1 },
  { "COUN90",     'L', 'N', OFTInteger,   92,  94,   3,       1,   1,     1 },
  { "AIR90",      'L', 'N', OFTInteger,   95,  98,   4,       1,   1,     1 }
};

static const TigerRecordInfo rtA_info =
  {
    rtA_fields,
    sizeof(rtA_fields) / sizeof(TigerFieldInfo),
    98
  };


static const TigerFieldInfo rtS_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       0,   0,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       0,   0,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       0,   0,     1 },
  { "STATE",      'L', 'N', OFTInteger,   26,  27,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,   28,  30,   3,       1,   1,     1 },
  { "TRACT",      'L', 'N', OFTInteger,   31,  36,   6,       0,   0,     1 },
  { "BLOCK",      'L', 'N', OFTInteger,   37,  40,   4,       0,   0,     1 },
  { "BLKGRP",     'L', 'N', OFTInteger,   41,  41,   1,       1,   1,     1 },
  { "AIANHHFP",   'L', 'N', OFTInteger,   42,  46,   5,       1,   1,     1 },
  { "AIANHH",     'L', 'N', OFTInteger,   47,  50,   4,       1,   1,     1 },
  { "AIHHTLI",    'L', 'A', OFTString,    51,  51,   1,       1,   1,     1 },
  { "ANRC",       'L', 'N', OFTInteger,   52,  56,   5,       1,   1,     1 },
  { "AITSCE",     'L', 'N', OFTInteger,   57,  59,   3,       1,   1,     1 },
  { "AITS",       'L', 'N', OFTInteger,   60,  64,   5,       1,   1,     1 },
  { "CONCIT",     'L', 'N', OFTInteger,   65,  69,   5,       1,   1,     1 },
  { "COUSUB",     'L', 'N', OFTInteger,   70,  74,   5,       1,   1,     1 },
  { "SUBMCD",     'L', 'N', OFTInteger,   75,  79,   5,       1,   1,     1 },
  { "PLACE",      'L', 'N', OFTInteger,   80,  84,   5,       1,   1,     1 },
  { "SDELM",      'L', 'N', OFTInteger,   85,  89,   5,       1,   1,     1 },
  { "SDSEC",      'L', 'N', OFTInteger,   90,  94,   5,       1,   1,     1 },
  { "SDUNI",      'L', 'N', OFTInteger,   95,  99,   5,       1,   1,     1 },
  { "MSACMSA",    'L', 'N', OFTInteger,  100, 103,   4,       1,   1,     1 },
  { "PMSA",       'L', 'N', OFTInteger,  104, 107,   4,       1,   1,     1 },
  { "NECMA",      'L', 'N', OFTInteger,  108, 111,   4,       1,   1,     1 },
  { "CD106",      'L', 'N', OFTInteger,  112, 113,   2,       1,   1,     1 },
  // Note: spec has CD106 with 'R', but sample data file (08005) seems to
  // have been written with 'L', so I'm using 'L' here.  mbp Tue Dec 24 19:03:40 2002
  { "CD108",      'R', 'N', OFTInteger,  114, 115,   2,       1,   1,     1 },
  { "PUMA5",      'L', 'N', OFTInteger,  116, 120,   5,       1,   1,     1 },
  { "PUMA1",      'L', 'N', OFTInteger,  121, 125,   5,       1,   1,     1 },
  { "ZCTA5",      'L', 'A', OFTString,   126, 130,   5,       1,   1,     1 },
  { "ZCTA3",      'L', 'A', OFTString,   131, 133,   3,       1,   1,     1 },
  { "TAZ",        'L', 'A', OFTString,   134, 139,   6,       1,   1,     1 },
  { "TAZCOMB",    'L', 'A', OFTString,   140, 145,   6,       1,   1,     1 },
  { "UA",         'L', 'N', OFTInteger,  146, 150,   5,       1,   1,     1 },
  { "UR",         'L', 'A', OFTString,   151, 151,   1,       1,   1,     1 },
  { "VTD",        'R', 'A', OFTString,   152, 157,   6,       1,   1,     1 },
  { "SLDU",       'R', 'A', OFTString,   158, 160,   3,       1,   1,     1 },
  { "SLDL",       'R', 'A', OFTString,   161, 163,   3,       1,   1,     1 },
  { "UGA",        'L', 'A', OFTString,   164, 168,   5,       1,   1,     1 },
};
static const TigerRecordInfo rtS_2002_info =
  {
    rtS_2002_fields,
    sizeof(rtS_2002_fields) / sizeof(TigerFieldInfo),
    168
  };


static const TigerFieldInfo rtS_2000_Redistricting_fields[] = {
  { "FILE",       'L', 'N', OFTString,     6,  10,   5,       0,   0,     1 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       0,   0,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       0,   0,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       0,   0,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       0,   0,     1 },
  { "WATER",      'L', 'N', OFTString,    26,  26,   1,       1,   1,     1 },
  { "CMSAMSA",    'L', 'N', OFTInteger,   27,  30,   4,       1,   1,     1 },
  { "PMSA",       'L', 'N', OFTInteger,   31,  34,   4,       1,   1,     1 },
  { "AIANHH",     'L', 'N', OFTInteger,   35,  39,   5,       1,   1,     1 },
  { "AIR",        'L', 'N', OFTInteger,   40,  43,   4,       1,   1,     1 },
  { "TRUST",      'L', 'A', OFTString,    44,  44,   1,       1,   1,     1 },
  { "ANRC",       'L', 'A', OFTInteger,   45,  46,   2,       1,   1,     1 },
  { "STATECU",    'L', 'N', OFTInteger,   47,  48,   2,       1,   1,     1 },
  { "COUNTYCU",   'L', 'N', OFTInteger,   49,  51,   3,       1,   1,     1 },
  { "FCCITY",     'L', 'N', OFTInteger,   52,  56,   5,       1,   1,     1 },
  { "FMCD",       'L', 'N', OFTInteger,   57,  61,   5,       0,   0,     1 },
  { "FSMCD",      'L', 'N', OFTInteger,   62,  66,   5,       1,   1,     1 },
  { "PLACE",      'L', 'N', OFTInteger,   67,  71,   5,       1,   1,     1 },
  { "CTBNA00",    'L', 'N', OFTInteger,   72,  77,   6,       1,   1,     1 },
  { "BLK00",      'L', 'N', OFTString,    78,  81,   4,       1,   1,     1 },
  { "RS10",       'R', 'N', OFTInteger,   82,  82,   0,       0,   1,     1 },
  { "CDCU",       'L', 'N', OFTInteger,   83,  84,   2,       1,   1,     1 },

  { "SLDU",       'R', 'A', OFTString,    85,  87,   3,       1,   1,     1 },
  { "SLDL",       'R', 'A', OFTString,    88,  90,   3,       1,   1,     1 },
  { "UGA",        'L', 'A', OFTString,    91,  95,   5,       1,   1,     1 },
  { "BLKGRP",     'L', 'N', OFTInteger,   96,  96,   1,       1,   1,     1 },
  { "VTD",        'R', 'A', OFTString,    97, 102,   6,       1,   1,     1 },
  { "STATECOL",   'L', 'N', OFTInteger,  103, 104,   2,       1,   1,     1 },
  { "COUNTYCOL",  'L', 'N', OFTInteger,  105, 107,   3,       1,   1,     1 },
  { "BLOCKCOL",   'R', 'N', OFTInteger,  108, 112,   5,       1,   1,     1 },
  { "BLKSUFCOL",  'L', 'A', OFTString,   113, 113,   1,       1,   1,     1 },
  { "ZCTA5",      'L', 'A', OFTString,   114, 118,   5,       1,   1,     1 }

};

static const TigerRecordInfo rtS_2000_Redistricting_info =
  {
    rtS_2000_Redistricting_fields,
    sizeof(rtS_2000_Redistricting_fields) / sizeof(TigerFieldInfo),
    120
  };

static const TigerFieldInfo rtS_fields[] = {
  { "FILE",       'L', 'N', OFTString,     6,  10,   5,       0,   0,     1 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       0,   0,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       0,   0,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       0,   0,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       0,   0,     1 },

  { "WATER",      'L', 'N', OFTString,    26,  26,   1,       1,   1,     1 },
  { "CMSAMSA",    'L', 'N', OFTInteger,   27,  30,   4,       1,   1,     1 },
  { "PMSA",       'L', 'N', OFTInteger,   31,  34,   4,       1,   1,     1 },
  { "AIANHH",     'L', 'N', OFTInteger,   35,  39,   5,       1,   1,     1 },
  { "AIR",        'L', 'N', OFTInteger,   40,  43,   4,       1,   1,     1 },
  { "TRUST",      'L', 'A', OFTString,    44,  44,   1,       1,   1,     1 },
  { "ANRC",       'L', 'A', OFTInteger,   45,  46,   2,       1,   1,     1 },
  { "STATECU",    'L', 'N', OFTInteger,   47,  48,   2,       1,   1,     1 },
  { "COUNTYCU",   'L', 'N', OFTInteger,   49,  51,   3,       1,   1,     1 },
  { "FCCITY",     'L', 'N', OFTInteger,   52,  56,   5,       1,   1,     1 },
  { "FMCD",       'L', 'N', OFTInteger,   57,  61,   5,       0,   0,     1 },
  { "FSMCD",      'L', 'N', OFTInteger,   62,  66,   5,       1,   1,     1 },
  { "PLACE",      'L', 'N', OFTInteger,   67,  71,   5,       1,   1,     1 },
  { "CTBNA00",    'L', 'N', OFTInteger,   72,  77,   6,       1,   1,     1 },
  { "BLK00",      'L', 'N', OFTString,    78,  81,   4,       1,   1,     1 },
  { "RS10",       'R', 'N', OFTInteger,   82,  82,   0,       0,   1,     1 },
  { "CDCU",       'L', 'N', OFTInteger,   83,  84,   2,       1,   1,     1 },

  { "STSENATE",   'L', 'A', OFTString,    85,  90,   6,       1,   1,     1 },
  { "STHOUSE",    'L', 'A', OFTString,    91,  96,   6,       1,   1,     1 },
  { "VTD00",      'L', 'A', OFTString,    97, 102,   6,       1,   1,     1 }
};
static const TigerRecordInfo rtS_info =
  {
    rtS_fields,
    sizeof(rtS_fields) / sizeof(TigerFieldInfo),
    120
  };

/************************************************************************/
/*                            TigerPolygon()                            */
/************************************************************************/

TigerPolygon::TigerPolygon( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "Polygon" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    fpRTS = NULL;
    bUsingRTS = TRUE;

    if( poDS->GetVersion() >= TIGER_2004 ) {
        psRTAInfo = &rtA_2004_info;
    } else if( poDS->GetVersion() >= TIGER_2003 ) {
        psRTAInfo = &rtA_2003_info;
    } else if( poDS->GetVersion() >= TIGER_2002 ) {
        psRTAInfo = &rtA_2002_info;
    } else {
        psRTAInfo = &rtA_info;
    }

    if( poDS->GetVersion() >= TIGER_2002 ) {
      psRTSInfo = &rtS_2002_info;
    } else if( poDS->GetVersion() >= TIGER_2000_Redistricting ) {
      psRTSInfo = &rtS_2000_Redistricting_info;
    } else {
      psRTSInfo = &rtS_info;
    }

    /* -------------------------------------------------------------------- */
    /*      Fields from type A record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTAInfo, poFeatureDefn);

    /* -------------------------------------------------------------------- */
    /*      Add the RTS records if it is available.                         */
    /* -------------------------------------------------------------------- */

    if( bUsingRTS ) {
      AddFieldDefns(psRTSInfo, poFeatureDefn);
    }
}

/************************************************************************/
/*                           ~TigerPolygon()                            */
/************************************************************************/

TigerPolygon::~TigerPolygon()

{
    if( fpRTS != NULL )
        VSIFCloseL( fpRTS );
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerPolygon::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, "A" ) )
        return FALSE;

    EstablishFeatureCount();
    
/* -------------------------------------------------------------------- */
/*      Open the RTS file                                               */
/* -------------------------------------------------------------------- */
    if( bUsingRTS )
    {
        if( fpRTS != NULL )
        {
            VSIFCloseL( fpRTS );
            fpRTS = NULL;
        }

        if( pszModule )
        {
            char        *pszFilename;
        
            pszFilename = poDS->BuildFilename( pszModule, "S" );

            fpRTS = VSIFOpenL( pszFilename, "rb" );

            CPLFree( pszFilename );

            nRTSRecLen = EstablishRecordLength( fpRTS );
        }
    }
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerPolygon::GetFeature( int nRecordId )

{
  char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %sA",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the raw record data from the file.                         */
/* -------------------------------------------------------------------- */
    if( fpPrimary == NULL )
        return NULL;

    if( VSIFSeekL( fpPrimary, nRecordId * nRecordLength, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to %d of %sA",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFReadL( achRecord, nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %sA",
                  nRecordId, pszModule );
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Set fields.                                                     */
    /* -------------------------------------------------------------------- */

    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRTAInfo, poFeature, achRecord );

    /* -------------------------------------------------------------------- */
    /*      Read RTS record, and apply fields.                              */
    /* -------------------------------------------------------------------- */

    if( fpRTS != NULL )
    {
        char    achRTSRec[OGR_TIGER_RECBUF_LEN];

        if( VSIFSeekL( fpRTS, nRecordId * nRTSRecLen, SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %sS",
                      nRecordId * nRTSRecLen, pszModule );
            return NULL;
        }

        if( VSIFReadL( achRTSRec, psRTSInfo->nRecordLength, 1, fpRTS ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read record %d of %sS",
                      nRecordId, pszModule );
            return NULL;
        }

        SetFields( psRTSInfo, poFeature, achRTSRec );

    }
    
    return poFeature;
}

/************************************************************************/
/*                           SetWriteModule()                           */
/************************************************************************/

int TigerPolygon::SetWriteModule( const char *pszFileCode, int nRecLen, 
                                  OGRFeature *poFeature )

{
    int bSuccess;

    bSuccess = TigerFileBase::SetWriteModule( pszFileCode, nRecLen, poFeature);
    if( !bSuccess )
        return bSuccess;

/* -------------------------------------------------------------------- */
/*      Open the RT3 file                                               */
/* -------------------------------------------------------------------- */
    if( bUsingRTS )
    {
        if( fpRTS != NULL )
        {
            VSIFCloseL( fpRTS );
            fpRTS = NULL;
        }

        if( pszModule )
        {
            char        *pszFilename;
        
            pszFilename = poDS->BuildFilename( pszModule, "S" );

            fpRTS = VSIFOpenL( pszFilename, "ab" );

            CPLFree( pszFilename );
        }
    }
    
    return TRUE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerPolygon::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[OGR_TIGER_RECBUF_LEN];

/* -------------------------------------------------------------------- */
/*      Write basic data record ("RTA")                                 */
/* -------------------------------------------------------------------- */

    if( !SetWriteModule( "A", psRTAInfo->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', psRTAInfo->nRecordLength );

    WriteFields( psRTAInfo, poFeature, szRecord );
    WriteRecord( szRecord, psRTAInfo->nRecordLength, "A" );

/* -------------------------------------------------------------------- */
/*      Prepare S record.                                               */
/* -------------------------------------------------------------------- */

    memset( szRecord, ' ', psRTSInfo->nRecordLength );

    WriteFields( psRTSInfo, poFeature, szRecord );
    WriteRecord( szRecord, psRTSInfo->nRecordLength, "S", fpRTS );


    return OGRERR_NONE;
}

