/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4) Reader
 * Purpose:  Read subdatasets of HDF4 file.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:   Andrey Kiselev, dron@at1895.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <a_kissel@eudoramail.com>
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
 * Revision 1.1  2002/07/16 11:04:11  dron
 * New driver: HDF4 datasets. Initial version.
 *
 *
 */

#include <hdf/hdf.h>
#include <hdf/mfhdf.h>

#include "gdal_priv.h"
#include "cpl_string.h"
#include "hdf4dataset.h"

CPL_CVSID("$Id$");

static GDALDriver	*poHDF4ImageDriver = NULL;

CPL_C_START
void	GDALRegister_HDF4(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				HDF4ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

class HDF4ImageDataset : /*public GDALDataset,*/ public HDF4Dataset
{
    friend class HDF4ImageRasterBand;

    char	**papszSubdatasetName;
    char	*pszFilename;
    int32	iSDS;
    int32	iRank, iNumType, nAttrs;
    int32	aiDimSizes[MAX_VAR_DIMS];
    char	szName[65];

//    double      adfGeoTransform[6];
//    char        *pszProjection;

//    void	ComputeGeoref();
    
  public:
                HDF4ImageDataset();
		~HDF4ImageDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

//    CPLErr 	GetGeoTransform( double * padfTransform );
//    const char *GetProjectionRef();

};

/************************************************************************/
/* ==================================================================== */
/*                            HDF4ImageRasterBand                       */
/* ==================================================================== */
/************************************************************************/

class HDF4ImageRasterBand : public GDALRasterBand
{
    friend class HDF4ImageDataset;

  public:

    		HDF4ImageRasterBand( HDF4ImageDataset *, int );
    
    GDALDataType GetDataType( int32 );
    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/*		Translate HDF4 data type into GDAL data type		*/
/************************************************************************/
GDALDataType HDF4ImageRasterBand::GetDataType( int32 iNumType )
{
    switch (iNumType)
    {
        case DFNT_CHAR8: // The same as DFNT_CHAR
	case DFNT_UCHAR8: // The same as DFNT_UCHAR
        case DFNT_INT8:
        case DFNT_UINT8:
	return GDT_Byte;
	break;
        case DFNT_INT16:
	return GDT_Int16;
	break;
        case DFNT_UINT16:
	return GDT_UInt16;
	break;
        case DFNT_INT32:
	return GDT_Int32;
	break;
        case DFNT_UINT32:
	return GDT_UInt32;
	break;
        case DFNT_INT64:
	return GDT_Unknown;
	break;
        case DFNT_UINT64:
	return GDT_Unknown;
	break;
        case DFNT_FLOAT32:
	return GDT_CFloat32;
	break;
        case DFNT_FLOAT64:
	return GDT_CFloat64;
	break;
	default:
	return GDT_Unknown;
	break;
    }
}

/************************************************************************/
/*                           HDF4ImageRasterBand()                      */
/************************************************************************/

HDF4ImageRasterBand::HDF4ImageRasterBand( HDF4ImageDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GetDataType( poDS->iNumType );

    nBlockXSize = poDS->GetRasterXSize();
    // Scanline based input is very slow with HDF library,
    // so we will read whole image plane at time (but this is also not very
    // fast :-( )
    nBlockYSize = poDS->GetRasterYSize();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr HDF4ImageRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    HDF4ImageDataset *poGDS = (HDF4ImageDataset *) poDS;
#define MAX_DIMS         20	/* FIXME: maximum number of dimensions in sds */
    int32	iStart[MAX_DIMS], iStride[MAX_DIMS], iEdges[MAX_DIMS];

/* HDF rank:
  A rank 2 dataset is an image read in scan-line order (2D). 
  A rank 3 dataset is a series of images which are read in an image at a time
  to form a volume.
  A rank 4 dataset may be thought of as a series of volumes.

  The "iStart" array specifies the multi-dimensional index of the starting
  corner of the hyperslab to read. The values are zero based.

  The "edge" array specifies the number of values to read along each
  dimension of the hyperslab.

  The "iStride" array allows for sub-sampling along each dimension. If a
  iStride value is specified for a dimension, that many values will be
  skipped over when reading along that dimension. Specifying iStride = NULL
  in the C interface or iStride = 1 in either interface specifies contiguous
  reading of data. If the iStride values are set to 0, SDreaddata returns
  FAIL (or -1). No matter what iStride value is provided, data is always
  placed contiguously in buffer.
 
  See also:
   http://www.dur.ac.uk/~dcs0elb/au-case-study/code/hdf-browse.c.html
   http://dao.gsfc.nasa.gov/DAO_people/yin/quads.code.html

Dimensions:
http://hdf.ncsa.uiuc.edu/training/UG_Examples/SD/write_slab.c
  aiDimSizes[0] = Z_LENGTH;
  aiDimSizes[1] = Y_LENGTH;
  aiDimSizes[2] = X_LENGTH;

 3D data:
  note that iEdges[1] is set to 1 to define a 2-dimensional slab parallel to the ZX plane.  

Meaning of iStart, iEdges, iStride:
http://www.swa.com/meteorology/hdf/tutorial/File_reading.html
YL = 30;
XL = 30;
dims[0] = YL;
 dims[1] = XL;
 iStart[0] =0;  row start - X axis
 iStart[1]=0; column start - Y axis
 iEdges[0] = dims[0];  - number elements to read  - rowmax
 iEdges[1] = dims[1]; - number elements to read - colmax
 iStride[0] = 3;   - skip every 3rd element 
 iStride[1] = 1; - no skip with value 1
*/
    switch ( poGDS->iRank )
    {
        case 4:	// 4Dim: volume-time
	// FIXME: needs sample file. Do not works currently.
	iStart[3] = 0/* range: 0--aiDimSizes[3]-1 */;	iStride[3] = 1;	iEdges[3] = 1;
	iStart[2] = 0/* range: 0--aiDimSizes[2]-1 */;	iStride[2] = 1;	iEdges[2] = 1;
	iStart[1] = nBlockYOff; iStride[1] = 1;	iEdges[1] = nBlockYSize;
	iStart[0] = nBlockXOff;	iStride[0] = 1;	iEdges[0] = nBlockXSize;
	break;
        case 3: // 3Dim: volume
	switch(poGDS->iDataType)
	{
	    case MODIS_L1B:
	    case MOD02QKM_L1B:
	    iStart[0] = nBand - 1;	iStride[0] = 1;	iEdges[0] = 1;
	    iStart[1] = nBlockYOff;	iStride[1] = 1;	iEdges[1] = nBlockYSize;
	    iStart[2] = nBlockXOff;	iStride[2] = 1;	iEdges[2] = nBlockXSize;
	    break;
	    default:
	    iStart[2] = nBand - 1;	iStride[2] = 1;	iEdges[2] = 1;
	    iStart[0] = nBlockYOff;	iStride[0] = 1;	iEdges[0] = nBlockYSize;
	    iStart[1] = nBlockXOff;	iStride[1] = 1;	iEdges[1] = nBlockXSize;
	    break;
	}		
	break;
        case 2: // 2Dim: rows/cols
	iStart[0] = nBlockYOff;	iStride[0] = 1;	iEdges[0] = nBlockYSize;
	iStart[1] = nBlockXOff;	iStride[1] = 1;	iEdges[1] = nBlockXSize;
	break;
    }

/************************************************************************/
/*                        Read HDF SDS arrays                           */
/************************************************************************/
    switch ( poGDS->iNumType )
    {
        case DFNT_FLOAT32:
	SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (float32 *)pImage );
	break;
        case DFNT_FLOAT64:
	SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (float64 *)pImage );
	break;
        case DFNT_INT8:
	SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (int8 *)pImage );
	break;
        case DFNT_UINT8:
	SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (uint8 *)pImage );
	break;
        case DFNT_INT16:
	SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (int16 *)pImage );
	break;
        case DFNT_UINT16:
	SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (uint16 *)pImage );
	break;
        case DFNT_INT32:
	SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (int32 *)pImage );
	break;
        case DFNT_UINT32:
	SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (uint32 *)pImage );
	break;
        case DFNT_CHAR8:
	SDreaddata(poGDS->iSDS, iStart, NULL, iEdges, (char8 *)pImage);
	break;
        case DFNT_UCHAR8:
	SDreaddata(poGDS->iSDS, iStart, NULL, iEdges, (uchar8 *)pImage);
	break;
        default:
	break;
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				HDF4ImageDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HDF4ImageDataset()                      	*/
/************************************************************************/

HDF4ImageDataset::HDF4ImageDataset()

{
    fp = NULL;
    hHDF4 = 0;
    papszSubdatasetName = NULL;
}

/************************************************************************/
/*                            ~HDF4ImageDataset()                       */
/************************************************************************/

HDF4ImageDataset::~HDF4ImageDataset()

{
    if ( !papszSubdatasetName )
	CSLDestroy( papszSubdatasetName );
    if( fp != NULL )
        VSIFClose( fp );
    if( hHDF4 > 0 )
	Hclose(hHDF4);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

/*CPLErr HDF4ImageDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}*/

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

/*const char *L1BDataset::GetProjectionRef()

{
    if( bProjDetermined )
        return pszProjection;
    else
        return "";
}*/


/************************************************************************/
/*                            ComputeGeoref()				*/
/************************************************************************/

/*void HDF4ImageDataset::ComputeGeoref()
{
    if (nGCPCount >= 3 && bProjDetermined)
    {
        int bApproxOK = TRUE;
        GDALGCPsToGeoTransform( 4, pasCorners, adfGeoTransform, bApproxOK );
    }
    else
    {
        adfGeoTransform[0] = adfGeoTransform[2] = adfGeoTransform[3] = adfGeoTransform[4] = 0;
	adfGeoTransform[1] = adfGeoTransform[5] = 1;
    }
}*/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HDF4ImageDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    
    if( !EQUALN( poOpenInfo->pszFilename,"HDF4_SDS:", 9 ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HDF4ImageDataset 	*poDS;

    poDS = new HDF4ImageDataset( );

    poDS->poDriver = poHDF4ImageDriver;
    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    
    poDS->papszSubdatasetName =
	        CSLTokenizeString2( poOpenInfo->pszFilename, ":", 0 );
    poDS->pszFilename = poDS->papszSubdatasetName[2];

    if( EQUAL( poDS->papszSubdatasetName[1], "SEAWIFS_L1A" ) )
        poDS->iDataType = SEAWIFS_L1A;
    else if( EQUAL( poDS->papszSubdatasetName[1], "MODIS_L1B" ) )
        poDS->iDataType = MODIS_L1B;
    else if( EQUAL( poDS->papszSubdatasetName[1], "MOD02QKM_L1B" ) )
        poDS->iDataType = MOD02QKM_L1B;
    else
	poDS->iDataType = UNKNOWN;

    // Does our file still here?
    if ( !Hishdf( poDS->pszFilename ) )
	return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int32	hHDF4;
    int32	iDataset = atoi( poDS->papszSubdatasetName[3] );
    
    hHDF4 = Hopen( poDS->pszFilename, DFACC_READ, 0 );
    
    if( hHDF4 <= 0 )
        return( NULL );

    
/* -------------------------------------------------------------------- */
/*      Open HDF SDS interface.                                         */
/* -------------------------------------------------------------------- */
    poDS->hSD = SDstart( poDS->pszFilename, DFACC_READ );
    if ( poDS->hSD == -1 )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Fetch dataset dimensions and band datatype.	                                */
/* -------------------------------------------------------------------- */
    poDS->iSDS = SDselect( poDS->hSD, iDataset );
    SDgetinfo( poDS->iSDS, poDS->szName, &poDS->iRank, poDS->aiDimSizes,
	       &poDS->iNumType, &poDS->nAttrs);

/* -------------------------------------------------------------------- */
/*		Read SDS Attributes.				        */
/* -------------------------------------------------------------------- */
    int32	iAttribute, nValues, iAttrNumType;
    char	szAttrName[MAX_NC_NAME];

    // Loop trough the all attributes
    for ( iAttribute = 0; iAttribute < poDS->nAttrs; iAttribute++ )
    {
        // Get information about the attribute.
        SDattrinfo( poDS->iSDS, iAttribute, szAttrName, &iAttrNumType, &nValues );
        poDS->TranslateHDF4Attributes( poDS->iSDS, iAttribute, szAttrName,
			               iAttrNumType, nValues );
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    switch( poDS->iRank )
    {
	case 2:
	poDS->nBands = 1;
        poDS->nRasterXSize = poDS->aiDimSizes[1];
        poDS->nRasterYSize = poDS->aiDimSizes[0];
        break;
	case 3:
	switch( poDS->iDataType)
	{
            case MODIS_L1B:
	    case MOD02QKM_L1B:
	    poDS->nBands = poDS->aiDimSizes[0];
            poDS->nRasterXSize = poDS->aiDimSizes[2];
            poDS->nRasterYSize = poDS->aiDimSizes[1];
            break;
	    default:
	    poDS->nBands = poDS->aiDimSizes[2];
            poDS->nRasterXSize = poDS->aiDimSizes[1];
            poDS->nRasterYSize = poDS->aiDimSizes[0];
	    break;
	}
	break;
	case 4:
	poDS->nBands = poDS->aiDimSizes[2] * poDS->aiDimSizes[3];
        break;
	default:
	break;
    }
    for( i = 1; i <= poDS->nBands; i++ )
        poDS->SetBand( i, new HDF4ImageRasterBand( poDS, i ) );

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_HDF4Image()				*/
/************************************************************************/

void GDALRegister_HDF4Image()

{
    GDALDriver	*poDriver;

    if( poHDF4ImageDriver == NULL )
    {
        poHDF4ImageDriver = poDriver = new GDALDriver();
        
        poDriver->SetDescription( "HDF4Image" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "HDF4 Internal Dataset" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_hdf4.html" );

        poDriver->pfnOpen = HDF4ImageDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

