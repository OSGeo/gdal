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
 * adapter to xml parser 
 * @defgroup reader xml reader functions
 * @{
 */

#include <cassert>
#include <cstring>

#include <xercesc/util/XMLString.hpp>
#include <xercesc/sax/Locator.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/Attributes.hpp>

#include <iom/iom_p.h>

XMLStringPool *ParserHandler::namev=0;


/** read complete HEADERSECTION
 *  Requires: call to setFilename()
 */
int iom_file::readHeader(const char *model)
{
	handler=dbgnew class ParserHandler(this,model);
	parser=XMLReaderFactory::createXMLReader();
	// Do not report validation errors
    parser->setFeature(XMLUni::fgSAX2CoreValidation, false);
	// use scanner that performs well-formedness checking only.
	parser->setProperty(XMLUni::fgXercesScannerName,(void *)XMLUni::fgWFXMLScanner);
    parser->setContentHandler(handler);
    parser->setEntityResolver(handler);
    parser->setErrorHandler(handler);
	return readLoop(filename);
}


int iom_file::readLoop(const char *filename)
{
	try
    {
		if (!parser->parseFirst(filename, token))
		{
			iom_issueerr("scanFirst() failed");
			return IOM_ERR_XMLPARSER;
		}
		bool gotMore = true;
		while (gotMore && !parser->getErrorCount()){
			gotMore = parser->parseNext(token);
		}
		parser->parseReset(token);
	}
	catch (const XMLException& toCatch)
    {
            char* message = XMLString::transcode(toCatch.getMessage());
			iom_issueerr(message);
            XMLString::release(&message);
            return IOM_ERR_XMLPARSER;
    }
    catch (const SAXException& toCatch)
    {
            char* message = XMLString::transcode(toCatch.getMessage());
			iom_issueerr(message);
            XMLString::release(&message);
            return IOM_ERR_XMLPARSER;
    }
	return 0;
}


/** read one (next) basket
 */
int iom_file::readBasket(IomFile file)
{
	return 1;
}

/** sets filename
 */
void iom_file::setFilename(const char *filename1)
{
	if(filename)free((void *)filename);
	filename=strdup(filename1);
}

/** @}
 */

//#include <xercesc/util/XMLUniDefs.hpp>
//#include <xercesc/util/XMLUni.hpp>
//#include <xercesc/sax/AttributeList.hpp>

Element::Element()
	: object(0)
	, propertyName(0)
	, oid(0)
	, bid(0)
	, orderPos(0)
{
}
Element::Element(const Element& src) 
	: object(src.object)
	, propertyName(src.propertyName)
	, oid(XMLString::replicate(src.oid))
	, bid(XMLString::replicate(src.bid))
	, orderPos(src.orderPos)
{
}
Element& Element::operator=(const Element& src){
	if(this!=&src){
		object=src.object;
		if(bid){
			XMLString::release(&bid);
		}
		if(src.bid){
			bid=XMLString::replicate(src.bid);
		}
		if(oid){
			XMLString::release(&oid);
		}
		if(src.oid){
			oid=XMLString::replicate(src.oid);
		}
		propertyName=src.propertyName;
		orderPos=src.orderPos;
	}
	return *this;
}
Element::~Element(){
	if(bid)XMLString::release(&bid);
	if(oid)XMLString::release(&oid);
}

const XMLCh *Element::getOid()
{
	return oid;
}

void Element::setOid(const XMLCh *oid1)
{
	if(oid)XMLString::release(&oid);
	oid=XMLString::replicate(oid1);
}

const XMLCh *Element::getBid()
{
	return bid;
}

void Element::setBid(const XMLCh *bid1)
{
	if(bid)XMLString::release(&bid);
	bid=XMLString::replicate(bid1);
}

unsigned int Element::getOrderPos()
{
	return orderPos;
}

void Element::setOrderPos(unsigned int value)
{
	orderPos=value;
}

static const XMLCh* stripX(const XMLCh* value)
{
	if(XMLString::startsWith(value,X("x"))){
		return value+1;
	}
	return value;
}

ParserHandler::ParserHandler(struct iom_file *inputfile,const char* model1) 
	: locator(0)
	,file(inputfile)
	,model(model1 ? strdup(model1) : 0)
	,skip(0)
	,level(0)
	,state(BEFORE_TRANSFER)
	,dataContainer(0)
	,object(0)
	,m_nEntityCounter(0)
{
	//  setupTag2MetaobjMapping();
}

ParserHandler::~ParserHandler()
{
	if(model){
		free(model);
		model=0;
	}
}

bool xisClassDef(int tag)
{
	const XMLCh *const type=ParserHandler::getTagName(tag);
	if(!XMLString::compareString(type,X("iom04.metamodel.Table"))){
		return true;
	}
	return false;
}
bool xisAssociationDef(int tag)
{
	const XMLCh *const type=ParserHandler::getTagName(tag);
	if(!XMLString::compareString(type,X("iom04.metamodel.AssociationDef"))){
		return true;
	}
	return false;
}
bool xisTopicDef(int tag)
{ 
	const XMLCh *const type=ParserHandler::getTagName(tag);
	if(!XMLString::compareString(type,X("iom04.metamodel.Topic"))){
		return true;
	}
	return false;
}

void ParserHandler::startEntity (const XMLCh *const name)
{
    m_nEntityCounter ++;
    if (m_nEntityCounter > 1000)
    {
        throw SAXNotSupportedException ("File probably corrupted (million laugh pattern)");
    }
}

void  ParserHandler::startElement (const XMLCh *const uri
									, const XMLCh *const localname
									, const XMLCh *const qname
									, const Attributes &attrs)
{
    	level++;
        m_nEntityCounter = 0;
    	if(skip>0){
    		skip++;
    		return;
    	}
		int tag=getTagId(localname);
		if(state==BEFORE_TRANSFER && tag==tags::get_TRANSFER()){
    		state=BEFORE_DATASECTION;
    		return;
    	}
		if(state==BEFORE_DATASECTION && tag==tags::get_HEADERSECTION()){
			const XMLCh* sender=0;
			const XMLCh* version=0;
			for(unsigned int attri=0;attri<attrs.getLength();attri++){
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_SENDER())){
					sender=attrs.getValue(attri);
				}
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_VERSION())){
					version=attrs.getValue(attri);
				}

			}
			if(!sender){
				// SENDER is mandatory
				iom_issueparserr("Attribute SENDER missing in file ",IOM_ERRKIND_MISSING,locator->getLineNumber(),locator->getColumnNumber());
			}else{
				file->setHeadSecSender(sender);
			}
			if(!version){
				// VERSION is mandatory
				iom_issueparserr("Attribute VERSION missing in file ",IOM_ERRKIND_MISSING,locator->getLineNumber(),locator->getColumnNumber());
			}else{
				file->setHeadSecVersion(version);
				if (XMLString::compareString(version,X("2.2")))
				{
					iom_issueparserr("The VERSION attribute must be \"2.2\"",IOM_ERRKIND_INVALID,locator->getLineNumber(),locator->getColumnNumber());
				}

			}
    		state=START_HEADERSECTION;
    		return;
    	}

		if(state==BEFORE_DATASECTION && tag==tags::get_DATASECTION()){
    		state=BEFORE_BASKET;
    		return;
    	}
    	if(state==BEFORE_BASKET){
			const XMLCh* bid=0;
			const XMLCh* consistency=0;
			for(unsigned int attri=0;attri<attrs.getLength();attri++){
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_BID())){
					bid=attrs.getValue(attri);
				}
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_CONSISTENCY())){
					consistency=attrs.getValue(attri);
				}
			}
			dataContainer=dbgnew struct iom_basket();
			dataContainer->setXMLLineNumber(locator->getLineNumber());
			dataContainer->setXMLColumnNumber(locator->getColumnNumber());
			if(!bid){
				// BOID is mandatory
				iom_issueparserr("Attribute BID missing in basket ",IOM_ERRKIND_MISSING,locator->getLineNumber(),locator->getColumnNumber());
			}else{
				dataContainer->setOid(stripX(bid));
			}
			if(consistency){
				if(!XMLString::compareString(consistency,ustrings::get_COMPLETE())){
				    dataContainer->setConsistency(IOM_COMPLETE);
				}else if(!XMLString::compareString(consistency,ustrings::get_INCOMPLETE())){
				    dataContainer->setConsistency(IOM_INCOMPLETE);
				}else if(!XMLString::compareString(consistency,ustrings::get_INCONSISTENT())){
				    dataContainer->setConsistency(IOM_INCONSISTENT);
				}else if(!XMLString::compareString(consistency,ustrings::get_ADAPTED())){
				    dataContainer->setConsistency(IOM_ADAPTED);
				}else{
					iom_issueparserr("Attribute CONSISTENCY has wrong value in basket ",IOM_ERRKIND_INVALID,locator->getLineNumber(),locator->getColumnNumber());
				}
			}
			dataContainer->setTag(tag);
			dataContainer->file=file;
		  	state=BEFORE_OBJECT;
			return;
    	}
    	// SegmentSequence
		if(state==SS_AFTER_COORD){
			pushReturnState(SS_AFTER_COORD);
			if(tag==tags::get_COORD()){
				state=CV_COORD;
				object=dbgnew struct iom_object();
				object->setTag(tags::get_COORD());
			}else{
				state=ST_BEFORE_PROPERTY;
				object=dbgnew struct iom_object();
				object->setTag(tag);
			}
			return;
		}
    	
    	// PolylineValue
		if((state==PV_POLYLINE 
				|| state==PV_AFTER_LINEATTR) 
				&& tag==tags::get_CLIPPED()){
			state=PV_CLIPPED;
			changeReturnState(PV_AFTER_CLIPPED);
			object->setConsistency(IOM_INCOMPLETE);
			return;
		}
		if(state==PV_POLYLINE && tag==tags::get_LINEATTR()){
			state=PV_LINEATTR;
			return;
		}
		if(state==PV_LINEATTR){
			pushReturnState(PV_AFTER_LINEATTRSTRUCT);
			state=ST_BEFORE_PROPERTY;
			object=dbgnew struct iom_object();
			object->setTag(tag);
			return;
		}
		if(state==PV_AFTER_CLIPPED && tag==tags::get_CLIPPED()){
			state=PV_CLIPPED;
			pushReturnState(PV_AFTER_CLIPPED);
			return;
		}
		if((state==PV_POLYLINE 
				|| state==PV_CLIPPED 
				|| state==PV_AFTER_LINEATTR)
				&& tag==tags::get_COORD()){
			pushReturnState(SS_AFTER_COORD);
			object=dbgnew struct iom_object();
			object->setTag(tags::get_SEGMENTS());
			Element ele;
			ele.object=object;
			ele.propertyName=tags::get_segment();
			objStack.push(ele);
			state=CV_COORD;
			object=dbgnew struct iom_object();
			object->setTag(tags::get_COORD());
			return;
		}
		
		// SurfaceValue
		if(state==SV_SURFACE && tag==tags::get_CLIPPED()){
			state=SV_CLIPPED;
			changeReturnState(SV_AFTER_CLIPPED);
			Element top=objStack.top();objStack.pop();
			Element ele=objStack.top();
			objStack.push(top);
			ele.object->setConsistency(IOM_INCOMPLETE);
			return;
		}
		if(state==SV_AFTER_CLIPPED && tag==tags::get_CLIPPED()){
			pushReturnState(SV_AFTER_CLIPPED);
			state=SV_CLIPPED;
			object=dbgnew struct iom_object();
			object->setTag(tags::get_SURFACE());
			Element ele;
			ele.object=object;
			ele.propertyName=tags::get_boundary();
			objStack.push(ele);
			return;
		}
		if((state==SV_SURFACE || state==SV_CLIPPED 
				|| state==BD_AFTER_BOUNDARY)
				&& tag==tags::get_BOUNDARY()){
			object=dbgnew struct iom_object();
			object->setTag(tags::get_BOUNDARY());
			Element ele;
			ele.object=object;
			ele.propertyName=tags::get_polyline();
			objStack.push(ele);
			state=BD_BOUNDARY;
			return;
		}
		if((state==BD_BOUNDARY || state==BD_AFTER_POLYLINE)
				&& tags::get_POLYLINE()){
			pushReturnState(BD_AFTER_POLYLINE);
			state=PV_POLYLINE;
			object=dbgnew struct iom_object();
			object->setTag(tags::get_POLYLINE());
			Element ele;
			ele.object=object;
			ele.propertyName=tags::get_sequence();
			objStack.push(ele);
			return;
		}
    	
    	// CoordValue
		if(state==CV_COORD && tag==tags::get_C1()){
			state=CV_C1;
			// ensure we save collected characters only inside C1
			propertyValue.reset();
			return;
		}
		if(state==CV_AFTER_C1 && tag==tags::get_C2()){
			state=CV_C2;
			// ensure we save collected characters only inside C2
			propertyValue.reset();
			return;
		}
		if(state==CV_AFTER_C2 && tag==tags::get_C3()){
			state=CV_C3;
			// ensure we save collected characters only inside C3
			propertyValue.reset();
			return;
		}
		if(state==ST_BEFORE_CHARACTERS && tag==tags::get_SURFACE()){
			pushReturnState(ST_AFTER_SURFACE);
			state=SV_SURFACE;
			object=dbgnew struct iom_object();
			object->setTag(tags::get_MULTISURFACE());
			Element ele;
			ele.object=object;
			ele.propertyName=tags::get_surface();
			objStack.push(ele);
			object=dbgnew struct iom_object();
			object->setTag(tags::get_SURFACE());
			//ele=ceisnew Element();
			ele.object=object;
			ele.propertyName=tags::get_boundary();
			objStack.push(ele);
			return;
		}
		if(state==ST_BEFORE_CHARACTERS && tag==tags::get_POLYLINE()){
			pushReturnState(ST_AFTER_POLYLINE);
			state=PV_POLYLINE;
			object=dbgnew struct iom_object();
			object->setTag(tags::get_POLYLINE());
			Element ele;
			ele.object=object;
			ele.propertyName=tags::get_sequence();
			objStack.push(ele);
			return;
		}
		if(state==ST_BEFORE_CHARACTERS && tag==tags::get_COORD()){
			pushReturnState(ST_AFTER_COORD);
			state=CV_COORD;
			object=dbgnew struct iom_object();
			object->setTag(tags::get_COORD());
			return;
		}

    	if(state==BEFORE_OBJECT || state==ST_AFTER_STRUCTVALUE 
			|| state==ST_BEFORE_CHARACTERS 
			|| state==ST_BEFORE_EMBASSOC){

		    // start StructValue
			if(state==BEFORE_OBJECT){
				pushReturnState(BEFORE_OBJECT);	      		
			}else if(state==ST_AFTER_STRUCTVALUE){
				pushReturnState(ST_AFTER_STRUCTVALUE);	      		
			}else if(state==ST_BEFORE_CHARACTERS){
				pushReturnState(ST_AFTER_STRUCTVALUE);	      		
			}else if(state==ST_BEFORE_EMBASSOC){
				pushReturnState(ST_BEFORE_EMBASSOC);	      		
			}
			state=ST_BEFORE_PROPERTY;
			const XMLCh* operation=0;
			const XMLCh* oid=0;
			const XMLCh* objBid=0;
			const XMLCh* consistency=0;
			for(unsigned int attri=0;attri<attrs.getLength();attri++){
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_TID())){
					oid=attrs.getValue(attri);
				}
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_BID())){
					objBid=attrs.getValue(attri);
				}
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_OPERATION())){
					operation=attrs.getValue(attri);
				}
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_CONSISTENCY())){
					consistency=attrs.getValue(attri);
				}
			}
			object=dbgnew struct iom_object();
		    object->setTag(tag);
			object->setXMLLineNumber(locator->getLineNumber());
			object->setXMLColumnNumber(locator->getColumnNumber());
			if(oid){
				object->setOid(stripX(oid));
			}
			if(objBid){
			    object->setBid(stripX(objBid));
			}			
			if(operation){
				if(!XMLString::compareString(operation,ustrings::get_INSERT())){
				    object->setOperation(IOM_OP_INSERT);
				}else if(!XMLString::compareString(operation,ustrings::get_UPDATE())){
				    object->setOperation(IOM_OP_UPDATE);
				}else if(!XMLString::compareString(operation,ustrings::get_DELETE())){
				    object->setOperation(IOM_OP_DELETE);
				}else{
					iom_issueparserr("Attribute OPERATION has wrong value in object ",IOM_ERRKIND_INVALID,locator->getLineNumber(),locator->getColumnNumber());
				}
			}
			if(consistency){
				if(!XMLString::compareString(consistency,ustrings::get_COMPLETE())){
				    object->setConsistency(IOM_COMPLETE);
				}else if(!XMLString::compareString(consistency,ustrings::get_INCOMPLETE())){
				    object->setConsistency(IOM_INCOMPLETE);
				}else if(!XMLString::compareString(consistency,ustrings::get_INCONSISTENT())){
				    object->setConsistency(IOM_INCONSISTENT);
				}else if(!XMLString::compareString(consistency,ustrings::get_ADAPTED())){
				    object->setConsistency(IOM_ADAPTED);
				}else{
					iom_issueparserr("Attribute CONSISTENCY has wrong value in object ",IOM_ERRKIND_INVALID,locator->getLineNumber(),locator->getColumnNumber());
				}
			}
			return;
    	}
    	if(state==ST_BEFORE_PROPERTY){
    		if(object.isNull()){
    			//throw new IllegalStateException();
				assert(false);
    		}
			// attribute ->characters 
			// struct ->startElement
			// ref (refattr) ->endElement
			// ref (role) ->endElement
			// ref (embedded assoc) ->startElement or EndElement
			const XMLCh* oid=0;
			const XMLCh* objBid=0;
			unsigned int orderPos=0;
			for(unsigned int attri=0;attri<attrs.getLength();attri++){
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_REF())){
					oid=attrs.getValue(attri);
				}
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_BID())){
					objBid=attrs.getValue(attri);
				}
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_EXTREF())){
					oid=attrs.getValue(attri);
				}
				if(!XMLString::compareString(attrs.getLocalName(attri),ustrings::get_ORDER_POS())){
					bool done=XMLString::textToBin(attrs.getValue(attri),orderPos);
					if(!done || orderPos==0){
						// illgegal value
						iom_issueparserr("Attribute ORDER_POS has wrong value in object ",IOM_ERRKIND_INVALID,locator->getLineNumber(),locator->getColumnNumber());
						orderPos=0;
					}
				}
			}
			// save name,oid,bid
			// push state
			Element ele;
			ele.object=object;
			ele.propertyName=tag;
			if(oid){
				if(objBid){
					ele.setBid(stripX(objBid));
				}
				ele.setOid(stripX(oid));
				ele.setOrderPos(orderPos);
			}
			objStack.push(ele);
			object=0;
			if(oid){
				state=ST_BEFORE_EMBASSOC;
			}else{
				state=ST_BEFORE_CHARACTERS;
			}
			// ensure we save collected characters only inside 
			// a property and not after a struct value
			propertyValue.reset(); 
			return;
    	}
    	skip=1;
}

void  ParserHandler::endElement (const XMLCh *const uri
								  , const XMLCh *const localname
								  , const XMLCh *const qname)
{
    	level--;
        m_nEntityCounter = 0;
		if(skip>0){
			skip--;
			return;
		}
		
		// SegmentSequence
		if(state==SS_AFTER_COORD){
			popReturnState();
			if(state==ST_AFTER_POLYLINE){
				Element ele=objStack.top();objStack.pop();
				object=ele.object; // SEGMENTS			
				ele=objStack.top();objStack.pop();
				ele.object->parser_addAttrValue(ele.propertyName,object);
				object=ele.object;	// POLYLINE		
			}else if(state==BD_AFTER_POLYLINE){
					Element ele=objStack.top();objStack.pop();
					object=ele.object; // SEGMENTS			
					ele=objStack.top();objStack.pop();
					ele.object->parser_addAttrValue(ele.propertyName,object);
					object=ele.object;	// POLYLINE		
					ele=objStack.top();
					ele.object->parser_addAttrValue(ele.propertyName,object);
					object=0;
			}else if(state==PV_AFTER_CLIPPED){
				Element ele=objStack.top();objStack.pop();
				object=ele.object;	// SEGMENTS		
				ele=objStack.top();
				ele.object->parser_addAttrValue(ele.propertyName,object);
				object=0;			
			}else{
				//throw new IllegalStateException();
				assert(false);
			}
		}else if(state==PV_AFTER_CLIPPED){
			Element ele=objStack.top();objStack.pop();
			object=ele.object;	// POLYLINE		
			state=ST_AFTER_POLYLINE;
		}else if(state==PV_AFTER_LINEATTRSTRUCT){
			state=PV_AFTER_LINEATTR;
		// Boundaries
		}else if(state==BD_AFTER_POLYLINE){
			Element ele=objStack.top();objStack.pop();
			object=ele.object; // BOUNDARY
			ele=objStack.top();			
			ele.object->parser_addAttrValue(ele.propertyName,object);
			//Dumper dumper=new Dumper();
			//dumper.dumpObject(System.err,ele.object);
			object=0;
			state=BD_AFTER_BOUNDARY;
		// SurfaceValue
		}else if(state==BD_AFTER_BOUNDARY){
			popReturnState();
			if(state==ST_AFTER_SURFACE){
				Element ele=objStack.top();objStack.pop();
				object=ele.object; // SURFACE
				ele=objStack.top();
				ele.object->parser_addAttrValue(ele.propertyName,object);
			}else if(state==SV_AFTER_CLIPPED){
				Element ele=objStack.top();objStack.pop();
				object=ele.object; // SURFACE
				ele=objStack.top();
				ele.object->parser_addAttrValue(ele.propertyName,object);
			}else{
				//throw new IllegalStateException("state "+state);
				assert(false);
			}
		}else if(state==SV_AFTER_CLIPPED){
			state=ST_AFTER_SURFACE;
		// CoordValue
		}else if(state==CV_AFTER_C1 || state==CV_AFTER_C2 || state==CV_AFTER_C3){
			popReturnState();
			if(state==SS_AFTER_COORD){
				// part of SEGMENTS
				Element ele=objStack.top();
				ele.object->parser_addAttrValue(ele.propertyName,object);
				//Dumper dumper=new Dumper();
				//dumper.dumpObject(System.err,ele.object);
				object=0;
			}else if(state==ST_AFTER_COORD){
			}else{
				//throw new IllegalStateException();
				assert(false);
			}			
		}else if(state==CV_C1){
			object->parser_addAttrValue(tags::get_C1(),propertyValue.getRawBuffer());
			propertyValue.reset();
			state=CV_AFTER_C1;
		}else if(state==CV_C2){
			object->parser_addAttrValue(tags::get_C2(),propertyValue.getRawBuffer());
			propertyValue.reset();
			state=CV_AFTER_C2;
		}else if(state==CV_C3){
			object->parser_addAttrValue(tags::get_C3(),propertyValue.getRawBuffer());
			propertyValue.reset();
			state=CV_AFTER_C3;
			
		// StructValue
		}else if(state==ST_AFTER_STRUCTVALUE 
				|| state==ST_BEFORE_CHARACTERS){
			// attribute
			// struct
			Element ele=objStack.top();objStack.pop();
			if(state==ST_BEFORE_CHARACTERS){
				// attribute
				// may be: illegal whitespace, legal whitespace, not whitespace
				ele.object->parser_addAttrValue(ele.propertyName,propertyValue.getRawBuffer());
    		}else{
				// bag of structvalues
				// added to ele.object in endElement of structvalue
    		}
			object=ele.object;
			propertyValue.reset();
			state=ST_BEFORE_PROPERTY;
    	}else if(state==ST_BEFORE_EMBASSOC){
				// ref (refattr)
				// ref (role)
				// ref (embedded assoc) with or without assocattrs
				Element ele=objStack.top();objStack.pop();
				if(object.isNull()){
					// ref (refattr)
					// ref (role)
					// ref (embedded assoc) without assocattrs
					object=dbgnew struct iom_object();
				}else{
					// ref (embedded assoc) with assocattrs
				}
				object->setRefOid(ele.getOid());
				object->setRefBid(ele.getBid());
				object->setRefOrderPos(ele.getOrderPos());
				ele.object->parser_addAttrValue(ele.propertyName,object);
				object=ele.object;
				propertyValue.reset();
				state=ST_BEFORE_PROPERTY;
		}else if(state==ST_AFTER_COORD){
			// attr of type COORD
			Element ele=objStack.top();objStack.pop();
			ele.object->parser_addAttrValue(ele.propertyName,object);
			object=ele.object;
			state=ST_BEFORE_PROPERTY;
			propertyValue.reset();
		}else if(state==ST_AFTER_POLYLINE){
			// attr of type POLYLINE
			Element ele=objStack.top();objStack.pop();
			ele.object->parser_addAttrValue(ele.propertyName,object);
			object=ele.object;			
			state=ST_BEFORE_PROPERTY;
			propertyValue.reset();
		}else if(state==ST_AFTER_SURFACE){
			// attr of type SURFACE/AREA
			Element ele=objStack.top();objStack.pop();
			object=ele.object; // MULTISURFACE
			ele=objStack.top();objStack.pop();
			ele.object->parser_addAttrValue(ele.propertyName,object);
			object=ele.object;			
			state=ST_BEFORE_PROPERTY;
			propertyValue.reset();
    	}else if(state==ST_BEFORE_PROPERTY){
			popReturnState();
			if(state==BEFORE_OBJECT){
				dataContainer->addObject(object);
				object=0;
			}else{
				if(state==ST_AFTER_STRUCTVALUE){
					Element ele=objStack.top();
					ele.object->parser_addAttrValue(ele.propertyName,object);
					object=0;
				}else if(state==PV_AFTER_LINEATTRSTRUCT){
					Element ele=objStack.top();
					ele.object->parser_addAttrValue(tags::get_lineattr(),object);
					object=0;
				}else if(state==SS_AFTER_COORD){
					// part of SEGMENTS
					Element ele=objStack.top();
					ele.object->parser_addAttrValue(ele.propertyName,object);
					//Dumper dumper=new Dumper();
					//dumper.dumpObject(System.err,ele.object);
					object=0;
				}
			}
    	}else if(state==BEFORE_OBJECT){
			file->addBasket(dataContainer);
			dataContainer=0;
    		state=BEFORE_BASKET;
    	}else if(state==BEFORE_BASKET){
    		state=BEFORE_DATASECTION;
    	}else if(state==START_HEADERSECTION){
    		state=BEFORE_DATASECTION;
    	}else if(state==BEFORE_DATASECTION){
    		state=BEFORE_TRANSFER;
    	}

}


#if XERCES_VERSION_MAJOR >= 3
/************************************************************************/
/*                     characters() (xerces 3 version)                  */
/************************************************************************/

void ParserHandler::characters( const XMLCh* const chars,
                                const XMLSize_t length )
{
    if(   state==ST_BEFORE_CHARACTERS
       || state==CV_C1
       || state==CV_C2
       || state==CV_C3 )
    {
        propertyValue.append(chars,length);
    }
}

#else
/************************************************************************/
/*                     characters() (xerces 2 version)                  */
/************************************************************************/

void ParserHandler::characters( const XMLCh* const chars,
                                const unsigned int length )
{
    if(   state==ST_BEFORE_CHARACTERS
       || state==CV_C1
       || state==CV_C2
       || state==CV_C3 )
    {
        propertyValue.append(chars,length);
    }
}
#endif

void ParserHandler::error(const SAXParseException& e)
{
#if 0
	std::cerr << "\nSAX Error at file " << StrX(e.getSystemId())
		 << ", line " << e.getLineNumber()
		 << ", char " << e.getColumnNumber()
         << "\n  Message: " << StrX(e.getMessage()) << std::endl;
#endif
	iom_issueparserr(StrX(e.getMessage()).localForm(),IOM_ERRKIND_XMLPARSER,e.getLineNumber(),e.getColumnNumber());
}

void ParserHandler::fatalError(const SAXParseException& e)
{
#if 0
	std::cerr << "\nSAX Error at file " << StrX(e.getSystemId())
		 << ", line " << e.getLineNumber()
		 << ", char " << e.getColumnNumber()
         << "\n  Message: " << StrX(e.getMessage()) << std::endl;
#endif
	iom_issueparserr(StrX(e.getMessage()).localForm(),IOM_ERRKIND_XMLPARSER,e.getLineNumber(),e.getColumnNumber());
}

void ParserHandler::warning(const SAXParseException& e)
{
#if 0
	std::cerr << "\nSAX Error at file " << StrX(e.getSystemId())
		 << ", line " << e.getLineNumber()
		 << ", char " << e.getColumnNumber()
         << "\n  Message: " << StrX(e.getMessage()) << std::endl;
#endif
	iom_issueparserr(StrX(e.getMessage()).localForm(),IOM_ERRKIND_XMLPARSER,e.getLineNumber(),e.getColumnNumber());
}

void  ParserHandler::setDocumentLocator (const Locator *const locator1)
{
	locator=locator1;
}


/** pushes a return state for the parser state machine to the stack.
 *  Used before entering a sub state machine.
 */
void ParserHandler::pushReturnState(int returnState){
	stateStack.push(returnState);
}

/** pops a state for the parser state machine from the stack and makes it the 
 *  new current state.
 *  Used when leaving a sub state machine.
 */
void ParserHandler::popReturnState(){
	state=stateStack.top();
	stateStack.pop();
}

/** changes the top state on the stack for the parser state machine.
 *  Used to change the return state of a sub state machine.
 */
void ParserHandler::changeReturnState(int returnState){
	stateStack.pop();
	stateStack.push(returnState);
}


/** gets the id of an xml-element name
 */
int ParserHandler::getTagId(const char *name)
{
	  if(!namev){
//FIXME		  namev=dbgnew XMLStringPool();
		  namev=new XMLStringPool();
	  }
	return namev->addOrFind(X(name));
}

/** gets the id of an xml-element name
 */
int ParserHandler::getTagId(const XMLCh *const name)
{
	  if(!namev){
		  namev=new XMLStringPool();
	  }
	return namev->addOrFind(name);
}

/** gets the id of an xml-element name
 */
const XMLCh *const ParserHandler::getTagName(int tagid)
{
	  if(!namev){
		  namev=new XMLStringPool();
	  }
	return namev->getValueForId(tagid);
}

/** cleanup reader module. This function is a part of iom_end().
 */
void ParserHandler::at_iom_end()
{
	  if(namev){
		  delete namev;
		  namev=0;
	  }
}
