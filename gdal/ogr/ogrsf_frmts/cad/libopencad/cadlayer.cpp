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
#include <iostream>
#include "cadlayer.h"
#include "cadfile.h"
#include <cassert>

CADLayer::CADLayer(CADFile * const file) : frozen(false), on(true),
    frozenByDefault(false), locked(false), plotting(false), lineWeight(1),
    color(0), layerId(0), handle(0), geometryType(-2), pCADFile(file)
{

}

string CADLayer::getName() const
{
    return layerName;
}

void CADLayer::setName(const string &value)
{
    layerName = value;
}

bool CADLayer::getFrozen() const
{
    return frozen;
}

void CADLayer::setFrozen(bool value)
{
    frozen = value;
}

bool CADLayer::getOn() const
{
    return on;
}

void CADLayer::setOn(bool value)
{
    on = value;
}

bool CADLayer::getFrozenByDefault() const
{
    return frozenByDefault;
}

void CADLayer::setFrozenByDefault(bool value)
{
    frozenByDefault = value;
}

bool CADLayer::getLocked() const
{
    return locked;
}

void CADLayer::setLocked(bool value)
{
    locked = value;
}

bool CADLayer::getPlotting() const
{
    return plotting;
}

void CADLayer::setPlotting(bool value)
{
    plotting = value;
}

short CADLayer::getLineWeight() const
{
    return lineWeight;
}

void CADLayer::setLineWeight(short value)
{
    lineWeight = value;
}

short CADLayer::getColor() const
{
    return color;
}

void CADLayer::setColor(short value)
{
    color = value;
}

size_t CADLayer::getId() const
{
    return layerId;
}

void CADLayer::setId(const size_t &value)
{
    layerId = value;
}

long CADLayer::getHandle() const
{
    return handle;
}

void CADLayer::setHandle(long value)
{
    handle = value;
}

void CADLayer::addHandle(long handle, CADObject::ObjectType type)
{
#ifdef _DEBUG
    cout << "addHandle: " << handle << " type: " << type << endl;
#endif //_DEBUG
    if( type == CADObject::ATTRIB || type == CADObject::ATTDEF )
    {
        unique_ptr< CADObject > geometry( pCADFile->getObject ( handle, false ) );

        if(addAttribute(geometry.get()))
            return;
    }

    if( type == CADObject::INSERT){
        // TODO: transform insert to block of objects (do we need to transform
        // coordinates according to insert point)?
        unique_ptr< CADObject > insert( pCADFile->getObject ( handle, false ) );
        CADInsertObject *pInsert = static_cast<CADInsertObject *>(insert.get ());
        if(nullptr != pInsert){
            unique_ptr< CADObject > blockHeader(
                        pCADFile->getObject (
                            pInsert->hBlockHeader.getAsLong (), false ));
            CADBlockHeaderObject *pBlockHeader =
                    static_cast<CADBlockHeaderObject *>(blockHeader.get ());
            if(nullptr != pBlockHeader){
#ifdef _DEBUG
               if(pBlockHeader->bBlkisXRef){
                   assert(0);
               }
#endif //_DEBUG
               for(CADHandle entHandle : pBlockHeader->hEntities){
                   unique_ptr< CADObject > entity(
                               pCADFile->getObject ( entHandle.getAsLong (),
                                                     false ) );
                   if(nullptr == entity)
                       continue;
                   addHandle(entHandle.getAsLong (), entity->getType ());
                   // add shift/scale/rotate to transform map
                   transformations[entHandle.getAsLong ()] =
                        {pInsert->vertInsertionPoint,
                         pInsert->vertScales,
                         pInsert->dfRotation};
               }
            }

            // TODO: what todo with attributes of insertion?
            //for (CADHandle attr : pInsert->hAtrribs) {
            //    addHandle(attr.getAsLong (), CADObject::ATTRIB);
            //}
        }
        return;
    }

    if( type == CADObject::BLOCK || type == CADObject::IMAGE  ||
            type == CADObject::IMAGEDEF || type == CADObject::IMAGEDEFREACTOR) {
#ifdef _DEBUG
        assert(0);
#endif //_DEBUG
    }

    if(isCommonEntityType (type))
    {
        geometryHandles.push_back( handle );
        if( geometryType == -2 ) // if not inited set type for first geometry
            geometryType = type;
        else if( geometryType != type ) // if type differs from previous geometry this is geometry bag (geometry type any)
            geometryType = -1;
    }
}

size_t CADLayer::getGeometryCount() const
{
    return geometryHandles.size ();
}

CADGeometry *CADLayer::getGeometry(size_t index)
{
    // TODO: transform geometry if geometryHandles[index] is in transformations
    return pCADFile->getGeometry(geometryHandles[index]);
}

bool CADLayer::addAttribute(const CADObject *pObject)
{
    if(nullptr == pObject)
        return true;

    auto attrib = static_cast<const CADAttribObject*>(pObject);
    for ( auto i = geometryAttributes.begin (); i != geometryAttributes.end(); ++i )
    {
        if ( i->first == attrib->stChed.hOwner.getAsLong () )
        {
            i->second.insert ( make_pair( attrib->sTag, handle ) );
            return true;
        }
    }

    return false;
}
