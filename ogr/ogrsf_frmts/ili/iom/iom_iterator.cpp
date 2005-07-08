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
: useCount(0)
, type(iom_iterator::eBASKET)
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
: useCount(0)
, type(iom_iterator::eOBJECT)
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
	type=eOBJECT;
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

