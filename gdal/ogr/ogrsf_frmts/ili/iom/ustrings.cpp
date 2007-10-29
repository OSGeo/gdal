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
