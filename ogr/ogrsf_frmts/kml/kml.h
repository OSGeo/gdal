/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Class for reading, parsing and handling a kmlfile.
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
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
#ifndef OGR_KML_KML_H_INCLUDED
#define OGR_KML_KML_H_INCLUDED

#include "kmlutility.h"

#include <expat.h>
// std
#include <iostream>
#include <string>
#include <vector>

#ifndef XMLCALL
#error "Old version of expat, please upgrade to expat 2.x"
#endif

class KMLNode;

class KML
{
public:
	KML();
	virtual ~KML();
	bool open(const char * pszFilename);
	bool isValid();
	bool isHandled(std::string const&) const;
	virtual bool isLeaf(std::string const&) const {return false;};
	virtual bool isFeature(std::string const&) const {return false;};
	virtual bool isFeatureContainer(std::string const&) const {return false;};
	virtual bool isContainer(std::string const&) const {return false;};
	virtual bool isRest(std::string const&) const {return false;};
    virtual void findLayers(KMLNode* poNode) {};

	void parse();
	void print(unsigned short what = 3);
    std::string getError();
	void classifyNodes();
	void eliminateEmpty();
	short numLayers();
    bool selectLayer(unsigned short);
    std::string getCurrentName();
    Nodetype getCurrentType();
    short getNumFeatures();
    Feature* getFeature(unsigned short);
    bool getExtents(double *pdfXMin, double *pdfXMax, double *pdfYMin, double *pdfYMax);

protected:
	void checkValidity();

	static void XMLCALL startElement(void *, const char *, const char **);
	static void XMLCALL startElementValidate(void *, const char *, const char **);
	static void XMLCALL dataHandler(void *, const char *, int);
	static void XMLCALL endElement(void *, const char *);

	// trunk of KMLnodes
	KMLNode* poTrunk_;
	// number of layers;
	short nNumLayers_;

private:
	// depth of the DOM
	unsigned int nDepth_;
	// KML version number
	std::string sVersion_;
	// set when the file was validated and is kml
	bool bValid_;
	// file descriptor
	FILE *pKMLFile_;
	// error text ("" when everything is OK")
	std::string sError_;
	// current KMLNode
	KMLNode *poCurrent_;
};

#endif /* OGR_KML_KML_H_INCLUDED */

