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


#include <iom/iom_p.h>

	int tags::COORD=0;
	int tags::ARC=0;
	int tags::C1=0;
	int tags::C2=0;
	int tags::C3=0;
	int tags::A1=0;
	int tags::A2=0;
	int tags::iom04_metamodel_AssociationDef=0;
	int tags::R=0;
	int tags::lineattr=0;
	int tags::TRANSFER=0;
	int tags::iom04_metamodel_Table=0;
	int tags::DATASECTION=0;
	int tags::HEADERSECTION=0;
	int tags::ALIAS=0;
	int tags::COMMENT=0;
	int tags::CLIPPED=0;
	int tags::LINEATTR=0;
	int tags::SEGMENTS=0;
	int tags::segment=0;
	int tags::SURFACE=0;
	int tags::surface=0;
	int tags::boundary=0;
	int tags::BOUNDARY=0;
	int tags::polyline=0;
	int tags::POLYLINE=0;
	int tags::sequence=0;
	int tags::MULTISURFACE=0;
	int tags::iom04_metamodel_ViewableAttributesAndRoles=0;
	int tags::viewable=0;
	int tags::attributesAndRoles=0;
	int tags::container=0;
	int tags::iom04_metamodel_TransferDescription=0;
	int tags::name=0;

void tags::clear()
{
	COORD=0;
	ARC=0;
	C1=0;
	C2=0;
	C3=0;
	A1=0;
	A2=0;
	iom04_metamodel_AssociationDef=0;
	R=0;
	lineattr=0;
	TRANSFER=0;
	iom04_metamodel_Table=0;
	DATASECTION=0;
	HEADERSECTION=0;
	ALIAS=0;
	COMMENT=0;
	CLIPPED=0;
	LINEATTR=0;
	SEGMENTS=0;
	segment=0;
	SURFACE=0;
	surface=0;
	boundary=0;
	BOUNDARY=0;
	polyline=0;
	POLYLINE=0;
	sequence=0;
	MULTISURFACE=0;
	iom04_metamodel_ViewableAttributesAndRoles=0;
	viewable=0;
	attributesAndRoles=0;
	container=0;
	iom04_metamodel_TransferDescription=0;
	name=0;
}

int tags::get_ARC()
{
	if(!ARC){
		ARC=ParserHandler::getTagId("ARC");
	}
	return ARC;
}

int tags::get_COORD()
{
	if(!COORD){
		COORD=ParserHandler::getTagId("COORD");
	}
	return COORD;
}

int tags::get_C1()
{
	if(!C1){
		C1=ParserHandler::getTagId("C1");
	}
	return C1;
}

int tags::get_C2()
{
	if(!C2){
		C2=ParserHandler::getTagId("C2");
	}
	return C2;
}

int tags::get_C3()
{
	if(!C3){
		C3=ParserHandler::getTagId("C3");
	}
	return C3;
}

int tags::get_A1()
{
	if(!A1){
		A1=ParserHandler::getTagId("A1");
	}
	return A1;
}

int tags::get_A2()
{
	if(!A2){
		A2=ParserHandler::getTagId("A2");
	}
	return A2;
}

int tags::get_iom04_metamodel_AssociationDef()
{
	if(!iom04_metamodel_AssociationDef){
		iom04_metamodel_AssociationDef=ParserHandler::getTagId("iom04.metamodel.AssociationDef");
	}
	return iom04_metamodel_AssociationDef;
}

int tags::get_R()
{
	if(!R){
		R=ParserHandler::getTagId("R");
	}
	return R;
}

int tags::get_lineattr()
{
	if(!lineattr){
		lineattr=ParserHandler::getTagId("lineattr");
	}
	return lineattr;
}

int tags::get_TRANSFER()
{
	if(!TRANSFER){
		TRANSFER=ParserHandler::getTagId("TRANSFER");
	}
	return TRANSFER;
}

int tags::get_iom04_metamodel_Table()
{
	if(!iom04_metamodel_Table){
		iom04_metamodel_Table=ParserHandler::getTagId("iom04.metamodel.Table");
	}
	return iom04_metamodel_Table;
}

int tags::get_DATASECTION()
{
	if(!DATASECTION){
		DATASECTION=ParserHandler::getTagId("DATASECTION");
	}
	return DATASECTION;
}

int tags::get_HEADERSECTION()
{
	if(!HEADERSECTION){
		HEADERSECTION=ParserHandler::getTagId("HEADERSECTION");
	}
	return HEADERSECTION;
}

int tags::get_ALIAS()
{
	if(!ALIAS){
		ALIAS=ParserHandler::getTagId("ALIAS");
	}
	return ALIAS;
}

int tags::get_COMMENT()
{
	if(!COMMENT){
		COMMENT=ParserHandler::getTagId("COMMENT");
	}
	return COMMENT;
}

int tags::get_CLIPPED()
{
	if(!CLIPPED){
		CLIPPED=ParserHandler::getTagId("CLIPPED");
	}
	return CLIPPED;
}

int tags::get_LINEATTR()
{
	if(!LINEATTR){
		LINEATTR=ParserHandler::getTagId("LINEATTR");
	}
	return LINEATTR;
}

int tags::get_SEGMENTS()
{
	if(!SEGMENTS){
		SEGMENTS=ParserHandler::getTagId("SEGMENTS");
	}
	return SEGMENTS;
}

int tags::get_segment()
{
	if(!segment){
		segment=ParserHandler::getTagId("segment");
	}
	return segment;
}


int tags::get_SURFACE()
{
	if(!SURFACE){
		SURFACE=ParserHandler::getTagId("SURFACE");
	}
	return SURFACE;
}

int tags::get_surface()
{
	if(!surface){
		surface=ParserHandler::getTagId("surface");
	}
	return surface;
}

int tags::get_boundary()
{
	if(!boundary){
		boundary=ParserHandler::getTagId("boundary");
	}
	return boundary;
}

int tags::get_BOUNDARY()
{
	if(!BOUNDARY){
		BOUNDARY=ParserHandler::getTagId("BOUNDARY");
	}
	return BOUNDARY;
}

int tags::get_polyline()
{
	if(!polyline){
		polyline=ParserHandler::getTagId("polyline");
	}
	return polyline;
}

int tags::get_POLYLINE()
{
	if(!POLYLINE){
		POLYLINE=ParserHandler::getTagId("POLYLINE");
	}
	return POLYLINE;
}

int tags::get_sequence()
{
	if(!sequence){
		sequence=ParserHandler::getTagId("sequence");
	}
	return sequence;
}

int tags::get_MULTISURFACE()
{
	if(!MULTISURFACE){
		MULTISURFACE=ParserHandler::getTagId("MULTISURFACE");
	}
	return MULTISURFACE;
}

int tags::get_iom04_metamodel_ViewableAttributesAndRoles()
{
	if(!iom04_metamodel_ViewableAttributesAndRoles){
		iom04_metamodel_ViewableAttributesAndRoles=ParserHandler::getTagId("iom04.metamodel.ViewableAttributesAndRoles");
	}
	return iom04_metamodel_ViewableAttributesAndRoles;
}

int tags::get_viewable()
{
	if(!viewable){
		viewable=ParserHandler::getTagId("viewable");
	}
	return viewable;
}
int tags::get_attributesAndRoles()
{
	if(!attributesAndRoles){
		attributesAndRoles=ParserHandler::getTagId("attributesAndRoles");
	}
	return attributesAndRoles;
}
int tags::get_container()
{
	if(!container){
		container=ParserHandler::getTagId("container");
	}
	return container;
}
int tags::get_iom04_metamodel_TransferDescription()
{
	if(!iom04_metamodel_TransferDescription){
		iom04_metamodel_TransferDescription=ParserHandler::getTagId("iom04.metamodel.TransferDescription");
	}
	return iom04_metamodel_TransferDescription;
}
int tags::get_name()
{
	if(!name){
		name=ParserHandler::getTagId("name");
	}
	return name;
}
