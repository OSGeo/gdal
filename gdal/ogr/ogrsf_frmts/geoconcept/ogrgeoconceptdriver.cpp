/******************************************************************************
 * $Id: ogrgeoconceptdriver.cpp 
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeoconceptDriver class.
 * Author:   Didier Richard, didier.richard@ign.fr
 * Language: C++
 *
 ******************************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
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

#include "ogrgeoconceptdatasource.h"
#include "ogrgeoconceptdriver.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id: ogrgeoconceptdriver.cpp 00000 2007-11-03 10:42:48Z drichard $");

/************************************************************************/
/*                          ~OGRGeoconceptDriver()                      */
/************************************************************************/

OGRGeoconceptDriver::~OGRGeoconceptDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRGeoconceptDriver::GetName()

{
    return "Geoconcept";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRGeoconceptDriver::Open( const char* pszFilename,
                                          int bUpdate )

{
    OGRGeoconceptDataSource  *poDS;

/* -------------------------------------------------------------------- */
/*      We will only consider .gxt and .txt files.                      */
/* -------------------------------------------------------------------- */
    const char* pszExtension = CPLGetExtension(pszFilename);
    if( !EQUAL(pszExtension,"gxt") && !EQUAL(pszExtension,"txt") )
    {
        return NULL;
    }

    poDS = new OGRGeoconceptDataSource();

    if( !poDS->Open( pszFilename, TRUE, bUpdate ) )
    {
        delete poDS;
        return NULL;
    }
    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/*                                                                      */
/* Options (-dsco) :                                                    */
/*   EXTENSION=GXT|TXT (default GXT)                                    */
/************************************************************************/

OGRDataSource *OGRGeoconceptDriver::CreateDataSource( const char* pszName,
                                                      char** papszOptions )

{
    VSIStatBuf  stat;
    /* int bSingleNewFile = FALSE; */

    if( pszName==NULL || strlen(pszName)==0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid datasource name (null or empty)");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Is the target a valid existing directory?                       */
/* -------------------------------------------------------------------- */
    if( CPLStat( pszName, &stat ) == 0 )
    {
        if( !VSI_ISDIR(stat.st_mode) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s is not a valid existing directory.",
                      pszName );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Does it end with the extension .gxt indicating the user likely  */
/*      wants to create a single file set?                              */
/* -------------------------------------------------------------------- */
    else if(
             EQUAL(CPLGetExtension(pszName),"gxt")  ||
             EQUAL(CPLGetExtension(pszName),"txt")
           )
    {
        /* bSingleNewFile = TRUE; */
    }

/* -------------------------------------------------------------------- */
/*      Otherwise try to create a new directory.                        */
/* -------------------------------------------------------------------- */
    else
    {
        VSIStatBuf  sStat;

        if( VSIStat( pszName, &sStat ) == 0 )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attempt to create datasource named %s, "
                      "but that is an existing directory.",
                      pszName );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Return a new OGRDataSource()                                    */
/* -------------------------------------------------------------------- */
    OGRGeoconceptDataSource  *poDS = NULL;

    poDS = new OGRGeoconceptDataSource();
    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    return poDS;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

OGRErr OGRGeoconceptDriver::DeleteDataSource( const char *pszDataSource )

{
    int iExt;
    VSIStatBuf sStatBuf;
    static const char *apszExtensions[] = 
        { "gxt", "txt", "gct", "gcm", "gcr", NULL };

    if( VSIStat( pszDataSource, &sStatBuf ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s does not appear to be a file or directory.",
                  pszDataSource );

        return OGRERR_FAILURE;
    }

    if( VSI_ISREG(sStatBuf.st_mode) 
        && (
            EQUAL(CPLGetExtension(pszDataSource),"gxt") ||
            EQUAL(CPLGetExtension(pszDataSource),"txt")
           ) )
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

    return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoconceptDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          RegisterOGRGeoconcept()                     */
/************************************************************************/

void RegisterOGRGeoconcept()

{
    OGRSFDriver* poDriver = new OGRGeoconceptDriver;
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "gxt txt" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='EXTENSION' type='string-select' description='indicates the GeoConcept export file extension. TXT was used by earlier releases of GeoConcept. GXT is currently used.' default='GXT'>"
"    <Value>GXT</Value>"
"    <Value>TXT</Value>"
"  </Option>"
"  <Option name='CONFIG' type='string' description='path to the GCT file that describes the GeoConcept types definitions.'/>"
"</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='FEATURETYPE' type='string' description='TYPE.SUBTYPE : defines the feature to be created. The TYPE corresponds to one of the Name found in the GCT file for a type section. The SUBTYPE corresponds to one of the Name found in the GCT file for a sub-type section within the previous type section'/>"
"</LayerCreationOptionList>");

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( poDriver );
}
