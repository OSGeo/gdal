/******************************************************************************
 * $Id$
 *
 * Name:     ogdidataset.h
 * Project:  OGDI Bridge
 * Purpose:  OGDIDataset and related declarations.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.6  2000/10/26 03:30:46  warmerda
 * added math.h so it doesn't get included first from within ecsutil.h
 *
 * Revision 1.5  2000/08/28 21:30:17  warmerda
 * restructure to use cln_GetNextObject
 *
 * Revision 1.4  2000/08/25 21:31:04  warmerda
 * added colortable support
 *
 * Revision 1.3  2000/08/25 14:28:04  warmerda
 * preliminary support with IRasterIO
 *
 * Revision 1.2  1999/02/25 22:20:47  warmerda
 * Explicitly declare OGDIDataset constructor and destructor
 *
 * Revision 1.1  1999/01/11 15:29:16  warmerda
 * New
 *
 */

#ifndef OGDIDATASET_H_INCLUDED
#define OGDIDATASET_H_INCLUDED

#include <math.h>
#include "ecs.h"
#include "gdal_priv.h"

CPL_C_START
void	GDALRegister_OGDI(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				OGDIDataset				*/
/* ==================================================================== */
/************************************************************************/

class OGDIRasterBand;

class CPL_DLL OGDIDataset : public GDALDataset
{
    friend	OGDIRasterBand;
    
    int		nClientID;

    ecs_Region	sGlobalBounds;
    ecs_Region  sCurrentBounds;
    int         nCurrentBand;
    int         nCurrentIndex;

    char	*pszProjection;

    static CPLErr CollectLayers(char***,char***);

  public:
    		OGDIDataset();
    		~OGDIDataset();
                
    static GDALDataset *Open( GDALOpenInfo * );

    int		GetClientID() { return nClientID; }

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

    virtual void *GetInternalHandle( const char * );
};

/************************************************************************/
/* ==================================================================== */
/*                            OGDIRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class OGDIRasterBand : public GDALRasterBand
{
    friend	OGDIDataset;

    char	*pszLayerName;
    ecs_Family  eFamily;

    GDALColorTable *poCT;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );

    CPLErr         EstablishAccess( int nYOff, int nXOff, int nXSize, 
                                    int nBufXSize );

  public:

                   OGDIRasterBand( OGDIDataset *, int, const char *,
                                   ecs_Family );
                   ~OGDIRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual int    HasArbitraryOverviews();
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};

#endif /* ndef OGDIDATASET_H_INCLUDED */
