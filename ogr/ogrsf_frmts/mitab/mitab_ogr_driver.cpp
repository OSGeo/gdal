/**********************************************************************
 * $Id: mitab_middatafile.cpp,v 1.1 1999/11/08 04:16:07 stephane Exp $
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
 * $Log: mitab_middatafile.cpp,v $
 * Revision 1.1  1999/11/08 04:16:07  stephane
 * First Revision
 *
 *
 **********************************************************************/

#include "mitab_ogr_driver.h"



/*=======================================================================
 *
 *                 OGRTab Class
 *=====================================================================*/

/************************************************************************/
/*                         OGRTABDataSource()                           */
/************************************************************************/
OGRTABDataSource::OGRTABDataSource( const char * pszNameIn,
                                        TABFile *poLayerIn )

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
    return "MapInfo TABFile";
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
    TABFile	*poLayer;

    poLayer = new TABFile();
    if (poLayer->Open(pszFilename,"r") ==0)
         return new OGRTABDataSource( pszFilename, poLayer );
    else
    {
	delete poLayer;
	return NULL;
    }
}

/*=======================================================================
 *
 *                 OGRMID Class
 *=====================================================================*/


/************************************************************************/
/*                         OGRMIDDataSource()                           */
/************************************************************************/
OGRMIDDataSource::OGRMIDDataSource( const char * pszNameIn,
                                        MIFFile *poLayerIn )

{
    m_pszName = CPLStrdup( pszNameIn );
    m_poLayer = poLayerIn;
}

/************************************************************************/
/*                        ~OGRMIDDataSource()                         */
/************************************************************************/
OGRMIDDataSource::~OGRMIDDataSource()

{
    CPLFree( m_pszName );
    delete m_poLayer;
}

/************************************************************************/
/*                          ~OGRMIDDriver()                           */
/************************************************************************/

OGRMIDDriver::~OGRMIDDriver()

{
}

/************************************************************************/
/*                OGRMIDDriver::GetName()                               */
/************************************************************************/

const char *OGRMIDDriver::GetName()

{
    return "MapInfo Mid/Mif File";
}

/************************************************************************/
/*                  OGRMIDDriver::Open()                                */
/************************************************************************/

OGRDataSource *OGRMIDDriver::Open( const char * pszFilename,
                                     int bUpdate )

{

    if( bUpdate )
    {
	return NULL;
    }
       
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    MIFFile	*poLayer;

    poLayer = new MIFFile();
    if (poLayer->Open(pszFilename,"r") == 0)
      return new OGRMIDDataSource( pszFilename, poLayer );
    else
    {
	delete poLayer;
	return NULL;
    }
}

/************************************************************************/
/*              RegisterOGRTAB() and RegisterOGRMID()                   */
/************************************************************************/

extern "C"
{

void RegisterOGRTAB()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRTABDriver );
}

void RegisterOGRMIF()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRMIDDriver );
}

}
