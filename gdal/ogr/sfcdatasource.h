/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SFCDatasource class, client side abstraction for OLE DB
 *           SFCOM datasource based on OLE DB CDataSource template. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Les Technologies SoftMap Inc.
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
 ****************************************************************************/

#ifndef SFCDATASOURCE_H_INCLUDED
#define SFCDATASOURCE_H_INCLUDED

#include <atldbcli.h>
#include "oledbgis.h"

class OGRGeometry;
class SFCTable;

/************************************************************************/
/*                            SFCDataSource                             */
/************************************************************************/

/**
 * Simplified SFCOM DataSource interface.
 *
 * This class is based on the ATL CDataSource, and adds a convenient
 * way to instantiate an SFCTable for a particular table in the data source.
 * This class is intended to include methods for getting a list of spatial
 * tables for the data source, but this hasn't been implemented yet.
 
   Questions:<p>

  <ul>

   <li> Do we want to use CPL error handling mechanisms?  This would help
     capture meaningful text messages for errors within the client side
     API, and make access to IErrorInfo (is that the right name) information
     easier.  Perhaps the client side code should throw exceptions instead?

   <li> Should our methods use BSTR or unicode strings instead of regular char?

  </ul>

 */

class SFCDataSource : public CDataSource
{
    int         bSessionEstablished;
    CSession    oSession;
    
    int         nSRInitialized;
    char        **papszSRName;

    int         UseOGISFeaturesTables();
    void        UseTables();

    void        AddSFTable( const char * );
    int         EstablishSession();

  public:

                SFCDataSource();
                ~SFCDataSource();

    void        Reinitialize();

    int         GetSFTableCount();

    const char  *GetSFTableName( int );
    
    SFCTable    *CreateSFCTable( const char * pszTablename,
                                 OGRGeometry * poFilterGeometry = NULL,
                                 DBPROPOGISENUM eOperator
                                    = DBPROP_OGIS_ENVELOPE_INTERSECTS );

    SFCTable    *Execute( const char *pszCommand,
                          OGRGeometry * poFilterGeometry,
                          DBPROPOGISENUM eOperator
                                = DBPROP_OGIS_ENVELOPE_INTERSECTS );
                          
    SFCTable    *Execute( const char *pszCommand,
                          DBPROPSET* pPropSet = NULL,
                          DBPARAMS *pParams = NULL );

    char        *GetWKTFromSRSId( int nSRS_ID );
    static char *GetWKTFromSRSId( CSession *, int nSRS_ID );
};

#endif /* ndef SFCDATASOURCE_H_INCLUDED */



