/******************************************************************************
 * $Id$
 * Project:  rasdaman Driver
 * Purpose:  Implement Rasdaman GDAL driver
 * Author:   Constantin Jucovschi, jucovschi@yahoo.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Constantin Jucovschi
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <map>


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


class Subset
{
public:
  Subset(int x_lo, int x_hi, int y_lo, int y_hi)
    : m_x_lo(x_lo), m_x_hi(x_hi),  m_y_lo(y_lo),  m_y_hi(y_hi)
  {}

  bool operator < (const Subset& rhs) const {
    if (m_x_lo < rhs.m_x_lo || m_x_hi < rhs.m_x_hi
        || m_y_lo < rhs.m_y_lo || m_y_hi < rhs.m_y_hi) {

      return true;
    }
    return false;
  }

  bool contains(const Subset& other) const {
    return m_x_lo <= other.m_x_lo && m_x_hi >= other.m_x_hi
        && m_y_lo <= other.m_y_lo && m_y_hi >= other.m_y_hi;
  }

  bool within(const Subset& other) const {
    return other.contains(*this);
  }

  void operator = (const Subset& rhs) {
    m_x_lo = rhs.m_x_lo;
    m_x_hi = rhs.m_x_hi;
    m_y_lo = rhs.m_y_lo;
    m_y_hi = rhs.m_y_hi;
  }

  int x_lo() const { return m_x_lo; }
  int x_hi() const { return m_x_hi; }
  int y_lo() const { return m_y_lo; }
  int y_hi() const { return m_y_hi; }

private:
  int m_x_lo;
  int m_x_hi;
  int m_y_lo;
  int m_y_hi;
};


/************************************************************************/
/* ==================================================================== */
/*        RasdamanDataset                                               */
/* ==================================================================== */
/************************************************************************/

typedef std::map<Subset, r_Ref<r_GMarray> > ArrayCache;

class RasdamanRasterBand;
static CPLString getQuery(const char *templateString, const char* x_lo, const char* x_hi, const char* y_lo, const char* y_hi);

class RasdamanDataset : public GDALPamDataset
{
  friend class RasdamanRasterBand;

public:
  RasdamanDataset(const char*, int, const char*, const char*, const char*);
  ~RasdamanDataset();

  static GDALDataset *Open( GDALOpenInfo * );

protected:

  virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                            void *, int, int, GDALDataType,
                            int, int *, int, int, int );

private:

  ArrayCache m_array_cache;

  r_Ref<r_GMarray>& request_array(int x_lo, int x_hi, int y_lo, int y_hi, int& offsetX, int& offsetY);
  r_Ref<r_GMarray>& request_array(const Subset&, int& offsetX, int& offsetY);

  void clear_array_cache();

  r_Set<r_Ref_Any> execute(const char* string);

  void getTypes(const r_Base_Type* baseType, int &counter, int pos);
  void createBands(const char* queryString);

  r_Database database;
  r_Transaction transaction;
  
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
/*                            RasdamanDataset()                         */
/************************************************************************/

RasdamanDataset::RasdamanDataset(const char* _host, int _port, const char* _username,
                                 const char* _userpassword, const char* _databasename)
  : host(_host), port(_port), username(_username), userpassword(_userpassword),
    databasename(_databasename)
{
  database.set_servername(host, port);
  database.set_useridentification(username, userpassword);
  database.open(databasename);
}

/************************************************************************/
/*                            ~RasdamanDataset()                        */
/************************************************************************/

RasdamanDataset::~RasdamanDataset()
{
  if (transaction.get_status() == r_Transaction::active) {
    transaction.commit();
  }
  database.close();
  FlushCache();
}


CPLErr RasdamanDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace)
{
  if (eRWFlag != GF_Read) {
    CPLError(CE_Failure, CPLE_NoWriteAccess, "Write support is not implemented.");
    return CE_Failure;
  }
  
  transaction.begin(r_Transaction::read_only);
  
  /* TODO: Setup database access/transaction */
  int dummyX, dummyY;
  /* Cache the whole image region */
  CPLDebug("rasdaman", "Pre-caching region (%d, %d, %d, %d).", nXOff, nXOff + nXSize, nYOff, nYOff + nYSize);
  request_array(nXOff, nXOff + nXSize, nYOff, nYOff + nYSize, dummyX, dummyY);

  CPLErr ret = GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                      nBufXSize, nBufYSize, eBufType, nBandCount,
                                      panBandMap, nPixelSpace, nLineSpace, nBandSpace);

  transaction.commit();
  
  /* Clear the cache */
  clear_array_cache();

  return ret;
}


r_Ref<r_GMarray>& RasdamanDataset::request_array(int x_lo, int x_hi, int y_lo, int y_hi, int& offsetX, int& offsetY)
{
  return request_array(Subset(x_lo, x_hi, y_lo, y_hi), offsetX, offsetY);
};

r_Ref<r_GMarray>& RasdamanDataset::request_array(const Subset& subset, int& offsetX, int& offsetY)
{
  // set the offsets to 0
  offsetX = 0; offsetY = 0;
  
  // check whether or not the subset was already requested
  ArrayCache::iterator it = m_array_cache.find(subset);
  if (it != m_array_cache.end()) {
    CPLDebug("rasdaman", "Fetching tile (%d, %d, %d, %d) from cache.",
          subset.x_lo(), subset.x_hi(), subset.y_lo(), subset.y_hi());

    return it->second;
  }

  // check if any tile contains the requested one
  for(it = m_array_cache.begin(); it != m_array_cache.end(); ++it) {
    if (it->first.contains(subset)) {
      const Subset& existing = it->first;

      // TODO: check if offsets are correct
      offsetX = subset.x_lo() - existing.x_lo();
      offsetY = subset.y_lo() - existing.y_lo();
      
      CPLDebug("rasdaman", "Found matching tile (%d, %d, %d, %d) for requested tile (%d, %d, %d, %d). Offests are (%d, %d).",
            existing.x_lo(), existing.x_hi(), existing.y_lo(), existing.y_hi(),
            subset.x_lo(), subset.x_hi(), subset.y_lo(), subset.y_hi(),
            offsetX, offsetY);
      
      
      return it->second;
    }
  }

  if (transaction.get_status() != r_Transaction::active) {
    transaction.begin(r_Transaction::read_only);
  }

  CPLDebug("rasdaman", "Tile (%d, %d, %d, %d) not found in cache, requesting it.",
        subset.x_lo(), subset.x_hi(), subset.y_lo(), subset.y_hi());

  char x_lo[11], x_hi[11], y_lo[11], y_hi[11];

  snprintf(x_lo, sizeof(x_lo), "%d", subset.x_lo());
  snprintf(x_hi, sizeof(x_hi), "%d", subset.x_hi());
  snprintf(y_lo, sizeof(y_lo), "%d", subset.y_lo());
  snprintf(y_hi, sizeof(y_hi), "%d", subset.y_hi());

  CPLString queryString = getQuery(queryParam, x_lo, x_hi, y_lo, y_hi);

  r_Set<r_Ref_Any> result_set = execute(queryString);
  if (result_set.get_element_type_schema()->type_id() == r_Type::MARRAYTYPE) {
    // TODO: throw exception
  }

  if (result_set.cardinality() != 1) {
    // TODO: throw exception
  }
  
  r_Ref<r_GMarray> result_array = r_Ref<r_GMarray>(*result_set.create_iterator());
  //std::auto_ptr<r_GMarray> ptr(new r_GMarray);
  //r_GMarray* ptr_ = ptr.get();
  //(*ptr) = *result_array;
  //std::pair<ArrayCache::iterator, bool> inserted = m_array_cache.insert(ArrayCache::value_type(subset, ptr));
  
  std::pair<ArrayCache::iterator, bool> inserted = m_array_cache.insert(ArrayCache::value_type(subset, result_array));

  return inserted.first->second;//*(ptr);
};


void RasdamanDataset::clear_array_cache() {
  m_array_cache.clear();
};

/************************************************************************/
/* ==================================================================== */
/*                            RasdamanRasterBand                        */
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
/*  RasdamanParams                                                      */
/************************************************************************/

/*struct RasdamanParams
{
  RasdamanParams(const char* dataset_info);

  void connect(const r_Database&);

  const char *query;
  const char *host;
  const int port;
  const char *username;
  const char *password;
};*/


/************************************************************************/
/*                           RasdamanRasterBand()                       */
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
/*                          ~RasdamanRasterBand()                       */
/************************************************************************/

RasdamanRasterBand::~RasdamanRasterBand()
{}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RasdamanRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                       void * pImage )
{
  RasdamanDataset *poGDS = (RasdamanDataset *) poDS;

  memset(pImage, 0, nRecordSize);

  try {
    int x_lo = nBlockXOff * nBlockXSize,
        x_hi = MIN(poGDS->nRasterXSize, (nBlockXOff + 1) * nBlockXSize),
        y_lo = nBlockYOff * nBlockYSize,
        y_hi = MIN(poGDS->nRasterYSize, (nBlockYOff + 1) * nBlockYSize),
        offsetX = 0, offsetY = 0;
    
    r_Ref<r_GMarray>& gmdd = poGDS->request_array(x_lo, x_hi, y_lo, y_hi, offsetX, offsetY);

    int xPos = poGDS->xPos;
    int yPos = poGDS->yPos;
  
    r_Minterval sp = gmdd->spatial_domain();
    r_Point extent = sp.get_extent();
    r_Point base = sp.get_origin();
    
    int extentX = extent[xPos];
    int extentY = extent[yPos];

    CPLDebug("rasdaman", "Extents (%d, %d).", extentX, extentY);

    r_Point access = base;
    char *resultPtr;

    for(int y = y_lo; y < y_hi; ++y) {
      for(int x = x_lo; x < x_hi; ++x) {
        resultPtr = (char*)pImage + ((y - y_lo) * nBlockXSize + x - x_lo) * typeSize;
        //resultPtr = (char*) pImage
        access[xPos] = x;// base[xPos] + offsetX; TODO: check if required
        access[yPos] = y;// base[yPos] + offsetY;
        const char *data = (*gmdd)[access] + typeOffset;
        memcpy(resultPtr, data, typeSize);
      }
    }
  }
  catch (r_Error error) {
    CPLError(CE_Failure, CPLE_AppDefined, "%s", error.what());
    return CPLGetLastErrorType();
  }
  
  return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            RasdamanDataset                           */
/* ==================================================================== */
/************************************************************************/

static CPLString getOption(const char *string, regmatch_t cMatch, const char* defaultValue) {
  if (cMatch.rm_eo == -1 || cMatch.rm_so == -1)
    return defaultValue;
  char *result = new char[cMatch.rm_eo-cMatch.rm_so+1];
  strncpy(result, string + cMatch.rm_so, cMatch.rm_eo - cMatch.rm_so);
  result[cMatch.rm_eo-cMatch.rm_so] = 0;
  CPLString osResult = result;
  delete[] result;
  return osResult;
}

static int getOption(const char *string, regmatch_t cMatch, int defaultValue) {
  if (cMatch.rm_eo == -1 || cMatch.rm_so == -1)
    return defaultValue;
  char *result = new char[cMatch.rm_eo-cMatch.rm_so+1];
  strncpy(result, string + cMatch.rm_so, cMatch.rm_eo - cMatch.rm_so);
  result[cMatch.rm_eo-cMatch.rm_so] = 0;
  int nRet = atoi(result);
  delete[] result;
  return nRet;
}

void replace(CPLString& str, const char *from, const char *to) {
  if(strlen(from) == 0)
    return;
  size_t start_pos = 0;
  while((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, strlen(from), to);
    start_pos += strlen(to); // In case 'to' contains 'from', like replacing 'x' with 'yx'
  }
}


static CPLString getQuery(const char *templateString, const char* x_lo, const char* x_hi, const char* y_lo, const char* y_hi) {
  CPLString result(templateString);

  replace(result, "$x_lo", x_lo);
  replace(result, "$x_hi", x_hi);
  replace(result, "$y_lo", y_lo);
  replace(result, "$y_hi", y_hi);
  
  return result;
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
    for (int i = 0; i < elem; ++i) {
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

r_Set<r_Ref_Any> RasdamanDataset::execute(const char* string) {
  CPLDebug("rasdaman", "Executing query '%s'.", string);
  r_Set<r_Ref_Any> result_set;
  r_OQL_Query query(string);
  r_oql_execute(query, result_set);
  return result_set;
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
    for (int i = 0; i < dim; ++i) {
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

  int result = regcomp(&optionRegEx, "^rasdaman:(query='([[:alnum:][:punct:] ]+)'|host='([[:alnum:][:punct:]]+)'|port=([0-9]+)|user='([[:alnum:]]+)'|password='([[:alnum:]]+)'|database='([[:alnum:]]+)'|tileXSize=([0-9]+)|tileYSize=([0-9]+)| )*", REG_EXTENDED);

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
    CPLError(CE_Failure, CPLE_AppDefined, "Parsing opening parameters failed with error: %s", connString + matches[0].rm_eo);
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

  CPLDebug("rasdaman", "osQueryString: %s", osQueryString.c_str());

  CPLString queryX = getQuery(osQueryString, "*", "*", "0", "0");
  CPLString queryY = getQuery(osQueryString, "0", "0", "*", "*");
  CPLString queryUnit = getQuery(queryParam, "0", "0", "0", "0");

  CPLDebug("rasdaman", "queryX: %s", queryX.c_str());
  CPLDebug("rasdaman", "queryY: %s", queryY.c_str());
  CPLDebug("rasdaman", "queryUnit: %s", queryUnit.c_str());

  RasdamanDataset *rasDataset = NULL;
  try {
    rasDataset = new RasdamanDataset(host, port, username, userpassword, databasename);
    //getMyExtent(osQueryString, posX, sizeX, posY, sizeY);

    r_Transaction transaction;
    transaction.begin(r_Transaction::read_only);

    int dimX = getExtent(queryX, rasDataset->xPos);
    int dimY = getExtent(queryY, rasDataset->yPos);
    rasDataset->nRasterXSize = dimX;
    rasDataset->nRasterYSize = dimY;
    rasDataset->tileXSize = tileXSize;
    rasDataset->tileYSize = tileYSize;
    rasDataset->createBands(queryUnit);

    transaction.commit();

    rasDataset->queryParam = queryParam;
    rasDataset->host = host;
    rasDataset->port = port;
    rasDataset->username = username;
    rasDataset->userpassword = userpassword;
    rasDataset->databasename = databasename;

    return rasDataset;
  } catch (r_Error error) {
    CPLError(CE_Failure, CPLE_AppDefined, "%s", error.what());
    delete rasDataset;
    return NULL;
  }
  

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

