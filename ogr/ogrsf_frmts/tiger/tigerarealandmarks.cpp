/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerAreaLandmarks, providing access to .RT8 files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

CPL_CVSID("$Id$")

static const char FILE_CODE[] = "8";

static const TigerFieldInfo rt8_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0 },
  { "FILE",       'L', 'N', OFTString,     6,  10,   5,       1,   1 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1 },
  { "LAND",       'R', 'N', OFTInteger,   26,  35,  10,       1,   1 }
};

static const TigerRecordInfo rt8_info =
  {
    rt8_fields,
    sizeof(rt8_fields) / sizeof(TigerFieldInfo),
    36
  };

/************************************************************************/
/*                         TigerAreaLandmarks()                         */
/************************************************************************/

TigerAreaLandmarks::TigerAreaLandmarks( OGRTigerDataSource * poDSIn,
                                        CPL_UNUSED const char * pszPrototypeModule ) :
    TigerFileBase(&rt8_info, FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "AreaLandmarks" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    /* -------------------------------------------------------------------- */
    /*      Fields from type 8 record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns( psRTInfo, poFeatureDefn );
}
