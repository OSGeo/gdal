/**********************************************************************
 * $Id: mitab_ogr_driver.cpp,v 1.7 2000/01/26 18:17:00 warmerda Exp $
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
/*                           ~OGRTABDriver()                            */
/************************************************************************/

OGRTABDriver::~OGRTABDriver()

{
}

/************************************************************************/
/*                OGRTABDriver::GetName()                               */
/************************************************************************/

const char *OGRTABDriver::GetName()

{
    return "MapInfo File";
}

/************************************************************************/
/*                  OGRTABDriver::Open()                                */
/************************************************************************/

OGRDataSource *OGRTABDriver::Open( const char * pszFilename,
                                   int bUpdate )

{
    OGRTABDataSource	*poDS;
    
    if( bUpdate )
    {
	return NULL;
    }

    poDS = new OGRTABDataSource();
    if( poDS->Open( pszFilename, TRUE ) )
        return poDS;
    else
    {
        delete poDS;
        return NULL;
    }
}


/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRTABDriver::CreateDataSource( const char * pszName,
                                               char ** papszOptions )

{
    VSIStatBuf	stat;
    OGRTABDataSource *poDS;

/* -------------------------------------------------------------------- */
/*      Verify that the target is a valid directory.                    */
/* -------------------------------------------------------------------- */
    if( VSIStat( pszName, &stat ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File already exists: %s\n"
                  "Can't create new Mapinfo file.\n",
                  pszName );
        
        return NULL;
    }

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
/*                           TestCapability()                           */
/************************************************************************/

int OGRTABDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*              RegisterOGRTAB()                                        */
/************************************************************************/

extern "C"
{

void RegisterOGRTAB()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRTABDriver );
}

}
