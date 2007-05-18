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

