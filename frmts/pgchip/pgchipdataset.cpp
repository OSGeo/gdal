/******************************************************************************
 *
 * File :    pgchipdataset.cpp
 * Project:  PGCHIP Driver
 * Purpose:  GDALDataset code for POSTGIS CHIP/GDAL Driver 
 * Author:   Benjamin Simon, noumayoss@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Benjamin Simon, noumayoss@gmail.com
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
 * Revision 1.1  2005/08/29 bsimon
 * New
 *
 */

#include "pgchip.h"


CPL_C_START
void	GDALRegister_PGCHIP(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				PGCHIPDataset				*/
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            PGCHIPDataset()                           */
/************************************************************************/

PGCHIPDataset::PGCHIPDataset(){

    hPGConn = NULL;
    pszConnectionString = NULL;
    pszDBName = NULL;
    pszName = NULL;
    bHavePostGIS = FALSE;
    PGCHIP = NULL;
    
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    
    SRID = -1;
    pszProjection = CPLStrdup("");
    
    bHaveNoData = FALSE;
    dfNoDataValue = -1;
}

/************************************************************************/
/*                            ~PGCHIPDataset()                             */
/************************************************************************/

PGCHIPDataset::~PGCHIPDataset(){

    CPLFree(pszProjection);
    CPLFree(pszConnectionString);
    CPLFree(pszDBName);
    CPLFree(pszName);

    
    if(PGCHIP->data)
        CPLFree(PGCHIP->data);
        
    if(PGCHIP)
        CPLFree(PGCHIP);
    
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PGCHIPDataset::GetGeoTransform( double * padfTransform ){
    
    memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform[0]) * 6 );

    if( bGeoTransformValid )
        return CE_None;
    else
        return CE_Failure;
}



/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PGCHIPDataset::SetGeoTransform( double * padfTransform ){

    CPLErr              eErr = CE_None;

    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

    if ( pszConnectionString && bGeoTransformValid )
    {
    
        /* NOT YET AVAILABLE */
    
    }

    return eErr;
}
    

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *PGCHIPDataset::GetProjectionRef(){

    char    szCommand[1024];
    PGconn      *hPGConn;
    PGresult    *hResult;
    int SRID = -1;        
    
    hPGConn = this->hPGConn;
    
    SRID = this->PGCHIP->SRID;
    
/* -------------------------------------------------------------------- */
/*      Reading proj                                                    */
/* -------------------------------------------------------------------- */
     
    sprintf( szCommand,"SELECT srtext FROM spatial_ref_sys where SRID=%d",SRID);
            
    hResult = PQexec(hPGConn,szCommand);
        
    if(SRID == -1) {
        return "";
    }
    else if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
             && PQntuples(hResult) > 0 ){
        
        pszProjection = CPLStrdup(PQgetvalue(hResult,0,0)); 
                
        return( pszProjection );
    }
    
    if( hResult )
        PQclear( hResult );
    
    return NULL;
}


/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr PGCHIPDataset::SetProjection( const char * pszNewProjection ){

    char    szCommand[1024];
    PGconn      *hPGConn;
    PGresult    *hResult;
    
    hPGConn = this->hPGConn;
    
    
    if( !EQUALN(pszNewProjection,"GEOGCS",6)
        && !EQUALN(pszNewProjection,"PROJCS",6)
        && !EQUALN(pszProjection,"+",1)
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Only OGC WKT Projections supported for writing to Postgis.\n"
                "%s not supported.",
                  pszNewProjection );
        
        return CE_Failure;
    }
    
    CPLFree( pszProjection );
        
/* -------------------------------------------------------------------- */
/*      Reading SRID                                                    */
/* -------------------------------------------------------------------- */
     
    this->SRID = -1;
    
    if( pszNewProjection[0]=='+')    
        sprintf( szCommand,"SELECT SRID FROM spatial_ref_sys where proj4text=%s",pszNewProjection);
    else
        sprintf( szCommand,"SELECT SRID FROM spatial_ref_sys where srtext=%s",pszNewProjection);
    
    
    hResult = PQexec(hPGConn,szCommand);
        
    
    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
             && PQntuples(hResult) > 0 ){
        
        this->SRID = atoi(PQgetvalue(hResult,0,0)); 
        
        pszProjection = CPLStrdup( pszNewProjection );             
        
        PQclear( hResult );        
        
        return CE_None;
    }
    
    // Try to find SRID via EPSG number
    if (this->SRID == -1 && strcmp(pszNewProjection,"")!=0){
                
            char *buf;
            char epsg[16];
            memset(epsg,0,16);
            char *workingproj = (char *)pszNewProjection;
    
            while( (buf = strstr(workingproj,"EPSG")) != 0){
                workingproj = buf+4;
            }
            
            int iChar = 0;
            workingproj = workingproj + 3;
            
            while(workingproj[iChar] != '"'){
                epsg[iChar] = workingproj[iChar];
                iChar++;
            }
            
            if(epsg[0] != 0){
                this->SRID = atoi(epsg); 
                pszProjection = CPLStrdup(pszNewProjection);    
            }
            
            return CE_None;
    }
    else{
         
            CPLError( CE_Failure, CPLE_AppDefined,
                "Projection %s not found in spatial_ref_sys table.\n",
                  pszNewProjection );
        
            this->SRID = -1;
            pszProjection = CPLStrdup("");    
                  
            if( hResult )
                PQclear( hResult );
                  
            return CE_Failure;
    }
}



/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PGCHIPDataset::Open( GDALOpenInfo * poOpenInfo ){

    char                szCommand[1024];
    PGresult            *hResult = NULL;
    PGCHIPDataset 	*poDS = NULL;
    char                *chipStringHex;

    unsigned char        *chipdata;
    char                *layerName;
    int                 t;
       
            
    /* Chek Postgis connection string */
    if( poOpenInfo->pszFilename == NULL)
        return NULL;
             
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */

    poDS = new PGCHIPDataset();
    poDS->pszConnectionString = CPLStrdup(poOpenInfo->pszFilename);
    layerName = CPLStrdup(poOpenInfo->pszFilename);

/* -------------------------------------------------------------------- */
/*      Verify postgresql prefix.                                       */
/* -------------------------------------------------------------------- */
    if( !EQUALN(poDS->pszConnectionString,"PG:",3) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                      "%s does not conform to PostgreSQL naming convention,"
                      " PG:*\n" );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Try to establish connection.                                    */
/* -------------------------------------------------------------------- */
    int i=0;
    while(poDS->pszConnectionString[i] != '\0'){
        
        if(poDS->pszConnectionString[i] == '#')
            poDS->pszConnectionString[i] = ' ';
        if(poDS->pszConnectionString[i] == '%')
            poDS->pszConnectionString[i] = '\0';
        i++;
    }
        
    
    poDS->hPGConn = PQconnectdb( poDS->pszConnectionString + 3 );
    
    if( poDS->hPGConn == NULL || PQstatus(poDS->hPGConn) == CONNECTION_BAD )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "PGconnectcb failed.\n%s", 
                  PQerrorMessage(poDS->hPGConn) );
        PQfinish(poDS->hPGConn);
        poDS->hPGConn = NULL;
        return NULL;
    }
    
    
/* -------------------------------------------------------------------- */
/*      Try to establish the database name from the connection          */
/*      string passed.                                                  */
/* -------------------------------------------------------------------- */
    if( strstr(poDS->pszConnectionString, "dbname=") != NULL )
    {
        int     i;

        poDS->pszDBName = CPLStrdup( strstr(poDS->pszConnectionString, "dbname=") + 7 );
                
        for( i = 0; poDS->pszDBName[i] != '\0'; i++ )
        {
            if( poDS->pszDBName[i] == ' ' )                                   
            {
                poDS->pszDBName[i] = '\0';
                break;
            }
        }
    }
    else if( getenv( "USER" ) != NULL )
        poDS->pszDBName = CPLStrdup( getenv("USER") );
    else
        poDS->pszDBName = CPLStrdup( "unknown_dbname" );
                   
        
/* -------------------------------------------------------------------- */
/*      Test to see if this database instance has support for the       */
/*      PostGIS Geometry type.  If so, disable sequential scanning      */
/*      so we will get the value of the gist indexes.                   */
/* -------------------------------------------------------------------- */
       
    
    hResult = PQexec(poDS->hPGConn, 
                         "SELECT oid FROM pg_type WHERE typname = 'geometry'" );
   
                         
    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
        && PQntuples(hResult) > 0 )
    {
        poDS->bHavePostGIS = TRUE;
    }

    if( hResult )
        PQclear( hResult );
    
    if(!poDS->bHavePostGIS){
        CPLError( CE_Failure, CPLE_AppDefined, 
                      "Can't find geometry type, is Postgis correctly installed ?\n");
        return NULL;
    }
    
    
/* -------------------------------------------------------------------- */
/*  try opening the layer                                               */
/* -------------------------------------------------------------------- */
    
    if( strstr(layerName, "layer=") != NULL )
    {
	poDS->pszName = CPLStrdup( strstr(layerName, "layer=") + 6 );
    }
    else        
        poDS->pszName = CPLStrdup("unknown_layer");
            
    
/* -------------------------------------------------------------------- */
/*      Read the chip header                                            */
/* -------------------------------------------------------------------- */
    
    
    hResult = PQexec(poDS->hPGConn, "BEGIN");
    
    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        PQclear( hResult );
        sprintf( szCommand, 
                 "SELECT raster FROM %s",
                 poDS->pszName);
                         
        hResult = PQexec(poDS->hPGConn,szCommand);
    }


    if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", PQerrorMessage(poDS->hPGConn) );
        return NULL;
    }
           
    chipStringHex = PQgetvalue(hResult, 0, 0);
    int stringlen = strlen((char *)chipStringHex);
        
    // Allocating memory for chip
    chipdata = (unsigned char *) CPLMalloc(stringlen/2);
                      	
    for (t=0;t<stringlen/2;t++){
	chipdata[t] = parse_hex( &chipStringHex[t*2]) ;
    }
    
    // Chip assigment
    poDS->PGCHIP = (CHIP *)chipdata;
           
    if( hResult )
        PQclear( hResult );
    
    hResult = PQexec(poDS->hPGConn, "COMMIT");
    PQclear( hResult );
    
    
/* -------------------------------------------------------------------- */
/*      Set some information from the file that is of interest.         */
/* -------------------------------------------------------------------- */

    poDS->nRasterXSize = poDS->PGCHIP->width;
    poDS->nRasterYSize = poDS->PGCHIP->height;
    poDS->nBands = (int)poDS->PGCHIP->future[0];
    poDS->nBitDepth = (int)poDS->PGCHIP->future[1];
    poDS->nColorType = (int)poDS->PGCHIP->future[2];
    
        
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
        poDS->SetBand( iBand+1, new PGCHIPRasterBand( poDS, iBand+1 ) );
    
    
/* -------------------------------------------------------------------- */
/*      Is there a palette?  Note: we should also read back and         */
/*      apply transparency values if available.                         */
/* -------------------------------------------------------------------- */
    if( poDS->nColorType == PGCHIP_COLOR_TYPE_PALETTE )
    {
        unsigned char *pPalette;
        int	nColorCount = 0;
        int     sizePalette = 0;
        int     offsetColor = -1;
        GDALColorEntry oEntry;
                
        nColorCount = (int)poDS->PGCHIP->compression;
        pPalette = (unsigned char *)chipdata + sizeof(CHIP);
        sizePalette = nColorCount * sizeof(pgchip_color);
        
        poDS->poColorTable = new GDALColorTable();
        
        for( int iColor = 0; iColor < nColorCount; iColor++ )
        {
            oEntry.c1 = pPalette[offsetColor++];
            oEntry.c2 = pPalette[offsetColor++];
            oEntry.c3 = pPalette[offsetColor++];
            oEntry.c4 = pPalette[offsetColor++];
           
            poDS->poColorTable->SetColorEntry( iColor, &oEntry );
        }
    }
    
    return( poDS );
}


/************************************************************************/
/*                           PGCHIPCreateCopy()                         */
/************************************************************************/
static GDALDataset * PGCHIPCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                int bStrict, char ** papszOptions, 
                GDALProgressFunc pfnProgress, void * pProgressData ){

    
    PGconn      *hPGConn;
    char	*pszConnectionString;
    char	*pszDBName;
    char        *pszName;
    int         bHavePostGIS;
    char                *szCommand;
    PGresult            *hResult;
    char    *layerName;
    char    *pszProjection;
    int    SRID;
    GDALColorTable	*poCT= NULL;
    
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    int  nBands = poSrcDS->GetRasterCount();
    
    
/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    
    /* check number of bands */
    if( nBands != 1 && nBands != 4)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Under development : PGCHIP driver doesn't support %d bands.  Must be 1 or 4\n", nBands );

        return NULL;
    }
    
    
    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte 
        && poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt16)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Under development : PGCHIP driver doesn't support data type %s. "
                  "Only eight bit (Byte) and sixteen bit (UInt16) bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }
    
    /* Check Postgis connection string */
    if( pszFilename == NULL){
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Connection string is NULL.\n");
        return NULL;
    }     
    
    
/* -------------------------------------------------------------------- */
/*      Setup some parameters.                                          */
/* -------------------------------------------------------------------- */
    
    int nBitDepth;
    GDALDataType eType;
    int storageChunk;
    int nColorType=0;
       
    
    if( nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() == NULL ){
        nColorType = PGCHIP_COLOR_TYPE_GRAY;
    }
    else if( nBands == 1 ){
        nColorType = PGCHIP_COLOR_TYPE_PALETTE;
    }
    else if( nBands == 4 ){
        nColorType = PGCHIP_COLOR_TYPE_RGB_ALPHA;
    }
    
    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt16 )
    {
        eType = GDT_Byte;
        nBitDepth = 8;
    }
    else 
    {
        eType = GDT_UInt16;
        nBitDepth = 16;
    }
    
    storageChunk = nBitDepth/8;
      
    printf("nBands = %d, nBitDepth = %d\n",nBands,nBitDepth);
    
/* -------------------------------------------------------------------- */
/*      Verify postgresql prefix.                                       */
/* -------------------------------------------------------------------- */
    
    if( !EQUALN(pszFilename,"PG:",3) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                      "%s does not conform to PostgreSQL naming convention,"
                      " PG:*\n" );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Try to establish connection.                                    */
/* -------------------------------------------------------------------- */
    
    pszConnectionString = CPLStrdup(pszFilename);    
    layerName = CPLStrdup(pszFilename);

    int i=0;
    while(pszConnectionString[i] != '\0'){
        
        if(pszConnectionString[i] == '#')
            pszConnectionString[i] = ' ';
        
        i++;
    }
    
    hPGConn = PQconnectdb( pszConnectionString + 3 );
    
    if( hPGConn == NULL || PQstatus(hPGConn) == CONNECTION_BAD )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "PGconnectcb failed.\n%s", 
                  PQerrorMessage(hPGConn) );
        PQfinish(hPGConn);
        hPGConn = NULL;
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Try to establish the database name from the connection          */
/*      string passed.                                                  */
/* -------------------------------------------------------------------- */
        
    if( strstr(pszFilename, "dbname=") != NULL )
    {
        int     i;

        pszDBName = CPLStrdup( strstr(pszFilename, "dbname=") + 7 );

        for( i = 0; pszDBName[i] != '\0'; i++ )
        {
            if( pszDBName[i] == ' ' )                                   
            {
                pszDBName[i] = '\0';
                break;
            }
        }
    }
    else if( getenv( "USER" ) != NULL )
        pszDBName = CPLStrdup( getenv("USER") );
    else
        pszDBName = CPLStrdup( "unknown_dbname" );
        
               
/* -------------------------------------------------------------------- */
/*      Test to see if this database instance has support for the       */
/*      PostGIS Geometry type.  If so, disable sequential scanning      */
/*      so we will get the value of the gist indexes.                   */
/* -------------------------------------------------------------------- */
       
    hResult = PQexec(hPGConn, "BEGIN");

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        PQclear( hResult );

        hResult = PQexec(hPGConn, 
                         "SELECT oid FROM pg_type WHERE typname = 'geometry'" );
    }

    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
        && PQntuples(hResult) > 0 )
    {
        bHavePostGIS = TRUE;
    }
    else {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "You don't seem to have Postgis installed. Check your settings.\n");
        return NULL;         
    }
        
    if( hResult )
        PQclear( hResult );


    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );
    
    
/* -------------------------------------------------------------------- */
/*     try opening Postgis Raster Layer                                 */
/* -------------------------------------------------------------------- */
    
    if( strstr(layerName, "layer=") != NULL )
    {
	pszName = CPLStrdup( strstr(layerName, "layer=") + 6 );
    }
    else        
        pszName = CPLStrdup("unknown_layer");
            
    CPLFree(layerName);
    
    
    // First allocation is small
    szCommand = (char *)CPLMalloc(1024);
        
    hResult = PQexec(hPGConn, "BEGIN");

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        int bTableExists = FALSE;    
    
        PQclear( hResult );
        sprintf( szCommand, 
                 "select b.attname from pg_class a,pg_attribute b where a.oid=b.attrelid and a.relname='%s' and b.attname='raster';",
                 pszName);
                 
        hResult = PQexec(hPGConn,szCommand);
        
        if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
        && PQntuples(hResult) > 0 ){
            bTableExists = TRUE;
        }
        
        if(!bTableExists){
            PQclear( hResult );
            sprintf( szCommand, 
                    "CREATE TABLE %s(raster chip)",
                    pszName);
                    
            hResult = PQexec(hPGConn,szCommand);
        }
    }

    if( hResult && (PQresultStatus(hResult) == PGRES_COMMAND_OK || PQresultStatus(hResult) == PGRES_TUPLES_OK)){
        PQclear( hResult );
    }
    else {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", PQerrorMessage(hPGConn) );
        CPLFree(szCommand);
        return NULL;
    }
    
    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );
        
       
/* -------------------------------------------------------------------- */
/*      Projection, finding SRID                                        */
/* -------------------------------------------------------------------- */    
  
    pszProjection = (char *)poSrcDS->GetProjectionRef();
    SRID = -1;
    
    if( !EQUALN(pszProjection,"GEOGCS",6)
        && !EQUALN(pszProjection,"PROJCS",6)
        && !EQUALN(pszProjection,"+",6)
        && !EQUAL(pszProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Only OGC WKT Projections supported for writing to Postgis.\n"
                "%s not supported.",
                  pszProjection );
    }
    
    
    if( pszProjection[0]=='+')    
        sprintf( szCommand,"SELECT SRID FROM spatial_ref_sys where proj4text=%s",pszProjection);
    else
        sprintf( szCommand,"SELECT SRID FROM spatial_ref_sys where srtext=%s",pszProjection);
            
    hResult = PQexec(hPGConn,szCommand);
        
    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
             && PQntuples(hResult) > 0 ){
        
            SRID = atoi(PQgetvalue(hResult,0,0)); 
        
    }
    
    // Try to find SRID via EPSG number
    if (SRID == -1 && strcmp(pszProjection,"") != 0){
            
            char *buf;
            char epsg[16];
            memset(epsg,0,16);
            char *workingproj = CPLStrdup( pszProjection );
                
            while( (buf = strstr(workingproj,"EPSG")) != 0){
                workingproj = buf+4;
            }
            
            int iChar = 0;
            workingproj = workingproj + 3;
            
            
            while(workingproj[iChar] != '"'){
                epsg[iChar] = workingproj[iChar];
                iChar++;
            }
            
            if(epsg[0] != 0){
                SRID = atoi(epsg); 
            }
    }
    else{
            CPLError( CE_Failure, CPLE_AppDefined,
                "Projection %s not found in spatial_ref_sys table. SRID will be set to -1.\n",
                  pszProjection );
        
            SRID = -1;
    }

    if( hResult )
        PQclear( hResult );
        
           
/* -------------------------------------------------------------------- */
/*      Write palette if there is one.  Technically, I think it is      */
/*      possible to write 16bit palettes for PNG, but we will omit      */
/*      this for now.                                                   */
/* -------------------------------------------------------------------- */
    
    unsigned char	*pPalette = NULL;
    int		bHaveNoData = FALSE;
    double	dfNoDataValue = -1;
    int nbColors = 0,bFoundTrans = FALSE;
    int sizePalette = 0;
    
    if( nColorType == PGCHIP_COLOR_TYPE_PALETTE )
    {
        
        GDALColorEntry  sEntry;
        int		iColor;
        int             offsetColor = -1;
                        
        poCT = poSrcDS->GetRasterBand(1)->GetColorTable();  
        nbColors = poCT->GetColorEntryCount();
                
        sizePalette += sizeof(pgchip_color) * poCT->GetColorEntryCount();
                
        pPalette = (unsigned char *) CPLMalloc(sizePalette);
               
                                               
        for( iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
        {
            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            if( sEntry.c4 != 255 )
                bFoundTrans = TRUE;
            
            pPalette[offsetColor++]  = (unsigned char) sEntry.c1;
            pPalette[offsetColor++]  = (unsigned char) sEntry.c2;
            pPalette[offsetColor++]  = (unsigned char) sEntry.c3;
            
                       
            if( bHaveNoData && iColor == (int) dfNoDataValue ){
                pPalette[offsetColor++]  = 0;
            }
            else{
                pPalette[offsetColor++]  = (unsigned char) sEntry.c4;
            }
        }
    }
    
        
/* -------------------------------------------------------------------- */
/*     Initialize CHIP Structure                                        */
/* -------------------------------------------------------------------- */  
    
    CHIP PGCHIP;
    
    memset(&PGCHIP,0,sizeof(PGCHIP));
    
    PGCHIP.factor = 1.0;
    PGCHIP.endian_hint = 1;
    PGCHIP.compression = nbColors; // To cope with palette extra information  : <header><palette><data>
    PGCHIP.height = nYSize;
    PGCHIP.width = nXSize;
    PGCHIP.SRID = SRID;
    PGCHIP.future[0] = nBands; //nBands is stored in future variable
    PGCHIP.future[1] = nBitDepth; //nBitDepth is stored in future variable
    PGCHIP.future[2] = nColorType; //nBitDepth is stored in future variable
    PGCHIP.future[3] = nbColors; // Useless as we store nbColors in the "compression" integer
    PGCHIP.data = NULL; // Serialized Form
    
    // PGCHIP.size changes if there is a palette.
    // Is calculated by Postgis when inserting anyway
    PGCHIP.size = sizeof(CHIP) + (nYSize * nXSize * storageChunk * nBands) + sizePalette;
    
    switch(storageChunk*nBands){
        case 1 :
            PGCHIP.datatype = 8;
            break;
        case 2 :
            PGCHIP.datatype = 6;
            break;
        case 4 :
            // Postgis sets data_size to 4 by default anyway
            PGCHIP.datatype = 0;
            break;
        default :
             CPLError( CE_Failure, CPLE_AppDefined,"Under development : ERROR STORAGE CHUNK SIZE NOT SUPPORTED\n");
            break;   
    }
    
                
/* -------------------------------------------------------------------- */
/*      Loop over image                                                 */
/* -------------------------------------------------------------------- */
       
    CPLErr      eErr;
    int lineSize = nXSize * storageChunk * nBands;
    
    // allocating data buffer
    GByte *data = (GByte *) CPLMalloc( nYSize * lineSize);
                    
    for( int iLine = 0; iLine < nYSize; iLine++ ){
        for( int iBand = 0; iBand < nBands; iBand++ ){
            
            GDALRasterBand * poBand = poSrcDS->GetRasterBand( iBand+1 );
            
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                     data + (iBand*storageChunk) + iLine * lineSize, 
                                     nXSize, 1, eType,
                                     nBands * storageChunk, 
                                     lineSize );  
         }
    }
    
        
/* -------------------------------------------------------------------- */
/*      Write Header, Palette and Data                                  */
/* -------------------------------------------------------------------- */    
    
    char *result;
    int j=0;
    
    // Calculating result length (*2 -> Hex form, +1 -> end string) 
    int size_result = (PGCHIP.size * 2) + 1;
        
    // memory allocation
    result = (char *) CPLMalloc( size_result * sizeof(char));
            
    // Assign chip
    GByte *header = (GByte *)&PGCHIP;
        
    // Copy header into result string 
    for(j=0;j<(int)sizeof(PGCHIP);j++){
        pgch_deparse_hex( header[j], (unsigned char*)&(result[j*2]));
    }  
    
    // Copy Palette into result string if required
    int offsetPalette = (int)sizeof(PGCHIP) * 2;
    if(nColorType == PGCHIP_COLOR_TYPE_PALETTE && sizePalette>0){
        for(j=0;j<sizePalette;j++){
            pgch_deparse_hex( pPalette[j], (unsigned char *)&result[offsetPalette + (j*2)]);     
        }                   
    }
    
    // Copy data into result string
    int offsetData = offsetPalette + sizePalette * 2;
    for(j=0;j<(nYSize * lineSize);j++){
         pgch_deparse_hex( data[j], (unsigned char *)&result[offsetData + (j*2)]);
    }
   
    
    // end string
    result[offsetData + j*2] = '\0';
    
                                         
/* -------------------------------------------------------------------- */
/*      Inserting Chip                                                  */
/* -------------------------------------------------------------------- */
     
    // Second allocation to cope with data size
    CPLFree(szCommand);
    szCommand = (char *)CPLMalloc(PGCHIP.size*2 + 256);

    hResult = PQexec(hPGConn, "BEGIN");

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
                        
        PQclear( hResult );
        sprintf( szCommand, 
                 "INSERT INTO %s(raster) values('%s')",
                 pszName,result);
                 
                
        hResult = PQexec(hPGConn,szCommand);
    
    }
    
    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK ){
        PQclear( hResult );
    }
    else {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", PQerrorMessage(hPGConn) );
        CPLFree(szCommand);
        return NULL;
    }
        
    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );
            
    CPLFree( szCommand );
    CPLFree( pPalette );
    CPLFree( data );       
    CPLFree( result );
             
    return (GDALDataset *)GDALOpen(pszFilename,GA_Update);
}


/************************************************************************/
/*                          Display CHIP information                    */
/************************************************************************/
void     PGCHIPDataset::printChipInfo(){

    if(this->PGCHIP != NULL){
        printf("\n---< CHIP INFO >----\n");
        printf("CHIP.datatype = %d\n",this->PGCHIP->datatype);
        printf("CHIP.compression = %d\n",this->PGCHIP->compression);
        printf("CHIP.size = %d\n",this->PGCHIP->size);
        printf("CHIP.factor = %f\n",this->PGCHIP->factor);
        printf("CHIP.width = %d\n",this->PGCHIP->width);
        printf("CHIP.height = %d\n",this->PGCHIP->height);
        printf("CHIP.nBands = %d\n",(int)this->PGCHIP->future[0]);
        printf("CHIP.nBitDepth = %d\n",(int)this->PGCHIP->future[1]);
        printf("--------------------\n");
     }
}


/************************************************************************/
/*                          GDALRegister_PGCHIP()                       */
/************************************************************************/
void GDALRegister_PGCHIP(){

    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "PGCHIP" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "PGCHIP" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Postgis CHIP raster" );
                                   
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16" );
         
        poDriver->pfnOpen = PGCHIPDataset::Open;
        poDriver->pfnCreateCopy = PGCHIPCreateCopy;
        
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}







