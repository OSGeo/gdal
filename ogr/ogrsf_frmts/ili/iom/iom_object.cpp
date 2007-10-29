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
 * implementation of object object
 * @defgroup object object level functions
 * @{
 */

#include <assert.h>
#include <iom/iom_p.h>

/**
 * release handle
 */
extern "C" int iom_releaseobject(IOM_OBJECT object)
{
	if(!object->freeRef()){
		delete object;
	}
	return 0;
}

// Objekt löschen
extern "C" int iom_deleteobject(IOM_OBJECT object);


/** gets the tag of this object.
 * The return value is valid until iom_close or iom_setclass.
 */
extern "C" IOM_TAG iom_getobjecttag(IOM_OBJECT object)
{
	return object->getTag_c();
}

/** sets the tag of this object.
 */
extern "C" void iom_setobjecttag(IOM_OBJECT object,IOM_TAG tag)
{
	object->setTag(ParserHandler::getTagId(tag));
}

/** gets the OID of this object.
 * The return value is valid until iom_close or iom_setobjectoid.
 */
extern "C" IOM_OID iom_getobjectoid(IOM_OBJECT object){
	return object->getOid_c();
}

/** sets the OID of this object.
 */
extern "C" void iom_setobjectoid(IOM_OBJECT object,IOM_OID oid)
{
	object->setOid(X(oid));
}

/** get xmlfile line number of a object.
 */
extern "C" int iom_getobjectline(IOM_OBJECT obj)
{
	return obj->getXMLLineNumber();
}

/** get xmlfile column number of a object.
 */
extern "C" int iom_getobjectcol(IOM_OBJECT obj)
{
	return obj->getXMLColumnNumber();
}

/** gets the oid of the referenced object or 0 if this is not a reference.
 */
extern "C" IOM_OID iom_getobjectrefoid(IOM_OBJECT object){
	return object->getRefOid_c();
}

/** sets the oid of the referenced object.
 */
extern "C" void iom_setobjectrefoid(IOM_OBJECT object,IOM_OID refoid)
{
	object->setRefOid(X(refoid));
}

/** get the bid of the referenced object or 0 if this is not a reference
 *  or returns 0 if the referenced object is in the same basket.
 */
extern "C" IOM_OID iom_getobjectrefbid(IOM_OBJECT object){
	return object->getRefBid_c();
}

/** sets the bid of the referenced object.
 */
extern "C" void iom_setobjectrefbid(IOM_OBJECT object,IOM_OID refbid)
{
	object->setRefBid(X(refbid));
}

/** gets the ORDER_POS of the referenced object.
*/
extern "C" unsigned int iom_getobjectreforderpos(IOM_OBJECT object)
{
	return object->getRefOrderPos();
}

/** sets the ORDER_POS of the referenced object.
*/
extern "C" void iom_setobjectreforderpos(IOM_OBJECT object,unsigned int orderPos)
{
	object->setRefOrderPos(orderPos);
}

/** gets the operation-mode of an object.
 */
extern "C" int iom_getobjectoperation(IOM_OBJECT object){
	return object->getOperation();
}

/** sets the operation-mode of an object.
 */
extern "C" void iom_setobjectoperation(IOM_OBJECT object,int operation){
	object->setOperation(operation);
}

/** gets the consistency of an object.
 */
extern "C" int iom_getobjectconsistency(IOM_OBJECT object){
	return object->getConsistency();
}

/** sets the consistency of an object.
 */
extern "C" void iom_setobjectconsistency(IOM_OBJECT object,int consistency){
	object->setConsistency(consistency);
}

/** @}
 */

iom_object::iom_object()
	:useCount(0)
	, basket(0)
	,consistency(IOM_COMPLETE)
	,operation(IOM_OP_INSERT)
	,tag(0)
	,tag_c(0)
	,xmlLine(0)
	,xmlCol(0)
	,oid_w(0)
	,oid_c(0)
	,bid_w(0)
	,bid_c(0)
	,refOid_w(0)
	,refOid_c(0)
	,refBid_w(0)
	,refBid_c(0)
	, refOrderPos(0)
{
}
iom_object::~iom_object()
{
	if(tag_c)XMLString::release(&tag_c);
	if(oid_c)XMLString::release(&oid_c);
	if(oid_w)XMLString::release(&oid_w);
	if(bid_c)XMLString::release(&bid_c);
	if(bid_w)XMLString::release(&bid_w);
	if(refOid_w)XMLString::release(&refOid_w);
	if(refOid_c)XMLString::release(&refOid_c);
	if(refBid_w)XMLString::release(&refBid_w);
	if(refBid_c)XMLString::release(&refBid_c);
}

void iom_object::setBasket(IomBasket basket1)
{
	basket=&(*basket1);
	assert(tag!=0);
}

void iom_object::setTag(int tag1)
{
	if(tag_c){
		XMLString::release(&tag_c);
		tag_c=0;
	}
	tag=tag1;
	const XMLCh *const tag_w=ParserHandler::getTagName(tag);
	tag_c=XMLString::transcode(tag_w);
}
const char *iom_object::getTag_c()
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
int iom_object::getTag()
{
	return tag;
}

void iom_object::setXMLLineNumber(int line)
{
	xmlLine=line;
}

int iom_object::getXMLLineNumber()
{
	return xmlLine;
}

void iom_object::setXMLColumnNumber(int col)
{
	xmlCol=col;
}

int iom_object::getXMLColumnNumber()
{
	return xmlCol;
}

/** sets the consistency of an object.
 */
void iom_object::setConsistency(int cons)
{
	consistency=cons;
}

/** gets the consistency of an object.
 */
int iom_object::getConsistency()
{
	return consistency;
}

/** sets the operation-mode of an object.
 */
void iom_object::setOperation(int op)
{
	operation=op;
}

/** gets the operation-mode of an object.
 */
int iom_object::getOperation()
{
	return operation;
}

void iom_object::setOid(const XMLCh *oid)
{
	if(oid_c)XMLString::release(&oid_c);
	if(oid_w)XMLString::release(&oid_w);
	oid_w=XMLString::replicate(oid);
}
const XMLCh *iom_object::getOid()
{
	return oid_w;
}
const char *iom_object::getOid_c()
{
	if(!oid_w){
		return 0;
	}
	if(!oid_c){
		oid_c=XMLString::transcode(oid_w);
	}
	return oid_c;
}

void iom_object::setBid(const XMLCh *bid)
{
	if(bid_c)XMLString::release(&bid_c);
	if(bid_w)XMLString::release(&bid_w);
	bid_w=XMLString::replicate(bid);
}
const char *iom_object::getBid_c()
{
	if(!bid_w){
		return 0;
	}
	if(!bid_c){
		bid_c=XMLString::transcode(bid_w);
	}
	return bid_c;
}
const XMLCh* iom_object::getBid()
{
	return bid_w;
}

/** sets the oid of the referenced object.
 */
void iom_object::setRefOid(const XMLCh *oid)
{
	if(refOid_c)XMLString::release(&refOid_c);
	if(refOid_w)XMLString::release(&refOid_w);
	refOid_w=XMLString::replicate(oid);
}

/** gets the oid of the referenced object.
 */
const char *iom_object::getRefOid_c()
{
	if(!refOid_w){
		return 0;
	}
	if(!refOid_c){
		refOid_c=XMLString::transcode(refOid_w);
	}
	return refOid_c;
}

/** gets the oid of the referenced object.
 */
const XMLCh *iom_object::getRefOid()
{
	return refOid_w;
}

/** sets the bid of the referenced object.
 */
void iom_object::setRefBid(const XMLCh *bid)
{
	if(refBid_c)XMLString::release(&refBid_c);
	if(refBid_w)XMLString::release(&refBid_w);
	refBid_w=XMLString::replicate(bid);
}


/** gets the bid of the referenced object.
 */
const char *iom_object::getRefBid_c()
{
	if(!refBid_w){
		return 0;
	}
	if(!refBid_c){
		refBid_c=XMLString::transcode(refBid_w);
	}
	return refBid_c;
}

/** gets the bid of the referenced object.
 */
const XMLCh *iom_object::getRefBid()
{
	return refBid_w;
}

/** gets ORDER_POS value, if this is a ORDERED association end.
 */
unsigned int iom_object::getRefOrderPos()
{
	return refOrderPos;
}

/** sets ORDER_POS value, if this is a ORDERED association end.
 */
void iom_object::setRefOrderPos(unsigned int value)
{
	refOrderPos=value;
}

/** dumps all attributes to stderr.
 */
void iom_object::dumpAttrs(){

	attrValuev_type::iterator attr=attrValuev.begin();
	while(attr!=attrValuev.end()){
		std::cerr << attr->first << ", " << StrX(ParserHandler::getTagName(attr->first)) << std::endl;
		attr++;
	}
}

/** sets the value of a primitive type attribute.
 */
void iom_object::parser_addAttrValue(int attrName,const XMLCh *value)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	int idx;
	if(attr==attrValuev.end()){
		// not found, add
		valuev_type valuev;
		idx=valuev.size();
		valuev.push_back(iom_value(XMLString::replicate(value)));
		attrValuev[attrName]=valuev;
	}else{
		// found, add
		valuev_type valuev=attr->second;
		idx=valuev.size();
		valuev.push_back(iom_value(XMLString::replicate(value)));
		attrValuev[attrName]=valuev;
	}
	xmleleidxv.push_back(xmlele_type(attrName,idx));
}
void iom_object::parser_addAttrValue(int attrName,IomObject value)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	int idx;
	if(attr==attrValuev.end()){
		// not found, add
		valuev_type valuev;
		idx=valuev.size();
		valuev.push_back(iom_value(value));
		attrValuev[attrName]=valuev;
	}else{
		// found, add
		valuev_type valuev=attr->second;
		idx=valuev.size();
		valuev.push_back(iom_value(value));
		attrValuev[attrName]=valuev;
	}
	xmleleidxv.push_back(xmlele_type(attrName,idx));
}

int iom_object::getXmleleCount()
{
	return xmleleidxv.size();
}

int iom_object::getXmleleAttrName(int index)
{
	return xmleleidxv.at(index).first;
}

int iom_object::getXmleleValueIdx(int index)
{
	return xmleleidxv.at(index).second;
}

/** sets the value of an attribute to undefined. 
 */
void iom_object::setAttrUndefined(int attrName)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found
	}else{
		// found, remove
		attrValuev.erase(attr); // TODO free strings
	}
}

/** sets the value of a primitive type attribute. 
 *  If value==0, the attribute is set to undefined.
 */
void iom_object::setAttrValue(int attrName,const XMLCh *value)
{
	
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found, add
		if(value){
			valuev_type valuev;
			valuev.push_back(iom_value(XMLString::replicate(value)));
			attrValuev[attrName]=valuev;
		}
	}else{
		// found, replace
		if(value){
			valuev_type valuev=attr->second;
			valuev.clear(); // TODO free strings
			valuev.push_back(iom_value(XMLString::replicate(value)));
			attrValuev[attrName]=valuev;
		}else{
			attrValuev.erase(attr); // TODO free strings
		}
	}
}

/** gets the value of a primitive type attribute.
 */
const XMLCh *iom_object::getAttrValue(int attrName)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found
		return 0;
	}
	// found
	valuev_type valuev=attr->second;
	iom_value value=valuev.at(0);
	return value.getStr();
}

int iom_object::getAttrCount()
{
	return attrValuev.size();
}

int iom_object::getAttrName(int index)
{
	attrValuev_type::iterator attri=attrValuev.begin();
	int i=0;
	while(attri!=attrValuev.end() && i<=index){
		if(i==index){
			return attri->first;
		}
		attri++;
		i++;
	}
	return 0;
}

int iom_object::getAttrValueCount(int attrName)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found
		return 0;
	}
	// found
	valuev_type valuev=attr->second;
	return valuev.size();
}

const XMLCh *iom_object::getAttrPrim(int attrName,int index)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found
		return 0;
	}
	// found
	valuev_type valuev=attr->second;
	iom_value value=valuev.at(index);
	return value.getStr();
}

/** get value at given index.
 */
IomObject iom_object::getAttrObj(int attrName,int index)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found
		return 0;
	}
	// found
	valuev_type valuev=attr->second;
	iom_value value=valuev.at(index);
	return value.getObj();
}

/** change value at given index
*/
void iom_object::setAttrObj(int attrName,int index,IomObject value)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found, add
		valuev_type valuev;
		valuev.push_back(iom_value(value));
		attrValuev[attrName]=valuev;
	}else{
		// found, replace
		valuev_type valuev=attr->second;
		valuev.at(index)=iom_value(value);
		attrValuev[attrName]=valuev;
	}
}

/** insert value a given index.
*/
void iom_object::insertAttrObj(int attrName,int index,IomObject value)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found, add
		valuev_type valuev;
		valuev.push_back(iom_value(value));
		attrValuev[attrName]=valuev;
	}else{
		// found, add
		valuev_type valuev=attr->second;
		valuev.insert(valuev.begin()+index,iom_value(value));
		attrValuev[attrName]=valuev;
	}
}

/** add value to end of list.
*/
void iom_object::addAttrObj(int attrName,IomObject value)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found, add
		valuev_type valuev;
		valuev.push_back(iom_value(value));
		attrValuev[attrName]=valuev;
	}else{
		// found, add
		valuev_type valuev=attr->second;
		valuev.push_back(iom_value(value));
		attrValuev[attrName]=valuev;
	}
}

/** remove value with given index
*/
void iom_object::removeAttrObj(int attrName,int index)
{
	attrValuev_type::iterator attr=attrValuev.find(attrName);
	if(attr==attrValuev.end()){
		// not found
	}else{
		// found, remove
		valuev_type valuev=attr->second;
		valuev.erase(valuev.begin()+index);
		attrValuev[attrName]=valuev;
	}
}

IomObject::IomObject(struct iom_object *pointee1) 
: pointee(pointee1 ? pointee1->getRef() : 0){
}
IomObject::IomObject(const IomObject& src) 
: pointee(src.pointee ? src.pointee->getRef() : 0){
}
IomObject& IomObject::operator=(const IomObject& src){
	if(this!=&src){
		if(pointee && !pointee->freeRef()){
			delete pointee;
		}
		pointee=src.pointee ? src.pointee->getRef() : 0;
	}
	return *this;
}
IomObject::~IomObject(){
	if(pointee && !pointee->freeRef()){
		delete pointee;
	}
}

