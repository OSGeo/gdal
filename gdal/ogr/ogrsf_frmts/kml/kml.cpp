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
#include "kmlnode.h"
#include "kml.h"
#include "cpl_error.h"
#include "cpl_conv.h"
// std
#include <cstdio>
#include <cerrno>
#include <string>
#include <iostream>

KML::KML()
{
	this->nDepth = 0;
	this->bValid = false;
	this->pKMLFile = NULL;
	this->sError = "";
	this->poTrunk = NULL;
	this->poCurrent = NULL;
	this->nNumLayers = -1;
}

KML::~KML()
{
	if(this->pKMLFile != NULL)
    	VSIFClose(this->pKMLFile);
    if(this->poTrunk != NULL)
        delete this->poTrunk;
}

bool KML::open(const char * pszFilename)
{
	if(this->pKMLFile != NULL)
        VSIFClose( this->pKMLFile );

    this->pKMLFile = VSIFOpen( pszFilename, "r" );
    if( this->pKMLFile == NULL )
    {
        return FALSE;
    }
    return TRUE;
}

void KML::parse()
{
	unsigned int nDone, nLen;
	char aBuf[BUFSIZ];
	
	if(this->pKMLFile == NULL) {
		this->sError = "No file given";
		return;
	}
	
    CPLDebug("KML", "Test");
	this->nDepth = 0;
    if(this->poTrunk != NULL) {
        delete this->poTrunk;
        this->poTrunk = NULL;
    }
    if(this->poCurrent != NULL) {
        delete this->poCurrent;
        this->poCurrent = NULL;
    }
	
	XML_Parser oParser = XML_ParserCreate(NULL);
	XML_SetUserData(oParser, this);
	XML_SetElementHandler(oParser, this->startElement, this->endElement);
	XML_SetCharacterDataHandler(oParser, this->dataHandler);

	do {
		nLen = (int)VSIFRead( aBuf, 1, sizeof(aBuf), this->pKMLFile );
		nDone = nLen < sizeof(aBuf);
		if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
			fprintf(stderr,
				"%s at line %u\n",
				XML_ErrorString(XML_GetErrorCode(oParser)),
				XML_GetCurrentLineNumber(oParser));
			XML_ParserFree(oParser);
			VSIRewind(this->pKMLFile);
			return;
		}
	} while (!nDone);

	XML_ParserFree(oParser);
    VSIRewind(this->pKMLFile);
    this->poCurrent = NULL;
}

void KML::checkValidity()
{
	unsigned int nDone, nLen;
	char aBuf[BUFSIZ];

	this->nDepth = 0;
    if(this->poTrunk != NULL) {
        delete this->poTrunk;
        this->poTrunk = NULL;
    }
    if(this->poCurrent != NULL) {
        delete this->poCurrent;
        this->poCurrent = NULL;
    }
	
	if(this->pKMLFile == NULL)
    {
		this->sError = "No file given";
		return;
	}

    CPLDebug("KML", "Check");
	
	XML_Parser oParser = XML_ParserCreate(NULL);
	XML_SetUserData(oParser, this);
	XML_SetElementHandler(oParser, this->startElementValidate, NULL);

	do
    {
		nLen = (int)VSIFRead( aBuf, 1, sizeof(aBuf), this->pKMLFile );
		nDone = nLen < sizeof(aBuf);
		if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            XML_ParserFree(oParser);
            VSIRewind(this->pKMLFile);
			return;
		}
	} while (!nDone || this->bValid);
	
    XML_ParserFree(oParser);
    VSIRewind(this->pKMLFile);
    this->poCurrent = NULL;
}

void XMLCALL KML::startElement(void *pUserData, const char *pszName, const char **ppszAttr)
{
	int i;
	KMLnode *poMynew;
	Attribute *poAtt;
	
	if(((KML *)pUserData)->poTrunk == NULL || (((KML *)pUserData)->poCurrent->getName()).compare("description") != 0) {
    	poMynew = new KMLnode();
	    poMynew->setName(pszName);
    	poMynew->setLevel(((KML *)pUserData)->nDepth);
	
    	for (i = 0; ppszAttr[i]; i += 2)
        {
            poAtt = new Attribute();
            poAtt->sName = ppszAttr[i];
            poAtt->sValue = ppszAttr[i + 1];
            poMynew->addAttribute(poAtt);
    	}

        if(((KML *)pUserData)->poTrunk == NULL)
            ((KML *)pUserData)->poTrunk = poMynew;
        if(((KML *)pUserData)->poCurrent != NULL)
            poMynew->setParent(((KML *)pUserData)->poCurrent);
        ((KML *)pUserData)->poCurrent = poMynew;

    	((KML *)pUserData)->nDepth++;
    }
    else
    {
        std::string sNewContent = "<";
        sNewContent += pszName;
    	for (i = 0; ppszAttr[i]; i += 2)
        {
            sNewContent += " ";
            sNewContent += ppszAttr[i];
            sNewContent += "=";
            sNewContent += ppszAttr[i + 1];
    	}
        sNewContent += ">";
        ((KML *)pUserData)->poCurrent->addContent(sNewContent);
    }
}

void XMLCALL KML::startElementValidate(void *pUserData, const char *pszNamein, const char **ppszAttr)
{
	int i;
	std::string *sName = new std::string(pszNamein);
	std::string *sAttribute;
	
	if(sName->compare("kml") == 0) {
		// Check all Attributes
		for (i = 0; ppszAttr[i]; i += 2) {
			sAttribute = new std::string(ppszAttr[i]);
			// Find the namespace

			if(sAttribute->compare("xmlns") == 0) {
			    delete sAttribute;
				sAttribute = new std::string(ppszAttr[i + 1]);
				// Is it KML 2.1?
				if(sAttribute->compare("http://earth.google.com/kml/2.2") == 0) {
					((KML *)pUserData)->bValid = true;
					((KML *)pUserData)->sVersion = "2.2 (beta)";
				} else if(sAttribute->compare("http://earth.google.com/kml/2.1") == 0) {
					((KML *)pUserData)->bValid = true;
					((KML *)pUserData)->sVersion = "2.1";
				} else if(sAttribute->compare("http://earth.google.com/kml/2.0") == 0) {
					((KML *)pUserData)->bValid = true;
					((KML *)pUserData)->sVersion = "2.0";
				}
			}
			delete sAttribute;
		}
	}
	delete sName;
}

void XMLCALL KML::endElement(void *pUserData, const char *pszName)
{
    KMLnode *poTmp;
    std::string sData, sTmp;
    unsigned short nPos = 0;

	if(((KML *)pUserData)->poCurrent != NULL && 
	    ((KML *)pUserData)->poCurrent->getName().compare(pszName) == 0) {
    	((KML *)pUserData)->nDepth--;
        poTmp = ((KML *)pUserData)->poCurrent;
        // Split the coordinates
        if(((KML *)pUserData)->poCurrent->getName().compare("coordinates") == 0) {
            sData = ((KML *)pUserData)->poCurrent->getContent(0);
            CPLDebug("KML", "Parsing: '%s'", sData.c_str());
            while(sData.length() > 0) {
                // Cut off whitespaces
           	    while((sData[nPos] == ' ' || sData[nPos] == '\n' || sData[nPos] == '\r' || 
        	        sData[nPos] == '\t' ) && sData.length() > 0)
                        sData = sData.substr(1, sData.length()-1);
                CPLDebug("KML", "Parsing first cut: '%s'", sData.c_str(), nPos);
                // Get content
                while(sData[nPos] != ' ' && sData[nPos] != '\n' && sData[nPos] != '\r' && 
                    sData[nPos] != '\t' && nPos < sData.length()) 
                        nPos++;
                sTmp = sData.substr(0, nPos);
                CPLDebug("KML", "Parsing found: '%s' - %d", sTmp.c_str(), nPos);
            	if(sTmp.length() > 0)
        	        ((KML *)pUserData)->poCurrent->addContent(sTmp);
                // Cut the content from the rest
        	    if(nPos < sData.length())
                    sData = sData.substr(nPos, sData.length() - nPos);
                else
                    break;
                nPos = 0;
                CPLDebug("KML", "Parsing next: '%s'", sData.c_str());
            }
            if(((KML *)pUserData)->poCurrent->numContent() > 1)
                ((KML *)pUserData)->poCurrent->deleteContent(0);
        }
        
        if(((KML *)pUserData)->poCurrent->getParent() != NULL)
            ((KML *)pUserData)->poCurrent = ((KML *)pUserData)->poCurrent->getParent();
        else
            ((KML *)pUserData)->poCurrent = NULL;

    	if(!((KML *)pUserData)->isHandled(pszName)) {
    	    CPLDebug("KML", "Not handled: %s", pszName);
    	    delete poTmp;
	    } else {
	        if(((KML *)pUserData)->poCurrent != NULL)
               	((KML *)pUserData)->poCurrent->addChildren(poTmp);
	    }
    } else if(((KML *)pUserData)->poCurrent != NULL) {
        std::string sNewContent = "</";
        sNewContent += pszName;
        sNewContent += ">";
        ((KML *)pUserData)->poCurrent->addContent(sNewContent);
    }
}


void XMLCALL KML::dataHandler(void *pUserData, const char *pszData, int nLen)
{
    if(nLen < 1 || ((KML *)pUserData)->poCurrent == NULL)
        return;
    std::string sData(pszData, nLen), sTmp;
    
	if(((KML *)pUserData)->poCurrent->getName().compare("coordinates") == 0)
	{
	    if(((KML *)pUserData)->poCurrent->numContent() == 0)
	        ((KML *)pUserData)->poCurrent->addContent(sData);
	    else
	        ((KML *)pUserData)->poCurrent->appendContent(sData);
    }
    else
    {
    	while(sData[0] == ' ' || sData[0] == '\n' || sData[0] == '\r' || sData[0] == '\t')
        {
    		sData = sData.substr(1, sData.length()-1);
    	}

       	if(sData.length() > 0)
	        ((KML *)pUserData)->poCurrent->addContent(sData);
	}
}

bool KML::isValid()
{
	this->checkValidity();
	CPLDebug("KML", "Valid: %d Version: %s", this->bValid, this->sVersion.c_str());
	return this->bValid;
}

std::string KML::getError()
{
	return this->sError;
}

void KML::classifyNodes() {
    this->poTrunk->classify(this);
}

void KML::eliminateEmpty() {
    this->poTrunk->eliminateEmpty(this);
}

void KML::print(unsigned short nNum) {
    if(this->poTrunk != NULL)
        this->poTrunk->print(nNum);
}

bool KML::isHandled(std::string const& sIn) const {
    if(this->isLeaf(sIn) || this->isFeature(sIn) || this->isFeatureContainer(sIn) || this->isContainer(sIn) || this->isRest(sIn))
        return true;
    return false;
}

short KML::numLayers() {
    return this->nNumLayers;
}

bool KML::selectLayer(unsigned short nNum) {
    if(this->nNumLayers < 1 || (short)nNum >= this->nNumLayers)
        return FALSE;
    this->poCurrent = this->poTrunk->getLayer(nNum);
    if(this->poCurrent == NULL)
        return FALSE;
    else
        return TRUE;
}

std::string KML::getCurrentName() {
    if(this->poCurrent != NULL)
        return this->poCurrent->getNameElement();
    else
        return "";
}

Nodetype KML::getCurrentType() {
    if(this->poCurrent != NULL)
        return (Nodetype)this->poCurrent->getType();
    else
        return Unknown;
}

short KML::getNumFeatures() {
    if(this->poCurrent != NULL)
        return this->poCurrent->getNumFeatures();
    else
        return -1;
}

Feature* KML::getFeature(unsigned short nNum) {
    if(this->poCurrent != NULL)
        return this->poCurrent->getFeature(nNum);
    else
        return NULL;
}

bool KML::getExtents(double *pdfXMin, double *pdfXMax, double *pdfYMin, double *pdfYMax) {
    if(this->poCurrent != NULL)
    {
        Extent *poXT = this->poCurrent->getExtents();
        *pdfXMin = poXT->dfX1;
        *pdfXMax = poXT->dfX2;
        *pdfYMin = poXT->dfY1;
        *pdfYMax = poXT->dfY2;
        return TRUE;
    }
    else
        return FALSE;
}

