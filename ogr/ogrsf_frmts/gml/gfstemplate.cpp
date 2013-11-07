/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GML GFS template management
 * Author:   Alessandro Furieri, a.furitier@lqt.it
 *
 ******************************************************************************
 * Copyright (c) 2011, Alessandro Furieri
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
 *
 ******************************************************************************
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * Developed for Faunalia ( http://www.faunalia.it) with funding from
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ****************************************************************************/

#include "gmlreaderp.h"
#include "ogr_gml.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        GFSTemplateItem                               */
/************************************************************************/

class GFSTemplateItem
{
private:
    char            *m_pszName;
    int             n_nItemCount;
    int             n_nGeomCount;
    GFSTemplateItem *pNext;
public:
                    GFSTemplateItem( const char *pszName );
                    ~GFSTemplateItem();
    const char      *GetName() { return m_pszName; }
    void            Update( int b_has_geom );
    int             GetCount() { return n_nItemCount; }
    int             GetGeomCount() { return n_nGeomCount; }
    void            SetNext( GFSTemplateItem *pN ) { pNext = pN; }
    GFSTemplateItem *GetNext() { return pNext; }
};

/***************************************************/
/*               gmlUpdateFeatureClasses()         */
/***************************************************/

 void gmlUpdateFeatureClasses ( GFSTemplateList *pCC,
                                GMLReader *pReader,
                                int *pbSequentialLayers )
{
/* updating the FeatureClass list */
    int clIdx;
    for (clIdx = 0; clIdx < pReader->GetClassCount(); clIdx++)
    {
        GMLFeatureClass* poClass = pReader->GetClass( clIdx );
        if (poClass != NULL)
            poClass->SetFeatureCount( 0 );
    }
    int m_bValid = FALSE;
    GFSTemplateItem *pItem = pCC->GetFirst();
    while ( pItem != NULL )
    {
    /* updating Classes */
        GMLFeatureClass* poClass = pReader->GetClass( pItem->GetName() );
        if (poClass != NULL)
        {
            poClass->SetFeatureCount( pItem->GetCount() );
            if ( pItem->GetGeomCount() != 0 && poClass->GetGeometryPropertyCount() == 0 )
                poClass->AddGeometryProperty( new GMLGeometryPropertyDefn( "", wkbUnknown ) );
            m_bValid = TRUE;
        }
        pItem = pItem->GetNext();
    }
    if ( m_bValid == TRUE && pCC->HaveSequentialLayers() == TRUE )
        *pbSequentialLayers = TRUE;
}

/***************************************************/
/*       GMLReader::ReArrangeTemplateClasses()     */
/***************************************************/

int GMLReader::ReArrangeTemplateClasses ( GFSTemplateList *pCC )
{
/* rearranging the final FeatureClass list [SEQUENTIAL] */
    int m_nSavedClassCount = GetClassCount();

/* saving the previous FeatureClass list */
    GMLFeatureClass **m_papoSavedClass = (GMLFeatureClass **)
                    CPLMalloc( sizeof(void*) * m_nSavedClassCount );
    int clIdx;
    for (clIdx = 0; clIdx < GetClassCount(); clIdx++)
    {
    /* tranferring any previous FeatureClass */
        m_papoSavedClass[clIdx] = m_papoClass[clIdx];
    }

/* cleaning the previous FeatureClass list */
    SetClassListLocked( FALSE );
    CPLFree( m_papoClass );
    m_nClassCount = 0;
    m_papoClass = NULL;

    GFSTemplateItem *pItem = pCC->GetFirst();
    while ( pItem != NULL )
    {
    /*
    * re-inserting any required FeatureClassup
    * accordingly to actual SEQUENTIAL layout
    */
        GMLFeatureClass* poClass = NULL;
        for( int iClass = 0; iClass < m_nSavedClassCount; iClass++ )
        {
            GMLFeatureClass* poItem = m_papoSavedClass[iClass];
            if( EQUAL(poItem->GetName(), pItem->GetName() ))
            {
                poClass = poItem;
                break;
            }
        }
        if (poClass != NULL)
        {
            if (poClass->GetFeatureCount() > 0)
                AddClass( poClass );
        }
        pItem = pItem->GetNext();
    }
    SetClassListLocked( TRUE );

/* destroying the saved List and any unused FeatureClass */
    for( int iClass = 0; iClass < m_nSavedClassCount; iClass++ )
    {
        int bUnused = TRUE;
        GMLFeatureClass* poClass = m_papoSavedClass[iClass];
        for( int iClass2 = 0; iClass2 < m_nClassCount; iClass2++ )
        {
            if (m_papoClass[iClass2] == poClass)
            {
                bUnused = FALSE;
                break;
            }
        }
        if ( bUnused == TRUE )
            delete poClass;
    }
    CPLFree( m_papoSavedClass );
    return 1;
}

/***************************************************/
/*       GMLReader::PrescanForTemplate()           */
/***************************************************/

int GMLReader::PrescanForTemplate ()
{
    int iCount = 0;
    GMLFeature      *poFeature;
    //int bSequentialLayers = TRUE;
    GFSTemplateList *pCC = new GFSTemplateList();

    /* processing GML features */
    while( (poFeature = NextFeature()) != NULL )
    {
        GMLFeatureClass *poClass = poFeature->GetClass();
        const CPLXMLNode* const * papsGeomList = poFeature->GetGeometryList();
        int b_has_geom = FALSE;

        if( papsGeomList != NULL )
        {
            int i = 0;
            const CPLXMLNode *psNode = papsGeomList[i];
            while( psNode != NULL )
            {
                b_has_geom = TRUE;
                i++;
                psNode = papsGeomList[i];
            }
        }
        pCC->Update( poClass->GetElementName(), b_has_geom );

        delete poFeature;
    }

    gmlUpdateFeatureClasses( pCC, this, &m_bSequentialLayers );
    if ( m_bSequentialLayers == TRUE )
        ReArrangeTemplateClasses( pCC );
    iCount = pCC->GetClassCount();
    delete pCC;
    CleanupParser();
    return iCount > 0;
}


/***************************************************/
/*                 GFSTemplateList()               */
/***************************************************/

GFSTemplateList::GFSTemplateList( void )
{
    m_bSequentialLayers = TRUE;
    pFirst = NULL;
    pLast = NULL;
}

/***************************************************/
/*                 GFSTemplateList()               */
/***************************************************/

GFSTemplateList::~GFSTemplateList()
{
    GFSTemplateItem *pNext;
    GFSTemplateItem *pItem = pFirst;
    while ( pItem != NULL )
    {
        pNext = pItem->GetNext();
        delete pItem;
        pItem = pNext;
    }
}

/***************************************************/
/*             GFSTemplateList::Insert()           */
/***************************************************/

GFSTemplateItem *GFSTemplateList::Insert( const char *pszName )
{
    GFSTemplateItem *pItem;
    pItem = new GFSTemplateItem( pszName );

    /* inserting into the linked list */
    if( pFirst == NULL )
        pFirst = pItem;
    if( pLast != NULL )
        pLast->SetNext( pItem );
    pLast = pItem;
    return pItem;
}

/***************************************************/
/*             GFSTemplateList::Update()           */
/***************************************************/

void GFSTemplateList::Update( const char *pszName, int bHasGeom )
{
    GFSTemplateItem *pItem;

    if( pFirst == NULL )
    {
    /* empty List: first item */
        pItem = Insert( pszName );
        pItem->Update( bHasGeom );
        return;
    }
    if( EQUAL(pszName, pLast->GetName() ) )
    {
    /* continuing with the current Class Item */
        pLast->Update( bHasGeom );
        return;
    }

    pItem = pFirst;
    while( pItem != NULL )
    {
        if( EQUAL(pszName, pItem->GetName() ))
        {
        /* Class Item previously declared: NOT SEQUENTIAL */
            m_bSequentialLayers = FALSE;
            pItem->Update( bHasGeom );
            return;
        }
        pItem = pItem->GetNext();
    }

    /* inserting a new Class Item */
    pItem = Insert( pszName );
    pItem->Update( bHasGeom );
}


/***************************************************/
/*          GFSTemplateList::GetClassCount()       */
/***************************************************/

int GFSTemplateList::GetClassCount( )
{
    int iCount = 0;
    GFSTemplateItem *pItem;

    pItem = pFirst;
    while( pItem != NULL )
    {
        iCount++;
        pItem = pItem->GetNext();
    }

    return iCount;
}

/***************************************************/
/*                 GFSTemplateItem()               */
/***************************************************/

GFSTemplateItem::GFSTemplateItem( const char *pszName )
{
    m_pszName = CPLStrdup( pszName );
    n_nItemCount = 0;
    n_nGeomCount = 0;
    pNext = NULL;
}

/***************************************************/
/*                ~GFSTemplateItem()               */
/***************************************************/

GFSTemplateItem::~GFSTemplateItem()
{
    CPLFree(m_pszName);
}

/***************************************************/
/*             GFSTemplateItem::Update()           */
/***************************************************/

void GFSTemplateItem::Update( int bHasGeom )
{
    n_nItemCount++;
    if( bHasGeom == TRUE )
        n_nGeomCount++;
}
