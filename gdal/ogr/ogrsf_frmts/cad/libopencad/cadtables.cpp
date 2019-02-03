/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/
#include "cadtables.h"
#include "opencad_api.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <set>

using namespace std;

CADTables::CADTables()
{
}

void CADTables::AddTable( TableType eType, const CADHandle& hHandle )
{
    mapTables[eType] = hHandle;
}

int CADTables::ReadTable( CADFile * const pCADFile, CADTables::TableType eType )
{
    auto iterAskedTable = mapTables.find( eType );
    if( iterAskedTable == mapTables.end() )
        return CADErrorCodes::TABLE_READ_FAILED;

    // TODO: read different tables
    switch( eType )
    {
        case LayersTable:
            return ReadLayersTable( pCADFile, iterAskedTable->second.getAsLong() );
        default:
            std::cerr << "Unsupported table.";
            break;
    }

    return CADErrorCodes::SUCCESS;
}

size_t CADTables::GetLayerCount() const
{
    return aLayers.size();
}

CADLayer& CADTables::GetLayer( size_t iIndex )
{
    return aLayers[iIndex];
}

CADHandle CADTables::GetTableHandle( enum TableType eType )
{
    // FIXME: need to add try/catch to prevent crashes on not found elem.
    return mapTables[eType];
}

int CADTables::ReadLayersTable( CADFile * const pCADFile, long dLayerControlHandle )
{
    // Reading Layer Control obj, and aLayers.
    unique_ptr<CADObject> pCADObject( pCADFile->GetObject( dLayerControlHandle ) );

    CADLayerControlObject* spLayerControl =
            dynamic_cast<CADLayerControlObject *>(pCADObject.get());
    if( !spLayerControl )
    {
        return CADErrorCodes::TABLE_READ_FAILED;
    }

    for( size_t i = 0; i < spLayerControl->hLayers.size(); ++i )
    {
        if( !spLayerControl->hLayers[i].isNull() )
        {
            CADLayer oCADLayer( pCADFile );

            // Init CADLayer from CADLayerObject properties
            CADObject* pCADLayerObject = pCADFile->GetObject(
                        spLayerControl->hLayers[i].getAsLong() );
            unique_ptr<CADLayerObject> oCADLayerObj(
                    dynamic_cast<CADLayerObject *>( pCADLayerObject ) );

            if(oCADLayerObj)
            {
                oCADLayer.setName( oCADLayerObj->sLayerName );
                oCADLayer.setFrozen( oCADLayerObj->bFrozen );
                oCADLayer.setOn( oCADLayerObj->bOn );
                oCADLayer.setFrozenByDefault( oCADLayerObj->bFrozenInNewVPORT );
                oCADLayer.setLocked( oCADLayerObj->bLocked );
                oCADLayer.setLineWeight( oCADLayerObj->dLineWeight );
                oCADLayer.setColor( oCADLayerObj->dCMColor );
                oCADLayer.setId( aLayers.size() + 1 );
                oCADLayer.setHandle( oCADLayerObj->hObjectHandle.getAsLong() );

                aLayers.push_back( oCADLayer );
            }
            else
            {
                delete pCADLayerObject;
            }
        }
    }

    auto iterBlockMS = mapTables.find( BlockRecordModelSpace );
    if( iterBlockMS == mapTables.end() )
        return CADErrorCodes::TABLE_READ_FAILED;

    CADObject* pCADBlockObject = pCADFile->GetObject(
                iterBlockMS->second.getAsLong() );
    unique_ptr<CADBlockHeaderObject> spModelSpace(
            dynamic_cast<CADBlockHeaderObject *>( pCADBlockObject ) );
    if(!spModelSpace)
    {
        delete pCADBlockObject;
        return CADErrorCodes::TABLE_READ_FAILED;
    }

    if(spModelSpace->hEntities.size() < 2)
    {
        return CADErrorCodes::TABLE_READ_FAILED;
    }

    auto dCurrentEntHandle = spModelSpace->hEntities[0].getAsLong();
    auto dLastEntHandle    = spModelSpace->hEntities[1].getAsLong();
    // To avoid infinite loops
    std::set<long> oVisitedHandles;
    while( dCurrentEntHandle != 0 &&
           oVisitedHandles.find(dCurrentEntHandle) == oVisitedHandles.end() )
    {
        oVisitedHandles.insert(dCurrentEntHandle);

        CADObject* pCADEntityObject = pCADFile->GetObject( dCurrentEntHandle, true );
        unique_ptr<CADEntityObject> spEntityObj(
                    dynamic_cast<CADEntityObject *>( pCADEntityObject ) );

        if( !spEntityObj )
        {
            delete pCADEntityObject;
            DebugMsg( "Entity object is null\n" );
            break;
        } 
        else if ( dCurrentEntHandle == dLastEntHandle )
        {
            FillLayer( spEntityObj.get() );
            break;
        }

        FillLayer( spEntityObj.get() );

        if( spEntityObj->stCed.bNoLinks )
        {
            ++dCurrentEntHandle;
        }
        else
        {
            dCurrentEntHandle = spEntityObj->stChed.hNextEntity.getAsLong( spEntityObj->stCed.hObjectHandle );
        }
    }

    DebugMsg( "Read aLayers using LayerControl object count: %d\n",
              static_cast<int>(aLayers.size()) );

    return CADErrorCodes::SUCCESS;
}

void CADTables::FillLayer( const CADEntityObject * pEntityObject )
{
    if(nullptr == pEntityObject)
    {
        return;
    }

    for( CADLayer& oLayer : aLayers )
    {
        if( pEntityObject->stChed.hLayer.getAsLong(
                    pEntityObject->stCed.hObjectHandle ) == oLayer.getHandle() )
        {
            DebugMsg( "Object with type: %s is attached to layer named: %s\n",
                      getNameByType( pEntityObject->getType() ).c_str(),
                      oLayer.getName().c_str() );

            oLayer.addHandle( pEntityObject->stCed.hObjectHandle.getAsLong(),
                              pEntityObject->getType() );
            break;
        }
    }
}
