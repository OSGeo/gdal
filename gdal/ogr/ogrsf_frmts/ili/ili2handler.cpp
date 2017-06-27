/******************************************************************************
 *
 * Project:  Interlis 2 Reader
 * Purpose:  Implementation of ILI2Handler class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2008, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"

#include "ili2readerp.h"
#include "ogr_ili2.h"

#include <xercesc/sax2/Attributes.hpp>

CPL_CVSID("$Id$")

//
// constants
//
static const char* const ILI2_DATASECTION = "DATASECTION";

//
// ILI2Handler
//
ILI2Handler::ILI2Handler( ILI2Reader *poReader ) :
    m_poReader(poReader),
    level(0),
    dom_doc(NULL),
    dom_elem(NULL),
    m_nEntityCounter(0)
{
  XMLCh *tmpCh = XMLString::transcode("CORE");
  DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation(tmpCh);
  XMLString::release(&tmpCh);

  // the root element
  tmpCh = XMLString::transcode("ROOT");
  dom_doc = impl->createDocument(NULL,tmpCh,NULL);
  XMLString::release(&tmpCh);

  // the first element is root
  dom_elem = dom_doc->getDocumentElement();
}

ILI2Handler::~ILI2Handler() {
  // remove all elements
  DOMNode *tmpNode = dom_doc->getFirstChild();
  while (tmpNode != NULL) {
    /*tmpNode = */dom_doc->removeChild(tmpNode);
    tmpNode = dom_doc->getFirstChild();
  }

  // release the dom tree
  dom_doc->release();
}

void ILI2Handler::startDocument() {
  // the level counter starts with DATASECTION
  level = -1;
  m_nEntityCounter = 0;
}

void ILI2Handler::endDocument() {
  // nothing to do
}

void ILI2Handler::startElement(
    const XMLCh* const /* uri */,
    const XMLCh* const /* localname */,
    const XMLCh* const qname,
    const Attributes& attrs
    ) {
  // start to add the layers, features with the DATASECTION
  char *tmpC = NULL;
  m_nEntityCounter = 0;
  if ((level >= 0) || (cmpStr(ILI2_DATASECTION,
                              tmpC = XMLString::transcode(qname)) == 0)) {
    level++;

    if (level >= 2) {

      // create the dom tree
      DOMElement *elem = reinterpret_cast<DOMElement *>(
          dom_doc->createElement(qname) );

      // add all attributes
      unsigned int len = static_cast<unsigned int>(attrs.getLength());
      for (unsigned int index = 0; index < len; index++)
        elem->setAttribute(attrs.getQName(index), attrs.getValue(index));
      dom_elem->appendChild(elem);
      dom_elem = elem;
    }
  }
  XMLString::release(&tmpC);
}

void ILI2Handler::endElement(
    CPL_UNUSED const XMLCh* const uri,
    CPL_UNUSED const XMLCh* const localname,
    CPL_UNUSED const XMLCh* const qname
    ) {
  m_nEntityCounter = 0;
  if (level >= 0) {
    if (level == 2) {

      // go to the parent element and parse the child element
      DOMElement* childElem = dom_elem;
      dom_elem = (DOMElement*)dom_elem->getParentNode();

      m_poReader->AddFeature(childElem);

      // remove the child element
      /*childElem = (DOMElement*)*/dom_elem->removeChild(childElem);
    } else if (level >= 3) {

      // go to the parent element
      dom_elem = reinterpret_cast<DOMElement *>( dom_elem->getParentNode() );
    }
    level--;
  }
}

/************************************************************************/
/*                     characters() (xerces 3 version)                  */
/************************************************************************/

void ILI2Handler::characters( const XMLCh *const chars,
                              CPL_UNUSED const XMLSize_t length ) {
  // add the text element
  if (level >= 3) {
    char *tmpC = XMLString::transcode(chars);

    // only add the text if it is not empty
    if (trim(tmpC) != "")
      dom_elem->appendChild(dom_doc->createTextNode(chars));

    XMLString::release(&tmpC);
  }
}

void ILI2Handler::startEntity (CPL_UNUSED const XMLCh *const name)
{
    m_nEntityCounter++;
    if (m_nEntityCounter > 1000)
    {
        throw SAXNotSupportedException (
            "File probably corrupted (million laugh pattern)" );
    }
}

void ILI2Handler::fatalError(const SAXParseException&) {
  // FIXME Error handling
}
