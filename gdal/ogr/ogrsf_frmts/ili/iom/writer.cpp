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
 * adapter to xml writer
 * @defgroup writer xml writer functions
 * @{
 */


#include <string.h>

//#include <xercesc/util/TransService.hpp>
#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <iom/iom_p.h>
#include <algorithm>
#include <string>

/** gets the xml representaion of a consistency value.
 */
static const XMLCh *encodeConsistency(int consistency)
{
	const XMLCh *ret;
	switch(consistency){
	case IOM_INCOMPLETE:
        ret=ustrings::get_INCOMPLETE();
		break;
    case IOM_INCONSISTENT:
        ret=ustrings::get_INCONSISTENT();
		break;
    case IOM_ADAPTED:
        ret=ustrings::get_ADAPTED();
		break;
    case IOM_COMPLETE:
	default:
        ret=0;
		break;
	}
	return ret;
}

/** gets the xml representaion of a basket-kind value.
 */
static const XMLCh *encodeBasketKind(int kind)
{
	const XMLCh *ret;
	switch(kind){
	case IOM_UPDATE:
        ret=ustrings::get_UPDATE();
		break;
	case IOM_INITIAL:
        ret=ustrings::get_INITIAL();
		break;
    case IOM_FULL:
	default:
        ret=0;
		break;
	}
	return ret;
}

/** gets the xml representaion of a operation value.
 */
static const XMLCh *encodeOperation(int ops)
{
	const XMLCh *ret;
	switch(ops){
	case IOM_OP_UPDATE:
        ret=ustrings::get_UPDATE();
		break;
	case IOM_OP_DELETE:
        ret=ustrings::get_DELETE();
		break;
    case IOM_OP_INSERT:
	default:
        ret=0;
		break;
	}
	return ret;
}

/** writes a coord value or a coord segment.
 */
static void writeCoord(XmlWriter &out, IomObject &obj)
{
/*
     object: COORD
       C1
         102.0
       C2
         402.0
	<COORD><C1>102.0</C1><C2>402.0</C2></COORD>
*/
	out.startElement(tags::get_COORD(),0,0);
	out.startElement(tags::get_C1(),0,0);
	const XMLCh *c1=obj->getAttrPrim(tags::get_C1(),0);
	out.characters(c1);
	out.endElement(/*C1*/);
	const XMLCh *c2=obj->getAttrPrim(tags::get_C2(),0);
	if(c2){
		out.startElement(tags::get_C2(),0,0);
		out.characters(c2);
		out.endElement(/*C2*/);
		const XMLCh *c3=obj->getAttrPrim(tags::get_C3(),0);
		if(c3){
			out.startElement(tags::get_C3(),0,0);
			out.characters(c3);
			out.endElement(/*C3*/);
		}
	}
	out.endElement(/*COORD*/);

}

/** writes a arc segment value.
 */
static void writeArc(XmlWriter &out, IomObject &obj)
{
/*
     object: ARC
       C1
         103.0
       C2
         403.0
       A1
         104.0
       A2
         404.0
	<COORD><C1>103.0</C1><C2>403.0</C2><A1>104.0</A1><A2>404.0</A2></COORD>
*/
	out.startElement(tags::get_ARC(),0,0);
	out.startElement(tags::get_C1(),0,0);
	const XMLCh *c1=obj->getAttrPrim(tags::get_C1(),0);
	out.characters(c1);
	out.endElement(/*C1*/);
	const XMLCh *c2=obj->getAttrPrim(tags::get_C2(),0);
	out.startElement(tags::get_C2(),0,0);
	out.characters(c2);
	out.endElement(/*C2*/);
	const XMLCh *c3=obj->getAttrPrim(tags::get_C3(),0);
	if(c3){
		out.startElement(tags::get_C3(),0,0);
		out.characters(c3);
		out.endElement(/*C3*/);
	}
	const XMLCh *a1=obj->getAttrPrim(tags::get_A1(),0);
	out.characters(a1);
	out.endElement(/*A1*/);
	const XMLCh *a2=obj->getAttrPrim(tags::get_A2(),0);
	out.startElement(tags::get_A2(),0,0);
	out.characters(a2);
	out.endElement(/*A2*/);
	const XMLCh *r=obj->getAttrPrim(tags::get_R(),0);
	if(r){
		out.startElement(tags::get_R(),0,0);
		out.characters(r);
		out.endElement(/*R*/);
	}
	out.endElement(/*ARC*/);
}

/** writes a polyline value.
 */
void iom_file::writePolyline(XmlWriter &out, IomObject &obj,bool hasLineAttr)
{
/*
     object: POLYLINE [INCOMPLETE]
       lineattr
         object: Model.Topic.LineAttr
           attr00
             11
       sequence // if incomplete; multi sequence values
         object: SEGMENTS
           segment
             object: COORD
               C1
                 102.0
               C2
                 402.0
           segment
             object: ARC
               C1
                 103.0
               C2
                 403.0
               A1
                 104.0
               A2
                 404.0
           segment
             object: Model.SplineParam
               SegmentEndPoint
                 object: COORD
                   C1
                     103.0
                   C2
                     403.0
               p0
                 1.0
               p1
                 2.0

		<POLYLINE>
			<LINEATTR>
				<Model.Topic.LineAttr>
					<attr00>11</attr00>
				</Model.Topic.LineAttr>
			</LINEATTR>
			<COORD>
				<C1>101.0</C1>
				<C2>401.0</C2>
			</COORD>
			<COORD>
				<C1>102.0</C1>
				<C2>402.0</C2>
			</COORD>
			<Model.SplineParam>
				<SegmentEndPoint>
					<COORD>
						<C1>103.0</C1>
						<C2>403.0</C2>
					</COORD>
				</SegmentEndPoint>
				<p0>1.0</p0>
				<p1>2.0</p1>
			</Model.SplineParam>
		</POLYLINE>
*/
	out.startElement(tags::get_POLYLINE(),0,0);
	if(hasLineAttr){
		IomObject lineattr=obj->getAttrObj(tags::get_lineattr(),0);
		if(!lineattr.isNull()){
			out.startElement(tags::get_LINEATTR(),0,0);
			out.startElement(lineattr->getTag(),0,0);
			writeAttrs(out,lineattr);
			out.endElement(/*lineattr*/);
			out.endElement(/*LINEATTR*/);
		}
	}
	bool clipped=obj->getConsistency()==IOM_INCOMPLETE;
	for(int sequencei=0;sequencei<obj->getAttrValueCount(tags::get_sequence());sequencei++){
		if(clipped){
			out.startElement(tags::get_CLIPPED(),0,0);
		}else{
			// an unclipped polyline should have only one sequence element
			if(sequencei>0){
				iom_issueerr("unclipped polyline with multi 'sequence' elements");
				break;
			}
		}
		IomObject sequence=obj->getAttrObj(tags::get_sequence(),sequencei);
		for(int segmenti=0;segmenti<sequence->getAttrValueCount(tags::get_segment());segmenti++){
			IomObject segment=sequence->getAttrObj(tags::get_segment(),segmenti);
			if(segment->getTag()==tags::get_COORD()){
				// COORD
				writeCoord(out,segment);
			}else if(segment->getTag()==tags::get_ARC()){
				// ARC
				writeArc(out,segment);
			}else{
				// custum line form
				out.startElement(segment->getTag(),0,0);
				writeAttrs(out,segment);
				out.endElement(/*segment*/);
			}

		}
		if(clipped){
			out.endElement(/*CLIPPED*/);
		}
	}
	out.endElement(/*POLYLINE*/);
}

/** writes a surface value.
 */
void iom_file::writeSurface(XmlWriter &out, IomObject &obj)
{
/*
     object: MULTISURFACE [INCOMPLETE]
       surface // if incomplete; multi surface values
         object: SURFACE
           boundary
             object: BOUNDARY
               polyline
                 object: POLYLINE

	
	<SURFACE>
		<BOUNDARY>
			<POLYLINE .../>
			<POLYLINE .../>
		</BOUNDARY>
		<BOUNDARY>
			<POLYLINE .../>
			<POLYLINE .../>
		</BOUNDARY>
	</SURFACE>
*/
	out.startElement(tags::get_SURFACE(),0,0);
	bool clipped=obj->getConsistency()==IOM_INCOMPLETE;
	for(int surfacei=0;surfacei<obj->getAttrValueCount(tags::get_surface());surfacei++){
		if(clipped){
			out.startElement(tags::get_CLIPPED(),0,0);
		}else{
			// an unclipped surface should have only one surface element
			if(surfacei>0){
				iom_issueerr("unclipped surface with multi 'surface' elements");
				break;
			}
		}
		IomObject surface=obj->getAttrObj(tags::get_surface(),surfacei);
		for(int boundaryi=0;boundaryi<surface->getAttrValueCount(tags::get_boundary());boundaryi++){
			IomObject boundary=surface->getAttrObj(tags::get_boundary(),boundaryi);
			out.startElement(tags::get_BOUNDARY(),0,0);
			for(int polylinei=0;polylinei<boundary->getAttrValueCount(tags::get_polyline());polylinei++){
				IomObject polyline=boundary->getAttrObj(tags::get_polyline(),polylinei);
				writePolyline(out,polyline,true);
			}
			out.endElement(/*BOUNDARY*/);
		}
		if(clipped){
			out.endElement(/*CLIPPED*/);
		}
	}
	out.endElement(/*SURFACE*/);
}

void iom_file::writeAttr(XmlWriter &out, IomObject &obj,int attr)
{
	int valueCount=obj->getAttrValueCount(attr);
	if(valueCount>0){
		const XMLCh *val=obj->getAttrPrim(attr,0);
		// not a primitive?
		if(!val){
			IomObject child=obj->getAttrObj(attr,0);
			// some special cases
			if(child->getTag()==tags::get_COORD()){
				// COORD
				out.startElement(attr,0,0);
				writeCoord(out,child);
				out.endElement(/*attr*/);
				if(valueCount>1){
					iom_issueerr("max one COORD value allowed");
				}
			}else if(child->getTag()==tags::get_POLYLINE()){
				// POLYLINE
				out.startElement(attr,0,0);
				writePolyline(out,child,false);
				out.endElement(/*attr*/);
				if(valueCount>1){
					iom_issueerr("max one POLYLINE value allowed");
				}
			}else if(child->getTag()==tags::get_MULTISURFACE()){
				// MULTISURFACE
				out.startElement(attr,0,0);
				writeSurface(out,child);
				out.endElement(/*attr*/);
				if(valueCount>1){
					iom_issueerr("max one MULTISURFACE value allowed");
				}
			}else{
				// normal case
				const XMLCh *ref=child->getRefOid();
				bool isRef= ref ? true : false;
				// Reference-attribute or Role or EmbeddedLink?
				if(isRef){
					const XMLCh *extref=0;
					const XMLCh *bid=0;
					XMLCh itoabuf[40];
					const XMLCh *orderpos=0;
					if(ref){
						if(child->getRefOrderPos()>0){
							XMLString::binToText( child->getRefOrderPos(),itoabuf,sizeof(itoabuf)-1,10);
							orderpos=itoabuf;
						}
					}
					bid=child->getRefBid();
					if(bid){
						extref=ref;
						ref=0;
					}
					XmlWrtAttr refAttr[]={
						 XmlWrtAttr(ref      ? ustrings::get_REF()     :0, ref,true)
						,XmlWrtAttr(extref   ? ustrings::get_EXTREF()  :0, extref,true)
						,XmlWrtAttr(bid      ? ustrings::get_BID()     :0, bid,true)
						,XmlWrtAttr(orderpos ? ustrings::get_ORDER_POS():0, orderpos)
					};
					out.startElement(attr,refAttr,sizeof(refAttr)/sizeof(refAttr[0]));
					if(child->getAttrCount()>0){
						out.startElement(child->getTag(),0,0);
						writeAttrs(out,child);
						out.endElement(/*child*/);
					}
					out.endElement(/*attr*/);
					if(valueCount>1){
						iom_issueerr("max one reference value allowed");
					}
				}else{
					// struct
					out.startElement(attr,0,0);
					int valuei=0;
					while(1){
						out.startElement(child->getTag(),0,0);
						writeAttrs(out,child);
						out.endElement(/*child*/);
						valuei++;
						if(valuei>=valueCount){
							break;
						}
						child=obj->getAttrObj(attr,valuei);
					}
					out.endElement(/*attr*/);
				}
			}

		}else{
			out.startElement(attr,0,0);
			out.characters(val);
			out.endElement(/*attr*/);
			if(valueCount>1){
				iom_issueerr("max one primitive-type value allowed");
			}
		}
	}
}

void iom_file::writeAttrs(XmlWriter &out, IomObject &obj)
{
	tagv_type::iterator tag=tagList.find(obj->getTag());
	// class not found?
	if(tag==tagList.end()){
		std::string msg="unknown type <";
		msg+=+obj->getTag_c();
		msg+=">";
		iom_issueerr(msg.c_str());
		// write all attributes
		for(int attri=0;attri<obj->getAttrCount();attri++){
			int attr=obj->getAttrName(attri);
			writeAttr(out, obj,attr);
		}
	}else{
		// class found
		attrv_type attrv=tag->second;
		for(attrv_type::size_type attri=0;attri<attrv.size();attri++){
			int attr=attrv[attri].second;
			writeAttr(out, obj,attr);
		}
	}
}

/** write all baskets to an xml file.
 */
int iom_file::save()
{
	// build class/attribute list
	if(ilibasket.isNull()){
		iom_issueerr("model required to save data");
		return IOM_ERR_ILLEGALSTATE;
	}
	buildTagList();

	// read rest of file (before we overwrite it!)
	IomIterator bi=new iom_iterator(this);
	while(!bi->next_basket().isNull()){
		; // empty
	}

	// open file for write
	XmlWriter out;
	int ind=0;
	out.open(filename);out.printNewLine();

	// write header
	XmlWrtAttr trsfAttr[]={XmlWrtAttr(ustrings::get_xmlns(),ustrings::get_NS_INTERLIS22())};
	out.printIndent(ind);
	out.startElement(tags::get_TRANSFER(),trsfAttr,sizeof(trsfAttr)/sizeof(trsfAttr[0]));out.printNewLine();
	{
		ind++;
		out.printIndent(ind);
		XStr version("2.2");
		XmlWrtAttr headAttr[]={
			XmlWrtAttr(ustrings::get_VERSION(),version.unicodeForm())
			,XmlWrtAttr(ustrings::get_SENDER(),getHeadSecSender())
		};
		out.startElement(tags::get_HEADERSECTION(),headAttr,sizeof(headAttr)/sizeof(headAttr[0]));out.printNewLine();
		{
			ind++;
			out.printIndent(ind);
			out.startElement(tags::get_ALIAS(),0,0);out.printNewLine();
			{
				ind++;
				ind--;
			}
			out.printIndent(ind);
			out.endElement(/*ALIAS*/);out.printNewLine();
			out.printIndent(ind);
			out.startElement(tags::get_COMMENT(),0,0);
			out.characters(getHeadSecComment());
			out.endElement(/*COMMENT*/);out.printNewLine();
			ind--;
		}
		out.printIndent(ind);
		out.endElement(/*HEADERSECTION*/);out.printNewLine();
		ind--;
	}

	// write DATASECTION
	{
		ind++;
		out.printIndent(ind);
		out.startElement(tags::get_DATASECTION(),0,0);out.printNewLine();
		{
			ind++;

			// write all baskets
			for(std::vector<IomBasket>::size_type basketi=0;basketi<basketv.size();basketi++){
				IomBasket basket=basketv.at(basketi);
				const XMLCh *topics=basket->getTopics();
				const XMLCh *kind=encodeBasketKind(basket->getKind());
				const XMLCh *startstate=basket->getKind()!=IOM_FULL ? basket->getStartState() : 0;
				const XMLCh *endstate=basket->getKind()!=IOM_FULL ? basket->getEndState() : 0;
				const XMLCh *consistency=encodeConsistency(basket->getConsistency());
				XmlWrtAttr basketAttr[]={
					XmlWrtAttr(ustrings::get_BID(),basket->getOid(),true)
					,XmlWrtAttr(topics ? ustrings::get_TOPICS():0,topics)
					,XmlWrtAttr(kind ? ustrings::get_KIND():0 ,kind)
					,XmlWrtAttr(startstate ? ustrings::get_STARTSTATE():0,startstate)
					,XmlWrtAttr(endstate ? ustrings::get_ENDSTATE():0,endstate)
					,XmlWrtAttr(consistency ? ustrings::get_CONSISTENCY():0,consistency)
				};
				out.printIndent(ind);
				if(basket->getTag()==0){
					iom_issueerr("basket requires a TOPIC name");
					return IOM_ERR_ILLEGALSTATE;
				}
				out.startElement(basket->getTag(),basketAttr,sizeof(basketAttr)/sizeof(basketAttr[0]));out.printNewLine();
				{
					ind++;
					// write all objects
					IomIterator obji=new iom_iterator(basket);
					IomObject obj=obji->next_object();
					while(!obj.isNull()){
						out.printIndent(ind);
						const XMLCh *bid=obj->getBid();
						const XMLCh *ops=encodeOperation(obj->getOperation());
						const XMLCh *consistency=encodeConsistency(basket->getConsistency());
						XmlWrtAttr objAttr[]={
							XmlWrtAttr(ustrings::get_TID(),obj->getOid(),true)
							,XmlWrtAttr(bid ? ustrings::get_BID():0,bid,true)
							,XmlWrtAttr(ops ? ustrings::get_OPERATION():0 ,ops)
							,XmlWrtAttr(consistency ? ustrings::get_CONSISTENCY():0,consistency)
						};
						out.startElement(obj->getTag(),objAttr,sizeof(objAttr)/sizeof(objAttr[0]));
						writeAttrs(out,obj);
						out.endElement(/*object*/);out.printNewLine();
						obj=obji->next_object();
					}
					ind--;
				}
				out.printIndent(ind);
				out.endElement(/*basket*/);out.printNewLine();
			}
			ind--;
		}
		out.printIndent(ind);
		out.endElement(/*DATASECTION*/);out.printNewLine();
		ind--;
	}
	out.printIndent(ind);
	out.endElement(/*TRANSFER*/);out.printNewLine();
	// close file
	out.close();
	return 0;
}

int iom_file::getQualifiedTypeName(IomObject &aclass)
{
	static const XMLCh  period[] =
	{
		chPeriod,  chNull
	};
	IomObject topic=ilibasket->getObject(aclass->getAttrObj(tags::get_container(),0)->getRefOid());
	IomObject model=ilibasket->getObject(topic->getAttrObj(tags::get_container(),0)->getRefOid());
	// class at model level?
	XMLCh *qname;
	if(model->getTag()==tags::get_iom04_metamodel_TransferDescription()){
		const XMLCh *modelName=topic->getAttrValue(tags::get_name());
		const XMLCh *className=aclass->getAttrValue(tags::get_name());
		int qnLen=XMLString::stringLen(modelName)+1+XMLString::stringLen(className)+1;
		qname=dbgnew XMLCh[qnLen];
		XMLString::copyString(qname,modelName);
		XMLString::catString(qname,period);
		XMLString::catString(qname,className);
	}else{
		const XMLCh *modelName=model->getAttrValue(tags::get_name());
		const XMLCh *topicName=topic->getAttrValue(tags::get_name());
		const XMLCh *className=aclass->getAttrValue(tags::get_name());
		int qnLen=XMLString::stringLen(modelName)+1+XMLString::stringLen(topicName)+1+XMLString::stringLen(className)+1;
		qname=dbgnew XMLCh[qnLen];
		XMLString::copyString(qname,modelName);
		XMLString::catString(qname,period);
		XMLString::catString(qname,topicName);
		XMLString::catString(qname,period);
		XMLString::catString(qname,className);
	}
	int classId=ParserHandler::getTagId(qname);
	delete[] qname;
	return classId;
}

void iom_file::buildTagList()
{
	// for all links class--(attribute|role)
	IomIterator obji=new iom_iterator(ilibasket);
	IomObject obj;
	while(!(obj=obji->next_object()).isNull()){
		if(obj->getTag()==tags::get_iom04_metamodel_Table() || obj->getTag()==tags::get_iom04_metamodel_AssociationDef()){
			// get qualified name of class
			int classId=getQualifiedTypeName(obj);
			// add to tag list
			tagv_type::iterator tag=tagList.find(classId);
			if(tag==tagList.end()){
				// not found, add empty attr list
				attrv_type attrv;
				tagList[classId]=attrv;
			}else{
				// found, don't change
			}
		}else if(obj->getTag()==tags::get_iom04_metamodel_ViewableAttributesAndRoles()){
			// get class
			IomObject aclass=ilibasket->getObject(obj->getAttrObj(tags::get_viewable(),0)->getRefOid());
			// get qualified name of class
			int classId=getQualifiedTypeName(aclass);
			// get attribute or role
			IomObject leafref=obj->getAttrObj(tags::get_attributesAndRoles(),0);
			// get name
			IomObject leafele=ilibasket->getObject(leafref->getRefOid());
			const XMLCh *leafName=leafele->getAttrValue(tags::get_name());
			int attrId=ParserHandler::getTagId(leafName);
			// get element index
			int eleIdx=leafref->getRefOrderPos()-1;
			// add to tag list
			tagv_type::iterator tag=tagList.find(classId);
			if(tag==tagList.end()){
				// not found, add
				attrv_type attrv;
				std::pair<int,int> ele(eleIdx,attrId);
				attrv.push_back(ele);
				tagList[classId]=attrv;
			}else{
				// found, replace
				attrv_type attrv=tag->second;
				std::pair<int,int> ele(eleIdx,attrId);
				attrv.push_back(ele);
				tagList[classId]=attrv;
			}
		}
	}

	// in all classes sort attrs according to pos
	tagv_type::iterator tag=tagList.begin();
	for(;tag!=tagList.end();tag++){
		attrv_type attrv=tag->second;
		std::sort(attrv.begin(),attrv.end());
		tagList[tag->first]=attrv;		
	}
	
}

/** @}
 */


//UTF-8
static const XMLCh  gUTF8[] =
{
    chLatin_U, chLatin_T, chLatin_F, chDash, chDigit_8, chNull
};

//</
static const XMLCh  gEndElement[] =
{
    chOpenAngle, chForwardSlash, chNull
};

//<?xml version="
static const XMLCh  gXMLDecl_VersionInfo[] =
{
    chOpenAngle, chQuestion, chLatin_x,     chLatin_m,  chLatin_l,  chSpace,
    chLatin_v,   chLatin_e,  chLatin_r,     chLatin_s,  chLatin_i,  chLatin_o,
    chLatin_n,   chEqual,    chDoubleQuote, chNull
};

static const XMLCh gXMLDecl_ver10[] =
{
    chDigit_1, chPeriod, chDigit_0, chNull
};

//encoding="
static const XMLCh  gXMLDecl_EncodingDecl[] =
{
    chLatin_e,  chLatin_n,  chLatin_c,  chLatin_o,      chLatin_d, chLatin_i,
    chLatin_n,  chLatin_g,  chEqual,    chDoubleQuote,  chNull
};

//"
static const XMLCh  gXMLDecl_separator[] =
{
    chDoubleQuote, chSpace, chNull
};

//?>
static const XMLCh  gXMLDecl_endtag[] =
{
    chQuestion, chCloseAngle,  chNull
};


const XMLCh *XmlWrtAttr::getName()
{
	return name;
}

const XMLCh *XmlWrtAttr::getValue()
{
	return value;
}
bool XmlWrtAttr::isOid()
{
	return oidAttr;
}
XmlWrtAttr::XmlWrtAttr(const XMLCh *name1,const XMLCh *value1)
	: name(name1)
	, value(value1)
	,oidAttr(false)
{
}
XmlWrtAttr::XmlWrtAttr(const XMLCh *name1,const XMLCh *value1,bool isOid)
	: name(name1)
	, value(value1)
	,oidAttr(isOid)
{
}

void XmlWriter::open(const char *filename)
{
	/*
	out=fopen("filename","w");
	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>",out);
	newline();

    XMLTransService::Codes resCode;
    transcoder = XMLPlatformUtils::fgTransService->makeNewTranscoderFor
    (
        "UTF-8"
        , resCode
        , 16 * 1024
    );
	*/
	destination=new LocalFileFormatTarget(filename); 
	out= new XMLFormatter(gUTF8
                 ,gXMLDecl_ver10
                 ,destination
                 ,XMLFormatter::NoEscapes
                 ,XMLFormatter::UnRep_CharRef);

    out->setUnRepFlags(XMLFormatter::UnRep_CharRef);

    *out << gXMLDecl_VersionInfo << gXMLDecl_ver10 << gXMLDecl_separator;
    *out << gXMLDecl_EncodingDecl << gUTF8 << gXMLDecl_separator;
    *out << gXMLDecl_endtag;
}

void XmlWriter::startElement(int tagid,XmlWrtAttr attrv[],int attrc)
{
	const XMLCh *tagName=ParserHandler::getTagName(tagid);

    *out  << XMLFormatter::NoEscapes
                 << chOpenAngle << tagName;


	for(int i=0;i<attrc;i++){
		if(attrv[i].getName()){
			*out  << XMLFormatter::NoEscapes
					 << chSpace << attrv[i].getName()
					 << chEqual << chDoubleQuote
					 << XMLFormatter::AttrEscapes;
			if(attrv[i].isOid()){
				*out << chLatin_x; 
			}
			*out << attrv[i].getValue()
					 << XMLFormatter::NoEscapes
					 << chDoubleQuote;
		}
	}

    *out << XMLFormatter::NoEscapes << chCloseAngle;
	stack.push(tagid);
}

void XmlWriter::endElement()
{
	const XMLCh *tagName=ParserHandler::getTagName(stack.top());
	stack.pop();
    *out  << XMLFormatter::NoEscapes
                 << chOpenAngle 
				 << chForwardSlash << tagName << chCloseAngle;
}

void XmlWriter::characters(const XMLCh *const chars)
{
    //fFormatter->setUnRepFlags(XMLFormatter::UnRep_CharRef);
	out->formatBuf(chars, XMLString::stringLen(chars), XMLFormatter::CharEscapes);
}
XmlWriter::XmlWriter()
: out(0)
, destination(0)
{
}
XmlWriter::~XmlWriter()
{
	close();
}
void XmlWriter::close()
{
	//fclose(out);out=0;
	if(destination){
		delete destination;destination=0;
	}
	if(out){
		delete out;out=0;
	}
}

void XmlWriter::printNewLine()
{
	static const XMLCh  gEOLSeq[] =
	{
		chLF, chNull
	};
   *out << gEOLSeq;
}

void XmlWriter::printIndent(int level)
{
	for(int i = 0; i < level; i++)
		*out << chSpace << chSpace;
}
