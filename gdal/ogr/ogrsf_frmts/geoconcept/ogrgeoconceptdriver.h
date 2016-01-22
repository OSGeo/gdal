/**********************************************************************
 * $Id: ogrgeoconceptdriver.h 
 *
 * Name:     ogrgeoconceptdriver.h
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeoconceptDriver class.
 * Language: C++
 *
 **********************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
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
 **********************************************************************/

#include "ogrsf_frmts.h"

#ifndef _GEOCONCEPT_OGR_DRIVER_H_INCLUDED_
#define _GEOCONCEPT_OGR_DRIVER_H_INCLUDED_

/************************************************************************/
/*                             OGRGeoconceptDriver                      */
/************************************************************************/

class OGRGeoconceptDriver : public OGRSFDriver
{
public:
                   ~OGRGeoconceptDriver();

    const char*    GetName( );
    OGRDataSource* Open( const char* pszName, int bUpdate = FALSE );
    int            TestCapability( const char* pszCap );
    OGRDataSource* CreateDataSource( const char* pszName, char** papszOptions = NULL );
    OGRErr         DeleteDataSource( const char* pszName );
};

#endif /* _GEOCONCEPT_OGR_DRIVER_H_INCLUDED_ */
