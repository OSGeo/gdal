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
#include "cadlayer.h"
#include "cadfile.h"

#include <cassert>
#include <iostream>
#include <algorithm>

CADLayer::CADLayer( CADFile * file ) :
    frozen( false ),
    on( true ),
    frozenByDefault( false ),
    locked( false ),
    plotting( false ),
    lineWeight( 1 ),
    color( 0 ),
    layerId( 0 ),
    layer_handle( 0 ),
    pCADFile( file )
{
}

std::string CADLayer::getName() const
{
    return layerName;
}

void CADLayer::setName( const std::string& value )
{
    layerName = value;
}

bool CADLayer::getFrozen() const
{
    return frozen;
}

void CADLayer::setFrozen( bool value )
{
    frozen = value;
}

bool CADLayer::getOn() const
{
    return on;
}

void CADLayer::setOn( bool value )
{
    on = value;
}

bool CADLayer::getFrozenByDefault() const
{
    return frozenByDefault;
}

void CADLayer::setFrozenByDefault( bool value )
{
    frozenByDefault = value;
}

bool CADLayer::getLocked() const
{
    return locked;
}

void CADLayer::setLocked( bool value )
{
    locked = value;
}

bool CADLayer::getPlotting() const
{
    return plotting;
}

void CADLayer::setPlotting( bool value )
{
    plotting = value;
}

short CADLayer::getLineWeight() const
{
    return lineWeight;
}

void CADLayer::setLineWeight( short value )
{
    lineWeight = value;
}

short CADLayer::getColor() const
{
    return color;
}

void CADLayer::setColor( short value )
{
    color = value;
}

size_t CADLayer::getId() const
{
    return layerId;
}

void CADLayer::setId( const size_t& value )
{
    layerId = value;
}

long CADLayer::getHandle() const
{
    return layer_handle;
}

void CADLayer::setHandle( long value )
{
    layer_handle = value;
}

void CADLayer::addHandle( long handle, CADObject::ObjectType type, long cadinserthandle )
{
#ifdef _DEBUG
    std::cout << "addHandle: " << handle << " type: " << type << "\n";
#endif //_DEBUG
    if( type == CADObject::ATTRIB || type == CADObject::ATTDEF )
    {
        auto pCADGeometryPtr = pCADFile->GetGeometry( getId() - 1, handle );
        std::unique_ptr<CADGeometry> pCADGeometry( pCADGeometryPtr );
        CADAttdef* attdef = dynamic_cast<CADAttdef*>(pCADGeometry.get());
        if(attdef)
        {
            attributesNames.insert( attdef->getTag() );
        }
    }

    if( type == CADObject::INSERT )
    {
        // TODO: transform insert to block of objects (do we need to transform
        // coordinates according to insert point)?
        auto insertPtr = pCADFile->GetObject( handle, false );
        std::unique_ptr<CADObject> insert( insertPtr );
        CADInsertObject * pInsert = dynamic_cast<CADInsertObject *>(insert.get());
        if( nullptr != pInsert )
        {
            std::unique_ptr<CADObject> blockHeader( pCADFile->GetObject(
                                    pInsert->hBlockHeader.getAsLong(), false ) );
            CADBlockHeaderObject * pBlockHeader =
                          dynamic_cast<CADBlockHeaderObject *>(blockHeader.get());
            if( nullptr != pBlockHeader )
            {
#ifdef _DEBUG
                if( pBlockHeader->bBlkisXRef )
                {
                    assert( 0 );
                }
#endif //_DEBUG
                auto dCurrentEntHandle = pBlockHeader->hEntities[0].getAsLong();
                auto dLastEntHandle    = pBlockHeader->hEntities.back().getAsLong(); // FIXME: in 2000+ entities probably has no links to each other.

                if( dCurrentEntHandle == dLastEntHandle ) // Blocks can be empty (contain no objects)
                {
                    return;
                }

                while( true )
                {
                    std::unique_ptr<CADObject> entity(pCADFile->GetObject(
                                                       dCurrentEntHandle, true ));
                    CADEntityObject* pEntity =
                            dynamic_cast<CADEntityObject *>( entity.get() );

                    if( nullptr == pEntity )
                    {
                        // shouldn't happen on a valid file, but can happen
                        // on broken ones
                        break;
                    }

                    if( dCurrentEntHandle == handle && type == pEntity->getType() )
                    {
                        // If the above condition is true, infinite recursion
                        // would occur in the following addHandle() call.
                        // Shouldn't happen on a valid file, but can happen
                        // on broken ones, such as in https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=46887
                        break;
                    }

                    addHandle( dCurrentEntHandle, pEntity->getType(), handle );
                    Matrix mat;
                    mat.translate( pInsert->vertInsertionPoint );
                    mat.scale( pInsert->vertScales );
                    mat.rotate( pInsert->dfRotation );
                    transformations[dCurrentEntHandle] = mat;

                    if( dCurrentEntHandle == dLastEntHandle )
                    {
                        break;
                    }
                    else
                    {
                        if( pEntity->stCed.bNoLinks )
                        {
                            ++dCurrentEntHandle;
                        }
                        else
                        {
                            dCurrentEntHandle =
                                pEntity->stChed.hNextEntity.getAsLong(
                                    pEntity->stCed.hObjectHandle );
                        }
                    }
                }
            }
        }
        return;
    }

    if( isCommonEntityType( type ) )
    {
        if( type == CADObject::IMAGE )
        {
            imageHandles.push_back( handle );
        }
        else
        {
            if( pCADFile->isReadingUnsupportedGeometries() == false )
            {
                if( isSupportedGeometryType( type ) )
                {
                    if( geometryTypes.empty() )
                    {
                        geometryTypes.push_back( type );
                    }

                    if( find( geometryTypes.begin(), geometryTypes.end(), type ) ==
                        geometryTypes.end() )
                    {
                        geometryTypes.push_back( type );
                    }
                    geometryHandles.push_back(
                                std::make_pair( handle, cadinserthandle ) );
                }
            }
            else
            {
                if( geometryTypes.empty() )
                {
                    geometryTypes.push_back( type );
                }

                if( find( geometryTypes.begin(), geometryTypes.end(), type ) ==
                    geometryTypes.end() )
                {
                    geometryTypes.push_back( type );
                }
                geometryHandles.push_back(
                            std::make_pair( handle, cadinserthandle ) );
            }
        }
    }
}

size_t CADLayer::getGeometryCount() const
{
    return geometryHandles.size();
}

CADGeometry * CADLayer::getGeometry( size_t index )
{
    auto handleBlockRefPair = geometryHandles[index];
    CADGeometry * pGeom = pCADFile->GetGeometry( this->getId() - 1,
                        handleBlockRefPair.first, handleBlockRefPair.second );
    if( nullptr == pGeom )
        return nullptr;
    auto iter = transformations.find( handleBlockRefPair.first );
    if( iter != transformations.end() )
    {
        // transform geometry if nHandle is in transformations
        pGeom->transform( iter->second );
    }
    return pGeom;
}

size_t CADLayer::getImageCount() const
{
    return imageHandles.size();
}

CADImage * CADLayer::getImage( size_t index )
{
    return static_cast<CADImage *>(pCADFile->GetGeometry( this->getId() - 1,
                                                            imageHandles[index] ));
}

bool CADLayer::addAttribute( const CADObject * pObject )
{
    if( nullptr == pObject )
        return true;

    auto attrib = static_cast<const CADAttribObject *>(pObject);
    for( auto i = geometryAttributes.begin(); i != geometryAttributes.end(); ++i )
    {
        if( i->first == attrib->stChed.hOwner.getAsLong() )
        {
            i->second.insert( make_pair( attrib->sTag, layer_handle ) );
            return true;
        }
    }

    return false;
}

std::vector<CADObject::ObjectType> CADLayer::getGeometryTypes()
{
    return geometryTypes;
}

std::unordered_set<std::string> CADLayer::getAttributesTags()
{
    return attributesNames;
}
