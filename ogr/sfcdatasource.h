/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SFCDatasource class, client side abstraction for OLE DB
 *           SFCOM datasource based on OLE DB CDataSource template. 
 * Author:   Frank Warmerdam, warmerda@home.com
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

class OGRGeometry;
class SFCTable;

/************************************************************************/
/*                            SFCDataSource                             */
/************************************************************************/

/**
 * Simplified SFCOM Datasource/Data Source interface.
 *
 * This class should make it easier to create a datasource, and also
 * abstract the identification of spatial tables.
 
   Questions:<p>

  <ul>

   <li> Do we want to use CPL error handling mechanisms?  This would help
     capture meaningful text messages for errors within the client side
     API, and make access to IErrorInfo (is that the right name) information
     easier.

   <li> Should there be an SFCDatasourceManager class to help in identifying all
     SFCOM datasources installed on a system?

   <li> Should our methods use BSTR or unicode strings instead of regular char?

   <li> Should we make SFC classes into COM classes/interfaces?

   <li> Should this class have explicit support for the spatial reference
        system table?

   <li> Should we be limiting application exposure to all the oledb_sup.h
        definitions?
        
  </ul>

 */

class SFCDataSource : public CDataSource
{
    int		nSRCount;
    char	**papszSRName;

  public:

    /** Reinitialize table list, and other info */
    void	Reinitialize();

#ifdef notdef    
    /** Get number of spatial tables available from this datasource */
    int		GetSFTableCount();

    /** Get name of a given spatial table. */
    const char	*GetSFTableName( int );
#endif
    
    SFCTable	*CreateSFCTable( const char * pszTablename,
                                 OGRGeometry * poFilterGeometry = NULL,
                                 const char * pszFilterOperator = NULL );
};

#endif /* ndef SFCDATASOURCE_H_INCLUDED */
