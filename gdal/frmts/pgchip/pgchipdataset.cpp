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

/* Define to enable debugging info */
/*#define PGCHIP_DEBUG 1*/

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
    pszTableName = NULL;
    pszDSName = NULL;
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

    if( hPGConn != NULL )
    {
        /* XXX - mloskot: After the connection is closed, valgrind still
         * reports 36 bytes definitely lost, somewhere in the libpq.
         */
        PQfinish( hPGConn );
        hPGConn = NULL;
    }

    CPLFree(pszProjection);
    CPLFree(pszTableName);
    CPLFree(pszDSName);
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
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *PGCHIPDataset::GetProjectionRef(){

    char    szCommand[1024];
    PGresult    *hResult;

    if(SRID == -1)
    {
        return "";
    }

/* -------------------------------------------------------------------- */
/*      Reading proj                                                    */
/* -------------------------------------------------------------------- */

    sprintf( szCommand,"SELECT srtext FROM spatial_ref_sys where SRID=%d",SRID);

    hResult = PQexec(hPGConn,szCommand);

    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
             && PQntuples(hResult) > 0 )
    {
        CPLFree(pszProjection);
        pszProjection = CPLStrdup(PQgetvalue(hResult,0,0)); 
    }

    if( hResult )
        PQclear( hResult );

    return pszProjection;
}

/************************************************************************/
/*                        PGChipOpenConnection()                        */
/************************************************************************/

static
PGconn* PGChipOpenConnection(const char* pszFilename, char** ppszTableName, const char** ppszDSName,
                             int bExitOnMissingTable, int* pbExistTable, int *pbHasNameCol)
{
    char       *pszConnectionString;
    PGconn     *hPGConn;
    PGresult   *hResult = NULL;
    int         i=0;
    int         bHavePostGIS;
    char       *pszTableName;
    char        szCommand[1024];

    if( pszFilename == NULL || !EQUALN(pszFilename,"PG:",3))
        return NULL;

    pszConnectionString = CPLStrdup(pszFilename);

/* -------------------------------------------------------------------- */
/*      Try to establish connection.                                    */
/* -------------------------------------------------------------------- */
    while(pszConnectionString[i] != '\0')
    {

        if(pszConnectionString[i] == '#')
            pszConnectionString[i] = ' ';
        else if(pszConnectionString[i] == '%')
        {
            pszConnectionString[i] = '\0';
            break;
        }
        i++;
    }

    hPGConn = PQconnectdb( pszConnectionString + 3 );
    if( hPGConn == NULL || PQstatus(hPGConn) == CONNECTION_BAD )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "PGconnectcb failed.\n%s", 
                  PQerrorMessage(hPGConn) );
        PQfinish(hPGConn);
        CPLFree(pszConnectionString);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Test to see if this database instance has support for the       */
/*      PostGIS Geometry type.  If so, disable sequential scanning      */
/*      so we will get the value of the gist indexes.                   */
/* -------------------------------------------------------------------- */

    hResult = PQexec(hPGConn, 
                         "SELECT oid FROM pg_type WHERE typname = 'geometry'" );

    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
        && PQntuples(hResult) > 0 )
    {
        bHavePostGIS = TRUE;
    }

    if( hResult )
        PQclear( hResult );

    if(!bHavePostGIS){
        CPLError( CE_Failure, CPLE_AppDefined, 
                      "Can't find geometry type, is Postgis correctly installed ?\n");
        PQfinish(hPGConn);
        CPLFree(pszConnectionString);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*  Try opening the layer                                               */
/* -------------------------------------------------------------------- */

    if( strstr(pszFilename, "layer=") != NULL )
    {
        pszTableName = CPLStrdup( strstr(pszFilename, "layer=") + 6 );
    }
    else
    {
        pszTableName = CPLStrdup("unknown_layer");
    }

    char* pszDSName = strstr(pszTableName, "%name=");
    if (pszDSName)
    {
        *pszDSName = '\0';
        pszDSName += 6;
    }
    else
        pszDSName = "unknown_name";

    sprintf( szCommand, 
             "select b.attname from pg_class a,pg_attribute b where a.oid=b.attrelid and a.relname='%s' and b.attname='raster';",
              pszTableName);

    hResult = PQexec(hPGConn,szCommand);

    if( ! (hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK && PQntuples(hResult) > 0) )
    {
        if (pbExistTable)
            *pbExistTable = FALSE;

        if (bExitOnMissingTable)
        {
            if (hResult)
                PQclear( hResult );
            CPLFree(pszConnectionString);
            CPLFree(pszTableName);
            PQfinish(hPGConn);
            return NULL;
        }
    }
    else
    {
        if (pbExistTable)
            *pbExistTable = TRUE;

        sprintf( szCommand, 
                "select b.attname from pg_class a,pg_attribute b where a.oid=b.attrelid and a.relname='%s' and b.attname='name';",
                pszTableName);

        if (hResult)
            PQclear( hResult );
        hResult = PQexec(hPGConn,szCommand);
        if (PQresultStatus(hResult) == PGRES_TUPLES_OK && PQntuples(hResult) > 0)
        {
            if (pbHasNameCol)
                *pbHasNameCol = TRUE;
        }
        else
        {
            if (pbHasNameCol)
                *pbHasNameCol = FALSE;
        }
    }

    if (hResult)
        PQclear( hResult );

    if (ppszTableName)
        *ppszTableName = pszTableName;
    else
        CPLFree(pszTableName);

    if (ppszDSName)
        *ppszDSName = pszDSName;

    CPLFree(pszConnectionString);

    return hPGConn;
}



/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PGCHIPDataset::Open( GDALOpenInfo * poOpenInfo ){

    char                szCommand[1024];
    PGresult            *hResult = NULL;
    PGCHIPDataset       *poDS = NULL;
    char                *chipStringHex;

    unsigned char       *chipdata;
    int                 t;
    char                *pszTableName = NULL;
    const char          *pszDSName = NULL;
    int                  bHasNameCol = FALSE;

    PGconn* hPGConn = PGChipOpenConnection(poOpenInfo->pszFilename, &pszTableName, &pszDSName, TRUE, NULL, &bHasNameCol);

    if( hPGConn == NULL)
        return NULL;

    poDS = new PGCHIPDataset();
    poDS->hPGConn = hPGConn;
    poDS->pszTableName = pszTableName;
    poDS->pszDSName = CPLStrdup(pszDSName);

/* -------------------------------------------------------------------- */
/*      Read the chip                                                   */
/* -------------------------------------------------------------------- */

    hResult = PQexec(poDS->hPGConn, "BEGIN");
    
    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
        PQclear( hResult );
        if (bHasNameCol)
            sprintf( szCommand, 
                    "SELECT raster FROM %s WHERE name = '%s' LIMIT 1 ",
                    poDS->pszTableName, poDS->pszDSName);
        else
            sprintf( szCommand, 
                    "SELECT raster FROM %s LIMIT 1",
                    poDS->pszTableName);

        hResult = PQexec(poDS->hPGConn,szCommand);
    }

    if( !hResult || PQresultStatus(hResult) != PGRES_TUPLES_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", PQerrorMessage(poDS->hPGConn) );
        if (hResult)
            PQclear( hResult );
        delete poDS;
        return NULL;
    }

    if (PQntuples(hResult) == 0)
    {
        PQclear( hResult );
        delete poDS;
        return NULL;
    }

    chipStringHex = PQgetvalue(hResult, 0, 0);

    if (chipStringHex == NULL)
    {
        PQclear( hResult );
        delete poDS;
        return NULL;
    }

    int stringlen = strlen((char *)chipStringHex);
        
    // Allocating memory for chip
    chipdata = (unsigned char *) CPLMalloc(stringlen/2);
                      	
    for (t=0;t<stringlen/2;t++){
	    chipdata[t] = parse_hex( &chipStringHex[t*2]) ;
    }
    
    // Chip assigment
    poDS->PGCHIP = (CHIP *)chipdata;
    poDS->SRID = poDS->PGCHIP->SRID;
    
    if (poDS->PGCHIP->bvol.xmin != 0 && poDS->PGCHIP->bvol.ymin != 0 &
        poDS->PGCHIP->bvol.xmax != 0 && poDS->PGCHIP->bvol.ymax != 0)
    {
        poDS->bGeoTransformValid = TRUE;

        poDS->adfGeoTransform[0] = poDS->PGCHIP->bvol.xmin;
        poDS->adfGeoTransform[3] = poDS->PGCHIP->bvol.ymax;
        poDS->adfGeoTransform[1] = (poDS->PGCHIP->bvol.xmax - poDS->PGCHIP->bvol.xmin) / poDS->PGCHIP->width;
        poDS->adfGeoTransform[5] = - (poDS->PGCHIP->bvol.ymax - poDS->PGCHIP->bvol.ymin) / poDS->PGCHIP->height;
    }
    
#ifdef PGCHIP_DEBUG
    poDS->printChipInfo(*(poDS->PGCHIP));
#endif

    PQclear( hResult );

    hResult = PQexec(poDS->hPGConn, "COMMIT");
    PQclear( hResult );
    
/* -------------------------------------------------------------------- */
/*      Verify that there's no unknown field set.                       */
/* -------------------------------------------------------------------- */

    if ( poDS->PGCHIP->future[0] != 0 ||
         poDS->PGCHIP->future[1] != 0 ||
         poDS->PGCHIP->future[2] != 0 ||
         poDS->PGCHIP->future[3] != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported CHIP format (future field bytes != 0)\n");
        delete poDS;
        return NULL;
    }

    if ( poDS->PGCHIP->compression != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Compressed CHIP unsupported\n");
        delete poDS;
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Set some information from the file that is of interest.         */
/* -------------------------------------------------------------------- */

    poDS->nRasterXSize = poDS->PGCHIP->width;
    poDS->nRasterYSize = poDS->PGCHIP->height;
    poDS->nBands = 1; // (int)poDS->PGCHIP->future[0];
    //poDS->nBitDepth = (int)poDS->PGCHIP->future[1];
    poDS->nColorType = PGCHIP_COLOR_TYPE_GRAY; //(int)poDS->PGCHIP->future[2];

    switch (poDS->PGCHIP->datatype)
    {
        case 5: // 24bit integer
            poDS->nBitDepth = 24;
            break;
        case 6: // 16bit integer
            poDS->nBitDepth = 16;
            break;
        case 8:
            poDS->nBitDepth = 8;
            break;
        case 1: // float32
        case 7: // 16bit ???
        case 101: // float32 (NDR)
        case 105: // 24bit integer (NDR)
        case 106: // 16bit integer (NDR)
        case 107: // 16bit ??? (NDR)
        case 108: // 8bit ??? (NDR) [ doesn't make sense ]
        default :
             CPLError( CE_Failure, CPLE_AppDefined,
                "Under development : CHIP datatype %d unsupported.\n",
                poDS->PGCHIP->datatype);
            break;   
    }
    
        
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
        poDS->SetBand( iBand+1, new PGCHIPRasterBand( poDS, iBand+1 ) );
    
    
/* -------------------------------------------------------------------- */
/*      Is there a palette?  Note: we should also read back and         */
/*      apply transparency values if available.                         */
/* -------------------------------------------------------------------- */
    CPLAssert (poDS->nColorType != PGCHIP_COLOR_TYPE_PALETTE);
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

    
    PGconn          *hPGConn;
    char            *pszTableName = NULL;
    const char      *pszDSName = NULL;
    char*            pszCommand;
    PGresult        *hResult;
    char            *pszProjection;
    int              SRID;
    GDALColorTable  *poCT= NULL;
    int              bTableExists = FALSE;
    int              bHasNameCol = FALSE;

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

    hPGConn = PGChipOpenConnection(pszFilename, &pszTableName, &pszDSName, FALSE, &bTableExists, &bHasNameCol);

    /* Check Postgis connection string */
    if( hPGConn == NULL){
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Cannont connect to %s.\n", pszFilename);
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
      
    //printf("nBands = %d, nBitDepth = %d\n",nBands,nBitDepth);
    
    pszCommand = (char*)CPLMalloc(1024);

    if(!bTableExists){
        sprintf( pszCommand, 
                "CREATE TABLE %s(raster chip, name varchar)",
                pszTableName);

        hResult = PQexec(hPGConn,pszCommand);
        bHasNameCol = TRUE;
    }
    else
    {
        if (bHasNameCol)
            sprintf( pszCommand, 
                    "DELETE FROM %s WHERE name = '%s'", pszTableName, pszDSName);
        else
            sprintf( pszCommand, 
                    "DELETE FROM %s", pszTableName);

        hResult = PQexec(hPGConn,pszCommand);
    }

    if( hResult && (PQresultStatus(hResult) == PGRES_COMMAND_OK || PQresultStatus(hResult) == PGRES_TUPLES_OK)){
        PQclear( hResult );
    }
    else {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", PQerrorMessage(hPGConn) );
        PQfinish( hPGConn );
        hPGConn = NULL;
        return NULL;
    }

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
        sprintf( pszCommand,"SELECT SRID FROM spatial_ref_sys where proj4text=%s",pszProjection);
    else
        sprintf( pszCommand,"SELECT SRID FROM spatial_ref_sys where srtext=%s",pszProjection);
            
    hResult = PQexec(hPGConn,pszCommand);
        
    if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK 
             && PQntuples(hResult) > 0 ){
        
            SRID = atoi(PQgetvalue(hResult,0,0)); 
        
    }
    
    // Try to find SRID via EPSG number
    if (SRID == -1 && strcmp(pszProjection,"") != 0){
            
            char *buf;
            char epsg[16];
            memset(epsg,0,16);
            const char *workingproj = pszProjection;
                
            while( (buf = strstr(workingproj,"EPSG")) != 0){
                workingproj = buf+4;
            }
            
            int iChar = 0;
            workingproj = workingproj + 3;
            
            
            while(workingproj[iChar] != '"' && iChar < 15){
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
    size_t sizePalette = 0;
    
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
    PGCHIP.compression = 0; // nbColors; // To cope with palette extra information  : <header><palette><data>
    PGCHIP.height = nYSize;
    PGCHIP.width = nXSize;
    PGCHIP.SRID = SRID;
    PGCHIP.future[0] = 0; // nBands; //nBands is stored in future variable
    PGCHIP.future[1] = 0; // nBitDepth; //nBitDepth is stored in future variable
    PGCHIP.future[2] = 0; // nColorType; //nBitDepth is stored in future variable
    PGCHIP.future[3] = 0; // nbColors; // Useless as we store nbColors in the "compression" integer
    PGCHIP.data = NULL; // Serialized Form
    
    double adfGeoTransform[6];
    if (GDALGetGeoTransform(poSrcDS, adfGeoTransform) == CE_None)
    {
        if (adfGeoTransform[2] == 0 &&
            adfGeoTransform[4] == 0 &&
            adfGeoTransform[5] < 0)
        {
            PGCHIP.bvol.xmin = adfGeoTransform[0];
            PGCHIP.bvol.ymax = adfGeoTransform[3];
            PGCHIP.bvol.xmax = adfGeoTransform[0] + nXSize * adfGeoTransform[1];
            PGCHIP.bvol.ymin = adfGeoTransform[3] + nYSize * adfGeoTransform[5];
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Could not write geotransform.\n" );
        }
    }

    // PGCHIP.size changes if there is a palette.
    // Is calculated by Postgis when inserting anyway
    PGCHIP.size = sizeof(CHIP) - sizeof(void*) + (nYSize * nXSize * storageChunk * nBands) + sizePalette;
    
    switch(nBitDepth)
    {
        case 8:
            PGCHIP.datatype = 8; // NDR|XDR ?
            break;
        case 16:
            PGCHIP.datatype = 6; // NDR|XDR ?
            break;
        case 24:
            PGCHIP.datatype = 5; // NDR|XDR ?
            break;
        default:
             CPLError( CE_Failure, CPLE_AppDefined,"Under development : ERROR STORAGE CHUNK SIZE NOT SUPPORTED\n");
            break;   
    }
    
#ifdef PGCHIP_DEBUG
    PGCHIPDataset::printChipInfo(PGCHIP);
#endif
                
/* -------------------------------------------------------------------- */
/*      Loop over image                                                 */
/* -------------------------------------------------------------------- */
       
    CPLErr      eErr;
    size_t lineSize = nXSize * storageChunk * nBands;
    
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
    size_t j=0;
    
    // Calculating result length (*2 -> Hex form, +1 -> end string) 
    size_t size_result = (PGCHIP.size * 2) + 1;
        
    // memory allocation
    result = (char *) CPLMalloc( size_result * sizeof(char));
            
    // Assign chip
    GByte *header = (GByte *)&PGCHIP;
        
    // Copy header into result string 
    for(j=0; j<sizeof(PGCHIP)-sizeof(void*); j++) {
        pgch_deparse_hex( header[j], (unsigned char*)&(result[j*2]));
    }  
    
    // Copy Palette into result string if required
    size_t offsetPalette = (sizeof(PGCHIP)-sizeof(void*)) * 2;
    if(nColorType == PGCHIP_COLOR_TYPE_PALETTE && sizePalette>0){
        for(j=0;j<sizePalette;j++){
            pgch_deparse_hex( pPalette[j], (unsigned char *)&result[offsetPalette + (j*2)]);     
        }                   
    }
    
    // Copy data into result string
    size_t offsetData = offsetPalette + sizePalette * 2;
    for(j=0;j<(nYSize * lineSize);j++){
         pgch_deparse_hex( data[j], (unsigned char *)&result[offsetData + (j*2)]);
    }
   
    
    // end string
    result[offsetData + j*2] = '\0';
    
                                         
/* -------------------------------------------------------------------- */
/*      Inserting Chip                                                  */
/* -------------------------------------------------------------------- */
     
    // Second allocation to cope with data size
    CPLFree(pszCommand);
    pszCommand = (char *)CPLMalloc(PGCHIP.size*2 + 256);

    hResult = PQexec(hPGConn, "BEGIN");

    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK )
    {
                        
        PQclear( hResult );
        if (bHasNameCol)
            sprintf( pszCommand, 
                    "INSERT INTO %s(raster, name) values('%s', '%s')",
                    pszTableName,result, pszDSName);
        else
            sprintf( pszCommand, 
                    "INSERT INTO %s(raster) values('%s')",
                    pszTableName,result);
                 
                
        hResult = PQexec(hPGConn,pszCommand);
    
    }
    
    CPLFree(pszTableName); pszTableName = NULL;
    
    if( hResult && PQresultStatus(hResult) == PGRES_COMMAND_OK ){
        PQclear( hResult );
    }
    else {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", PQerrorMessage(hPGConn) );
        CPLFree(pszCommand); pszCommand = NULL;
        PQfinish( hPGConn );
        hPGConn = NULL;
        return NULL;
    }
        
    hResult = PQexec(hPGConn, "COMMIT");
    PQclear( hResult );
            
    CPLFree( pszCommand ); pszCommand = NULL;
    CPLFree( pPalette ); pPalette = NULL;
    CPLFree( data ); data = NULL;
    CPLFree( result ); result = NULL;

    PQfinish( hPGConn );
    hPGConn = NULL;

    return (GDALDataset *)GDALOpen(pszFilename,GA_Update);
}


/************************************************************************/
/*                          Display CHIP information                    */
/************************************************************************/
void     PGCHIPDataset::printChipInfo(const CHIP& c){

    //if(this->PGCHIP != NULL){
        printf("\n---< CHIP INFO >----\n");
        printf("CHIP.datatype = %d\n", c.datatype);
        printf("CHIP.compression = %d\n", c.compression);
        printf("CHIP.size = %d\n", c.size);
        printf("CHIP.factor = %f\n", c.factor);
        printf("CHIP.width = %d\n", c.width);
        printf("CHIP.height = %d\n", c.height);
        //printf("CHIP.future[0] (nBands?) = %d\n", (int)c.future[0]);
        //printf("CHIP.future[1] (nBitDepth?) = %d\n", (int)c.future[1]);
        printf("--------------------\n");
     //}
}

/************************************************************************/
/*                              PGCHIPDelete                            */
/************************************************************************/
static CPLErr PGCHIPDelete(const char* pszFilename)
{
    return CE_None;
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
        poDriver->pfnDelete = PGCHIPDelete;
        
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

