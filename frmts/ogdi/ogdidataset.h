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
 * Revision 1.1  1999/01/11 15:29:16  warmerda
 * New
 *
 */

#ifndef OGDIDATASET_H_INCLUDED
#define OGDIDATASET_H_INCLUDED

#include "gdal_priv.h"
#include "ecs.h"

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
    char	*pszProjection;

  public:
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

  public:

                   OGDIRasterBand( OGDIDataset *, int );

    // should override RasterIO eventually.
    
    virtual CPLErr IReadBlock( int, int, void * );
};

#endif /* ndef OGDIDATASET_H_INCLUDED */
