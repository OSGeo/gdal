/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1/2 Translator
 * Purpose:  Implementation of ILI1Reader class.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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


#include "iomhelper.h"
#include "cpl_port.h"

CPL_CVSID("$Id$");


IOM_OBJECT GetAttrObj(IOM_BASKET model, IOM_OBJECT obj, const char* attrname) {
    IOM_OBJECT attrobj = iom_getattrobj(obj, attrname, 0);
    if (attrobj == NULL) return NULL;
    const char *refoid=iom_getobjectrefoid(attrobj);
    if (refoid == NULL) return NULL;
    return iom_getobject(model, refoid);
}

int GetAttrObjPos(IOM_OBJECT obj, const char* attrname) {
    IOM_OBJECT attrobj = iom_getattrobj(obj, attrname, 0);
    if (attrobj == NULL) return -1;
    return iom_getobjectreforderpos(attrobj);
}

const char* GetAttrObjName(IOM_BASKET model, IOM_OBJECT obj, const char* attrname) {
    return iom_getattrvalue(GetAttrObj(model, obj, attrname), "name");
}

IOM_OBJECT GetTypeObj(IOM_BASKET model, IOM_OBJECT obj) {
    IOM_OBJECT typeobj = GetAttrObj(model, obj, "type");
    if (typeobj && EQUAL(iom_getobjecttag(typeobj), "iom04.metamodel.TypeAlias")) {
        typeobj = GetTypeObj(model, GetAttrObj(model, typeobj, "aliasing"));
    }
    return typeobj;
}

const char* GetTypeName(IOM_BASKET model, IOM_OBJECT obj) {
    IOM_OBJECT typeobj = GetTypeObj(model, obj);
    if (typeobj == NULL) return "(null)";
    return iom_getobjecttag(typeobj);
}

unsigned int GetCoordDim(IOM_BASKET model, IOM_OBJECT typeobj) {
  unsigned int dim = 0;
  //find attribute of this type with highest orderpos
  IOM_ITERATOR modelelei=iom_iteratorobject(model);
  IOM_OBJECT modelele=iom_nextobject(modelelei);
  while(modelele){
    const char *tag=iom_getobjecttag(modelele);
    if (tag && EQUAL(tag,"iom04.metamodel.NumericType")) {
      if (GetAttrObj(model, modelele, "coordType") == typeobj) {
        unsigned int orderpos = GetAttrObjPos(modelele, "coordType");
        if (orderpos > dim) dim = orderpos;
      }
    }
    iom_releaseobject(modelele);
    modelele=iom_nextobject(modelelei);
  }
  iom_releaseiterator(modelelei);

  return dim;
}

const char* GetAttrObjName(IOM_BASKET model, const char* tagname) {
  const char* result = NULL;
  IOM_ITERATOR modelelei = iom_iteratorobject(model);
  IOM_OBJECT modelele = iom_nextobject(modelelei);
  while (modelele && result == NULL)
  {
      if(EQUAL(iom_getobjecttag(modelele), tagname)){
          // get name of topic
          result = iom_getattrvalue(modelele, "name");
      }
      iom_releaseobject(modelele);
      modelele=iom_nextobject(modelelei);
  }
  iom_releaseiterator(modelelei);
  return result;
}
