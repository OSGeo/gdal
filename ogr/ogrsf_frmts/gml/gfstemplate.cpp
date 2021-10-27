/******************************************************************************
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

#include "cpl_port.h"
#include "gmlreaderp.h"
#include "ogr_gml.h"

#include <cstddef>

#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "gmlreader.h"
#include "ogr_core.h"

CPL_CVSID("$Id$")

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
    explicit        GFSTemplateItem( const char *pszName );
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
    // Updating the FeatureClass list.
    for( int clIdx = 0; clIdx < pReader->GetClassCount(); clIdx++ )
    {
        GMLFeatureClass* poClass = pReader->GetClass( clIdx );
        if (poClass != nullptr)
            poClass->SetFeatureCount( 0 );
    }
    bool bValid = false;
    GFSTemplateItem *pItem = pCC->GetFirst();
    while ( pItem != nullptr )
    {
        // Updating Classes.
        GMLFeatureClass* poClass = pReader->GetClass( pItem->GetName() );
        if (poClass != nullptr)
        {
            poClass->SetFeatureCount( pItem->GetCount() );
            if ( pItem->GetGeomCount() != 0 &&
                 poClass->GetGeometryPropertyCount() == 0 )
                poClass->AddGeometryProperty(
                    new GMLGeometryPropertyDefn( "", "", wkbUnknown, -1, true));
            bValid = true;
        }
        pItem = pItem->GetNext();
    }
    if ( bValid && pCC->HaveSequentialLayers() )
        *pbSequentialLayers = TRUE;
}

/***************************************************/
/*       GMLReader::ReArrangeTemplateClasses()     */
/***************************************************/

bool GMLReader::ReArrangeTemplateClasses ( GFSTemplateList *pCC )
{
    // Rearranging the final FeatureClass list [SEQUENTIAL].
    // TODO(schwehr): Why the m_ for m_nSavedClassCount?  Not a member.
    const int m_nSavedClassCount = GetClassCount();

    // Saving the previous FeatureClass list.
    GMLFeatureClass **m_papoSavedClass = static_cast<GMLFeatureClass **>(
                    CPLMalloc(sizeof(void*) * m_nSavedClassCount));

    for( int clIdx = 0; clIdx < GetClassCount(); clIdx++ )
    {
        // Transferring any previous FeatureClass.
        m_papoSavedClass[clIdx] = m_papoClass[clIdx];
    }

    // Cleaning the previous FeatureClass list.
    SetClassListLocked(false);
    CPLFree(m_papoClass);
    m_nClassCount = 0;
    m_papoClass = nullptr;

    GFSTemplateItem *pItem = pCC->GetFirst();
    while ( pItem != nullptr )
    {
        // Re-inserting any required FeatureClassup
        // accordingly to actual SEQUENTIAL layout.
        GMLFeatureClass* poClass = nullptr;
        for( int iClass = 0; iClass < m_nSavedClassCount; iClass++ )
        {
            GMLFeatureClass* poItem = m_papoSavedClass[iClass];
            if( EQUAL(poItem->GetName(), pItem->GetName() ))
            {
                poClass = poItem;
                break;
            }
        }
        if (poClass != nullptr)
        {
            if (poClass->GetFeatureCount() > 0)
                AddClass(poClass);
        }
        pItem = pItem->GetNext();
    }
    SetClassListLocked(true);

    // Destroying the saved List and any unused FeatureClass.
    for( int iClass = 0; iClass < m_nSavedClassCount; iClass++ )
    {
        bool bUnused = true;
        GMLFeatureClass* poClass = m_papoSavedClass[iClass];
        for( int iClass2 = 0; iClass2 < m_nClassCount; iClass2++ )
        {
            if (m_papoClass[iClass2] == poClass)
            {
                bUnused = false;
                break;
            }
        }
        if ( bUnused )
            delete poClass;
    }
    CPLFree(m_papoSavedClass);
    return true;
}

/***************************************************/
/*       GMLReader::PrescanForTemplate()           */
/***************************************************/

bool GMLReader::PrescanForTemplate()
{
    GMLFeature *poFeature = nullptr;
    GFSTemplateList *pCC = new GFSTemplateList();

    // Processing GML features.
    while( (poFeature = NextFeature()) != nullptr )
    {
        GMLFeatureClass *poClass = poFeature->GetClass();
        const CPLXMLNode* const * papsGeomList = poFeature->GetGeometryList();
        bool b_has_geom = false;

        if( papsGeomList != nullptr )
        {
            int i = 0;
            const CPLXMLNode *psNode = papsGeomList[i];
            while( psNode != nullptr )
            {
                b_has_geom = true;
                i++;
                psNode = papsGeomList[i];
            }
        }
        pCC->Update( poClass->GetElementName(), b_has_geom );

        delete poFeature;
    }

    gmlUpdateFeatureClasses(pCC, this, &m_nHasSequentialLayers);
    if ( m_nHasSequentialLayers == TRUE )
        ReArrangeTemplateClasses(pCC);
    const int iCount = pCC->GetClassCount();
    delete pCC;
    CleanupParser();
    return iCount > 0;
}

/***************************************************/
/*                 GFSTemplateList()               */
/***************************************************/

GFSTemplateList::GFSTemplateList() :
    m_bSequentialLayers(true),
    pFirst(nullptr),
    pLast(nullptr)
{}

/***************************************************/
/*                 GFSTemplateList()               */
/***************************************************/

GFSTemplateList::~GFSTemplateList()
{
    GFSTemplateItem *pItem = pFirst;
    while ( pItem != nullptr )
    {
        GFSTemplateItem *pNext = pItem->GetNext();
        delete pItem;
        pItem = pNext;
    }
}

/***************************************************/
/*             GFSTemplateList::Insert()           */
/***************************************************/

GFSTemplateItem *GFSTemplateList::Insert( const char *pszName )
{
    GFSTemplateItem *pItem = new GFSTemplateItem( pszName );

    // Inserting into the linked list.
    if( pFirst == nullptr )
        pFirst = pItem;
    if( pLast != nullptr )
        pLast->SetNext( pItem );
    pLast = pItem;
    return pItem;
}

/***************************************************/
/*             GFSTemplateList::Update()           */
/***************************************************/

void GFSTemplateList::Update( const char *pszName, int bHasGeom )
{
    GFSTemplateItem *pItem = nullptr;

    if( pFirst == nullptr )
    {
        // Empty List: first item.
        pItem = Insert( pszName );
        pItem->Update( bHasGeom );
        return;
    }
    if( EQUAL(pszName, pLast->GetName() ) )
    {
        // Continuing with the current Class Item.
        pLast->Update( bHasGeom );
        return;
    }

    pItem = pFirst;
    while( pItem != nullptr )
    {
        if( EQUAL(pszName, pItem->GetName() ))
        {
            // Class Item previously declared: NOT SEQUENTIAL.
            m_bSequentialLayers = false;
            pItem->Update( bHasGeom );
            return;
        }
        pItem = pItem->GetNext();
    }

    // Inserting a new Class Item.
    pItem = Insert( pszName );
    pItem->Update( bHasGeom );
}

/***************************************************/
/*          GFSTemplateList::GetClassCount()       */
/***************************************************/

int GFSTemplateList::GetClassCount()
{
    int iCount = 0;
    GFSTemplateItem *pItem = pFirst;
    while( pItem != nullptr )
    {
        iCount++;
        pItem = pItem->GetNext();
    }

    return iCount;
}

/***************************************************/
/*                 GFSTemplateItem()               */
/***************************************************/

GFSTemplateItem::GFSTemplateItem( const char *pszName ) :
    m_pszName(CPLStrdup( pszName )),
    n_nItemCount(0),
    n_nGeomCount(0),
    pNext(nullptr)
{}

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
