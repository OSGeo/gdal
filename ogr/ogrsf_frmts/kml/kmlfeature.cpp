#include "kmlreader.h"
#include "cpl_conv.h"
#include "cpl_error.h"

/************************************************************************/
/*                             KMLFeature()                             */
/************************************************************************/
KMLFeature::KMLFeature( KMLFeatureClass *poClass )
{
    CPLAssert( NULL != poClass );

    m_poClass = poClass;
    m_pszFID = NULL;
    m_pszGeometry = NULL;
    
    m_nPropertyCount = 0;
    m_papszProperty = NULL;
}

/************************************************************************/
/*                            ~KMLFeature()                             */
/************************************************************************/
KMLFeature::~KMLFeature()
{
    CPLFree( m_pszFID );    
    
    CPLFree( m_pszGeometry );
}

/************************************************************************/
/*                               SetFID()                               */
/************************************************************************/
void KMLFeature::SetFID( const char *pszFID )
{
    CPLFree( m_pszFID );
    if( pszFID != NULL )
        m_pszFID = CPLStrdup( pszFID );
    else
        m_pszFID = NULL;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/
void KMLFeature::Dump( FILE * fp )
{
    printf( "KMLFeature(%s):\n", m_poClass->GetName() );
    
    if( m_pszFID != NULL )
        printf( "  FID = %s\n", m_pszFID );       

    if( m_pszGeometry )
        printf( "  %s\n", m_pszGeometry );
}

/************************************************************************/
/*                        SetGeometryDirectly()                         */
/************************************************************************/
void KMLFeature::SetGeometryDirectly( char *pszGeometry )
{
    CPLAssert( NULL != pszGeometry );

    // Free current geometry instance
    CPLFree( m_pszGeometry );

    // Assign passed geometry directly, not a copy
    m_pszGeometry = pszGeometry;
}
