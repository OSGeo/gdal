/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Read subdatasets of HDF4 file.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:   Andrey Kiselev, dron@at1895.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@at1895.spb.edu>
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
    int32	iSDS, iGR, iPal;
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

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
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

    		HDF4ImageRasterBand( HDF4ImageDataset *, int );
    
    GDALDataType GetDataType( int32 );
    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
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
    int32	iStart[MAX_DIMS], iEdges[MAX_DIMS];

    switch ( poGDS->iSubdatasetType )
    {
	case HDF4_SDS:
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
    	    break;
        }
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
    	    break;
        }
	break;
	default:
	break;
    }

    return CE_None;
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
    fp = NULL;
    hHDF4 = 0;
    papszSubdatasetName = NULL;
    iSubdatasetType = HDF4_UNKNOWN;
    papszLocalMetadata = NULL;
    poColorTable = NULL;
    pszProjection = "";
}

/************************************************************************/
/*                            ~HDF4ImageDataset()                       */
/************************************************************************/

HDF4ImageDataset::~HDF4ImageDataset()

{
    if ( !papszSubdatasetName )
	CSLDestroy( papszSubdatasetName );
    if ( !papszLocalMetadata )
	CSLDestroy( papszLocalMetadata );
    if( fp != NULL )
        VSIFClose( fp );
    if( hHDF4 > 0 )
	Hclose(hHDF4);
    if( poColorTable != NULL )
        delete poColorTable;

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
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *HDF4ImageDataset::GetProjectionRef()

{
    return pszProjection;
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
    if( poTransform != NULL 
        && poTransform->Transform(1, pdfGeoX, pdfGeoY,NULL) == OGRERR_NONE )

    if( poTransform != NULL )
        CPLFree( poTransform );

    if( poLatLong != NULL )
        CPLFree( poLatLong );
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
	        CSLTokenizeString2( poOpenInfo->pszFilename, ":", 0 );
    poDS->pszFilename = poDS->papszSubdatasetName[2];

    if( EQUAL( poDS->papszSubdatasetName[0], "HDF4_SDS" ) )
	poDS->iSubdatasetType = HDF4_SDS;
    else if ( EQUAL( poDS->papszSubdatasetName[0], "HDF4_GR" ) )
        poDS->iSubdatasetType = HDF4_GR;
    else
	poDS->iSubdatasetType = HDF4_UNKNOWN;
    
    if( EQUAL( poDS->papszSubdatasetName[1], "SEAWIFS_L1A" ) )
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
    int32	iDataset = atoi( poDS->papszSubdatasetName[3] );
    int32	iAttribute, nValues, iAttrNumType;
    char	szAttrName[MAX_NC_NAME];
    
    poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_READ, 0 );
    
    if( poDS->hHDF4 <= 0 )
        return( NULL );

    switch ( poDS->iSubdatasetType )
    {
        case HDF4_SDS:
        poDS->hSD = SDstart( poDS->pszFilename, DFACC_READ );
        if ( poDS->hSD == -1 )
           return NULL;
        
	if ( poDS->ReadGlobalAttributes( poDS->hSD ) != CE_None )
            return NULL;
    
        poDS->iSDS = SDselect( poDS->hSD, iDataset );
        SDgetinfo( poDS->iSDS, poDS->szName, &poDS->iRank, poDS->aiDimSizes,
	           &poDS->iNumType, &poDS->nAttrs);

        for ( iAttribute = 0; iAttribute < poDS->nAttrs; iAttribute++ )
        {
            SDattrinfo( poDS->iSDS, iAttribute, szAttrName, &iAttrNumType, &nValues );
            poDS->papszLocalMetadata =
		poDS->TranslateHDF4Attributes( poDS->iSDS, iAttribute,
		    szAttrName, iAttrNumType, nValues, poDS->papszLocalMetadata );
        }
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
        
	poDS->iGR = GRselect( poDS->hGR, iDataset );
       	if ( GRgetiminfo( poDS->iGR, poDS->szName, &poDS->iRank, &poDS->iNumType,
			  &poDS->iInterlaceMode, poDS->aiDimSizes, &poDS->nAttrs ) != 0 )
	    return NULL;

        // Read Attributes:
        // Loop trough the all attributes
        /*for ( iAttribute = 0; iAttribute < poDS->nAttrs; iAttribute++ )
        {
            // Get information about the attribute.
            GRattrinfo( poDS->iGR, iAttribute, szAttrName, &iAttrNumType, &nValues );
            papszLocalMetadata = poDS->TranslateHDF4Attributes( poDS->iGR, iAttribute, szAttrName,
			                   iAttrNumType, nValues, papszLocalMetadata );
	}*/
	// Read color table
	GDALColorEntry oEntry;
	 
	poDS->iPal = GRgetlutid ( poDS->iGR, iDataset );
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
        poDS->SetBand( i, new HDF4ImageRasterBand( poDS, i ) );

/* -------------------------------------------------------------------- */
/*      Read projection information                                     */
/* -------------------------------------------------------------------- */
    /*char 	**papszProjParmList;*/
    int		iUTMZone;
    double	dfULX, dfULY, dfURX, dfURY, dfLLX, dfLLY, dfLRX, dfLRY;
    double	dfCenterX, dfCenterY;
    OGRSpatialReference oSRS;
    poDS->adfGeoTransform[0] = poDS->adfGeoTransform[2] =
        poDS->adfGeoTransform[3] = poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[1] = poDS->adfGeoTransform[5] = 1.0;
    switch ( poDS->iDataType )
    {
        case ASTER_L1B:
        /*papszProjParmList = CSLTokenizeString2(
			poDS->GetMetadataItem( CPLSPrintf("PROJECTIONPARAMETERS%c%c",
			poDS->szName[9], poDS->szName[10]), 0 ),", " , 0 );*/
	if ( strlen( poDS->szName ) >= 10 &&
	     EQUAL( CSLFetchNameValue( poDS->papszGlobalMetadata,
		CPLSPrintf("MPMETHOD%s", &poDS->szName[9]) ), "UTM" ) )
	{
            oSRS.SetProjCS( "UTM" );
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
	    
	    /*oProj.SetGeogCS( "ASTER_PROJ", "ASTER_DATUM", "ASTER_SPHEROID",
			    atof(papszProjParmList[0]), // Semi-major axis
			    1/(atof(papszProjParmList[0])/atof(papszProjParmList[1]) - 1.0), // Inverse Flattening
			    NULL,
			    0,
			    );*/
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
            oSRS.SetProjCS( "UTM" );
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
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    GDALRasterBand  *poBand;
    int32	    hSD, iSDS, iRank = 2;
    int32	    iStart[MAX_DIMS], iEdges[MAX_DIMS];
    int		    iXDim = 1, iYDim = 0, iBandDim = 2;
    int		    nBlockXSize, nBlockYSize;
    int		    iBand, iXBlock, iYBlock;
    GDALDataType    iNumType;
    CPLErr	    eErr = CE_None;
    GByte	    *pabyData;
    int32	    aiDimSizes[MAX_VAR_DIMS];

    hSD = SDstart( pszFilename, DFACC_CREATE );
    aiDimSizes[iXDim] = nXSize;
    aiDimSizes[iYDim] = nYSize;
    //aiDimSizes[iBandDim] = nBands;

    for( iBand = 0; iBand < nBands; iBand++ )
    {
	poBand = poSrcDS->GetRasterBand( iBand + 1);
	poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
	iNumType = poBand->GetRasterDataType();
	pabyData = (GByte *) CPLMalloc( nBlockXSize * nBlockYSize *
					GDALGetDataTypeSize( iNumType ) / 8 );
	
	switch ( iNumType )
	{
	    case GDT_Float64:
	    iSDS = SDcreate( hSD, poBand->GetDescription(), DFNT_FLOAT64, iRank, aiDimSizes );
	    break;
	    case GDT_Float32:
	    iSDS = SDcreate( hSD, poBand->GetDescription(), DFNT_FLOAT32, iRank, aiDimSizes );
	    break;
	    case GDT_UInt32:
	    iSDS = SDcreate( hSD, poBand->GetDescription(), DFNT_UINT32, iRank, aiDimSizes );
	    break;
	    case GDT_UInt16:
	    iSDS = SDcreate( hSD, poBand->GetDescription(), DFNT_UINT16, iRank, aiDimSizes );
	    break;
	    case GDT_Int32:
	    iSDS = SDcreate( hSD, poBand->GetDescription(), DFNT_INT32, iRank, aiDimSizes );
	    break;
	    case GDT_Int16:
	    iSDS = SDcreate( hSD, poBand->GetDescription(), DFNT_INT16, iRank, aiDimSizes );
	    break;
	    case GDT_Byte:
	    default:
	    iSDS = SDcreate( hSD, poBand->GetDescription(), DFNT_UCHAR8, iRank, aiDimSizes );
	    break;
	}
	
	for( iYBlock = 0; iYBlock < nYSize; iYBlock += nBlockYSize )
	{
	    for( iXBlock = 0; iXBlock < nXSize; iXBlock += nBlockXSize )
	    {
		eErr = poBand->ReadBlock( iXBlock, iYBlock, pabyData );
		
		iStart[iBandDim] = iBand;
		iEdges[iBandDim] = 1;
	    
		iStart[iYDim] = iYBlock;
		iEdges[iYDim] = nBlockYSize;
	    
		iStart[iXDim] = iXBlock;
		iEdges[iXDim] = nBlockXSize;
		
		SDwritedata( iSDS, iStart, NULL, iEdges, (VOIDP)pabyData );
	    }
	    
            if( eErr == CE_None &&
            !pfnProgress( (iYBlock + 1 + iBand * nYSize) / ((double) nYSize * nBands),
			 NULL, pProgressData) )
            {
                eErr = CE_Failure;
                CPLError( CE_Failure, CPLE_UserInterrupt, 
                      "User terminated CreateCopy()" );
            }
	}
	
	CPLFree( pabyData );
    }
/* -------------------------------------------------------------------- */
/*      Set global attributes.                                          */
/* -------------------------------------------------------------------- */
    const char *pszSignature =
	"Created with GDAL (http://www.remotesensing.org/gdal/)";
    
    SDsetattr( hSD, "Signature", DFNT_CHAR8, strlen(pszSignature), pszSignature );
    
/* -------------------------------------------------------------------- */
/*      That's all, folks.                                              */
/* -------------------------------------------------------------------- */
    SDendaccess( iSDS );
    SDend( hSD );
    
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

        poDriver->pfnOpen = HDF4ImageDataset::Open;
        poDriver->pfnCreateCopy = HDF4ImageCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

