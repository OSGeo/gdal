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
/*                          ~OGRShapeDriver()                           */
/************************************************************************/

OGRShapeDriver::~OGRShapeDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRShapeDriver::GetName()

{
    return "ESRI Shapefile";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRShapeDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGRShapeDataSource  *poDS;

    poDS = new OGRShapeDataSource();

    /* If no layer was found, return NULL otherwise how will we allow any other driver */
    /* that is directory oriented to work? I just don't see how we can know the */
    /* directory is for shapefiles if there aren't any in it (#2686) */
    if( !poDS->Open( pszFilename, bUpdate, TRUE )
        || poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRShapeDriver::CreateDataSource( const char * pszName,
                                                 char **papszOptions )

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
    
    if( !poDS->Open( pszName, TRUE, FALSE, bSingleNewFile ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

OGRErr OGRShapeDriver::DeleteDataSource( const char *pszDataSource )

{
    int iExt;
    VSIStatBufL sStatBuf;
    static const char *apszExtensions[] = 
        { "shp", "shx", "dbf", "sbn", "sbx", "prj", "idm", "ind", 
          "qix", NULL };

    if( VSIStatL( pszDataSource, &sStatBuf ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s does not appear to be a file or directory.",
                  pszDataSource );

        return OGRERR_FAILURE;
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

    return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRShapeDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          RegisterOGRShape()                          */
/************************************************************************/

void RegisterOGRShape()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRShapeDriver );
}

