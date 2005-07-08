/** @file
 * implementation of object file
 * @defgroup file file level functions
 * @{
 */

#include <iostream>
#include <string>

#include <iom/iom_p.h>


/** open an INTERLIS XML file.
 * @see IOM_CREATE IOM_DONTREAD
 */
extern "C" IOM_FILE iom_open(const char *filename,int flags,const char *model)
{
	IomFile ret(dbgnew iom_file());
	ret->setFilename(filename);
	if(iom_fileexists(filename)){
		// read file?
		if((flags & IOM_DONTREAD)==0){
			ret->readHeader(model);
		}
	}else{
		// file doesn't exist
		// do not create file?
		if((flags & IOM_CREATE)==0){
			std::string msg="File '";
			msg+=filename;
			msg+="' doesn't exist";
			iom_issueerr(msg.c_str());
			return 0;
		}
	}
	return ret->getRef();
}

/** saves data to an INTERLIS XML file.
 *  Requires: call to iom_setmodel().
 */
extern "C" int iom_save(IOM_FILE file)
{
	return file->save();
}

/** closes an INTERLIS XML file.
 *
 */
extern "C" void iom_close(IOM_FILE file)
{
	if(!file->freeRef()){
		delete file;
	}
}

/** compiles an INTERLIS model file.
 *  Returns 0 if failed.
 *  Requirements: Installed JRE (Java Runtime Environment) and INTERLIS 2-Compiler. The programs 
 *  java and ili2c.jar somewhere in the PATH.
 */
extern "C" IOM_BASKET iom_compileIli(int filec,char *filename[])
{
	char *ili2cout=iom_gettmpnam();
	char *ili2c=iom_searchenv("ili2c.jar","PATH");
	if(!ili2c){
		iom_issueerr("ili2c.jar not found");
		return 0;
	}
	// call compiler
	std::string cmdline="java -jar ";
	cmdline+=ili2c;
	cmdline+=" --without-warnings -oIOM";
        int i;
	for(i=0;i<filec;i++){
		cmdline+=" ";
		cmdline+=filename[i];
	}
	if(i==0){
		iom_issueerr("no ili-file given");
		return 0;
	}
	cmdline+=" >";
	cmdline+=ili2cout;
	//std::cerr << cmdline << std::endl;
	system(cmdline.c_str());
	// read xtf of models
	IomFile model(dbgnew iom_file());
	model->setFilename(ili2cout);
	if(model->readHeader("iom04")){
		return 0;
	}
	IomIterator basketi(dbgnew iom_iterator(model));
        IomBasket nb = basketi->next_basket();
	return nb.isNull() ? 0 : nb->getRef();
}

/** gets the INTERLIS model.
*/
extern "C" IOM_BASKET iom_getmodel(IOM_FILE file)
{
	IomBasket ret=file->getModel();
	return ret.isNull() ? 0 : ret->getRef();
}

/** sets the INTERLIS model.
 */
extern "C" void iom_setmodel(IOM_FILE file,IOM_BASKET model)
{
	file->setModel(model);
}

/** gets an iterator to list all baskets in a file.
 */
extern "C" IOM_ITERATOR iom_iteratorbasket(IOM_FILE file)
{
	return (dbgnew iom_iterator(file))->getRef();
}

/** gets the next basket or 0.
 */
extern "C" IOM_BASKET iom_nextbasket(IOM_ITERATOR iterator)
{
	IomBasket ret=iterator->next_basket();
	return ret.isNull() ? 0 : ret->getRef();
}

/** gets the basket with a given bid or 0.
 */
extern "C" IOM_BASKET iom_getbasket(IOM_FILE file,IOM_OID oid)
{
	IomBasket ret=file->getBasket(X(oid));
	return ret.isNull() ? 0 : ret->getRef();
}

// Basket in eine andere Datei verschieben
extern "C" int iom_relocatebasket(IOM_FILE file,IOM_BASKET basket);

/** creates a new basket.
 */
extern "C" IOM_BASKET iom_newbasket(IOM_FILE file)
{
	IomBasket basket=new iom_basket();
	file->addBasket(basket);
	return basket->getRef();
}

/** gets the content of the VERSION element in the headersection.
 */
extern "C" const char *iom_getheadversion(IOM_FILE file)
{
	return file->getHeadSecVersion_c();
}

/** gets the content of the SENDER element in the headersection.
 */
extern "C" const char *iom_getheadsender(IOM_FILE file)
{
	return file->getHeadSecSender_c();
}

/** sets the content of the SENDER element in the headersection.
 */
extern "C" void iom_setheadsender(IOM_FILE file,const char *sender)
{
	file->setHeadSecSender(X(sender));
}

/** gets the content of the COMMENT element in the headersection.
 */
extern "C" const char *iom_getheadcomment(IOM_FILE file)
{
	return file->getHeadSecComment_c();
}

/** sets the content of the COMMENT element in the headersection.
 */
extern "C" void iom_setheadcomment(IOM_FILE file,const char *comment)
{
	file->setHeadSecComment(X(comment));
}

/** @}
 */

iom_file::iom_file()
: useCount(0)
, filename(0)
, parser(0)
, handler(0)
, headversion_w(0)
, headversion_c(0)
, headsender_w(0)
, headsender_c(0)
, headcomment_w(0)
, headcomment_c(0)

{
}
iom_file::~iom_file()
{
	if(headversion_c)XMLString::release(&headversion_c);
	if(headversion_w)XMLString::release(&headversion_w);
	if(headsender_c)XMLString::release(&headsender_c);
	if(headsender_w)XMLString::release(&headsender_w);
	if(headcomment_c)XMLString::release(&headcomment_c);
	if(headcomment_w)XMLString::release(&headcomment_w);
	if(filename){
		free((void *)filename);
	}
	if(parser){
		delete parser;
	}
	if(handler){
		delete handler;
	}
}

/** sets the model.
 */
void iom_file::setModel(IomBasket model1)
{
	ilibasket=model1;
}

/** gets the model.
 */
IomBasket iom_file::getModel()
{
	return ilibasket;
}

void iom_file::addBasket(IomBasket basket)
{
	basketv.push_back(basket);
}

/** gets a basket with a given oid or null.
 */
IomBasket iom_file::getBasket(const XMLCh *oid)
{
	std::vector<IomBasket>::iterator it;
	for(it=basketv.begin();it!=basketv.end();it++){
		IomBasket obj=*it;
		if(!XMLString::compareString(oid,obj->getOid())){
			return obj;
		}
	}
	return IomBasket();
}


void iom_file::setHeadSecVersion(const XMLCh *version)
{
	if(headversion_c)XMLString::release(&headversion_c);
	if(headversion_w)XMLString::release(&headversion_w);
	headversion_w=XMLString::replicate(version);
}

const char *iom_file::getHeadSecVersion_c()
{
	if(!headversion_w){
		return 0;
	}
	if(!headversion_c){
		headversion_c=XMLString::transcode(headversion_w);
	}
	return headversion_c;
}

void iom_file::setHeadSecSender(const XMLCh *sender)
{
	if(headsender_c)XMLString::release(&headsender_c);
	if(headsender_w)XMLString::release(&headsender_w);
	headsender_w=XMLString::replicate(sender);
}

const char *iom_file::getHeadSecSender_c()
{
	if(!headsender_w){
		return 0;
	}
	if(!headsender_c){
		headsender_c=XMLString::transcode(headsender_w);
	}
	return headsender_c;
}
const XMLCh *iom_file::getHeadSecSender()
{
	return headsender_w;
}

void iom_file::setHeadSecComment(const XMLCh *comment)
{
	if(headcomment_c)XMLString::release(&headcomment_c);
	if(headcomment_w)XMLString::release(&headcomment_w);
	headcomment_w=XMLString::replicate(comment);
}

const char *iom_file::getHeadSecComment_c()
{
	if(!headcomment_w){
		return 0;
	}
	if(!headcomment_c){
		headcomment_c=XMLString::transcode(headcomment_w);
	}
	return headcomment_c;
}
const XMLCh *iom_file::getHeadSecComment()
{
	return headcomment_w;
}


IomFile::IomFile(struct iom_file *pointee1) 
: pointee(pointee1 ? pointee1->getRef() : 0){
}
IomFile::IomFile(const IomFile& src) 
: pointee(src.pointee ? src.pointee->getRef() : 0){
}
IomFile& IomFile::operator=(const IomFile& src){
	if(this!=&src){
		if(pointee && !pointee->freeRef()){
			delete pointee;
		}
		pointee=src.pointee ? src.pointee->getRef() : 0;
	}
	return *this;
}
IomFile::~IomFile(){
	if(pointee && !pointee->freeRef()){
		delete pointee;
	}
}

