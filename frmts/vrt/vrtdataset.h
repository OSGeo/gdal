/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Declaration of virtual gdal dataset classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.4  2002/05/29 18:13:44  warmerda
 * added nodata handling for averager
 *
 * Revision 1.3  2002/05/29 16:06:05  warmerda
 * complete detailed band metadata
 *
 * Revision 1.2  2001/11/18 15:46:45  warmerda
 * added SRS and GeoTransform
 *
 * Revision 1.1  2001/11/16 21:14:31  warmerda
 * New
 *
 */

#ifndef VIRTUALDATASET_H_INCLUDED
#define VIRTUALDATASET_H_INCLUDED

#include "gdal_priv.h"
#include "cpl_minixml.h"

CPL_C_START
void	GDALRegister_VRT(void);
CPL_C_END

int VRTApplyMetadata( CPLXMLNode *, GDALMajorObject * );
CPLXMLNode *VRTSerializeMetadata( GDALMajorObject * );

/************************************************************************/
/*                              VRTDataset                              */
/************************************************************************/

class VRTRasterBand;

class CPL_DLL VRTDataset : public GDALDataset
{
    char           *pszProjection;

    int            bGeoTransformSet;
    double         adfGeoTransform[6];

    int           nGCPCount;
    GDAL_GCP      *pasGCPList;
    char          *pszGCPProjection;

    int            bNeedsFlush;

  public:
                 VRTDataset(int nXSize, int nYSize);
                ~VRTDataset();

    void          SetNeedsFlush() { bNeedsFlush = TRUE; }
    virtual void  FlushCache();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

    virtual CPLErr AddBand( GDALDataType eType, 
                            char **papszOptions=NULL );

    CPLXMLNode *   SerializeToXML(void);
 
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszName,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/*                            VRTRasterBand                             */
/************************************************************************/

class VRTSimpleSource;

class CPL_DLL VRTRasterBand : public GDALRasterBand
{
    int		   nSources;
    VRTSimpleSource **papoSources;

    int            bNoDataValueSet;
    double         dfNoDataValue;

    GDALColorTable *poColorTable;

    GDALColorInterp eColorInterp;

    void           Initialize( int nXSize, int nYSize );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );
  public:

    		   VRTRasterBand( GDALDataset *poDS, int nBand );
                   VRTRasterBand( GDALDataType eType, 
                                  int nXSize, int nYSize );
                   VRTRasterBand( GDALDataset *poDS, int nBand, 
                                  GDALDataType eType, 
                                  int nXSize, int nYSize );
    virtual        ~VRTRasterBand();

    CPLErr         XMLInit( CPLXMLNode * );
    CPLXMLNode *   SerializeToXML(void);

#define VRT_NODATA_UNSET -1234.56

    CPLErr         AddSimpleSource( GDALRasterBand *poSrcBand, 
                                    int nSrcXOff=-1, int nSrcYOff=-1, 
                                    int nSrcXSize=-1, int nSrcYSize=-1, 
                                    int nDstXOff=-1, int nDstYOff=-1, 
                                    int nDstXSize=-1, int nDstYSize=-1,
                                    const char *pszResampling = "near",
                                    double dfNoDataValue = VRT_NODATA_UNSET);

    virtual CPLErr IReadBlock( int, int, void * );

    virtual CPLErr SetNoDataValue( double );
    virtual double GetNoDataValue( int *pbSuccess = NULL );

    virtual CPLErr SetColorTable( GDALColorTable * ); 
    virtual GDALColorTable *GetColorTable();

    virtual CPLErr SetColorInterpretation( GDALColorInterp );
    virtual GDALColorInterp GetColorInterpretation();
};

#endif /* ndef VIRTUALDATASET_H_INCLUDED */







