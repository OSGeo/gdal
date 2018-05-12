#include <map>
#include "ogr_sosi.h"

CPL_CVSID("$Id$")

C2F* poTypes = nullptr;

/*** class definitions ***/

OGRSOSIDataType::OGRSOSIDataType(int nSize) {
    poElements = new OGRSOSISimpleDataType[nSize];
    nElementCount = nSize;
}
OGRSOSIDataType::~OGRSOSIDataType() {
    delete[] poElements;
}
void OGRSOSIDataType::setElement(int nIndex, const char *name, OGRFieldType type) {
    poElements[nIndex].setType(name, type);
}

OGRSOSISimpleDataType::OGRSOSISimpleDataType ():
    osName(),
    nType(OFTString)
{}

OGRSOSISimpleDataType::OGRSOSISimpleDataType (const char *name, OGRFieldType type) {
    setType(name, type);
}
void OGRSOSISimpleDataType::setType (const char *name, OGRFieldType type) {
    osName  = name;
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
  delete poType;
}

void SOSIInitTypes() {
  CPLAssert(poTypes == nullptr);
  poTypes = new C2F();
#include "ogrsosidatatypes.h"

  /* Actually not headers */
  addSimpleType(poTypes, "PUNKT", "", OFTInteger); //ignore
  addSimpleType(poTypes, "KURVE", "", OFTInteger); //ignore
  addSimpleType(poTypes, "FLATE", "", OFTInteger); //ignore
  addSimpleType(poTypes, "BUEP", "", OFTInteger);  //ignore
  addSimpleType(poTypes, "TEKST", "", OFTInteger); //ignore
  addSimpleType(poTypes, "REF", "", OFTString); //ignore this
}

void SOSICleanupTypes()
{
    delete poTypes;
    poTypes = nullptr;
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

static OGRSOSIDataType* SOSIGetTypeFallback(const CPLString& name) {
  addSimpleType(poTypes, name.c_str(), name.c_str(), OFTString);
  return SOSIGetType(name);
}

OGRSOSIDataType* SOSIGetType(const CPLString& name) {
  auto iTypes = poTypes->find(name);
  if (iTypes != poTypes->end()) {
    return &(iTypes->second);
  } else {
    return SOSIGetTypeFallback(name);
  }
}
