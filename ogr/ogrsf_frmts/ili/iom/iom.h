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


#ifndef IOM_IOM_H
#define IOM_IOM_H

/** @mainpage
 * IOM soll analog der XML Parser-Interfaces SAX und DOM aufgebaut werden.
 * IOM stellt kein spezifisches Interface zum Metamodell des Compilers bereit. 
 * Das Metamodell des Compilers kann mit denselben Funktionen wie die Daten 
 * abgefragt werden. Die Definitionen aus dem INTERLIS-Modell stehen als normale 
 * IOM-Objekte zur Verfügung.
 * IOM nutzt selbst auch Informationen aus dem Compiler-Metamodell (IOM kann das Modell nicht via IOM lesen, darum ein Adapter) 
 * IOM unterstützt nur INTERLIS 2. (Kann aber zu einem späteren Zeitpunkt für INTERLIS 1 oder ein GML-Subset ergänzt werden.)
 * Kein Modul von IOM ruft die Funktion "exit()" auf. Alle Funktionen geben einen entsprechenden Fehlerstatus zurück.
 * IOM (Interlis Object Model)
 * <UL>
 * <LI>entspricht in etwa DOM. IOM weist aber eine höhere Abstraktionsstufe auf (z.B. gib mir alle Objekte eines Basket, gib mir alle Attribute eines Objekts, gibt mir den Wert eines Attributs).</LI>
 * <LI>ist die Schnittstelle nach Aussen</LI>
 * <LI>Objektbaum durch den man mit entsprechenden Funktionen navigieren kann</LI>
 * <LI>bei grossen Datenmengen werden Objekte in eine binäre Datei ausgelagert</LI>
 * <LI>Der Prefix für die temporären Dateien ist konfigurierbar</LI>
 * <LI>Die XML-Datei wird nicht als Ganzes gelesen. Es wird Basket um Basket gelesen. Gelesene Baskets werden in die binäre Datei ausgelagert. Es ist immer nur ein Basket im RAM.</LI>
 * <LI>eingebettete Beziehungsobjekte erscheinen als eigenständige Objekte, so dass die Verarbeitung von Beziehung einheitlich ist</LI>
 * <LI>Ein Objekt beinhaltet alle Attributwerte inkl. direkten/indirekten Substrukturen</LI>
 * <LI>Funktion um eine XML-Datei gem. Regeln von INTERLIS zu lesen</LI>
 * <LI>Funktion um eine XML-Datei gem. Regeln von INTERLIS zu schreiben</LI>
 * <LI>Headerinformationen aus einer Transferdatei werden gem. einem zu definierenden INTERLIS-Modell als IOM-Objekte bereitgestellt</LI>
 * <LI>muss nach XML-Format Fehlern (not well-formed) wiederaufsetzen und weiterlesen. In diesem Fall stehen die Daten nicht zur Verfügung, nur die Format-Fehlermeldungen. Dieses Feature ist keine Muss-Anforderung</LI>
 * <LI>XML-Format-Fehlermeldungen werden gem. einem zu definierenden INTERLIS-Modell als IOM-Objekte bereitgestellt</LI>
 * <LI>Sortierung: keine</LI>
 * <LI>Transformationsfunktionen (z.B. LV03->LV95): keine</LI>
 * <LI>Aggregationsfunktionen (z.B. count(), sum()): keine</LI>
 * <LI>bei fehlerhaften Daten enthält der Objektbaum:</LI>
 * <UL>
 *	<LI>unvollständige Objekte</LI>
 *	<LI>Objekte mit Attributen die es laut Modell nicht gibt</LI>
 *	<LI>Attributwerte die es laut Modell nicht gibt </LI>
 *	<LI>mehrere Objekte mit der selben OID</LI>
 *	<LI>Referenzen auf Objekte die in den Daten nicht existieren</LI>
 *	<LI>Kardinalitäten die nicht mit dem Modell übereinstimmen</LI>
 * </UL>
 * <LI>Werte von Geometrieattributen werden als Strukturen/Substrukturen abgebildet</LI>
 * <LI>Von einem Basket wird soviel gelesen, wie aufgrund der Modelle bekannt ist. Erweiterungen zu denen kein Modell bekannt ist, werden ignoriert (mit Hilfe der Alias-Tabelle).</LI>
 * </UL>
 */

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/** @file
 * Main IOM header.
 */

/**
 * @defgroup types types that appear in the interfaces of iom.
 */

/**
 * file handle, released by close
 * @ingroup types
 */
typedef struct iom_file *IOM_FILE;

/**
 * basket handle, requires call to release
 * @ingroup types
 */
typedef struct iom_basket *IOM_BASKET;

/**
 * object handle, requires call to release
 * @ingroup types
 */
typedef struct iom_object *IOM_OBJECT;


/**
 * iterator handle, requires call to release
 * @ingroup types
 */
typedef struct iom_iterator *IOM_ITERATOR;

/**
 * a tag, if returned normaly valid as associated handle
 * @ingroup types
 */
typedef const char *IOM_TAG;

/**
 * an object identifier, if returned normaly valid as associated handle
 * @ingroup types
 */
typedef const char *IOM_OID;

/**
 * pointer to function that may act as an error listener of IOM.
 * @param errobj listener should not call iom_releaseobject()
 * @ingroup types
 */
typedef void (*IOM_ERRLISTENER)(IOM_OBJECT errobj) ;

void iom_init();
void iom_end();
void iom_settmpdir(const char *dirname);
char *iom_gettmpnam();
char *iom_searchenv(const char *filename, const char *varname);
int iom_fileexists(const char *filename);
unsigned long iom_currentmilis();

// error message handling
void iom_stderrlistener(IOM_OBJECT errobj);
IOM_ERRLISTENER iom_seterrlistener(IOM_ERRLISTENER newlistener);
void iom_issueparserr(const char *message,int kind,int line,int col);
void iom_issuesemerr(const char *message,IOM_OID bid,IOM_OID oid);
void iom_issueerr(const char *message);
void iom_issueanyerr(IOM_OBJECT err);

// model handling
IOM_BASKET iom_compileIli(int filec,char *filename[]);

// data file handling
IOM_FILE iom_open(const char *filename,int flags,const char *model);
int iom_save(IOM_FILE file);
void iom_close(IOM_FILE file);
IOM_BASKET iom_getmodel(IOM_FILE file);
void iom_setmodel(IOM_FILE file,IOM_BASKET model);

// list all baskets in a file
IOM_ITERATOR iom_iteratorbasket(IOM_FILE file);
IOM_BASKET iom_nextbasket(IOM_ITERATOR iterator); 

// get infos from headersection 
const char *iom_getheadversion(IOM_FILE file);
const char *iom_getheadversionUTF8(IOM_FILE file);
const char *iom_getheadsender(IOM_FILE file);
const char *iom_getheadsenderUTF8(IOM_FILE file);
void iom_setheadsender(IOM_FILE file,const char *sender);
void iom_setheadsenderUTF8(IOM_FILE file,const char *sender);
const char *iom_getheadcomment(IOM_FILE file);
const char *iom_getheadcommentUTF8(IOM_FILE file);
void iom_setheadcomment(IOM_FILE file,const char *comment);
void iom_setheadcommentUTF8(IOM_FILE file,const char *comment);

// gets the basket with a given bid or 0
IOM_BASKET iom_getbasket(IOM_FILE file,IOM_OID oid);


// Basket in eine andere Datei verschieben
int iom_relocatebasket(IOM_FILE newfile,IOM_BASKET basket);

// create a new basket
IOM_BASKET iom_newbasket(IOM_FILE file);
// release handle
int iom_releasebasket(IOM_BASKET basket);


// Basket löschen (Basket aus Datei entfernen)
int iom_deletebasket(IOM_BASKET basket);


// gets/sets OID of a basket
IOM_OID iom_getbasketoid(IOM_BASKET basket);
void iom_setbasketoid(IOM_BASKET basket,IOM_OID oid);

// gets/sets the consistency of a basket
int iom_getbasketconsistency(IOM_BASKET basket);
void iom_setbasketconsistency(IOM_BASKET basket,int consistency);

// gets/sets type of a basket
IOM_TAG iom_getbaskettag(IOM_BASKET basket); 
void iom_setbaskettag(IOM_BASKET basket,IOM_TAG topic); 

// get xml file location of a basket
int iom_getbasketline(IOM_BASKET basket);
int iom_getbasketcol(IOM_BASKET basket);



// get all objects of a basket
IOM_ITERATOR iom_iteratorobject(IOM_BASKET basket);
IOM_OBJECT iom_nextobject(IOM_ITERATOR iterator); 

IOM_OBJECT iom_newobject(IOM_BASKET basket,IOM_TAG type,IOM_OID oid);
int iom_releaseobject(IOM_OBJECT object);
IOM_OBJECT iom_getobject(IOM_BASKET basket,IOM_OID oid);
int iom_deleteobject(IOM_OBJECT object);
int iom_relocateobject(IOM_BASKET basket,IOM_OBJECT object);

// get xml file location of an object
int iom_getobjectline(IOM_OBJECT obj);
int iom_getobjectcol(IOM_OBJECT obj);

// gets/sets tag of an object
IOM_TAG iom_getobjecttag(IOM_OBJECT object); 
void iom_setobjecttag(IOM_OBJECT object,IOM_TAG tag); 

// gets/sets the oid of an object
IOM_OID iom_getobjectoid(IOM_OBJECT object);
void iom_setobjectoid(IOM_OBJECT object,IOM_OID oid);

// gets the oid/bid of the referenced object
IOM_OID iom_getobjectrefoid(IOM_OBJECT object);
IOM_OID iom_getobjectrefbid(IOM_OBJECT object);
void iom_setobjectrefoid(IOM_OBJECT object,IOM_OID refoid);
void iom_setobjectrefbid(IOM_OBJECT object,IOM_OID refbid);

// gets/sets the ORDER_POS of the referenced object
unsigned int iom_getobjectreforderpos(IOM_OBJECT object);
void iom_setobjectreforderpos(IOM_OBJECT object,unsigned int orderPos);

// gets/sets the operation-mode of an object
int iom_getobjectoperation(IOM_OBJECT object);
void iom_setobjectoperation(IOM_OBJECT object,int operation);

// gets/sets the consistency of an object
int iom_getobjectconsistency(IOM_OBJECT object);
void iom_setobjectconsistency(IOM_OBJECT object,int consistency);

// get XML-elements of an object
int iom_getxmlelecount(IOM_OBJECT object);
IOM_TAG iom_getxmleleattrname(IOM_OBJECT object,int index);
int iom_getxmlelevalueidx(IOM_OBJECT object,int index);
char *iom_getxmleleprim(IOM_OBJECT object,int index);
char *iom_getxmleleprimUTF8(IOM_OBJECT object,int index);
IOM_OBJECT iom_getxmleleobj(IOM_OBJECT object,int index);

// gets/sets attribute values of an object
int iom_getattrcount(IOM_OBJECT object);
IOM_TAG iom_getattrname(IOM_OBJECT object,int index);
int iom_getattrvaluecount(IOM_OBJECT object,IOM_TAG attrName);
char *iom_getattrvalue(IOM_OBJECT object,IOM_TAG attrName);
char *iom_getattrvalueUTF8(IOM_OBJECT object,IOM_TAG attrName);
void iom_setattrvalue(IOM_OBJECT object,IOM_TAG attrName,const char *value);
void iom_setattrvalueUTF8(IOM_OBJECT object,IOM_TAG attrName,const char *value);
void iom_setattrundefined(IOM_OBJECT object,IOM_TAG attrName);
char *iom_getattrprim(IOM_OBJECT object,IOM_TAG attrName,int index);
char *iom_getattrprimUTF8(IOM_OBJECT object,IOM_TAG attrName,int index);
IOM_OBJECT iom_getattrobj(IOM_OBJECT object,IOM_TAG attrName,int index);
IOM_OBJECT iom_changeattrobj(IOM_OBJECT object,IOM_TAG attrName,int index,IOM_TAG type);
IOM_OBJECT iom_insertattrobj(IOM_OBJECT object,IOM_TAG attrName,int index,IOM_TAG type);
IOM_OBJECT iom_addattrobj(IOM_OBJECT object,IOM_TAG attrName,IOM_TAG type);
void iom_deleteattrobj(IOM_OBJECT object,IOM_TAG attrName,int index);


// seit dem letzten Lesen/Schreiben geänderte Objekte
IOM_ITERATOR iom_iteratorchgobject(IOM_BASKET basket);
IOM_OBJECT iom_nextchgobject(IOM_ITERATOR iterator); 

// seit dem letzten Lesen/Schreiben gelöschte Objekte
IOM_ITERATOR iom_iteratordelobject(IOM_BASKET basket);
IOM_OBJECT iom_nextdelobject(IOM_ITERATOR iterator); 

/**
 * release handle
 */
int iom_releaseiterator(IOM_ITERATOR iterator);

/** @name ERR runtime errors
 * @ingroup types
 * @{
 */
#define IOM_ERR_NOTIMPLEMENTED  -29000
#define IOM_ERR_XMLPARSER       -29001
#define IOM_ERR_ILLEGALARGUMENT -29002
#define IOM_ERR_ILLEGALSTATE    -29003
/** @} */

/** @name OPENMODE possible values for  iom_open().
 * @ingroup types
 * @{
 */
/** Create file, as necessary.
 *  If the file does not already exist and 
 * the IOM_CREATE flag is not specified, the call will fail. 
 * @see iom_open
 */
#define IOM_CREATE 1
/** Do not read file, if it already exists.
 * @see iom_open
 */
#define IOM_DONTREAD 2

/** @} */


/** @name CONSISTENCY possible values for the consistency of an object or a basket.
 * @ingroup types
 * @{
 */
#define IOM_COMPLETE     0
#define IOM_INCOMPLETE   1
#define IOM_INCONSISTENT 2
#define IOM_ADAPTED      3
/** @} */

/** @name BASKETKIND possible values for the kind of a basket.
 * @ingroup types
 * @{
 */
#define IOM_FULL     0
#define IOM_UPDATE   1
#define IOM_INITIAL  2
/** @} */

/** @name OPMODE possible values for the operation mode of an object.
 * @ingroup types
 * @{
 */
#define IOM_OP_INSERT  0
#define IOM_OP_UPDATE  1
#define IOM_OP_DELETE  2
/** @} */


/** @name ERRKIND possible values for the kind of an parse error.
 * @ingroup types
 * @{
 */
#define IOM_ERRKIND_XMLPARSER 0
#define IOM_ERRKIND_MISSING   1
#define IOM_ERRKIND_INVALID   2
#define IOM_ERRKIND_OTHER     3
/** @} */

/** @page iommodel model used to represent internal objects of iom.
 * @section Errors
 * @image html Errors.jpeg
 * @section ModelDef
 * @image html ModelDef.jpeg
 * @section TopicDef
 * @image html TopicDef.jpeg
 * @section ClassDef
 * @image html ClassDef.jpeg
 * @section DomainDef
 * @image html DomainDef.jpeg
 * @section UnitDef
 * @image html UnitDef.jpeg
 * @section MetadataUseDef
 * @image html MetadataUseDef.jpeg
 * @section ConstraintDef
 * @image html ConstraintDef.jpeg
 * @section Expression
 * @image html Expression.jpeg
 * @section Factor
 * @image html Factor.jpeg
 * @section Constant
 * @image html Constant.jpeg
 * @section ObjectOrAttributePath
 * @image html ObjectOrAttributePath.jpeg
 * @section FunctionDef
 * @image html FunctionDef.jpeg
 * @section RuntimeParameterDef
 * @image html RuntimeParameterDef.jpeg
 * @section ViewDef
 * @image html ViewDef.jpeg
 * @section GraphicDef
 * @image html GraphicDef.jpeg
 * @section INTERLIS
 * @verbinclude iom04.ili
 */



#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
