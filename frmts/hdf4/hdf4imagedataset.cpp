/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Read subdatasets of HDF4 file.
 *           This driver initially based on code supplied by Markus Neteler
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
 * Revision 1.20  2003/01/27 16:45:16  dron
 * Fixed problems with wrong count in SDsetattr() calls.
 *
 * Revision 1.19  2002/12/30 15:07:45  dron
 * SetProjCS() call removed.
 *
 * Revision 1.18  2002/12/19 19:20:20  dron
 * Wrong comments removed.
 *
 * Revision 1.17  2002/12/02 19:07:55  dron
 * Added SetMetadata()/SetMetadataItem().
 *
 * Revision 1.16  2002/11/23 18:54:17  warmerda
 * added CREATIONDATATYPES metadata for drivers
 *
 * Revision 1.15  2002/11/13 06:43:36  warmerda
 * honour quoted strings when tokenizing filename
 *
 * Revision 1.14  2002/11/12 09:16:52  dron
 * Added rank choosing in Create() method.
 *
 * Revision 1.13  2002/11/11 16:43:50  dron
 * Create() method now really works.
 *
 * Revision 1.12  2002/11/08 18:29:04  dron
 * Added Create() method.
 *
 * Revision 1.11  2002/11/07 13:23:44  dron
 * Support for projection information writing.
 *
 * Revision 1.10  2002/11/06 15:47:14  dron
 * Added support for 3D datasets creation
 *
 * Revision 1.9  2002/10/25 14:28:54  dron
 * Initial support for HDF4 creation.
 *
 * Revision 1.8  2002/09/06 11:47:23  dron
 * Fixes in pixel sizes determining for ASTER L1B
 *
 * Revision 1.7  2002/09/06 10:42:23  dron
 * Georeferencing for ASTER Level 1b datasets and ASTER DEMs.
 *
 * Revision 1.6  2002/07/23 12:27:58  dron
 * General Raster Interface support added.
 *
 * Revision 1.5  2002/07/18 08:27:05  dron
 * Improved multidimesional SDS arrays handling.
 *
 * Revision 1.4  2002/07/17 16:24:31  dron
 * MODIS support improved a bit.
 *
 * Revision 1.3  2002/07/17 13:36:18  dron
 * <hdf.h> and <mfhdf.h> changed to "hdf.h" and "mfhdf.h".
 *
 * Revision 1.2  2002/07/16 17:51:10  warmerda
 * removed hdf/ from include statements
 *
 * Revision 1.1  2002/07/16 11:04:11  dron
 * New driver: HDF4 datasets. Initial version.
 *
 *
 */

#include <string.h>

#include "hdf.h"
#include "mfhdf.h"

#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

#include "hdf4dataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_HDF4(void);
CPL_C_END

// Signature to recognize files written by GDAL
const char	*pszGDALSignature =
	"Created with GDAL (http://www.remotesensing.org/gdal/)";

#define MAX_DIMS         20	/* FIXME: maximum number of dimensions in sds */

/************************************************************************/
/* ==================================================================== */
/*				HDF4ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

class HDF4ImageDataset : public HDF4Dataset
{
    friend class HDF4ImageRasterBand;

    char	**papszSubdatasetName;
    char	*pszFilename;
    HDF4SubdatasetType iSubdatasetType;
    int32	iSDS, iGR, iPal, iDataset;
    int32	iRank, iNumType, nAttrs, iInterlaceMode, iPalInterlaceMode, iPalDataType;
    int32	nComps, nPalEntries;
    int32	aiDimSizes[MAX_VAR_DIMS];
    int		iXDim, iYDim, iBandDim;
    char	**papszLocalMetadata;
#define    N_COLOR_ENTRIES    256
    uint8	aiPaletteData[N_COLOR_ENTRIES][3];	// XXX: Static array currently
    char	szName[65];

    GDALColorTable *poColorTable;

    double      adfGeoTransform[6];
    char        *pszProjection;

  public:
                HDF4ImageDataset();
		~HDF4ImageDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    virtual void FlushCache( void );
    CPLErr	GetGeoTransform( double * padfTransform );
    virtual CPLErr SetGeoTransform( double * );
    const char	*GetProjectionRef();
    virtual CPLErr SetProjection( const char * );
    CPLErr	SetMetadata( char **, const char * );
    CPLErr	SetMetadataItem( const char *, const char*, const char * );
    GDALDataType GetDataType( int32 );
    void	ReadCoordinates( const char*, double*, double* );
    void	ToUTM( OGRSpatialReference *, double *, double * );
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

    		HDF4ImageRasterBand( HDF4ImageDataset *, int, GDALDataType );
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};

/************************************************************************/
/*                           HDF4ImageRasterBand()                      */
/************************************************************************/

HDF4ImageRasterBand::HDF4ImageRasterBand( HDF4ImageDataset *poDS, int nBand,
					  GDALDataType eType)

{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = eType;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr HDF4ImageRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                        void * pImage )
{
    HDF4ImageDataset	*poGDS = (HDF4ImageDataset *) poDS;
    int32		iStart[MAX_DIMS], iEdges[MAX_DIMS];
    CPLErr		eErr = CE_None;

    if( poGDS->eAccess == GA_Update )
    {
        memset( pImage, 0,
		nBlockXSize * nBlockYSize * GDALGetDataTypeSize(eDataType) / 8 );
        return CE_None;
    }

    switch ( poGDS->iSubdatasetType )
    {
	case HDF4_SDS:
	poGDS->iSDS = SDselect( poGDS->hSD, poGDS->iDataset );
        /* HDF rank:
        A rank 2 dataset is an image read in scan-line order (2D). 
        A rank 3 dataset is a series of images which are read in an image
	at a time to form a volume.
        A rank 4 dataset may be thought of as a series of volumes.

        The "iStart" array specifies the multi-dimensional index of the
	starting corner of the hyperslab to read. The values are zero based.

        The "edge" array specifies the number of values to read along each
        dimension of the hyperslab.

        The "iStride" array allows for sub-sampling along each dimension. If a
        iStride value is specified for a dimension, that many values will be
        skipped over when reading along that dimension. Specifying
	iStride = NULL in the C interface or iStride = 1 in either
	interface specifies contiguous reading of data. If the iStride
	values are set to 0, SDreaddata returns FAIL (or -1). No matter
	what iStride value is provided, data is always placed contiguously
	in buffer.
     
        See also:
        http://www.dur.ac.uk/~dcs0elb/au-case-study/code/hdf-browse.c.html
        http://dao.gsfc.nasa.gov/DAO_people/yin/quads.code.html
        */
        switch ( poGDS->iRank )
        {
            case 4:	// 4Dim: volume-time
    	    // FIXME: needs sample file. Does not works currently.
    	    iStart[3] = 0/* range: 0--aiDimSizes[3]-1 */;	iEdges[3] = 1;
    	    iStart[2] = 0/* range: 0--aiDimSizes[2]-1 */;	iEdges[2] = 1;
    	    iStart[1] = nBlockYOff; iEdges[1] = nBlockYSize;
    	    iStart[0] = nBlockXOff; iEdges[0] = nBlockXSize;
    	    break;
            case 3: // 3Dim: volume
    	    iStart[poGDS->iBandDim] = nBand - 1;
    	    iEdges[poGDS->iBandDim] = 1;
    	
    	    iStart[poGDS->iYDim] = nBlockYOff;
    	    iEdges[poGDS->iYDim] = nBlockYSize;
    	
    	    iStart[poGDS->iXDim] = nBlockXOff;
    	    iEdges[poGDS->iXDim] = nBlockXSize;
    	    break;
            case 2: // 2Dim: rows/cols
            iStart[poGDS->iYDim] = nBlockYOff;
	    iEdges[poGDS->iYDim] = nBlockYSize;
	
            iStart[poGDS->iXDim] = nBlockXOff;
	    iEdges[poGDS->iXDim] = nBlockXSize;
	    break;
        }
        // Read HDF SDS arrays
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
	    eErr = CE_Failure;
    	    break;
        }
	poGDS->iSDS = SDendaccess( poGDS->iSDS );
        break;
	case HDF4_GR:
	iStart[poGDS->iYDim] = nBlockYOff;
	iEdges[poGDS->iYDim] = nBlockYSize;
	
	iStart[poGDS->iXDim] = nBlockXOff;
	iEdges[poGDS->iXDim] = nBlockXSize;
        switch ( poGDS->iNumType )
        {
            case DFNT_FLOAT32:
    	    GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (float32 *)pImage );
    	    break;
            case DFNT_FLOAT64:
    	    GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (float64 *)pImage );
    	    break;
            case DFNT_INT8:
    	    GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (int8 *)pImage );
    	    break;
            case DFNT_UINT8:
    	    GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (uint8 *)pImage );
    	    break;
            case DFNT_INT16:
    	    GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (int16 *)pImage );
    	    break;
            case DFNT_UINT16:
    	    GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (uint16 *)pImage );
    	    break;
            case DFNT_INT32:
    	    GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (int32 *)pImage );
    	    break;
            case DFNT_UINT32:
    	    GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (uint32 *)pImage );
    	    break;
            case DFNT_CHAR8:
    	    GRreadimage(poGDS->iGR, iStart, NULL, iEdges, (char8 *)pImage);
    	    break;
            case DFNT_UCHAR8:
    	    GRreadimage(poGDS->iGR, iStart, NULL, iEdges, (uchar8 *)pImage);
    	    break;
            default:
	    eErr = CE_Failure;
    	    break;
        }
	break;
	default:
	eErr = CE_Failure;
	break;
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr HDF4ImageRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
					 void * pImage )
{
    HDF4ImageDataset	*poGDS = (HDF4ImageDataset *)poDS;
    int32		iStart[MAX_DIMS], iEdges[MAX_DIMS];
    CPLErr		eErr = CE_None;

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    switch ( poGDS->iRank )
    {
	case 3:
	iStart[poGDS->iBandDim] = nBand - 1;
	iEdges[poGDS->iBandDim] = 1;
    
	iStart[poGDS->iYDim] = nBlockYOff;
	iEdges[poGDS->iYDim] = nBlockYSize;
    
	iStart[poGDS->iXDim] = nBlockXOff;
	iEdges[poGDS->iXDim] = nBlockXSize;
	if ( (SDwritedata( poGDS->iSDS, iStart, NULL,
			   iEdges, (VOIDP)pImage )) < 0 )
	    eErr = CE_Failure;
	break;
	case 2:
	poGDS->iSDS = SDselect( poGDS->hSD, nBand - 1 );
	iStart[poGDS->iYDim] = nBlockYOff;
	iEdges[poGDS->iYDim] = nBlockYSize;
    
	iStart[poGDS->iXDim] = nBlockXOff;
	iEdges[poGDS->iXDim] = nBlockXSize;
	if ( (SDwritedata( poGDS->iSDS, iStart, NULL,
			   iEdges, (VOIDP)pImage )) < 0 )
	    eErr = CE_Failure;
	poGDS->iSDS = SDendaccess( poGDS->iSDS );
	break;
	default:
	eErr = CE_Failure;
	break;
    }

    return eErr;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *HDF4ImageRasterBand::GetColorTable()
{
    HDF4ImageDataset	*poGDS = (HDF4ImageDataset *) poDS;

    return poGDS->poColorTable;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp HDF4ImageRasterBand::GetColorInterpretation()
{
    HDF4ImageDataset	*poGDS = (HDF4ImageDataset *) poDS;

    if ( poGDS->iSubdatasetType == HDF4_SDS )
        return GCI_GrayIndex;
    else if ( poGDS->iSubdatasetType == HDF4_GR )
    {
	if ( poGDS->poColorTable != NULL )
            return GCI_PaletteIndex;
	else if ( poGDS->nBands != 1 )
	{
	    if ( nBand == 1 )
                return GCI_RedBand;
            else if ( nBand == 2 )
                return GCI_GreenBand;
            else if ( nBand == 3 )
                return GCI_BlueBand;
            else if ( nBand == 4 )
                return GCI_AlphaBand;
            else
                return GCI_Undefined;
	}
	else
	    return GCI_GrayIndex;
    }
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/* ==================================================================== */
/*				HDF4ImageDataset			*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HDF4ImageDataset()                      	*/
/************************************************************************/

HDF4ImageDataset::HDF4ImageDataset()
{
    hSD = 0;
    hGR = 0;
    iSDS = 0;
    iGR = 0;
    papszSubdatasetName = NULL;
    iSubdatasetType = HDF4_UNKNOWN;
    papszLocalMetadata = NULL;
    poColorTable = NULL;
    pszProjection = CPLStrdup( "" );
}

/************************************************************************/
/*                            ~HDF4ImageDataset()                       */
/************************************************************************/

HDF4ImageDataset::~HDF4ImageDataset()
{
    FlushCache();
    
    if ( iSDS > 0 )
	SDendaccess( iSDS );
    if ( hSD > 0 )
	SDend( hSD );
    if ( iGR > 0 )
	GRendaccess( iGR );
    if ( hGR > 0 )
	GRend( hGR );
    if ( papszSubdatasetName )
	CSLDestroy( papszSubdatasetName );
    if ( papszLocalMetadata )
	CSLDestroy( papszLocalMetadata );
    if ( poColorTable != NULL )
        delete poColorTable;
    if ( pszProjection )
	CPLFree( pszProjection );

}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HDF4ImageDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr HDF4ImageDataset::SetGeoTransform( double * padfTransform )
{
    CPLErr		eErr = CE_None;
    const char		*pszValue;

    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );
    pszValue = CPLSPrintf( "%f, %f, %f, %f, %f, %f",
			  adfGeoTransform[0], adfGeoTransform[1],
			  adfGeoTransform[2], adfGeoTransform[3],
			  adfGeoTransform[4], adfGeoTransform[5] );
    if ( (SDsetattr( hSD, "TransformationMatrix", DFNT_CHAR8,
		     strlen(pszValue) + 1, pszValue )) < 0 )
    {
	CPLDebug( "HDF4Image",
		  "Can't write transformation matrix to output file" );
	eErr = CE_Failure;
    }

    CPLDebug( "HDF4Image",
	      "SetGeoTransform() succeeds with return code %d", eErr );
    
    return eErr;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *HDF4ImageDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr HDF4ImageDataset::SetProjection( const char *pszNewProjection )

{
    CPLErr		eErr = CE_None;

    if ( pszProjection )
	CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );
    if ( (SDsetattr( hSD, "Projection", DFNT_CHAR8,
		     strlen(pszProjection) + 1, pszProjection )) < 0 )
    {
	CPLDebug( "HDF4Image",
		  "Can't write projection information to output file");
	eErr = CE_Failure;
    }

    CPLDebug( "HDF4Image",
	      "SetProjection() succeeds with return code %d", eErr );

    return eErr;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/
CPLErr HDF4ImageDataset::SetMetadata( char ** papszMetadata,
				      const char *pszDomain )

{
    const char	*pszValue;
    char	*pszName;
    
    // Store all metadata from source dataset as HDF attributes
    if ( papszMetadata )
    {
	while ( *papszMetadata )
	{
	    pszValue = CPLParseNameValue( *papszMetadata++, &pszName );
	    SDsetattr( hSD, pszName, DFNT_CHAR8, strlen(pszValue) + 1, pszValue );
	}
    }

    return GDALDataset::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr HDF4ImageDataset::SetMetadataItem( const char *pszName, 
				          const char *pszValue,
                                          const char *pszDomain )

{
    if ( pszName && pszValue )
	SDsetattr( hSD, pszName, DFNT_CHAR8, strlen(pszValue) + 1, pszValue );
    
    return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void HDF4ImageDataset::FlushCache()

{
    GDALDataset::FlushCache();
}

/************************************************************************/
/*		Translate HDF4 data type into GDAL data type		*/
/************************************************************************/
GDALDataType HDF4ImageDataset::GetDataType( int32 iNumType )
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
	return GDT_Float32;
	break;
        case DFNT_FLOAT64:
	return GDT_Float64;
	break;
	default:
	return GDT_Unknown;
	break;
    }
}

/************************************************************************/
/*                                ToUTM()				*/
/************************************************************************/

void HDF4ImageDataset::ToUTM( OGRSpatialReference *poProj,
				double *pdfGeoX, double *pdfGeoY )
{
/* -------------------------------------------------------------------- */
/*      Setup transformation to lat/long.                               */
/* -------------------------------------------------------------------- */
    OGRCoordinateTransformation *poTransform = NULL;
    OGRSpatialReference *poLatLong = NULL;
    poLatLong = poProj->CloneGeogCS();
    poTransform = OGRCreateCoordinateTransformation( poLatLong, poProj );
    
/* -------------------------------------------------------------------- */
/*      Transform to latlong and report.                                */
/* -------------------------------------------------------------------- */
    if( poTransform != NULL )
	poTransform->Transform( 1, pdfGeoX, pdfGeoY, NULL );
	
    if( poTransform != NULL )
        delete poTransform;

    if( poLatLong != NULL )
        delete poLatLong;
}

/************************************************************************/
/*                            ReadCoordinates()				*/
/************************************************************************/

void HDF4ImageDataset::ReadCoordinates( const char *pszString,
					double *pdfX, double *pdfY )
{
    char **papszStrList;
    papszStrList = CSLTokenizeString2( pszString, ", ", 0 );
    *pdfX = atof(papszStrList[0]);
    *pdfY = atof(papszStrList[1]);
    CPLFree( papszStrList );
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HDF4ImageDataset::Open( GDALOpenInfo * poOpenInfo )
{
    int		i;
    
    if( !EQUALN( poOpenInfo->pszFilename, "HDF4_SDS:", 9 ) &&
	!EQUALN( poOpenInfo->pszFilename, "HDF4_GR:", 8 ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HDF4ImageDataset 	*poDS;

    poDS = new HDF4ImageDataset( );

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

    poDS->papszSubdatasetName =
	        CSLTokenizeString2( poOpenInfo->pszFilename, ":", 
                                    CSLT_HONOURSTRINGS );
    poDS->pszFilename = poDS->papszSubdatasetName[2];

    if( EQUAL( poDS->papszSubdatasetName[0], "HDF4_SDS" ) )
	poDS->iSubdatasetType = HDF4_SDS;
    else if ( EQUAL( poDS->papszSubdatasetName[0], "HDF4_GR" ) )
        poDS->iSubdatasetType = HDF4_GR;
    else
	poDS->iSubdatasetType = HDF4_UNKNOWN;
    
    if( EQUAL( poDS->papszSubdatasetName[1], "GDAL_HDF4" ) )
        poDS->iDataType = GDAL_HDF4;
    else if( EQUAL( poDS->papszSubdatasetName[1], "SEAWIFS_L1A" ) )
        poDS->iDataType = SEAWIFS_L1A;
    else if( EQUAL( poDS->papszSubdatasetName[1], "ASTER_L1B" ) )
        poDS->iDataType = ASTER_L1B;
    else if( EQUAL( poDS->papszSubdatasetName[1], "AST14DEM" ) )
        poDS->iDataType = AST14DEM;
    else if( EQUAL( poDS->papszSubdatasetName[1], "MODIS_L1B" ) )
        poDS->iDataType = MODIS_L1B;
    else if( EQUAL( poDS->papszSubdatasetName[1], "MOD02QKM_L1B" ) )
        poDS->iDataType = MOD02QKM_L1B;
    else if( EQUAL( poDS->papszSubdatasetName[1], "MODIS_UNK" ) )
        poDS->iDataType = MOD02QKM_L1B;
    else
	poDS->iDataType = UNKNOWN;

    // Does our file still here?
    if ( !Hishdf( poDS->pszFilename ) )
	return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int32	iAttribute, nValues, iAttrNumType;
    char	szAttrName[MAX_NC_NAME];
    
    if( poOpenInfo->eAccess == GA_ReadOnly )
	poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_READ, 0 );
    else
	poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_WRITE, 0 );
    
    if( poDS->hHDF4 <= 0 )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Select SDS or GR for reading from.                              */
/* -------------------------------------------------------------------- */
    poDS->iDataset = atoi( poDS->papszSubdatasetName[3] );
    switch ( poDS->iSubdatasetType )
    {
        case HDF4_SDS:
        poDS->hSD = SDstart( poDS->pszFilename, DFACC_READ );
        if ( poDS->hSD == -1 )
           return NULL;
        
	if ( poDS->ReadGlobalAttributes( poDS->hSD ) != CE_None )
            return NULL;
    
        poDS->iSDS = SDselect( poDS->hSD, poDS->iDataset );
        SDgetinfo( poDS->iSDS, poDS->szName, &poDS->iRank, poDS->aiDimSizes,
	           &poDS->iNumType, &poDS->nAttrs);

        for ( iAttribute = 0; iAttribute < poDS->nAttrs; iAttribute++ )
        {
            SDattrinfo( poDS->iSDS, iAttribute, szAttrName,
			&iAttrNumType, &nValues );
            poDS->papszLocalMetadata =
		poDS->TranslateHDF4Attributes( poDS->iSDS, iAttribute,
		    szAttrName, iAttrNumType, nValues, poDS->papszLocalMetadata );
        }
	poDS->iSDS = SDendaccess( poDS->iSDS );
        // Create band information objects.
        switch( poDS->iRank )
        {
	    case 2:
	    poDS->nBands = 1;
            poDS->iXDim = 1;
            poDS->iYDim = 0;
            break;
	    case 3:
	    if( poDS->aiDimSizes[0] < poDS->aiDimSizes[2] )
	    {
	        poDS->iBandDim = 0;
                poDS->iXDim = 2;
                poDS->iYDim = 1;
	    }
	    else
	    {
	        poDS->iBandDim = 2;
                poDS->iXDim = 1;
                poDS->iYDim = 0;
	    }
            poDS->nBands = poDS->aiDimSizes[poDS->iBandDim];
	    break;
	    case 4: // FIXME
	    poDS->nBands = poDS->aiDimSizes[2] * poDS->aiDimSizes[3];
            break;
	    default:
	    break;
        }
	break;
	case HDF4_GR:
        poDS->hGR = GRstart( poDS->hHDF4 );
        if ( poDS->hGR == -1 )
           return NULL;
        
	poDS->iGR = GRselect( poDS->hGR, poDS->iDataset );
       	if ( GRgetiminfo( poDS->iGR, poDS->szName,
			  &poDS->iRank, &poDS->iNumType,
			  &poDS->iInterlaceMode, poDS->aiDimSizes,
			  &poDS->nAttrs ) != 0 )
	    return NULL;

        for ( iAttribute = 0; iAttribute < poDS->nAttrs; iAttribute++ )
        {
            GRattrinfo( poDS->iGR, iAttribute, szAttrName,
			&iAttrNumType, &nValues );
            poDS->papszLocalMetadata = 
		poDS->TranslateHDF4Attributes( poDS->iGR, iAttribute,
		    szAttrName, iAttrNumType, nValues, poDS->papszLocalMetadata );
	}
	// Read colour table
	GDALColorEntry oEntry;
	 
	poDS->iPal = GRgetlutid ( poDS->iGR, poDS->iDataset );
	if ( poDS->iPal != -1 )
	{
	    GRgetlutinfo( poDS->iPal, &poDS->nComps, &poDS->iPalDataType,
	                  &poDS->iPalInterlaceMode, &poDS->nPalEntries );
	    GRreadlut( poDS->iPal, poDS->aiPaletteData );
	    poDS->poColorTable = new GDALColorTable();
	    for( i = 0; i < N_COLOR_ENTRIES; i++ )
	    {
                oEntry.c1 = poDS->aiPaletteData[i][0];
                oEntry.c2 = poDS->aiPaletteData[i][1];
                oEntry.c3 = poDS->aiPaletteData[i][2];
		oEntry.c4 = 255;
		
                poDS->poColorTable->SetColorEntry( i, &oEntry );
	    }
	}

        poDS->iXDim = 0;
	poDS->iYDim = 1;
        poDS->nBands = poDS->iRank;
	break;
	default:
	return NULL;
	break;
    }
    
    poDS->nRasterXSize = poDS->aiDimSizes[poDS->iXDim];
    poDS->nRasterYSize = poDS->aiDimSizes[poDS->iYDim];
    for( i = 1; i <= poDS->nBands; i++ )
        poDS->SetBand( i, new HDF4ImageRasterBand( poDS, i,
				    poDS->GetDataType( poDS->iNumType ) ) );

/* -------------------------------------------------------------------- */
/*      Read projection information                                     */
/* -------------------------------------------------------------------- */
    int		    iUTMZone;
    double	    dfULX, dfULY, dfURX, dfURY, dfLLX, dfLLY, dfLRX, dfLRY;
    double	    dfCenterX, dfCenterY;
    const char	    *pszValue;
    OGRSpatialReference oSRS;
    //int32		iStart[MAX_DIMS], iEdges[MAX_DIMS];

    poDS->adfGeoTransform[0] = poDS->adfGeoTransform[2] =
	poDS->adfGeoTransform[3] = poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[1] = poDS->adfGeoTransform[5] = 1.0;
    
    switch ( poDS->iDataType )
    {
	case GDAL_HDF4:
	if ( (pszValue =
	      CSLFetchNameValue(poDS->papszGlobalMetadata, "Projection")) )
	{
	    if ( poDS->pszProjection )
		CPLFree( poDS->pszProjection );
	    poDS->pszProjection = CPLStrdup( pszValue );
	}
	if ( (pszValue =
	      CSLFetchNameValue(poDS->papszGlobalMetadata, "TransformationMatrix")) )
	{
	    int i = 0;
	    char *pszString = (char *) pszValue; 
	    while ( *pszValue && i < 6 )
	    {
		poDS->adfGeoTransform[i++] = strtod( pszString, &pszString );
		pszString++;
	    }
	}
	for( i = 1; i <= poDS->nBands; i++ )
	{
	    if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata,
					       CPLSPrintf("BandDesc%d", i))) )
		poDS->GetRasterBand( i )->SetDescription( pszValue );
	}
	break;
	case ASTER_L1B:
	// Read geolocation points
	/*poDS->iSDS = SDselect( poDS->hSD, poDS->iDataset );
	iStart[poGDS->iYDim] = nBlockYOff;
	iEdges[poGDS->iYDim] = nBlockYSize;
    
	iStart[poGDS->iXDim] = nBlockXOff;
	iEdges[poGDS->iXDim] = nBlockXSize;
	SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (float64 *)pImage );*/
	if ( strlen( poDS->szName ) >= 10 &&
	     EQUAL( CSLFetchNameValue( poDS->papszGlobalMetadata,
		CPLSPrintf("MPMETHOD%s", &poDS->szName[9]) ), "UTM" ) )
	{
	    oSRS.SetWellKnownGeogCS( "WGS84" );
            
	    poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "UPPERLEFT" ), &dfULY, &dfULX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "UPPERRIGHT" ), &dfURY, &dfURX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "LOWERLEFT" ), &dfLLY, &dfLLX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "LOWERRIGHT" ), &dfLRY, &dfLRX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "SCENECENTER" ),
		                &dfCenterY, &dfCenterX );
	    
	    iUTMZone = atoi( CSLFetchNameValue( poDS->papszGlobalMetadata,
			    CPLSPrintf("UTMZONECODE%s", &poDS->szName[9]) ) );
	    if( iUTMZone > 0 )
		oSRS.SetUTM( iUTMZone, TRUE );
	    else
		oSRS.SetUTM( - iUTMZone, FALSE );

	    oSRS.exportToWkt( &poDS->pszProjection );

	    poDS->ToUTM( &oSRS, &dfULX, &dfULY );
            poDS->ToUTM( &oSRS, &dfURX, &dfURY );
            poDS->ToUTM( &oSRS, &dfLLX, &dfLLY );
            poDS->ToUTM( &oSRS, &dfLRX, &dfLRY );
            
	    // Determine pixel sizes for different bands
	    if( EQUAL(&poDS->szName[9], "1") || EQUAL(&poDS->szName[9], "2") ||
		EQUAL(&poDS->szName[9], "3N") || EQUAL(&poDS->szName[9], "3B") )
	        poDS->adfGeoTransform[1] = poDS->adfGeoTransform[5] = 15;     // VNIR, 15 m
	    else if ( EQUAL(&poDS->szName[9], "4") || EQUAL(&poDS->szName[9], "5") ||
	              EQUAL(&poDS->szName[9], "6") || EQUAL(&poDS->szName[9], "7") || 
		      EQUAL(&poDS->szName[9], "8") || EQUAL(&poDS->szName[9], "9") )
		poDS->adfGeoTransform[1] = poDS->adfGeoTransform[5] = 30;     // SWIR, 30 m
	    else
		poDS->adfGeoTransform[1] = poDS->adfGeoTransform[5] = 90;     // TIR, 90 m
	    if( dfCenterY > 0 )
		poDS->adfGeoTransform[5] = -poDS->adfGeoTransform[5];
	    
            poDS->adfGeoTransform[0] = dfLLX + poDS->adfGeoTransform[1] / 2;
            poDS->adfGeoTransform[3] = dfULY - poDS->adfGeoTransform[5] / 2;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[4] = 0.0;
	}
	break;
	case AST14DEM:
	    oSRS.SetWellKnownGeogCS( "WGS84" );
            
	    poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "UPPERLEFT" ), &dfULY, &dfULX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "UPPERRIGHT" ), &dfURY, &dfURX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "LOWERLEFT" ), &dfLLY, &dfLLX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "LOWERRIGHT" ), &dfLRY, &dfLRX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
		poDS->papszGlobalMetadata, "SCENECENTER" ),
		                &dfCenterY, &dfCenterX );
            
	    // Calculate UTM zone from scene center coordinates
            iUTMZone = 30 + (int) ((dfCenterX + 3.0) / 6.0);
	    if( dfCenterY > 0 )			// FIXME: does it right?
		oSRS.SetUTM( iUTMZone, TRUE );
	    else
		oSRS.SetUTM( - iUTMZone, FALSE );
	    oSRS.exportToWkt( &poDS->pszProjection );
            
	    poDS->ToUTM( &oSRS, &dfULX, &dfULY );
            poDS->ToUTM( &oSRS, &dfURX, &dfURY );
            poDS->ToUTM( &oSRS, &dfLLX, &dfLLY );
            poDS->ToUTM( &oSRS, &dfLRX, &dfLRY );
            
	    // Pixel size always 30 m for ASTER DEMs
	    poDS->adfGeoTransform[1] = 30;
	    if( dfCenterY > 0 )
	        poDS->adfGeoTransform[5] = -30;
            else	    
	        poDS->adfGeoTransform[5] = 30;

            poDS->adfGeoTransform[0] = dfLLX + poDS->adfGeoTransform[1] / 2;
            poDS->adfGeoTransform[3] = dfULY - poDS->adfGeoTransform[5] / 2;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[4] = 0.0;
	break;
	default:
	break;
    }

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *HDF4ImageDataset::Create( const char * pszFilename,
				       int nXSize, int nYSize, int nBands,
				       GDALDataType eType,
				       char **papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    HDF4ImageDataset	*poDS;
    const char		*pszSDSName;
    int			iBand;
    int32		aiDimSizes[MAX_VAR_DIMS];

    poDS = new HDF4ImageDataset();

/* -------------------------------------------------------------------- */
/*      Choose rank for the created dataset.                            */
/* -------------------------------------------------------------------- */
    poDS->iRank = 3;
    if ( CSLFetchNameValue( papszOptions, "RANK" ) != NULL &&
	 EQUAL( CSLFetchNameValue( papszOptions, "RANK" ), "2" ) )
	poDS->iRank = 2;
    
    poDS->hSD = SDstart( pszFilename, DFACC_CREATE );
    if ( poDS->hSD == -1 )
    {
	CPLError( CE_Failure, CPLE_OpenFailed,
		  "Can't create HDF4 file %s", pszFilename );
	return NULL;
    }
    poDS->iXDim = 1;
    poDS->iYDim = 0;
    poDS->iBandDim = 2;
    aiDimSizes[poDS->iXDim] = nXSize;
    aiDimSizes[poDS->iYDim] = nYSize;
    aiDimSizes[poDS->iBandDim] = nBands;

    if ( poDS->iRank == 2 )
    {
	for ( iBand = 0; iBand < nBands; iBand++ )
	{
	    pszSDSName = CPLSPrintf( "Band%d", iBand );
	    switch ( eType )
	    {
		case GDT_Float64:
		poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_FLOAT64,
				       poDS->iRank, aiDimSizes );
		break;
		case GDT_Float32:
		poDS->iSDS = SDcreate(poDS-> hSD, pszSDSName, DFNT_FLOAT32,
				      poDS->iRank, aiDimSizes );
		break;
		case GDT_UInt32:
		poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT32,
				       poDS->iRank, aiDimSizes );
		break;
		case GDT_UInt16:
		poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT16,
				       poDS->iRank, aiDimSizes );
		break;
		case GDT_Int32:
		poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT32,
				       poDS->iRank, aiDimSizes );
		break;
		case GDT_Int16:
		poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT16,
				       poDS->iRank, aiDimSizes );
		break;
		case GDT_Byte:
		default:
		poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UCHAR8,
				       poDS->iRank, aiDimSizes );
		break;
	    }
	    poDS->iSDS = SDendaccess( poDS->iSDS );
	}
    }
    else if ( poDS->iRank == 3 )
    {
	pszSDSName = "3-dimensional Scientific Dataset";
	switch ( eType )
	{
	    case GDT_Float64:
	    poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_FLOAT64,
				   poDS->iRank, aiDimSizes );
	    break;
	    case GDT_Float32:
	    poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_FLOAT32,
				   poDS->iRank, aiDimSizes );
	    break;
	    case GDT_UInt32:
	    poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT32,
				   poDS->iRank, aiDimSizes );
	    break;
	    case GDT_UInt16:
	    poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT16,
				   poDS->iRank, aiDimSizes );
	    break;
	    case GDT_Int32:
	    poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT32,
				   poDS->iRank, aiDimSizes );
	    break;
	    case GDT_Int16:
	    poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT16,
				   poDS->iRank, aiDimSizes );
	    break;
	    case GDT_Byte:
	    default:
	    poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UCHAR8,
				   poDS->iRank, aiDimSizes );
	    break;
	}
    }
    else					    // Should never happen
	return NULL;

    if ( poDS->iSDS < 0 )
    {
	CPLError( CE_Failure, CPLE_AppDefined,
		  "Can't create SDS with rank %d for file %s",
		  poDS->iRank, pszFilename );
	return NULL;
    }

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->iSubdatasetType = HDF4_SDS;
    poDS->iDataType = GDAL_HDF4;
    poDS->nBands = nBands;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( iBand = 1; iBand <= nBands; iBand++ )
        poDS->SetBand( iBand, new HDF4ImageRasterBand( poDS, iBand, eType ) );

    SDsetattr( poDS->hSD, "Signature", DFNT_CHAR8, strlen(pszGDALSignature) + 1,
	       pszGDALSignature );
    
    return (GDALDataset *) poDS;
}

/************************************************************************/
/*                      HDF4ImageCreateCopy()                           */
/************************************************************************/

static GDALDataset *
HDF4ImageCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                    int bStrict, char ** papszOptions, 
                    GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Choose rank for the created dataset. We will write 3D dataset   */
/*      by default (if all bands has equal bit depths) and set of       */
/*      2D datasets by user request.                                    */
/* -------------------------------------------------------------------- */
    int32	    iRank = 3;
    int		    iBand;
    GDALRasterBand  *poBand;
    GDALDataType    eType;

    if ( CSLFetchNameValue( papszOptions, "RANK" ) != NULL &&
	 EQUAL( CSLFetchNameValue( papszOptions, "RANK" ), "2" ) )
	iRank = 2;

    if ( iRank == 3 )
    {
	iBand = 1;
	poBand = poSrcDS->GetRasterBand( iBand );
	eType = poBand->GetRasterDataType();
	while( iBand <= nBands )
	{
	    poBand = poSrcDS->GetRasterBand( iBand++ );
	    if ( eType != poBand->GetRasterDataType() )
	    {
		CPLDebug("HDF4Image", "Can't create 3D SDS because of different"
			 "band depths. Set of 2D SDSs will be created");
		iRank = 2;
		break;
	    }
	}
    }

/* -------------------------------------------------------------------- */
/*      Do we need compression?                                         */
/*      NOTE: current NCSA HDF library implementation do not allow      */
/*            partial modification to a compressed datastream.          */
/*            Disabled for a future times.                              */
/* -------------------------------------------------------------------- */
#if 0
    int		iCompType = COMP_CODE_NONE; // Without compression by default
    comp_info	sCompInfo;
    
    if ( CSLFetchNameValue( papszOptions, "COMPRESS" ) != NULL )
    {
	if ( EQUAL( CSLFetchNameValue( papszOptions, "COMPRESS" ), "RLE" ) )
	{
	    iCompType = COMP_CODE_RLE;
	}
	else if ( EQUAL( CSLFetchNameValue( papszOptions, "COMPRESS" ), "HUFFMAN" ) )
	{
	    iCompType = COMP_CODE_SKPHUFF;
	    sCompInfo.skphuff.skp_size = 1;
	}
	else if ( EQUAL( CSLFetchNameValue( papszOptions, "COMPRESS" ), "DEFLATE" ) )
	{
	    iCompType = COMP_CODE_DEFLATE;
	    sCompInfo.deflate.level = 9;
	}
    }
#endif

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    int32	    hSD, iSDS;
    int32	    iStart[MAX_DIMS], iEdges[MAX_DIMS];
    int		    iXDim = 1, iYDim = 0, iBandDim = 2;
    int		    nBlockXSize, nBlockYSize;
    int		    iXBlock, iYBlock;
    int		    nXBlocks, nYBlocks;
    CPLErr	    eErr = CE_None;
    GByte	    *pabyData;
    const char	    *pszSDSName;
    int32	    aiDimSizes[MAX_VAR_DIMS];

    hSD = SDstart( pszFilename, DFACC_CREATE );
    aiDimSizes[iXDim] = nXSize;
    aiDimSizes[iYDim] = nYSize;
    aiDimSizes[iBandDim] = nBands;

    if ( iRank == 2 )
    {
	for( iBand = 0; iBand < nBands; iBand++ )
	{
	    poBand = poSrcDS->GetRasterBand( iBand + 1 );
	    poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
	    nXBlocks = (poBand->GetXSize() + nBlockXSize - 1) / nBlockXSize;
	    nYBlocks = (poBand->GetYSize() + nBlockYSize - 1) / nBlockYSize;
	    eType = poBand->GetRasterDataType();
	    pabyData = (GByte *) CPLMalloc( nBlockXSize * nBlockYSize *
					    GDALGetDataTypeSize( eType ) / 8 );
	    pszSDSName = poBand->GetDescription();
	    
	    switch ( eType )
	    {
		case GDT_Float64:
		iSDS = SDcreate( hSD, pszSDSName, DFNT_FLOAT64, iRank, aiDimSizes );
		break;
		case GDT_Float32:
		iSDS = SDcreate( hSD, pszSDSName, DFNT_FLOAT32, iRank, aiDimSizes );
		break;
		case GDT_UInt32:
		iSDS = SDcreate( hSD, pszSDSName, DFNT_UINT32, iRank, aiDimSizes );
		break;
		case GDT_UInt16:
		iSDS = SDcreate( hSD, pszSDSName, DFNT_UINT16, iRank, aiDimSizes );
		break;
		case GDT_Int32:
		iSDS = SDcreate( hSD, pszSDSName, DFNT_INT32, iRank, aiDimSizes );
		break;
		case GDT_Int16:
		iSDS = SDcreate( hSD, pszSDSName, DFNT_INT16, iRank, aiDimSizes );
		break;
		case GDT_Byte:
		default:
		iSDS = SDcreate( hSD, pszSDSName, DFNT_UCHAR8, iRank, aiDimSizes );
		break;
	    }
#if 0
	    if ( iCompType != COMP_CODE_NONE )
	    {
		if ( iCompType != COMP_CODE_SKPHUFF )
		    sCompInfo.skphuff.skp_size =
			GDALGetDataTypeSize( eType ) / 8;
		SDsetcompress( iSDS, iCompType, &sCompInfo ); 
	    }
#endif
	    
	    for( iYBlock = 0; iYBlock < nYBlocks; iYBlock++ )
	    {
		for( iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
		{
		    eErr = poBand->ReadBlock( iXBlock, iYBlock, pabyData );
		
		    iStart[iYDim] = iYBlock * nBlockYSize;
		    iEdges[iYDim] = nBlockYSize;
		
		    iStart[iXDim] = iXBlock * nBlockXSize;
		    iEdges[iXDim] = nBlockXSize;
		    
		    SDwritedata( iSDS, iStart, NULL, iEdges, (VOIDP)pabyData );
		}
		
		if( eErr == CE_None &&
		!pfnProgress( (iYBlock + 1 + iBand * nYBlocks) / ((double) nYBlocks * nBands),
			     NULL, pProgressData) )
		{
		    eErr = CE_Failure;
		    CPLError( CE_Failure, CPLE_UserInterrupt, 
			  "User terminated CreateCopy()" );
		}
	    }
	    
	    CPLFree( pabyData );
	}
    }
    else if ( iRank == 3 )
    {
	pszSDSName = "3-dimensional Scientific Dataset";
	switch ( eType )
	{
	    case GDT_Float64:
	    iSDS = SDcreate( hSD, pszSDSName, DFNT_FLOAT64, iRank, aiDimSizes );
	    break;
	    case GDT_Float32:
	    iSDS = SDcreate( hSD, pszSDSName, DFNT_FLOAT32, iRank, aiDimSizes );
	    break;
	    case GDT_UInt32:
	    iSDS = SDcreate( hSD, pszSDSName, DFNT_UINT32, iRank, aiDimSizes );
	    break;
	    case GDT_UInt16:
	    iSDS = SDcreate( hSD, pszSDSName, DFNT_UINT16, iRank, aiDimSizes );
	    break;
	    case GDT_Int32:
	    iSDS = SDcreate( hSD, pszSDSName, DFNT_INT32, iRank, aiDimSizes );
	    break;
	    case GDT_Int16:
	    iSDS = SDcreate( hSD, pszSDSName, DFNT_INT16, iRank, aiDimSizes );
	    break;
	    case GDT_Byte:
	    default:
	    iSDS = SDcreate( hSD, pszSDSName, DFNT_UCHAR8, iRank, aiDimSizes );
	    break;
	}
#if 0
	if ( iCompType != COMP_CODE_NONE )
	    if ( iCompType != COMP_CODE_SKPHUFF )
		sCompInfo.skphuff.skp_size = GDALGetDataTypeSize( eType ) / 8;
	    SDsetcompress( iSDS, iCompType, &sCompInfo );
#endif

	for( iBand = 0; iBand < nBands; iBand++ )
	{
	    poBand = poSrcDS->GetRasterBand( iBand + 1 );
	    poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
	    nXBlocks = (poBand->GetXSize() + nBlockXSize - 1) / nBlockXSize;
	    nYBlocks = (poBand->GetYSize() + nBlockYSize - 1) / nBlockYSize;
	    pabyData = (GByte *) CPLMalloc( nBlockXSize * nBlockYSize *
					    GDALGetDataTypeSize( eType ) / 8 );
	    
	    for( iYBlock = 0; iYBlock < nYBlocks; iYBlock++ )
	    {
		for( iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
		{
		    eErr = poBand->ReadBlock( iXBlock, iYBlock, pabyData );
		    
		    iStart[iBandDim] = iBand;
		    iEdges[iBandDim] = 1;
		
		    iStart[iYDim] = iYBlock * nBlockYSize;
		    iEdges[iYDim] = nBlockYSize;
		
		    iStart[iXDim] = iXBlock * nBlockXSize;
		    iEdges[iXDim] = nBlockXSize;
		    
		    SDwritedata( iSDS, iStart, NULL, iEdges, (VOIDP)pabyData );
		}
		
		if( eErr == CE_None &&
		!pfnProgress( (iYBlock + 1 + iBand * nYBlocks) /
			      ((double) nYBlocks * nBands),
			     NULL, pProgressData) )
		{
		    eErr = CE_Failure;
		    CPLError( CE_Failure, CPLE_UserInterrupt, 
			  "User terminated CreateCopy()" );
		}
	    }
	    
	    CPLFree( pabyData );
	}
    }
    else					    // Should never happen
	return NULL;

/* -------------------------------------------------------------------- */
/*      Set global attributes.                                          */
/* -------------------------------------------------------------------- */
    const char	*pszValue;
    char	*pszKey;
    char	**papszMetadata;
    double	adfGeoTransform[6];
    
    SDsetattr( hSD, "Signature", DFNT_CHAR8, strlen(pszGDALSignature) + 1,
	       pszGDALSignature );

    pszValue = poSrcDS->GetProjectionRef();
    if ( pszValue != NULL && !EQUAL( pszValue, "" ) )
	SDsetattr( hSD, "Projection", DFNT_CHAR8, strlen(pszValue) + 1, pszValue );

    pszValue = poSrcDS->GetGCPProjection();
    if ( pszValue != NULL && !EQUAL( pszValue, "" ) )
	SDsetattr( hSD, "GCPProjection", DFNT_CHAR8, strlen(pszValue) + 1, pszValue );

    poSrcDS->GetGeoTransform( adfGeoTransform );
    pszValue = CPLSPrintf( "%f, %f, %f, %f, %f, %f",
			  adfGeoTransform[0], adfGeoTransform[1],
			  adfGeoTransform[2], adfGeoTransform[3],
			  adfGeoTransform[4], adfGeoTransform[5] );
    SDsetattr( hSD, "TransformationMatrix", DFNT_CHAR8, strlen(pszValue) + 1, pszValue );
    
    for( iBand = 1; iBand <= nBands; iBand++ )
    {
	poBand = poSrcDS->GetRasterBand( iBand );
	pszValue = poBand->GetDescription();
	pszKey = (char *)CPLSPrintf( "BandDesc%d", iBand );
	if ( pszValue != NULL && !EQUAL( pszValue, "" ) )
	    SDsetattr( hSD, pszKey, DFNT_CHAR8, strlen(pszValue) + 1, pszValue );
    }

    // Store all metadata from source dataset as HDF attributes
    papszMetadata = poSrcDS->GetMetadata();
    if ( papszMetadata )
    {
	while ( *papszMetadata )
	{
	    pszValue = CPLParseNameValue( *papszMetadata++, &pszKey );
	    SDsetattr( hSD, pszKey, DFNT_CHAR8, strlen(pszValue) + 1, pszValue );
	}
    }
    
/* -------------------------------------------------------------------- */
/*      That's all, folks.                                              */
/* -------------------------------------------------------------------- */
    iSDS = SDendaccess( iSDS );
    hSD = SDend( hSD );
    
    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                        GDALRegister_HDF4Image()			*/
/************************************************************************/

void GDALRegister_HDF4Image()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "HDF4Image" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "HDF4Image" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "HDF4 Dataset" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_hdf4.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 Float32 Float64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='RANK' type='int' description='Rank of output SDS'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = HDF4ImageDataset::Open;
        poDriver->pfnCreate = HDF4ImageDataset::Create;
        poDriver->pfnCreateCopy = HDF4ImageCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

