/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerZeroCellID, providing access to .RTT files.
 * Author:   Mark Phillips, mbp@geomtech.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam, Mark Phillips
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

#define FILE_CODE       "T"

static const TigerFieldInfo rtT_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "TZID",       'R', 'N', OFTInteger,   11,  20,  10,       1,   1,     1 },
  { "SOURCE",     'L', 'A', OFTString,    21,  30,  10,       1,   1,     1 },
  { "FTRP",       'L', 'A', OFTString,    31,  47,  17,       1,   1,     1 }
};
static const TigerRecordInfo rtT_info =
  {
    rtT_fields,
    sizeof(rtT_fields) / sizeof(TigerFieldInfo),
    47
  };

/************************************************************************/
/*                           TigerZeroCellID()                           */
/************************************************************************/

TigerZeroCellID::TigerZeroCellID( OGRTigerDataSource * poDSIn,
                                  CPL_UNUSED const char * pszPrototypeModule )
  : TigerFileBase(&rtT_info, FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "ZeroCellID" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

/* -------------------------------------------------------------------- */
/*      Fields from type T record.                                      */
/* -------------------------------------------------------------------- */

    AddFieldDefns( psRTInfo, poFeatureDefn );

}
