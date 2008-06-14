/******************************************************************************
 *
 * File :    pgchip.h
 * Project:  PGCHIP Driver
 * Purpose:  Main header file for POSTGIS CHIP/GDAL Driver 
 * Author:   Benjamin Simon, noumayoss@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Benjamin Simon, noumayoss@gmail.com
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
 * Revision 1.1  2005/08/29  bsimon
 * New
 *
 */

#include "gdal_priv.h"
#include "libpq-fe.h"
#include "liblwgeom.h"

// External functions (what's again the reason for using explicit hex form ?)
extern void pgch_deparse_hex(unsigned char in, unsigned char *out);
extern void deparse_hex_string(unsigned char *strOut,char *strIn,int length);
extern void parse_hex_string(unsigned char *strOut,char *strIn,int length);

/* color types */
#define PGCHIP_COLOR_TYPE_GRAY 0
#define PGCHIP_COLOR_TYPE_PALETTE 1
#define PGCHIP_COLOR_TYPE_RGB_ALPHA 4

//pg_chip color struct 
typedef struct pgchip_color_nohex_struct
{
   unsigned char red; 
   unsigned char green;
   unsigned char blue;
   unsigned char alpha;
} pgchip_color;


/************************************************************************/
/* ==================================================================== */
/*				PGCHIPDataset				*/
/* ==================================================================== */
/************************************************************************/

class PGCHIPRasterBand;

class PGCHIPDataset : public GDALDataset{

    friend class PGCHIPRasterBand;

    PGconn      *hPGConn;
    char        *pszTableName;
    char	*pszDSName;
    char	*pszProjection;

    CHIP        *PGCHIP;
    int         SRID;
    int         nBitDepth;
    
    int                 nColorType; /* PGHIP_COLOR_TYPE_* */
    GDALColorTable      *poColorTable;
    int		bHaveNoData;
    double 	dfNoDataValue;
    
    double              adfGeoTransform[6];
    int                 bGeoTransformValid;
    
  public:
	
	PGCHIPDataset();
        ~PGCHIPDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
    
    static void        printChipInfo(const CHIP& chip);

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            PGCHIPRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class PGCHIPRasterBand : public GDALRasterBand{

    friend class PGCHIPDataset;
    
  public:

    PGCHIPRasterBand( PGCHIPDataset *, int );
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
            
};
