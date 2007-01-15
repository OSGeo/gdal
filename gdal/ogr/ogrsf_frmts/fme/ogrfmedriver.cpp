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
 * consequential damages, arising out of the use of the Software.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2003/03/12 14:45:24  warmerda
 * Don't report that FME driver doesn't support update unless this is
 * an FME supported dataset.
 *
 * Revision 1.1  2002/05/29 20:41:35  warmerda
 * New
 *
 * Revision 1.5  2001/09/07 15:52:23  warmerda
 * fix driver name in debug msg
 *
 * Revision 1.4  2001/07/27 17:27:06  warmerda
 * added CVSID
 *
 * Revision 1.3  2001/07/27 17:24:45  warmerda
 * First phase rewrite for MapGuide
 *
 * Revision 1.2  1999/11/23 15:39:51  warmerda
 * tab expantion
 *
 * Revision 1.1  1999/11/23 15:22:58  warmerda
 * New
 *
 * Revision 1.2  1999/11/10 14:04:44  warmerda
 * updated to new fmeobjects kit
 *
 * Revision 1.1  1999/09/09 20:40:56  warmerda
 * New
 *
 */

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
