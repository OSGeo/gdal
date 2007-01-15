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
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.11  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.10  2001/11/01 17:07:30  warmerda
 * lots of changes, including support for executing commands
 *
 * Revision 1.9  1999/11/18 19:02:20  warmerda
 * expanded tabs
 *
 * Revision 1.8  1999/06/26 05:27:08  warmerda
 * Separate out GetWKTFromSRSId static method for use of SFCTable
 *
 * Revision 1.7  1999/06/10 19:18:22  warmerda
 * added support for the spatial ref schema rowset
 *
 * Revision 1.6  1999/06/10 14:39:25  warmerda
 * Added use of OGIS Features Tables schema rowset
 *
 * Revision 1.5  1999/06/09 21:09:36  warmerda
 * updated docs
 *
 * Revision 1.4  1999/06/09 21:00:10  warmerda
 * added support for spatial table identification
 *
 * Revision 1.3  1999/06/08 19:05:27  warmerda
 * fixed up documentation
 *
 * Revision 1.2  1999/06/08 16:07:10  warmerda
 * Removed short doc for CreateSFCTable.
 *
 * Revision 1.1  1999/06/08 03:50:25  warmerda
 * New
 *
 */

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



