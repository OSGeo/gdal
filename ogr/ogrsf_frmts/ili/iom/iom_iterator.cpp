/**********************************************************************
 * $Id$
 *
 * Project:  iom - The INTERLIS Object Model
 * Purpose:  For more information, please see <http://iom.sourceforge.net>
 * Author:   Claude Eisenhut
 *
 **********************************************************************
 * Copyright (c) 2007, Claude Eisenhut
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
 ****************************************************************************/


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

