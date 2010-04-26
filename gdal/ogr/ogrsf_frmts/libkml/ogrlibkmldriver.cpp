/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
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
 *****************************************************************************/

#include "ogr_libkml.h"
#include "cpl_conv.h"
#include "cpl_error.h"

#include <kml/dom.h>

using kmldom::KmlFactory;

/******************************************************************************
 OGRLIBKMLDriver()
******************************************************************************/

OGRLIBKMLDriver::OGRLIBKMLDriver (  )
{
    m_poKmlFactory = KmlFactory::GetFactory (  );

}

/******************************************************************************
 ~OGRLIBKMLDriver()
******************************************************************************/

OGRLIBKMLDriver::~OGRLIBKMLDriver (  )
{
    delete m_poKmlFactory;

}

/******************************************************************************
 GetName()
******************************************************************************/

const char *OGRLIBKMLDriver::GetName (
     )
{

    return "LIBKML";
}

/******************************************************************************
 Open()
******************************************************************************/

OGRDataSource *OGRLIBKMLDriver::Open (
    const char *pszFilename,
    int bUpdate )
{
    OGRLIBKMLDataSource *poDS = new OGRLIBKMLDataSource ( m_poKmlFactory );

    if ( !poDS->Open ( pszFilename, bUpdate ) ) {
        delete poDS;

        poDS = NULL;
    }

    return poDS;
}

/******************************************************************************
 CreateDataSource()
******************************************************************************/

OGRDataSource *OGRLIBKMLDriver::CreateDataSource (
    const char *pszName,
    char **papszOptions )
{
    CPLAssert ( NULL != pszName );
    CPLDebug ( "LIBKML", "Attempt to create: %s", pszName );

    OGRLIBKMLDataSource *poDS = new OGRLIBKMLDataSource ( m_poKmlFactory );

    if ( !poDS->Create ( pszName, papszOptions ) ) {
        delete poDS;

        poDS = NULL;
    }

    return poDS;
}

/******************************************************************************
 DeleteDataSource()

 note: this method recursivly deletes an entire dir if the datasource is a dir
       and all the files are kml or kmz
 
******************************************************************************/

OGRErr OGRLIBKMLDriver::DeleteDataSource (
    const char *pszName )
{

    /***** dir *****/

    VSIStatBufL sStatBuf = { };
    if ( !VSIStatL ( pszName, &sStatBuf ) && VSI_ISDIR ( sStatBuf.st_mode ) ) {

        char **papszDirList = NULL;

        if ( !( papszDirList = VSIReadDir ( pszName ) ) ) {
            if ( VSIRmdir ( pszName ) < 0 )
                return OGRERR_FAILURE;
        }

        int nFiles = CSLCount ( papszDirList );
        int iFile;

        for ( iFile = 0; iFile < nFiles; iFile++ ) {
            if ( OGRERR_FAILURE ==
                 this->DeleteDataSource ( papszDirList[iFile] ) ) {
                CSLDestroy ( papszDirList );
                return OGRERR_FAILURE;
            }
        }

        if ( VSIRmdir ( pszName ) < 0 ) {
            CSLDestroy ( papszDirList );
            return OGRERR_FAILURE;
        }

        CSLDestroy ( papszDirList );
    }

   /***** kml *****/

    else if ( EQUAL ( CPLGetExtension ( pszName ), "kml" ) ) {
        if ( VSIUnlink ( pszName ) < 0 )
            return OGRERR_FAILURE;
    }

    /***** kmz *****/

    else if ( EQUAL ( CPLGetExtension ( pszName ), "kmz" ) ) {
        if ( VSIUnlink ( pszName ) < 0 )
            return OGRERR_FAILURE;
    }

    /***** do not delete other types of files *****/

    else
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}

/******************************************************************************
 TestCapability()
******************************************************************************/

int OGRLIBKMLDriver::TestCapability (
    const char *pszCap )
{
    if ( EQUAL ( pszCap, ODrCCreateDataSource ) )
        return TRUE;

    else if ( EQUAL ( pszCap, ODrCDeleteDataSource ) )
        return bUpdate;

    return FALSE;
}

/******************************************************************************
 RegisterOGRLIBKML()
******************************************************************************/

void RegisterOGRLIBKML (
     )
{
    OGRSFDriverRegistrar::GetRegistrar (  )->
        RegisterDriver ( new OGRLIBKMLDriver );

}
