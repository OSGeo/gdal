/******************************************************************************
 * $Id$
 *
 * Project:  CartoDB Translator
 * Purpose:  Implements OGRCARTODBDriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
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

#include "ogr_cartodb.h"

// g++ -g -Wall -fPIC -shared -o ogr_CARTODB.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/cartodb ogr/ogrsf_frmts/cartodb/*.c* -L. -lgdal -Iogr/ogrsf_frmts/geojson/libjson 

CPL_CVSID("$Id$");

extern "C" void RegisterOGRCartoDB();

/************************************************************************/
/*                        ~OGRCARTODBDriver()                           */
/************************************************************************/

OGRCARTODBDriver::~OGRCARTODBDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRCARTODBDriver::GetName()

{
    return "CartoDB";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRCARTODBDriver::Open( const char * pszFilename, int bUpdate )

{
    if (!EQUALN(pszFilename, "CARTODB:", strlen("CARTODB:")))
        return NULL;

    OGRCARTODBDataSource   *poDS = new OGRCARTODBDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRCARTODBDriver::CreateDataSource( const char * pszName,
                                              char ** /* papszOptions */ )

{
    OGRCARTODBDataSource   *poDS = new OGRCARTODBDataSource();

    if( !poDS->Open( pszName, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "CartoDB driver doesn't support database creation." );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCARTODBDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                         RegisterOGRCARTODB()                         */
/************************************************************************/

void RegisterOGRCartoDB()

{
    OGRSFDriver* poDriver = new OGRCARTODBDriver;
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                "drv_cartodb.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList/>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
"</LayerCreationOptionList>");
    
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime Time" );
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( poDriver );
}

