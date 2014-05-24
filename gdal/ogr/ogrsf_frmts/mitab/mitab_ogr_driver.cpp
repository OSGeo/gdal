/**********************************************************************
 * $Id: mitab_ogr_driver.cpp,v 1.11 2005-05-21 03:15:18 fwarmerdam Exp $
 *
 * Name:     mitab_ogr_driver.cpp
 * Project:  MapInfo Mid/Mif, Tab ogr support
 * Language: C++
 * Purpose:  Implementation of the MIDDATAFile class used to handle
 *           reading/writing of the MID/MIF files
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Stephane Villeneuve
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log: mitab_ogr_driver.cpp,v $
 * Revision 1.11  2005-05-21 03:15:18  fwarmerdam
 * Removed unused stat buffer.
 *
 * Revision 1.10  2004/02/27 21:06:03  fwarmerdam
 * Better support for "single file" creation ... don't allow other layers to
 * be created.  But *do* single file to satisfy the first layer creation request
 * made.  Also, allow creating a datasource "on" an existing directory.
 *
 * Revision 1.9  2003/03/20 15:57:46  warmerda
 * Added delete datasource support
 *
 * Revision 1.8  2001/01/22 16:03:58  warmerda
 * expanded tabs
 *
 * Revision 1.7  2000/01/26 18:17:00  warmerda
 * reimplement OGR driver
 *
 * Revision 1.6  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.5  1999/12/15 17:05:24  warmerda
 * Only create OGRTABDataSource if SmartOpen() result is non-NULL.
 *
 * Revision 1.4  1999/12/15 16:28:17  warmerda
 * fixed a few type problems
 *
 * Revision 1.3  1999/12/14 02:22:29  daniel
 * Merged TAB+MIF DataSource/Driver into ane using IMapInfoFile class
 *
 * Revision 1.2  1999/11/12 02:44:36  stephane
 * added comment, change Register name.
 *
 * Revision 1.1  1999/11/08 21:05:51  svillene
 * first revision
 *
 * Revision 1.1  1999/11/08 04:16:07  stephane
 * First Revision
 *
 **********************************************************************/

#include "mitab_ogr_driver.h"


/************************************************************************/
/*                  OGRTABDriverIdentify()                              */
/************************************************************************/

static int OGRTABDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    /* Files not ending with .tab, .mif or .mid are not handled by this driver */
    if( !poOpenInfo->bStatOK )
        return FALSE;
    if( poOpenInfo->bIsDirectory )
        return -1; /* unsure */
    if( poOpenInfo->fpL == NULL )
        return FALSE;
    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MIF") ||
        EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MID") )
    {
        return TRUE;
    }
    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "TAB") )
    {
        for( int i = 0; i < poOpenInfo->nHeaderBytes; i++)
        {
            const char* pszLine = (const char*)poOpenInfo->pabyHeader + i;
            if (EQUALN(pszLine, "Fields", 6))
                return TRUE;
            else if (EQUALN(pszLine, "create view", 11))
                return TRUE;
            else if (EQUALN(pszLine, "\"\\IsSeamless\" = \"TRUE\"", 21))
                return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                  OGRTABDriver::Open()                                */
/************************************************************************/

static GDALDataset *OGRTABDriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRTABDataSource    *poDS;

    if( OGRTABDriverIdentify(poOpenInfo) == FALSE )
    {
        return NULL;
    }

    if( poOpenInfo->eAccess == GA_Update )
    {
        return NULL;
    }

    poDS = new OGRTABDataSource();
    if( poDS->Open( poOpenInfo, TRUE ) )
        return poDS;
    else
    {
        delete poDS;
        return NULL;
    }
}


/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRTABDriverCreate( const char * pszName,
                                   int nBands, int nXSize, int nYSize, GDALDataType eDT,
                                   char **papszOptions )

{
    OGRTABDataSource *poDS;

/* -------------------------------------------------------------------- */
/*      Try to create the data source.                                  */
/* -------------------------------------------------------------------- */
    poDS = new OGRTABDataSource();
    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

static CPLErr OGRTABDriverDelete( const char *pszDataSource )

{
    int iExt;
    VSIStatBuf sStatBuf;
    static const char *apszExtensions[] = 
        { "mif", "mid", "tab", "map", "ind", "dat", "id", NULL };

    if( VSIStat( pszDataSource, &sStatBuf ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s does not appear to be a file or directory.",
                  pszDataSource );

        return CE_Failure;
    }

    if( VSI_ISREG(sStatBuf.st_mode) 
        && (EQUAL(CPLGetExtension(pszDataSource),"mif")
            || EQUAL(CPLGetExtension(pszDataSource),"mid")
            || EQUAL(CPLGetExtension(pszDataSource),"tab")) )
    {
        for( iExt=0; apszExtensions[iExt] != NULL; iExt++ )
        {
            const char *pszFile = CPLResetExtension(pszDataSource,
                                                    apszExtensions[iExt] );
            if( VSIStat( pszFile, &sStatBuf ) == 0 )
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
/*              RegisterOGRTAB()                                        */
/************************************************************************/

extern "C"
{

void RegisterOGRTAB()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "MapInfo File" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "MapInfo File" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "MapInfo File" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "tab mif mid" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_mitab.html" );

        poDriver->pfnOpen = OGRTABDriverOpen;
        poDriver->pfnIdentify = OGRTABDriverIdentify;
        poDriver->pfnCreate = OGRTABDriverCreate;
        poDriver->pfnDelete = OGRTABDriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

}
