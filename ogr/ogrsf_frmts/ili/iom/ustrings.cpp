#include <iom/iom_p.h>


const XMLCh* ustrings::get_xmlns()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("xmlns");
	}
	return ret;
}

const XMLCh* ustrings::get_NS_INTERLIS22()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("http://www.interlis.ch/INTERLIS2.2");
	}
	return ret;
}

const XMLCh* ustrings::get_BID()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("BID");
	}
	return ret;
}

const XMLCh* ustrings::get_TOPICS()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("TOPICS");
	}
	return ret;
}

const XMLCh* ustrings::get_KIND()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("KIND");
	}
	return ret;
}

const XMLCh* ustrings::get_STARTSTATE()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("STARTSTATE");
	}
	return ret;
}

const XMLCh* ustrings::get_ENDSTATE()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("ENDSTATE");
	}
	return ret;
}

const XMLCh* ustrings::get_TID()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("TID");
	}
	return ret;
}

const XMLCh* ustrings::get_OPERATION()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("OPERATION");
	}
	return ret;
}

const XMLCh* ustrings::get_INSERT()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("INSERT");
	}
	return ret;
}

const XMLCh* ustrings::get_UPDATE()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("UPDATE");
	}
	return ret;
}

const XMLCh* ustrings::get_DELETE()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("DELETE");
	}
	return ret;
}

const XMLCh* ustrings::get_REF()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("REF");
	}
	return ret;
}

const XMLCh* ustrings::get_EXTREF()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("EXTREF");
	}
	return ret;
}

const XMLCh* ustrings::get_ORDER_POS()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("ORDER_POS");
	}
	return ret;
}

const XMLCh* ustrings::get_CONSISTENCY()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("CONSISTENCY");
	}
	return ret;
}

const XMLCh* ustrings::get_COMPLETE()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("COMPLETE");
	}
	return ret;
}

const XMLCh* ustrings::get_INCOMPLETE()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("INCOMPLETE");
	}
	return ret;
}

const XMLCh* ustrings::get_INCONSISTENT()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("INCONSISTENT");
	}
	return ret;
}

const XMLCh* ustrings::get_ADAPTED()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("ADAPTED");
	}
	return ret;
}

const XMLCh* ustrings::get_SENDER()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("SENDER");
	}
	return ret;
}

const XMLCh* ustrings::get_VERSION()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("VERSION");
	}
	return ret;
}

const XMLCh* ustrings::get_INITIAL()
{
	static XMLCh* ret=0;
	if(!ret){
		ret=XMLString::transcode("INITIAL");
	}
	return ret;
}
