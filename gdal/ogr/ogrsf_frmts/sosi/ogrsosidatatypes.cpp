#include <map>
#include "ogr_sosi.h"

CPL_CVSID("$Id$")

C2F oTypes;
C2F::iterator iTypes;

/*** class definitions ***/

OGRSOSIDataType::OGRSOSIDataType(int nSize) {
    poElements = new OGRSOSISimpleDataType[nSize];
    nElementCount = nSize;
}
OGRSOSIDataType::~OGRSOSIDataType() {
    //delete[] poElements;
}
void OGRSOSIDataType::setElement(int nIndex, const char *name, OGRFieldType type) {
    poElements[nIndex].setType(name, type);
}

OGRSOSISimpleDataType::OGRSOSISimpleDataType ():
    pszName(""),
    nType(OFTString)
{}

OGRSOSISimpleDataType::OGRSOSISimpleDataType (const char *name, OGRFieldType type) {
    setType(name, type);
}
void OGRSOSISimpleDataType::setType (const char *name, OGRFieldType type) {
    pszName = name;
    nType   = type;
}
OGRSOSISimpleDataType::~OGRSOSISimpleDataType () {}

/*** utility methods ***/

static void addType(C2F* map, const char *key, OGRSOSIDataType *type) {
  map->insert(std::pair<CPLString, OGRSOSIDataType>(CPLString(key),*type));
}
static void addSimpleType(C2F* map, const char *key, const char *gmlKey, OGRFieldType type) {
  OGRSOSIDataType *poType = new OGRSOSIDataType(1);
  poType->setElement(0, gmlKey, type);
  addType(map, key, poType);
}

void SOSIInitTypes() {
#include "ogrsosidatatypes.h"

  /* Actually not headers */
  addSimpleType(&oTypes, "PUNKT", "", OFTInteger); //ignore
  addSimpleType(&oTypes, "KURVE", "", OFTInteger); //ignore
  addSimpleType(&oTypes, "FLATE", "", OFTInteger); //ignore
  addSimpleType(&oTypes, "BUEP", "", OFTInteger);  //ignore
  addSimpleType(&oTypes, "TEKST", "", OFTInteger); //ignore
  addSimpleType(&oTypes, "REF", "", OFTString); //ignore this
}

int SOSITypeToInt(const char* value) {
  return atoi(value);
}
double SOSITypeToReal(const char* value) {
  return CPLAtof(value);
}

void SOSITypeToDate(const char* value, int* date) {
  char dato[9];
  snprintf(dato, 9, "%s", value);
  date[2] = atoi(dato+6);
  dato[6]='\0';
  date[1] = atoi(dato+4);
  dato[4]='\0';
  date[0] = atoi(dato);
}

void SOSITypeToDateTime(const char* value, int* date) {
  char dato[15];
  snprintf(dato, 15, "%s", value);
  if (strlen(dato)==14) {
    date[5] = atoi(dato+12);
    dato[12]='\0';
    date[4] = atoi(dato+10);
    dato[10]='\0';
    date[3] = atoi(dato+8);
  } else {
    date[3] = 0; date[4] = 0; date[5] = 0;
  }
  dato[8]='\0';
  date[2] = atoi(dato+6);
  dato[6]='\0';
  date[1] = atoi(dato+4);
  dato[4]='\0';
  date[0] = atoi(dato);
}

// Leaks memory
static OGRSOSIDataType* SOSIGetTypeFallback(CPLString name) {
  CPLString* copy = new CPLString(name.c_str());
  addSimpleType(&oTypes, copy->c_str(), copy->c_str(), OFTString);
  return SOSIGetType(name);
}

OGRSOSIDataType* SOSIGetType(CPLString name) {
  iTypes = oTypes.find(name);
  if (iTypes != oTypes.end()) {
    return &(iTypes->second);
  } else {
    return SOSIGetTypeFallback(name);
  }
}
