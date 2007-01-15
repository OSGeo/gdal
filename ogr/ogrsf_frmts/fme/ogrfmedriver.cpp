/******************************************************************************
 * $Id$
 *
 * Project:  FMEObjects Translator
 * Purpose:  Implementations of the OGRFMEDriver class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001 Safe Software Inc.
 * All Rights Reserved
 *
 * This software may not be copied or reproduced, in all or in part, 
 * without the prior written consent of Safe Software Inc.
 *
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although
 * Safe Software Incorporated has used considerable efforts in preparing 
 * the Software, Safe Software Incorporated does not warrant the
 * accuracy or completeness of the Software. In no event will Safe Software 
 * Incorporated be liable for damages, including loss of profits or 
 ****************************************************************************/

#include "fme2ogr.h"
#include "cpl_error.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                            OGRFMEDriver                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           ~OGRFMEDriver()                            */
/************************************************************************/

OGRFMEDriver::~OGRFMEDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRFMEDriver::GetName()

{
    return "FMEObjects Gateway";
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRFMEDriver::TestCapability( const char * )

{
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRFMEDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRFMEDataSource    *poDS = new OGRFMEDataSource;

    if( !poDS->Open( pszFilename ) )
    {
        delete poDS;
        return NULL;
    }

    if( bUpdate )
    {
        delete poDS;

        CPLError( CE_Failure, CPLE_OpenFailed,
                  "FMEObjects Driver doesn't support update." );
        return NULL;
    }
    
    return poDS;
}

/************************************************************************/
/*                           RegisterOGRFME()                           */
/************************************************************************/

void RegisterOGRFME()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRFMEDriver );
}
