/******************************************************************************
 * $Id$
 *
 * Project:  IDA Raster Driver
 * Purpose:  Implemenents IDA driver/dataset/rasterband.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.3  2004/12/26 21:25:36  fwarmerdam
 * avoid spurious opens
 *
 * Revision 1.2  2004/12/26 17:37:46  fwarmerdam
 * fix unix/dos format
 *
 * Revision 1.1  2004/12/26 16:17:00  fwarmerdam
 * New
 *
 */

#include "rawdataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_IDA(void);
CPL_C_END

// convert a Turbo Pascal real into a double
static double tp2c(GByte *r);

// convert a double into a Turbo Pascal real
//static tpReal c2tp(double n);

/************************************************************************/
/* ==================================================================== */
/*				IDADataset				*/
/* ==================================================================== */
/************************************************************************/

class IDADataset : public RawDataset
{
    int         nImageType;
    int         nProjection;
    char        szTitle[81];
    double      dfLatCenter;
    double      dfLongCenter;
    double      dfXCenter;
    double      dfYCenter;
    double      dfDX;
    double      dfDY;
    double      dfParallel1;
    double      dfParallel2;
    int         nLower;
    int         nUpper;
    int         nMissing;
    double      dfM;
    double      dfB;
    int         nDecimals;

    FILE       *fpRaw;
    
  public:
    		IDADataset();
    	        ~IDADataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*			        IDARasterBand                           */
/* ==================================================================== */
/************************************************************************/

class IDARasterBand : public RawRasterBand
{
  public:
    		IDARasterBand( IDADataset *poDSIn, FILE *fpRaw, int nXSize );
    virtual     ~IDARasterBand();
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            IDARasterBand                             */
/************************************************************************/

IDARasterBand::IDARasterBand( IDADataset *poDSIn,
                              FILE *fpRaw, int nXSize )
        : RawRasterBand( poDSIn, 1, fpRaw, 512, 1, nXSize, 
                         GDT_Byte, FALSE, FALSE )

{
}

/************************************************************************/
/*                           ~IDARasterBand()                           */
/************************************************************************/

IDARasterBand::~IDARasterBand()

{
}

/************************************************************************/
/* ==================================================================== */
/*				IDADataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             IDADataset()                             */
/************************************************************************/

IDADataset::IDADataset()
{
    fpRaw = NULL;
}

/************************************************************************/
/*                            ~IDADataset()                             */
/************************************************************************/

IDADataset::~IDADataset()

{
    if( fpRaw != NULL )
        VSIFClose( fpRaw );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *IDADataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this an IDA file?                                            */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 512 )
        return NULL;

    // For now only allow GA file till we get more specific 
    // criteria to limit the format.
    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"GA") )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    IDADataset *poDS = new IDADataset();

/* -------------------------------------------------------------------- */
/*      Parse various values out of the header.                         */
/* -------------------------------------------------------------------- */
    poDS->nImageType = poOpenInfo->pabyHeader[22];
    poDS->nProjection = poOpenInfo->pabyHeader[23];

    poDS->nRasterYSize = poOpenInfo->pabyHeader[30] 
        + poOpenInfo->pabyHeader[31] * 256;
    poDS->nRasterXSize = poOpenInfo->pabyHeader[32] 
        + poOpenInfo->pabyHeader[33] * 256;

    strncpy( poDS->szTitle, (const char *) poOpenInfo->pabyHeader+38, 80 );
    poDS->szTitle[80] = '\0';

    poDS->dfLatCenter = tp2c( poOpenInfo->pabyHeader + 120 );
    poDS->dfLongCenter = tp2c( poOpenInfo->pabyHeader + 126 );
    poDS->dfXCenter = tp2c( poOpenInfo->pabyHeader + 132 );
    poDS->dfYCenter = tp2c( poOpenInfo->pabyHeader + 138 );
    poDS->dfDX = tp2c( poOpenInfo->pabyHeader + 144 );
    poDS->dfDY = tp2c( poOpenInfo->pabyHeader + 150 );
    poDS->dfParallel1 = tp2c( poOpenInfo->pabyHeader + 156 );
    poDS->dfParallel2 = tp2c( poOpenInfo->pabyHeader + 162 );

/* -------------------------------------------------------------------- */
/*      Create the band.                                                */
/* -------------------------------------------------------------------- */

    poDS->fpRaw = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

    poDS->SetBand( 1, new IDARasterBand( poDS, poDS->fpRaw, 
                                         poDS->nRasterXSize ) );

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                                tp2c()                                */
/*                                                                      */
/*      convert a Turbo Pascal real into a double                       */
/************************************************************************/

static double tp2c(GByte *r)
{
  double mant;
  int sign, exp, i;

  // handle 0 case
  if (r[0] == 0)
    return 0.0;

  // extract sign: bit 7 of byte 5
  sign = r[5] & 0x80 ? -1 : 1;

  // extract mantissa from first bit of byte 1 to bit 7 of byte 5
  mant = 0;
  for (i = 1; i < 5; i++)
    mant = (r[i] + mant) / 256;
  mant = (mant + (r[5] & 0x7F)) / 128 + 1;

  // extract exponent
  exp = r[0] - 129;

  // compute the damned number
  return sign * ldexp(mant, exp);
}

#ifdef notdef
/************************************************************************/
/*                                c2tp()                                */
/*                                                                      */
/*      convert a double into a Turbo Pascal real                       */
/************************************************************************/

static tpReal c2tp(double x)
{
  double mant, temp;
  int negative, exp, i;
  tpReal r;

  // handle 0 case
  if (x == 0.0)
  {
    for (i = 0; i < 6; r.b[i++] = 0);
    return r;
  }
  // compute mantissa, sign and exponent
  mant = frexp(x, &exp) * 2 - 1;
  exp--;
  negative = 0;
  if (mant < 0)
  {
    mant = -mant;
    negative = 1;
  }
  // stuff mantissa into Turbo Pascal real
  mant = modf(mant * 128, &temp);
  r.b[5] = temp;
  for (i = 4; i >= 1; i--)
  {
    mant = modf(mant * 256, &temp);
    r.b[i] = temp;
  }
  // add sign
  if (negative)
    r.b[5] |= 0x80;

  // put exponent
  r.b[0] = exp + 129;

  return r;
}
#endif

/************************************************************************/
/*                         GDALRegister_IDA()                          */
/************************************************************************/

void GDALRegister_IDA()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "IDA" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "IDA" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Image Data and Analysis" );
//        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_IDA.html" );

        poDriver->pfnOpen = IDADataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

