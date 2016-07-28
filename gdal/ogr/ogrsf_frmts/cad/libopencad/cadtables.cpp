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
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
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

#include <memory>
#include <cassert>
#include <iostream>

using namespace std;

CADTables::CADTables()
{

}

void CADTables::addTable(TableType eType, CADHandle hHandle)
{
    tableMap[eType] = hHandle;
}

int CADTables::readTable( CADFile * const file, CADTables::TableType eType)
{
    auto iter = tableMap.find (eType);
    if( iter == tableMap.end () )
        return CADErrorCodes::TABLE_READ_FAILED;

    // TODO: read different tables
    switch (eType)
    {
        case LayersTable:
            return readLayersTable(file, iter->second.getAsLong ());
        default:
            std::cerr << "Unsupported table";
            break;
    }

    return CADErrorCodes::SUCCESS;
}

size_t CADTables::getLayerCount() const
{
    return layers.size ();
}

CADLayer& CADTables::getLayer(size_t index)
{
    return layers[index];
}

CADHandle CADTables::getTableHandle ( enum TableType type )
{
    // FIXME: need to add try/catch to prevent crashes on not found elem.
    return tableMap[type];
}

int CADTables::readLayersTable( CADFile  * const file, long index)
{
    // Reading Layer Control obj, and layers.
    unique_ptr<CADLayerControlObject> layerControl(
                static_cast<CADLayerControlObject*>(file->getObject (index)));
    if(nullptr == layerControl)
        return CADErrorCodes::TABLE_READ_FAILED;

    for ( size_t i = 0; i < layerControl->hLayers.size(); ++i )
    {
        if ( !layerControl->hLayers[i].isNull())
        {
            CADLayer layer(file);

            // Init CADLayer from objLayer properties
            unique_ptr<CADLayerObject> objLayer(
                        static_cast<CADLayerObject*>(file->getObject (
                                        layerControl->hLayers[i].getAsLong ())));

            layer.setName (objLayer->sLayerName);
            layer.setFrozen (objLayer->bFrozen);
            layer.setOn (objLayer->bOn);
            layer.setFrozenByDefault (objLayer->bFrozenInNewVPORT);
            layer.setLocked (objLayer->bLocked);
            layer.setLineWeight (objLayer->dLineWeight);
            layer.setColor (objLayer->dCMColor);
            layer.setId (layers.size () + 1);
            layer.setHandle (objLayer->hObjectHandle.getAsLong ());

            layers.push_back (layer);
        }
    }

    auto it = tableMap.find (BlockRecordModelSpace);
    if(it == tableMap.end ())
        return CADErrorCodes::TABLE_READ_FAILED;

    unique_ptr<CADBlockHeaderObject> pstModelSpace (
            static_cast<CADBlockHeaderObject *>(file->getObject (
                                                    it->second.getAsLong ())));

    auto dCurrentEntHandle = pstModelSpace->hEntities[0].getAsLong ();
    auto dLastEntHandle    = pstModelSpace->hEntities[1].getAsLong ();
    while ( true )
    {
        unique_ptr<CADEntityObject> ent( static_cast<CADEntityObject *>(
                                          file->getObject (dCurrentEntHandle,
                                                           true))); // true = read CED && handles only

        if ( dCurrentEntHandle == dLastEntHandle )
        {
            if(nullptr != ent)
                fillLayer(ent.get ());
            else
            {
#ifdef _DEBUG
                assert(0);
#endif //_DEBUG
            }
            break;
        }

        /* TODO: this check is excessive, but if something goes wrong way -
         * some part of geometries will be parsed. */
        if ( ent != nullptr )
        {
            fillLayer(ent.get ());

            if ( ent->stCed.bNoLinks )
                ++dCurrentEntHandle;
            else
                dCurrentEntHandle = ent->stChed.hNextEntity.getAsLong (
                            ent->stCed.hObjectHandle);
        }
        else
        {
#ifdef _DEBUG
            assert(0);
#endif //_DEBUG
        }
    }

    DebugMsg ("Readed layers using LayerControl object count: %d\n",
              layers.size ());

    return CADErrorCodes::SUCCESS;
}

void CADTables::fillLayer(const CADEntityObject *ent)
{
    for ( CADLayer &layer : layers )
    {
        if ( ent->stChed.hLayer.getAsLong (ent->stCed.hObjectHandle) ==
             layer.getHandle () )
        {
            DebugMsg ("Object with type: %s is attached to layer named: %s\n",
                      getNameByType(ent->getType()).c_str (),
                      layer.getName ().c_str ());

            layer.addHandle (ent->stCed.hObjectHandle.getAsLong (),
                             ent->getType());
            break; // TODO: check if only can be add to one layer
        }
    }
}



