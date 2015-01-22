/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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

#include "ogrshape.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static int OGRShapeDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    /* Files not ending with .shp, .shx or .dbf are not handled by this driver */
    if( !poOpenInfo->bStatOK )
        return FALSE;
    if( poOpenInfo->bIsDirectory )
        return -1; /* unsure */
    if( poOpenInfo->fpL != NULL &&
        (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "SHP") ||
         EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "SHX")) )
    {
        return memcmp(poOpenInfo->pabyHeader, "\x00\x00\x27\x0A", 4) == 0 ||
               memcmp(poOpenInfo->pabyHeader, "\x00\x00\x27\x0D", 4) == 0;
    }
    if( poOpenInfo->fpL != NULL && EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "DBF") )
    {
        if( poOpenInfo->nHeaderBytes < 32 )
            return FALSE;
        const GByte* pabyBuf = poOpenInfo->pabyHeader;
        unsigned int nHeadLen = pabyBuf[8] + pabyBuf[9]*256;
        unsigned int nRecordLength = pabyBuf[10] + pabyBuf[11]*256;
        if( nHeadLen < 32 )
            return FALSE;
        if( (nHeadLen % 32) != 0 && (nHeadLen % 32) != 1 )
            return FALSE;
        unsigned int nFields = (nHeadLen - 32) / 32;
        if( nRecordLength < nFields )
            return FALSE;
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRShapeDriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRShapeDataSource  *poDS;

    if( OGRShapeDriverIdentify(poOpenInfo) == FALSE )
        return NULL;

    poDS = new OGRShapeDataSource();

    if( !poDS->Open( poOpenInfo, TRUE ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRShapeDriverCreate( const char * pszName,
                                          CPL_UNUSED int nBands,
                                          CPL_UNUSED int nXSize,
                                          CPL_UNUSED int nYSize,
                                          CPL_UNUSED GDALDataType eDT,
                                          CPL_UNUSED char **papszOptions )
{
    VSIStatBuf  stat;
    int         bSingleNewFile = FALSE;

/* -------------------------------------------------------------------- */
/*      Is the target a valid existing directory?                       */
/* -------------------------------------------------------------------- */
    if( CPLStat( pszName, &stat ) == 0 )
    {
        if( !VSI_ISDIR(stat.st_mode) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s is not a directory.\n",
                      pszName );
            
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Does it end in the extension .shp indicating the user likely    */
/*      wants to create a single file set?                              */
/* -------------------------------------------------------------------- */
    else if( EQUAL(CPLGetExtension(pszName),"shp") 
             || EQUAL(CPLGetExtension(pszName),"dbf") )
    {
        bSingleNewFile = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise try to create a new directory.                        */
/* -------------------------------------------------------------------- */
    else
    {
        if( VSIMkdir( pszName, 0755 ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to create directory %s\n"
                      "for shapefile datastore.\n",
                      pszName );
            
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Return a new OGRDataSource()                                    */
/* -------------------------------------------------------------------- */
    OGRShapeDataSource  *poDS = NULL;

    poDS = new OGRShapeDataSource();
    
    GDALOpenInfo oOpenInfo( pszName, GA_Update );
    if( !poDS->Open( &oOpenInfo, FALSE, bSingleNewFile ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           Delete()                                   */
/************************************************************************/

static CPLErr OGRShapeDriverDelete( const char *pszDataSource )

{
    int iExt;
    VSIStatBufL sStatBuf;
    static const char *apszExtensions[] = 
        { "shp", "shx", "dbf", "sbn", "sbx", "prj", "idm", "ind", 
          "qix", "cpg", NULL };

    if( VSIStatL( pszDataSource, &sStatBuf ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s does not appear to be a file or directory.",
                  pszDataSource );

        return CE_Failure;
    }

    if( VSI_ISREG(sStatBuf.st_mode) 
        && (EQUAL(CPLGetExtension(pszDataSource),"shp")
            || EQUAL(CPLGetExtension(pszDataSource),"shx")
            || EQUAL(CPLGetExtension(pszDataSource),"dbf")) )
    {
        for( iExt=0; apszExtensions[iExt] != NULL; iExt++ )
        {
            const char *pszFile = CPLResetExtension(pszDataSource,
                                                    apszExtensions[iExt] );
            if( VSIStatL( pszFile, &sStatBuf ) == 0 )
                VSIUnlink( pszFile );
        }
    }
    else if( VSI_ISDIR(sStatBuf.st_mode) )
    {
        char **papszDirEntries = CPLReadDir( pszDataSource );
        int  iFile;

        for( iFile = 0; 
             papszDirEntries != NULL && papszDirEntries[iFile] != NULL;
             iFile++ )
        {
            if( CSLFindString( (char **) apszExtensions, 
                               CPLGetExtension(papszDirEntries[iFile])) != -1)
            {
                VSIUnlink( CPLFormFilename( pszDataSource, 
                                            papszDirEntries[iFile], 
                                            NULL ) );
            }
        }

        CSLDestroy( papszDirEntries );

        VSIRmdir( pszDataSource );
    }

    return CE_None;
}

/************************************************************************/
/*                          RegisterOGRShape()                          */
/************************************************************************/

void RegisterOGRShape()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "ESRI Shapefile" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "ESRI Shapefile" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "ESRI Shapefile" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "shp" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "shp dbf" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_shape.html" );

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='ENCODING' type='string' description='to override the encoding interpretation of the DBF with any encoding supported by CPLRecode or to \"\" to avoid any recoding'/>"
"  <Option name='DBF_DATE_LAST_UPDATE' type='string' description='Modification date to write in DBF header with YYYY-MM-DD format'/>"
"</OpenOptionList>");

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList/>" );
        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='SHPT' type='string-select' description='type of shape' default='automatically detected'>"
"    <Value>POINT</Value>"
"    <Value>ARC</Value>"
"    <Value>POLYGON</Value>"
"    <Value>MULTIPOINT</Value>"
"    <Value>POINTZ</Value>"
"    <Value>ARCZ</Value>"
"    <Value>POLYGONZ</Value>"
"    <Value>MULTIPOINTZ</Value>"
"    <Value>NONE</Value>"
"    <Value>NULL</Value>"
"  </Option>"
"  <Option name='2GB_LIMIT' type='boolean' description='Restrict .shp and .dbf to 2GB' default='NO'/>"
"  <Option name='ENCODING' type='string' description='DBF encoding' default='LDID/87'/>"
"  <Option name='RESIZE' type='boolean' description='To resize fields to their optimal size.' default='NO'/>"
"  <Option name='SPATIAL_INDEX' type='boolean' description='To create a spatial index.' default='NO'/>"
"  <Option name='DBF_DATE_LAST_UPDATE' type='string' description='Modification date to write in DBF header with YYYY-MM-DD format'/>"
"</LayerCreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRShapeDriverOpen;
        poDriver->pfnIdentify = OGRShapeDriverIdentify;
        poDriver->pfnCreate = OGRShapeDriverCreate;
        poDriver->pfnDelete = OGRShapeDriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
