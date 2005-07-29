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
 * Revision 1.7  2005/07/29 02:33:32  dnadeau
 * enhance netcdf to conform to CF convention.  Allow multidimensionnal array and read metatdata
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

#include "gdal_pam.h"
#include "gdal_frmts.h"
#include "cpl_string.h"
#include "netcdf.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*			     netCDFDataset				*/
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand;

class netCDFDataset : public GDALPamDataset
{
    double      adfGeoTransform[6];
    char        **papszMetadata;

  public:
    int         cdfid;

		netCDFDataset();
		~netCDFDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr      ReadGlobalAttributes( int, int, int );

    CPLErr 	GetGeoTransform( double * padfTransform );
};

/************************************************************************/
/* ==================================================================== */
/*                         netCDFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand : public GDALPamRasterBand
{
    nc_type nc_datatype;
    int         nZId;
    int		nLevel;
    int         nBandXPos;
    int         nBandYPos;
    int         *panBandZPos;
    int         *panBandZLev;
    int         bNoDataSet;
    double      dfNoDataValue;

  public:

    netCDFRasterBand( netCDFDataset *poDS, 
		      int nZId, 
		      int nLevel, 
		      int *panBandZLen,
		      int *panBandPos, 
		      int nBand );
    virtual double          GetNoDataValue( int * );
    virtual CPLErr          SetNoDataValue( double );
    virtual CPLErr IReadBlock( int, int, void * );


};

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
/*                          netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::netCDFRasterBand( netCDFDataset *poDS, int nZId, int nLevel, 
				    int *panBandZLev, int *panBandDimPos, 
				    int nBand)

{
    double dfNoData;
    int    bNoDataSet = FALSE;
    nc_type  vartype;

    this->poDS = poDS;
    this->nBand = nBand;
    this->nZId = nZId;
    this->nLevel = nLevel;
    this->nBandXPos = panBandDimPos[0];
    this->nBandYPos = panBandDimPos[1];
/* -------------------------------------------------------------------- */
/*      Take care of all other dimmensions                              */
/* ------------------------------------------------------------------ */
    if (sizeof(panBandDimPos) > 2) {
	this->panBandZPos = 
	    (int *) CPLCalloc( sizeof(panBandDimPos)-2, sizeof( int ) );
	this->panBandZLev = 
	    (int *) CPLCalloc( sizeof(panBandDimPos)-2, sizeof( int ) );

	for ( unsigned int i=0; i < sizeof( panBandDimPos ) - 2; i++ ){
	    this->panBandZPos[i] = panBandDimPos[i+2];
	    this->panBandZLev[i] = panBandZLev[i];
	}
    }

    bNoDataSet = FALSE;
    dfNoDataValue = -9999.0;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

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
    nc_type atttype;		/* attribute */
    size_t  attlen;

    nc_inq_att( poDS->cdfid, nZId, _FillValue, &atttype, &attlen);
    nc_inq_att( poDS->cdfid, nZId, "missing_value", &atttype, &attlen);
    nc_inq_vartype( poDS->cdfid, nZId, &vartype);
    
    if( atttype == vartype && attlen == 1) {
	if(vartype == NC_CHAR) {
	    char fillc;
	    nc_get_att_text( poDS->cdfid, nZId, "missing_value", &fillc );
	    dfNoData = atof(&fillc);
	} else {
	    nc_get_att_double(poDS->cdfid, nZId, "missing_value", &dfNoData);
	}
    } else {
	switch (vartype) {
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
    int    nErr;
    int    cdfid = ((netCDFDataset *) poDS)->cdfid;
    size_t start[MAX_NC_DIMS], edge[MAX_NC_DIMS];
    char   pszName[1024];
    int    i,j;
    int    Sum,Taken;
    int    nd;

    *pszName='\0';
    memset(start,0,sizeof(start));
    memset(edge,0,sizeof(edge));
    nc_inq_varndims ( cdfid, nZId, &nd );
/* -------------------------------------------------------------------- */
/*      Locate X, Y and Z position in the array                         */
/* -------------------------------------------------------------------- */
	
    start[nBandXPos] = 0;          // x dim can move arround in array
    start[nBandYPos] = nBlockYOff; // y
        
    edge[nBandXPos] = nBlockXSize; 
    edge[nBandYPos] = 1;

    if( nd == 3 )
    {
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
	for( i=0; i < nd-2-1 ; i++ ){
	    Sum  = 1;
	    Taken = 0;
	    for( j=i+1; j < nd-2; j++ ){
		Sum *= panBandZLev[j];
	    }
	    //printf("Sum = %d\n",Sum);
	    start[panBandZPos[i]] = (nLevel-Taken) / Sum;
	    edge[ panBandZPos[i]] = 1;
	    Taken += Sum;
	    //printf("Taken = %d\n",Taken);
	}
	start[panBandZPos[i]] = (nLevel-(Taken-Sum)) % Sum;
	edge[ panBandZPos[i]] = 1;

    }

    //nc_inq_varname  ( cdfid, nZId, pszName);
    //printf("Var Name= %s\n",pszName);
    //printf("start[0] = %d\n",start[0]);
    //printf("start[1] = %d\n",start[1]);
    //printf("start[2] = %d\n",start[2]);
    //printf("start[3] = %d\n",start[3]);
    //
    //printf("edge[0] = %d\n",edge[0]);
    //printf("edge[1] = %d\n",edge[1]);
    //printf("edge[2] = %d\n",edge[2]);
    //printf("edge[3] = %d\n",edge[3]);



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
	    if(isnan(((float *)pImage)[i]))
		((float *)pImage)[i] = dfNoDataValue;
	}
    }
    else if( eDataType == GDT_Float64 ){
        nErr = nc_get_vara_double( cdfid, nZId, start, edge, 
                                   (double *) pImage );
	for( i=0; i<nBlockXSize; i++ ){
	    if(isnan(((double *)pImage)[i])) 
		((double *)pImage)[i] = dfNoDataValue;
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
    papszMetadata   = NULL;						
}


/************************************************************************/
/*                           ~netCDFDataset()                           */
/************************************************************************/

netCDFDataset::~netCDFDataset()

{

    FlushCache();
    if( papszMetadata != NULL )
      CSLDestroy( papszMetadata );
    nc_close (cdfid);
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr netCDFDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                        ReadGlobalAttributes()                        */
/************************************************************************/

CPLErr netCDFDataset::ReadGlobalAttributes( int cdfid, int var, int nBand )

{
    char szAttrName[NC_MAX_NAME];
    char szVarName[NC_MAX_NAME];
    char szMetaName[8192];
    char szMetaTemp[8192]="";
    nc_type pnAttrType;
    size_t  pnAttrLen,m;
    int nbAttr;
    char  szTemp[8192];
    
    nc_inq_varnatts( cdfid, var, &nbAttr );
    if( var == NC_GLOBAL ) {
	strcpy( szVarName,"NC_GLOBAL" );
    }
    else {
	nc_inq_varname(  cdfid, var, szVarName );
    }
    
    if( nBand >= 0 ) {
	sprintf( szMetaName,"%s:GDALDataBand",szVarName);
	sprintf( szMetaTemp,"%d",nBand+1);
	papszMetadata = CSLSetNameValue(papszMetadata, 
					szMetaName, 
					szMetaTemp);
    }
    for( int l=0; l < nbAttr; l++){
	
	nc_inq_attname( cdfid, var, l, szAttrName);
	sprintf( szMetaName, "%s:%s", szVarName, szAttrName  );
	*szMetaTemp='\0';
	nc_inq_att( cdfid, var, szAttrName, &pnAttrType, &pnAttrLen );
	
	
	switch (pnAttrType) {
	case NC_CHAR:
	    char *pszTemp;
	    pszTemp = (char *) CPLCalloc( pnAttrLen+1, sizeof( char ) );
	    nc_get_att_text( cdfid, var, szAttrName,pszTemp );
	    pszTemp[pnAttrLen]='\0';
	    strcpy(szMetaTemp,pszTemp);
	    CPLFree(pszTemp);
	    break;
	case NC_SHORT:
	    short *psTemp;
	    
	    psTemp = (short *) CPLCalloc( pnAttrLen, sizeof( short ) );
	    nc_get_att_short( cdfid, var, szAttrName, psTemp );
	    for(m=0; m < pnAttrLen-1; m++) {
		sprintf( szTemp, "%d, ",psTemp[m] );
		strcat(szMetaTemp,szTemp);
	    }
	    sprintf( szTemp, "%d",psTemp[m] );
	    strcat(szMetaTemp,szTemp);
	    
	    break;
	case NC_INT:
	    int *pnTemp;
	    
	    pnTemp = (int *) CPLCalloc( pnAttrLen, sizeof( int ) );
	    nc_get_att_int( cdfid, var, szAttrName, pnTemp );
	    for(m=0; m < pnAttrLen-1; m++) {
		sprintf( szTemp, "%d",pnTemp[m] );
		strcat(szMetaTemp,szTemp);
	    }
	    sprintf( szTemp, "%d",pnTemp[m] );
	    strcat(szMetaTemp,szTemp);
	    break;
	case NC_FLOAT:
	    float *pfTemp;
	    printf("pnAttrLen = %d ",pnAttrLen);
	    pfTemp = (float *) CPLCalloc( pnAttrLen, sizeof( float ) );
	    nc_get_att_float( cdfid, var, szAttrName, pfTemp );
	    for(m=0; m < pnAttrLen-1; m++) {
		sprintf( szTemp, "%e",pfTemp[m] );
		strcat(szMetaTemp,szTemp);
	    }
	    sprintf( szTemp, "%e",pfTemp[m] );
	    strcat(szMetaTemp,szTemp);
	    
	    break;
	case NC_DOUBLE:
	    double *pdfTemp;
	    pdfTemp = (double *) CPLCalloc(pnAttrLen, sizeof(double));
	    nc_get_att_double( cdfid, var, szAttrName, pdfTemp );
	    for(m=0; m < pnAttrLen-1; m++) {
		sprintf( szTemp, "%g",pdfTemp[m] );
		strcat(szMetaTemp,szTemp);
	    }
	    sprintf( szTemp, "%g",pdfTemp[m] );
	    strcat(szMetaTemp,szTemp);
	    
	    break;
	default:
	    break;
	}
	
	papszMetadata = CSLSetNameValue(papszMetadata, 
					szMetaName, 
					szMetaTemp);
	
    }
	
    SetMetadata( papszMetadata );
    return CE_None;
}
/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *netCDFDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Does this file have the netCDF magic number?                    */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 50 )
        return NULL;

    if( poOpenInfo->pabyHeader[0] != 'C' 
        || poOpenInfo->pabyHeader[1] != 'D' 
        || poOpenInfo->pabyHeader[2] != 'F' 
        || poOpenInfo->pabyHeader[3] != 1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int cdfid, dim_count, var_count;

    if( nc_open( poOpenInfo->pszFilename, NC_NOWRITE, &cdfid ) != NC_NOERR )
        return NULL;

    if( nc_inq_ndims( cdfid, &dim_count ) != NC_NOERR || dim_count < 2 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "%s is a netCDF file, but not in GMT configuration.",
                  poOpenInfo->pszFilename );

        nc_close( cdfid );
        return NULL;
    }
    CPLDebug( "GDAL_netCDF", "dim_count = %d\n", dim_count );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    netCDFDataset 	*poDS;

    poDS = new netCDFDataset();

    poDS->cdfid = cdfid;
    
/* -------------------------------------------------------------------- */
/*      Get dimensions.  If we can't find this, then this is a          */
/*      netCDF file, but not a normal grid product.                     */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Find out which dimension to use for X and Y axis.               */
/* -------------------------------------------------------------------- */
    size_t xdim, ydim;
    int j;
    int *panBandDimPos;         // X, Y, Z postion in array
    int *panBandZLev;
    unsigned int k;
    int nDimXid,nDimYid;
    int *paDimIds;

    char papszDimName[NC_MAX_NAME][1024];
    for( j=0; j < dim_count; j++ ){
	nc_inq_dimname( cdfid, j, papszDimName[j] );

/* -------------------------------------------------------------------- */
/*      Make sure variable all in lowercase before comparison           */
/* -------------------------------------------------------------------- */
	for( k=0; k < strlen(papszDimName[j]); k++ )
	    if( isupper( papszDimName[j][k] ) ) 
		papszDimName[j][k] = tolower(papszDimName[j][k]);

	if ( EQUAL( papszDimName[j],"lat" ) ||
	     EQUAL( papszDimName[j], "latitude" ) ||
	     EQUAL( papszDimName[j],"y" ) ){
	    nc_inq_dimlen ( cdfid, j, &ydim );
	    poDS->nRasterYSize = ydim;
	    nDimYid=j;
	}
	if ( EQUAL( papszDimName[j],"lon" ) ||
	     EQUAL( papszDimName[j], "longitude" ) ||
	     EQUAL( papszDimName[j],"x" ) ){
	    nc_inq_dimlen ( cdfid, j, &xdim );
	    poDS->nRasterXSize = xdim;
	    nDimXid=j;
	}
    }


/* -------------------------------------------------------------------- */
/*      Get x/y range information.                                      */
/* -------------------------------------------------------------------- */
    size_t start[2], edge[2];
    int x_range_id, y_range_id;

    if( nc_inq_varid (cdfid, "x_range", &x_range_id) == NC_NOERR 
        && nc_inq_varid (cdfid, "y_range", &y_range_id) == NC_NOERR )
    {
        double x_range[2], y_range[2];

        nc_get_vara_double( cdfid, x_range_id, start, edge, x_range );
        nc_get_vara_double( cdfid, y_range_id, start, edge, y_range );

        poDS->adfGeoTransform[0] = x_range[0];
        poDS->adfGeoTransform[1] = 
            (x_range[1] - x_range[0]) / poDS->nRasterXSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = y_range[1];
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 
            (y_range[0] - y_range[1]) / poDS->nRasterYSize;
    }
    else
    {
        poDS->adfGeoTransform[0] = 0.0;
        poDS->adfGeoTransform[1] = 1.0;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 1.0;
    }    



/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    if ( nc_inq_nvars ( cdfid, &var_count) != NC_NOERR )
	return NULL;    

    CPLDebug( "GDAL_netCDF", "var_count = %d\n", var_count );

    // Add new band for each variable - 3. dimension level
    int i = 0;
    //printf ( "var_count = %d\n", var_count );
    for ( int var = 0; var < var_count; var++ ) {
	int nd;
	
	nc_inq_varndims ( cdfid, var, &nd );

	paDimIds = (int *)CPLCalloc(nd, sizeof(int));
	panBandDimPos = ( int * ) CPLCalloc( nd, sizeof( int ) );
	nc_inq_vardimid( cdfid, var, paDimIds );

	if ( ( nd < 2 ) || ( nd > 4 ) )
	    continue;
    
/* -------------------------------------------------------------------- */
/*      Verify that this variable dimensions have X name and Y          */
/*      name. If it does not, I reject that variable.                   */
/* -------------------------------------------------------------------- */
	for( j=0,k=0; j < nd; j++ ){
	    if( paDimIds[j] == nDimXid ){ 
		panBandDimPos[0] = j;         // Save Position of XDim
		k++;
	    }
	    if( paDimIds[j] == nDimYid ){
		panBandDimPos[1] = j;         // Save Position of YDim
		k++;
	    }
	}
/* -------------------------------------------------------------------- */
/*      X and Y Dimension Ids were not found!                           */
/* -------------------------------------------------------------------- */
	if( k != 2 ) continue;
	    
	//printf ( "var = %d nd = %d\n", var, nd );
/* -------------------------------------------------------------------- */
/*      Read Metadata for each variable                                 */
/* -------------------------------------------------------------------- */
	poDS->ReadGlobalAttributes(cdfid,NC_GLOBAL,-1);	
	poDS->ReadGlobalAttributes(cdfid,var,i);

/* -------------------------------------------------------------------- */
/*      Look for third dimension ID                                     */
/* -------------------------------------------------------------------- */
	size_t lev_count;
	size_t nTotLevCount = 1;
	int nDim = 2;
	panBandZLev = (int *)CPLCalloc( sizeof( panBandDimPos ) - 2, 
					sizeof( int ) );
	if ( dim_count > 2 ) {
	    for( j=0; j < nd; j++ ){
		if( ( paDimIds[j] != nDimXid ) && 
		    ( paDimIds[j] != nDimYid ) ){
		    nc_inq_dimlen ( cdfid, paDimIds[j], &lev_count );
		    nTotLevCount *= lev_count;
		    panBandZLev[ nDim-2 ] = lev_count;
		    printf("ZLev[%d]=%d\n",nDim-2,lev_count);
		    panBandDimPos[nDim++] = j;         // Save Position of ZDim

		}
	    }
	}
        printf ( "nTotLevCount = %d\n", nTotLevCount );
	for ( int lev = 0; lev < (int) lev_count; lev++ ) {
	    //printf ( "lev = %d\n", lev );
	    netCDFRasterBand *poBand = new netCDFRasterBand(poDS, var, 
							    lev, 
							    panBandZLev,
							    panBandDimPos,
							    i+1);
            poDS->SetBand( i+1, poBand);
	    i++;
	}
	CPLFree(paDimIds);
	CPLFree(panBandDimPos);
	CPLFree(panBandZLev);
    }
    poDS->nBands = i;
        
/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}
/************************************************************************/
/*                          GDALRegister_netCDF()                          */
/************************************************************************/

void GDALRegister_netCDF()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "netCDF" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "netCDF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "network Common Data Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#netCDF" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "nc" );

        poDriver->pfnOpen = netCDFDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
