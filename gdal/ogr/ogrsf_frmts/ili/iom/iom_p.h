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


#ifndef IOM_IOM_P_H
#define IOM_IOM_P_H

/** @file
 * private IOM header.
 */

#ifdef _MSC_VER
// disable warning C4786: symbol greater than 255 character,
#pragma warning(disable: 4786)
#endif

#include <iostream>
#include <vector>
#include <stack>
#include <map>
#include <xercesc/sax/Locator.hpp>
#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/framework/XMLBuffer.hpp>
#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/framework/XMLPScanToken.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/StringPool.hpp>
#include <xercesc/util/TransService.hpp>
#include <iom/iom.h>
#ifdef _MSC_VER
#include <crtdbg.h>
// signed/unsigned mismatch
#pragma warning(default: 4018)
#endif

#ifdef _DEBUG
   //#define dbgnew   new( _CLIENT_BLOCK, __FILE__, __LINE__)
   #define dbgnew   new
#else
   #define dbgnew   new
#endif // _DEBUG

#ifdef _MSC_VER
#define IOM_PATH_MAX _MAX_PATH
#else
#define IOM_PATH_MAX PATH_MAX
#endif

XERCES_CPP_NAMESPACE_USE


class XmlWrtAttr {
private:
	const XMLCh *name;
	const XMLCh *value;
	bool  oidAttr;
public:
	const XMLCh *getName();
	const XMLCh *getValue();
	bool isOid();
	XmlWrtAttr(const XMLCh *name,const XMLCh *value);
	XmlWrtAttr(const XMLCh *name,const XMLCh *value,bool isOid);
};

class XmlWriter {
private:
	XMLFormatter *out;
	XMLFormatTarget* destination;
public:
	~XmlWriter();
	XmlWriter();
	void open(const char *filename);
	void startElement(int tagid,XmlWrtAttr attrv[],int attrc);
	void endElement(int tagid);
	void endElement();
	void characters(const XMLCh *const chars);
	void close();
	void printNewLine();
	void printIndent(int level);
private:
	std::stack<int> stack;
};

class IomIterator { 
private: 
	struct iom_iterator *pointee;
public:
	IomIterator(){pointee=0;}
	IomIterator(struct iom_iterator *pointee1);
	IomIterator(const IomIterator& src);
	IomIterator& operator=(const IomIterator& src);
	~IomIterator();
	struct iom_iterator& operator*() const
	{
		return *pointee;
	}
	struct iom_iterator* operator->() const
	{
		return pointee;
	}
	bool isNull(){return pointee==0;}
};

class IomFile { 
private: 
	struct iom_file *pointee;
public:
	IomFile(){pointee=0;}
	IomFile(struct iom_file *pointee1);
	IomFile(const IomFile& src);
	IomFile& operator=(const IomFile& src);
	~IomFile();
	struct iom_file& operator*() const
	{
		return *pointee;
	}
	struct iom_file* operator->() const
	{
		return pointee;
	}
	bool isNull(){return pointee==0;}
};
class IomBasket { 
private: 
	struct iom_basket *pointee;
public:
	IomBasket(){pointee=0;}
	IomBasket(struct iom_basket *pointee1);
	IomBasket(const IomBasket& src);
	IomBasket& operator=(const IomBasket& src);
	~IomBasket();
	struct iom_basket& operator*() const
	{
		return *pointee;
	}
	struct iom_basket* operator->() const
	{
		return pointee;
	}
	bool isNull(){return pointee==0;}
};

class IomObject { 
protected: 
	struct iom_object *pointee;
public:
	IomObject(){pointee=0;}
	IomObject(struct iom_object *pointee1);
	IomObject(const IomObject& src);
	IomObject& operator=(const IomObject& src);
	~IomObject();
	struct iom_object& operator*() const
	{
		return *pointee;
	}
	struct iom_object* operator->() const
	{
		return pointee;
	}
	bool isNull(){return pointee==0;}
};

struct iom_file {
private:
		XMLPScanToken token;
		SAX2XMLReader* parser;
		class ParserHandler* handler;
		// table of baskets
		std::vector<IomBasket> basketv;
public:
		void addBasket(IomBasket basket);
		IomBasket getBasket(const XMLCh *oid);
private:
		const char *filename;
public:
		~iom_file();
		iom_file();
		int readHeader(const char *model);
		int readBasket(IomFile file);
		int readLoop(const char *filename);
		void setFilename(const char *filename);
		int save();
private:
		int useCount;
public:
		struct iom_file *getRef(){useCount++;return this;}
		int freeRef(){useCount--;return useCount;}


private:
		IomBasket ilibasket;

public:
		void setModel(IomBasket model);
		IomBasket getModel();

private:
		XMLCh *headversion_w;
		char *headversion_c;
public:
		void setHeadSecVersion(const XMLCh *version);
		const char *getHeadSecVersion_c();
		const XMLCh *getHeadSecVersion();

private:
		XMLCh *headsender_w;
		char *headsender_c;
public:
		void setHeadSecSender(const XMLCh *sender);
		const char *getHeadSecSender_c();
		const XMLCh *getHeadSecSender();

private:
		XMLCh *headcomment_w;
		char *headcomment_c;
public:
		void setHeadSecComment(const XMLCh *comment);
		const char *getHeadSecComment_c();
		const XMLCh *getHeadSecComment();
private:
		void writePolyline(XmlWriter &out, IomObject &obj,bool hasLineAttr);
		void writeSurface(XmlWriter &out, IomObject &obj);
		void writeAttrs(XmlWriter &out, IomObject &obj);
		void writeAttr(XmlWriter &out, IomObject &obj,int attr);
private:
		// map<int classTag,vector<pair<int pos,int attrName>>>
		typedef std::vector< std::pair<int,int> > attrv_type;
		typedef std::map<int,attrv_type> tagv_type;
		tagv_type tagList;		
		void buildTagList();
		int getQualifiedTypeName(IomObject &aclass);

private:
public:
		friend struct iom_iterator;
};


struct iom_basket {
		struct iom_file *file; // use weak pointer to avoid a circular reference in the smart pointers
private:
		int tag;
		char *tag_c;
public:
		void setTag(int tag);
		const char *getTag_c();
		int getTag();
private:
		int xmlLine;
		int xmlCol;
public:
		void setXMLLineNumber(int line);
		int getXMLLineNumber();
		void setXMLColumnNumber(int col);
		int getXMLColumnNumber();

private:
		int consistency;
public:
		void setConsistency(int cons);
		int getConsistency();

private:
		int kind;
public:
		void setKind(int kind);
		int getKind();

private:
		XMLCh *oid_w;
		char *oid_c;
public:
		void setOid(const XMLCh *oid);
		const char *getOid_c();
		const XMLCh *getOid();
private:
		XMLCh *startstate_w;
		char *startstate_c;
public:
		void setStartState(const XMLCh *startstate);
		const char *getStartState_c();
		const XMLCh *getStartState();
private:
		XMLCh *endstate_w;
		char *endstate_c;
public:
		void setEndState(const XMLCh *endstate);
		const char *getEndState_c();
		const XMLCh *getEndState();
private:
		XMLCh *topics_w;
		char *topics_c;
public:
		void setTopics(const XMLCh *topics);
		const char *getTopics_c();
		const XMLCh *getTopics();
public:
		iom_basket();
		~iom_basket();
private:
		int useCount;
public:
		struct iom_basket *getRef(){useCount++;return this;}
		int freeRef(){useCount--;return useCount;}
private:
		std::vector<IomObject> objectv;
public:
		void addObject(IomObject object);
		IomObject getObject(const XMLCh *oid);

		friend struct iom_iterator;
};

class iom_value {
private:
	const XMLCh *str;
	IomObject obj;
	iom_value(); 
public:
	iom_value(IomObject value); 
	iom_value(const XMLCh *value); 
	const XMLCh *getStr();
	IomObject getObj();
};

struct iom_object {
private:
	int useCount;
public:
	struct iom_object *getRef(){useCount++;return this;}
	int freeRef(){useCount--;return useCount;}
private:
	struct iom_basket *basket; // use weak pointer to avoid a circular reference in the smart pointers
public:
	void setBasket(IomBasket basket);
public:
		//iom_object();
		iom_object();
		virtual ~iom_object();

private:
		int consistency;
public:
		void setConsistency(int cons);
		int getConsistency();

private:
		int operation;
public:
		void setOperation(int op);
		int getOperation();

private:
		int tag;
		char *tag_c;
public:
		void setTag(int tag);
		const char *getTag_c();
		int getTag();

private:
	int xmlLine;
	int xmlCol;
public:
		void setXMLLineNumber(int line);
		int getXMLLineNumber();
		void setXMLColumnNumber(int col);
		int getXMLColumnNumber();

private:
		XMLCh *oid_w;
		char *oid_c;
public:
		void setOid(const XMLCh *oid);
		const XMLCh *getOid();
		const char *getOid_c();

private:
		XMLCh *bid_w;
		char *bid_c;
public:
		void setBid(const XMLCh *bid);
		const char *getBid_c();
		const XMLCh* getBid();

private:
		XMLCh *refOid_w;
		char *refOid_c;
public:
		void setRefOid(const XMLCh *oid);
		const XMLCh *getRefOid();
		const char *getRefOid_c();
private:
		XMLCh *refBid_w;
		char *refBid_c;
public:
		void setRefBid(const XMLCh *bid);
		const XMLCh *getRefBid();
		const char *getRefBid_c();

private:
		unsigned int refOrderPos;
public:
		unsigned int getRefOrderPos();
		void setRefOrderPos(unsigned int value);

private:
		// ArrayList<(String attrName,int index)>
		typedef std::pair<int,int> xmlele_type;
		typedef std::vector<xmlele_type> xmleleidxv_type;
		xmleleidxv_type xmleleidxv;
public:
		int getXmleleCount();
		int getXmleleAttrName(int index);
		int getXmleleValueIdx(int index);


private:
		// HashMap<String attrName,ArrayList<String|IliObject>>
		typedef std::vector<class iom_value> valuev_type;
		typedef std::map<int,valuev_type> attrValuev_type;
		attrValuev_type	attrValuev;		
public:
		void dumpAttrs();
		void parser_addAttrValue(int attrName,IomObject value);
		void parser_addAttrValue(int attrName,const XMLCh *value);
		//void setAttrValue(int attrName,IomObject value);
		int getAttrCount();
		int getAttrName(int index);
		int getAttrValueCount(int attrName);
		void setAttrUndefined(int attrName);
		const XMLCh *getAttrValue(int attrName);
		void setAttrValue(int attrName,const XMLCh *value);
		const XMLCh *getAttrPrim(int attrName,int index);
		IomObject getAttrObj(int attrName,int index);
		void setAttrObj(int attrName,int index,IomObject value);
		void insertAttrObj(int attrName,int index,IomObject value);
		void addAttrObj(int attrName,IomObject value);
		void removeAttrObj(int attrName,int index);
};

struct iom_iterator {
		enum { eBASKET,eOBJECT } type;
		~iom_iterator();
private:
		int useCount;
public:
		struct iom_iterator *getRef(){useCount++;return this;}
		int freeRef(){useCount--;return useCount;}
private:
		IomFile basketv;
		std::vector<IomBasket>::size_type basketi;
public:
		iom_iterator(IomFile file);
		IomBasket next_basket();
private:
		IomBasket objectv;
		std::vector<IomObject>::size_type objecti;
public:
		iom_iterator(IomBasket basket);
		IomObject next_object();
};

class Element {
public:
	IomObject object;
	int propertyName;
	Element(); 
	Element(const Element& src);
	Element& operator=(const Element& src);
	~Element();

private:
	XMLCh *oid;
public:
	const XMLCh *getOid();
	void setOid(const XMLCh *oid);

private:
	XMLCh *bid;
public:
	const XMLCh *getBid();
	void setBid(const XMLCh *bid);

private:
	unsigned int orderPos;
public:
	unsigned int getOrderPos();
	void setOrderPos(unsigned int value);
};


class ParserHandler : 
public DefaultHandler
{

public :
    ParserHandler(IOM_FILE file,const char* model);
    ~ParserHandler();

private:
	// table of element and attribute names
	static XMLStringPool *namev;
public:
	static void at_iom_end();

public:
	static int getTagId(const char *name);
	static int getTagId(const XMLCh *const name);
	static const XMLCh *const getTagName(int tagid);

public:
	// SAX handler
#if XERCES_VERSION_MAJOR >= 3
	void  characters (const XMLCh *const chars, const XMLSize_t length); // xerces 3
	// void  ignorableWhitespace (const XMLCh *const chars, const XMLSize_t length); // xerces 3
#else
	void  characters (const XMLCh *const chars, const unsigned int length); // xerces 2
	// void  ignorableWhitespace (const XMLCh *const chars, const unsigned int length); // xerces 2
#endif
	// void  startDocument ();
	// void  endDocument ();
	void  startElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname, const Attributes &attrs);
	void  endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname);
	// void  processingInstruction (const XMLCh *const target, const XMLCh *const data); 
	void  setDocumentLocator (const Locator *const locator);
	// void  startPrefixMapping (const XMLCh *const prefix, const XMLCh *const uri);
	// void  endPrefixMapping (const XMLCh *const prefix);
	// void  skippedEntity (const XMLCh *const name);
        void startEntity (const XMLCh *const name);
 

    void warning(const SAXParseException& exc);
    void error(const SAXParseException& exc);
    void fatalError(const SAXParseException& exc);

private:
	const Locator *locator;

private:
	struct iom_file *file;
	char *model;
	int skip;
	int level;
	int state;
	XMLBuffer propertyValue;
	IomBasket dataContainer; 
	IomObject object;
        int m_nEntityCounter;
	std::stack<class Element> objStack;
	std::stack<int> stateStack;
	void pushReturnState(int returnState);
	void popReturnState();
	void changeReturnState(int returnState);

private:
		enum e_states {
			BEFORE_TRANSFER=1,
			BEFORE_DATASECTION=2,
			BEFORE_BASKET=3,
			BEFORE_OBJECT=4,
			// StructValue
			ST_BEFORE_PROPERTY=5,
			ST_AFTER_STRUCTVALUE=6,
			ST_BEFORE_EMBASSOC=7,
			ST_BEFORE_CHARACTERS=8,
			ST_AFTER_COORD=9,
			ST_AFTER_POLYLINE=10,
			ST_AFTER_SURFACE=11,
			// CoordValue
			CV_COORD=20,
			CV_C1=21,
			CV_AFTER_C1=22,
			CV_C2=23,
			CV_AFTER_C2=24,
			CV_C3=25,
			CV_AFTER_C3=26,
			// PolylineValue
			PV_POLYLINE=40,
			PV_LINEATTR=41,
			PV_AFTER_LINEATTRSTRUCT=42,
			PV_AFTER_LINEATTR=43,
			PV_CLIPPED=44,
			PV_AFTER_CLIPPED=45,
			// SegmentSequence
			SS_AFTER_COORD=60,
			// SurfaceValue
			SV_SURFACE=80,
			SV_CLIPPED=81,
			SV_AFTER_CLIPPED=82,
			// Boundaries
			BD_BOUNDARY=100,
			BD_AFTER_POLYLINE=101,
			BD_AFTER_BOUNDARY=102,
			// HeaderSection
			START_HEADERSECTION=200
		};
};


class tags {
public:
	static int get_COORD();
	static int get_ARC();
	static int get_C1();
	static int get_C2();
	static int get_C3();
	static int get_A1();
	static int get_A2();
	static int get_iom04_metamodel_AssociationDef();
	static int get_R();
	static int get_lineattr();
	static int get_TRANSFER();
	static int get_iom04_metamodel_Table();
	static int get_DATASECTION();
	static int get_HEADERSECTION();
	static int get_ALIAS();
	static int get_COMMENT();
	static int get_CLIPPED();
	static int get_LINEATTR();
	static int get_SEGMENTS();
	static int get_segment();
	static int get_SURFACE();
	static int get_surface();
	static int get_boundary();
	static int get_BOUNDARY();
	static int get_polyline();
	static int get_POLYLINE();
	static int get_sequence();
	static int get_MULTISURFACE();
	static int get_iom04_metamodel_ViewableAttributesAndRoles();
	static int get_viewable();
	static int get_attributesAndRoles();
	static int get_container();
	static int get_iom04_metamodel_TransferDescription();
	static int get_name();
	static void clear();
private:
	static int COORD;
	static int ARC;
	static int C1;
	static int C2;
	static int C3;
	static int A1;
	static int A2;
	static int iom04_metamodel_AssociationDef;
	static int R;
	static int lineattr;
	static int TRANSFER;
	static int iom04_metamodel_Table;
	static int DATASECTION;
	static int HEADERSECTION;
	static int ALIAS;
	static int COMMENT;
	static int CLIPPED;
	static int LINEATTR;
	static int SEGMENTS;
	static int segment;
	static int SURFACE;
	static int surface;
	static int boundary;
	static int BOUNDARY;
	static int polyline;
	static int POLYLINE;
	static int sequence;
	static int MULTISURFACE;
	static int iom04_metamodel_ViewableAttributesAndRoles;
	static int viewable;
	static int attributesAndRoles;
	static int container;
	static int iom04_metamodel_TransferDescription;
	static int name;
};
class ustrings {
public:
	static const XMLCh*  get_xmlns();
	static const XMLCh*  get_NS_INTERLIS22();
	static const XMLCh*  get_BID();
	static const XMLCh*  get_TOPICS();
	static const XMLCh*  get_KIND();
	static const XMLCh*  get_STARTSTATE();
	static const XMLCh*  get_ENDSTATE();
	static const XMLCh*  get_TID();
	static const XMLCh*  get_OPERATION();
	static const XMLCh*  get_INSERT();
	static const XMLCh*  get_UPDATE();
	static const XMLCh*  get_DELETE();
	static const XMLCh*  get_REF();
	static const XMLCh*  get_EXTREF();
	static const XMLCh*  get_ORDER_POS();
	static const XMLCh*  get_CONSISTENCY();
	static const XMLCh*  get_COMPLETE();
	static const XMLCh*  get_INCOMPLETE();
	static const XMLCh*  get_INCONSISTENT();
	static const XMLCh*  get_ADAPTED();
	static const XMLCh*  get_SENDER();
	static const XMLCh*  get_VERSION();
	static const XMLCh*  get_INITIAL();

};

char *iom_toUTF8(const XMLCh *src);
XMLCh *iom_fromUTF8(const char *src);

// ---------------------------------------------------------------------------
//  This is a simple class that lets us do easy (though not terribly efficient)
//  trancoding of XMLCh data to local code page for display.
// ---------------------------------------------------------------------------
class StrX
{
public :
    // -----------------------------------------------------------------------
    //  Constructors and Destructor
    // -----------------------------------------------------------------------
    StrX(const XMLCh* const toTranscode)
    {
        // Call the private transcoding method
        fLocalForm = XMLString::transcode(toTranscode);
    }

    ~StrX()
    {
        XMLString::release(&fLocalForm);
    }

    // -----------------------------------------------------------------------
    //  Getter methods
    // -----------------------------------------------------------------------
    const char* localForm() const
    {
        return fLocalForm;
    }

private :
    // -----------------------------------------------------------------------
    //  Private data members
    //
    //  fLocalForm
    //      This is the local code page form of the string.
    // -----------------------------------------------------------------------
    char*   fLocalForm;
};

inline std::ostream& operator<<(std::ostream& target, const StrX& toDump)
{
    target << toDump.localForm();
    return target;
}

// ---------------------------------------------------------------------------
//  This is a simple class that lets us do easy (though not terribly efficient)
//  trancoding of char* data to XMLCh data.
// ---------------------------------------------------------------------------
class XStr
{
public :
    // -----------------------------------------------------------------------
    //  Constructors and Destructor
    // -----------------------------------------------------------------------
    XStr(const char* const toTranscode)
    {
        // Call the private transcoding method
        fUnicodeForm = XMLString::transcode(toTranscode);
    }

    ~XStr()
    {
        XMLString::release(&fUnicodeForm);
    }


    // -----------------------------------------------------------------------
    //  Getter methods
    // -----------------------------------------------------------------------
    const XMLCh* unicodeForm() const
    {
        return fUnicodeForm;
    }

private :
    // -----------------------------------------------------------------------
    //  Private data members
    //
    //  fUnicodeForm
    //      This is the Unicode XMLCh format of the string.
    // -----------------------------------------------------------------------
    XMLCh*   fUnicodeForm;
};

#define X(str) XStr(str).unicodeForm()

/** namespace of error handler.
*/
class ErrorUtility {
public:
	static IomBasket errs;
	static int errc;
	static XMLCh itoabuf[40];
	static IOM_ERRLISTENER listener;
	static void at_iom_end();
	static void notifyerr(IomObject obj);
	static void init();
};


#endif
