/******************************************************************************
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogrgeoconceptdatasource.h"
#include "ogrgeoconceptdriver.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          ~OGRGeoconceptDriver()                      */
/************************************************************************/

OGRGeoconceptDriver::~OGRGeoconceptDriver() {}

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
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
/* -------------------------------------------------------------------- */
/*      We will only consider .gxt and .txt files.                      */
/* -------------------------------------------------------------------- */
    const char* pszExtension = CPLGetExtension(pszFilename);
    if( !EQUAL(pszExtension,"gxt") && !EQUAL(pszExtension,"txt") )
    {
        return nullptr;
    }
#endif

    OGRGeoconceptDataSource  *poDS = new OGRGeoconceptDataSource();

    if( !poDS->Open( pszFilename, true, CPL_TO_BOOL(bUpdate) ) )
    {
        delete poDS;
        return nullptr;
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
    VSIStatBufL  sStat;
    /* int bSingleNewFile = FALSE; */

    if( pszName==nullptr || strlen(pszName)==0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid datasource name (null or empty)");
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Is the target a valid existing directory?                       */
/* -------------------------------------------------------------------- */
    if( VSIStatL( pszName, &sStat ) == 0 )
    {
        if( !VSI_ISDIR(sStat.st_mode) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s is not a valid existing directory.",
                      pszName );
            return nullptr;
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
/*      Return a new OGRDataSource()                                    */
/* -------------------------------------------------------------------- */
    OGRGeoconceptDataSource  *poDS = new OGRGeoconceptDataSource();
    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return nullptr;
    }
    return poDS;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

OGRErr OGRGeoconceptDriver::DeleteDataSource( const char *pszDataSource )

{
    VSIStatBufL sStatBuf;
    static const char * const apszExtensions[] =
        { "gxt", "txt", "gct", "gcm", "gcr", nullptr };

    if( VSIStatL( pszDataSource, &sStatBuf ) != 0 )
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
        for( int iExt=0; apszExtensions[iExt] != nullptr; iExt++ )
        {
            const char *pszFile = CPLResetExtension(pszDataSource,
                                                    apszExtensions[iExt] );
            if( VSIStatL( pszFile, &sStatBuf ) == 0 )
                VSIUnlink( pszFile );
        }
    }
    else if( VSI_ISDIR(sStatBuf.st_mode) )
    {
        char **papszDirEntries = VSIReadDir( pszDataSource );

        for( int iFile = 0;
             papszDirEntries != nullptr && papszDirEntries[iFile] != nullptr;
             iFile++ )
        {
            if( CSLFindString( const_cast<char **>( apszExtensions ),
                               CPLGetExtension(papszDirEntries[iFile])) != -1)
            {
                VSIUnlink( CPLFormFilename( pszDataSource,
                                            papszDirEntries[iFile],
                                            nullptr ) );
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

    if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;

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
"  <Option name='EXTENSION' type='string-select' description='indicates the "
"GeoConcept export file extension. TXT was used by earlier releases of "
"GeoConcept. GXT is currently used.' default='GXT'>"
"    <Value>GXT</Value>"
"    <Value>TXT</Value>"
"  </Option>"
"  <Option name='CONFIG' type='string' description='path to the GCT file that "
"describes the GeoConcept types definitions.'/>"
"</CreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='FEATURETYPE' type='string' description='TYPE.SUBTYPE : "
"defines the feature to be created. The TYPE corresponds to one of the Name "
"found in the GCT file for a type section. The SUBTYPE corresponds to one of "
"the Name found in the GCT file for a sub-type section within the previous "
"type section'/>"
"</LayerCreationOptionList>" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( poDriver );
}
