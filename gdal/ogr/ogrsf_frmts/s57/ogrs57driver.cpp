/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements OGRS57Driver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_s57.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

S57ClassRegistrar *OGRS57Driver::poRegistrar = NULL;
static void* hS57RegistrarMutex = NULL;

/************************************************************************/
/*                            OGRS57Driver()                            */
/************************************************************************/

OGRS57Driver::OGRS57Driver()

{
}

/************************************************************************/
/*                           ~OGRS57Driver()                            */
/************************************************************************/

OGRS57Driver::~OGRS57Driver()

{
    if( poRegistrar != NULL )
    {
        delete poRegistrar;
        poRegistrar = NULL;
    }
    
    if( hS57RegistrarMutex != NULL )
    {
        CPLDestroyMutex(hS57RegistrarMutex);
        hS57RegistrarMutex = NULL;
    }
}

/************************************************************************/
/*                          OGRS57DriverIdentify()                      */
/************************************************************************/

static int OGRS57DriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 10 )
        return FALSE;
    const char* pachLeader = (const char* )poOpenInfo->pabyHeader;
    if( (pachLeader[5] != '1' && pachLeader[5] != '2'
                && pachLeader[5] != '3' )
            || pachLeader[6] != 'L'
            || (pachLeader[8] != '1' && pachLeader[8] != ' ') )
    {
        return FALSE;
    }
    return strstr( pachLeader, "DSID") != NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *OGRS57Driver::Open( GDALOpenInfo* poOpenInfo )

{
    OGRS57DataSource    *poDS;

    if( !OGRS57DriverIdentify(poOpenInfo) )
        return NULL;

    poDS = new OGRS57DataSource(poOpenInfo->papszOpenOptions);
    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    if( poDS && poOpenInfo->eAccess == GA_Update )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "S57 Driver doesn't support update." );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

GDALDataset *OGRS57Driver::Create( const char * pszName,
                                int nBands, int nXSize, int nYSize, GDALDataType eDT,
                                char **papszOptions )
{
    OGRS57DataSource *poDS = new OGRS57DataSource();

    if( poDS->Create( pszName, papszOptions ) )
        return poDS;
    else
    {
        delete poDS;
        return NULL;
    }
}

/************************************************************************/
/*                          GetS57Registrar()                           */
/************************************************************************/

S57ClassRegistrar *OGRS57Driver::GetS57Registrar()

{
/* -------------------------------------------------------------------- */
/*      Instantiate the class registrar if possible.                    */
/* -------------------------------------------------------------------- */
    CPLMutexHolderD(&hS57RegistrarMutex);

    if( poRegistrar == NULL )
    {
        poRegistrar = new S57ClassRegistrar();

        if( !poRegistrar->LoadInfo( NULL, NULL, FALSE ) )
        {
            delete poRegistrar;
            poRegistrar = NULL;
        }
    }

    return poRegistrar;
}

/************************************************************************/
/*                           RegisterOGRS57()                           */
/************************************************************************/

void RegisterOGRS57()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "S57" ) == NULL )
    {
        poDriver = new OGRS57Driver();

        poDriver->SetDescription( "S57" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "IHO S-57 (ENC)" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "000" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_s57.html" );

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='" S57O_UPDATES "' type='string-select' description='Should update files be incorporated into the base data on the fly' default='APPLY'>"
"    <Value>APPLY</Value>"
"    <Value>IGNORE</Value>"
"  </Option>"
"  <Option name='" S57O_SPLIT_MULTIPOINT "' type='boolean' description='Should multipoint soundings be split into many single point sounding features' default='NO'/>"
"  <Option name='" S57O_ADD_SOUNDG_DEPTH "' type='boolean' description='Should a DEPTH attribute be added on SOUNDG features and assign the depth of the sounding' default='NO'/>"
"  <Option name='" S57O_RETURN_PRIMITIVES "' type='boolean' description='Should all the low level geometry primitives be returned as special IsolatedNode, ConnectedNode, Edge and Face layers' default='NO'/>"
"  <Option name='" S57O_PRESERVE_EMPTY_NUMBERS "' type='boolean' description='If enabled, numeric attributes assigned an empty string as a value will be preserved as a special numeric value' default='NO'/>"
"  <Option name='" S57O_LNAM_REFS "' type='boolean' description='Should LNAM and LNAM_REFS fields be attached to features capturing the feature to feature relationships in the FFPT group of the S-57 file' default='YES'/>"
"  <Option name='" S57O_RETURN_LINKAGES "' type='boolean' description='Should additional attributes relating features to their underlying geometric primtives be attached' default='NO'/>"
"  <Option name='" S57O_RECODE_BY_DSSI "' type='boolean' description='Should attribute values be recoded to UTF-8 from the character encoding specified in the S57 DSSI record.' default='NO'/>"
"</OpenOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRS57Driver::Open;
        poDriver->pfnIdentify = OGRS57DriverIdentify;
        poDriver->pfnCreate = OGRS57Driver::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
