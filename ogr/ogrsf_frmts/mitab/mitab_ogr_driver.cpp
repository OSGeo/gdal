/**********************************************************************
 * $Id: mitab_ogr_driver.cpp,v 1.5 1999/12/15 17:05:24 warmerda Exp $
 *
 * Name:     mitab_ogr_driver.cpp
 * Project:  MapInfo Mid/Mif, Tab ogr support
 * Language: C++
 * Purpose:  Implementation of the MIDDATAFile class used to handle
 *           reading/writing of the MID/MIF files
 * Author:   Stephane Villeneuve, s.villeneuve@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, Stephane Villeneuve
 *
 * All rights reserved.  This software may be copied or reproduced, in
 * all or in part, without the prior written consent of its author,
 * Daniel Morissette (danmo@videotron.ca).  However, any material copied
 * or reproduced must bear the original copyright notice (above), this 
 * original paragraph, and the original disclaimer (below).
 * 
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although 
 * considerable efforts have been used in preparing the Software, the 
 * author does not warrant the accuracy or completeness of the Software.
 * In no event will the author be liable for damages, including loss of
 * profits or consequential damages, arising out of the use of the 
 * Software.
 * 
 **********************************************************************
 *
 * $Log: mitab_ogr_driver.cpp,v $
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
 *
 **********************************************************************/

#include "mitab_ogr_driver.h"



/*=======================================================================
 *                 OGRTABDataSource/OGRTABDriver Classes
 *
 * We need one single OGRDataSource/Driver set of classes to handle all
 * the MapInfo file types.  They all deal with the IMapInfoFile abstract
 * class.
 *=====================================================================*/

/************************************************************************/
/*                         OGRTABDataSource()                           */
/************************************************************************/
OGRTABDataSource::OGRTABDataSource( const char * pszNameIn,
                                    IMapInfoFile *poLayerIn )

{
    m_pszName = CPLStrdup( pszNameIn );
    m_poLayer = poLayerIn;
}

/************************************************************************/
/*                         ~OGRTABDataSource()                          */
/************************************************************************/
OGRTABDataSource::~OGRTABDataSource()

{
    CPLFree( m_pszName ); 
    delete m_poLayer;
}


/************************************************************************/
/*                          ~OGRTABDriver()                           */
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

    if( bUpdate )
    {
	return NULL;
    }
       
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    IMapInfoFile *poLayer;

    if( (poLayer = IMapInfoFile::SmartOpen( pszFilename, TRUE )) != NULL )
         return new OGRTABDataSource( pszFilename, poLayer );
 
    return NULL;
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
