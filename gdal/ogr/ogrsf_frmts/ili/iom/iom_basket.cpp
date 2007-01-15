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
 * implementation of object basket
 * @defgroup basket basket functions
 * @{
 */

#include <iom/iom_p.h>

/** gets an iterator to walk threw all objects of a basket.
 * The list includes embedded link(-objects), but no structures.
 */
extern "C" IOM_ITERATOR iom_iteratorobject(IOM_BASKET basket)
{
	return (dbgnew iom_iterator(basket))->getRef();
}

/** gets the next object.
 * Returns 0 if no more objects.
 */
extern "C" IOM_OBJECT iom_nextobject(IOM_ITERATOR iterator)
{
	IomObject ret=iterator->next_object();
	return ret.isNull() ? 0 : ret->getRef();
}

/** create a new object.
 */
extern "C" IOM_OBJECT iom_newobject(IOM_BASKET basket,IOM_TAG type,IOM_OID oid)
{
	IomObject ret=new iom_object();
	ret->setOid(X(oid));
	ret->setTag(ParserHandler::getTagId(type));
	basket->addObject(ret);
	return ret->getRef();
}


/**
 * release handle
 */
extern "C" int iom_releasebasket(IOM_BASKET basket)
{
	if(!basket->freeRef()){
		delete basket;
	}
	return 0;
}

// Basket löschen (Basket aus Datei entfernen)
extern "C" int iom_deletebasket(IOM_BASKET basket);


/** gets OID of a basket.
 *  return value is valid until iom_setbasketoid() or iom_close()
 */
extern "C" IOM_OID iom_getbasketoid(IOM_BASKET basket)
{
	return basket->getOid_c();
}

/** sets OID of a basket.
 */
extern "C" void iom_setbasketoid(IOM_BASKET basket,IOM_OID oid){
	basket->setOid(X(oid));
}


/** gets the consistency of a basket.
 */
extern "C" int iom_getbasketconsistency(IOM_BASKET basket)
{
	return basket->getConsistency();
}

/** sets the consistency of a basket.
 */
extern "C" void iom_setbasketconsistency(IOM_BASKET basket,int consistency)
{
	basket->setConsistency(consistency);
}

/** gets xml element name of a basket.
 *  return value is valid until iom_close() or iom_setbaskettag().
 */
extern "C" IOM_TAG iom_getbaskettag(IOM_BASKET basket)
{
	return basket->getTag_c();
}

/** sets xml element name of a basket.
 */
extern "C" void iom_setbaskettag(IOM_BASKET basket,IOM_TAG topic)
{
	basket->setTag(ParserHandler::getTagId(topic));
}

/** get xmlfile line number of a basket.
 */
extern "C" int iom_getbasketline(IOM_BASKET basket)
{
	return basket->getXMLLineNumber();
}

/** get xmlfile column number of a basket.
 */
extern "C" int iom_getbasketcol(IOM_BASKET basket)
{
	return basket->getXMLColumnNumber();
}

/** gets object with a given OID or 0.
 */
extern "C" IOM_OBJECT iom_getobject(IOM_BASKET basket,IOM_OID oid)
{
	IomObject ret=basket->getObject(X(oid));
	return ret.isNull() ? 0 : ret->getRef();
}

// Objekt in einen anderen Basket verschieben
extern "C" int iom_relocateobject(IOM_BASKET basket,IOM_OBJECT object);


// seit dem letzten Lesen/Schreiben geänderte Objekte
extern "C" IOM_ITERATOR iom_iteratorchgobject(IOM_BASKET basket);
extern "C" IOM_OBJECT iom_nextchgobject(IOM_ITERATOR iterator); 

// seit dem letzten Lesen/Schreiben gelöschte Objekte
extern "C" IOM_ITERATOR iom_iteratordelobject(IOM_BASKET basket);
extern "C" IOM_OBJECT iom_nextdelobject(IOM_ITERATOR iterator);


/** @}
 */

iom_basket::iom_basket() :
	file()
	,tag(0)
	,tag_c(0)
	,xmlLine(0)
	,xmlCol(0)
	,consistency(IOM_COMPLETE)
	,kind(IOM_FULL)
	,oid_w(0)
	,oid_c(0)
	,startstate_w(0)
	,startstate_c(0)
	,endstate_w(0)
	,endstate_c(0)
	,topics_w(0)
	,topics_c(0)
	,useCount(0)
{
}

iom_basket::~iom_basket()
{
	if(tag_c)XMLString::release(&tag_c);
	if(oid_c)XMLString::release(&oid_c);
	if(oid_w)XMLString::release(&oid_w);
	if(startstate_c)XMLString::release(&startstate_c);
	if(startstate_w)XMLString::release(&startstate_w);
	if(endstate_c)XMLString::release(&endstate_c);
	if(endstate_w)XMLString::release(&endstate_w);
	if(topics_c)XMLString::release(&topics_c);
	if(topics_w)XMLString::release(&topics_w);
}

void iom_basket::setTag(int tag1)
{
	if(tag_c)tag_c=0;
	tag=tag1;
}
int iom_basket::getTag()
{
	return tag;
}
const char *iom_basket::getTag_c()
{
	if(!tag){
		return 0;
	}
	if(!tag_c){
		const XMLCh *const tag_w=ParserHandler::getTagName(tag);
		tag_c=XMLString::transcode(tag_w);
	}
	return tag_c;
}

void iom_basket::setXMLLineNumber(int line)
{
	xmlLine=line;
}
int iom_basket::getXMLLineNumber()
{
	return xmlLine;
}

void iom_basket::setXMLColumnNumber(int col)
{
	xmlCol=col;
}

int iom_basket::getXMLColumnNumber()
{
	return xmlCol;
}

void iom_basket::setConsistency(int cons)
{
	consistency=cons;
}

int iom_basket::getConsistency()
{
	return consistency;
}

void iom_basket::setKind(int kind1)
{
	kind=kind1;
}

int iom_basket::getKind()
{
	return kind;
}

void iom_basket::setOid(const XMLCh *oid)
{
	if(oid_c)XMLString::release(&oid_c);
	if(oid_w)XMLString::release(&oid_w);
	oid_w=XMLString::replicate(oid);
}
const XMLCh *iom_basket::getOid()
{
	return oid_w;
}
const char *iom_basket::getOid_c()
{
	if(!oid_w){
		return 0;
	}
	if(!oid_c){
		oid_c=XMLString::transcode(oid_w);
	}
	return oid_c;
}

void iom_basket::setStartState(const XMLCh *startstate)
{
	if(startstate_c)XMLString::release(&startstate_c);
	if(startstate_w)XMLString::release(&startstate_w);
	startstate_w=XMLString::replicate(startstate);
}
const XMLCh *iom_basket::getStartState()
{
	return startstate_w;
}
const char *iom_basket::getStartState_c()
{
	if(!startstate_w){
		return 0;
	}
	if(!startstate_c){
		startstate_c=XMLString::transcode(startstate_w);
	}
	return startstate_c;
}


void iom_basket::setEndState(const XMLCh *endstate)
{
	if(endstate_c)XMLString::release(&endstate_c);
	if(endstate_w)XMLString::release(&endstate_w);
	endstate_w=XMLString::replicate(endstate);
}
const XMLCh *iom_basket::getEndState()
{
	return endstate_w;
}
const char *iom_basket::getEndState_c()
{
	if(!endstate_w){
		return 0;
	}
	if(!endstate_c){
		endstate_c=XMLString::transcode(endstate_w);
	}
	return endstate_c;
}


void iom_basket::setTopics(const XMLCh *topics)
{
	if(topics_c)XMLString::release(&topics_c);
	if(topics_w)XMLString::release(&topics_w);
	topics_w=XMLString::replicate(topics);
}
const XMLCh *iom_basket::getTopics()
{
	return topics_w;
}
const char *iom_basket::getTopics_c()
{
	if(!topics_w){
		return 0;
	}
	if(!topics_c){
		topics_c=XMLString::transcode(topics_w);
	}
	return topics_c;
}


void iom_basket::addObject(IomObject object)
{
	objectv.push_back(object);
	object->setBasket(this);
}

IomObject iom_basket::getObject(const XMLCh *oid)
{
	std::vector<IomObject>::iterator it;
	for(it=objectv.begin();it!=objectv.end();it++){
		IomObject obj=*it;
		if(!XMLString::compareString(oid,obj->getOid())){
			return obj;
		}
	}
	return IomObject();
}


IomBasket::IomBasket(struct iom_basket *pointee1) 
: pointee(pointee1 ? pointee1->getRef() : 0){
}
IomBasket::IomBasket(const IomBasket& src) 
: pointee(src.pointee ? src.pointee->getRef() : 0){
}
IomBasket& IomBasket::operator=(const IomBasket& src){
	if(this!=&src){
		if(pointee && !pointee->freeRef()){
			delete pointee;
		}
		pointee=src.pointee ? src.pointee->getRef() :0;
	}
	return *this;
}
IomBasket::~IomBasket(){
	if(pointee && !pointee->freeRef()){
		delete pointee;
	}
}
