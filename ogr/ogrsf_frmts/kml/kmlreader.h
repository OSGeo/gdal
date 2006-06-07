#ifndef _KMLREADER_H_INCLUDED
#define _KMLREADER_H_INCLUDED

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


#endif /* _KMLREADER_H_INCLUDED */
