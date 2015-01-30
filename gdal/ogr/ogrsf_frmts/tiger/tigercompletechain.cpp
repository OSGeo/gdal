/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerCompleteChain, providing access to RT1 and
 *           related files.
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

CPL_CVSID("$Id$");

static const TigerFieldInfo rt1_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "TLID",       'R', 'N', OFTInteger,    6,  15,  10,       1,   1,     1 },
  { "SIDE1",      'R', 'N', OFTInteger,   16,  16,   1,       1,   1,     1 },
  { "SOURCE",     'L', 'A', OFTString,    17,  17,   1,       1,   1,     1 },
  { "FEDIRP",     'L', 'A', OFTString,    18,  19,   2,       1,   1,     1 },
  { "FENAME",     'L', 'A', OFTString,    20,  49,  30,       1,   1,     1 },
  { "FETYPE",     'L', 'A', OFTString,    50,  53,   4,       1,   1,     1 },
  { "FEDIRS",     'L', 'A', OFTString,    54,  55,   2,       1,   1,     1 },
  { "CFCC",       'L', 'A', OFTString,    56,  58,   3,       1,   1,     1 },
  { "FRADDL",     'R', 'A', OFTString,    59,  69,  11,       1,   1,     1 },
  { "TOADDL",     'R', 'A', OFTString,    70,  80,  11,       1,   1,     1 },
  { "FRADDR",     'R', 'A', OFTString,    81,  91,  11,       1,   1,     1 },
  { "TOADDR",     'R', 'A', OFTString,    92, 102,  11,       1,   1,     1 },
  { "FRIADDL",    'L', 'A', OFTString,   103, 103,   1,       1,   1,     1 },
  { "TOIADDL",    'L', 'A', OFTString,   104, 104,   1,       1,   1,     1 },
  { "FRIADDR",    'L', 'A', OFTString,   105, 105,   1,       1,   1,     1 },
  { "TOIADDR",    'L', 'A', OFTString,   106, 106,   1,       1,   1,     1 },
  { "ZIPL",       'L', 'N', OFTInteger,  107, 111,   5,       1,   1,     1 },
  { "ZIPR",       'L', 'N', OFTInteger,  112, 116,   5,       1,   1,     1 },
  { "AIANHHFPL",  'L', 'N', OFTInteger,  117, 121,   5,       1,   1,     1 },
  { "AIANHHFPR",  'L', 'N', OFTInteger,  122, 126,   5,       1,   1,     1 },
  { "AIHHTLIL",   'L', 'A', OFTString,   127, 127,   1,       1,   1,     1 },
  { "AIHHTLIR",   'L', 'A', OFTString,   128, 128,   1,       1,   1,     1 },
  { "CENSUS1",    'L', 'A', OFTString,   129, 129,   1,       1,   1,     1 },
  { "CENSUS2",    'L', 'A', OFTString,   130, 130,   1,       1,   1,     1 },
  { "STATEL",     'L', 'N', OFTInteger,  131, 132,   2,       1,   1,     1 },
  { "STATER",     'L', 'N', OFTInteger,  133, 134,   2,       1,   1,     1 },
  { "COUNTYL",    'L', 'N', OFTInteger,  135, 137,   3,       1,   1,     1 },
  { "COUNTYR",    'L', 'N', OFTInteger,  138, 140,   3,       1,   1,     1 },

  { "COUSUBL",    'L', 'N', OFTInteger,  141, 145,   5,       1,   1,     1 },
  { "COUSUBR",    'L', 'N', OFTInteger,  146, 150,   5,       1,   1,     1 },
  { "SUBMCDL",    'L', 'N', OFTInteger,  151, 155,   5,       1,   1,     1 },
  { "SUBMCDR",    'L', 'N', OFTInteger,  156, 160,   5,       1,   1,     1 },
  { "PLACEL",     'L', 'N', OFTInteger,  161, 165,   5,       1,   1,     1 },
  { "PLACER",     'L', 'N', OFTInteger,  166, 170,   5,       1,   1,     1 },
  { "TRACTL",     'L', 'N', OFTInteger,  171, 176,   6,       1,   1,     1 },
  { "TRACTR",     'L', 'N', OFTInteger,  177, 182,   6,       1,   1,     1 },
  { "BLOCKL",     'L', 'N', OFTInteger,  183, 186,   4,       1,   1,     1 },
  { "BLOCKR",     'L', 'N', OFTInteger,  187, 190,   4,       1,   1,     1 }
};
static const TigerRecordInfo rt1_2002_info =
  {
    rt1_2002_fields,
    sizeof(rt1_2002_fields) / sizeof(TigerFieldInfo),
    228
  };

static const TigerFieldInfo rt1_fields[] = {
  // fieldname    fmt  type OFTType      beg  end   len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "TLID",       'R', 'N', OFTInteger,    6,  15,  10,       1,   1,     1 },
  { "SIDE1",      'R', 'N', OFTInteger,   16,  16,   1,       1,   1,     1 },
  { "SOURCE",     'L', 'A', OFTString,    17,  17,   1,       1,   1,     1 },
  { "FEDIRP",     'L', 'A', OFTString,    18,  19,   2,       1,   1,     1 },
  { "FENAME",     'L', 'A', OFTString,    20,  49,  30,       1,   1,     1 },
  { "FETYPE",     'L', 'A', OFTString,    50,  53,   4,       1,   1,     1 },
  { "FEDIRS",     'L', 'A', OFTString,    54,  55,   2,       1,   1,     1 },
  { "CFCC",       'L', 'A', OFTString,    56,  58,   3,       1,   1,     1 },
  { "FRADDL",     'R', 'A', OFTString,    59,  69,  11,       1,   1,     1 },
  { "TOADDL",     'R', 'A', OFTString,    70,  80,  11,       1,   1,     1 },
  { "FRADDR",     'R', 'A', OFTString,    81,  91,  11,       1,   1,     1 },
  { "TOADDR",     'R', 'A', OFTString,    92, 102,  11,       1,   1,     1 },
  { "FRIADDL",    'L', 'A', OFTInteger,  103, 103,   1,       1,   1,     1 },
  { "TOIADDL",    'L', 'A', OFTInteger,  104, 104,   1,       1,   1,     1 },
  { "FRIADDR",    'L', 'A', OFTInteger,  105, 105,   1,       1,   1,     1 },
  { "TOIADDR",    'L', 'A', OFTInteger,  106, 106,   1,       1,   1,     1 },
  { "ZIPL",       'L', 'N', OFTInteger,  107, 111,   5,       1,   1,     1 },
  { "ZIPR",       'L', 'N', OFTInteger,  112, 116,   5,       1,   1,     1 },
  { "FAIRL",      'L', 'N', OFTInteger,  117, 121,   5,       1,   1,     1 },
  { "FAIRR",      'L', 'N', OFTInteger,  122, 126,   5,       1,   1,     1 },
  { "TRUSTL",     'L', 'A', OFTString,   127, 127,   1,       1,   1,     1 },
  { "TRUSTR",     'L', 'A', OFTString,   128, 128,   1,       1,   1,     1 },
  { "CENSUS1",    'L', 'A', OFTString,   129, 129,   1,       1,   1,     1 },
  { "CENSUS2",    'L', 'A', OFTString,   130, 130,   1,       1,   1,     1 },
  { "STATEL",     'L', 'N', OFTInteger,  131, 132,   2,       1,   1,     1 },
  { "STATER",     'L', 'N', OFTInteger,  133, 134,   2,       1,   1,     1 },
  { "COUNTYL",    'L', 'N', OFTInteger,  135, 137,   3,       1,   1,     1 },
  { "COUNTYR",    'L', 'N', OFTInteger,  138, 140,   3,       1,   1,     1 },

  { "FMCDL",      'L', 'N', OFTInteger,  141, 145,   5,       1,   1,     1 },
  { "FMCDR",      'L', 'N', OFTInteger,  146, 150,   5,       1,   1,     1 },
  { "FSMCDL",     'L', 'N', OFTInteger,  151, 155,   5,       1,   1,     1 },
  { "FSMCDR",     'L', 'N', OFTInteger,  156, 160,   5,       1,   1,     1 },
  { "FPLL",       'L', 'N', OFTInteger,  161, 165,   5,       1,   1,     1 },
  { "FPLR",       'L', 'N', OFTInteger,  166, 170,   5,       1,   1,     1 },
  { "CTBNAL",     'L', 'N', OFTInteger,  171, 176,   6,       1,   1,     1 },
  { "CTBNAR",     'L', 'N', OFTInteger,  177, 182,   6,       1,   1,     1 },
  { "BLKL",       'L', 'N', OFTString,   183, 186,   4,       1,   1,     1 },
  { "BLKR",       'L', 'N', OFTString,   187, 190,   4,       1,   1,     1 }
}; 
static const TigerRecordInfo rt1_info =
  {
    rt1_fields,
    sizeof(rt1_fields) / sizeof(TigerFieldInfo),
    228
  };

static const TigerRecordInfo rt2_info =
  {
    NULL,       // RT2 is handled specially in the code below; the only
    0,          // thing from this structure that is used is:
    208         // <--- nRecordLength
  };


static const TigerFieldInfo rt3_2000_Redistricting_fields[] = {
  // fieldname    fmt  type OFTType       beg  end  len  bDefine bSet bWrite
  { "TLID",       'R', 'N', OFTInteger,     6,  15,  10,       0,   0,     1 },
  { "STATE90L",   'L', 'N', OFTInteger,    16,  17,   2,       1,   1,     1 },
  { "STATE90R",   'L', 'N', OFTInteger,    18,  19,   2,       1,   1,     1 },
  { "COUN90L",    'L', 'N', OFTInteger,    20,  22,   3,       1,   1,     1 },
  { "COUN90R",    'L', 'N', OFTInteger,    23,  25,   3,       1,   1,     1 },
  { "FMCD90L",    'L', 'N', OFTInteger,    26,  30,   5,       1,   1,     1 },
  { "FMCD90R",    'L', 'N', OFTInteger,    31,  35,   5,       1,   1,     1 },
  { "FPL90L",     'L', 'N', OFTInteger,    36,  40,   5,       1,   1,     1 },
  { "FPL90R",     'L', 'N', OFTInteger,    41,  45,   5,       1,   1,     1 },
  { "CTBNA90L",   'L', 'N', OFTInteger,    46,  51,   6,       1,   1,     1 },
  { "CTBNA90R",   'L', 'N', OFTInteger,    52,  57,   6,       1,   1,     1 },
  { "AIR90L",     'L', 'N', OFTInteger,    58,  61,   4,       1,   1,     1 },
  { "AIR90R",     'L', 'N', OFTInteger,    62,  65,   4,       1,   1,     1 },
  { "TRUST90L",   'L', 'A', OFTString,     66,  66,   1,       1,   1,     1 },
  { "TRUST90R",   'L', 'A', OFTString,     67,  67,   1,       1,   1,     1 },
  { "BLK90L",     'L', 'A', OFTString,     70,  73,   4,       1,   1,     1 },
  { "BLK90R",     'L', 'A', OFTString,     74,  77,   4,       1,   1,     1 },
  { "AIRL",       'L', 'N', OFTInteger,    78,  81,   4,       1,   1,     1 },
  { "AIRR",       'L', 'N', OFTInteger,    82,  85,   4,       1,   1,     1 },

  { "ANRCL",      'L', 'N', OFTInteger,    86,  90,   5,       1,   1,     1 },
  { "ANRCR",      'L', 'N', OFTInteger,    91,  95,   5,       1,   1,     1 },
  { "AITSCEL",    'L', 'N', OFTInteger,    96,  98,   3,       1,   1,     1 },
  { "AITSCER",    'L', 'N', OFTInteger,    99, 101,   3,       1,   1,     1 },
  { "AITSL",      'L', 'N', OFTInteger,   102, 106,   5 ,      1,   1,     1 },
  { "AITSR",      'L', 'N', OFTInteger,   107, 111,   5,       1,   1,     1 }
};
static const TigerRecordInfo rt3_2000_Redistricting_info  =
  {
    rt3_2000_Redistricting_fields,
    sizeof(rt3_2000_Redistricting_fields) / sizeof(TigerFieldInfo),
    111
  };

static const TigerFieldInfo rt3_fields[] = {
  // fieldname    fmt  type OFTType       beg  end  len  bDefine bSet bWrite
  { "TLID",       'R', 'N', OFTInteger,     6,  15,  10,       0,   0,     1 },
  { "STATE90L",   'L', 'N', OFTInteger,    16,  17,   2,       1,   1,     1 },
  { "STATE90R",   'L', 'N', OFTInteger,    18,  19,   2,       1,   1,     1 },
  { "COUN90L",    'L', 'N', OFTInteger,    20,  22,   3,       1,   1,     1 },
  { "COUN90R",    'L', 'N', OFTInteger,    23,  25,   3,       1,   1,     1 },
  { "FMCD90L",    'L', 'N', OFTInteger,    26,  30,   5,       1,   1,     1 },
  { "FMCD90R",    'L', 'N', OFTInteger,    31,  35,   5,       1,   1,     1 },
  { "FPL90L",     'L', 'N', OFTInteger,    36,  40,   5,       1,   1,     1 },
  { "FPL90R",     'L', 'N', OFTInteger,    41,  45,   5,       1,   1,     1 },
  { "CTBNA90L",   'L', 'N', OFTInteger,    46,  51,   6,       1,   1,     1 },
  { "CTBNA90R",   'L', 'N', OFTInteger,    52,  57,   6,       1,   1,     1 },
  { "AIR90L",     'L', 'N', OFTInteger,    58,  61,   4,       1,   1,     1 },
  { "AIR90R",     'L', 'N', OFTInteger,    62,  65,   4,       1,   1,     1 },
  { "TRUST90L",   'L', 'A', OFTInteger,    66,  66,   1,       1,   1,     1 },
  { "TRUST90R",   'L', 'A', OFTInteger,    67,  67,   1,       1,   1,     1 },
  { "BLK90L",     'L', 'A', OFTString,     70,  73,   4,       1,   1,     1 },
  { "BLK90R",     'L', 'A', OFTString,     74,  77,   4,       1,   1,     1 },
  { "AIRL",       'L', 'N', OFTInteger,    78,  81,   4,       1,   1,     1 },
  { "AIRR",       'L', 'N', OFTInteger,    82,  85,   4,       1,   1,     1 },
 
  { "VTDL",       'L', 'A', OFTString,    104, 107,   4,       1,   1,     1 },
  { "VTDR",       'L', 'A', OFTString,    108, 111,   4,       1,   1,     1 }

};
static const TigerRecordInfo rt3_info =
  {
    rt3_fields,
    sizeof(rt3_fields) / sizeof(TigerFieldInfo),
    111
  };

/************************************************************************/
/*                         TigerCompleteChain()                         */
/************************************************************************/

TigerCompleteChain::TigerCompleteChain( OGRTigerDataSource * poDSIn,
                                        CPL_UNUSED const char * pszPrototypeModule )
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "CompleteChain" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbLineString );


    if (poDS->GetVersion() >= TIGER_2002) {
      psRT1Info = &rt1_2002_info;
      bUsingRT3 = FALSE;
    } else {
      psRT1Info = &rt1_info;
      bUsingRT3 = TRUE;
    }

    psRT2Info = &rt2_info;

    nRT1RecOffset = 0;

    if (poDS->GetVersion() >= TIGER_2000_Redistricting) {
      psRT3Info = &rt3_2000_Redistricting_info;
    } else {
      psRT3Info = &rt3_info;
    }

    fpRT3 = NULL;

    panShapeRecordId = NULL;
    fpShape = NULL;
    
    /* -------------------------------------------------------------------- */
    /*      Fields from type 1 record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns( psRT1Info, poFeatureDefn );

    /* -------------------------------------------------------------------- */
    /*      Fields from type 3 record.  Eventually we should verify that    */
    /*      a .RT3 file is available before adding these fields.            */
    /* -------------------------------------------------------------------- */
    if( bUsingRT3 ) {
      AddFieldDefns( psRT3Info, poFeatureDefn );
    }
}

/************************************************************************/
/*                        ~TigerCompleteChain()                         */
/************************************************************************/

TigerCompleteChain::~TigerCompleteChain()

{
    CPLFree( panShapeRecordId );

    if( fpRT3 != NULL )
        VSIFCloseL( fpRT3 );

    if( fpShape != NULL )
        VSIFCloseL( fpShape );
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerCompleteChain::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, "1" ) )
        return FALSE;

    EstablishFeatureCount();

/* -------------------------------------------------------------------- */
/*      Is this a copyright record inserted at the beginning of the     */
/*      RT1 file by the folks at GDT?  If so, setup to ignore the       */
/*      first record.                                                   */
/* -------------------------------------------------------------------- */
    nRT1RecOffset = 0;
    if( pszModule )
    {
        char achHeader[10];
        
        VSIFSeekL( fpPrimary, 0, SEEK_SET );
        VSIFReadL( achHeader, sizeof(achHeader), 1, fpPrimary );
        
        if( EQUALN(achHeader,"Copyright",8) )
        {
            nRT1RecOffset = 1;
            nFeatures--;
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Open the RT3 file                                               */
/* -------------------------------------------------------------------- */
    if( bUsingRT3 )
    {
        if( fpRT3 != NULL )
        {
            VSIFCloseL( fpRT3 );
            fpRT3 = NULL;
        }

        if( pszModule )
        {
            char        *pszFilename;
        
            pszFilename = poDS->BuildFilename( pszModule, "3" );

            fpRT3 = VSIFOpenL( pszFilename, "rb" );

            CPLFree( pszFilename );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Close the shape point file, if open and free the list of        */
/*      record ids.                                                     */
/* -------------------------------------------------------------------- */
    if( fpShape != NULL )
    {
        VSIFCloseL( fpShape );
        fpShape = NULL;
    }
    
    CPLFree( panShapeRecordId );
    panShapeRecordId = NULL;

/* -------------------------------------------------------------------- */
/*      Try to open the RT2 file corresponding to this RT1 file.        */
/* -------------------------------------------------------------------- */
    if( pszModule != NULL )
    {
        char    *pszFilename;

        pszFilename = poDS->BuildFilename( pszModule, "2" );

        fpShape = VSIFOpenL( pszFilename, "rb" );
        
        if( fpShape == NULL )
        {
            if( nRT1RecOffset == 0 )
                CPLError( CE_Warning, CPLE_OpenFailed,
                          "Failed to open %s, intermediate shape arcs will not be available.\n",
                          pszFilename );
        }
        else
            panShapeRecordId = (int *)CPLCalloc(sizeof(int),(size_t)GetFeatureCount());
        
        CPLFree( pszFilename );
    }

    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerCompleteChain::GetFeature( int nRecordId )

{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s1",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the raw record data from the file.                         */
/* -------------------------------------------------------------------- */
    if( fpPrimary == NULL )
        return NULL;

    if( VSIFSeekL( fpPrimary, (nRecordId+nRT1RecOffset) * nRecordLength, 
                  SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to %d of %s1",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFReadL( achRecord, psRT1Info->nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read %d bytes of record %d of %s1 at offset %d",
                  psRT1Info->nRecordLength, nRecordId, pszModule,
                  (nRecordId+nRT1RecOffset) * nRecordLength );
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Set fields.                                                     */
    /* -------------------------------------------------------------------- */

    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRT1Info, poFeature, achRecord );

    /* -------------------------------------------------------------------- */
    /*      Read RT3 record, and apply fields.                              */
    /* -------------------------------------------------------------------- */

    if( fpRT3 != NULL )
    {
        char    achRT3Rec[OGR_TIGER_RECBUF_LEN];
        int     nRT3RecLen = psRT3Info->nRecordLength + nRecordLength - psRT1Info->nRecordLength;

        if( VSIFSeekL( fpRT3, nRecordId * nRT3RecLen, SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %s3",
                      nRecordId * nRT3RecLen, pszModule );
            return NULL;
        }

        if( VSIFReadL( achRT3Rec, psRT3Info->nRecordLength, 1, fpRT3 ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read record %d of %s3",
                      nRecordId, pszModule );
            return NULL;
        }

        SetFields( psRT3Info, poFeature, achRT3Rec );

    }

/* -------------------------------------------------------------------- */
/*      Set geometry                                                    */
/* -------------------------------------------------------------------- */
    OGRLineString       *poLine = new OGRLineString();

    poLine->setPoint(0,
                     atoi(GetField(achRecord, 191, 200)) / 1000000.0,
                     atoi(GetField(achRecord, 201, 209)) / 1000000.0 );

    if( !AddShapePoints( poFeature->GetFieldAsInteger("TLID"), nRecordId,
                         poLine, 0 ) )
    {
        delete poFeature;
        return NULL;
    }
    
    poLine->addPoint(atoi(GetField(achRecord, 210, 219)) / 1000000.0,
                     atoi(GetField(achRecord, 220, 228)) / 1000000.0 );

    poFeature->SetGeometryDirectly( poLine );

    return poFeature;
}

/************************************************************************/
/*                           AddShapePoints()                           */
/*                                                                      */
/*      Record zero or more shape records associated with this line     */
/*      and add the points to the passed line geometry.                 */
/************************************************************************/

int TigerCompleteChain::AddShapePoints( int nTLID, int nRecordId,
                                        OGRLineString * poLine,
                                        CPL_UNUSED int nSeqNum )
{
    int         nShapeRecId;

    nShapeRecId = GetShapeRecordId( nRecordId, nTLID );

    // -2 means an error occured.
    if( nShapeRecId == -2 )
        return FALSE;

    // -1 means there are no extra shape vertices, but things worked fine.
    if( nShapeRecId == -1 )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Read all the sequential records with the same TLID.             */
/* -------------------------------------------------------------------- */
    char        achShapeRec[OGR_TIGER_RECBUF_LEN];
    int         nShapeRecLen = psRT2Info->nRecordLength + nRecordLength - psRT1Info->nRecordLength;

    for( ; TRUE; nShapeRecId++ )
    {
        int  nBytesRead = 0;

        if( VSIFSeekL( fpShape, (nShapeRecId-1) * nShapeRecLen,
                      SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %s2",
                      (nShapeRecId-1) * nShapeRecLen, pszModule );
            return FALSE;
        }

        nBytesRead = VSIFReadL( achShapeRec, 1, psRT2Info->nRecordLength, 
                                fpShape );

        /* 
        ** Handle case where the last record in the file is full.  We will
        ** try to read another record but not find it.  We require that we
        ** have found at least one shape record for this case though. 
        */
        if( nBytesRead <= 0 && VSIFEofL( fpShape ) 
            && poLine->getNumPoints() > 0 )
            break;

        if( nBytesRead != psRT2Info->nRecordLength )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read %d bytes of record %d of %s2 at offset %d",
                      psRT2Info->nRecordLength, nShapeRecId, pszModule,
                      (nShapeRecId-1) * nShapeRecLen );
            return FALSE;
        }

        if( atoi(GetField(achShapeRec,6,15)) != nTLID )
            break;

/* -------------------------------------------------------------------- */
/*      Translate the locations into OGRLineString vertices.            */
/* -------------------------------------------------------------------- */
        int     iVertex;

        for( iVertex = 0; iVertex < 10; iVertex++ )
        {
            int         iStart = 19 + 19*iVertex;
            int         nX = atoi(GetField(achShapeRec,iStart,iStart+9));
            int         nY = atoi(GetField(achShapeRec,iStart+10,iStart+18));

            if( nX == 0 && nY == 0 )
                break;

            poLine->addPoint( nX / 1000000.0, nY / 1000000.0 );
        }

/* -------------------------------------------------------------------- */
/*      Don't get another record if this one was incomplete.            */
/* -------------------------------------------------------------------- */
        if( iVertex < 10 )
            break;
    }

    return TRUE;
}

/************************************************************************/
/*                          GetShapeRecordId()                          */
/*                                                                      */
/*      Get the record id of the first record of shape points for       */
/*      the provided TLID (complete chain).                             */
/************************************************************************/

int TigerCompleteChain::GetShapeRecordId( int nChainId, int nTLID )

{
    CPLAssert( nChainId >= 0 && nChainId < GetFeatureCount() );

    if( fpShape == NULL || panShapeRecordId == NULL )
        return -1;
    
/* -------------------------------------------------------------------- */
/*      Do we already have the answer?                                  */
/* -------------------------------------------------------------------- */
    if( panShapeRecordId[nChainId] != 0 )
        return panShapeRecordId[nChainId];
    
/* -------------------------------------------------------------------- */
/*      If we don't already have this value, then search from the       */
/*      previous known record.                                          */
/* -------------------------------------------------------------------- */
    int iTestChain, nWorkingRecId;
        
    for( iTestChain = nChainId-1;
         iTestChain >= 0 && panShapeRecordId[iTestChain] <= 0;
         iTestChain-- ) {}

    if( iTestChain < 0 )
    {
        iTestChain = -1;
        nWorkingRecId = 1;
    }
    else
    {
        nWorkingRecId = panShapeRecordId[iTestChain]+1;
    }

/* -------------------------------------------------------------------- */
/*      If we have non existent records following (-1's) we can         */
/*      narrow our search a bit.                                        */
/* -------------------------------------------------------------------- */
    while( panShapeRecordId[iTestChain+1] == -1 )
    {
        iTestChain++;
    }

/* -------------------------------------------------------------------- */
/*      Read records up to the maximum distance that is possibly        */
/*      required, looking for our target TLID.                          */
/* -------------------------------------------------------------------- */
    int         nMaxChainToRead = nChainId - iTestChain;
    int         nChainsRead = 0;
    char        achShapeRec[OGR_TIGER_RECBUF_LEN];
    int         nShapeRecLen = psRT2Info->nRecordLength + nRecordLength - psRT1Info->nRecordLength;

    while( nChainsRead < nMaxChainToRead )
    {
        if( VSIFSeekL( fpShape, (nWorkingRecId-1) * nShapeRecLen,
                      SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %s2",
                      (nWorkingRecId-1) * nShapeRecLen, pszModule );
            return -2;
        }

        if( VSIFReadL( achShapeRec, psRT2Info->nRecordLength, 1, fpShape ) != 1 )
        {
            if( !VSIFEofL( fpShape ) )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to read record %d of %s2",
                          nWorkingRecId-1, pszModule );
                return -2;
            }
            else
                return -1;
        }

        if( atoi(GetField(achShapeRec,6,15)) == nTLID )
        {
            panShapeRecordId[nChainId] = nWorkingRecId;

            return nWorkingRecId;
        }

        if( atoi(GetField(achShapeRec,16,18)) == 1 )
        {
            nChainsRead++;
        }

        nWorkingRecId++;
    }

    panShapeRecordId[nChainId] = -1;

    return -1;
}

/************************************************************************/
/*                           SetWriteModule()                           */
/************************************************************************/
int TigerCompleteChain::SetWriteModule( const char *pszFileCode, int nRecLen, 
                                        OGRFeature *poFeature )

{
    int bSuccess;

    bSuccess = TigerFileBase::SetWriteModule( pszFileCode, nRecLen, poFeature);
    if( !bSuccess )
        return bSuccess;

/* -------------------------------------------------------------------- */
/*      Open the RT3 file                                               */
/* -------------------------------------------------------------------- */
    if( bUsingRT3 )
    {
        if( fpRT3 != NULL )
        {
            VSIFCloseL( fpRT3 );
            fpRT3 = NULL;
        }

        if( pszModule )
        {
            char        *pszFilename;
        
            pszFilename = poDS->BuildFilename( pszModule, "3" );

            fpRT3 = VSIFOpenL( pszFilename, "ab" );

            CPLFree( pszFilename );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Close the shape point file, if open and free the list of        */
/*      record ids.                                                     */
/* -------------------------------------------------------------------- */
    if( fpShape != NULL )
    {
        VSIFCloseL( fpShape );
        fpShape = NULL;
    }
    
    if( pszModule )
    {
        char        *pszFilename;
        
        pszFilename = poDS->BuildFilename( pszModule, "2" );
        
        fpShape = VSIFOpenL( pszFilename, "ab" );
        
        CPLFree( pszFilename );
    }

    return TRUE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerCompleteChain::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[OGR_TIGER_RECBUF_LEN];
    OGRLineString *poLine = (OGRLineString *) poFeature->GetGeometryRef();

    if( poLine == NULL 
        || (poLine->getGeometryType() != wkbLineString
            && poLine->getGeometryType() != wkbLineString25D) )
        return OGRERR_FAILURE;

    /* -------------------------------------------------------------------- */
    /*      Write basic data record ("RT1")                                 */
    /* -------------------------------------------------------------------- */
    if( !SetWriteModule( "1", psRT1Info->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;
    memset( szRecord, ' ', psRT1Info->nRecordLength );
    WriteFields( psRT1Info, poFeature, szRecord );
    WritePoint( szRecord, 191, poLine->getX(0), poLine->getY(0) );
    WritePoint( szRecord, 210, 
                poLine->getX(poLine->getNumPoints()-1), 
                poLine->getY(poLine->getNumPoints()-1) );
    WriteRecord( szRecord, psRT1Info->nRecordLength, "1" );

    /* -------------------------------------------------------------------- */
    /*      Write geographic entity codes (RT3)                             */
    /* -------------------------------------------------------------------- */
    if (bUsingRT3) {
      memset( szRecord, ' ', psRT3Info->nRecordLength );
      WriteFields( psRT3Info, poFeature, szRecord );
      WriteRecord( szRecord, psRT3Info->nRecordLength, "3", fpRT3 );
    }

    /* -------------------------------------------------------------------- */
    /*      Write shapes sections (RT2)                                     */
    /* -------------------------------------------------------------------- */
    if( poLine->getNumPoints() > 2 )
    {
        int     nPoints = poLine->getNumPoints();
        int     iPoint, nRTSQ = 1;

        for( iPoint = 1; iPoint < nPoints-1; )
        {
            int         i;
            char        szTemp[5];

            memset( szRecord, ' ', psRT2Info->nRecordLength );

            WriteField( poFeature, "TLID", szRecord, 6, 15, 'R', 'N' );
            
            sprintf( szTemp, "%3d", nRTSQ );
            strncpy( ((char *)szRecord) + 15, szTemp, 4 );

            for( i = 0; i < 10; i++ )
            {
                if( iPoint < nPoints-1 )
                    WritePoint( szRecord, 19+19*i, 
                                poLine->getX(iPoint), poLine->getY(iPoint) );
                else
                    WritePoint( szRecord, 19+19*i, 0.0, 0.0 );

                iPoint++;
            }
            
            WriteRecord( szRecord, psRT2Info->nRecordLength, "2", fpShape );

            nRTSQ++;
        }
    }

    return OGRERR_NONE;
}
