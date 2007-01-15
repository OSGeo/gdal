/* This file is part of the iom project.
 * For more information, please see <http://www.interlis.ch>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/** @file
 * implementation of object iterator
 * @defgroup iterator iterator functions
 * @{
 */

#include <iom/iom_p.h>


/**
 * release handle
 */
extern "C" int iom_releaseiterator(IOM_ITERATOR iterator)
{
	if(!iterator->freeRef()){
		delete iterator;
	}
	return 0;
}

/** @}
 */

iom_iterator::iom_iterator(IomFile file1)
: type(iom_iterator::eBASKET)
, useCount(0)
, basketv(file1)
, basketi(0)
{
}

IomBasket iom_iterator::next_basket()
{
	if(basketi==basketv->basketv.size()){
		// file completly read?
		if(!basketv->parser){
			return IomBasket();
		}
		// read next basket
		basketv->readBasket(basketv);
	}
	if(basketi==basketv->basketv.size()){
		// file completly read
		return IomBasket();
	}
	return basketv->basketv.at(basketi++);
}

iom_iterator::iom_iterator(IomBasket basket1)
: type(iom_iterator::eOBJECT)
, useCount(0)
, objectv(basket1)
, objecti(0)
{
}

IomObject iom_iterator::next_object()
{
	if(objecti==objectv->objectv.size()){
		// basket completly read
		return IomObject();
	}
	return objectv->objectv.at(objecti++);
}


iom_iterator::~iom_iterator()
{
}


IomIterator::IomIterator(struct iom_iterator *pointee1) 
: pointee(pointee1 ? pointee1->getRef() : 0){
}
IomIterator::IomIterator(const IomIterator& src) 
: pointee(src.pointee ? src.pointee->getRef() : 0){
}
IomIterator& IomIterator::operator=(const IomIterator& src){
	if(this!=&src){
		if(pointee && !pointee->freeRef()){
			delete pointee;
		}
		pointee=src.pointee ? src.pointee->getRef() : 0;
	}
	return *this;
}
IomIterator::~IomIterator(){
	if(pointee && !pointee->freeRef()){
		delete pointee;
	}
}

