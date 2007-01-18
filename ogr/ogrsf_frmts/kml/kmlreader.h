/******************************************************************************
 * $Id$
 *
 * Project:  KML Reader
 * Purpose:  Declaration of KMLFeatureClass class.
 * Author:   Christopher Condit, condit@sdsc.edu
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
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
#ifndef KMLREADER_H_INCLUDED
#define KMLREADER_H_INCLUDED

#include "cpl_port.h"
#include "cpl_minixml.h"

/************************************************************************/
/*                           KMLFeatureClass                            */
/************************************************************************/
class CPL_DLL KMLFeatureClass
{
    char        *m_pszName;
    char        *m_pszElementName;
    char        *m_pszGeometryElement;
    int         m_nPropertyCount;    

    int         m_bSchemaLocked;

    int         m_nFeatureCount;

    char        *m_pszExtraInfo;

    int         m_bHaveExtents;
    double      m_dfXMin;
    double      m_dfXMax;
    double      m_dfYMin;
    double      m_dfYMax;

public:
            KMLFeatureClass( const char *pszName = "" );
           ~KMLFeatureClass();

    const char *GetElementName() const;
    void        SetElementName( const char *pszElementName );

    const char *GetGeometryElement() const { return m_pszGeometryElement; }
    void        SetGeometryElement( const char *pszElementName );

    const char *GetName() { return m_pszName; } const
    int         GetPropertyCount() const { return m_nPropertyCount; }        

    int         IsSchemaLocked() const { return m_bSchemaLocked; }
    void        SetSchemaLocked( int bLock ) { m_bSchemaLocked = bLock; }

    const char  *GetExtraInfo() const;
    void        SetExtraInfo( const char * );

    int         GetFeatureCount() const;
    void        SetFeatureCount( int );

    void        SetExtents( double dfXMin, double dfXMax, 
                            double dFYMin, double dfYMax );
    int         GetExtents( double *pdfXMin, double *pdfXMax, 
                            double *pdFYMin, double *pdfYMax );

    CPLXMLNode *SerializeToXML();
    int         InitializeFromXML( CPLXMLNode * );
};

/************************************************************************/
/*                              KMLFeature                              */
/************************************************************************/
class CPL_DLL KMLFeature
{
    KMLFeatureClass *m_poClass;
    char            *m_pszFID;

    int              m_nPropertyCount;
    char           **m_papszProperty;

    char            *m_pszGeometry;

public:
                    KMLFeature( KMLFeatureClass * );
                   ~KMLFeature();

    KMLFeatureClass*    GetClass() const { return m_poClass; }

    void            SetGeometryDirectly( char * );
    const char     *GetGeometry() const { return m_pszGeometry; }
    
    const char      *GetFID() const { return m_pszFID; }
    void             SetFID( const char *pszFID );

    void             Dump( FILE *fp );
};


#endif /* KMLREADER_H_INCLUDED */
