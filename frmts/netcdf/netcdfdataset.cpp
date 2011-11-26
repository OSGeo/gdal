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
 ****************************************************************************/

#include "netcdfdataset.h"
#include "cpl_error.h"
CPL_CVSID("$Id$");

#include <map> //for NCDFWriteProjAttribs()

/* Internal function declarations */

int NCDFIsGDALVersionGTE(const char* pszVersion, int nTarget);

void NCDFAddGDALHistory( int fpImage, const char * pszFilename, const char *pszOldHist );

void NCDFAddHistory(int fpImage, const char *pszAddHist, const char *pszOldHist);

int NCDFIsCfProjection( const char* pszProjection );

void NCDFWriteProjAttribs(const OGR_SRSNode *poPROJCS,
                            const char* pszProjection,
                            const int fpImage, const int NCDFVarID);

/************************************************************************/
/*                         MISC Notes                                   */
/************************************************************************/
/* Various bugs fixed / new features:
- bottom-up by default #4284, with config option GDAL_NETCDF_BOTTOMUP to override import and export
  and create option WRITE_BOTTOMUP
- fix projected export+import CF
- added simple progress
- metadata export fixes + add_offset/scale_factor (bug #4211)
- added missing	case NC_SHORT: in netCDFRasterBand::CreateBandMetadata( ) 
*/

/* Various bugs that need fixing:
- support import of unequally-spaced grids
- support of lon/lat grids from CF grids which are not supported
- support variable with multiple values in metadataitem (CreateBandMetadata), 
  this should use a generic function which detects the var type also.
*/

/*
TODO

- test signed byte import
// Detect unsigned Byte data 
    int bUnsignedByte = TRUE;
if ( this->eDataType == GDT_Byte &&
( atttype == NC_CHAR && EQUAL(szMetaTemp,"_Unsigned") 
&& ( EQUAL(szTemp,"true") || EQUAL(szTemp,"1") ) ) ) {
bUnsigned = TRUE;
printf("Is unsigned\n");
}

*/


/************************************************************************/
/* ==================================================================== */
/*                         netCDFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand : public GDALPamRasterBand
{
    nc_type     nc_datatype;
    int         nZId;
    int         nZDim;
    int         nLevel;
    int         nBandXPos;
    int         nBandYPos;
    int         *panBandZPos;
    int         *panBandZLev;
    int         bNoDataSet;
    double      dfNoDataValue;
    double      dfScale;
    double      dfOffset;
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
    virtual double          GetOffset( int * );
    virtual CPLErr          SetOffset( double );
    virtual double          GetScale( int * );
    virtual CPLErr          SetScale( double );
    virtual CPLErr IReadBlock( int, int, void * );

};

/************************************************************************/ 
/*                             GetOffset()                              */ 
/************************************************************************/ 
double netCDFRasterBand::GetOffset( int *pbSuccess ) 
{ 
    if( pbSuccess != NULL ) 
        *pbSuccess = TRUE; 
	 
    return dfOffset; 
}

/************************************************************************/ 
/*                             SetOffset()                              */ 
/************************************************************************/ 
CPLErr netCDFRasterBand::SetOffset( double dfNewOffset ) 
{ 
    dfOffset = dfNewOffset; 
    return CE_None; 
}

/************************************************************************/ 
/*                              GetScale()                              */ 
/************************************************************************/ 
double netCDFRasterBand::GetScale( int *pbSuccess ) 
{ 
    if( pbSuccess != NULL ) 
        *pbSuccess = TRUE; 
    return dfScale; 
}

/************************************************************************/ 
/*                              SetScale()                              */ 
/************************************************************************/ 
CPLErr netCDFRasterBand::SetScale( double dfNewScale )  
{ 
    dfScale = dfNewScale; 
    return CE_None; 
} 

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double netCDFRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataSet;

    if( bNoDataSet )
        return dfNoDataValue;
    else
        return GDALPamRasterBand::GetNoDataValue( pbSuccess );
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
    char     szMetaTemp[NCDF_MAX_STR_LEN];
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
            Sum = 1;
            for( j=i+1; j < nd-2; j++ ) {
                Sum *= panBandZLev[j];
            }
            result = (int) ( ( nLevel-Taken) / Sum );
        }
        else {
            result = (int) ( ( nLevel-Taken) % Sum );
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
            szVarName[0]=(char) toupper(szVarName[0]);
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
                    sprintf( szMetaTemp,"%.7g", fData );
                    break;
                case NC_DOUBLE:
                    double dfData;
                    status =  nc_get_vara_double( poDS->cdfid, nVarID, 
                                                  start,
                                                  count, &dfData);
                    sprintf( szMetaTemp,"%.15g", dfData );
                    break;
                default: 
                    CPLDebug( "GDAL_netCDF", "invalid dim %s, type=%d", 
                              szMetaTemp, nVarType);
                    break;
            }
        }
        else
            sprintf( szMetaTemp,"%d", result+1);
	
        CPLDebug( "GDAL_netCDF", "setting dimension metadata %s=%s", 
                  szMetaName, szMetaTemp );

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

/* -------------------------------------------------------------------- */
/*      Get all other metadata                                          */
/* -------------------------------------------------------------------- */

    int nAtt=0;
    nc_type  atttype=NC_NAT;
    size_t   attlen;
    float fval;
    double dval;
    short sval;
    int ival;
    int bValidMeta;

    nc_inq_varnatts( poDS->cdfid, nZId, &nAtt );
    for( i=0; i < nAtt ; i++ ) {
        bValidMeta = TRUE;
    	status = nc_inq_attname( poDS->cdfid, nZId, 
    				 i, szTemp);
    	status = nc_inq_att( poDS->cdfid, nZId, 
    			     szTemp, &atttype, &attlen);
    	if(strcmp(szTemp,_FillValue) ==0) continue;
    	sprintf( szMetaTemp,"%s",szTemp);       

    	switch( atttype ) {
            /* TODO support NC_BYTE */
    	case NC_CHAR:
    	    char *fillc;
    	    fillc = (char *) CPLCalloc( attlen+1, sizeof(char) );
    	    status=nc_get_att_text( poDS->cdfid, nZId,
    				    szTemp, fillc );
    	    // SetMetadataItem( szMetaTemp, fillc );
    	    sprintf( szTemp,"%s",fillc);
    	    CPLFree(fillc);
    	    break;
    	case NC_SHORT:
    	    status = nc_get_att_short( poDS->cdfid, nZId,
    				     szTemp, &sval );
    	    sprintf( szTemp,"%d",sval);
    	    // SetMetadataItem( szMetaTemp, szTemp );
    	    break;
    	case NC_INT:
    	    status = nc_get_att_int( poDS->cdfid, nZId,
    				     szTemp, &ival );
    	    sprintf( szTemp,"%d",ival);
    	    // SetMetadataItem( szMetaTemp, szTemp );
    	    break;
    	case NC_FLOAT:
    	    status = nc_get_att_float( poDS->cdfid, nZId,
    				       szTemp, &fval );
    	    sprintf( szTemp,"%.7g",fval);
    	    // SetMetadataItem( szMetaTemp, szTemp );
    	    break;
    	case NC_DOUBLE:
    	    status = nc_get_att_double( poDS->cdfid, nZId,
                                        szTemp, &dval );
    	    sprintf( szTemp,"%.15g",dval);
            // SetMetadataItem( szMetaTemp, szTemp );
    	    break;
    	default:
            bValidMeta = FALSE;
    	    break;
    	}

        if ( bValidMeta ) {
            CPLDebug( "GDAL_netCDF", "setting metadata %s=%s, type=%d, len=%lu", 
                      szMetaTemp, szTemp, atttype, attlen );
           SetMetadataItem( szMetaTemp, szTemp );
        }
        else {
            CPLDebug( "GDAL_netCDF", "invalid metadata %s, type=%d, len=%lu", 
                      szMetaTemp, atttype, attlen );
        }
        
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
    double   dfNoData = 0.0;
    int      bNoDataSet = FALSE;
    nc_type  vartype=NC_NAT;
    nc_type  atttype=NC_NAT;
    size_t   attlen;
    int      status;
    char     szNoValueName[NCDF_MAX_STR_LEN];


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
            (int *) CPLCalloc( nZDim-1, sizeof( int ) );
        this->panBandZLev = 
            (int *) CPLCalloc( nZDim-1, sizeof( int ) );

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
#ifdef NETCDF_HAS_NC4
    else if( nc_datatype == NC_UBYTE )
        eDataType = GDT_Byte;
#endif    
    else if( nc_datatype == NC_CHAR )
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
            /* TODO support NC_BYTE */
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
                status = -1;
                break;
        }
        // status = nc_get_att_double( poDS->cdfid, nZId, 
        //                             szNoValueName, &dfNoData );

        if ( status == NC_NOERR )
            bNoDataSet = TRUE;
	
    }
    /* if not found NoData, set the default one */
    if ( ! bNoDataSet ) { 
        switch( vartype ) {
            case NC_BYTE:
#ifdef NETCDF_HAS_NC4
            case NC_UBYTE:
#endif    
                /* don't do default fill-values for bytes, too risky */
                dfNoData = 0.0;
                /* should print a warning as users might not be expecting this */
                /* CPLError(CE_Warning, 1,"GDAL netCDF driver is setting default NoData value to 0.0 for NC_BYTE data\n"); */
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
    
    if ( bNoDataSet ) 
        SetNoDataValue( dfNoData );
    else 
        CPLDebug( "GDAL_netCDF", "did not get nodata value for variable #%d", nZId );

    /* -------------------------------------------------------------------- */
    /* Attempt to fetch the scale_factor and add_offset attributes for the  */
    /* variable and set them.  If these values are not available, set       */
    /* offset to 0 and scale to 1                                           */
    /* -------------------------------------------------------------------- */
    double dfOff = 0.0; 
    double dfScale = 1.0; 
    
    if ( nc_inq_attid ( poDS->cdfid, nZId, CF_ADD_OFFSET, NULL) == NC_NOERR ) { 
        status = nc_get_att_double( poDS->cdfid, nZId, CF_ADD_OFFSET, &dfOff );
        CPLDebug( "GDAL_netCDF", "got add_offset=%.15g, status=%d", dfOff, status );
    }
    if ( nc_inq_attid ( poDS->cdfid, nZId, 
                        CF_SCALE_FACTOR, NULL) == NC_NOERR ) { 
        status = nc_get_att_double( poDS->cdfid, nZId, CF_SCALE_FACTOR, &dfScale ); 
        CPLDebug( "GDAL_netCDF", "got scale_factor=%.15g, status=%d", dfScale, status );
    }
    SetOffset( dfOff ); 
    SetScale( dfScale ); 
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
    char   pszName[ NCDF_MAX_STR_LEN ];
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
    // check y order
    if( ( ( netCDFDataset *) poDS )->bBottomUp ) {
        start[nBandYPos] = ( ( netCDFDataset * ) poDS )->ydim - 1 - nBlockYOff;
    } else {
        start[nBandYPos] = nBlockYOff; // y
    }
        
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
    if (nd > 3) 
    {
        Taken = 0;
        for( i=0; i < nd-2 ; i++ ) 
        {
            if( i != nd - 2 -1 ) {
                Sum = 1;
                for( j=i+1; j < nd-2; j++ ) {
                    Sum *= panBandZLev[j];
                }
                start[panBandZPos[i]] = (int) ( ( nLevel-Taken) / Sum );
                edge[panBandZPos[i]] = 1;
            } else {
                start[panBandZPos[i]] = (int) ( ( nLevel-Taken) % Sum );
                edge[panBandZPos[i]] = 1;
            }
            Taken += start[panBandZPos[i]] * Sum;
        }
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
                ( (float *)pImage )[i] = (float) dfNoDataValue;
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
/*				netCDFDataset				                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           netCDFDataset()                            */
/************************************************************************/

netCDFDataset::netCDFDataset()

{
    papszMetadata    = NULL;	
    papszSubDatasets = NULL;
    bGotGeoTransform = FALSE;
    pszProjection    = NULL;
    cdfid            = 0;
    // bBottomUp        = FALSE;
    bBottomUp        = TRUE;
    nFormat          = NCDF_FORMAT_NONE;
    bIsGdalFile      = FALSE;
    bIsGdalCfFile    = FALSE;
}


/************************************************************************/
/*                           ~netCDFDataset()                           */
/************************************************************************/

netCDFDataset::~netCDFDataset()

{
    FlushCache();

    CSLDestroy( papszMetadata );
    CSLDestroy( papszSubDatasets );

    CPLFree( pszProjection );

    if( cdfid ) 
        nc_close( cdfid );
}

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
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                           FetchCopyParm()                            */
/************************************************************************/

double netCDFDataset::FetchCopyParm( const char *pszGridMappingValue, 
                                     const char *pszParm, double dfDefault )

{
    char         szTemp[ MAX_NC_NAME ];
    const char  *pszValue;

    strcpy(szTemp,pszGridMappingValue);
    strcat( szTemp, "#" );
    strcat( szTemp, pszParm );
    pszValue = CSLFetchNameValue(papszMetadata, szTemp);

    if( pszValue )
    {
        return CPLAtofM(pszValue);
    }
    else
        return dfDefault;
}

/************************************************************************/
/*                           FetchStandardParallels()                   */
/************************************************************************/

char** netCDFDataset::FetchStandardParallels( const char *pszGridMappingValue )
{
    char         szTemp[ MAX_NC_NAME ];
    const char   *pszValue;
    char         **papszValues = NULL;
    //cf-1.0 tags
    strcpy( szTemp,pszGridMappingValue );
    strcat( szTemp, "#" );
    strcat( szTemp, CF_PP_STD_PARALLEL );
    pszValue = CSLFetchNameValue( papszMetadata, szTemp );
    if( pszValue != NULL )
        papszValues = CSLTokenizeString2( pszValue, ",", CSLT_STRIPLEADSPACES |
                                          CSLT_STRIPENDSPACES );
    //try gdal tags
    else
    {
        strcpy( szTemp, pszGridMappingValue );
        strcat( szTemp, "#" );
        strcat( szTemp, CF_PP_STD_PARALLEL_1 );

        pszValue = CSLFetchNameValue( papszMetadata, szTemp );
	
        if ( pszValue != NULL )
            papszValues = CSLAddString( papszValues, pszValue );
				    
        strcpy( szTemp,pszGridMappingValue );
        strcat( szTemp, "#" );
        strcat( szTemp, CF_PP_STD_PARALLEL_2 );

        pszValue = CSLFetchNameValue( papszMetadata, szTemp );
	
        if( pszValue != NULL )	
            papszValues = CSLAddString( papszValues, pszValue );
    }
    
    return papszValues;
}

/************************************************************************/
/*                      SetProjectionFromVar()                          */
/************************************************************************/
void netCDFDataset::SetProjectionFromVar( int var )
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

    double       dfStdP1=0.0;
    double       dfStdP2=0.0;
    double       dfCenterLat;
    double       dfCenterLon;
    double       dfScale;
    double       dfFalseEasting;
    double       dfFalseNorthing;
    double       dfCentralMeridian;
    double       dfEarthRadius;
    double       dfInverseFlattening;
    double       dfLonPrimeMeridian;
    double       dfSemiMajorAxis;
    double       dfSemiMinorAxis;
    
    int          bGotGeogCS = FALSE;
    int          bGotCfSRS = FALSE;
    int          bGotGdalSRS = FALSE;
    int          bLookForWellKnownGCS = FALSE;  //this could be a Config Option

    /* These values from CF metadata */
    OGRSpatialReference oSRS;
    int          nVarDimXID = -1;
    int          nVarDimYID = -1;
    double       *pdfXCoord;
    double       *pdfYCoord;
    char         szDimNameX[ MAX_NC_NAME ];
    char         szDimNameY[ MAX_NC_NAME ];
    int          nSpacingBegin;
    int          nSpacingMiddle;
    int          nSpacingLast;
    const char  *pszUnits = NULL;

    /* These values from GDAL metadata */
    const char *pszWKT = NULL;
    const char *pszGeoTransform = NULL;
    char **papszGeoTransform = NULL;

    netCDFDataset * poDS;
    poDS = this;

    CPLDebug( "GDAL_netCDF", "\n=====\nSetProjectionFromVar( %d )\n", var );

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

    nc_inq_varname( cdfid, var, szVarName );
    strcpy(szTemp,szVarName);
    strcat(szTemp,"#");
    strcat(szTemp,CF_GRD_MAPPING);

    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
    if( pszValue ) {
        strcpy(szGridMappingName,szTemp);
        strcpy(szGridMappingValue,pszValue);
    }

    if( !EQUAL( szGridMappingValue, "" )  ) {

        /*  Read grid_mapping metadata */
        nc_inq_varid( cdfid, szGridMappingValue, &nVarProjectionID );
        poDS->ReadAttributes( cdfid, nVarProjectionID );
        
/* -------------------------------------------------------------------- */
/*      Look for GDAL spatial_ref and GeoTransform within grid_mapping  */
/* -------------------------------------------------------------------- */
        CPLDebug( "GDAL_netCDF", "got grid_mapping %s", szGridMappingValue );
        strcpy( szTemp,szGridMappingValue);
        strcat( szTemp, "#" );
        strcat( szTemp, NCDF_SPATIAL_REF);

        pszWKT = CSLFetchNameValue(poDS->papszMetadata, szTemp);
	
        if( pszWKT != NULL ) {
            strcpy( szTemp,szGridMappingValue);
            strcat( szTemp, "#" );
            strcat( szTemp, NCDF_GEOTRANSFORM);
            pszGeoTransform = CSLFetchNameValue(poDS->papszMetadata, szTemp);	    
        }
    }

/* -------------------------------------------------------------------- */
/*      Get information about the file.                                 */
/* -------------------------------------------------------------------- */
/*      Was this file created by the GDAL netcdf driver?                */
/*      Was this file created by the newer (CF-conformant) driver?      */
/* -------------------------------------------------------------------- */
/* 1) If GDAL netcdf metadata is set, and version >= 1.9,               */
/*    it was created with the new driver                                */
/* 2) Else, if spatial_ref and GeoTransform are present in the          */
/*    grid_mapping variable, it was created by the old driver           */
/* -------------------------------------------------------------------- */
    pszValue = CSLFetchNameValue(poDS->papszMetadata, "NC_GLOBAL#GDAL");

   if( pszValue && NCDFIsGDALVersionGTE(pszValue, 1900)) {
        bIsGdalFile = TRUE;
        bIsGdalCfFile = TRUE;
    }
    else  if( pszWKT != NULL && pszGeoTransform != NULL ) {
        bIsGdalFile = TRUE;
        bIsGdalCfFile = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Set default bottom-up default value                             */
/*      New driver is bottom-up, old driver is top-down                 */
/*      Config option GDAL_NETCDF_BOTTOMUP and Y axis dimension variable*/ 
/*      override the default                                            */
/* -------------------------------------------------------------------- */
    pszValue = CPLGetConfigOption( "GDAL_NETCDF_BOTTOMUP", NULL );
    if ( pszValue ) {
        poDS->bBottomUp = CSLTestBoolean( pszValue ) != FALSE;
    }
    else {
        if ( bIsGdalFile && ! bIsGdalCfFile ) 
            poDS->bBottomUp = FALSE;
        else
            poDS->bBottomUp = TRUE;
    }
    CPLDebug( "GDAL_netCDF", 
              "bIsGdalFile=%d bIsGdalCfFile=%d bBottomUp=%d", 
              bIsGdalFile, bIsGdalCfFile, bBottomUp );
 
/* -------------------------------------------------------------------- */
/*      Look for dimension: lon                                         */
/* -------------------------------------------------------------------- */

    memset( szDimNameX, '\0', sizeof( char ) * MAX_NC_NAME );
    memset( szDimNameY, '\0', sizeof( char ) * MAX_NC_NAME );

    for( i = 0; (i < strlen( poDS->papszDimName[ poDS->nDimXid ] )  && 
                 i < 3 ); i++ ) {
        szDimNameX[i]=(char)tolower( ( poDS->papszDimName[poDS->nDimXid] )[i] );
    }
    szDimNameX[3] = '\0';
    for( i = 0; (i < strlen( poDS->papszDimName[ poDS->nDimYid ] )  && 
                 i < 3 ); i++ ) {
        szDimNameY[i]=(char)tolower( ( poDS->papszDimName[poDS->nDimYid] )[i] );
    }
    szDimNameY[3] = '\0';

/* -------------------------------------------------------------------- */
/*      Read grid_mapping information and set projections               */
/* -------------------------------------------------------------------- */

    if( !( EQUAL(szGridMappingName,"" ) ) ) {
     
        strcpy( szTemp, szGridMappingValue );
        strcat( szTemp, "#" );
        strcat( szTemp, CF_GRD_MAPPING_NAME );
        pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);

        if( pszValue != NULL ) {

/* -------------------------------------------------------------------- */
/*      Check for datum/spheroid information                            */
/* -------------------------------------------------------------------- */
            dfEarthRadius = 
                poDS->FetchCopyParm( szGridMappingValue, 
                                     CF_PP_EARTH_RADIUS, 
                                     -1.0 );

            dfLonPrimeMeridian = 
                poDS->FetchCopyParm( szGridMappingValue,
                                     CF_PP_LONG_PRIME_MERIDIAN, 
                                     0.0 );

            dfInverseFlattening = 
                poDS->FetchCopyParm( szGridMappingValue, 
                                     CF_PP_INVERSE_FLATTENING, 
                                     -1.0 );
	    
            dfSemiMajorAxis = 
                poDS->FetchCopyParm( szGridMappingValue, 
                                     CF_PP_SEMI_MAJOR_AXIS, 
                                     -1.0 );
	    
            dfSemiMinorAxis = 
                poDS->FetchCopyParm( szGridMappingValue, 
                                     CF_PP_SEMI_MINOR_AXIS, 
                                     -1.0 );

            //see if semi-major exists if radius doesn't
            if( dfEarthRadius < 0.0 )
                dfEarthRadius = dfSemiMajorAxis;
	    
            //if still no radius, check old tag
            if( dfEarthRadius < 0.0 )
                dfEarthRadius = poDS->FetchCopyParm( szGridMappingValue, 
                                                     CF_PP_EARTH_RADIUS_OLD,
                                                     -1.0 );

            //has radius value
            if( dfEarthRadius > 0.0 ) {
                //check for inv_flat tag
                if( dfInverseFlattening < 0.0 ) {
                    //no inv_flat tag, check for semi_minor
                    if( dfSemiMinorAxis < 0.0 ) {
                        //no way to get inv_flat, use sphere
                        oSRS.SetGeogCS( "unknown", 
                                        NULL, 
                                        "Sphere", 
                                        dfEarthRadius, 0.0 );
                        bGotGeogCS = TRUE;
                    }
                    else {
                        if( dfSemiMajorAxis < 0.0 )
                            dfSemiMajorAxis = dfEarthRadius;
                        //set inv_flat using semi_minor/major
                        dfInverseFlattening = 
                            1.0 / ( dfSemiMajorAxis - dfSemiMinorAxis ) / dfSemiMajorAxis;
                        oSRS.SetGeogCS( "unknown", 
                                        NULL, 
                                        "Spheroid", 
                                        dfEarthRadius, dfInverseFlattening );
                        bGotGeogCS = TRUE;
                    }
                }
                else {
                    oSRS.SetGeogCS( "unknown", 
                                    NULL, 
                                    "Spheroid", 
                                    dfEarthRadius, dfInverseFlattening );
                    bGotGeogCS = TRUE;
                }  

                if ( bGotGeogCS )
                    CPLDebug( "GDAL_netCDF", "got spheroid from CF: (%f , %f)", dfEarthRadius, dfInverseFlattening );

            }
            //no radius, set as wgs84 as default?
            else {
                // This would be too indiscrimant.  But we should set
                // it if we know the data is geographic.
                //oSRS.SetWellKnownGeogCS( "WGS84" );
            }
	    		
/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */

            if( EQUAL( pszValue, CF_PT_TM ) ) {

                dfScale = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_SCALE_FACTOR_MERIDIAN, 1.0 );

                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );
 
                bGotCfSRS = TRUE;
                oSRS.SetTM( dfCenterLat, 
                            dfCenterLon,
                            dfScale,
                            dfFalseEasting,
                            dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }

/* -------------------------------------------------------------------- */
/*      Albers Equal Area                                               */
/* -------------------------------------------------------------------- */

            if( EQUAL( pszValue, CF_PT_AEA ) ) {
                char **papszStdParallels = NULL;
		
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );
		
                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );

                if( papszStdParallels != NULL ) {
		  
                    if ( CSLCount( papszStdParallels ) == 1 ) {
                        /* TODO CF-1 standard says it allows AEA to be encoded with only 1 standard parallel */
                        /* how should this actually map to a 2StdP OGC WKT version? */
                        CPLError( CE_Warning, CPLE_NotSupported, 
                                  "NetCDF driver import of AEA-1SP is not tested, using identical std. parallels\n" );
                        dfStdP1 = CPLAtofM( papszStdParallels[0] );
                        dfStdP2 = dfStdP1;

                    }
		
                    else if( CSLCount( papszStdParallels ) == 2 ) {
                        dfStdP1 = CPLAtofM( papszStdParallels[0] );
                        dfStdP2 = CPLAtofM( papszStdParallels[1] );
                    }
                }
                //old default
                else {
                    dfStdP1 = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_STD_PARALLEL_1, 0.0 );

                    dfStdP2 = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_STD_PARALLEL_2, 0.0 );
                }

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                bGotCfSRS = TRUE;
                oSRS.SetACEA( dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
                              dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );

                CSLDestroy( papszStdParallels );
            }

/* -------------------------------------------------------------------- */
/*      Cylindrical Equal Area                                          */
/* -------------------------------------------------------------------- */

            else if( EQUAL( pszValue, CF_PT_CEA ) || EQUAL( pszValue, CF_PT_LCEA ) ) {
                char **papszStdParallels = NULL;

                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );

                if( papszStdParallels != NULL ) {
                    dfStdP1 = CPLAtofM( papszStdParallels[0] );
                }
                else {
                    //TODO: add support for 'scale_factor_at_projection_origin' variant to standard parallel
                    //Probably then need to calc a std parallel equivalent
                    CPLError( CE_Failure, CPLE_NotSupported, 
                              "NetCDF driver does not support import of CF-1 LCEA "
                              "'scale_factor_at_projection_origin' variant yet.\n" );
                }

                dfCentralMeridian = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );
		
                bGotCfSRS = TRUE;
                oSRS.SetCEA( dfStdP1, dfCentralMeridian,
                             dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
		
            }

/* -------------------------------------------------------------------- */
/*      lambert_azimuthal_equal_area                                    */
/* -------------------------------------------------------------------- */
            else if( EQUAL( pszValue, CF_PT_LAEA ) ) {
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LON_PROJ_ORIGIN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                oSRS.SetProjCS( "LAEA (WGS84) " );
		
                bGotCfSRS = TRUE;
                oSRS.SetLAEA( dfCenterLat, dfCenterLon,
                              dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
		
            }

/* -------------------------------------------------------------------- */
/*      Azimuthal Equidistant                                           */
/* -------------------------------------------------------------------- */
            else if( EQUAL( pszValue, CF_PT_AE ) ) {
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LON_PROJ_ORIGIN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                bGotCfSRS = TRUE;
                oSRS.SetAE( dfCenterLat, dfCenterLon,
                            dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
		
            }

/* -------------------------------------------------------------------- */
/*      Lambert conformal conic                                         */
/* -------------------------------------------------------------------- */
            else if( EQUAL( pszValue, CF_PT_LCC ) ) {
		
                char **papszStdParallels = NULL;
		
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );
		
                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );

                if( papszStdParallels != NULL ) {
		  
                   if ( CSLCount( papszStdParallels ) == 1 ) {
                       /* TODO - this is not CF!!! */
                       dfScale = 
                           poDS->FetchCopyParm( szGridMappingValue, 
                                               SCALE_FACTOR, 1.0 );
                       dfStdP1 = CPLAtofM( papszStdParallels[0] );
                       dfStdP2 = dfStdP1;
                        /* should use dfStdP1 and dfStdP2 instead of dfScale */ 
                       CPLError( CE_Warning, CPLE_NotSupported, 
                                   "NetCDF driver import of LCC-1SP is not tested nor supported, using SetLCC1SP()\n" );
                       oSRS.SetLCC1SP( dfCenterLat, dfCenterLon, dfScale, 
                                       dfFalseEasting, dfFalseNorthing );
                    }
		
                    else if( CSLCount( papszStdParallels ) == 2 ) {
                        dfStdP1 = CPLAtofM( papszStdParallels[0] );
                        dfStdP2 = CPLAtofM( papszStdParallels[1] );
                        oSRS.SetLCC( dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
                                     dfFalseEasting, dfFalseNorthing );
                    }
                }
                //old default
                else {
                    dfStdP1 = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_STD_PARALLEL_1, 0.0 );

                    dfStdP2 = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_STD_PARALLEL_2, 0.0 );

                    oSRS.SetLCC( dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
                                 dfFalseEasting, dfFalseNorthing );
                }				

                bGotCfSRS = TRUE;
                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );

                CSLDestroy( papszStdParallels );
            }
		
/* -------------------------------------------------------------------- */
/*      Is this Latitude/Longitude Grid explicitly                      */
/* -------------------------------------------------------------------- */
	    
            else if ( EQUAL ( pszValue, CF_PT_LATITUDE_LONGITUDE ) ) {
                bGotCfSRS = TRUE;
                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }
/* -------------------------------------------------------------------- */
/*      Mercator                                                        */
/* -------------------------------------------------------------------- */
		  
            else if ( EQUAL ( pszValue, CF_PT_MERCATOR ) ) {
                char **papszStdParallels = NULL;

                /* If there is a standard_parallel, know it is Mercator 2SP */
                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );
                
                if (NULL != papszStdParallels) {
                    /* CF-1 Mercator 2SP always has lat centered at equator */
                    dfStdP1 = CPLAtofM( papszStdParallels[0] );

                    dfCenterLat = 0.0;

                    dfCenterLon = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_LON_PROJ_ORIGIN, 0.0 );
              
                    dfFalseEasting = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_FALSE_EASTING, 0.0 );

                    dfFalseNorthing = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_FALSE_NORTHING, 0.0 );

                    oSRS.SetMercator2SP( dfStdP1, dfCenterLat, dfCenterLon, 
                                      dfFalseEasting, dfFalseNorthing );
                }
                else {
                    dfCenterLon = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_LON_PROJ_ORIGIN, 0.0 );
              
                    dfCenterLat = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                    dfScale = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_SCALE_FACTOR_ORIGIN,
                                             1.0 );

                    dfFalseEasting = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_FALSE_EASTING, 0.0 );

                    dfFalseNorthing = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_FALSE_NORTHING, 0.0 );

                    oSRS.SetMercator( dfCenterLat, dfCenterLon, dfScale, 
                                      dfFalseEasting, dfFalseNorthing );
                }                      

                bGotCfSRS = TRUE;

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }

/* -------------------------------------------------------------------- */
/*      Orthographic                                                    */
/* -------------------------------------------------------------------- */
		  

            else if ( EQUAL ( pszValue, CF_PT_ORTHOGRAPHIC ) ) {
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LON_PROJ_ORIGIN, 0.0 );
	      
                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                bGotCfSRS = TRUE;

                oSRS.SetOrthographic( dfCenterLat, dfCenterLon, 
                                      dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }

/* -------------------------------------------------------------------- */
/*      Polar Stereographic                                             */
/* -------------------------------------------------------------------- */
		  
            else if ( EQUAL ( pszValue, CF_PT_POLAR_STEREO ) ) {
                /* Note: reversing our current mapping on export, from
                   'latitude_of_origin' in OGC WKT to 'standard_parallel' in CF-1
                   TODO: this could do with further verification */
                char **papszStdParallels = NULL;

                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );
                
                if (NULL != papszStdParallels) {
                    dfCenterLat = CPLAtofM( papszStdParallels[0] );
                }
                else {
                    //TODO: not sure how to handle if CF-1 doesn't include a std_parallel:
                    dfCenterLat = 0.0; //just to avoid warning at compilation
                    CPLError( CE_Failure, CPLE_NotSupported, 
                              "The NetCDF driver does not yet to support import of CF-1 Polar stereographic "
                              "without a std_parallel attribute.\n" );
                }
                dfScale = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_SCALE_FACTOR_ORIGIN, 
                                         1.0 );

                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_VERT_LONG_FROM_POLE, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                bGotCfSRS = TRUE;
                oSRS.SetPS( dfCenterLat, dfCenterLon, dfScale, 
                            dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }

/* -------------------------------------------------------------------- */
/*      Stereographic                                                   */
/* -------------------------------------------------------------------- */
		  
            else if ( EQUAL ( pszValue, CF_PT_STEREO ) ) {
	        
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LON_PROJ_ORIGIN, 0.0 );
	      
                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfScale = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_SCALE_FACTOR_ORIGIN,
                                         1.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                bGotCfSRS = TRUE;
                oSRS.SetStereographic( dfCenterLat, dfCenterLon, dfScale, 
                                       dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }
  
/* -------------------------------------------------------------------- */
/*      Is this Latitude/Longitude Grid, default                        */
/* -------------------------------------------------------------------- */
	    
        } else if( EQUAL( szDimNameX,"lon" ) ) {
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

        start[0] = 0;
        edge[0]  = xdim;
        status = nc_get_vara_double( cdfid, nVarDimXID, 
                                     start, edge, pdfXCoord);
        
        edge[0]  = ydim;
        status = nc_get_vara_double( cdfid, nVarDimYID, 
                                     start, edge, pdfYCoord);

/* -------------------------------------------------------------------- */
/*      Check for bottom-up from the Y-axis order                       */
/*      see bugs #4284 and #4251                                        */
/* -------------------------------------------------------------------- */

        if ( pdfYCoord[0] > pdfYCoord[1] )
            poDS->bBottomUp = FALSE;
        else
            poDS->bBottomUp = TRUE;

        CPLDebug( "GDAL_netCDF", "set bBottomUp = %d from Y axis", poDS->bBottomUp );


/* -------------------------------------------------------------------- */
/*      Is pixel spacing is uniform accross the map?                    */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Check Longitude                                                 */
/* -------------------------------------------------------------------- */

        nSpacingBegin   = (int) poDS->rint((pdfXCoord[1]-pdfXCoord[0]) * 1000);
	
        nSpacingMiddle  = (int) poDS->rint((pdfXCoord[xdim / 2] - 
                                            pdfXCoord[(xdim / 2) + 1]) * 1000);
	
        nSpacingLast    = (int) poDS->rint((pdfXCoord[xdim - 2] - 
                                            pdfXCoord[xdim-1]) * 1000);
	
        if( ( abs( nSpacingBegin )  ==  abs( nSpacingLast )     )  &&
            ( abs( nSpacingBegin )  ==  abs( nSpacingMiddle )   ) &&
            ( abs( nSpacingMiddle ) ==  abs( nSpacingLast )     ) ) {

/* -------------------------------------------------------------------- */
/*      Longitude is equally spaced, check latitude                     */
/* -------------------------------------------------------------------- */
            nSpacingBegin   = (int) poDS->rint((pdfYCoord[1]-pdfYCoord[0]) * 
                                               1000); 
	    
            nSpacingMiddle  = (int) poDS->rint((pdfYCoord[ydim / 2] - 
                                                pdfYCoord[(ydim / 2) + 1]) * 
                                               1000);
	    
            nSpacingLast    = (int) poDS->rint((pdfYCoord[ydim - 2] - 
                                                pdfYCoord[ydim-1]) * 
                                               1000);

		    
/* -------------------------------------------------------------------- */
/*   For Latitude  we allow an error of 0.1 degrees for gaussian        */
/*   gridding                                                           */
/* -------------------------------------------------------------------- */

            if((( abs( abs(nSpacingBegin) - abs(nSpacingLast) ) )   < 100 ) &&
               (( abs( abs(nSpacingBegin) -  abs(nSpacingMiddle) ) ) < 100 ) &&
               (( abs( abs(nSpacingMiddle) - abs(nSpacingLast) ) )   < 100) ) {

                if( ( abs( nSpacingBegin )  !=  abs( nSpacingLast )     )  ||
                    ( abs( nSpacingBegin )  !=  abs( nSpacingMiddle )   ) ||
                    ( abs( nSpacingMiddle ) !=  abs( nSpacingLast )     ) ) {
		    
                    CPLError(CE_Warning, 1,"Latitude grid not spaced evenly.\nSeting projection for grid spacing is within 0.1 degrees threshold.\n");

                }
/* -------------------------------------------------------------------- */
/*      We have gridded data s we can set the Gereferencing info.       */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Enable GeoTransform                                             */
/* -------------------------------------------------------------------- */
                /* ----------------------------------------------------------*/
                /*    In the following "actual_range" and "node_offset"      */
                /*    are attributes used by netCDF files created by GMT.    */
                /*    If we find them we know how to proceed. Else, use      */
                /*    the original algorithm.                                */
                /* --------------------------------------------------------- */
                double	dummy[2], xMinMax[2], yMinMax[2];
                int	node_offset = 0;

                poDS->bGotGeoTransform = TRUE;

                nc_get_att_int (cdfid, NC_GLOBAL, "node_offset", &node_offset);

                if (!nc_get_att_double (cdfid, nVarDimXID, "actual_range", dummy)) {
                    xMinMax[0] = dummy[0];		
                    xMinMax[1] = dummy[1];
                }
                else {
                    xMinMax[0] = pdfXCoord[0];
                    xMinMax[1] = pdfXCoord[xdim-1];
                    node_offset = 0;
                }

                if (!nc_get_att_double (cdfid, nVarDimYID, "actual_range", dummy)) {
                    yMinMax[0] = dummy[0];		
                    yMinMax[1] = dummy[1];
                }
                else {
                    yMinMax[0] = pdfYCoord[0];	
                    yMinMax[1] = pdfYCoord[ydim-1];
                    node_offset = 0;
                }

                /* for CF-1 conventions, assume bottom first */
                /* was here, now is higher up in the code */
                // if( ( EQUAL( szDimNameY, "lat" ) || EQUAL( szDimNameY, "y" ) )
                //     && pdfYCoord[0] < pdfYCoord[1] )
                //     poDS->bBottomUp = TRUE;

                /* Check for reverse order of y-coordinate */
                if ( yMinMax[0] > yMinMax[1] ) {
                    dummy[0] = yMinMax[1];
                    dummy[1] = yMinMax[0];
                    yMinMax[0] = dummy[0];
                    yMinMax[1] = dummy[1];
                }

                /* ----------------------------------------------------------*/
                /*    Many netcdf files are weather files distributed        */
                /*    in km for the x/y resolution.  This isn't perfect,     */
                /*    but geotransforms can be terribly off if this isn't    */
                /*    checked and accounted for.  Maybe one more level of    */
                /*    checking (grid_mapping_value#GRIB_param_Dx, or         */
                /*    x#grid_spacing), but those are not cf tags.            */
                /*    Have to change metadata value if change Create() to    */
                /*    write cf tags                                          */
                /* ----------------------------------------------------------*/
                
                //check units for x and y, expand to other values 
                //and conversions.
                if( oSRS.IsProjected( ) ) {
                    strcpy( szTemp, "x" );
                    strcat( szTemp, "#units" );
                    pszValue = CSLFetchNameValue( poDS->papszMetadata, 
                                                  szTemp );
                    if( pszValue != NULL ) {
                        pszUnits = pszValue;
                        if( EQUAL( pszValue, "km" ) ) {
                            xMinMax[0] = xMinMax[0] * 1000;
                            xMinMax[1] = xMinMax[1] * 1000;
                        }
                    }
                    strcpy( szTemp, "y" );
                    strcat( szTemp, "#units" );
                    pszValue = CSLFetchNameValue( poDS->papszMetadata, 
                                                  szTemp );
                    if( pszValue != NULL ) {
                        /* TODO: see how to deal with diff. values */
                        // if ( ! EQUAL( pszValue, szUnits ) )
                        //     strcpy( szUnits, "\0" );
                        if( EQUAL( pszValue, "km" ) ) {
                            yMinMax[0] = yMinMax[0] * 1000;
                            yMinMax[1] = yMinMax[1] * 1000;
                        }
                    }
                }

                poDS->adfGeoTransform[0] = xMinMax[0];
                poDS->adfGeoTransform[2] = 0;
                poDS->adfGeoTransform[3] = yMinMax[1];
                poDS->adfGeoTransform[4] = 0;
                poDS->adfGeoTransform[1] = ( xMinMax[1] - xMinMax[0] ) / 
                    ( poDS->nRasterXSize + (node_offset - 1) );
                poDS->adfGeoTransform[5] = ( yMinMax[0] - yMinMax[1] ) / 
                    ( poDS->nRasterYSize + (node_offset - 1) );

/* -------------------------------------------------------------------- */
/*     Compute the center of the pixel                                  */
/* -------------------------------------------------------------------- */
                if ( !node_offset ) {	// Otherwise its already the pixel center
                    poDS->adfGeoTransform[0] -= (poDS->adfGeoTransform[1] / 2);
                    poDS->adfGeoTransform[3] -= (poDS->adfGeoTransform[5] / 2);
                }
            }// end if (Latitude is equally spaced, within 0.1 degrees)
            else {
                CPLDebug( "GDAL_netCDF", 
                          "Latitude is not equally spaced." );
            }
        }// end if (Longitude is equally spaced)
        else {
            CPLDebug( "GDAL_netCDF", 
                      "Longitude is not equally spaced." );
        }

        CPLFree( pdfXCoord );
        CPLFree( pdfYCoord );
    }// end if (has dims)

    CPLDebug( "GDAL_netCDF", 
              "bGotGeogCS=%d bGotCfSRS=%d bGotGeoTransform=%d",
              bGotGeogCS, bGotCfSRS, bGotGeoTransform );

/* -------------------------------------------------------------------- */
/*     Set Projection if we got a geotransform                          */
/* -------------------------------------------------------------------- */
    if ( bGotGeoTransform ) {
        /* Set SRS Units */
        /* TODO: check for other units */
        if ( pszUnits != NULL && ! EQUAL(pszUnits,"") ) {
            if ( EQUAL(pszUnits,"m") ) {
                oSRS.SetLinearUnits( CF_UNITS_M, 1.0 );
                oSRS.SetAuthority( "PROJCS|UNIT", "EPSG", 9001 );
            }
            else if ( EQUAL(pszUnits,"km") ) {
                oSRS.SetLinearUnits( CF_UNITS_M, 1000.0 );
                oSRS.SetAuthority( "PROJCS|UNIT", "EPSG", 9001 );
            }
            else if ( EQUALN(pszUnits,"degrees",7) ) {
                oSRS.SetAngularUnits( CF_UNITS_D, CPLAtof(SRS_UA_DEGREE_CONV) );
                oSRS.SetAuthority( "GEOGCS|UNIT", "EPSG", 9108 );
            }
            // else 
            //     oSRS.SetLinearUnits(pszUnits, 1.0);
        }

        oSRS.exportToWkt( &(poDS->pszProjection) );
        CPLDebug( "GDAL_netCDF", "set WKT from CF [%s]\n", poDS->pszProjection );
        // CPLDebug( "GDAL_netCDF", "set WKT from CF" );
    }
    else if ( bGotGeogCS || bGotCfSRS ) {
        CPLError(CE_Warning, 1,"got SRS but no geotransform from CF!");
    }
/* -------------------------------------------------------------------- */
/*      Process custom GDAL values (spatial_ref, GeoTransform)          */
/* -------------------------------------------------------------------- */
    if( !EQUAL( szGridMappingValue, "" )  ) {
	
        if( pszWKT != NULL ) {
	    
/* -------------------------------------------------------------------- */
/*      Compare CRS obtained from CF attributes and GDAL WKT            */
/*      If possible use the more complete GDAL WKT                      */
/* -------------------------------------------------------------------- */
            // pszProjectionGDAL = CPLStrdup( pszWKT );
            /* Set the CRS to the one written by GDAL */
            if ( ! bGotCfSRS || poDS->pszProjection == NULL || ! bIsGdalCfFile ) {   
                bGotGdalSRS = TRUE;
                CPLFree(poDS->pszProjection);
                poDS->pszProjection = CPLStrdup( pszWKT ); 
                CPLDebug( "GDAL_netCDF", "set WKT from GDAL [%s]\n", poDS->pszProjection );
                // CPLDebug( "GDAL_netCDF", "set WKT from GDAL" );
            }
            else { /* use the SRS from GDAL if it doesn't conflict with the one from CF */
                char *pszProjectionGDAL = (char*) pszWKT ;
                OGRSpatialReference oSRSGDAL;
                oSRSGDAL.importFromWkt( &pszProjectionGDAL );
                /* set datum to unknown or else datums will not match, see bug #4281 */
                if ( oSRSGDAL.GetAttrNode( "DATUM" ) )
                    oSRSGDAL.GetAttrNode( "DATUM" )->GetChild(0)->SetValue( "unknown" );
                if ( oSRS.IsSame(&oSRSGDAL) ) {
                    // printf("ARE SAME, using GDAL WKT\n");
                    bGotGdalSRS = TRUE;
                    CPLFree(poDS->pszProjection);
                    poDS->pszProjection = CPLStrdup( pszWKT );
                    CPLDebug( "GDAL_netCDF", "set WKT from GDAL [%s]\n", poDS->pszProjection );
                    // CPLDebug( "GDAL_netCDF", "set WKT from GDAL" );
                }
                else {
                    CPLDebug( "GDAL_netCDF", 
                              "got WKT from GDAL but not using it because conflicts with CF[%s]\n", 
                              poDS->pszProjection );
                }
            }
/* -------------------------------------------------------------------- */
/*      Look for GeoTransform Array, if not found previously            */
/* -------------------------------------------------------------------- */
            if ( !bGotGeoTransform ) {

                /* TODO read the GT values and detect for conflict with CF */
                /* this could resolve the GT precision loss issue  */

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
                double dfNN=0.0, dfSN=0.0, dfEE=0.0, dfWE=0.0;
                int bGotNN=FALSE, bGotSN=FALSE, bGotEE=FALSE, bGotWE=FALSE;
                // CPLDebug( "GDAL_netCDF", "looking for geotransform corners\n" );
                strcpy(szTemp,szGridMappingValue);
                strcat( szTemp, "#" );
                strcat( szTemp, "Northernmost_Northing");
                pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		
                if( pszValue != NULL ) {
                    dfNN = atof( pszValue );
                    bGotNN = TRUE;
                }
                strcpy(szTemp,szGridMappingValue);
                strcat( szTemp, "#" );
                strcat( szTemp, "Southernmost_Northing");
                pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		
                if( pszValue != NULL ) {
                    dfSN = atof( pszValue );
                    bGotSN = TRUE;
                }
		
                strcpy(szTemp,szGridMappingValue);
                strcat( szTemp, "#" );
                strcat( szTemp, "Easternmost_Easting");
                pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		
                if( pszValue != NULL ) {
                    dfEE = atof( pszValue );
                    bGotEE = TRUE;
                }
		
                strcpy(szTemp,szGridMappingValue);
                strcat( szTemp, "#" );
                strcat( szTemp, "Westernmost_Easting");
                pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
		
                if( pszValue != NULL ) {
                    dfWE = atof( pszValue ); 
                    bGotWE = TRUE;
                }

                /* Only set the GeoTransform if we got all the values */
                if (  bGotNN && bGotSN && bGotEE && bGotWE ) {
                adfGeoTransform[0] = dfWE;
                adfGeoTransform[1] = (dfEE - dfWE) / 
                    ( poDS->GetRasterXSize() - 1 );
                adfGeoTransform[2] = 0.0;
                adfGeoTransform[3] = dfNN;
                adfGeoTransform[4] = 0.0;
                adfGeoTransform[5] = (dfSN - dfNN) / 
                    ( poDS->GetRasterYSize() - 1 );
/* -------------------------------------------------------------------- */
/*     Compute the center of the pixel                                  */
/* -------------------------------------------------------------------- */
                adfGeoTransform[0] = dfWE
                    - (adfGeoTransform[1] / 2);

                adfGeoTransform[3] = dfNN
                    - (adfGeoTransform[5] / 2);


                bGotGeoTransform = TRUE;
                }
            } // (pszGeoTransform != NULL)
            CSLDestroy( papszGeoTransform );

            /* Issue a warning if we did not get a geotransform from GDAL */
            if ( !bGotGeoTransform ) {
                CPLError(CE_Warning, 1,"got SRS but not geotransform from GDAL!");
            }
            } // (!bGotGeoTransform)
        }
    }

    if ( bGotGeoTransform ) {
        CPLDebug( "GDAL_netCDF", "Got GeoTransform:" 
                  "  %.15g, %.15g, %.15g"
                  "  %.15g, %.15g, %.15g", 
                  adfGeoTransform[0], adfGeoTransform[1],
                  adfGeoTransform[2], adfGeoTransform[3],
                  adfGeoTransform[4], adfGeoTransform[5] );
    }
    
/* -------------------------------------------------------------------- */
/*     Search for Well-known GeogCS if got only CF WKT                  */
/*     Disabled for now, as a named datum also include control points   */
/*     (see mailing list and bug#4281                                   */
/*     For example, WGS84 vs. GDA94 (EPSG:3577) - AEA in netcdf_cf.py   */
/* -------------------------------------------------------------------- */
    /* disabled for now, but could be set in a config option */
    bLookForWellKnownGCS = FALSE; 
    if ( bLookForWellKnownGCS && bGotCfSRS && ! bGotGdalSRS ) {
        /* ET - could use a more exhaustive method by scanning all EPSG codes in data/gcs.csv */
        /* as proposed by Even in the gdal-dev mailing list "help for comparing two WKT" */
        /* this code could be contributed to a new function */
        /* OGRSpatialReference * OGRSpatialReference::FindMatchingGeogCS( const OGRSpatialReference *poOther ) */ 
        CPLDebug( "GDAL_netCDF", "Searching for Well-known GeogCS" );
        const char *pszWKGCSList[] = { "WGS84", "WGS72", "NAD27", "NAD83" };
        char *pszWKGCS = NULL;
        oSRS.exportToPrettyWkt( &pszWKGCS );
        for( size_t i=0; i<sizeof(pszWKGCSList)/8; i++ ) {
            pszWKGCS = CPLStrdup( pszWKGCSList[i] );
            OGRSpatialReference oSRSTmp;
            oSRSTmp.SetWellKnownGeogCS( pszWKGCSList[i] );
            /* set datum to unknown, bug #4281 */
            if ( oSRSTmp.GetAttrNode( "DATUM" ) )
                oSRSTmp.GetAttrNode( "DATUM" )->GetChild(0)->SetValue( "unknown" );
            /* could use  OGRSpatialReference::StripCTParms() but let's keep TOWGS84 */
            oSRSTmp.GetRoot()->StripNodes( "AXIS" );
            oSRSTmp.GetRoot()->StripNodes( "AUTHORITY" );
            oSRSTmp.GetRoot()->StripNodes( "EXTENSION" );
            oSRSTmp.exportToPrettyWkt( &pszWKGCS );
            if ( oSRS.IsSameGeogCS(&oSRSTmp) ) {
                oSRS.SetWellKnownGeogCS( pszWKGCSList[i] );
                CPLFree(poDS->pszProjection);
                poDS->pszProjection = NULL;
                oSRS.exportToWkt( &(poDS->pszProjection) );
            }
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
        return GDALPamDataset::GetGeoTransform( padfTransform );;
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
CPLErr netCDFDataset::SafeStrcat(char** ppszDest, char* pszSrc, size_t* nDestSize)
{
    /* Reallocate the data string until the content fits */
    while(*nDestSize < (strlen(*ppszDest) + strlen(pszSrc) + 1)) {
        (*nDestSize) *= 2;
        *ppszDest = (char*) CPLRealloc((void*) *ppszDest, *nDestSize);
    }
    strcat(*ppszDest, pszSrc);
    
    return CE_None;
}

CPLErr netCDFDataset::ReadAttributes( int cdfid, int var)

{
    char    szAttrName[ NC_MAX_NAME ];
    char    szVarName [ NC_MAX_NAME ];
    char    szMetaName[ NC_MAX_NAME * 2 ];
    char    *pszMetaTemp = NULL;
    size_t  nMetaTempSize;
    nc_type nAttrType;
    size_t  nAttrLen, m;
    int     nbAttr;
    char    szTemp[ NCDF_MAX_STR_LEN ];

    nc_inq_varnatts( cdfid, var, &nbAttr );
    if( var == NC_GLOBAL ) {
        strcpy( szVarName,"NC_GLOBAL" );
    }
    else {
        nc_inq_varname( cdfid, var, szVarName );
    }

    for( int l=0; l < nbAttr; l++) {
	
        nc_inq_attname( cdfid, var, l, szAttrName);
        sprintf( szMetaName, "%s#%s", szVarName, szAttrName  );
        nc_inq_att( cdfid, var, szAttrName, &nAttrType, &nAttrLen );
	
        /* Allocate guaranteed minimum size */
        nMetaTempSize = nAttrLen + 1;
        pszMetaTemp = (char *) CPLCalloc( nMetaTempSize, sizeof( char ));
        *pszMetaTemp = '\0';
	
        switch (nAttrType) {
            /* TODO support NC_BYTE */
            case NC_CHAR:
                nc_get_att_text( cdfid, var, szAttrName, pszMetaTemp );
                pszMetaTemp[nAttrLen]='\0';
                break;
            case NC_SHORT:
                short *psTemp;
                psTemp = (short *) CPLCalloc( nAttrLen, sizeof( short ) );
                nc_get_att_short( cdfid, var, szAttrName, psTemp );
                for(m=0; m < nAttrLen-1; m++) {
                    sprintf( szTemp, "%hd, ", psTemp[m] );
                    SafeStrcat(&pszMetaTemp, szTemp, &nMetaTempSize);
                }
                sprintf( szTemp, "%hd", psTemp[m] );
                SafeStrcat(&pszMetaTemp, szTemp, &nMetaTempSize);
                CPLFree(psTemp);
                break;
            case NC_INT:
                int *pnTemp;
                pnTemp = (int *) CPLCalloc( nAttrLen, sizeof( int ) );
                nc_get_att_int( cdfid, var, szAttrName, pnTemp );
                for(m=0; m < nAttrLen-1; m++) {
                    sprintf( szTemp, "%d, ", pnTemp[m] );
                    SafeStrcat(&pszMetaTemp, szTemp, &nMetaTempSize);
                }
        	    sprintf( szTemp, "%d", pnTemp[m] );
        	    SafeStrcat(&pszMetaTemp, szTemp, &nMetaTempSize);
                CPLFree(pnTemp);
                break;
            case NC_FLOAT:
                float *pfTemp;
                pfTemp = (float *) CPLCalloc( nAttrLen, sizeof( float ) );
                nc_get_att_float( cdfid, var, szAttrName, pfTemp );
                for(m=0; m < nAttrLen-1; m++) {
                    sprintf( szTemp, "%.7g, ", pfTemp[m] );
                    SafeStrcat(&pszMetaTemp, szTemp, &nMetaTempSize);
                }
        	    sprintf( szTemp, "%.7g", pfTemp[m] );
        	    SafeStrcat(&pszMetaTemp,szTemp, &nMetaTempSize);
                CPLFree(pfTemp);
                break;
            case NC_DOUBLE:
                double *pdfTemp;
                pdfTemp = (double *) CPLCalloc(nAttrLen, sizeof(double));
                nc_get_att_double( cdfid, var, szAttrName, pdfTemp );
                for(m=0; m < nAttrLen-1; m++) {
                    sprintf( szTemp, "%.15g, ", pdfTemp[m] );
                    SafeStrcat(&pszMetaTemp, szTemp, &nMetaTempSize);
                }
        	    sprintf( szTemp, "%.15g", pdfTemp[m] );
        	    SafeStrcat(&pszMetaTemp, szTemp, &nMetaTempSize);
                CPLFree(pdfTemp);
                break;
            default:
                break;
        }

        papszMetadata = CSLSetNameValue(papszMetadata, 
                                        szMetaName, 
                                        pszMetaTemp);
        CPLFree(pszMetaTemp);
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
                sprintf(szTemp, "%d", (int) nDimLen);
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
#ifdef NETCDF_HAS_NC4
                case NC_UBYTE:
#endif    
                case NC_CHAR:
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
            nc_inq_varname( cdfid, nVar, szName);
            nc_inq_att( cdfid, nVar, CF_STD_NAME, &nAttype, &nAttlen);
            if( nc_get_att_text ( cdfid, nVar, CF_STD_NAME, 
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
                                             poDS->osFilename.c_str(),
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
/*                              IdentifyFormat()                      */
/************************************************************************/

int netCDFDataset::IdentifyFormat( GDALOpenInfo * poOpenInfo, bool bCheckExt = TRUE )

{
/* -------------------------------------------------------------------- */
/*      Does this appear to be a netcdf file? If so, which format?      */
/*      http://www.unidata.ucar.edu/software/netcdf/docs/faq.html#fv1_5 */
/* -------------------------------------------------------------------- */
    // CPLDebug( "GDAL_netCDF", "netCDFDataset::IdentifyFormat() nHeaderBytes=%d, header=[%s]",
    //           poOpenInfo->nHeaderBytes, (char*)poOpenInfo->pabyHeader );
    // #undef HAVE_HDF5
    // #undef HAVE_HDF4

    if( EQUALN(poOpenInfo->pszFilename,"NETCDF:",7) )
        return NCDF_FORMAT_UNKNOWN;
    if ( poOpenInfo->nHeaderBytes < 4 )
        return NCDF_FORMAT_NONE;
    if ( EQUALN((char*)poOpenInfo->pabyHeader,"CDF\001",4) )
        return NCDF_FORMAT_NC;
    else if ( EQUALN((char*)poOpenInfo->pabyHeader,"CDF\002",4) )
        return NCDF_FORMAT_NC2;
    else if ( EQUALN((char*)poOpenInfo->pabyHeader,"\211HDF\r\n\032\n",8) ) {
        // CPLDebug( "GDAL_netCDF", "netCDFDataset::IdentifyFormat() detected HDF5/netcdf file" );
        /* Requires netCDF-4/HDF5 support in libnetcdf (not just libnetcdf-v4).
           If HDF5 is not supported in GDAL, this driver will try to open the file 
           Else, make sure this driver does not try to open HDF5 files 
           If user really wants to open with this driver, use NETCDF:file.h5 format. 
           This check should be relaxed, but there is no clear way to make a difference. 
        */

        /* Check for HDF5 support in GDAL */
#ifdef HAVE_HDF5
        if ( bCheckExt ) { /* Check by default */
            const char* pszExtension = CPLGetExtension( poOpenInfo->pszFilename );
            if ( ! ( EQUAL( pszExtension, "nc")  || EQUAL( pszExtension, "cdf") 
                     || EQUAL( pszExtension, "nc2") || EQUAL( pszExtension, "nc4") ) )
                return NCDF_FORMAT_HDF5;
        }
#endif

        /* Check for netcdf-4 support in libnetcdf */
#ifdef NETCDF_HAS_NC4
        return NCDF_FORMAT_NC4;
#else
        return NCDF_FORMAT_HDF5;
#endif

    }
    else if ( EQUALN((char*)poOpenInfo->pabyHeader,"\016\003\023\001",4) ) {
        // CPLDebug( "GDAL_netCDF", "netCDFDataset::IdentifyFormat() detected HDF4 file" );
        /* Requires HDF4 support in libnetcdf, but if HF4 is supported by GDAL don't try to open. */
        /* If user really wants to open with this driver, use NETCDF:file.hdf syntax. */

        /* Check for HDF4 support in GDAL */
#ifdef HAVE_HDF4
        if ( bCheckExt ) { /* Check by default */
            /* Always treat as HDF4 file */
            return NCDF_FORMAT_HDF4;
        }
#endif

        /* Check for HDF4 support in libnetcdf */
#ifdef NETCDF_HAS_HDF4
        return NCDF_FORMAT_NC4; 
#else
        return NCDF_FORMAT_HDF4;
#endif
    }

    // CPLDebug( "GDAL_netCDF", "netCDFDataset::IdentifyFormat() did not detect a netcdf file" );

    return NCDF_FORMAT_NONE;
} 

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int netCDFDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( EQUALN(poOpenInfo->pszFilename,"NETCDF:",7) ) {
        return TRUE;
    }
    int nTmpFormat = IdentifyFormat( poOpenInfo );
    CPLDebug( "GDAL_netCDF", "netCDFDataset::Identify(), detected format %d", nTmpFormat );
    if( NCDF_FORMAT_NC  == nTmpFormat ||
        NCDF_FORMAT_NC2  == nTmpFormat ||
        NCDF_FORMAT_NC4  == nTmpFormat ||
        NCDF_FORMAT_NC4C  == nTmpFormat )
        return TRUE;
    else
        return FALSE;
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
    int          nCount=0;
    int          nVarID=-1;
    int          nTmpFormat=NCDF_FORMAT_NONE;

/* -------------------------------------------------------------------- */
/*      Does this appear to be a netcdf file?                           */
/* -------------------------------------------------------------------- */
    if( ! EQUALN(poOpenInfo->pszFilename,"NETCDF:",7) ) {
        nTmpFormat = IdentifyFormat( poOpenInfo );
        CPLDebug( "GDAL_netCDF", "netCDFDataset::Open(), detected format %d", nTmpFormat );
        /* Note: not calling Identify() directly, because we want the file type */
        /* Only support NCDF_FORMAT* formats */
        if( ! ( NCDF_FORMAT_NC  == nTmpFormat ||
                NCDF_FORMAT_NC2  == nTmpFormat ||
                NCDF_FORMAT_NC4  == nTmpFormat ||
                NCDF_FORMAT_NC4C  == nTmpFormat ) )
            return NULL;
    }

    netCDFDataset 	*poDS;
    poDS = new netCDFDataset();

/* -------------------------------------------------------------------- */
/*      Disable PAM, at least temporarily. See bug #4244                */
/* -------------------------------------------------------------------- */
    poDS->nPamFlags |= GPF_DISABLED;


    poDS->SetDescription( poOpenInfo->pszFilename );
    
/* -------------------------------------------------------------------- */
/*       Check if filename start with NETCDF: tag                       */
/* -------------------------------------------------------------------- */
    if( EQUALN( poOpenInfo->pszFilename,"NETCDF:",7) )
    {
        char **papszName =
            CSLTokenizeString2( poOpenInfo->pszFilename,
                                ":", CSLT_HONOURSTRINGS|CSLT_PRESERVEESCAPES );
        
        /* -------------------------------------------------------------------- */
        /*    Check for drive name in windows NETCDF:"D:\...                    */
        /* -------------------------------------------------------------------- */
        if ( CSLCount(papszName) == 4 &&
             strlen(papszName[1]) == 1 &&
             (papszName[2][0] == '/' || papszName[2][0] == '\\') )
        {
            poDS->osFilename = papszName[1];
            poDS->osFilename += ':';
            poDS->osFilename += papszName[2];
            poDS->osSubdatasetName = papszName[3];
            poDS->bTreatAsSubdataset = TRUE;
            CSLDestroy( papszName );
        }
        else if( CSLCount(papszName) == 3 )
        {
            poDS->osFilename = papszName[1];
            poDS->osSubdatasetName = papszName[2];
            poDS->bTreatAsSubdataset = TRUE;
            CSLDestroy( papszName );
    	}
        else if( CSLCount(papszName) == 2 )
        {
            poDS->osFilename = papszName[1];
            poDS->osSubdatasetName = "";
            poDS->bTreatAsSubdataset = FALSE;
            CSLDestroy( papszName );
    	}
        else
        {
            CSLDestroy( papszName );
            delete poDS;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to parse NETCDF: prefix string into expected 2, 3 or 4 fields." );
            return NULL;
        }
        /* Identify Format from real file, with bCheckExt=FALSE */ 
        GDALOpenInfo* poOpenInfo2 = new GDALOpenInfo(poDS->osFilename.c_str(), GA_ReadOnly );
        poDS->nFormat = IdentifyFormat( poOpenInfo2, FALSE );
        delete poOpenInfo2;
        if( NCDF_FORMAT_NONE == poDS->nFormat ||
            NCDF_FORMAT_UNKNOWN == poDS->nFormat ) {
            delete poDS;
            return NULL;
        }        
    }
    else 
    {
        poDS->osFilename = poOpenInfo->pszFilename;
        poDS->bTreatAsSubdataset = FALSE;
        poDS->nFormat = nTmpFormat;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    CPLDebug( "GDAL_netCDF", "\n=====\ncalling nc_open( %s )\n", poDS->osFilename.c_str() );
    if( nc_open( poDS->osFilename, NC_NOWRITE, &cdfid ) != NC_NOERR ) {
        delete poDS;
        return NULL;
    }


/* -------------------------------------------------------------------- */
/*      Is this a real netCDF file?                                     */
/* -------------------------------------------------------------------- */
    status = nc_inq(cdfid, &ndims, &nvars, &ngatts, &unlimdimid);
    if( status != NC_NOERR ) {
        delete poDS;
        return NULL;
    }   

/* -------------------------------------------------------------------- */
/*      Get file type from netcdf                                       */
/* -------------------------------------------------------------------- */
    status = nc_inq_format (cdfid, &nTmpFormat);
    if ( status != NC_NOERR ) {
        NCDF_ERR(status);
    }
    else {
        CPLDebug( "GDAL_netCDF", 
                  "driver detected file type=%d, libnetcdf detected type=%d",
                  poDS->nFormat, nTmpFormat );
        if ( nTmpFormat != poDS->nFormat ) {
            /* warn if file detection conflicts with that from libnetcdf */
            /* except for NC4C, which we have no way of detecting initially */
            if ( nTmpFormat != NCDF_FORMAT_NC4C ) {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "NetCDF driver detected file type=%d, but libnetcdf detected type=%d",
                          poDS->nFormat, nTmpFormat );
            }
            CPLDebug( "GDAL_netCDF", "seting file type to %d, was %d", 
                      nTmpFormat, poDS->nFormat );
            poDS->nFormat = nTmpFormat;
        }
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The NETCDF driver does not support update access to existing"
                  " datasets.\n" );
        nc_close( cdfid );
        delete poDS;
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Does the request variable exist?                                */
/* -------------------------------------------------------------------- */
    if( poDS->bTreatAsSubdataset )
    {
        status = nc_inq_varid( cdfid, poDS->osSubdatasetName, &var);
        if( status != NC_NOERR ) {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "%s is a netCDF file, but %s is not a variable.",
                      poOpenInfo->pszFilename, 
                      poDS->osSubdatasetName.c_str() );
            
            nc_close( cdfid );
            delete poDS;
            return NULL;
        }
    }

    if( nc_inq_ndims( cdfid, &dim_count ) != NC_NOERR || dim_count < 2 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "%s is a netCDF file, but not in GMT configuration.",
                  poOpenInfo->pszFilename );

        nc_close( cdfid );
        delete poDS;
        return NULL;
    }

    CPLDebug( "GDAL_netCDF", "dim_count = %d", dim_count );

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
    {
        delete poDS;
        return NULL;
    }    
    
    CPLDebug( "GDAL_netCDF", "var_count = %d", var_count );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/*      Create Netcdf Subdataset if filename as NETCDF tag              */
/* -------------------------------------------------------------------- */
    poDS->cdfid = cdfid;

    poDS->ReadAttributes( cdfid, NC_GLOBAL );	

/* -------------------------------------------------------------------- */
/*  Verify if only one variable has 2 dimensions                        */
/* -------------------------------------------------------------------- */
    for ( j = 0; j < nvars; j++ ) {

        nc_inq_varndims ( cdfid, j, &ndims );
        if( ndims >= 2 ) {
            nVarID=j;
            nCount++;
        }
    }

/* -------------------------------------------------------------------- */
/*      We have more than one variable with 2 dimensions in the         */
/*      file, then treat this as a subdataset container dataset.        */
/* -------------------------------------------------------------------- */
    if( (nCount > 1) && !poDS->bTreatAsSubdataset )
    {
        poDS->CreateSubDatasetList();
        poDS->SetMetadata( poDS->papszMetadata );
        poDS->TryLoadXML();
        return( poDS );
    }

/* -------------------------------------------------------------------- */
/*      If we are not treating things as a subdataset, then capture     */
/*      the name of the single available variable as the subdataset.    */
/* -------------------------------------------------------------------- */
    if( !poDS->bTreatAsSubdataset ) // nCount must be 1!
    {
        char szVarName[NC_MAX_NAME];

        nc_inq_varname( cdfid, nVarID, szVarName);
        poDS->osSubdatasetName = szVarName;
    }

/* -------------------------------------------------------------------- */
/*      Open the NETCDF subdataset NETCDF:"filename":subdataset         */
/* -------------------------------------------------------------------- */
    var=-1;
    nc_inq_varid( cdfid, poDS->osSubdatasetName, &var);
    nd = 0;
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
        delete poDS;
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

    poDS->SetProjectionFromVar( var );
    poDS->SetMetadata( poDS->papszMetadata );

/* -------------------------------------------------------------------- */
/*      Create bands                                                    */
/* -------------------------------------------------------------------- */
    poDS->panBandZLev = (int *)CPLCalloc( nd-2, sizeof( int ) );
    
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
        char ** papszToken;
        papszToken=NULL;

        netCDFRasterBand *poBand =
            new netCDFRasterBand(poDS, var, nDim, lev,
                                 poDS->panBandZLev, poDS->panBandDimPos, i+1 );

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
    if( poDS->bTreatAsSubdataset )
    {
        poDS->SetPhysicalFilename( poDS->osFilename );
        poDS->SetSubdatasetName( poDS->osSubdatasetName );
    }
    
    poDS->TryLoadXML();

    if( poDS->bTreatAsSubdataset )
        poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );
    else
        poDS->oOvManager.Initialize( poDS, poDS->osFilename );

    return( poDS );
}


/************************************************************************/
/*                            CopyMetadata()                            */
/*                                                                      */
/*      Create a copy of metadata for NC_GLOBAL or a variable           */
/************************************************************************/

void CopyMetadata( void  *poDS, int fpImage, int CDFVarID ) {

    char       **papszMetadata;
    char       **papszFieldData;
    const char *pszField;
    char       szMetaName[ NCDF_MAX_STR_LEN ];
    char       szMetaValue[ NCDF_MAX_STR_LEN ];
    char       szTemp[ NCDF_MAX_STR_LEN ];
    int        nItems;
    int        bCopyItem;

    /* These values for the detection of data type */
    nc_type    nMetaType;
    int        nMetaValue;
    float      fMetaValue;
    double     dfMetaValue;
    char       *pszTemp;

    if( CDFVarID == NC_GLOBAL ) {
        papszMetadata = GDALGetMetadata( (GDALDataset *) poDS,"");
    } else {
        papszMetadata = GDALGetMetadata( (GDALRasterBandH) poDS, NULL );
    }

    nItems = CSLCount( papszMetadata );             
    
    for(int k=0; k < nItems; k++ ) {
        bCopyItem = TRUE;
        pszField = CSLGetField( papszMetadata, k );
        papszFieldData = CSLTokenizeString2 (pszField, "=", 
                                             CSLT_HONOURSTRINGS );
        if( papszFieldData[1] != NULL ) {
            strcpy( szMetaName,  papszFieldData[ 0 ] );
            strcpy( szMetaValue, papszFieldData[ 1 ] );

            /* Fix various issues with metadata translation */ 
            if( CDFVarID == NC_GLOBAL ) {
                /* Remove NC_GLOBAL prefix for netcdf global Metadata */ 
                if( strncmp( szMetaName, "NC_GLOBAL#", 10 ) == 0 ) {
                    strcpy( szTemp, szMetaName+10 );
                    strcpy( szMetaName, szTemp );
                } 
                /* GDAL Metadata renamed as GDAL-[meta] */
                else if ( strstr( szMetaName, "#" ) == NULL ) {
                    strcpy( szTemp, "GDAL_" );
                    strcat( szTemp, szMetaName );
                    strcpy( szMetaName, szTemp );
                }
                /* Keep time, lev and depth information for safe-keeping */
                /* Time and vertical coordinate handling need improvements */
                else if( strncmp( szMetaName, "time#", 5 ) == 0 ) {
                    szMetaName[4] = '-';
                }
                else if( strncmp( szMetaName, "lev#", 4 ) == 0 ) {
                    szMetaName[3] = '-';
                }
                else if( strncmp( szMetaName, "depth#", 6 ) == 0 ) {
                    szMetaName[5] = '-';
                }
                /* Only copy data without # (previously all data was copied)  */
                if ( strstr( szMetaName, "#" ) != NULL ) {   
                    bCopyItem = FALSE;
                }
                /* netCDF attributes do not like the '#' character. */
                // for( unsigned int h=0; h < strlen( szMetaName ) -1 ; h++ ) {
                //     if( szMetaName[h] == '#' ) szMetaName[h] = '-'; 
                // }
            }
            else {
                /* for variables, don't copy varname */
                if ( strncmp( szMetaName, "NETCDF_VARNAME", 14) == 0 ) 
                    bCopyItem = FALSE;
                /* Remove add_offset and scale_factor, but set them later from band data */
                else if ( strcmp( szMetaName, CF_ADD_OFFSET ) == 0 ) 
                    bCopyItem = FALSE;
                else if ( strcmp( szMetaName, CF_SCALE_FACTOR ) == 0 ) 
                    bCopyItem = FALSE;
            }

            if ( bCopyItem ) {

                /* By default write NC_CHAR, but detect for int/float/double */
                nMetaType = NC_CHAR;
                nMetaValue = 0;
                fMetaValue = 0.0f;
                dfMetaValue = 0.0;

                errno = 0;
                nMetaValue = strtol( szMetaValue, &pszTemp, 10 );
                if ( (errno == 0) && (szMetaValue != pszTemp) && (*pszTemp == 0) ) {
                    nMetaType = NC_INT;
                }
                else {
                    errno = 0;
                    dfMetaValue = strtod( szMetaValue, &pszTemp );
                    if ( (errno == 0) && (szMetaValue != pszTemp) && (*pszTemp == 0) ) {
                        /* test for float instead of double */
                        /* strtof() is C89, which is not available in MSVC */
                        /* see if we loose precision if we cast to float and write to char* */
                        fMetaValue = (float)dfMetaValue; 
                        sprintf( szTemp,"%.7g",fMetaValue); 
                        if ( EQUAL(szTemp, szMetaValue ) )
                            nMetaType = NC_FLOAT;
                        else
                            nMetaType = NC_DOUBLE;                   
                    }
                }

                /* now write the data */
                switch( nMetaType ) {
                    case  NC_INT:
                        nc_put_att_int( fpImage, CDFVarID, szMetaName,
                                        NC_INT, 1, &nMetaValue );
                        break;
                    case  NC_FLOAT:
                        nc_put_att_float( fpImage, CDFVarID, szMetaName,
                                          NC_FLOAT, 1, &fMetaValue );
                        break;
                    case  NC_DOUBLE:
                        nc_put_att_double( fpImage, CDFVarID,  szMetaName,
                                           NC_DOUBLE, 1, &dfMetaValue );
                        break;
                    default:
                        nc_put_att_text( fpImage, CDFVarID, szMetaName,
                                         strlen( szMetaValue ),
                                         szMetaValue );          
                        break;
                }
            }
            
        }
        CSLDestroy( papszFieldData );
    }

    /* Set add_offset and scale_factor here if present */
    if( CDFVarID != NC_GLOBAL ) {

        int bGotAddOffset, bGotScale;
        double dfAddOffset = GDALGetRasterOffset( (GDALRasterBandH) poDS, &bGotAddOffset );
        double dfScale = GDALGetRasterScale( (GDALRasterBandH) poDS, &bGotScale );

        if ( bGotAddOffset && dfAddOffset != 0.0 && bGotScale && dfScale != 1.0 ) {
            nc_put_att_double( fpImage, CDFVarID, CF_ADD_OFFSET,
                               NC_DOUBLE, 1, &dfAddOffset );
            nc_put_att_double( fpImage, CDFVarID, CF_SCALE_FACTOR,
                               NC_DOUBLE, 1, &dfScale );
        }

    }

}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

/*
Driver options:

FORMAT=NC/NC2/NC4/NC4C (COMPRESS=DEFLATE sets FORMAT=NC4C)
COMPRESS=NONE/DEFLATE/PACKED (default: NONE)
ZLEVEL=[1-9] (default: 6)
WRITE_BOTTOMUP=YES/NO (default: NO)
WRITE_GDAL_TAGS=YES/NO (default: YES)
WRITE_LONLAT=YES/NO/IF_NEEDED (default: YES for geographic, NO for projected)
TYPE_LONLAT=float/double (default: double for geographic, float for projected)

Config Options:

GDAL_NETCDF_BOTTOMUP=YES/NO (default:YES) -> this sets the default of WRITE_BOTTOMUP

Processing steps:

1 error checking
2 create dataset
3 def dims and write metadata
4 write projection info
5 write variables: data, projection var, metadata
6 close dataset
7 write pam 
*/

static GDALDataset*
NCDFCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                int bStrict, char ** papszOptions, 
                GDALProgressFunc pfnProgress, void * pProgressData )

{
    CPLErr eErr = CE_None;
   
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    int  nLonSize=0, nLatSize=0; 
    int  iBand;
    GDALDataType eDT;

    int  anBandDims[ NC_MAX_DIMS ];
    // int  anBandMap[  NC_MAX_DIMS ];

    int  bBottomUp = FALSE;
    int  bWriteGridMapping = FALSE;
    int  bWriteLonLat = FALSE;
    int  bWriteGdalTags = FALSE;
    char pszNetcdfProjection[ NC_MAX_NAME ];

    const char *pszValue;
    char   szTemp[ NCDF_MAX_STR_LEN ];

    /* Variables needed for projection */
    int NCDFVarID=0;
 
    double adfGeoTransform[6];
    char   szGeoTransform[ NCDF_MAX_STR_LEN ];
    int bSourceHasGeoTransform = TRUE;


    const char *pszLonDimName = NCDF_DIMNAME_LON;
    const char *pszLatDimName = NCDF_DIMNAME_LAT;
    nc_type eLonLatType = NC_DOUBLE;
    double *padLonVal = NULL;
    double *padLatVal = NULL; /* should use float for projected, save space */
    double dfX0=0.0, dfDX=0.0, dfY0=0.0, dfDY=0.0;
    double dfTemp=0.0;
    size_t *startLon = NULL;
    size_t *countLon = NULL;
    size_t *startLat = NULL;
    size_t *countLat = NULL;

    OGRSpatialReference oSRS;
    char *pszWKT = NULL;

    int nFormat = NCDF_FORMAT_UNKNOWN;
    int nCompress = NCDF_COMPRESS_NONE;
    int nZLevel = NCDF_DEFLATE_LEVEL;
    int nCreateMode = NC_CLOBBER;

    CPLDebug( "GDAL_netCDF", "\n=====\nNCDFCreateCopy( %s, ... )\n", pszFilename );
 
/* -------------------------------------------------------------------- */
/*      Check input bands for errors                                    */
/* -------------------------------------------------------------------- */

    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "NetCDF driver does not support source dataset with zero band.\n");
        return NULL;
    }

    for( iBand=1; iBand <= nBands; iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand );
        GDALDataType eDT = poSrcBand->GetRasterDataType();
        if (eDT == GDT_Unknown || GDALDataTypeIsComplex(eDT))
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "NetCDF driver does not support source dataset with band of complex type.");
            return NULL;
        }
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get Projection ref and GeoTransform from source                 */
/* -------------------------------------------------------------------- */

    pszWKT = (char *) poSrcDS->GetProjectionRef();
    if( pszWKT != NULL ) {
        CPLDebug( "GDAL_netCDF", "WKT = %s", pszWKT );
        oSRS.importFromWkt( &pszWKT );
    // char *pszProj4Defn = NULL;
    // oSRS.exportToProj4( &pszProj4Defn );

    }
    eErr = poSrcDS->GetGeoTransform( adfGeoTransform );
    *szGeoTransform = '\0';
    for( int i=0; i<6; i++ ) {
        sprintf( szTemp, "%.18g ",
                 adfGeoTransform[i] );
        // sprintf( szTemp, "%.15g ",
        //          adfGeoTransform[i] );
        strcat( szGeoTransform, szTemp );
    }
    CPLDebug( "GDAL_netCDF", "szGeoTranform = %s", szGeoTransform );
    
    if ( eErr != CE_None ) {
        CPLDebug( "GDAL_netCDF", 
                  "did not get a GeoTransform, will not write lon/lat values." );
        // bWriteLonLat = FALSE;
        bSourceHasGeoTransform = FALSE;
    }


/* -------------------------------------------------------------------- */
/*      Process options.                                                */
/* -------------------------------------------------------------------- */
    /* File format */
    nFormat = NCDF_FORMAT_NC;
    pszValue = CSLFetchNameValue( papszOptions, "FORMAT" );
    if ( pszValue != NULL ) {
        if ( EQUAL( pszValue, "NC" ) ) {
            nFormat = NCDF_FORMAT_NC;
        }
#ifdef NETCDF_HAS_NC2
        else if ( EQUAL( pszValue, "NC2" ) ) {
            nFormat = NCDF_FORMAT_NC2;
        }
#endif
#ifdef NETCDF_HAS_NC4
        else if ( EQUAL( pszValue, "NC4" ) ) {
            nFormat = NCDF_FORMAT_NC4;
        }    
        else if ( EQUAL( pszValue, "NC4C" ) ) {
            nFormat = NCDF_FORMAT_NC4C;
        }    
#endif
        else {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "FORMAT=%s in not supported, using the default NC format.", pszValue );        
        }
    }

    /* compression only available for NC4 */
#ifdef NETCDF_HAS_NC4

    /* COMPRESS option */
    pszValue = CSLFetchNameValue( papszOptions, "COMPRESS" );
    if ( pszValue != NULL ) {
        if ( EQUAL( pszValue, "NONE" ) ) {
            nCompress = NCDF_COMPRESS_NONE;
        }       
        else if ( EQUAL( pszValue, "DEFLATE" ) ) {
            nCompress = NCDF_COMPRESS_DEFLATE;
            if ( !((nFormat == NCDF_FORMAT_NC4) || (nFormat == NCDF_FORMAT_NC4C)) ) {
                CPLError( CE_Warning, CPLE_IllegalArg,
                          "NOTICE: Format set to NC4C because compression is set to DEFLATE." );
                nFormat = NCDF_FORMAT_NC4C;
            }
        }
        else {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "COMPRESS=%s is not supported.", pszValue );
        }
    }

    /* ZLEVEL option */
    pszValue = CSLFetchNameValue( papszOptions, "ZLEVEL" );
    if( pszValue  != NULL )
    {
        nZLevel =  atoi( pszValue );
        if (!(nZLevel >= 1 && nZLevel <= 9))
        {
            CPLError( CE_Warning, CPLE_IllegalArg, 
                    "ZLEVEL=%s value not recognised, ignoring.",
                    pszValue );
            nZLevel = NCDF_DEFLATE_LEVEL;
        }
    }

#endif

    /* code for compression support - avoid code duplication */
    /* with refactoring this could be a function */
#ifdef NETCDF_HAS_NC4
// must set chunk size to avoid huge performace hit                
// perhaps another solution it to change the chunk cache?
// http://www.unidata.ucar.edu/software/netcdf/docs/netcdf.html#Chunk-Cache
#define NCDF_DEF_VAR_DEFLATE \
           if ( nCompress == NCDF_COMPRESS_DEFLATE ) { \
                status = nc_def_var_deflate(fpImage,NCDFVarID,1,1,nZLevel); \
                NCDF_ERR(status) \
                size_t chunksize[] = { 1, nXSize }; \
                status = nc_def_var_chunking( fpImage, NCDFVarID, \
                                              NC_CHUNKED, chunksize ); \
                NCDF_ERR(status) \
            } 
#define NCDF_DEF_VAR_DEFLATE_NOCHUNK \
           if ( nCompress == NCDF_COMPRESS_DEFLATE ) { \
                status = nc_def_var_deflate(fpImage,NCDFVarID,1,1,nZLevel); \
                NCDF_ERR(status) \
            } 
#else
#define NCDF_DEF_VAR_DEFLATE
#define NCDF_DEF_VAR_DEFLATE_NOCHUNK
#endif


    CPLDebug( "GDAL_netCDF", 
              "file options: format=%d compress=%d zlevel=%d\n",
              nFormat, nCompress, nZLevel );

    /* netcdf standard is bottom-up */
    /* overriden by config option GDAL_NETCDF_BOTTOMUP and -co option WRITE_BOTTOMUP */
    // bBottomUp = CSLTestBoolean( CPLGetConfigOption( "GDAL_NETCDF_BOTTOMUP", "YES" ) );
    bBottomUp = CSLTestBoolean( CPLGetConfigOption( "GDAL_NETCDF_BOTTOMUP", "NO" ) );
    bBottomUp = CSLFetchBoolean( papszOptions, "WRITE_BOTTOMUP", bBottomUp );       

    /* TODO could add a config option GDAL_NETCDF_PREF=GDAL/CF  */

    if( oSRS.IsProjected() ) 
    {
        int bIsCfProjection = NCDFIsCfProjection( oSRS.GetAttrValue( "PROJECTION" ) );
        bWriteGridMapping = TRUE;
        bWriteGdalTags = CSLFetchBoolean( papszOptions, "WRITE_GDAL_TAGS", TRUE );
        /* force WRITE_GDAL_TAGS if is not a CF projection */
        if ( ! bWriteGdalTags && ! bIsCfProjection )
            bWriteGdalTags = TRUE;
                
        pszValue = CSLFetchNameValueDef(papszOptions,"WRITE_LONLAT", "NO");
        if ( EQUAL( pszValue, "IF_NEEDED" ) ) {
            if  ( bIsCfProjection )
                bWriteLonLat = FALSE;
            else 
                bWriteLonLat = TRUE;
        }
        else bWriteLonLat = CSLTestBoolean( pszValue );
        if ( bWriteLonLat == TRUE ) {
            nLonSize = nXSize * nYSize;
            nLatSize = nXSize * nYSize;
        }
        eLonLatType = NC_FLOAT;
        pszValue =  CSLFetchNameValueDef(papszOptions,"TYPE_LONLAT", "FLOAT");
        if ( EQUAL(pszValue, "DOUBLE" ) ) 
            eLonLatType = NC_DOUBLE;
    }
    else 
    { 
        /* files without a Datum will not have a grid_mapping variable and geographic information */
        if ( oSRS.IsGeographic() )  bWriteGridMapping = TRUE;
        else  bWriteGridMapping = FALSE;
        bWriteGdalTags = CSLFetchBoolean( papszOptions, "WRITE_GDAL_TAGS", bWriteGridMapping );

        pszValue =  CSLFetchNameValueDef(papszOptions,"WRITE_LONLAT", "YES");
        if ( EQUAL( pszValue, "IF_NEEDED" ) )  
            bWriteLonLat = TRUE;
        else bWriteLonLat = CSLTestBoolean( pszValue );
        /*  Don't write lon/lat if no source geotransform */
        if ( ! bSourceHasGeoTransform )
            bWriteLonLat = FALSE;
        /* If we don't write lon/lat, set dimnames to X/Y and write gdal tags*/
        if ( ! bWriteLonLat ) {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "creating geographic file without lon/lat values!");
            if ( bSourceHasGeoTransform ) 
                bWriteGdalTags = TRUE; //not desireable if no geotransform
            pszLonDimName = NCDF_DIMNAME_X;
            pszLatDimName = NCDF_DIMNAME_Y;
            // bBottomUp = FALSE; 
        }
        nLonSize = nXSize;
        nLatSize = nYSize;     
 
        eLonLatType = NC_DOUBLE;
        pszValue =  CSLFetchNameValueDef(papszOptions,"TYPE_LONLAT", "DOUBLE");
        if ( EQUAL(pszValue, "FLOAT" ) ) 
            eLonLatType = NC_FLOAT;
    }
    
    /* make sure we write grid_mapping if we need to write GDAL tags */
    if ( bWriteGdalTags ) bWriteGridMapping = TRUE;

    CPLDebug( "GDAL_netCDF", 
              "bWriteGridMapping=%d bWriteGdalTags=%d bWriteLonLat=%d bBottomUp=%d",
              bWriteGridMapping,bWriteGdalTags,bWriteLonLat,bBottomUp );

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */

    int fpImage;
    int status;
    int nXDimID = 0;
    int nYDimID = 0;
    int nLonDimID = 0;
    int nLatDimID = 0;
    
    // status = nc_create( pszFilename, NC_CLOBBER,  &fpImage );
    switch ( nFormat ) {        
#ifdef NETCDF_HAS_NC2
        case NCDF_FORMAT_NC2:
            nCreateMode = NC_CLOBBER|NC_64BIT_OFFSET;
            break;
#endif
#ifdef NETCDF_HAS_NC4
        case NCDF_FORMAT_NC4:
            nCreateMode = NC_CLOBBER|NC_NETCDF4;
            break;
        case NCDF_FORMAT_NC4C:
            nCreateMode = NC_CLOBBER|NC_NETCDF4|NC_CLASSIC_MODEL;
            break;
#endif
        case NCDF_FORMAT_NC:
        default:
            nCreateMode = NC_CLOBBER;
            break;
    }

    status = nc_create( pszFilename, nCreateMode,  &fpImage );

    if( status != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create netCDF file %s (Error code %d): %s .\n", 
                  pszFilename, status, nc_strerror(status) );
        return NULL;
    }

    if( oSRS.IsProjected() ) 
    {
        status = nc_def_dim( fpImage, NCDF_DIMNAME_X, nXSize, &nXDimID );
        CPLDebug( "GDAL_netCDF", "status nc_def_dim %s = %d", NCDF_DIMNAME_X, status );   
        status = nc_def_dim( fpImage, NCDF_DIMNAME_Y, nYSize, &nYDimID );
        CPLDebug( "GDAL_netCDF", "status nc_def_dim %s = %d", NCDF_DIMNAME_Y, status );
        anBandDims[0] = nYDimID;
        anBandDims[1] = nXDimID;
        CPLDebug( "GDAL_netCDF", "nYDimID = %d", nXDimID );
        CPLDebug( "GDAL_netCDF", "nXDimID = %d", nYDimID );
    }
    else 
    { 
        status = nc_def_dim( fpImage, pszLonDimName, nLonSize, &nLonDimID );
        CPLDebug( "GDAL_netCDF", "status nc_def_dim %s = %d", pszLonDimName, status );   
        status = nc_def_dim( fpImage, pszLatDimName, nLatSize, &nLatDimID );
        CPLDebug( "GDAL_netCDF", "status nc_def_dim %s = %d", pszLatDimName, status );   
        anBandDims[0] = nLatDimID;
        anBandDims[1] = nLonDimID;       
        CPLDebug( "GDAL_netCDF", "nLonDimID = %d", nLonDimID );
        CPLDebug( "GDAL_netCDF", "nLatDimID = %d", nLatDimID );
    }
   
/* -------------------------------------------------------------------- */
/*      Copy global metadata                                            */
/*      Add Conventions, GDAL info and history                          */
/* -------------------------------------------------------------------- */
    CopyMetadata((void *) poSrcDS, fpImage, NC_GLOBAL );
    NCDFAddGDALHistory( fpImage, pszFilename,
                        poSrcDS->GetMetadataItem("NC_GLOBAL#history","") );
    
/* -------------------------------------------------------------------- */
/*      Get projection values                                           */
/* -------------------------------------------------------------------- */

    if( oSRS.IsProjected() )
    {
        const OGR_SRSNode *poPROJCS = oSRS.GetAttrNode( "PROJCS" );
        const char  *pszProjection;
        OGRSpatialReference *poLatLonCRS = NULL;
        OGRCoordinateTransformation *poTransform = NULL;

        double *padYVal = NULL;
        double *padXVal = NULL;
        size_t startX[1];
        size_t countX[1];
        size_t startY[1];
        size_t countY[1];

        pszProjection = oSRS.GetAttrValue( "PROJECTION" );

/* -------------------------------------------------------------------- */
/*      Write projection attributes                                     */
/* -------------------------------------------------------------------- */

        /* Basic Projection info (grid_mapping and datum) */
        for( int i=0; poNetcdfSRS_PT[i].WKT_SRS != NULL; i++ ) {
            if( EQUAL( poNetcdfSRS_PT[i].WKT_SRS, pszProjection ) ) {
                CPLDebug( "GDAL_netCDF", "GDAL PROJECTION = %s , NCDF PROJECTION = %s", 
                          poNetcdfSRS_PT[i].WKT_SRS, 
                          poNetcdfSRS_PT[i].CF_SRS);
                strcpy( pszNetcdfProjection, poNetcdfSRS_PT[i].CF_SRS );
                status = nc_def_var( fpImage, 
                                     poNetcdfSRS_PT[i].CF_SRS,
                                     NC_CHAR, 
                                     0, NULL, &NCDFVarID );
                break;
            }
        }
        nc_put_att_text( fpImage, NCDFVarID, CF_GRD_MAPPING_NAME,
                         strlen( pszNetcdfProjection ),
                         pszNetcdfProjection );

        /* write DATUM information */
        dfTemp = oSRS.GetPrimeMeridian();
        nc_put_att_double( fpImage, NCDFVarID, CF_PP_LONG_PRIME_MERIDIAN,
                           NC_DOUBLE, 1, &dfTemp );
        dfTemp = oSRS.GetSemiMajor();
        nc_put_att_double( fpImage, NCDFVarID, CF_PP_SEMI_MAJOR_AXIS,
                           NC_DOUBLE, 1, &dfTemp );
        dfTemp = oSRS.GetInvFlattening();
        nc_put_att_double( fpImage, NCDFVarID, CF_PP_INVERSE_FLATTENING,
                           NC_DOUBLE, 1, &dfTemp );


        /* Various projection attributes */
        // PDS: keep in synch with SetProjection function
        NCDFWriteProjAttribs(poPROJCS, pszProjection, fpImage, NCDFVarID);
        
        /*  Optional GDAL custom projection tags */
        if ( bWriteGdalTags ) {
            // if ( strlen(pszProj4Defn) > 0 ) {
            //     nc_put_att_text( fpImage, NCDFVarID, "proj4",
            //                      strlen( pszProj4Defn ), pszProj4Defn );
            // }
            pszWKT = (char *) poSrcDS->GetProjectionRef() ;
            nc_put_att_text( fpImage, NCDFVarID, NCDF_SPATIAL_REF,
                             strlen( pszWKT ), pszWKT );
            /* for now write the geotransform for back-compat */
            /* the old (1.8.1) driver overrides the CF geotransform with */
            /* empty values from dfNN, dfSN, dfEE, dfWE; */
            /* with an option to not write XY we should write the geotransform */           
            nc_put_att_text( fpImage, NCDFVarID, NCDF_GEOTRANSFORM,
                             strlen( szGeoTransform ),
                             szGeoTransform );
        }

/* -------------------------------------------------------------------- */
/*      CF projection X/Y attributes                                    */
/* -------------------------------------------------------------------- */
        CPLDebug("GDAL_netCDF", "Getting (X,Y) values" );

        padXVal = (double *) CPLMalloc( nXSize * sizeof( double ) );
        padYVal = (double *) CPLMalloc( nYSize * sizeof( double ) );

        /* Get OGR transform */
        if ( bWriteLonLat == TRUE ) {
      
            poLatLonCRS = oSRS.CloneGeogCS();
            if ( poLatLonCRS != NULL )
                poTransform = OGRCreateCoordinateTransformation( &oSRS, poLatLonCRS );
            if( poTransform != NULL )
            {
                /* TODO: fix this to transform and write in blocks, to lower memory needed */
                padLatVal = (double *) CPLMalloc( nLatSize * sizeof( double ) );
                padLonVal = (double *) CPLMalloc( nLonSize * sizeof( double ) );
            }
            else /* if no OGR transform, then don't write CF lon/lat */
                bWriteLonLat = FALSE;
        }
/* -------------------------------------------------------------------- */
/*      Get Y values                                                    */
/* -------------------------------------------------------------------- */
        if ( ! bBottomUp )
            dfY0 = adfGeoTransform[3];
        else /* invert latitude values */ 
            dfY0 = adfGeoTransform[3] + ( adfGeoTransform[5] * nYSize );
        dfDY = adfGeoTransform[5];
        
        for( int j=0; j<nYSize; j++ ) {
            /* The data point is centered inside the pixel */
            if ( ! bBottomUp )
                padYVal[j] = dfY0 + (j+0.5)*dfDY ;
            else /* invert latitude values */ 
                padYVal[j] = dfY0 - (j+0.5)*dfDY ;
            
             if ( bWriteLonLat == TRUE ) {
                 for( int i=0; i<nXSize; i++ ) {
                     padLatVal[j*nXSize+i] = padYVal[j];
                 }
             }
        }
        startX[0] = 0;
        countX[0] = nXSize;
        if ( bWriteLonLat == TRUE ) {
            startLat = (size_t *) CPLMalloc( 2 * sizeof( size_t ) );
            countLat = (size_t *) CPLMalloc( 2 * sizeof( size_t ) );
            startLat[0] = 0;
            startLat[1] = 0;
            countLat[0] = nYSize;
            countLat[1] = nXSize;
        }
/* -------------------------------------------------------------------- */
/*      Get X values                                                    */
/* -------------------------------------------------------------------- */
        dfX0 = adfGeoTransform[0];
        dfDX = adfGeoTransform[1];

        for( int i=0; i<nXSize; i++ ) {
            /* The data point is centered inside the pixel */
            padXVal[i] = dfX0 + (i+0.5)*dfDX ;
            if ( bWriteLonLat == TRUE ) {
                for( int j=0; j<nYSize; j++ ) {
                    padLonVal[j*nXSize+i] = padXVal[i];
                    // padLonVal[k] = dfX0 + i*dfDX ;
                }
            }
        }
        startY[0] = 0;
        countY[0] = nYSize;
        if ( bWriteLonLat == TRUE ) {
            startLon = (size_t *) CPLMalloc( 2 * sizeof( size_t ) );
            countLon = (size_t *) CPLMalloc( 2 * sizeof( size_t ) );
            startLon[0] = 0;
            startLon[1] = 0;
            countLon[0] = nYSize;
            countLon[1] = nXSize;
        }
/* -------------------------------------------------------------------- */
/*      Transform (X,Y) values to (lon,lat)                             */
/* -------------------------------------------------------------------- */

        pfnProgress( 0.10, NULL, pProgressData );

        if ( bWriteLonLat == TRUE ) { 
            CPLDebug("GDAL_netCDF", "Transforming (X,Y)->(lon,lat)" );
            if( ! poTransform->Transform( nXSize * nYSize, padLonVal, padLatVal, NULL ) ) {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unable to Transform (X,Y) to (lon,lat).\n" );
            }
        }

        pfnProgress( 0.30, NULL, pProgressData );

        /* Free the srs and transform objects */
        if ( poLatLonCRS != NULL ) CPLFree( poLatLonCRS );
        if ( poTransform != NULL ) CPLFree( poTransform );

/* -------------------------------------------------------------------- */
/*      Write X attributes                                              */
/* -------------------------------------------------------------------- */
        int anXDims[1];
        anXDims[0] = nXDimID;
        status = nc_def_var( fpImage, NCDF_DIMNAME_X, NC_DOUBLE, 
                             1, anXDims, &NCDFVarID );
        nc_put_att_text( fpImage, NCDFVarID, CF_STD_NAME,
                         strlen(CF_PROJ_X_COORD),
                         CF_PROJ_X_COORD );
        nc_put_att_text( fpImage, NCDFVarID, CF_LNG_NAME,
                         strlen("x coordinate of projection"),
                         "x coordinate of projection" );
        /*TODO verify this */
        nc_put_att_text( fpImage, NCDFVarID, CF_UNITS, 1, "m" ); 

/* -------------------------------------------------------------------- */
/*      Write X values                                                  */
/* -------------------------------------------------------------------- */

        /* Temporarily switch to data mode and write data */
        status = nc_enddef( fpImage );
        CPLDebug("GDAL_netCDF", "Writing X values" );
        status = nc_put_vara_double( fpImage, NCDFVarID, startX,
                                     countX, padXVal);
        status = nc_redef( fpImage );
        
        /* free values */
        CPLFree( padXVal );

/* -------------------------------------------------------------------- */
/*      Write Y attributes                                              */
/* -------------------------------------------------------------------- */
        int anYDims[1];
        anYDims[0] = nYDimID;
        status = nc_def_var( fpImage, NCDF_DIMNAME_Y, NC_DOUBLE, 
                             1, anYDims, &NCDFVarID );
        nc_put_att_text( fpImage, NCDFVarID, CF_STD_NAME,
                         strlen(CF_PROJ_Y_COORD),
                         CF_PROJ_Y_COORD );
        nc_put_att_text( fpImage, NCDFVarID, CF_LNG_NAME,
                         strlen("y coordinate of projection"),
                         "y coordinate of projection" );
        nc_put_att_text( fpImage, NCDFVarID, CF_UNITS, 1, "m" ); 

/* -------------------------------------------------------------------- */
/*      Write Y values                                                  */
/* -------------------------------------------------------------------- */

        /* Temporarily switch to data mode and write data */
        status = nc_enddef( fpImage );
        CPLDebug("GDAL_netCDF", "Writing Y values" );
        status = nc_put_vara_double( fpImage, NCDFVarID, startY,
                                     countY, padYVal);
        status = nc_redef( fpImage );
        
        /* free values */
        CPLFree( padYVal );

    } // projected

    else  {  /* If not Projected assume Geographic to catch grids without Datum */
        	
/* -------------------------------------------------------------------- */
/*      Write CF-1.x compliant Geographics attributes                   */
/*      Note: WKT information will not be preserved (e.g. WGS84)        */
/* -------------------------------------------------------------------- */
        
        if( bWriteGridMapping == TRUE ) 
 	    {
            strcpy( pszNetcdfProjection, "crs" );
            nc_def_var( fpImage, pszNetcdfProjection, NC_CHAR, 
                        0, NULL, &NCDFVarID );
            nc_put_att_text( fpImage, NCDFVarID, CF_GRD_MAPPING_NAME,
                             strlen(CF_PT_LATITUDE_LONGITUDE),
                             CF_PT_LATITUDE_LONGITUDE );

            /* ET - this should be written in a common block with projected */

            /* write DATUM information */
            dfTemp = oSRS.GetPrimeMeridian();
            nc_put_att_double( fpImage, NCDFVarID, CF_PP_LONG_PRIME_MERIDIAN,
                               NC_DOUBLE, 1, &dfTemp );
            dfTemp = oSRS.GetSemiMajor();
            nc_put_att_double( fpImage, NCDFVarID, CF_PP_SEMI_MAJOR_AXIS,
                               NC_DOUBLE, 1, &dfTemp );
            dfTemp = oSRS.GetInvFlattening();
            nc_put_att_double( fpImage, NCDFVarID, CF_PP_INVERSE_FLATTENING,
                               NC_DOUBLE, 1, &dfTemp );

            if ( bWriteGdalTags ) {
                // if ( strlen(pszProj4Defn) > 0 ) {
                //     nc_put_att_text( fpImage, NCDFVarID, "proj4",
                //                      strlen( pszProj4Defn ), pszProj4Defn );
                // }
                pszWKT = (char *) poSrcDS->GetProjectionRef() ;
                nc_put_att_text( fpImage, NCDFVarID, NCDF_SPATIAL_REF,
                                 strlen( pszWKT ), pszWKT );
                /* Only write GeoTransform in absence of lon/lat values */
                if ( bWriteLonLat == FALSE ) {
                    nc_put_att_text( fpImage, NCDFVarID, NCDF_GEOTRANSFORM,
                                     strlen( szGeoTransform ), 
                                     szGeoTransform );
                }
            }
        }
        

/* -------------------------------------------------------------------- */
/*      Get latitude values                                             */
/* -------------------------------------------------------------------- */
        if ( ! bBottomUp )
            dfY0 = adfGeoTransform[3];
        else /* invert latitude values */ 
            dfY0 = adfGeoTransform[3] + ( adfGeoTransform[5] * nYSize );
        dfDY = adfGeoTransform[5];
        
        padLatVal = (double *) CPLMalloc( nYSize * sizeof( double ) );
        for( int i=0; i<nYSize; i++ ) {
            /* The data point is centered inside the pixel */
            if ( ! bBottomUp )
                padLatVal[i] = dfY0 + (i+0.5)*dfDY ;
            else /* invert latitude values */ 
                padLatVal[i] = dfY0 - (i+0.5)*dfDY ;
        }
        
        startLat = (size_t *) CPLMalloc( sizeof( size_t ) );
        countLat = (size_t *) CPLMalloc( sizeof( size_t ) );
        startLat[0] = 0;
        countLat[0] = nYSize;
                
/* -------------------------------------------------------------------- */
/*      Get longitude values                                            */
/* -------------------------------------------------------------------- */
        dfX0 = adfGeoTransform[0];
        dfDX = adfGeoTransform[1];
        
        padLonVal = (double *) CPLMalloc( nXSize * sizeof( double ) );
        for( int i=0; i<nXSize; i++ ) {
            /* The data point is centered inside the pixel */
            padLonVal[i] = dfX0 + (i+0.5)*dfDX ;
        }
        
        startLon = (size_t *) CPLMalloc( sizeof( size_t ) );
        countLon = (size_t *) CPLMalloc( sizeof( size_t ) );
        startLon[0] = 0;
        countLon[0] = nXSize;
        
    }// not projected
    
    pfnProgress( 0.40, NULL, pProgressData );

/* -------------------------------------------------------------------- */
/*      Write CF projection lat/lon attributes                          */
/* -------------------------------------------------------------------- */
    if ( bWriteLonLat == TRUE ) {

/* -------------------------------------------------------------------- */
/*      Write latitude attributes                                     */
/* -------------------------------------------------------------------- */
        if ( oSRS.IsProjected() ) {
            int anLatDims[2];
            anLatDims[0] = nYDimID;
            anLatDims[1] = nXDimID;
            status = nc_def_var( fpImage, NCDF_DIMNAME_LAT, eLonLatType, 
                                 2, anLatDims, &NCDFVarID );
            /* compress lon/lat to save space */
            NCDF_DEF_VAR_DEFLATE;
        }
        else {
            int anLatDims[1];
            anLatDims[0] = nLatDimID;
            status = nc_def_var( fpImage, NCDF_DIMNAME_LAT, eLonLatType, 
                                 1, anLatDims, &NCDFVarID );                  
        }
        status = nc_put_att_text( fpImage, NCDFVarID, CF_STD_NAME,
                                8,"latitude" );
        status = nc_put_att_text( fpImage, NCDFVarID, CF_LNG_NAME,
                                  8, "latitude" );
        status = nc_put_att_text( fpImage, NCDFVarID, CF_UNITS,
                                  13, "degrees_north" );

/* -------------------------------------------------------------------- */
/*      Write latitude values                                         */
/* -------------------------------------------------------------------- */

        /* Temporarily switch to data mode and write data */
        status = nc_enddef( fpImage );
        CPLDebug("GDAL_netCDF", "Writing lat values" );
        status = nc_put_vara_double( fpImage, NCDFVarID, startLat,
                                     countLat, padLatVal);

        status = nc_redef( fpImage );
        
        /* free values */
        CPLFree( padLatVal );  
        CPLFree( startLat );
        CPLFree( countLat );
        
/* -------------------------------------------------------------------- */
/*      Write longitude attributes                                    */
/* -------------------------------------------------------------------- */
        if ( oSRS.IsProjected() ) {
            int anLonDims[2];
            anLonDims[0] = nYDimID;
            anLonDims[1] = nXDimID;
            status = nc_def_var( fpImage, NCDF_DIMNAME_LON, eLonLatType, 
                                 2, anLonDims, &NCDFVarID );
            /* compress lon/lat to save space */
            NCDF_DEF_VAR_DEFLATE;
        }
        else {
            int anLonDims[1];
            anLonDims[0] = nLonDimID;
            status = nc_def_var( fpImage, NCDF_DIMNAME_LON, eLonLatType, 
                                 1, anLonDims, &NCDFVarID );
        }
        nc_put_att_text( fpImage, NCDFVarID, CF_STD_NAME,
                         9, "longitude" );
        nc_put_att_text( fpImage, NCDFVarID, CF_LNG_NAME,
                         9, "longitude" );
        nc_put_att_text( fpImage, NCDFVarID, CF_UNITS,
                         12, "degrees_east" );
        
/* -------------------------------------------------------------------- */
/*      Write longitude values                                        */	
/* -------------------------------------------------------------------- */
        
        /* Temporarily switch to data mode and write data */
        status = nc_enddef( fpImage );
        CPLDebug("GDAL_netCDF", "Writing lon values" );
        status = nc_put_vara_double( fpImage, NCDFVarID, startLon,
                                    countLon, padLonVal);
        status = nc_redef( fpImage );
        
        /* free values */
        CPLFree( padLonVal );  
        CPLFree( startLon );
        CPLFree( countLon );
 
    } // bWriteLonLat

    pfnProgress( 0.50, NULL, pProgressData );

/* -------------------------------------------------------------------- */
/*      Initialize Band Map                                             */
/* -------------------------------------------------------------------- */
/* TODO: What was this used for? */
    // for(int j=1; j <= nBands; j++ ) {
    //     anBandMap[j-1]=j;
    // }

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
        int       NCDFVarID = 0;
        size_t    start[ NCDF_NBDIM ];
        size_t    *startY = NULL;
        size_t    count[ NCDF_NBDIM ];
        double    dfNoDataValue;
        /* unsigned char not supported by netcdf-3 */
        signed char cNoDataValue; 
        float     fNoDataValue;
        int       nlNoDataValue;
        short     nsNoDataValue;
        GDALRasterBandH	hBand;
        const char *tmpMetadata;
        char      szLongName[ NC_MAX_NAME ];

        nc_type nDataType = NC_NAT;
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( i );
        hBand = GDALGetRasterBand( poSrcDS, i );

        /* Get var name from NETCDF_VARNAME */
        tmpMetadata = poSrcBand->GetMetadataItem("NETCDF_VARNAME");
       	if( tmpMetadata != NULL) {
            if( nBands > 1 ) sprintf(szBandName,"%s%d",tmpMetadata,i);
            else strcpy( szBandName, tmpMetadata );
            // poSrcBand->SetMetadataItem("NETCDF_VARNAME","");
        }
        else 
            sprintf( szBandName, "Band%d", i );

        /* Get long_name from <var>#long_name */
        sprintf(szLongName,"%s#%s",
                poSrcBand->GetMetadataItem("NETCDF_VARNAME"),
                CF_LNG_NAME);
        tmpMetadata = poSrcDS->GetMetadataItem(szLongName);
        if( tmpMetadata != NULL) 
            strcpy( szLongName, tmpMetadata);
        else 
            sprintf( szLongName, "GDAL Band Number %d", i); 

/* -------------------------------------------------------------------- */
/*      Avoid code duplication                                          */
/* -------------------------------------------------------------------- */
        /* pre-calculate start and count to make code shorter */
        count[0]=1;
        count[1]=nXSize;
        start[1]=0;
        startY = (size_t *) CPLMalloc( nYSize * sizeof( size_t ) );

        for( int iLine = 0; iLine < nYSize ; iLine++ )  {
            if ( ! bBottomUp )
                startY[iLine] = iLine;
            else /* invert latitude values */
                startY[iLine] = nYSize - iLine - 1;
        }

        CPLDebug("GDAL_netCDF", "Writing Band #%d - %s", i, szLongName );

/* -------------------------------------------------------------------- */
/*      Get Data type                                                   */
/* -------------------------------------------------------------------- */
 
        eDT = poSrcBand->GetRasterDataType();

        eErr = CE_None;

        dfNoDataValue = poSrcBand->GetNoDataValue(0);

/* -------------------------------------------------------------------- */
/*      Byte                                                            */
/* -------------------------------------------------------------------- */
        if( eDT == GDT_Byte ) {
            CPLDebug( "GDAL_netCDF", "%s = GDT_Byte ",szBandName );
 
/* -------------------------------------------------------------------- */
/*      Define variable and attributes                                  */
/* -------------------------------------------------------------------- */
            /* Byte can be of different type according to file version */
#ifdef NETCDF_HAS_NC4
            /* NC4 supports NC_UBYTE, but should it always be ubyte? */
            if ( nFormat == NCDF_FORMAT_NC4 )
                nDataType = NC_UBYTE; 
            else 
#endif
                nDataType = NC_BYTE;

            status = nc_def_var( fpImage, szBandName, nDataType, 
                                 NCDF_NBDIM, anBandDims, &NCDFVarID );
            NCDF_ERR(status);

            NCDF_DEF_VAR_DEFLATE;

            /* Fill Value */
            cNoDataValue=(unsigned char) dfNoDataValue;
            nc_put_att_schar( fpImage, NCDFVarID, _FillValue,
                              nDataType, 1, &cNoDataValue );            
            NCDF_ERR(status);

            /* For NC_BYTE, add valid_range and _Unsigned = "true" */
            /* to specify unsigned byte ( defined in CF-1 and NUG ) */
            if ( nDataType == NC_BYTE ) {
                short int nValidRange[] = {0,255};
                status=nc_put_att_short( fpImage, NCDFVarID, "valid_range",
                                         NC_SHORT, 2, nValidRange );
                status = nc_put_att_text( fpImage, NCDFVarID, 
                                          "_Unsigned", 4, "true" );
            }

/* -------------------------------------------------------------------- */
/*      Read and write data from band i                                 */
/* -------------------------------------------------------------------- */
            /* End define mode */
            status = nc_enddef( fpImage );

            pabScanline = (GByte *) CPLMalloc( 1 * nXSize * sizeof(GByte) );
            
            for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {
                
                eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                            pabScanline, nXSize, 1, GDT_Byte,
                                            0,0);
                
                start[0]=startY[iLine];          
                status = nc_put_vara_uchar (fpImage, NCDFVarID, start,
                                            count, pabScanline);
                NCDF_ERR(status);
            }

            CPLFree( pabScanline );

            /* Back to define mode */
           status = nc_redef( fpImage );

/* -------------------------------------------------------------------- */
/*      Int16                                                           */
/* -------------------------------------------------------------------- */

        } else if( ( eDT == GDT_UInt16 ) || ( eDT == GDT_Int16 ) ) {
            CPLDebug( "GDAL_netCDF", "%s = GDT_Int16 ",szBandName );

/* -------------------------------------------------------------------- */
/*      Define variable and attributes                                  */
/* -------------------------------------------------------------------- */
            nDataType = NC_SHORT;
            status = nc_def_var( fpImage, szBandName, nDataType, 
                                 NCDF_NBDIM, anBandDims, &NCDFVarID );

            NCDF_DEF_VAR_DEFLATE;

            nsNoDataValue= (GInt16) dfNoDataValue;
            nc_put_att_short( fpImage, NCDFVarID, _FillValue,
                              nDataType, 1, &nsNoDataValue );

/* -------------------------------------------------------------------- */
/*      Read and write data from band i                                 */
/* -------------------------------------------------------------------- */
            status = nc_enddef( fpImage );

            pasScanline = (GInt16 *) CPLMalloc( 1 * nXSize *
                                                sizeof( GInt16 ) );

            for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {

                eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                            pasScanline, nXSize, 1, GDT_Int16,
                                            0,0);

                start[0]=startY[iLine];          
                status = nc_put_vara_short( fpImage, NCDFVarID, start,
                                            count, pasScanline);
            }

            CPLFree( pasScanline );

            status = nc_redef( fpImage );

/* -------------------------------------------------------------------- */
/*      Int32                                                           */
/* -------------------------------------------------------------------- */

        } else if( (eDT == GDT_UInt32) || (eDT == GDT_Int32) ) {
            CPLDebug( "GDAL_netCDF", "%s = GDT_Int32 ",szBandName );

/* -------------------------------------------------------------------- */
/*      Define variable and attributes                                  */
/* -------------------------------------------------------------------- */
            nDataType = NC_INT;
            status = nc_def_var( fpImage, szBandName, nDataType, 
                                 NCDF_NBDIM, anBandDims, &NCDFVarID );

            NCDF_DEF_VAR_DEFLATE;
            
            nlNoDataValue= (GInt32) dfNoDataValue;
            nc_put_att_int( fpImage, NCDFVarID, _FillValue, 
                            nDataType, 1, &nlNoDataValue );

/* -------------------------------------------------------------------- */
/*      Read and write data from band i                                 */
/* -------------------------------------------------------------------- */
            status = nc_enddef( fpImage );

            panScanline = (GInt32 *) CPLMalloc( 1 * nXSize *
                                                sizeof( GInt32 ) );

            for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {

                eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                            panScanline, nXSize, 1, GDT_Int32,
                                            0,0);

                start[0]=startY[iLine];          
                status = nc_put_vara_int( fpImage, NCDFVarID, start,
                                          count, panScanline);
            }

            CPLFree( panScanline );

            status = nc_redef( fpImage );

/* -------------------------------------------------------------------- */
/*      float                                                           */
/* -------------------------------------------------------------------- */
        } else if( eDT == GDT_Float32 ) {
            CPLDebug( "GDAL_netCDF", "%s = GDT_Float32 ",szBandName );

/* -------------------------------------------------------------------- */
/*      Define variable and attributes                                  */
/* -------------------------------------------------------------------- */
            nDataType = NC_FLOAT;
            status = nc_def_var( fpImage, szBandName, nDataType, 
                                 NCDF_NBDIM, anBandDims, &NCDFVarID );

            NCDF_DEF_VAR_DEFLATE;

            fNoDataValue= (float) dfNoDataValue;
            nc_put_att_float( fpImage, NCDFVarID, _FillValue, 
                              nDataType, 1, &fNoDataValue );

/* -------------------------------------------------------------------- */
/*      Read and write data from band i                                 */
/* -------------------------------------------------------------------- */
            status = nc_enddef( fpImage );

            pafScanline = (float *) CPLMalloc( 1 * nXSize *
                                               sizeof( float ) );

            for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {

                eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                            pafScanline, nXSize, 1, GDT_Float32, 
                                            0,0);

                start[0]=startY[iLine];          
                status = nc_put_vara_float( fpImage, NCDFVarID, start,
                                            count, pafScanline);
            }

            CPLFree( pafScanline );

            status = nc_redef( fpImage );

/* -------------------------------------------------------------------- */
/*      double                                                          */
/* -------------------------------------------------------------------- */
        } else if( eDT == GDT_Float64 ) {
            CPLDebug( "GDAL_netCDF", "%s = GDT_Float64 ",szBandName );

/* -------------------------------------------------------------------- */
/*      Define variable and attributes                                  */
/* -------------------------------------------------------------------- */
            nDataType = NC_DOUBLE;
            status = nc_def_var( fpImage, szBandName, nDataType, 
                                 NCDF_NBDIM, anBandDims, &NCDFVarID );

            NCDF_DEF_VAR_DEFLATE;

            nc_put_att_double( fpImage, NCDFVarID, _FillValue,
                               nDataType, 1, &dfNoDataValue );

/* -------------------------------------------------------------------- */
/*      Read and write data from band i                                 */
/* -------------------------------------------------------------------- */
            status = nc_enddef( fpImage );

            padScanline = (double *) CPLMalloc( 1 * nXSize *
                                                sizeof( double ) );

            for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {

                eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                            padScanline, nXSize, 1, 
                                            GDT_Float64,
                                            0,0);

                start[0]=startY[iLine];          
                status = nc_put_vara_double( fpImage, NCDFVarID, start,
                                             count, padScanline);
            }

            CPLFree( padScanline );

            status = nc_redef( fpImage );

        }
        else {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "The NetCDF driver does not support GDAL data type %d",
                      eDT );
        }
        
        CPLFree( startY );

/* -------------------------------------------------------------------- */
/*      Copy Metadata for band                                          */
/* -------------------------------------------------------------------- */

        nc_put_att_text( fpImage, NCDFVarID, CF_LNG_NAME, 
                         strlen( szLongName ), szLongName );

        CopyMetadata( (void *) hBand, fpImage, NCDFVarID );

/* -------------------------------------------------------------------- */
/*      Write Projection for band                                       */
/* -------------------------------------------------------------------- */
        if( bWriteGridMapping == TRUE ) {
            nc_put_att_text( fpImage, NCDFVarID, CF_GRD_MAPPING,
                             strlen( pszNetcdfProjection ),
                             pszNetcdfProjection );
            if ( bWriteLonLat == TRUE ) {
                nc_put_att_text( fpImage, NCDFVarID, CF_COORDINATES,
                                 strlen( NCDF_LONLAT ), NCDF_LONLAT );
            }           
        }

        dfTemp = 0.50 + ( (0.90 - 0.50) * i / nBands );            
        pfnProgress( dfTemp, NULL, pProgressData );

    } // for nBands


/* -------------------------------------------------------------------- */
/*      Cleanup and close.                                              */
/* -------------------------------------------------------------------- */
    nc_close( fpImage );
// CPLFree(pszProj4Defn );
 
    pfnProgress( 0.95, NULL, pProgressData );

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/*      Disable PAM, at least temporarily. See bug #4244                */
/* -------------------------------------------------------------------- */
    netCDFDataset *poDS = (netCDFDataset *) GDALOpen( pszFilename, GA_ReadOnly );

    // if( poDS )
    //     poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
    
    pfnProgress( 1.0, NULL, pProgressData );

    return poDS;
}


/************************************************************************/
/*                          GDALRegister_netCDF()                       */
/************************************************************************/

void GDALRegister_netCDF()

{
    if (! GDAL_CHECK_VERSION("netCDF driver"))
        return;

    if( GDALGetDriverByName( "netCDF" ) == NULL )
    {
        GDALDriver	*poDriver;
        char szCreateOptions[3072];

        poDriver = new GDALDriver( );

/* -------------------------------------------------------------------- */
/*      Build full creation option list.                                */
/* -------------------------------------------------------------------- */
    sprintf( szCreateOptions, "%s", 
"<CreationOptionList>"
"   <Option name='FORMAT' type='string-select' default='NC'>"
"     <Value>NC</Value>"
#ifdef NETCDF_HAS_NC2
"     <Value>NC2</Value>"
#endif
#ifdef NETCDF_HAS_NC4
"     <Value>NC4</Value>"
"     <Value>NC4C</Value>"
#endif
"   </Option>"
#ifdef NETCDF_HAS_NC4
"   <Option name='COMPRESS' type='string-select' default='NONE'>"
"     <Value>NONE</Value>"
"     <Value>DEFLATE</Value>"
"   </Option>"
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='1'/>"
#endif
"   <Option name='WRITE_BOTTOMUP' type='boolean' default='NO'>"
"   </Option>"
"   <Option name='WRITE_GDAL_TAGS' type='boolean' default='YES'>"
"   </Option>"
"   <Option name='WRITE_LONLAT' type='string-select'>"
"     <Value>YES</Value>"
"     <Value>NO</Value>"
"     <Value>IF_NEEDED</Value>"
"   </Option>"
"   <Option name='TYPE_LONLAT' type='string-select'>"
"     <Value>float</Value>"
"     <Value>double</Value>"
"   </Option>"
"</CreationOptionList>" );

        
/* -------------------------------------------------------------------- */
/*      Set the driver details.                                         */
/* -------------------------------------------------------------------- */
        poDriver->SetDescription( "netCDF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Network Common Data Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_netcdf.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "nc" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
                                   szCreateOptions );

        /* make driver config and capabilities available */
        poDriver->SetMetadataItem( "NETCDF_VERSION", nc_inq_libvers() );
        poDriver->SetMetadataItem( "NETCDF_CONVENTIONS", NCDF_CONVENTIONS_CF );
#ifdef NETCDF_HAS_NC2
        poDriver->SetMetadataItem( "NETCDF_HAS_NC2", "YES" );
#endif
#ifdef NETCDF_HAS_NC4
        poDriver->SetMetadataItem( "NETCDF_HAS_NC4", "YES" );
#endif
#ifdef NETCDF_HAS_HDF4
        poDriver->SetMetadataItem( "NETCDF_HAS_HDF4", "YES" );
#endif
#ifdef HAVE_HDF4
        poDriver->SetMetadataItem( "GDAL_HAS_HDF4", "YES" );
#endif
#ifdef HAVE_HDF5
        poDriver->SetMetadataItem( "GDAL_HAS_HDF5", "YES" );
#endif
 
        /* set pfns and register driver */
        poDriver->pfnOpen = netCDFDataset::Open;
        poDriver->pfnCreateCopy = NCDFCreateCopy;
        poDriver->pfnIdentify = netCDFDataset::Identify;

        GetGDALDriverManager( )->RegisterDriver( poDriver );
    }
}

/************************************************************************/
/*                          New functions                               */
/************************************************************************/

/* Test for GDAL version string >= target */
int NCDFIsGDALVersionGTE(const char* pszVersion, int nTarget)
{
    int nVersion = 0;
    int nVersions [] = {0,0,0,0};
    char **papszTokens;

    /* Valid strings are "GDAL 1.9dev, released 2011/01/18" and "GDAL 1.8.1 " */
    if ( pszVersion == NULL || EQUAL( pszVersion, "" ) )
        return FALSE;
    else if ( ! EQUALN("GDAL ", pszVersion, 5) )
        return FALSE;
    else if ( EQUALN("GDAL 1.9dev", pszVersion,11 ) )
        return nTarget <= 1900;
    else if ( EQUALN("GDAL 1.8dev", pszVersion,11 ) )
        return nTarget <= 1800;

    papszTokens = CSLTokenizeString2( pszVersion+5, ".", 0 );

    for ( int iToken = 0; papszTokens && papszTokens[iToken]; iToken++ )  {
        nVersions[iToken] = atoi( papszTokens[iToken] );
    }
    /* (GDAL_VERSION_MAJOR*1000+GDAL_VERSION_MINOR*100+GDAL_VERSION_REV*10+GDAL_VERSION_BUILD) */
    nVersion = nVersions[0]*1000 + nVersions[1]*100 + 
        nVersions[2]*10 + nVersions[3]; 
    
    CSLDestroy( papszTokens );
    return nTarget <= nVersion;
}

/* Add Conventions, GDAL version and history  */ 
void NCDFAddGDALHistory( int fpImage, const char * pszFilename, const char *pszOldHist )
{
    char     szTemp[NC_MAX_NAME];

    nc_put_att_text( fpImage, NC_GLOBAL, "Conventions", 
                     strlen(NCDF_CONVENTIONS_CF),
                     NCDF_CONVENTIONS_CF ); 
    
    nc_put_att_text( fpImage, NC_GLOBAL, "GDAL", 
                     strlen(NCDF_GDAL), NCDF_GDAL ); 

    /* Add history */
#ifdef GDAL_SET_CMD_LINE_DEFINED
    if ( ! EQUAL(GDALGetCmdLine(), "" ) )
        strcpy( szTemp, GDALGetCmdLine() );
    else
        sprintf( szTemp, "GDAL NCDFCreateCopy( %s, ... )",pszFilename );
#else
    sprintf( szTemp, "GDAL NCDFCreateCopy( %s, ... )",pszFilename );
#endif
    
    NCDFAddHistory( fpImage, szTemp, pszOldHist );

}

/* code taken from cdo and libcdi, used for writing the history attribute */
//void cdoDefHistory(int fileID, char *histstring)
void NCDFAddHistory(int fpImage, const char *pszAddHist, const char *pszOldHist)
{
    char strtime[32];
    time_t tp;
    struct tm *ltime;

    char *pszNewHist = NULL;
    size_t nNewHistSize = 0;
    int disableHistory = FALSE;
    int status;

    /* Check pszOldHist - as if there was no previous history, it will be
       a null pointer - if so set as empty. */
    if (NULL == pszOldHist) {
        pszOldHist = "";
    }

    tp = time(NULL);
    if ( tp != -1 )
    {
        ltime = localtime(&tp);
        (void) strftime(strtime, sizeof(strtime), "%a %b %d %H:%M:%S %Y: ", ltime);
    }

    // status = nc_get_att_text( fpImage, NC_GLOBAL, 
    //                           "history", pszOldHist );
    // printf("status: %d pszOldHist: [%s]\n",status,pszOldHist);
    
    nNewHistSize = strlen(pszOldHist)+strlen(strtime)+strlen(pszAddHist)+1+1;
    pszNewHist = (char *) CPLMalloc(nNewHistSize * sizeof(char));
    
    strcpy(pszNewHist, strtime);
    strcat(pszNewHist, pszAddHist);

    if ( disableHistory == FALSE && pszNewHist )
    {
        if ( ! EQUAL(pszOldHist,"") )
            strcat(pszNewHist, "\n");
        strcat(pszNewHist, pszOldHist);
    }

    status = nc_put_att_text( fpImage, NC_GLOBAL, 
                              "history", strlen(pszNewHist),
                              pszNewHist ); 
    NCDF_ERR(status);

    CPLFree(pszNewHist);
}


int NCDFIsCfProjection( const char* pszProjection ) 
{
    /* Find the appropriate mapping */
    for (int iMap = 0; poNetcdfSRS_PT[iMap].WKT_SRS != NULL; iMap++ ) {
        // printf("now at %d, proj=%s\n",i, poNetcdfSRS_PT[i].GDAL_SRS);
        if ( EQUAL( pszProjection, poNetcdfSRS_PT[iMap].WKT_SRS ) )  {
            if ( poNetcdfSRS_PT[iMap].mappings != NULL )
                return TRUE;            
            else 
                return FALSE;
        }
    }
    return FALSE;
}


/* Write any needed projection attributes *
 * poPROJCS: ptr to proj crd system
 * pszProjection: name of projection system in GDAL WKT
 * fpImage: open NetCDF file in writing mode
 * NCDFVarID: NetCDF Var Id of proj system we're writing in to
 *
 * The function first looks for the oNetcdfSRS_PP mapping object
 * that corresponds to the input projection name. If none is found
 * the generic mapping is used.  In the case of specific mappings,
 * the driver looks for each attribute listed in the mapping object
 * and then looks up the value within the OGR_SRSNode. In the case
 * of the generic mapping, the lookup is reversed (projection params, 
 * then mapping).  For more generic code, GDAL->NETCDF 
 * mappings and the associated value are saved in std::map objects.
 */

/* NOTE modifications by ET to combine the specific and generic mappings */

void NCDFWriteProjAttribs( const OGR_SRSNode *poPROJCS,
                           const char* pszProjection,
                           const int fpImage, const int NCDFVarID ) 
{                            
    double dfStdP[2];
    int bFoundStdP1=FALSE,bFoundStdP2=FALSE;
    double dfValue=0.0;
    const char *pszParamStr, *pszParamVal;
    const std::string *pszNCDFAtt, *pszGDALAtt;
    static const oNetcdfSRS_PP *poMap = NULL;
    int nMapIndex = -1;

    //Attribute <GDAL,NCDF> and Value <NCDF,value> mappings
    std::map< std::string, std::string > oAttMap;
    std::map< std::string, std::string >::iterator oAttIter;
    std::map< std::string, double > oValMap;
    std::map< std::string, double >::iterator oValIter;
    //results to write
    std::vector< std::pair<std::string,double> > oOutList;
 
    /* Find the appropriate mapping */
    for (int iMap = 0; poNetcdfSRS_PT[iMap].WKT_SRS != NULL; iMap++ ) {
        // printf("now at %d, proj=%s\n",i, poNetcdfSRS_PT[i].GDAL_SRS);
        if ( EQUAL( pszProjection, poNetcdfSRS_PT[iMap].WKT_SRS ) ) {
            nMapIndex = iMap;
            poMap = poNetcdfSRS_PT[iMap].mappings;
            // CPLDebug( "GDAL_netCDF", 
            //           "Found mapping{poNetcdfSRS_PT[iMap].NCDF_SRS ,poNetcdfSRS_PT[iMap].GDAL_SRS ,}\n" );
            break;
        }
    }

    //ET TODO if projection name is not found, should we do something special?
    if ( nMapIndex == -1 ) {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "projection name %s not found in the lookup tables!!!",
                  pszProjection);
    }
    /* if no mapping was found or assigned, set the generic one */
    if ( !poMap ) {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "projection name %s in not part of the CF standard, will not be supported by CF!",
                  pszProjection);
        poMap = poGenericMappings;
    }

    /* initialize local map objects */
    for ( int iMap = 0; poMap[iMap].WKT_ATT != NULL; iMap++ ) {
        oAttMap[poMap[iMap].WKT_ATT] = poMap[iMap].CF_ATT;
    }

    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ ) {

        const OGR_SRSNode *poNode;

        poNode = poPROJCS->GetChild( iChild );
        if( !EQUAL(poNode->GetValue(),"PARAMETER") 
            || poNode->GetChildCount() != 2 )
            continue;
        pszParamStr = poNode->GetChild(0)->GetValue();
        pszParamVal = poNode->GetChild(1)->GetValue();

        oValMap[pszParamStr] = atof(pszParamVal);
    }

    /* Lookup mappings and fill output vector */
    if ( poMap != poGenericMappings ) { /* specific mapping, loop over mapping values */

        for ( oAttIter = oAttMap.begin(); oAttIter != oAttMap.end(); oAttIter++ ) {

            pszGDALAtt = &(oAttIter->first);
            pszNCDFAtt = &(oAttIter->second);
            oValIter = oValMap.find( *pszGDALAtt );

            if ( oValIter != oValMap.end() ) {

                dfValue = oValIter->second;
                oOutList.push_back( std::make_pair( *pszNCDFAtt, dfValue ) );
                
                /* special case for PS grid */
                if ( EQUAL( SRS_PP_LATITUDE_OF_ORIGIN, pszGDALAtt->c_str() ) &&
                     EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) ) {
                    double dfLatPole = 0.0;
                    if ( dfValue > 0.0) dfLatPole = 90.0;
                    else dfLatPole = -90.0;
                        oOutList.push_back( std::make_pair( CF_PP_LAT_PROJ_ORIGIN, 
                                                            dfLatPole ) );
                }              
            }
            // else printf("NOT FOUND!!!\n");
        }
    
    }
    else { /* generic mapping, loop over projected values */

        for ( oValIter = oValMap.begin(); oValIter != oValMap.end(); oValIter++ ) {

            pszGDALAtt = &(oValIter->first);
            dfValue = oValIter->second;

            oAttIter = oAttMap.find( *pszGDALAtt );

            if ( oAttIter != oAttMap.end() ) {
                oOutList.push_back( std::make_pair( oAttIter->second, dfValue ) );
            }
            /* for SRS_PP_SCALE_FACTOR write 2 mappings */
            else if (  EQUAL(pszGDALAtt->c_str(), SRS_PP_SCALE_FACTOR) ) {
                oOutList.push_back( std::make_pair( CF_PP_SCALE_FACTOR_MERIDIAN,
                                                    dfValue ) );
                oOutList.push_back( std::make_pair( CF_PP_SCALE_FACTOR_ORIGIN,
                                                    dfValue ) );
            }
            /* if not found insert the GDAL name */
            else {
                oOutList.push_back( std::make_pair( *pszGDALAtt, dfValue ) );
            }
        }
    }

    /* Write all the values that were found */
    // std::vector< std::pair<std::string,double> >::reverse_iterator it;
    // for (it = oOutList.rbegin();  it != oOutList.rend(); it++ ) {
    std::vector< std::pair<std::string,double> >::iterator it;
    for (it = oOutList.begin();  it != oOutList.end(); it++ ) {
        pszParamVal = (it->first).c_str();
        dfValue = it->second;
        /* Handle the STD_PARALLEL attrib */
        if( EQUAL( pszParamVal, CF_PP_STD_PARALLEL_1 ) ) {
            bFoundStdP1 = TRUE;
            dfStdP[0] = dfValue;
        }
        else if( EQUAL( pszParamVal, CF_PP_STD_PARALLEL_2 ) ) {
            bFoundStdP2 = TRUE;
            dfStdP[1] = dfValue;
        } 
        else {
            nc_put_att_double( fpImage, NCDFVarID, pszParamVal,
                               NC_DOUBLE, 1,&dfValue );
        }
    }
    /* Now write the STD_PARALLEL attrib */
    if ( bFoundStdP1 ) { 
        /* one value or equal values */
        if ( !bFoundStdP2 || dfStdP[0] ==  dfStdP[1] ) {
            nc_put_att_double( fpImage, NCDFVarID, CF_PP_STD_PARALLEL, 
                               NC_DOUBLE, 1, &dfStdP[0] );
        }
        else { /* two values */
            nc_put_att_double( fpImage, NCDFVarID, CF_PP_STD_PARALLEL, 
                               NC_DOUBLE, 2, dfStdP );
        }
    }
}
