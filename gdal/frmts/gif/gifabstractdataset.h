/******************************************************************************
 * $Id$
 *
 * Project:  GIF Driver
 * Purpose:  GIF Abstract Dataset
 * Author:   Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ****************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _GIFABSTRACTDATASET_H_INCLUDED
#define _GIFABSTRACTDATASET_H_INCLUDED

#include "gdal_pam.h"

CPL_C_START
#include "gif_lib.h"
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                        GIFAbstractDataset                            */
/* ==================================================================== */
/************************************************************************/

class GIFAbstractDataset : public GDALPamDataset
{
  protected:
    friend class    GIFAbstractRasterBand;

    VSILFILE        *fp;

    GifFileType *hGifFile;

    char        *pszProjection;
    int         bGeoTransformValid;
    double      adfGeoTransform[6];

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    int         bHasReadXMPMetadata;
    void        CollectXMPMetadata();

    CPLString   osWldFilename;

    void        DetectGeoreferencing( GDALOpenInfo * poOpenInfo );

  public:
                 GIFAbstractDataset();
                 ~GIFAbstractDataset();

    virtual const char *GetProjectionRef();
    virtual CPLErr GetGeoTransform( double * );
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    virtual char      **GetMetadataDomainList();
    virtual char  **GetMetadata( const char * pszDomain = "" );

    virtual char **GetFileList(void);

    static int          Identify( GDALOpenInfo * );

    static GifFileType* myDGifOpen( void *userPtr, InputFunc readFunc );
    static int          myDGifCloseFile( GifFileType *hGifFile );
    static int          myEGifCloseFile( GifFileType *hGifFile );
    static int          ReadFunc( GifFileType *psGFile, GifByteType *pabyBuffer, 
                                  int nBytesToRead );
};

/************************************************************************/
/* ==================================================================== */
/*                        GIFAbstractRasterBand                         */
/* ==================================================================== */
/************************************************************************/

class GIFAbstractRasterBand : public GDALPamRasterBand
{
  protected:
    SavedImage  *psImage;

    int         *panInterlaceMap;
    
    GDALColorTable *poColorTable;

    int         nTransparentColor;

  public:

                   GIFAbstractRasterBand(GIFAbstractDataset *poDS, int nBand, 
                                         SavedImage *psSavedImage, int nBackground,
                                         int bAdvertizeInterlacedMDI );
    virtual       ~GIFAbstractRasterBand();

    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};


#endif
