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
 * implementation of error utilities
 * @defgroup error error handling functions
 * @{
 */

#include <iostream>
#include <string>
#include <stdlib.h>

#include <iom/iom_p.h>

IomBasket ErrorUtility::errs;
int ErrorUtility::errc=0;
XMLCh ErrorUtility::itoabuf[40];
IOM_ERRLISTENER ErrorUtility::listener=iom_stderrlistener;


void ErrorUtility::notifyerr(IomObject obj)
{
	if(listener){
		(*listener)(&(*obj)); // do not increment useCount!
	}
}

void ErrorUtility::init()
{
	if(errs.isNull()){
		errs=dbgnew iom_basket();
	}
}

/** cleanup error module. This function is a part of iom_end().
 */
void ErrorUtility::at_iom_end()
{
	if(!errs.isNull()){
		errs=0;
	}
}

/** issues a any object that denotes an error.
 */
extern "C" void iom_issueanyerr(IOM_OBJECT err)
{
	ErrorUtility::init();
	IomObject obj(err);
	ErrorUtility::errs->addObject(obj);
	ErrorUtility::notifyerr(obj);
}


/** issues a general IOM error.
 */
extern "C" void iom_issueerr(const char *message)
{
	ErrorUtility::init();
	IomObject obj(dbgnew iom_object());
	XMLString::binToText( ErrorUtility::errc++,ErrorUtility::itoabuf,sizeof(ErrorUtility::itoabuf)-1,10);
	obj->setOid(ErrorUtility::itoabuf);
	obj->setTag(ParserHandler::getTagId(X("iomerr04.errors.Error")));
	obj->setAttrValue(ParserHandler::getTagId("message"),X(message));
	ErrorUtility::errs->addObject(obj);
	ErrorUtility::notifyerr(obj);
	
}

/** issues a post parsing error.
 */
extern "C" void iom_issuesemerr(const char *message,IOM_OID bid,IOM_OID oid)
{
	ErrorUtility::init();
	IomObject obj(dbgnew iom_object());
	XMLString::binToText( ErrorUtility::errc++,ErrorUtility::itoabuf,sizeof(ErrorUtility::itoabuf)-1,10);
	obj->setOid(ErrorUtility::itoabuf);
	obj->setTag(ParserHandler::getTagId(X("iomerr04.errors.SemanticError")));
	obj->setAttrValue(ParserHandler::getTagId("message"),X(message));
	obj->setAttrValue(ParserHandler::getTagId("bid"),X(bid));
	if(oid){
		obj->setAttrValue(ParserHandler::getTagId("oid"),X(oid));
	}
	ErrorUtility::errs->addObject(obj);
	ErrorUtility::notifyerr(obj);

}

/** issues an XML parse error or warning..
 */
extern "C" void iom_issueparserr(const char *message,int kind,int line,int col)
{
	ErrorUtility::init();
	IomObject obj(dbgnew iom_object());
	XMLString::binToText( ErrorUtility::errc++,ErrorUtility::itoabuf,sizeof(ErrorUtility::itoabuf)-1,10);
	obj->setOid(ErrorUtility::itoabuf);
	obj->setTag(ParserHandler::getTagId(X("iomerr04.errors.XmlParseError")));
	obj->setAttrValue(ParserHandler::getTagId("message"),X(message));
	const char *kind_c;
	switch(kind){
	case IOM_ERRKIND_XMLPARSER:
        kind_c="XmlParser";
		break;
    case IOM_ERRKIND_MISSING:
        kind_c="Missing";
		break;
    case IOM_ERRKIND_INVALID:
        kind_c="Invalid";
		break;
    case IOM_ERRKIND_OTHER:
	default:
        kind_c="Other";
		break;
	}
	obj->setAttrValue(ParserHandler::getTagId("kind"),X(kind_c));
	XMLString::binToText( line,ErrorUtility::itoabuf,sizeof(ErrorUtility::itoabuf)-1,10);
	obj->setAttrValue(ParserHandler::getTagId("line"),ErrorUtility::itoabuf);
	XMLString::binToText( col,ErrorUtility::itoabuf,sizeof(ErrorUtility::itoabuf)-1,10);
	obj->setAttrValue(ParserHandler::getTagId("col"),ErrorUtility::itoabuf);
	ErrorUtility::errs->addObject(obj);
	ErrorUtility::notifyerr(obj);
}

/** sets a new error listener.
 * returns the old or 0.
 */
extern "C" IOM_ERRLISTENER iom_seterrlistener(IOM_ERRLISTENER newlistener)
{
	IOM_ERRLISTENER old=ErrorUtility::listener;
	ErrorUtility::listener=newlistener;
	return old;
}

/** error listener that dumps all errors to stderr.
 *  Can be used in a iom_eterrlistener() call.
 */
extern "C" void iom_stderrlistener(IOM_OBJECT errobj1)
{
	IomObject errobj(errobj1);
	if(errobj->getTag()==ParserHandler::getTagId(X("iomerr04.errors.Error"))){
		std::cerr << StrX(errobj->getAttrValue(ParserHandler::getTagId(X("message")))) << std::endl;
	}else if(errobj->getTag()==ParserHandler::getTagId(X("iomerr04.errors.XmlParseError"))){
		std::cerr << StrX(errobj->getAttrValue(ParserHandler::getTagId(X("kind")))) << ", ";
		std::cerr << StrX(errobj->getAttrValue(ParserHandler::getTagId(X("line")))) << ", ";
		std::cerr << StrX(errobj->getAttrValue(ParserHandler::getTagId(X("col")))) << ": ";
		std::cerr << StrX(errobj->getAttrValue(ParserHandler::getTagId(X("message")))) << std::endl;
	}else if(errobj->getTag()==ParserHandler::getTagId(X("iomerr04.errors.SemanticError"))){
		std::cerr << "basket " << StrX(errobj->getAttrValue(ParserHandler::getTagId(X("bid"))));
		const XMLCh *oid=errobj->getAttrValue(ParserHandler::getTagId(X("oid")));
		if(oid){
			std::cerr << ", object " << StrX(oid);
		}
		std::cerr << ": " << StrX(errobj->getAttrValue(ParserHandler::getTagId(X("message")))) << std::endl;
	}else{
		std::cerr << "ERROR: " << errobj->getTag_c() << std::endl;
		errobj->dumpAttrs();
	}
}

/** @}
 */

