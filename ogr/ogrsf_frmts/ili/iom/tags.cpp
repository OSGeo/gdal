#include <iom/iom_p.h>

int tags::get_ARC()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("ARC");
	}
	return ret;
}

int tags::get_COORD()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("COORD");
	}
	return ret;
}

int tags::get_C1()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("C1");
	}
	return ret;
}

int tags::get_C2()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("C2");
	}
	return ret;
}

int tags::get_C3()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("C3");
	}
	return ret;
}

int tags::get_A1()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("A1");
	}
	return ret;
}

int tags::get_A2()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("A2");
	}
	return ret;
}

int tags::get_R()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("R");
	}
	return ret;
}

int tags::get_lineattr()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("lineattr");
	}
	return ret;
}

int tags::get_TRANSFER()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("TRANSFER");
	}
	return ret;
}

int tags::get_DATASECTION()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("DATASECTION");
	}
	return ret;
}

int tags::get_HEADERSECTION()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("HEADERSECTION");
	}
	return ret;
}

int tags::get_ALIAS()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("ALIAS");
	}
	return ret;
}

int tags::get_COMMENT()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("COMMENT");
	}
	return ret;
}

int tags::get_CLIPPED()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("CLIPPED");
	}
	return ret;
}

int tags::get_LINEATTR()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("LINEATTR");
	}
	return ret;
}

int tags::get_SEGMENTS()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("SEGMENTS");
	}
	return ret;
}

int tags::get_segment()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("segment");
	}
	return ret;
}


int tags::get_SURFACE()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("SURFACE");
	}
	return ret;
}

int tags::get_surface()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("surface");
	}
	return ret;
}

int tags::get_boundary()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("boundary");
	}
	return ret;
}

int tags::get_BOUNDARY()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("BOUNDARY");
	}
	return ret;
}

int tags::get_polyline()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("polyline");
	}
	return ret;
}

int tags::get_POLYLINE()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("POLYLINE");
	}
	return ret;
}

int tags::get_sequence()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("sequence");
	}
	return ret;
}

int tags::get_MULTISURFACE()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("MULTISURFACE");
	}
	return ret;
}

int tags::get_iom04_metamodel_ViewableAttributesAndRoles()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("iom04.metamodel.ViewableAttributesAndRoles");
	}
	return ret;
}

int tags::get_viewable()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("viewable");
	}
	return ret;
}
int tags::get_attributesAndRoles()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("attributesAndRoles");
	}
	return ret;
}
int tags::get_container()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("container");
	}
	return ret;
}
int tags::get_iom04_metamodel_TransferDescription()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("iom04.metamodel.TransferDescription");
	}
	return ret;
}
int tags::get_name()
{
	static int ret=0;
	if(!ret){
		ret=ParserHandler::getTagId("name");
	}
	return ret;
}
