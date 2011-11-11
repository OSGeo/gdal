/******************************************************************************
 * $Id$
 * Project:  rasdaman Driver
 * Purpose:  Implement Rasdaman GDAL driver 
 * Author:   Constantin Jucovschi, jucovschi@yahoo.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Constantin Jucovschi
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
 ******************************************************************************/


#include "gdal_pam.h"
#include "cpl_string.h"
#include "regex.h"
#include <string>
#include <memory>


#define __EXECUTABLE__
#define EARLY_TEMPLATE
#include "raslib/template_inst.hh"
#include "raslib/structuretype.hh"
#include "raslib/type.hh"

#include "rasodmg/database.hh"

CPL_CVSID("$Id$");


CPL_C_START
void	GDALRegister_RASDAMAN(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				RasdamanDataset				*/
/* ==================================================================== */
/************************************************************************/


class RasdamanRasterBand;
static CPLString getQuery(const char *templateString, const char* x_lo, const char* x_hi, const char* y_lo, const char* y_hi);

class RasdamanDataset : public GDALPamDataset
{
  friend class RasdamanRasterBand;

  public:
		~RasdamanDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

private:
  void getTypes(const r_Base_Type* baseType, int &counter, int pos);
  void createBands(const char* queryString);

  CPLString queryParam;
  CPLString host;
  int port;
  CPLString username;
  CPLString userpassword;
  CPLString databasename;
  int xPos;
  int yPos;
  int tileXSize;
  int tileYSize;
};

/************************************************************************/
/* ==================================================================== */
/*                            RasdamanRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class RasdamanRasterBand : public GDALPamRasterBand
{
  friend class RasdamanDataset;
  
  int          nRecordSize;
  int          typeOffset;
  int          typeSize;
  
public:
  
  RasdamanRasterBand( RasdamanDataset *, int, GDALDataType type, int offset, int size, int nBlockXSize, int nBlockYSize );
  ~RasdamanRasterBand();
  
  virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           RasdamanRasterBand()                            */
/************************************************************************/

RasdamanRasterBand::RasdamanRasterBand( RasdamanDataset *poDS, int nBand, GDALDataType type, int offset, int size, int nBlockXSize, int nBlockYSize )
{
  this->poDS = poDS;
  this->nBand = nBand;
  
  eDataType = type;
  typeSize = size;
  typeOffset = offset;

  this->nBlockXSize = nBlockXSize;
  this->nBlockYSize = nBlockYSize;
    
  nRecordSize = nBlockXSize * nBlockYSize * typeSize;
}

/************************************************************************/
/*                          ~RasdamanRasterBand()                            */
/************************************************************************/

RasdamanRasterBand::~RasdamanRasterBand()
{

}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RasdamanRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
				       void * pImage )
  
{
  //cerr << "Read block " << nBlockXOff << " " << nBlockYOff << endl;
  RasdamanDataset *poGDS = (RasdamanDataset *) poDS;
  
  r_Database database;  
  r_Transaction transaction;

  memset(pImage, 0, nRecordSize);

  try {
    database.set_servername(poGDS->host, poGDS->port);
    database.set_useridentification(poGDS->username, poGDS->userpassword);
    database.open(poGDS->databasename);
    transaction.begin();

    char x_lo[11], x_hi[11], y_lo[11], y_hi[11];
    int xPos = poGDS->xPos;
    int yPos = poGDS->yPos;
    
    sprintf(x_lo, "%d", nBlockXOff * nBlockXSize);
    sprintf(x_hi, "%d", (nBlockXOff+1) * nBlockXSize-1);
    sprintf(y_lo, "%d", nBlockYOff * nBlockYSize);
    sprintf(y_hi, "%d", (nBlockYOff+1) * nBlockYSize-1);
    CPLString queryString = getQuery(poGDS->queryParam, x_lo, x_hi, y_lo, y_hi);
  
    r_Set<r_Ref_Any> result_set;
    r_OQL_Query query (queryString);
    r_oql_execute (query, result_set);
    if (result_set.get_element_type_schema()->type_id() == r_Type::MARRAYTYPE) {
      r_Iterator<r_Ref_Any> iter = result_set.create_iterator();
      r_Ref<r_GMarray> gmdd = r_Ref<r_GMarray>(*iter);
      r_Minterval sp = gmdd->spatial_domain();
      r_Point extent = sp.get_extent();
      r_Point base = sp.get_origin();
      int tileX = extent[xPos];
      int tileY = extent[yPos];
      r_Point access = base;
      char *resultPtr;
      for (int j=0; j<tileY; ++j) {
	for (int i=0; i<tileX; ++i) {
	  resultPtr = (char*)pImage + (j*nBlockYSize+i)*typeSize;
	  access[xPos] = base[xPos]+i;
	  access[yPos] = base[yPos]+j;
	  const char *data = (*gmdd)[access] + typeOffset;
	  memcpy(resultPtr, data, typeSize);
	}
      }
    }
    
    transaction.commit();
    database.close();
  } catch (r_Error error) {
    CPLError(CE_Failure, CPLE_AppDefined, "%s", error.what());
    return CPLGetLastErrorType();
  }
  return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				RasdamanDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ~RasdamanDataset()                             */
/************************************************************************/

RasdamanDataset::~RasdamanDataset()
{
  FlushCache();
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

static CPLString getOption(const char *string, regmatch_t cMatch, const char* defaultValue) {
  if (cMatch.rm_eo == -1 || cMatch.rm_so == -1)
    return defaultValue;
  char *result = new char[cMatch.rm_eo-cMatch.rm_so+1];
  strncpy(result, string+cMatch.rm_so, cMatch.rm_eo-cMatch.rm_so);
  result[cMatch.rm_eo-cMatch.rm_so]=0;
  CPLString osResult = result;
  delete[] result;
  return osResult;
}

static int getOption(const char *string, regmatch_t cMatch, int defaultValue) {
  if (cMatch.rm_eo == -1 || cMatch.rm_so == -1)
    return defaultValue;
  char *result = new char[cMatch.rm_eo-cMatch.rm_so+1];
  strncpy(result, string+cMatch.rm_so, cMatch.rm_eo-cMatch.rm_so);
  result[cMatch.rm_eo-cMatch.rm_so]=0;
  int nRet = atoi(result);
  delete[] result;
  return nRet;
}

static CPLString getQuery(const char *templateString, const char* x_lo, const char* x_hi, const char* y_lo, const char* y_hi) {
  static regex_t* replaceRegEx = NULL;
  regmatch_t match[3];
  if (replaceRegEx == NULL) {
    replaceRegEx = new regex_t;
    regcomp(replaceRegEx, "\\$(x|y)_(lo|hi)", REG_EXTENDED);    
  }

  int pos = 0, resPos = 0;
  char *result = new char[strlen(templateString)*2];

  while (regexec(replaceRegEx, templateString+pos, 3, match, 0) == 0) {
    strncpy(result+resPos, templateString+pos, match[1].rm_so-1);
    resPos += match[1].rm_so-1;
    if (templateString[pos+match[1].rm_so]=='x') {
      if (templateString[pos+match[2].rm_so]=='h') {
	strcpy(result+resPos, x_hi); resPos+=strlen(x_hi);
      } else {
	strcpy(result+resPos, x_lo); resPos+=strlen(x_lo);
      }
    } else {
      if (templateString[pos+match[2].rm_so]=='h') {
	strcpy(result+resPos, y_hi); resPos+=strlen(y_hi);
      } else {
	strcpy(result+resPos, y_lo); resPos+=strlen(y_lo);
      }
    }
    pos += match[2].rm_eo;
  }
  int rest = strlen(templateString+pos);
  strncpy(result+resPos, templateString+pos, rest);
  resPos += rest;
  result[resPos]='\0';
  CPLString osResult = result;
  delete[] result;
  return osResult;
}

GDALDataType mapRasdamanTypesToGDAL(r_Type::r_Type_Id typeId) {
  switch (typeId) {
  case r_Type::ULONG:
    return GDT_UInt32;
  case r_Type::LONG:
    return GDT_Int32;
  case r_Type::SHORT:
    return GDT_Int16;
  case r_Type::USHORT:
    return GDT_UInt16;
  case r_Type::BOOL:
  case r_Type::CHAR:
    return GDT_Byte;
  case r_Type::DOUBLE:
    return GDT_Float64;
  case r_Type::FLOAT:
    return GDT_Float32;
  case r_Type::COMPLEXTYPE1:
    return GDT_CFloat32;
  case r_Type::COMPLEXTYPE2:
    return GDT_CFloat64;
  default:
    return GDT_Unknown;
  }
}

void RasdamanDataset::getTypes(const r_Base_Type* baseType, int &counter, int pos) {
  if (baseType->isStructType()) {
    r_Structure_Type* tp = (r_Structure_Type*) baseType;
    int elem = tp->count_elements();
    for (int i=0; i<elem; ++i) {
      r_Attribute attr = (*tp)[i];
      getTypes(&attr.type_of(), counter, attr.global_offset());
    }
    
  }
  if (baseType->isPrimitiveType()) {
    r_Primitive_Type *primType = (r_Primitive_Type*)baseType;
    r_Type::r_Type_Id typeId = primType->type_id();
    SetBand(counter, new RasdamanRasterBand(this, counter, mapRasdamanTypesToGDAL(typeId), pos, primType->size(), this->tileXSize, this->tileYSize));
    counter ++;
  }
}

void RasdamanDataset::createBands(const char* queryString) {
  r_Set<r_Ref_Any> result_set;
  r_OQL_Query query (queryString);
  r_oql_execute (query, result_set);
  if (result_set.get_element_type_schema()->type_id() == r_Type::MARRAYTYPE) {
    r_Iterator<r_Ref_Any> iter = result_set.create_iterator();
    r_Ref<r_GMarray> gmdd = r_Ref<r_GMarray>(*iter);
    const r_Base_Type* baseType = gmdd->get_base_type_schema();
    int counter = 1;
    getTypes(baseType, counter, 0);
  }
}


static int getExtent(const char *queryString, int &pos) {
  r_Set<r_Ref_Any> result_set;
  r_OQL_Query query (queryString);
  r_oql_execute (query, result_set);
  if (result_set.get_element_type_schema()->type_id() == r_Type::MINTERVALTYPE) {
    r_Iterator<r_Ref_Any> iter = result_set.create_iterator();
    r_Ref<r_Minterval> interv = r_Ref<r_Minterval>(*iter);
    r_Point extent = interv->get_extent();
    int dim = extent.dimension();
    int result = -1;
    for (int i=0; i<dim; ++i) {
      if (extent[i] == 1)
	continue;
      if (result != -1)
	return -1;
      result = extent[i];
      pos = i;
    }
    if (result == -1)
      return 1;
    else
      return result;
  } else
    return -1;
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RasdamanDataset::Open( GDALOpenInfo * poOpenInfo )
{
  // buffer to communicate errors 
  char errbuffer[4096];

  // fast checks if current module should handle the request
  // check 1: the request is not on a existing file in the file system
  if (poOpenInfo->fp != NULL) {
    return NULL;
  }
  // check 2: the request contains --collection
  char* connString = poOpenInfo->pszFilename;
  if (!EQUALN(connString, "rasdaman", 8)) {
    return NULL;
  }

  // regex for parsing options
  regex_t optionRegEx;
  // regex for parsing query
  regex_t queryRegEx;
  // array to store matching subexpressions
  regmatch_t matches[10];

  #define QUERY_POSITION 2
  #define SERVER_POSITION 3
  #define PORT_POSITION 4
  #define USERNAME_POSITION 5
  #define USERPASSWORD_POSITION 6
  #define DATABASE_POSITION 7
  #define TILEXSIZE_POSITION 8
  #define TILEYSIZE_POSITION 9

  int result = regcomp(&optionRegEx, "^rasdaman:(query='([[:alnum:][:punct:] ]+)'|host='([[:alnum:]]+)'|port=([0-9]+)|user='([[:alnum:]]+)'|password='([[:alnum:]]+)'|database '([[:alnum:]]+)'|tileXSize=([0-9]+)|tileYSize=([0-9]+)| )*", REG_EXTENDED);

  // should never happen 
  if (result != 0) {
    regerror(result, &optionRegEx, errbuffer, 4096);
    CPLError(CE_Failure, CPLE_AppDefined, "Internal error at compiling option parsing regex: %s", errbuffer);
    return NULL; 
  }

  result = regcomp(&queryRegEx, "^select ([[:alnum:][:punct:] ]*) from ([[:alnum:][:punct:] ]*)$", REG_EXTENDED);
  // should never happen 
  if (result != 0) {
    regerror(result, &queryRegEx, errbuffer, 4096);
    CPLError(CE_Failure, CPLE_AppDefined, "Internal error at compiling option parsing regex: %s", errbuffer);
    return NULL; 
  }

  // executing option parsing regex on the connection string and checking if it succeeds
  result = regexec(&optionRegEx, connString, 10, matches, 0);
  if (result != 0) {
    regerror(result, &optionRegEx, errbuffer, 4096);
    CPLError(CE_Failure, CPLE_AppDefined, "Parsing opening parameters failed with error: %s", errbuffer);
    regfree(&optionRegEx);
    regfree(&queryRegEx);
    return NULL; 
  }

  regfree(&optionRegEx);
  

  // checking if the whole expressions was matches, if not give an error where 
  // the matching stopped and exit
  if (size_t(matches[0].rm_eo) < strlen(connString)) {
    CPLError(CE_Failure, CPLE_AppDefined, "Parsing opening parameters failed with error: %s", connString+matches[0].rm_eo);
    regfree(&queryRegEx);
    return NULL; 
  }

  CPLString queryParam = getOption(connString, matches[QUERY_POSITION], (const char*)NULL);
  CPLString host = getOption(connString, matches[SERVER_POSITION], "localhost");
  int port = getOption(connString, matches[PORT_POSITION], 7001);
  CPLString username = getOption(connString, matches[USERNAME_POSITION], "rasguest");
  CPLString userpassword = getOption(connString, matches[USERPASSWORD_POSITION], "rasguest");
  CPLString databasename = getOption(connString, matches[DATABASE_POSITION], "RASBASE");
  int tileXSize = getOption(connString, matches[TILEXSIZE_POSITION], 1024);
  int tileYSize = getOption(connString, matches[TILEYSIZE_POSITION], 1024);

  result = regexec(&queryRegEx, queryParam, 10, matches, 0);
  if (result != 0) {
    regerror(result, &queryRegEx, errbuffer, 4096);
    CPLError(CE_Failure, CPLE_AppDefined, "Parsing query parameter failed with error: %s", errbuffer);
    regfree(&queryRegEx);
    return NULL; 
  }

  regfree(&queryRegEx);

  CPLString osQueryString = "select sdom(";
  osQueryString += getOption(queryParam, matches[1], "");
  osQueryString += ") from ";
  osQueryString += getOption(queryParam, matches[2], "");

  CPLString queryX = getQuery(osQueryString, "*", "*", "0", "0");
  CPLString queryY = getQuery(osQueryString, "0", "0", "*", "*");
  CPLString queryUnit = getQuery(queryParam, "0", "0", "0", "0");

  stringstream queryStream;
  
  r_Transaction transaction;

  RasdamanDataset *rasDataset = new RasdamanDataset();

  r_Database database;  
  try {
    database.set_servername(host, port);
    database.set_useridentification(username, userpassword);
    database.open(databasename);
    transaction.begin();
    
    int dimX = getExtent(queryX, rasDataset->xPos);
    int dimY = getExtent(queryY, rasDataset->yPos);
    rasDataset->nRasterXSize = dimX;
    rasDataset->nRasterYSize = dimY;
    rasDataset->tileXSize = tileXSize;
    rasDataset->tileYSize = tileYSize;
    rasDataset->createBands(queryUnit);

    transaction.commit();
    database.close();
  } catch (r_Error error) {
    CPLError(CE_Failure, CPLE_AppDefined, "%s", error.what());
    delete rasDataset;
    return NULL;
  }
  rasDataset->queryParam = queryParam;
  rasDataset->host = host;
  rasDataset->port = port;
  rasDataset->username = username;
  rasDataset->userpassword = userpassword;
  rasDataset->databasename = databasename;

  return rasDataset;
}

/************************************************************************/
/*                          GDALRegister_RASDAMAN()                     */
/************************************************************************/

extern void GDALRegister_RASDAMAN()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "RASDAMAN" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "RASDAMAN" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "RASDAMAN" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_rasdaman.html" );

        poDriver->pfnOpen = RasdamanDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
