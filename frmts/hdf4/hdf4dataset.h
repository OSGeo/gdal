/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Header file for HDF4 datasets reader.
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@remotesensing.org>
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
 * Revision 1.18  2006/11/23 13:23:56  dron
 * Added more logic to guess HDF-EOS datasets.
 *
 * Revision 1.17  2006/08/03 11:47:38  dron
 * Added EOS_SWATH_GEOL subdataset type.
 *
 * Revision 1.16  2005/04/25 20:04:43  dron
 * Make more functions constant.
 *
 * Revision 1.15  2005/03/29 12:42:53  dron
 * Added AnyTypeToDouble() method.
 *
 * Revision 1.14  2004/06/19 21:37:31  dron
 * Use HDF-EOS library for appropriate datasets; major cpde rewrite.
 *
 * Revision 1.13  2003/11/07 15:49:14  dron
 * Added GetDataType() and GetDataTypeName().
 *
 * Revision 1.12  2003/06/26 20:42:31  dron
 * Support for Hyperion Level 1 data product.
 *
 * Revision 1.11  2003/06/25 08:26:18  dron
 * Support for Aster Level 1A/1B/2 products.
 *
 * Revision 1.10  2003/06/12 15:07:34  dron
 * Value for MODIS Level 2 added.
 *
 * Revision 1.9  2003/06/10 09:33:48  dron
 * Added support for MODIS Level 3 products.
 *
 * Revision 1.8  2003/05/21 14:11:43  dron
 * MODIS Level 1B earth-view (EV) product now supported.
 *
 * Revision 1.7  2002/11/08 17:57:43  dron
 * Type of pszDataType changet to const char*.
 *
 * Revision 1.6  2002/11/06 15:47:14  dron
 * Added support for 3D datasets creation
 *
 * Revision 1.5  2002/10/25 14:28:54  dron
 * Initial support for HDF4 creation.
 *
 * Revision 1.4  2002/09/06 10:42:23  dron
 * Georeferencing for ASTER Level 1b datasets and ASTER DEMs.
 *
 * Revision 1.3  2002/07/23 12:27:58  dron
 * General Raster Interface support added.
 *
 * Revision 1.2  2002/07/17 16:24:31  dron
 * MODIS support improved a bit.
 *
 * Revision 1.1  2002/07/16 11:04:11  dron
 * New driver: HDF4 datasets. Initial version.
 *
 *
 */

#ifndef _HDF4DATASET_H_INCLUDED_
#define _HDF4DATASET_H_INCLUDED_

#include "cpl_list.h"

typedef enum			// Types of dataset:
{
    HDF4_SDS,			// Scientific Dataset
    HDF4_GR,			// General Raster Image
    HDF4_EOS,                   // HDF EOS
    HDF4_UNKNOWN
} HDF4DatasetType;

typedef enum			// Types of data products:
{
    GDAL_HDF4,			// HDF written by GDAL
    EOS_GRID,                   // HDF-EOS Grid
    EOS_SWATH,                  // HDF-EOS Swath
    EOS_SWATH_GEOL,             // HDF-EOS Swath Geolocation Array
    SEAWIFS_L1A,		// SeaWiFS Level-1A Data
    SEAWIFS_L2,			// SeaWiFS Level-2 Data
    SEAWIFS_L3,			// SeaWiFS Level-3 Standard Mapped Image
    HYPERION_L1,                // Hyperion L1 Data Product
    UNKNOWN
} HDF4SubdatasetType;

/************************************************************************/
/* ==================================================================== */
/*				HDF4Dataset				*/
/* ==================================================================== */
/************************************************************************/

class HDF4Dataset : public GDALDataset
{

  private:

    int         bIsHDFEOS;

    char        **HDF4EOSTokenizeAttrs( const char *pszString ) const;
    char        **HDF4EOSGetObject( char **papszAttrList, char **ppszAttrName,
                                    char **ppszAttrValue ) const;
     
  protected:

    FILE	*fp;
    int32	hHDF4, hSD, hGR;
    int32	nDatasets, nImages;
    HDF4DatasetType iDatasetType;
    HDF4SubdatasetType iSubdatasetType;
    const char	*pszSubdatasetType;

    char	**papszGlobalMetadata;
    char	**papszSubDatasets;

    GDALDataType GetDataType( int32 ) const;
    const char  *GetDataTypeName( int32 ) const;
    int         GetDataTypeSize( int32 ) const;
    double      AnyTypeToDouble( int32, void * ) const;
    char        **TranslateHDF4Attributes( int32, int32, char *,
                                           int32, int32, char ** ) const;
    char        ** TranslateHDF4EOSAttributes( int32, int32, int32,
                                               char ** ) const;
    CPLErr      ReadGlobalAttributes( int32 );

  public:
                HDF4Dataset();
		~HDF4Dataset();
    
    virtual char **GetMetadata( const char * pszDomain = "" );
    static GDALDataset *Open( GDALOpenInfo * );
};

char *SPrintArray( GDALDataType eDataType, void *paDataArray,
                          int nValues, char * pszDelimiter );


#endif /* _HDF4DATASET_H_INCLUDED_ */

