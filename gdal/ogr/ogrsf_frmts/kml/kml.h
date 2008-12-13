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

#include <expat.h>
// std
#include <iostream>
#include <string>
#include <vector>

#include "cpl_port.h"
#include "kmlutility.h"

/* Compatibility stuff for expat >=1.95.0 and < 1.95.7 */
#ifndef XMLCALL
#define XMLCALL
#endif
#ifndef XML_STATUS_OK
#define XML_STATUS_OK    1
#define XML_STATUS_ERROR 0
#endif

class KMLNode;


typedef enum
{
    KML_VALIDITY_UNKNOWN,
    KML_VALIDITY_INVALID,
    KML_VALIDITY_VALID
} OGRKMLValidity;

class KML
{
public:
	KML();
	virtual ~KML();
	bool open(const char* pszFilename);
	bool isValid();
	bool isHandled(std::string const& elem) const;
	virtual bool isLeaf(std::string const& elem) const;
	virtual bool isFeature(std::string const& elem) const;
	virtual bool isFeatureContainer(std::string const& elem) const;
	virtual bool isContainer(std::string const& elem) const;
	virtual bool isRest(std::string const& elem) const;
    virtual void findLayers(KMLNode* poNode);

	void parse();
	void print(unsigned short what = 3);
    std::string getError() const;
	void classifyNodes();
	void eliminateEmpty();
	int getNumLayers() const;
    bool selectLayer(unsigned short);
    std::string getCurrentName() const;
    Nodetype getCurrentType() const;
    int getNumFeatures() const;
    Feature* getFeature(std::size_t nNum) const;
    bool getExtents(double& pdfXMin, double& pdfXMax, double& pdfYMin, double& pdfYMax) const;

protected:
	void checkValidity();

	static void XMLCALL startElement(void *, const char *, const char **);
	static void XMLCALL startElementValidate(void *, const char *, const char **);
	static void XMLCALL dataHandler(void *, const char *, int);
        static void XMLCALL dataHandlerValidate(void *, const char *, int);
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
	// set to KML_VALIDITY_VALID if the beginning of the file is detected as KML
	OGRKMLValidity validity;
	// file descriptor
	FILE *pKMLFile_;
	// error text ("" when everything is OK")
	std::string sError_;
	// current KMLNode
	KMLNode *poCurrent_;
        
        XML_Parser oCurrentParser;
        int nDataHandlerCounter;
        int nWithoutEventCounter;
};

#endif /* OGR_KML_KML_H_INCLUDED */

