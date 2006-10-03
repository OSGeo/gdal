/******************************************************************************
 * $Id$
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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
 * Revision 1.25  2006/10/03 20:40:49  dnadeau
 * CreateCopy: Fix _fillvalue type problem and Add GDAL geotransform information
 * if input data is Geographic.
 * Delete "lat lon" coordinates in GDAL information to make it more CF-1
 * compatible.
 *
 * Revision 1.24  2006/08/07 18:35:25  dnadeau
 * Also check if latitude is equally spaced before setting projection for lat lon
 * array case.
 *
 * Revision 1.23  2006/04/28 19:28:45  dnadeau
 * CreateCopy added.
 * Generate CF compliant netCDF files.
 * Generation on the fly of geolocation arrays.
 *
 * Revision 1.22  2006/04/10 20:25:21  dnadeau
 * generalized identification of the X and Y; 
 * collaping all dimension  
 * other than X and Y into bands when reading; 
 * SUBDATASET mechanism for datasets having multiple geospatial data arrays;  
 * identify (via metadata) geolocation data
 *
 * Revision 1.21  2005/10/12 20:14:30  fwarmerdam
 * Use SetMetadataItem() for bands.
 *
 * Revision 1.20  2005/08/29 16:26:41  hobu
 * move the declaration up instead of having it
 * inlined so it is easy to find
 *
 * Revision 1.19  2005/08/29 16:16:31  hobu
 * according to msvc 7.1, var on line 1099 was already
 * declared on line 946.  removed the declaration and it
 * builds fine now
 *
 * Revision 1.18  2005/08/27 19:46:59  dnadeau
 * add rint function for Windows
 *
 * Revision 1.17  2005/08/26 22:37:22  dnadeau
 * add WGS84 for lat/lon data
 *
 * Revision 1.16  2005/08/26 22:02:09  fwarmerdam
 * Changed to use # instead of : as the separator between variable name
 * and attribute name in metadata, since : is reserved.
 * Set WGS84 as the GEOGCS for projected coordinate systems.
 * Don't set WGS84 as a GEOGCS if we don't recognise the projection.
 *
 * Revision 1.15  2005/08/26 21:34:53  dnadeau
 * support projections (UTM and LCC)
 *
 * Revision 1.14  2005/08/25 23:10:44  dnadeau
 * add metadata for all bands
 *
 * Revision 1.13  2005/08/25 22:38:44  dnadeau
 * add xdim and ydim as label for x and y dimension
 *
 * Revision 1.12  2005/08/19 20:07:55  dnadeau
 * add netcdf band metadata info
 *
 * Revision 1.11  2005/08/18 17:00:31  dnadeau
 * fix latitude/longitude variable name identification
 *
 * Revision 1.9  2005/08/17 21:43:16  dnadeau
 * support CF convention and lat/long proj.
 *
 * Revision 1.8  2005/07/29 16:21:37  fwarmerdam
 * use CPLIsNan, and float.h
 *
 * Revision 1.7  2005/07/29 02:33:32  dnadeau
 * enhance netcdf to conform to CF convention.  
 * Allow multidimensionnal array and read metatdata
 *
 * Revision 1.6  2005/05/05 15:54:49  fwarmerdam
 * PAM Enabled
 *
 * Revision 1.5  2004/10/16 14:57:31  fwarmerdam
 * Substantial rewrite by Radim to sometimes handle COARDS style datasets
 * but really only under some circumstances.   CreateCopy() removed since
 * no COARDS implementation is available.
 *
 * Revision 1.4  2004/10/14 14:51:31  fwarmerdam
 * Fixed last fix.
 *
 * Revision 1.3  2004/10/14 13:59:13  fwarmerdam
 * Added error for non-GMT netCDF files.
 *
 * Revision 1.2  2004/01/07 21:02:19  warmerda
 * fix up driver metadata
 *
 * Revision 1.1  2004/01/07 20:05:53  warmerda
 * New
 *
 */

#include "netcdfdataset.h"

CPL_CVSID("$Id$");


/************************************************************************/
/* ==================================================================== */
/*                         netCDFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand : public GDALPamRasterBand
{
    nc_type nc_datatype;
    int         nZId;
    int         nZDim;
    int		nLevel;
    int         nBandXPos;
    int         nBandYPos;
    int         *panBandZPos;
    int         *panBandZLev;
    int         bNoDataSet;
    double      dfNoDataValue;
    CPLErr	    CreateBandMetadata( ); 
    
  public:

    netCDFRasterBand( netCDFDataset *poDS, 
		      int nZId, 
		      int nZDim,
		      int nLevel, 
		      int *panBandZLen,
		      int *panBandPos, 
		      int nBand );
    ~netCDFRasterBand( );
    virtual double          GetNoDataValue( int * );
    virtual CPLErr          SetNoDataValue( double );
    virtual CPLErr IReadBlock( int, int, void * );


};

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/
char **netCDFDataset::GetMetadata( const char *pszDomain )
{
    if( pszDomain != NULL && EQUALN( pszDomain, "SUBDATASETS", 11 ) )
        return papszSubDatasets;
    else
        return GDALDataset::GetMetadata( pszDomain );
}


/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char * netCDFDataset::GetProjectionRef()
{
    if( bGotGeoTransform )
	return pszProjection;
    else
	return "";
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double netCDFRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataSet;

    return dfNoDataValue;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr netCDFRasterBand::SetNoDataValue( double dfNoData )

{
    bNoDataSet = TRUE;
    dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/*                         ~netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::~netCDFRasterBand()
{
    if( panBandZPos ) 
	CPLFree( panBandZPos );
    if( panBandZLev )
	CPLFree( panBandZLev );
}

/************************************************************************/
/*                         CreateBandMetadata()                         */
/************************************************************************/

CPLErr netCDFRasterBand::CreateBandMetadata( ) 
{
    char     szVarName[NC_MAX_NAME];
    char     szMetaName[NC_MAX_NAME];
    char     szMetaTemp[8192];
    int      nd;
    int      i,j;
    int      Sum  = 1;
    int      Taken = 0;
    int      result = 0;
    int      status;
    int      nVarID = -1;
    int      nDims;
    size_t   start[1];
    size_t   count[1];
    char     szTemp[NC_MAX_NAME];
    const char *pszValue;

    nc_type nVarType;
    netCDFDataset *poDS;

    poDS = (netCDFDataset *) this->poDS;
/* -------------------------------------------------------------------- */
/*      Compute all dimensions from Band number and save in Metadata    */
/* -------------------------------------------------------------------- */
    nc_inq_varname( poDS->cdfid, nZId, szVarName );
    nc_inq_varndims( poDS->cdfid, nZId, &nd );
/* -------------------------------------------------------------------- */
/*      Compute multidimention band position                            */
/*                                                                      */
/* BandPosition = (Total - sum(PastBandLevels) - 1)/sum(remainingLevels)*/
/* if Data[2,3,4,x,y]                                                   */
/*                                                                      */
/*  BandPos0 = (nBand ) / (3*4)                                         */
/*  BandPos1 = (nBand - BandPos0*(3*4) ) / (4)                          */
/*  BandPos2 = (nBand - BandPos0*(3*4) ) % (4)                          */
/* -------------------------------------------------------------------- */

    sprintf( szMetaName,"NETCDF_VARNAME");
    sprintf( szMetaTemp,"%s",szVarName);
    SetMetadataItem( szMetaName, szMetaTemp );
    if( nd == 3 ) {
	Sum *= panBandZLev[0];
    }

    for( i=0; i < nd-2 ; i++ ) {
	if( i != nd - 2 -1 ) {
	    for( j=i+1; j < nd-2; j++ ) {
		Sum *= panBandZLev[j];
	    }
	    result = (int) ( ( nLevel-Taken ) / Sum );
	}
	else {
	    result = (int) ( ( nLevel-Taken ) % Sum );
	}

	strcpy(szVarName, poDS->papszDimName[poDS->paDimIds[
			  panBandZPos[i]]] );

	sprintf( szMetaName,"NETCDF_DIMENSION_%s",  szVarName );

	status=nc_inq_varid(poDS->cdfid,  
			    szVarName,
			    &nVarID );

/* -------------------------------------------------------------------- */
/*      Try to uppercase the first letter of the variable               */
/* -------------------------------------------------------------------- */

	if( status != NC_NOERR ) {
	    szVarName[0]=toupper(szVarName[0]);
	    status=nc_inq_varid(poDS->cdfid,  
				szVarName,
				&nVarID );
	}

	status = nc_inq_vartype( poDS->cdfid, nVarID, &nVarType );

	nDims = 0;
	status = nc_inq_varndims( poDS->cdfid, nVarID, &nDims );

	if( nDims == 1 ) {
	    count[0]=1;
	    start[0]=result;
	    switch( nVarType ) {
	    case NC_SHORT:
		short sData;
		status =  nc_get_vara_short( poDS->cdfid, nVarID, 
					     start,
					     count, &sData );
		sprintf( szMetaTemp,"%d", sData );
		break;
	    case NC_INT:
		int nData;
		status =  nc_get_vara_int( poDS->cdfid, nVarID, 
					   start,
					   count, &nData );
		sprintf( szMetaTemp,"%d", nData );
		break;
	    case NC_FLOAT:
		float fData;
		status =  nc_get_vara_float( poDS->cdfid, nVarID, 
					     start,
					     count, &fData );
		sprintf( szMetaTemp,"%f", fData );
		break;
	    case NC_DOUBLE:
		double dfData;
		status =  nc_get_vara_double( poDS->cdfid, nVarID, 
					      start,
					      count, &dfData);
		sprintf( szMetaTemp,"%g", dfData );
		break;
	    default:
		break;
	    }
	}
	else
	    sprintf( szMetaTemp,"%d", result+1);
	
        SetMetadataItem( szMetaName, szMetaTemp );

/* -------------------------------------------------------------------- */
/*      Fetch dimension units                                           */
/* -------------------------------------------------------------------- */

	strcpy( szTemp, szVarName );
	strcat( szTemp, "#units" );
	pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
	if( pszValue != NULL ) {
	    if( EQUAL( pszValue, "T") ) { 
		strcpy( szTemp, szVarName );
		strcat( szTemp, "#original_units" );
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		strcpy( szTemp, "NETCDF_");
		strcat( szTemp, szVarName );
		strcat( szTemp, "_original_units" );
		SetMetadataItem( szTemp, pszValue );
	    }
	    else {
		strcpy( szTemp, "NETCDF_");
		strcat( szTemp, szVarName  );
		strcat( szTemp, "_units" );
		SetMetadataItem( szTemp, pszValue );
	    }
	}
	Taken += result * Sum;
    }



    return CE_None;
}

/************************************************************************/
/*                          netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::netCDFRasterBand( netCDFDataset *poDS, 
				    int nZId, 
				    int nZDim,
				    int nLevel, 
				    int *panBandZLev, 
				    int *panBandDimPos, 
				    int nBand)

{
    double   dfNoData;
    int      bNoDataSet = FALSE;
    nc_type  vartype=NC_NAT;
    nc_type  atttype=NC_NAT;
    size_t   attlen;
    int      status;
    char     szNoValueName[8192];


    this->panBandZPos = NULL;
    this->panBandZLev = NULL;
    this->poDS = poDS;
    this->nBand = nBand;
    this->nZId = nZId;
    this->nZDim = nZDim;
    this->nLevel = nLevel;
    this->nBandXPos = panBandDimPos[0];
    this->nBandYPos = panBandDimPos[1];

/* -------------------------------------------------------------------- */
/*      Take care of all other dimmensions                              */
/* ------------------------------------------------------------------ */
    if( nZDim > 2 ) {
	this->panBandZPos = 
	    (int *) CPLCalloc( nZDim-2, sizeof( int ) );
	this->panBandZLev = 
	    (int *) CPLCalloc( nZDim-2, sizeof( int ) );

	for ( int i=0; i < nZDim - 2; i++ ){
	    this->panBandZPos[i] = panBandDimPos[i+2];
	    this->panBandZLev[i] = panBandZLev[i];
	}
    }
    CreateBandMetadata();
    bNoDataSet    = FALSE;
    dfNoDataValue = -9999.0;

    nBlockXSize   = poDS->GetRasterXSize( );
    nBlockYSize   = 1;

/* -------------------------------------------------------------------- */
/*      Get the type of the "z" variable, our target raster array.      */
/* -------------------------------------------------------------------- */
    if( nc_inq_var( poDS->cdfid, nZId, NULL, &nc_datatype, NULL, NULL,
                   NULL ) != NC_NOERR ){
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Error in nc_var_inq() on 'z'." );
        return;
    }

    if( nc_datatype == NC_BYTE )
        eDataType = GDT_Byte;
    else if( nc_datatype == NC_SHORT )
        eDataType = GDT_Int16;
    else if( nc_datatype == NC_INT )
        eDataType = GDT_Int32;
    else if( nc_datatype == NC_FLOAT )
        eDataType = GDT_Float32;
    else if( nc_datatype == NC_DOUBLE )
        eDataType = GDT_Float64;
    else
    {
        if( nBand == 1 )
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Unsupported netCDF datatype (%d), treat as Float32.", 
                      (int) nc_datatype );
        eDataType = GDT_Float32;
    }
/* -------------------------------------------------------------------- */
/*      Find out what is No Data for this variable                      */
/* -------------------------------------------------------------------- */

    status = nc_inq_att( poDS->cdfid, nZId, 
			 _FillValue, &atttype, &attlen);

/* -------------------------------------------------------------------- */
/*      Look for either Missing_Value or _FillValue attributes          */
/* -------------------------------------------------------------------- */

    if( status == NC_NOERR ) {
	strcpy(szNoValueName, _FillValue );
    }
    else {
	status = nc_inq_att( poDS->cdfid, nZId, 
			     "missing_value", &atttype, &attlen );
	if( status == NC_NOERR ) {

	    strcpy( szNoValueName, "missing_value" );
	}
    }

    nc_inq_vartype( poDS->cdfid, nZId, &vartype );

    if( status == NC_NOERR ) {
	switch( atttype ) {
	case NC_CHAR:
	    char *fillc;
	    fillc = (char *) CPLCalloc( attlen+1, sizeof(char) );
	    status=nc_get_att_text( poDS->cdfid, nZId,
				    szNoValueName, fillc );
	    dfNoData = atof( fillc );
	    CPLFree(fillc);
	    break;
	case NC_SHORT:
	    short sNoData;
	    status = nc_get_att_short( poDS->cdfid, nZId,
				       szNoValueName, &sNoData );
	    dfNoData = (double) sNoData;
	    break;
	case NC_INT:
	    int nNoData;
	    status = nc_get_att_int( poDS->cdfid, nZId,
				     szNoValueName, &nNoData );
	    dfNoData = (double) nNoData;
	    break;
	case NC_FLOAT:
	    float fNoData;
	    status = nc_get_att_float( poDS->cdfid, nZId,
				       szNoValueName, &fNoData );
	    dfNoData = (double) fNoData;
	    break;
	case NC_DOUBLE:
	    status = nc_get_att_double( poDS->cdfid, nZId,
					 szNoValueName, &dfNoData );
	    break;
	default:
	    break;
	}
	status = nc_get_att_double( poDS->cdfid, nZId, 
				    szNoValueName, &dfNoData );
	
    } else {
	switch( vartype ) {
	case NC_BYTE:
	    /* don't do default fill-values for bytes, too risky */
	    dfNoData = 0.0;
	    break;
	case NC_CHAR:
	    dfNoData = NC_FILL_CHAR;
	    break;
	case NC_SHORT:
	    dfNoData = NC_FILL_SHORT;
	    break;
	case NC_INT:
	    dfNoData = NC_FILL_INT;
	    break;
	case NC_FLOAT:
	    dfNoData = NC_FILL_FLOAT;
	    break;
	case NC_DOUBLE:
	    dfNoData = NC_FILL_DOUBLE;
	    break;
	default:
	    dfNoData = 0.0;
	    break;
	}
	    bNoDataSet = TRUE;
    }
    SetNoDataValue( dfNoData );
    
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr netCDFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    int    nErr=-1;
    int    cdfid = ( ( netCDFDataset * ) poDS )->cdfid;
    size_t start[ MAX_NC_DIMS ];
    size_t edge[ MAX_NC_DIMS ];
    char   pszName[ MAX_STR_LEN ];
    int    i,j;
    int    Sum=-1;
    int    Taken=-1;
    int    nd;

    *pszName='\0';
    memset( start, 0, sizeof( start ) );
    memset( edge,  0, sizeof( edge )  );
    nc_inq_varndims ( cdfid, nZId, &nd );

/* -------------------------------------------------------------------- */
/*      Locate X, Y and Z position in the array                         */
/* -------------------------------------------------------------------- */
	
    start[nBandXPos] = 0;          // x dim can move arround in array
    start[nBandYPos] = nBlockYOff; // y
        
    edge[nBandXPos] = nBlockXSize; 
    edge[nBandYPos] = 1;

    if( nd == 3 ) {
        start[panBandZPos[0]]  = nLevel;     // z
        edge [panBandZPos[0]]  = 1;
    }

/* -------------------------------------------------------------------- */
/*      Compute multidimention band position                            */
/*                                                                      */
/* BandPosition = (Total - sum(PastBandLevels) - 1)/sum(remainingLevels)*/
/* if Data[2,3,4,x,y]                                                   */
/*                                                                      */
/*  BandPos0 = (nBand ) / (3*4)                                         */
/*  BandPos1 = (nBand - (3*4) ) / (4)                                   */
/*  BandPos2 = (nBand - (3*4) ) % (4)                                   */
/* -------------------------------------------------------------------- */
    if( nd > 3 ) {
	for( i=0; i < nd-2-1 ; i++ ) {
	    Sum  = 1;
	    Taken = 0;
	    for( j=i+1; j < nd-2; j++ ) {
		Sum *= panBandZLev[j];
	    }
	    start[panBandZPos[i]] = ( nLevel-Taken ) / Sum;
	    edge[panBandZPos[i]] = 1;
	    Taken += Sum;
	}
	start[panBandZPos[i]] = ( nLevel-( Taken-Sum ) ) % Sum;
	edge[panBandZPos[i]] = 1;
    }



    if( eDataType == GDT_Byte )
        nErr = nc_get_vara_uchar( cdfid, nZId, start, edge, 
                                  (unsigned char *) pImage );
    else if( eDataType == GDT_Int16 )
        nErr = nc_get_vara_short( cdfid, nZId, start, edge, 
                                  (short int *) pImage );
    else if( eDataType == GDT_Int32 )
    {
        if( sizeof(long) == 4 )
            nErr = nc_get_vara_long( cdfid, nZId, start, edge, 
                                     (long *) pImage );
        else
            nErr = nc_get_vara_int( cdfid, nZId, start, edge, 
                                    (int *) pImage );
    }
    else if( eDataType == GDT_Float32 ){
        nErr = nc_get_vara_float( cdfid, nZId, start, edge, 
                                  (float *) pImage );
	for( i=0; i<nBlockXSize; i++ ){
	    if( CPLIsNan( ( (float *) pImage )[i] ) )
		( (float *)pImage )[i] = dfNoDataValue;
	}
    }
    else if( eDataType == GDT_Float64 ){
        nErr = nc_get_vara_double( cdfid, nZId, start, edge, 
                                   (double *) pImage );
	for( i=0; i<nBlockXSize; i++ ){
	    if( CPLIsNan( ( (double *) pImage)[i] ) ) 
		( (double *)pImage )[i] = dfNoDataValue;
	}

    }

    if( nErr != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "netCDF scanline fetch failed: %s", 
                  nc_strerror( nErr ) );
        return CE_Failure;
    }
    else
        return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				netCDFDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           netCDFDataset()                            */
/************************************************************************/

netCDFDataset::netCDFDataset()

{
    papszMetadata    = NULL;	
    papszGeolocation = NULL;
    papszSubDatasets = NULL;
    bGotGeoTransform = FALSE;
    pszProjection    = NULL;
    papszName        = NULL;
    pszFilename      = NULL;
    cdfid             = 0;
}


/************************************************************************/
/*                           ~netCDFDataset()                           */
/************************************************************************/

netCDFDataset::~netCDFDataset()

{

    FlushCache();

    if( papszMetadata != NULL )
	CSLDestroy( papszMetadata );

    if( pszProjection != NULL ) 
	CPLFree( pszProjection );

    if( papszName != NULL ) 
	CSLDestroy( papszName );

    if( pszFilename != NULL )
	CPLFree( pszFilename );

    if( papszSubDatasets )
	CSLDestroy( papszSubDatasets );

    if( papszGeolocation )
	CSLDestroy( papszGeolocation );

    if( cdfid ) 
	nc_close( cdfid );


}


/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/
void netCDFDataset::SetProjection( int var )
{
/* -------------------------------------------------------------------- */
/*      Set Projection                                                  */
/* -------------------------------------------------------------------- */

    size_t       start[2], edge[2];
    int          status;
    unsigned int i;
    const char   *pszValue;
    int          nVarProjectionID;
    char         szVarName[ MAX_NC_NAME ];
    char         szTemp[ MAX_NC_NAME ];
    char         szGridMappingName[ MAX_NC_NAME ];
    char         szGridMappingValue[ MAX_NC_NAME ];

    double       dfStdP1;
    double       dfStdP2;
    double       dfCenterLat;
    double       dfCenterLon;
    double       dfScale;
    double       dfFalseEasting;
    double       dfFalseNorthing;

    OGRSpatialReference oSRS;
    int          nVarDimXID = -1;
    int          nVarDimYID = -1;
    double       *pdfXCoord;
    double       *pdfYCoord;
    char         szDimNameX[ MAX_NC_NAME ];
    int          nSpacingBegin;
    int          nSpacingMiddle;
    int          nSpacingLast;

    const char *pszWKT;
    const char *pszGeoTransform;
    char **papszGeoTransform;


    netCDFDataset * poDS;
    poDS = this;

/* -------------------------------------------------------------------- */
/*      Get x/y range information.                                      */
/* -------------------------------------------------------------------- */

    poDS->adfGeoTransform[0] = 0.0;
    poDS->adfGeoTransform[1] = 1.0;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = 0.0;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = 1.0;
    poDS->pszProjection = NULL;
    

/* -------------------------------------------------------------------- */
/*      Look for grid_mapping metadata                                  */
/* -------------------------------------------------------------------- */

    strcpy( szGridMappingValue, "" );
    strcpy( szGridMappingName, "" );

    nc_inq_varname(  cdfid, var, szVarName );
    strcpy(szTemp,szVarName);
    strcat(szTemp,"#grid_mapping");
    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
    if( pszValue ) {
	strcpy(szGridMappingName,szTemp);
	strcpy(szGridMappingValue,pszValue);
    }

/* -------------------------------------------------------------------- */
/*      Look for dimension: lon                                         */
/* -------------------------------------------------------------------- */

    for( i = 0; (i < strlen( poDS->papszDimName[ poDS->nDimXid ] )  && 
		 i < 3 ); i++ ) {
	szDimNameX[i] = tolower( ( poDS->papszDimName[poDS->nDimXid] )[i] );
    }
    szDimNameX[3] = '\0';

/* -------------------------------------------------------------------- */
/*      Read grid_mappinginformation and set projections               */
/* -------------------------------------------------------------------- */

    if( !( EQUAL(szGridMappingName,"" ) ) ) {
	nc_inq_varid( cdfid, szGridMappingValue, &nVarProjectionID );
	poDS->ReadAttributes( cdfid, nVarProjectionID );
    
	strcpy( szTemp, szGridMappingValue );
	strcat( szTemp, "#" );
	strcat( szTemp, GRD_MAPPING_NAME );
	pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
	papszGeolocation = CSLSetNameValue( papszGeolocation,
					    "grid_mapping", 
					    pszValue);
	if( pszValue != NULL ) {
/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */

	    if( EQUAL( pszValue, TM ) ) {
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, SCALE_FACTOR );
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    SCALE_FACTOR,
						    pszValue);
		if( pszValue )
		    dfScale = atof( pszValue );
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, LONG_CENTRAL_MERIDIAN );
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    LONG_CENTRAL_MERIDIAN,
						    pszValue);
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		if( pszValue )
		    dfCenterLon = atof( pszValue );
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, LAT_PROJ_ORIGIN );
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    LAT_PROJ_ORIGIN,
						    pszValue);
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		if( pszValue )
		    dfCenterLat = atof( pszValue );
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, FALSE_EASTING );
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    FALSE_EASTING,
						    pszValue);
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		if( pszValue )
		    dfFalseEasting = atof( pszValue );
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, FALSE_NORTHING );
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    FALSE_NORTHING,
						    pszValue);
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		if( pszValue )
		    dfFalseNorthing = atof (pszValue);
		
		oSRS.SetTM( dfCenterLat, 
			    dfCenterLon,
			    dfScale,
			    dfFalseEasting,
			    dfFalseNorthing );
		oSRS.SetWellKnownGeogCS( "WGS84" );
	    }
/* -------------------------------------------------------------------- */
/*      Lambert conformal conic                                         */
/* -------------------------------------------------------------------- */
	    else if( EQUAL( pszValue, L_C_CONIC ) ) {
		
		strcpy( szTemp,szGridMappingValue );
		strcat( szTemp, "#" );
		strcat( szTemp, STD_PARALLEL_1 );
		pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    STD_PARALLEL_1,
						    pszValue);
		
		
		if( pszValue ) {
		    dfStdP1 = atof( pszValue );
		}
		
		strcpy( szTemp,szGridMappingValue );
		strcat( szTemp, "#" );
		strcat( szTemp, STD_PARALLEL_1 );
		pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    STD_PARALLEL_2,
						    pszValue);
		
		if( pszValue ) {
		    dfStdP2 = atof( pszValue );
		}
		
		strcpy( szTemp,szGridMappingValue );
		strcat( szTemp, "#" );
		strcat( szTemp, LONG_CENTRAL_MERIDIAN );
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    LONG_CENTRAL_MERIDIAN,
						    pszValue);
		
		if( pszValue )
		    dfCenterLon = atof(pszValue);
		
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, LAT_PROJ_ORIGIN );
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    LAT_PROJ_ORIGIN,
						    pszValue);
		
		if( pszValue )
		    dfCenterLat = atof(pszValue);
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, FALSE_EASTING );
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    FALSE_EASTING,
						    pszValue);
		
		if( pszValue )
		    dfFalseEasting = atof(pszValue);
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, FALSE_NORTHING );
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		papszGeolocation = CSLSetNameValue( papszGeolocation,
						    FALSE_NORTHING,
						    pszValue);
		
		if( pszValue )
		dfFalseNorthing = atof(pszValue);
		
		oSRS.SetLCC( dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
			     dfFalseEasting, dfFalseNorthing );
		oSRS.SetWellKnownGeogCS( "WGS84" );

	    }
/* -------------------------------------------------------------------- */
/*      Is this Latitude/Longitude Grid                                 */
/* -------------------------------------------------------------------- */
	    
	} else if( EQUAL( szDimNameX,"lon" ) ) {
	    papszGeolocation = CSLSetNameValue( papszGeolocation,
						"grid_mapping", 
						"Latitude Longitude" );
	    
	    strcpy( szTemp, poDS->papszDimName[nDimXid] );
	    strcat( szTemp, "#" );
	    strcat( szTemp, UNITS );
	    
	    pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
	    
	    strcpy( szTemp, "Lon_");
	    strcat( szTemp, UNITS );
	    
	    papszGeolocation = CSLSetNameValue( papszGeolocation,
						szTemp,
						pszValue);
	    
	    strcpy(szTemp, poDS->papszDimName[nDimXid]);
	    strcat(szTemp, "#" );
	    strcat(szTemp, AXIS );
	    
	    pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
	    
	    strcpy( szTemp, "Lon_");
	    strcat( szTemp, AXIS );
	    
	    papszGeolocation = CSLSetNameValue( papszGeolocation,
						szTemp,
						pszValue);
	    
	    strcpy(szTemp, poDS->papszDimName[nDimXid]);
	    strcat(szTemp, "#" );
	    strcat(szTemp, BOUNDS );
	    
	    pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
	    
	    strcpy( szTemp, "Lon_");
	    strcat( szTemp, BOUNDS );
	    
	    papszGeolocation = CSLSetNameValue( papszGeolocation,
						szTemp,
						pszValue);
	    strcpy(szTemp, poDS->papszDimName[nDimXid]);
	    strcat(szTemp, "#" );
	    strcat(szTemp, ORIG_AXIS );
	    
	    pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
	    
	    strcpy( szTemp, "Lon_");
	    strcat( szTemp, ORIG_AXIS );
	    
	    papszGeolocation = CSLSetNameValue( papszGeolocation,
						szTemp,
						pszValue);
	    
	    
	    strcpy( szTemp, poDS->papszDimName[nDimYid] );
	    strcat( szTemp, "#" );
	    strcat( szTemp, UNITS );
	    
	    pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
	    
	    strcpy( szTemp, "Lat_");
	    strcat( szTemp, UNITS );
	    
	    papszGeolocation = CSLSetNameValue( papszGeolocation,
					    szTemp,
						pszValue);
	    
	    strcpy(szTemp, poDS->papszDimName[nDimYid]);
	    strcat(szTemp, "#" );
	    strcat(szTemp, AXIS );
	    
	    pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
	    
	    strcpy( szTemp, "Lat_");
	    strcat( szTemp, AXIS );
	    
	    papszGeolocation = CSLSetNameValue( papszGeolocation,
						szTemp,
						pszValue);
	    
	    strcpy(szTemp, poDS->papszDimName[nDimYid]);
	    strcat(szTemp, "#" );
	    strcat(szTemp, BOUNDS );
	    
	    pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
	    
	    strcpy( szTemp, "Lat_");
	    strcat( szTemp, BOUNDS );
	    
	    papszGeolocation = CSLSetNameValue( papszGeolocation,
						szTemp,
						pszValue)
;
	    strcpy(szTemp, poDS->papszDimName[nDimYid]);
	    strcat(szTemp, "#" );
	    strcat(szTemp, ORIG_AXIS );
	    
	    pszValue = CSLFetchNameValue( poDS->papszMetadata, szTemp );
	    
	    strcpy( szTemp, "Lat_");
	    strcat( szTemp, ORIG_AXIS );
	    
	    papszGeolocation = CSLSetNameValue( papszGeolocation,
						szTemp,
						pszValue);

	    
	    
	    oSRS.SetWellKnownGeogCS( "WGS84" );
	} else {
	    // This would be too indiscrimant.  But we should set
	    // it if we know the data is geographic.
	    //oSRS.SetWellKnownGeogCS( "WGS84" );
	}
    }
/* -------------------------------------------------------------------- */
/*      Read projection coordinates                                     */
/* -------------------------------------------------------------------- */

    nc_inq_varid( cdfid, poDS->papszDimName[nDimXid], &nVarDimXID );
    nc_inq_varid( cdfid, poDS->papszDimName[nDimYid], &nVarDimYID );
    
    if( ( nVarDimXID != -1 ) && ( nVarDimYID != -1 ) ) {
	pdfXCoord = (double *) CPLCalloc( xdim, sizeof(double) );
	pdfYCoord = (double *) CPLCalloc( ydim, sizeof(double) );
    
/* -------------------------------------------------------------------- */
/*      Is pixel spacing is uniform accross the map?                    */
/* -------------------------------------------------------------------- */
	start[0] = 0;
	edge[0]  = xdim;
	
	status = nc_get_vara_double( cdfid, nVarDimXID, 
				     start, edge, pdfXCoord);
	edge[0]  = ydim;
	status = nc_get_vara_double( cdfid, nVarDimYID, 
                                     start, edge, pdfYCoord);

/* -------------------------------------------------------------------- */
/*      Check Longitude                                                 */
/* -------------------------------------------------------------------- */

	nSpacingBegin   = (int) poDS->rint((pdfXCoord[1]-pdfXCoord[0]) * 1000);
	
	nSpacingMiddle  = (int) poDS->rint((pdfXCoord[xdim / 2] - 
				      pdfXCoord[(xdim / 2) + 1]) * 1000);
	
	nSpacingLast    = (int) poDS->rint((pdfXCoord[xdim - 2] - 
				      pdfXCoord[xdim-1]) * 1000);
	
	if( ( abs( nSpacingBegin ) == abs( nSpacingLast )) &&
	    ( abs( nSpacingBegin ) == abs( nSpacingMiddle )) &&
	    ( abs( nSpacingMiddle ) == abs( nSpacingLast )) ) {

/* -------------------------------------------------------------------- */
/*      Longitude is equaly spaced, check lattitde                      */
/* -------------------------------------------------------------------- */
	    nSpacingBegin   = (int) poDS->rint((pdfYCoord[1]-pdfYCoord[0]) * 
					       1000); 
	    
	    nSpacingMiddle  = (int) poDS->rint((pdfYCoord[ydim / 2] - 
						pdfYCoord[(ydim / 2) + 1]) * 
					       1000);
	    
	    nSpacingLast    = (int) poDS->rint((pdfYCoord[ydim - 2] - 
						pdfYCoord[ydim-1]) * 
					       1000);
	    

	    if( ( abs( nSpacingBegin ) == abs( nSpacingLast )) &&
		( abs( nSpacingBegin ) == abs( nSpacingMiddle )) &&
		( abs( nSpacingMiddle ) == abs( nSpacingLast )) ) {
/* -------------------------------------------------------------------- */
/*      We have gridded data s we can set the Gereferencing info.       */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Enable GeoTransform                                             */
/* -------------------------------------------------------------------- */
		    poDS->bGotGeoTransform = TRUE;
		    
		    poDS->adfGeoTransform[0] = pdfXCoord[0];
		    poDS->adfGeoTransform[3] = pdfYCoord[0];
		    poDS->adfGeoTransform[2] = 0;
		    poDS->adfGeoTransform[4] = 0;
		    poDS->adfGeoTransform[1] = (( pdfXCoord[xdim-1] - 
						  pdfXCoord[0] ) / 
						poDS->nRasterXSize);

		    poDS->adfGeoTransform[5] = (( pdfYCoord[ydim-1] - 
						  pdfYCoord[0] ) / 
						poDS->nRasterYSize);

		    oSRS.exportToWkt( &(poDS->pszProjection) );
		    
	    } 
	}
	CPLFree( pdfXCoord );
	CPLFree( pdfYCoord );

    }


/* -------------------------------------------------------------------- */
/*      Is this a netCDF file created by GDAL?                          */
/* -------------------------------------------------------------------- */
    if( !EQUAL( szGridMappingValue, "" )  ) {
	strcpy( szTemp,szGridMappingValue);
	strcat( szTemp, "#" );
	strcat( szTemp, "spatial_ref");
	pszWKT = CSLFetchNameValue(poDS->papszMetadata, szTemp);
	
	if( pszWKT != NULL ) {
	    
	    pszProjection = strdup( pszWKT );
	    
	    strcpy(szTemp,szGridMappingValue);
	    strcat( szTemp, "#" );
	    strcat( szTemp, "GeoTransform");
	    
	    pszGeoTransform = CSLFetchNameValue(poDS->papszMetadata, szTemp);
	    
/* -------------------------------------------------------------------- */
/*      Look for GeoTransform Array                                     */
/* -------------------------------------------------------------------- */

	    if( pszGeoTransform != NULL ) {
		papszGeoTransform = CSLTokenizeString2( pszGeoTransform,
							" ", 
							CSLT_HONOURSTRINGS );
		poDS->bGotGeoTransform   = TRUE;
		
		poDS->adfGeoTransform[0] = atof( papszGeoTransform[0] );
		poDS->adfGeoTransform[1] = atof( papszGeoTransform[1] );
		poDS->adfGeoTransform[2] = atof( papszGeoTransform[2] );
		poDS->adfGeoTransform[3] = atof( papszGeoTransform[3] );
		poDS->adfGeoTransform[4] = atof( papszGeoTransform[4] );
		poDS->adfGeoTransform[5] = atof( papszGeoTransform[5] );
/* -------------------------------------------------------------------- */
/*      Look for corner array values                                    */
/* -------------------------------------------------------------------- */
	    } else {
		double dfNN, dfSN, dfEE, dfWE;
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, "Northernmost_Northing");
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		
		if( pszValue != NULL ) {
		    dfNN = atof( pszValue );
		}
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, "Southernmost_Northing");
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		
		if( pszValue != NULL ) {
		    dfSN = atof( pszValue );
		}
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, "Easternmost_Easting");
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		
		if( pszValue != NULL ) {
		    dfEE = atof( pszValue );
		}
		
		strcpy(szTemp,szGridMappingValue);
		strcat( szTemp, "#" );
		strcat( szTemp, "Westernmost_Easting");
		pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		
		if( pszValue != NULL ) {
		    dfWE = atof( pszValue );
		}
		
		adfGeoTransform[0] = dfWE;
		adfGeoTransform[1] = (dfEE - dfWE) / poDS->GetRasterXSize();
		adfGeoTransform[2] = 0.0;
		adfGeoTransform[3] = dfNN;
		adfGeoTransform[4] = 0.0;
		adfGeoTransform[5] = (dfSN - dfNN) / poDS->GetRasterYSize();
		
		bGotGeoTransform = TRUE;
	    }
	    CSLDestroy( papszGeoTransform );
	    
	}
    }

}
/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr netCDFDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    if( bGotGeoTransform )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                                rint()                                */
/************************************************************************/

double netCDFDataset::rint( double dfX)
{
    if( dfX > 0 ) {
        int nX = (int) (dfX+0.5);
        if( nX % 2 ) {
            double dfDiff = dfX - (double)nX;
            if( dfDiff == -0.5 )
                return double( nX-1 );
        }
        return double( nX );
    } else {
        int nX= (int) (dfX-0.5);
        if( nX % 2 ) {
            double dfDiff = dfX - (double)nX;
            if( dfDiff == 0.5 )
                return double(nX+1);
        }
        return double(nX);
    }
}

/************************************************************************/
/*                        ReadAttributes()                              */
/************************************************************************/

CPLErr netCDFDataset::ReadAttributes( int cdfid, int var)

{
    char    szAttrName[ NC_MAX_NAME ];
    char    szVarName [ NC_MAX_NAME ];
    char    szMetaName[ NC_MAX_NAME ];
    char    szMetaTemp[ MAX_STR_LEN ];
    nc_type nAttrType;
    size_t  nAttrLen,m;
    int     nbAttr;
    char    szTemp[ NC_MAX_NAME ];

    nc_inq_varnatts( cdfid, var, &nbAttr );
    if( var == NC_GLOBAL ) {
	strcpy( szVarName,"NC_GLOBAL" );
    }
    else {
	nc_inq_varname(  cdfid, var, szVarName );
    }

    for( int l=0; l < nbAttr; l++){
	
	nc_inq_attname( cdfid, var, l, szAttrName);
	sprintf( szMetaName, "%s#%s", szVarName, szAttrName  );
	*szMetaTemp='\0';
	nc_inq_att( cdfid, var, szAttrName, &nAttrType, &nAttrLen );
	
	
	switch (nAttrType) {
	case NC_CHAR:
	    char *pszTemp;
	    pszTemp = (char *) CPLCalloc( nAttrLen+1, sizeof( char ) );
	    nc_get_att_text( cdfid, var, szAttrName,pszTemp );
	    pszTemp[nAttrLen]='\0';
	    strcpy(szMetaTemp,pszTemp);
	    CPLFree(pszTemp);
	    break;
	case NC_SHORT:
	    short *psTemp;
	    
	    psTemp = (short *) CPLCalloc( nAttrLen, sizeof( short ) );
	    nc_get_att_short( cdfid, var, szAttrName, psTemp );
	    for(m=0; m < nAttrLen-1; m++) {
		sprintf( szTemp, "%d, ",psTemp[m] );
		strcat(szMetaTemp,szTemp);
	    }
	    sprintf( szTemp, "%d",psTemp[m] );
	    CPLFree(psTemp);
	    strcat(szMetaTemp,szTemp);
	    
	    break;
	case NC_INT:
	    int *pnTemp;
	    
	    pnTemp = (int *) CPLCalloc( nAttrLen, sizeof( int ) );
	    nc_get_att_int( cdfid, var, szAttrName, pnTemp );
	    for(m=0; m < nAttrLen-1; m++) {
		sprintf( szTemp, "%d",pnTemp[m] );
		strcat(szMetaTemp,szTemp);
	    }
	    sprintf( szTemp, "%d",pnTemp[m] );
	    CPLFree(pnTemp);
	    strcat(szMetaTemp,szTemp);
	    break;
	case NC_FLOAT:
	    float *pfTemp;
	    pfTemp = (float *) CPLCalloc( nAttrLen, sizeof( float ) );
	    nc_get_att_float( cdfid, var, szAttrName, pfTemp );
	    for(m=0; m < nAttrLen-1; m++) {
		sprintf( szTemp, "%e",pfTemp[m] );
		strcat(szMetaTemp,szTemp);
	    }
	    sprintf( szTemp, "%e",pfTemp[m] );
	    CPLFree(pfTemp);
	    strcat(szMetaTemp,szTemp);
	    
	    break;
	case NC_DOUBLE:
	    double *pdfTemp;
	    pdfTemp = (double *) CPLCalloc(nAttrLen, sizeof(double));
	    nc_get_att_double( cdfid, var, szAttrName, pdfTemp );
	    for(m=0; m < nAttrLen-1; m++) {
		sprintf( szTemp, "%g",pdfTemp[m] );
		strcat(szMetaTemp,szTemp);
	    }
	    sprintf( szTemp, "%g",pdfTemp[m] );
	    CPLFree(pdfTemp);
	    strcat(szMetaTemp,szTemp);
	    
	    break;
	default:
	    break;
	}

	papszMetadata = CSLSetNameValue(papszMetadata, 
					szMetaName, 
					szMetaTemp);
	
    }
	

    return CE_None;

}

/************************************************************************/
/*                netCDFDataset::CreateSubDatasetList()                 */
/************************************************************************/
void netCDFDataset::CreateSubDatasetList( )
{

    char         szDim[ MAX_NC_NAME ];
    char         szTemp[ MAX_NC_NAME ];
    char         szType[ MAX_NC_NAME ];
    char         szName[ MAX_NC_NAME ];
    char         szVarStdName[ MAX_NC_NAME ];
    int          nDims;
    int          nVar;
    int          nVarCount;
    int          i;
    nc_type      nVarType;
    int          *ponDimIds;
    size_t       nDimLen;
    int          nSub;
    nc_type      nAttype;
    size_t       nAttlen;

    netCDFDataset 	*poDS;
    poDS = this;

    nSub=1;
    nc_inq_nvars ( cdfid, &nVarCount );
    for ( nVar = 0; nVar < nVarCount; nVar++ ) {

	nc_inq_varndims ( cdfid, nVar, &nDims );
	if( nDims >= 2 ) {
	    ponDimIds = (int *) CPLCalloc( nDims, sizeof( int ) );
	    nc_inq_vardimid ( cdfid, nVar, ponDimIds );
	    
/* -------------------------------------------------------------------- */
/*      Create Sub dataset list                                         */
/* -------------------------------------------------------------------- */
	    szDim[0]='\0';
	    for( i = 0; i < nDims; i++ ) {
		nc_inq_dimlen ( cdfid, ponDimIds[i], &nDimLen );
		sprintf(szTemp, "%d", nDimLen);
		strcat(szTemp,  "x" );
		strcat(szDim,   szTemp);
	    }

	    nc_inq_vartype( cdfid, nVar, &nVarType );
/* -------------------------------------------------------------------- */
/*      Get rid of the last "x" character                               */
/* -------------------------------------------------------------------- */
	    szDim[strlen(szDim) - 1] = '\0';
	    
	    switch( nVarType ) {
	    case NC_BYTE:
		strcpy(szType, "8-bit character");
		break;
	    case NC_SHORT: 
		strcpy(szType, "8-bit integer");
		break;
	    case NC_INT:
		strcpy(szType, "16-bit integer");
		break;
	    case NC_FLOAT:
		strcpy(szType, "32-bit floating-point");
		break;
	    case NC_DOUBLE:
		strcpy(szType, "64-bit floating-point");
		break;
	    default:
		break;
	    }
	    nc_inq_varname  ( cdfid, nVar, szName);
	    nc_inq_att( cdfid, nVar, "standard_name", &nAttype, &nAttlen);
	    if( nc_get_att_text ( cdfid, nVar, "standard_name", 
				  szVarStdName ) == NC_NOERR ) {
		szVarStdName[nAttlen] = '\0';
	    }
	    else {
		strcpy( szVarStdName, szName );
	    }
    
	    sprintf( szTemp, "SUBDATASET_%d_NAME", nSub) ;
	    
	    poDS->papszSubDatasets =
		CSLSetNameValue( poDS->papszSubDatasets, szTemp,
				 CPLSPrintf( "NETCDF:\"%s\":%s",
					     poDS->pszFilename,
					     szName) ) ;
	    
	    sprintf(  szTemp, "SUBDATASET_%d_DESC", nSub++ );

	    poDS->papszSubDatasets =
		CSLSetNameValue( poDS->papszSubDatasets, szTemp,
				 CPLSPrintf( "[%s] %s (%s)", 
					     szDim,
					     szVarStdName,
					     szType ) );
	    CPLFree(ponDimIds);
	}
    }

}
    
/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *netCDFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int          j;
    unsigned int k;
    int          nd;
    int          cdfid, dim_count, var, var_count;
    int          i = 0;
    size_t       lev_count;
    size_t       nTotLevCount = 1;
    int          nDim = 2;
    int          status;
    int          nDimID;
    char         attname[NC_MAX_NAME];
    int          ndims, nvars, ngatts, unlimdimid;


/* -------------------------------------------------------------------- */
/*       Check if filename start with NETCDF: tag                       */
/* -------------------------------------------------------------------- */

    netCDFDataset 	*poDS;
    poDS = new netCDFDataset();

    poDS->papszName = CSLTokenizeString2(  poOpenInfo->pszFilename,
					   ":", CSLT_HONOURSTRINGS );

    if( EQUAL( poDS->papszName[0], "NETCDF" ) ) {
	if( ( CSLCount(poDS->papszName) == 3  ) ){
	    poDS->pszFilename = strdup( poDS->papszName[1] );
	}
    }
    else {
	poDS->pszFilename = strdup( poOpenInfo->pszFilename );
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    if( nc_open( poDS->pszFilename, NC_NOWRITE, &cdfid ) != NC_NOERR ) {
	delete poDS;
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Is this a real netCDF file?                                     */
/* -------------------------------------------------------------------- */
    status = nc_inq(cdfid, &ndims, &nvars, &ngatts, &unlimdimid);
    if( status != NC_NOERR ) {
	CPLFree( poDS->pszFilename );
	CSLDestroy( poDS->papszName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Does the request variable exist?                                */
/* -------------------------------------------------------------------- */
    if( ( CSLCount(poDS->papszName) == 3  ) ){
	status = nc_inq_varid( cdfid, poDS->papszName[2], &var);
	if( status != NC_NOERR ) {
	    CPLError( CE_Warning, CPLE_AppDefined, 
		      "%s is a netCDF file, but %s is not a variable.",
		      poOpenInfo->pszFilename, 
		      poDS->papszName[2] );
	    
	    nc_close( cdfid );
	    return NULL;
	}
    }

    if( nc_inq_ndims( cdfid, &dim_count ) != NC_NOERR || dim_count < 2 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "%s is a netCDF file, but not in GMT configuration.",
                  poOpenInfo->pszFilename );

        nc_close( cdfid );
        return NULL;
    }

    CPLDebug( "GDAL_netCDF", "dim_count = %d\n", dim_count );

    if( (status = nc_get_att_text( cdfid, NC_GLOBAL, "Conventions",
				   attname )) != NC_NOERR ) {
	CPLError( CE_Warning, CPLE_AppDefined, 
		  "No UNIDATA NC_GLOBAL:Conventions attribute");
        /* note that 'Conventions' is always capital 'C' in CF spec*/
    }


/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    if ( nc_inq_nvars ( cdfid, &var_count) != NC_NOERR )
	return NULL;    
    
    CPLDebug( "GDAL_netCDF", "var_count = %d\n", var_count );
    

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/*      Create Netcdf Subdataset if filename as NETCDF tag              */
/* -------------------------------------------------------------------- */
    poDS->cdfid = cdfid;

    poDS->ReadAttributes( cdfid, NC_GLOBAL );	

    if( ( !EQUAL( poDS->papszName[0], "NETCDF" ) ) ) {
	poDS->CreateSubDatasetList( );
	poDS->SetMetadata( poDS->papszMetadata );
	return( poDS );
    }


/* -------------------------------------------------------------------- */
/*      Open the NETCDF subdataset NETCDF:"filename":subdataset         */
/* -------------------------------------------------------------------- */

    var=-1;
    nc_inq_varid( cdfid, poDS->papszName[2], &var);
    nc_inq_varndims ( cdfid, var, &nd );

    poDS->paDimIds = (int *)CPLCalloc(nd, sizeof( int ) );
    poDS->panBandDimPos = ( int * ) CPLCalloc( nd, sizeof( int ) );

    nc_inq_vardimid( cdfid, var, poDS->paDimIds );

	
/* -------------------------------------------------------------------- */
/*      Check fi somebody tried to pass a variable with less than 2D    */
/* -------------------------------------------------------------------- */

    if ( nd < 2 ) {
	CPLFree( poDS->paDimIds );
	CPLFree( poDS->panBandDimPos );
	return NULL;
    }

/* -------------------------------------------------------------------- */
/*      CF-1 Convention                                                 */
/*      dimensions to appear in the relative order T, then Z, then Y,   */
/*      then X  to the file. All other dimensions should, whenever      */
/*      possible, be placed to the left of the spatiotemporal           */
/*      dimensions.                                                     */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Get X dimensions information                                    */
/* -------------------------------------------------------------------- */
    poDS->nDimXid = poDS->paDimIds[nd-1];
    nc_inq_dimlen ( cdfid, poDS->nDimXid, &poDS->xdim );
    poDS->nRasterXSize = poDS->xdim;

/* -------------------------------------------------------------------- */
/*      Get Y dimension information                                     */
/* -------------------------------------------------------------------- */
    poDS->nDimYid = poDS->paDimIds[nd-2];
    nc_inq_dimlen ( cdfid, poDS->nDimYid, &poDS->ydim );
    poDS->nRasterYSize = poDS->ydim;


    for( j=0,k=0; j < nd; j++ ){
	if( poDS->paDimIds[j] == poDS->nDimXid ){ 
	    poDS->panBandDimPos[0] = j;         // Save Position of XDim
	    k++;
	}
	if( poDS->paDimIds[j] == poDS->nDimYid ){
	    poDS->panBandDimPos[1] = j;         // Save Position of YDim
	    k++;
	}
    }
/* -------------------------------------------------------------------- */
/*      X and Y Dimension Ids were not found!                           */
/* -------------------------------------------------------------------- */
    if( k != 2 ) {
	CPLFree( poDS->paDimIds );
	CPLFree( poDS->panBandDimPos );
	return NULL;
    }
	    
/* -------------------------------------------------------------------- */
/*      Read Metadata for this variable                                 */
/* -------------------------------------------------------------------- */
    poDS->ReadAttributes( cdfid, var );
	
/* -------------------------------------------------------------------- */
/*      Read Metadata for each dimension                                */
/* -------------------------------------------------------------------- */
    
    for( j=0; j < dim_count; j++ ){
	nc_inq_dimname( cdfid, j, poDS->papszDimName[j] );
	status = nc_inq_varid( cdfid, poDS->papszDimName[j], &nDimID );
	if( status == NC_NOERR ) {
	    poDS->ReadAttributes( cdfid, nDimID );
	}
    }

    poDS->SetProjection( var );
/* -------------------------------------------------------------------- */
/*      Create bands                                                    */
/* -------------------------------------------------------------------- */
    poDS->panBandZLev = (int *)CPLCalloc( sizeof( nd ) - 2, 
                                              sizeof( int ) );
    
    nTotLevCount = 1;
    if ( dim_count > 2 ) {
	nDim=2;
	for( j=0; j < nd; j++ ){
	    if( ( poDS->paDimIds[j] != poDS->nDimXid ) && 
		( poDS->paDimIds[j] != poDS->nDimYid ) ){
		nc_inq_dimlen ( cdfid, poDS->paDimIds[j], &lev_count );
		nTotLevCount *= lev_count;
		poDS->panBandZLev[ nDim-2 ] = lev_count;
		poDS->panBandDimPos[ nDim++ ] = j;  //Save Position of ZDim
	    }
	}
    }
    i=0;

    for ( unsigned int lev = 0; lev < nTotLevCount ; lev++ ) {
	char szBandItemName[ MAX_STR_LEN ];
	char ** papszToken;
	const char * pszGeo;
	papszToken=NULL;

	netCDFRasterBand *poBand=new netCDFRasterBand(poDS, 
						      var, 
						      nDim,
						      lev,
						      poDS->panBandZLev,
						      poDS->panBandDimPos,
						      i+1 );

	for( j = 0; j < CSLCount( poDS->papszGeolocation ); j++ ) {
	    pszGeo = CSLGetField( poDS->papszGeolocation, j );
	    papszToken = CSLTokenizeString2(  pszGeo,
					      "=", CSLT_HONOURSTRINGS );

	    strcpy( szBandItemName, "NETCDF_GEOLOCATION_" );
	    strcat( szBandItemName, papszToken[0] );
	    poBand->SetMetadataItem( szBandItemName, papszToken[1] );
	    CSLDestroy( papszToken );
	}

	poDS->SetBand( i+1, poBand );
	i++;
   } 

    CPLFree( poDS->paDimIds );
    CPLFree( poDS->panBandDimPos );
    CPLFree( poDS->panBandZLev );
    
    poDS->nBands = i;

    // Handle angular geographic coordinates here

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML( );
    poDS->SetMetadata( poDS->papszMetadata );
    return( poDS );
}


/************************************************************************/
/*                            CopyMetadata()                            */
/*                                                                      */
/*      Create a copy of metadata for NC_GLOBAL or a variable           */
/************************************************************************/

void CopyMetadata( void  *poDS, int fpImage, int CDFVarID ) {

/* -------------------------------------------------------------------- */
/*      Add CF-1.0 Conventions Global attribute                         */
/* -------------------------------------------------------------------- */
    char       **papszMetadata;
    char       **papszFieldData;
    const char *pszField;
    char       szMetaName[ MAX_STR_LEN ];
    char       szMetaValue[ MAX_STR_LEN ];
    int        nDataLength;
    int        nItems;

/* -------------------------------------------------------------------- */
/*      Global metadata are set with NC_GLOBAL as the varid             */
/* -------------------------------------------------------------------- */
    
    if( CDFVarID == NC_GLOBAL ) {

	papszMetadata = GDALGetMetadata( (GDALDataset *) poDS,"");

	nc_put_att_text( fpImage, 
			 NC_GLOBAL, 
			 "Conventions", 
			 6,
			 "CF-1.0" );
    } else {

	papszMetadata = GDALGetMetadata( (GDALRasterBandH) poDS, NULL );

    }

    nItems = CSLCount( papszMetadata );             
    
    for(int k=0; k < nItems; k++ ) {
	pszField = CSLGetField( papszMetadata, k );
	papszFieldData = CSLTokenizeString2 (pszField, "=", 
					     CSLT_HONOURSTRINGS );
	if( papszFieldData[1] != NULL ) {
	    strcpy( szMetaName,  papszFieldData[ 0 ] );
	    strcpy( szMetaValue, papszFieldData[ 1 ] );

/* -------------------------------------------------------------------- */
/*      netCDF attributes do not like the '#' character.                */
/* -------------------------------------------------------------------- */

	    for( unsigned int h=0; h < strlen( szMetaName ) -1 ; h++ ) {
		if( szMetaName[h] == '#' ) szMetaName[h] = '-'; 
	    }
	    
	    nDataLength = strlen( szMetaValue );
	    nc_put_att_text( fpImage, 
			     CDFVarID, 
			     szMetaName,
			     nDataLength,
			     szMetaValue );

	    
	}
	CSLDestroy( papszFieldData );

    }
}
/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/


static GDALDataset*
NCDFCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                int bStrict, char ** papszOptions, 
                GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    int  bProgressive = FALSE;

    int  anBandDims[ NC_MAX_DIMS ];
    int  anBandMap[  NC_MAX_DIMS ];

    int    bWriteGeoTransform = FALSE;
    char  pszNetcdfProjection[ NC_MAX_NAME ];


    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;


    bProgressive = CSLFetchBoolean( papszOptions, "PROGRESSIVE", FALSE );

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    int fpImage;
    int status;
    int nXDimID = 0;
    int nYDimID = 0;

    status = nc_create( pszFilename, NC_CLOBBER,  &fpImage );

    if( status != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create netCDF file %s.\n", 
                  pszFilename );
        return NULL;
    }

    GDALDataType eDT;

    status = nc_def_dim( fpImage, "x", nXSize, &nXDimID );
    CPLDebug( "GDAL_netCDF", "status nc_def_dim X = %d\n", status );

    status = nc_def_dim( fpImage, "y", nYSize, &nYDimID );
    CPLDebug( "GDAL_netCDF", "status nc_def_dim Y = %d\n", status );

    CPLDebug( "GDAL_netCDF", "nYDimID = %d\n", nXDimID );
    CPLDebug( "GDAL_netCDF", "nXDimID = %d\n", nYDimID );
    CPLDebug( "GDAL_netCDF", "nXSize = %d\n", nXSize );
    CPLDebug( "GDAL_netCDF", "nYSize = %d\n", nYSize );


    CopyMetadata((void *) poSrcDS, fpImage, NC_GLOBAL );

/* -------------------------------------------------------------------- */
/*      Set Projection for netCDF data CF-1 Convention                  */
/* -------------------------------------------------------------------- */

    OGRSpatialReference oSRS;
    char *pszWKT = (char *) poSrcDS->GetProjectionRef();


    if( pszWKT != NULL )
	oSRS.importFromWkt( &pszWKT );

    if( oSRS.IsGeographic() ) {

	int status;
	int i;
	int NCDFVarID;

	double dfNN=0.0;
	double dfSN=0.0;
	double dfEE=0.0;
	double dfWE=0.0;
	double adfGeoTransform[6];
	char   szGeoTransform[ MAX_STR_LEN ];
	char   szTemp[ MAX_STR_LEN ];


/* -------------------------------------------------------------------- */
/*      Copy GeoTransform array from source                             */
/* -------------------------------------------------------------------- */
	poSrcDS->GetGeoTransform( adfGeoTransform );
        bWriteGeoTransform = TRUE;

	*szGeoTransform = '\0';
	for( i=0; i<6; i++ ) {
	    sprintf( szTemp, "%g ",
		     adfGeoTransform[i] );
	    strcat( szGeoTransform, szTemp );
	}
	CPLDebug( "GDAL_netCDF", "szGeoTranform = %s", szGeoTransform );
	strcpy( pszNetcdfProjection, "GDAL_Geographics" );

	status = nc_def_var( fpImage, 
			     pszNetcdfProjection, 
			     NC_CHAR, 
			     0, NULL, &NCDFVarID );
	
	dfNN = adfGeoTransform[3];
	dfSN = ( adfGeoTransform[5] * nYSize ) + dfNN;
	dfWE = adfGeoTransform[0];
	dfEE = ( adfGeoTransform[1] * nXSize ) + dfWE;

        status = nc_put_att_double( fpImage,
				    NCDFVarID, 
				    "Northernmost_Northing",
				    NC_DOUBLE,
				    1,
				    &dfNN );
        status = nc_put_att_double( fpImage,
				    NCDFVarID, 
				    "Southernmost_Northing",
				    NC_DOUBLE,
				    1,
				    &dfSN );
	status = nc_put_att_double( fpImage,
				    NCDFVarID,
				    "Easternmost_Easting",
				    NC_DOUBLE,
				    1,
				    &dfEE );
	status = nc_put_att_double( fpImage,
				    NCDFVarID,
				    "Westernmost_Easting",
				    NC_DOUBLE,
				    1,
				    &dfWE );
	pszWKT = (char *) poSrcDS->GetProjectionRef() ;

	nc_put_att_text( fpImage, 
			 NCDFVarID, 
			 "spatial_ref",
			 strlen( pszWKT ),
			 pszWKT );

	nc_put_att_text( fpImage, 
			 NCDFVarID, 
			 "GeoTransform",
			 strlen( szGeoTransform ),
			 szGeoTransform );

	nc_put_att_text( fpImage, 
			 NCDFVarID, 
			 GRD_MAPPING_NAME,
			 30,
			 "Geographics Coordinate System" );
	nc_put_att_text( fpImage, 
			 NCDFVarID, 
			 LNG_NAME,
			 14,
			 "Grid_latitude" );
    }

    if( oSRS.IsProjected() )
    {
	const char *pszParamStr, *pszParamVal;
        const OGR_SRSNode *poPROJCS = oSRS.GetAttrNode( "PROJCS" );
	int status;
	int i;
	int NCDFVarID;
	const char  *pszProjection;
	double dfNN=0.0;
	double dfSN=0.0;
	double dfEE=0.0;
	double dfWE=0.0;
	double adfGeoTransform[6];
	char   szGeoTransform[ MAX_STR_LEN ];
	char   szTemp[ MAX_STR_LEN ];

	poSrcDS->GetGeoTransform( adfGeoTransform );

	*szGeoTransform = '\0';
	for( i=0; i<6; i++ ) {
	    sprintf( szTemp, "%g ",
		     adfGeoTransform[i] );
	    strcat( szGeoTransform, szTemp );
	}

	CPLDebug( "GDAL_netCDF", "szGeoTranform = %s", szGeoTransform );

        pszProjection = oSRS.GetAttrValue( "PROJECTION" );
        bWriteGeoTransform = TRUE;


	for(i=0; poNetcdfSRS[i].netCDFSRS != NULL; i++ ) {
	    if( EQUAL( poNetcdfSRS[i].SRS, pszProjection ) ) {
		CPLDebug( "GDAL_netCDF", "PROJECTION = %s", 
			  poNetcdfSRS[i].netCDFSRS);
		strcpy( pszNetcdfProjection, poNetcdfSRS[i].netCDFSRS );

		break;
	    }
	}

	status = nc_def_var( fpImage, 
			     poNetcdfSRS[i].netCDFSRS, 
			     NC_CHAR, 
			     0, NULL, &NCDFVarID );

	dfNN = adfGeoTransform[3];
	dfSN = ( adfGeoTransform[5] * nYSize ) + dfNN;
	dfWE = adfGeoTransform[0];
	dfEE = ( adfGeoTransform[1] * nXSize ) + dfWE;

        status = nc_put_att_double( fpImage,
				    NCDFVarID, 
				    "Northernmost_Northing",
				    NC_DOUBLE,
				    1,
				    &dfNN );
        status = nc_put_att_double( fpImage,
				    NCDFVarID, 
				    "Southernmost_Northing",
				    NC_DOUBLE,
				    1,
				    &dfSN );
	status = nc_put_att_double( fpImage,
				    NCDFVarID,
				    "Easternmost_Easting",
				    NC_DOUBLE,
				    1,
				    &dfEE );
	status = nc_put_att_double( fpImage,
				    NCDFVarID,
				    "Westernmost_Easting",
				    NC_DOUBLE,
				    1,
				    &dfWE );
	pszWKT = (char *) poSrcDS->GetProjectionRef() ;

	nc_put_att_text( fpImage, 
			 NCDFVarID, 
			 "spatial_ref",
			 strlen( pszWKT ),
			 pszWKT );

	nc_put_att_text( fpImage, 
			 NCDFVarID, 
			 "GeoTransform",
			 strlen( szGeoTransform ),
			 szGeoTransform );

	nc_put_att_text( fpImage, 
			 NCDFVarID, 
			 GRD_MAPPING_NAME,
			 strlen( pszNetcdfProjection ),
			 pszNetcdfProjection );


        for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
        {
            const OGR_SRSNode    *poNode;
	    float fValue;

            poNode = poPROJCS->GetChild( iChild );
            if( !EQUAL(poNode->GetValue(),"PARAMETER") 
                || poNode->GetChildCount() != 2 )
                continue;

/* -------------------------------------------------------------------- */
/*      Look for projection attributes                                  */
/* -------------------------------------------------------------------- */
	    pszParamStr = poNode->GetChild(0)->GetValue();
	    pszParamVal = poNode->GetChild(1)->GetValue();
	    

	    for(i=0; poNetcdfSRS[i].netCDFSRS != NULL; i++ ) {
	 	if( EQUAL( poNetcdfSRS[i].SRS, pszParamStr ) ) {
		    CPLDebug( "GDAL_netCDF", "%s = %s", 
			      poNetcdfSRS[i].netCDFSRS, 
			      pszParamVal );
		    break;
		}
	    }
/* -------------------------------------------------------------------- */
/*      Write Projection attribute                                      */
/* -------------------------------------------------------------------- */
	    sscanf( pszParamVal, "%f", &fValue );
	    if( poNetcdfSRS[i].netCDFSRS != NULL ) {
		nc_put_att_float( fpImage, 
				  NCDFVarID, 
				  poNetcdfSRS[i].netCDFSRS, 
				  NC_FLOAT,
				  1,
				  &fValue );

	    }	
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize Band Map                                             */
/* -------------------------------------------------------------------- */

    for(int j=1; j <= nBands; j++ ) {
	anBandMap[j-1]=j;
    }
    
/* -------------------------------------------------------------------- */
/*      Create netCDF variable                                          */
/* -------------------------------------------------------------------- */

    for( int i=1; i <= nBands; i++ ) {

	char      szBandName[ NC_MAX_NAME ];
	GByte     *pabScanline  = NULL;
	GInt16    *pasScanline  = NULL;
	GInt32    *panScanline  = NULL;
	float     *pafScanline  = NULL;
	double    *padScanline  = NULL;
	int       NCDFVarID;
	size_t    start[ GDALNBDIM ];
	size_t    count[ GDALNBDIM ];
	size_t    nBandNameLen;
	double    dfNoDataValue;
	unsigned char      cNoDataValue;
	float     fNoDataValue;
	int       nlNoDataValue;
	short     nsNoDataValue;
	GDALRasterBandH	hBand;

	GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( i );
        hBand = GDALGetRasterBand( poSrcDS, i );

	sprintf( szBandName, "Band%d", i );

	eDT = poSrcDS->GetRasterBand(i)->GetRasterDataType();
	anBandDims[0] = nYDimID;
	anBandDims[1] = nXDimID;
	CPLErr      eErr = CE_None;

	dfNoDataValue = poSrcBand->GetNoDataValue(0);

	if( eDT == GDT_Byte ) {
	    CPLDebug( "GDAL_netCDF", "%s = GDT_Byte ", szBandName );

	    status = nc_def_var( fpImage, szBandName, NC_BYTE, 
				 GDALNBDIM, anBandDims, &NCDFVarID );


/* -------------------------------------------------------------------- */
/*      Write data line per line                                        */
/* -------------------------------------------------------------------- */

	    pabScanline = (GByte *) CPLMalloc( nBands * nXSize * nYSize *
					       sizeof( GByte ) );
	    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {

/* -------------------------------------------------------------------- */
/*      Read data from band i                                           */
/* -------------------------------------------------------------------- */
		eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
					    pabScanline, nXSize, 1, GDT_Byte,
					    0,0);

/* -------------------------------------------------------------------- */
/*      Write Fill Value                                                */
/* -------------------------------------------------------------------- */

	    cNoDataValue=(unsigned char) dfNoDataValue;
	    nc_put_att_uchar( fpImage,
			       NCDFVarID,
			       _FillValue,
			       NC_CHAR,
			       1,
			       &cNoDataValue );
			   
/* -------------------------------------------------------------------- */
/*      Write Data from Band i                                          */
/* -------------------------------------------------------------------- */
		start[0]=iLine;
		start[1]=0;
		count[0]=1;
		count[1]=nXSize;

/* -------------------------------------------------------------------- */
/*      Put NetCDF file in data mode.                                   */
/* -------------------------------------------------------------------- */
		status = nc_enddef( fpImage );
		status = nc_put_vara_uchar (fpImage, NCDFVarID, start,
					    count, pabScanline);


/* -------------------------------------------------------------------- */
/*      Put NetCDF file back in define mode.                            */
/* -------------------------------------------------------------------- */
		status = nc_redef( fpImage );
		
	    }
	    CPLFree( pabScanline );
/* -------------------------------------------------------------------- */
/*      Int16                                                           */
/* -------------------------------------------------------------------- */

	} else if( ( eDT == GDT_UInt16 ) || ( eDT == GDT_Int16 ) ) {
	    CPLDebug( "GDAL_netCDF", "%s = GDT_Int16 ",szBandName );
	    status = nc_def_var( fpImage, szBandName, NC_SHORT, 
				 GDALNBDIM, anBandDims, &NCDFVarID );

	    pasScanline = (GInt16 *) CPLMalloc( nBands * nXSize * nYSize *
						sizeof( GInt16 ) );
/* -------------------------------------------------------------------- */
/*      Write Fill Value                                                */
/* -------------------------------------------------------------------- */
	    nsNoDataValue= (GInt16) dfNoDataValue;
	    nc_put_att_short( fpImage,
			       NCDFVarID,
			       _FillValue,
			       NC_SHORT,
			       1,
			       &nsNoDataValue );
	    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {

		eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
					    pasScanline, nXSize, 1, GDT_Int16,
					    0,0);

		start[0]=iLine;
		start[1]=0;
		count[0]=1;
		count[1]=nXSize;


		status = nc_enddef( fpImage );
		status = nc_put_vara_short( fpImage, NCDFVarID, start,
					    count, pasScanline);
		status = nc_redef( fpImage );
	    }
	    CPLFree( pasScanline );
/* -------------------------------------------------------------------- */
/*      Int32                                                           */
/* -------------------------------------------------------------------- */

	} else if( (eDT == GDT_UInt32) || (eDT == GDT_Int32) ) {
	    CPLDebug( "GDAL_netCDF", "%s = GDT_Int32 ",szBandName );
	    status = nc_def_var( fpImage, szBandName, NC_INT, 
				 GDALNBDIM, anBandDims, &NCDFVarID );

	    panScanline = (GInt32 *) CPLMalloc( nBands * nXSize * nYSize *
						sizeof( GInt32 ) );
/* -------------------------------------------------------------------- */
/*      Write Fill Value                                                */
/* -------------------------------------------------------------------- */
	    nlNoDataValue= (GInt32) dfNoDataValue;

	    nc_put_att_int( fpImage,
			       NCDFVarID,
			       _FillValue,
			       NC_INT,
			       1,
			       &nlNoDataValue );


	    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {

		eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
					    panScanline, nXSize, 1, GDT_Int32,
					    0,0);

		start[0]=iLine;
		start[1]=0;
		count[0]=1;
		count[1]=nXSize;


		status = nc_enddef( fpImage );
		status = nc_put_vara_int( fpImage, NCDFVarID, start,
					    count, panScanline);
		status = nc_redef( fpImage );
	    }
	    CPLFree( panScanline );
/* -------------------------------------------------------------------- */
/*      float                                                           */
/* -------------------------------------------------------------------- */
	} else if( (eDT == GDT_Float32) ) {
	    CPLDebug( "GDAL_netCDF", "%s = GDT_Float32 ",szBandName );
	    status = nc_def_var( fpImage, szBandName, NC_FLOAT, 
				 GDALNBDIM, anBandDims, &NCDFVarID );

	    pafScanline = (float *) CPLMalloc( nBands * nXSize * nYSize *
					       sizeof( float ) );

/* -------------------------------------------------------------------- */
/*      Write Fill Value                                                */
/* -------------------------------------------------------------------- */
	    fNoDataValue= (float) dfNoDataValue;
	    nc_put_att_float( fpImage,
			       NCDFVarID,
			       _FillValue,
			       NC_FLOAT,
			       1,
			       &fNoDataValue );
			   
	    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {

		eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
					    pafScanline, nXSize, 1, 
					    GDT_Float32,
					    0,0);

		start[0]=iLine;
		start[1]=0;
		count[0]=1;
		count[1]=nXSize;


		status = nc_enddef( fpImage );
		status = nc_put_vara_float( fpImage, NCDFVarID, start,
					    count, pafScanline);
		status = nc_redef( fpImage );
	    }
	    CPLFree( pafScanline );
/* -------------------------------------------------------------------- */
/*      double                                                          */
/* -------------------------------------------------------------------- */
	} else if( (eDT == GDT_Float64) ) {
	    CPLDebug( "GDAL_netCDF", "%s = GDT_Float64 ",szBandName );
	    status = nc_def_var( fpImage, szBandName, NC_DOUBLE, 
				 GDALNBDIM, anBandDims, &NCDFVarID );

	    padScanline = (double *) CPLMalloc( nBands * nXSize * nYSize *
						sizeof( double ) );
/* -------------------------------------------------------------------- */
/*      Write Fill Value                                                */
/* -------------------------------------------------------------------- */
		
	    nc_put_att_double( fpImage,
			       NCDFVarID,
			       _FillValue,
			       NC_DOUBLE,
			       1,
			       &dfNoDataValue );

	    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {

		eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
					    padScanline, nXSize, 1, 
					    GDT_Float64,
					    0,0);

		start[0]=iLine;
		start[1]=0;
		count[0]=1;
		count[1]=nXSize;


		status = nc_enddef( fpImage );
		status = nc_put_vara_double( fpImage, NCDFVarID, start,
					    count, padScanline);
		status = nc_redef( fpImage );
	    }
	    CPLFree( padScanline );
	}
	
/* -------------------------------------------------------------------- */
/*      Write Projection for band                                       */
/* -------------------------------------------------------------------- */
	if( bWriteGeoTransform == TRUE ) {
	    /*	    nc_put_att_text( fpImage, NCDFVarID, 
			     COORDINATES,
			     7,
			     LONLAT );
	    */
	    nc_put_att_text( fpImage, NCDFVarID, 
			     GRD_MAPPING,
			     strlen( pszNetcdfProjection ),
			     pszNetcdfProjection );
	}

	sprintf( szBandName, "GDAL Band Number %d", i);
	nBandNameLen = strlen( szBandName );
	nc_put_att_text( fpImage, 
			 NCDFVarID, 
			 "long_name", 
			 nBandNameLen,
			 szBandName );

	CopyMetadata( (void *) hBand, fpImage, NCDFVarID );

			   

    }



    //    poDstDS->SetGeoTransform( adfGeoTransform );


/* -------------------------------------------------------------------- */
/*      Cleanup and close.                                              */
/* -------------------------------------------------------------------- */
//    CPLFree( pabScanline );

    nc_close( fpImage );
/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    netCDFDataset *poDS = (netCDFDataset *) GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;


    }

/************************************************************************/
/*                          GDALRegister_netCDF()                       */
/************************************************************************/

void GDALRegister_netCDF()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "netCDF" ) == NULL )
    {
        poDriver = new GDALDriver( );
        
        poDriver->SetDescription( "netCDF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Network Common Data Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#netCDF" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "nc" );

        poDriver->pfnOpen = netCDFDataset::Open;
        poDriver->pfnCreateCopy = NCDFCreateCopy;


        GetGDALDriverManager( )->RegisterDriver( poDriver );
    }
}



/* -------------------------------------------------------------------- */
/*      Set Lambert Conformal Conic Projection                          */
/* -------------------------------------------------------------------- */


    
//Albers equal area
//
//grid_mapping_name = albers_conical_equal_area
//
//Map parameters:
//
//    * standard_parallel - There may be 1 or 2 values.
//    * longitude_of_central_meridian
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//Lambert azimuthal equal area
//
//grid_mapping_name = lambert_azimuthal_equal_area
//
//Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//Lambert conformal
//
//grid_mapping_name = lambert_conformal_conic
//
//Map parameters:
//
//    * standard_parallel - There may be 1 or 2 values.
//    * longitude_of_central_meridian
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//Polar stereographic
//
//grid_mapping_name = polar_stereographic
//
//Map parameters:
//
//    * straight_vertical_longitude_from_pole
//    * latitude_of_projection_origin - Either +90. or -90.
//    * Either standard_parallel or scale_factor_at_projection_origin
//    * false_easting
//    * false_northing
//Rotated pole
//
//grid_mapping_name = rotated_latitude_longitude
//
//Map parameters:
//
//    * grid_north_pole_latitude
//    * grid_north_pole_longitude
//    * north_pole_grid_longitude - This parameter is optional (default is 0.).
//Stereographic
//
//grid_mapping_name = stereographic
//
//Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * scale_factor_at_projection_origin
//    * false_easting
//    * false_northing
//Transverse Mercator
//
//grid_mapping_name = transverse_mercator
//
//Map parameters:
//
//    * scale_factor_at_central_meridian
//    * longitude_of_central_meridian
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
//
//
//Grid mapping attributes
//
//false_easting 	
//false_northing 	
//grid_mapping_name 	
//grid_north_pole_latitude
//grid_north_pole_longitude
//latitude_of_projection_origin 
//longitude_of_central_meridian 
//longitude_of_projection_origin
//north_pole_grid_longitude 	
//scale_factor_at_central_meridian 
//scale_factor_at_projection_origin 
//standard_parallel 	
//straight_vertical_longitude_from_pole 	


