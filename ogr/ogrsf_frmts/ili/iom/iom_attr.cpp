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
 * implementation of object attribute
 * @defgroup attribute attribute level functions
 * @{
 */
#include <string.h>
#include <iom/iom_p.h>

/** gets the number of attributes, roles and embedded roles 
 * of an object.
 */
extern "C" int iom_getattrcount(IOM_OBJECT object)
{
	return object->getAttrCount();
}

/** gets the name of an attribute, role or embedded role.
 *  returns a pointer to a static buffer.
 */
extern "C" IOM_TAG iom_getattrname(IOM_OBJECT object,int index)
{
	static char *name=0;
	int tag=object->getAttrName(index);
	if(name){
		XMLString::release(&name);
	}
	const XMLCh *const name_w=ParserHandler::getTagName(tag);
	name=XMLString::transcode(name_w);
	return name;
}

/** gets the number of values of an attribute.
 */
extern "C" int iom_getattrvaluecount(IOM_OBJECT object,IOM_TAG attrName)
{
	int tag=ParserHandler::getTagId(attrName);
	return object->getAttrValueCount(tag);
}

/** gets the value of a primitive type attribute. 
 * returns 0 if the attribute doesn't exist or has no value
 * Returns a pointer to a static buffer.
 */
extern "C" char *iom_getattrvalue(IOM_OBJECT object,IOM_TAG attrName)
{
	static char *value=0;
	if(value){
		XMLString::release(&value);
	}
	int nameId=ParserHandler::getTagId(attrName);
	if(!nameId) return 0;
	const XMLCh *ret=object->getAttrValue(nameId);
	if(!ret)return 0;
	value=XMLString::transcode(ret);
	return value;
}

/** gets the value of a primitive type attribute. 
 * returns 0 if the attribute doesn't exist or has no value
 * Returns a pointer to a static buffer.
 */
extern "C" char *iom_getattrvalueUTF8(IOM_OBJECT object,IOM_TAG attrName)
{
	static char *value=0;
	if(value){
		XMLString::release(&value);
	}
	int nameId=ParserHandler::getTagId(attrName);
	if(!nameId) return 0;
	const XMLCh *ret=object->getAttrValue(nameId);
	if(!ret)return 0;
	value=iom_toUTF8(ret);
	return value;
}

/** sets the value of a primitive type attribute. 
 *  If value==0, the attribute is set to undefined.
 */
extern "C" void iom_setattrvalue(IOM_OBJECT object,IOM_TAG attrName,const char *value)
{
	if(value){
		object->setAttrValue(ParserHandler::getTagId(attrName),X(value));
	}else{
		object->setAttrValue(ParserHandler::getTagId(attrName),0);
	}
}
/** sets the value of a primitive type attribute. 
 *  If value==0, the attribute is set to undefined.
 */
extern "C" void iom_setattrvalueUTF8(IOM_OBJECT object,IOM_TAG attrName,const char *value)
{
	if(value){
		XMLCh *unicodeForm=iom_fromUTF8(value);
		object->setAttrValue(ParserHandler::getTagId(attrName),unicodeForm);
		XMLString::release(&unicodeForm);
	}else{
		object->setAttrValue(ParserHandler::getTagId(attrName),0);
	}
}

/** sets the attribute to undefined.
 */
extern "C" void iom_setattrundefined(IOM_OBJECT object,IOM_TAG attrName)
{
	object->setAttrUndefined(ParserHandler::getTagId(attrName));
}

/** gets the value of a primitive type attribute. 
 * returns 0 if the attribute doesn't exist or has no value
 * Returns a pointer to a static buffer.
 */
extern "C" char *iom_getattrprim(IOM_OBJECT object,IOM_TAG attrName,int index)
{
	static char *value=0;
	if(value){
		XMLString::release(&value);
	}
	int tag=ParserHandler::getTagId(attrName);
	const XMLCh *ret=object->getAttrPrim(tag,index);
	if(!ret)return 0;
	value=XMLString::transcode(ret);
	return value;
}
/** gets the value of a primitive type attribute. 
 * returns 0 if the attribute doesn't exist or has no value
 * Returns a pointer to a static buffer.
 */
extern "C" char *iom_getattrprimUTF8(IOM_OBJECT object,IOM_TAG attrName,int index)
{
	static char *value=0;
	if(value){
		XMLString::release(&value);
	}
	int tag=ParserHandler::getTagId(attrName);
	const XMLCh *ret=object->getAttrPrim(tag,index);
	if(!ret)return 0;
	value=iom_toUTF8(ret);
	return value;
}

/** gets the value of a object type attribute. 
 * returns 0 if the attribute doesn't exist or has no value
 * It is the responsibility of the caller to release the returned object.
 */
extern "C" IOM_OBJECT iom_getattrobj(IOM_OBJECT object,IOM_TAG attrName,int index)
{
	int tag=ParserHandler::getTagId(attrName);
	IomObject ret=object->getAttrObj(tag,index);
	return ret.isNull() ? 0 : ret->getRef();
}

/** replaces the value of an object type attribute.
 */
extern "C" IOM_OBJECT iom_changeattrobj(IOM_OBJECT object,IOM_TAG attrName,int index,IOM_TAG type)
{
	IomObject ret=new iom_object();
	ret->setTag(ParserHandler::getTagId(type));
	object->setAttrObj(ParserHandler::getTagId(attrName),index,ret);
	return ret->getRef();
}

/** insert a new value of an object type attribute.
 */
extern "C" IOM_OBJECT iom_insertattrobj(IOM_OBJECT object,IOM_TAG attrName,int index,IOM_TAG type)
{
	IomObject ret=new iom_object();
	ret->setTag(ParserHandler::getTagId(type));
	object->insertAttrObj(ParserHandler::getTagId(attrName),index,ret);
	return ret->getRef();
}

/** add a new value of an object type attribute to end of list.
 */
extern "C" IOM_OBJECT iom_addattrobj(IOM_OBJECT object,IOM_TAG attrName,IOM_TAG type)
{
	IomObject ret=new iom_object();
	ret->setTag(ParserHandler::getTagId(type));
	object->addAttrObj(ParserHandler::getTagId(attrName),ret);
	return ret->getRef();
}

/** remove a value of an object type attribute from the list.
 */
extern "C" void iom_deleteattrobj(IOM_OBJECT object,IOM_TAG attrName,int index)
{
	object->removeAttrObj(ParserHandler::getTagId(attrName),index);
}

/** gets the number of xml-elements of an object. 
 * This function can only be called after reading a file.
 */
extern "C" int iom_getxmlelecount(IOM_OBJECT object)
{
	return object->getXmleleCount();
}

/** gets the attribute name of an xml-element of an object. 
 * To get the value use iom_getattrprim(), iom_getattrobj().
 * This function can only be called after reading a file.
 */
extern "C" IOM_TAG iom_getxmleleattrname(IOM_OBJECT object,int index)
{
	static char *name=0;
	int tag=object->getXmleleAttrName(index);
	if(name){
		XMLString::release(&name);
	}
	const XMLCh *const name_w=ParserHandler::getTagName(tag);
	name=XMLString::transcode(name_w);
	return name;
}

/** gets the index of the value of an xml-element of an object. 
 * This function can only be called after reading a file.
 * To get the value use iom_getattrprim(), iom_getattrobj().
 */
extern "C" int iom_getxmlelevalueidx(IOM_OBJECT object,int index)
{
	return object->getXmleleValueIdx(index);
}

/** gets the value of a primitive type attribute. 
 * returns 0 if the attribute doesn't exist or has no value
 * Returns a pointer to a static buffer.
 */
extern "C" char *iom_getxmleleprim(IOM_OBJECT object,int index)
{
	static char *value=0;
	if(value){
		XMLString::release(&value);
	}
	int tag=object->getXmleleAttrName(index);
	int val_index=object->getXmleleValueIdx(index);
	const XMLCh *ret=object->getAttrPrim(tag,val_index);
	if(!ret)return 0;
	value=XMLString::transcode(ret);
	return value;
}
/** gets the value of a primitive type attribute. 
 * returns 0 if the attribute doesn't exist or has no value
 * Returns a pointer to a static buffer.
 */
extern "C" char *iom_getxmleleprimUTF8(IOM_OBJECT object,int index)
{
	static char *value=0;
	if(value){
		XMLString::release(&value);
	}
	int tag=object->getXmleleAttrName(index);
	int val_index=object->getXmleleValueIdx(index);
	const XMLCh *ret=object->getAttrPrim(tag,val_index);
	if(!ret)return 0;
	value=iom_toUTF8(ret);
	return value;
}

/** gets the value of a object type attribute. 
 * returns 0 if the attribute doesn't exist or has no value
 * It is the responsibility of the caller to release the returned object.
 */
extern "C" IOM_OBJECT iom_getxmleleobj(IOM_OBJECT object,int index)
{
	int tag=object->getXmleleAttrName(index);
	int val_index=object->getXmleleValueIdx(index);
	return object->getAttrObj(tag,val_index)->getRef();
}

/** @}
 */

