/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Python plugin loader
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2017-2019, Even Rouault, <even dot rouault at spatialys dot com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "gdalpython.h"

#include <algorithm>
#include <memory>
#include <mutex>

using namespace GDALPy;

#ifdef GDAL_NO_AUTOLOAD
void GDALDriverManager::AutoLoadPythonDrivers()
{
}

void GDALDriverManager::CleanupPythonDrivers()
{
}

#else

static PyObject *
layer_featureCount(PyObject *m, PyObject* args, PyObject* kwargs);

static const PyMethodDef gdal_python_driver_methods[] = {
    {"layer_featureCount", layer_featureCount,
                                        METH_VARARGS | METH_KEYWORDS, nullptr},
    {nullptr, nullptr, 0, nullptr}
};

static PyObject *Py_None = nullptr;

static PyObject* gpoGDALPythonDriverModule = nullptr;

/************************************************************************/
/*                         IncRefAndReturn()                            */
/************************************************************************/

static PyObject* IncRefAndReturn(PyObject* obj)
{
    Py_IncRef(obj);
    return obj;
}

/************************************************************************/
/*                            CallPython()                              */
/************************************************************************/

static PyObject* CallPython(PyObject* function)
{
    PyObject* pyArgs = PyTuple_New(0);
    PyObject* pRet = PyObject_Call(function, pyArgs, nullptr);
    Py_DecRef(pyArgs);
    return pRet;
}

/************************************************************************/
/*                            CallPython()                              */
/************************************************************************/

static PyObject* CallPython(PyObject* function, int nVal)
{
    PyObject* pyArgs = PyTuple_New(1);
    PyTuple_SetItem(pyArgs, 0, PyLong_FromLong(nVal));
    PyObject* pRet = PyObject_Call(function, pyArgs, nullptr);
    Py_DecRef(pyArgs);
    return pRet;
}

/************************************************************************/
/*                InitializePythonAndLoadGDALPythonDriverModule()               */
/************************************************************************/

static bool InitializePythonAndLoadGDALPythonDriverModule()
{
    if( !GDALPythonInitialize() )
        return false;

    static std::mutex gMutex;
    static bool gbAlreadyInitialized = false;
    std::lock_guard<std::mutex> guard(gMutex);

    if( gbAlreadyInitialized )
        return true;
    gbAlreadyInitialized = true;

    GIL_Holder oHolder(false);

    static PyModuleDef gdal_python_driver_moduledef = {
            PyModuleDef_HEAD_INIT,
            "_gdal_python_driver",
            nullptr,
            static_cast<Py_ssize_t>(-1), // sizeof(struct module_state),
            gdal_python_driver_methods,
            nullptr,
            nullptr,
            nullptr,
            nullptr
    };

    PyObject* module = PyModule_Create2(&gdal_python_driver_moduledef,
                    PYTHON_API_VERSION);
    // Add module to importable modules
    PyObject* sys = PyImport_ImportModule("sys");
    PyObject* sys_modules = PyObject_GetAttrString(sys, "modules");
    PyDict_SetItemString(sys_modules, "_gdal_python_driver", module);
    Py_DecRef(sys_modules);
    Py_DecRef(sys);
    Py_DecRef(module);

    PyObject* poCompiledString = Py_CompileString(
"import _gdal_python_driver\n"
"import json\n"
"import inspect\n"
"import sys\n"
"class BaseLayer(object):\n"
"   RandomRead='RandomRead'\n"
"   FastSpatialFilter='FastSpatialFilter'\n"
"   FastFeatureCount='FastFeatureCount'\n"
"   FastGetExtent='FastGetExtent'\n"
"   StringsAsUTF8='StringsAsUTF8'\n"
"\n"
"   def __init__(self):\n"
"       pass\n"
"\n"
"   def feature_count(self, force):\n"
"       assert isinstance(self, BaseLayer), 'self not instance of BaseLayer'\n"
"       return _gdal_python_driver.layer_featureCount(self, force)\n"
"\n"
"class BaseDataset(object):\n"
"   def __init__(self):\n"
"       pass\n"
"\n"
"class BaseDriver(object):\n"
"   def __init__(self):\n"
"       pass\n"
"\n"
"def _gdal_returnNone():\n"
"  return None"
"\n"
"def _gdal_json_serialize(d):\n"
"  return json.dumps(d)\n"
"\n"
"def _instantiate_plugin(plugin_module):\n"
"   candidate = None\n"
"   for key in dir(plugin_module):\n"
"       elt = getattr(plugin_module, key)\n"
"       if inspect.isclass(elt) and sys.modules[elt.__module__] == plugin_module and issubclass(elt, BaseDriver):\n"
"           if candidate:\n"
"               raise Exception(\"several classes in \" + plugin_module.__name__ + \" deriving from gdal_python_driver.BaseDriver\")\n"
"           candidate = elt\n"
"   if candidate:\n"
"       return candidate()\n"
"   raise Exception(\"cannot find class in \" + plugin_module.__name__ + \" deriving from gdal_python_driver.BaseDriver\")\n",
"gdal_python_driver", Py_file_input);
    gpoGDALPythonDriverModule =
        PyImport_ExecCodeModule("gdal_python_driver", poCompiledString);
    Py_DecRef(poCompiledString);

    // Initialize Py_None
    PyObject* returnNone = PyObject_GetAttrString(gpoGDALPythonDriverModule,
                                                "_gdal_returnNone" );
    Py_None = CallPython(returnNone);
    Py_DecRef(returnNone);

    return true;
}

/************************************************************************/
/*                           GetIntRes()                                */
/************************************************************************/

static
int GetIntRes(PyObject* poObj, const char* pszFunctionName)
{
    PyObject* poMethod = PyObject_GetAttrString(poObj, pszFunctionName );
    if (poMethod == nullptr || PyErr_Occurred())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
        return 0;
    }

    PyObject* poMethodRes = CallPython(poMethod);
    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poMethod);
        return 0;
    }
    Py_DecRef(poMethod);

    int nRes = static_cast<int>(PyLong_AsLong(poMethodRes));
    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poMethodRes);
        return 0;
    }

    Py_DecRef(poMethodRes);
    return nRes;
}

/************************************************************************/
/*                           GetDict()                                  */
/************************************************************************/

static char** GetDict(PyObject* poDict)
{
    PyObject *key, *value;
    size_t pos = 0;

    char** papszRes = nullptr;
    while (PyDict_Next(poDict, &pos, &key, &value))
    {
        if( ErrOccurredEmitCPLError() )
        {
            break;
        }
        CPLString osKey = GetString(key);
        if( ErrOccurredEmitCPLError() )
        {
            break;
        }
        CPLString osValue = GetString(value);
        if( ErrOccurredEmitCPLError() )
        {
            break;
        }
        papszRes = CSLSetNameValue(papszRes, osKey, osValue);
    }
    return papszRes;
}

/************************************************************************/
/*                          GetStringRes()                              */
/************************************************************************/

static
CPLString GetStringRes(PyObject* poObj, const char* pszFunctionName,
                       bool bOptionalMethod = false)
{
    PyObject* poMethod = PyObject_GetAttrString(poObj, pszFunctionName );
    if (poMethod == nullptr || PyErr_Occurred())
    {
        if( bOptionalMethod )
        {
            PyErr_Clear();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "%s", GetPyExceptionString().c_str());
        }
        return CPLString();
    }

    PyObject* poMethodRes = CallPython(poMethod);

    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poMethod);
        return CPLString();
    }
    Py_DecRef(poMethod);

    CPLString osRes = GetString(poMethodRes);
    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poMethodRes);
        return CPLString();
    }

    Py_DecRef(poMethodRes);
    return osRes;
}

/************************************************************************/
/*                          PythonPluginLayer                           */
/************************************************************************/

class PythonPluginLayer final: public OGRLayer
{
        PyObject* m_poLayer = nullptr;
        OGRFeatureDefn* m_poFeatureDefn = nullptr;
        CPLString m_osName{};
        CPLString m_osFIDColumn{};
        bool m_bHasFIDColumn = false;
        std::map<CPLString, CPLStringList> m_oMapMD{};
        PyObject* m_pyFeatureByIdMethod = nullptr;
        bool m_bIteratorHonourSpatialFilter = false;
        bool m_bIteratorHonourAttributeFilter = false;
        bool m_bFeatureCountHonourSpatialFilter = false;
        bool m_bFeatureCountHonourAttributeFilter = false;
        PyObject* m_pyIterator = nullptr;
        bool m_bStopIteration = false;

        void RefreshHonourFlags();
        void StoreSpatialFilter();

        void GetFields();
        void GetGeomFields();
        OGRFeature* TranslateToOGRFeature(PyObject* poObj);

        PythonPluginLayer(const PythonPluginLayer&) = delete;
        PythonPluginLayer& operator= (const PythonPluginLayer&) = delete;

    public:

        explicit PythonPluginLayer(PyObject* poLayer);
        ~PythonPluginLayer();

        const char* GetName() override;
        void ResetReading() override;
        OGRFeature* GetNextFeature() override;
        OGRFeature* GetFeature(GIntBig nFID) override;
        int TestCapability(const char*) override;
        OGRFeatureDefn* GetLayerDefn() override;

        GIntBig GetFeatureCount(int bForce) override;
        const char* GetFIDColumn() override;
        OGRErr SetAttributeFilter(const char*) override;

        void        SetSpatialFilter( OGRGeometry * ) override;
        void        SetSpatialFilter( int iGeomField, OGRGeometry * ) override;

        OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) override;
        OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

        char** GetMetadata(const char* pszDomain = "") override;
};

/************************************************************************/
/*                          PythonPluginLayer()                         */
/************************************************************************/

PythonPluginLayer::PythonPluginLayer(PyObject* poLayer) :
    m_poLayer(poLayer),
    m_poFeatureDefn(nullptr)
{
    SetDescription(PythonPluginLayer::GetName());
    const char* pszPtr = CPLSPrintf("%p", this);
    PyObject* ptr = PyUnicode_FromString(pszPtr);
    PyObject_SetAttrString(m_poLayer, "_gdal_pointer", ptr);
    Py_DecRef(ptr);
    PyObject_SetAttrString(m_poLayer, "spatial_filter_extent", Py_None);
    PyObject_SetAttrString(m_poLayer, "spatial_filter", Py_None);
    PyObject_SetAttrString(m_poLayer, "attribute_filter", Py_None);
    auto poFalse = PyBool_FromLong(false);
    if( !PyObject_HasAttrString(m_poLayer, "iterator_honour_attribute_filter" ) )
    {
        PyObject_SetAttrString(m_poLayer, "iterator_honour_attribute_filter", poFalse);
    }
    if( !PyObject_HasAttrString(m_poLayer, "iterator_honour_spatial_filter" ) )
    {
        PyObject_SetAttrString(m_poLayer, "iterator_honour_spatial_filter", poFalse);
    }
    if( !PyObject_HasAttrString(m_poLayer, "feature_count_honour_attribute_filter" ) )
    {
        PyObject_SetAttrString(m_poLayer, "feature_count_honour_attribute_filter", poFalse);
    }
    if( !PyObject_HasAttrString(m_poLayer, "feature_count_honour_spatial_filter" ) )
    {
        PyObject_SetAttrString(m_poLayer, "feature_count_honour_spatial_filter", poFalse);
    }
    Py_DecRef(poFalse);
    RefreshHonourFlags();

    if( PyObject_HasAttrString(m_poLayer, "feature_by_id" ) )
    {
        m_pyFeatureByIdMethod = PyObject_GetAttrString(m_poLayer, "feature_by_id" );
    }
}

/************************************************************************/
/*                          ~PythonPluginLayer()                        */
/************************************************************************/

PythonPluginLayer::~PythonPluginLayer()
{
    GIL_Holder oHolder(false);
    if( m_poFeatureDefn )
        m_poFeatureDefn->Release();
    Py_DecRef(m_pyFeatureByIdMethod);
    Py_DecRef(m_poLayer);
    Py_DecRef(m_pyIterator);
}

/************************************************************************/
/*                        RefreshHonourFlags()               */
/************************************************************************/

void PythonPluginLayer::RefreshHonourFlags()
{
    if( PyObject_HasAttrString(m_poLayer, "iterator_honour_attribute_filter" ) )
    {
        auto poObj = PyObject_GetAttrString(m_poLayer, "iterator_honour_attribute_filter");
        m_bIteratorHonourAttributeFilter = PyLong_AsLong(poObj) != 0;
        Py_DecRef(poObj);
    }
    if( PyObject_HasAttrString(m_poLayer, "iterator_honour_spatial_filter" ) )
    {
        auto poObj = PyObject_GetAttrString(m_poLayer, "iterator_honour_spatial_filter");
        m_bIteratorHonourSpatialFilter = PyLong_AsLong(poObj) != 0;
        Py_DecRef(poObj);
    }
    if( PyObject_HasAttrString(m_poLayer, "feature_count_honour_attribute_filter" ) )
    {
        auto poObj = PyObject_GetAttrString(m_poLayer, "feature_count_honour_attribute_filter");
        m_bFeatureCountHonourAttributeFilter = PyLong_AsLong(poObj) != 0;
        Py_DecRef(poObj);
    }
    if( PyObject_HasAttrString(m_poLayer, "feature_count_honour_spatial_filter" ) )
    {
        auto poObj = PyObject_GetAttrString(m_poLayer, "feature_count_honour_spatial_filter");
        m_bFeatureCountHonourSpatialFilter = PyLong_AsLong(poObj) != 0;
        Py_DecRef(poObj);
    }
}

/************************************************************************/
/*                          SetAttributeFilter()                        */
/************************************************************************/

OGRErr PythonPluginLayer::SetAttributeFilter(const char* pszFilter )
{
    GIL_Holder oHolder(false);
    PyObject* str = pszFilter ? PyUnicode_FromString(pszFilter) : IncRefAndReturn(Py_None);
    PyObject_SetAttrString(m_poLayer, "attribute_filter", str);
    Py_DecRef(str);

    if( PyObject_HasAttrString(m_poLayer, "attribute_filter_changed" ) )
    {
        auto poObj = PyObject_GetAttrString(m_poLayer, "attribute_filter_changed");
        Py_DecRef(CallPython(poObj));
        Py_DecRef(poObj);
    }

    return OGRLayer::SetAttributeFilter(pszFilter);
}

/************************************************************************/
/*                          StoreSpatialFilter()                        */
/************************************************************************/

void PythonPluginLayer::StoreSpatialFilter()
{
    GIL_Holder oHolder(false);
    if( m_poFilterGeom && !m_poFilterGeom->IsEmpty() )
    {
        PyObject* list = PyList_New(4);
        PyList_SetItem(list, 0, PyFloat_FromDouble(m_sFilterEnvelope.MinX));
        PyList_SetItem(list, 1, PyFloat_FromDouble(m_sFilterEnvelope.MinY));
        PyList_SetItem(list, 2, PyFloat_FromDouble(m_sFilterEnvelope.MaxX));
        PyList_SetItem(list, 3, PyFloat_FromDouble(m_sFilterEnvelope.MaxY));
        PyObject_SetAttrString(m_poLayer, "spatial_filter_extent", list);
        Py_DecRef(list);

        char* pszWKT = nullptr;
        m_poFilterGeom->exportToWkt(&pszWKT);
        PyObject* str = PyUnicode_FromString(pszWKT);
        PyObject_SetAttrString(m_poLayer, "spatial_filter", str);
        Py_DecRef(str);
        CPLFree(pszWKT);
    }
    else
    {
        PyObject_SetAttrString(m_poLayer, "spatial_filter_extent", Py_None);
        PyObject_SetAttrString(m_poLayer, "spatial_filter", Py_None);
    }

    if( PyObject_HasAttrString(m_poLayer, "spatial_filter_changed" ) )
    {
        auto poObj = PyObject_GetAttrString(m_poLayer, "spatial_filter_changed");
        Py_DecRef(CallPython(poObj));
        Py_DecRef(poObj);
    }

}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void PythonPluginLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    OGRLayer::SetSpatialFilter(poGeom);
    StoreSpatialFilter();
}

void PythonPluginLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeom )
{
    OGRLayer::SetSpatialFilter(iGeomField, poGeom);
    StoreSpatialFilter();
}

/************************************************************************/
/*                           GetName()                                  */
/************************************************************************/

const char* PythonPluginLayer::GetName()
{
    if( m_osName.empty() )
    {
        GIL_Holder oHolder(false);

        PyObject* poObj = PyObject_GetAttrString(m_poLayer, "name" );
        if( ErrOccurredEmitCPLError() )
            return m_osName;
        if( PyCallable_Check(poObj) )
        {
            m_osName = GetStringRes(m_poLayer, "name");
        }
        else
        {
            m_osName = GetString(poObj);
            CPL_IGNORE_RET_VAL( ErrOccurredEmitCPLError() );
        }
        Py_DecRef(poObj);
    }
    return m_osName;
}

/************************************************************************/
/*                       TestCapability()                               */
/************************************************************************/

int PythonPluginLayer::TestCapability(const char* pszCap)
{
    GIL_Holder oHolder(false);
    if( PyObject_HasAttrString(m_poLayer, "test_capability") )
    {
        PyObject* poObj = PyObject_GetAttrString(m_poLayer, "test_capability" );
        if( ErrOccurredEmitCPLError() )
            return 0;
        PyObject* pyArgs = PyTuple_New(1);
        PyTuple_SetItem(pyArgs, 0, PyUnicode_FromString(pszCap));
        PyObject* pRet = PyObject_Call(poObj, pyArgs, nullptr);
        Py_DecRef(pyArgs);
        Py_DecRef(poObj);
        if( ErrOccurredEmitCPLError() )
        {
            Py_DecRef(pRet);
            return 0;
        }
        int nRes = static_cast<int>(PyLong_AsLong(pRet));
        Py_DecRef(pRet);
        if( ErrOccurredEmitCPLError() )
        {
            return 0;
        }
        return nRes;
    }
    return 0;
}

/************************************************************************/
/*                         GetFIDColumn()                               */
/************************************************************************/

const char* PythonPluginLayer::GetFIDColumn()
{
    if( !m_bHasFIDColumn )
    {
        m_bHasFIDColumn = true;
        GIL_Holder oHolder(false);
        PyObject* poObj = PyObject_GetAttrString(m_poLayer, "fid_name" );
        if( PyErr_Occurred() )
        {
            PyErr_Clear();
        }
        else
        {
            if( PyCallable_Check(poObj) )
            {
                m_osFIDColumn = GetStringRes(m_poLayer, "fid_name", true);
            }
            else
            {
                m_osFIDColumn = GetString(poObj);
                CPL_IGNORE_RET_VAL( ErrOccurredEmitCPLError() );
            }
            Py_DecRef(poObj);
        }
    }
    return m_osFIDColumn;
}

/************************************************************************/
/*                        layer_featureCount()                           */
/************************************************************************/

static PyObject *
layer_featureCount(PyObject * /*m*/, PyObject* args, PyObject* /*kwargs*/)
{
    PyObject* poPyLayer = nullptr;
    int bForce = 0;
    if( PyArg_ParseTuple(args, "O|i", &poPyLayer, &bForce) )
    {
        PyObject* poPointer = PyObject_GetAttrString(poPyLayer, "_gdal_pointer");
        if( poPointer )
        {
            CPLString osPtr = GetString(poPointer);
            Py_DecRef(poPointer);
            void* pPtr = nullptr;
            sscanf(osPtr, "%p", &pPtr);
            PythonPluginLayer* poLayer = static_cast<PythonPluginLayer*>(pPtr);
            return PyLong_FromLongLong(poLayer->OGRLayer::GetFeatureCount(bForce));
        }
    }
    Py_IncRef(Py_None);
    return Py_None;
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig PythonPluginLayer::GetFeatureCount(int bForce)
{
    GIL_Holder oHolder(false);

    if( PyObject_HasAttrString(m_poLayer, "feature_count" ) &&
        (m_bFeatureCountHonourAttributeFilter || m_poAttrQuery == nullptr) &&
        (m_bFeatureCountHonourSpatialFilter || m_poFilterGeom == nullptr) )
    {
        auto poMethod = PyObject_GetAttrString(m_poLayer,
                                              "feature_count" );
        PyObject* poRet = CallPython(poMethod, bForce);
        if( ErrOccurredEmitCPLError() )
        {
            Py_DecRef(poRet);
            return OGRLayer::GetFeatureCount(bForce);
        }

        GIntBig nRet = PyLong_AsLongLong(poRet);
        if( ErrOccurredEmitCPLError() )
        {
            Py_DecRef(poRet);
            return OGRLayer::GetFeatureCount(bForce);
        }

        Py_DecRef(poRet);
        return nRet;
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                           GetExtent()                                */
/************************************************************************/

OGRErr PythonPluginLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    GIL_Holder oHolder(false);
    if( PyObject_HasAttrString(m_poLayer, "extent" ) )
    {
        PyObject* poMethod = PyObject_GetAttrString(m_poLayer, "extent" );
        if (poMethod != nullptr)
        {
            PyObject* poRet = CallPython(poMethod, bForce);

            if( ErrOccurredEmitCPLError() )
            {
                Py_DecRef(poRet);
                return OGRLayer::GetExtent(psExtent, bForce);
            }

            if( poRet == Py_None )
            {
                Py_DecRef(poRet);
                return OGRERR_FAILURE;
            }

            if( PySequence_Size(poRet) == 4 )
            {
                PyObject* poMinX = PySequence_GetItem(poRet, 0);
                PyObject* poMinY = PySequence_GetItem(poRet, 1);
                PyObject* poMaxX = PySequence_GetItem(poRet, 2);
                PyObject* poMaxY = PySequence_GetItem(poRet, 3);
                double dfMinX = PyFloat_AsDouble(poMinX);
                double dfMinY = PyFloat_AsDouble(poMinY);
                double dfMaxX = PyFloat_AsDouble(poMaxX);
                double dfMaxY = PyFloat_AsDouble(poMaxY);
                if( ErrOccurredEmitCPLError() )
                {
                    Py_DecRef(poRet);
                    return OGRLayer::GetExtent(psExtent, bForce);
                }
                Py_DecRef(poRet);
                psExtent->MinX = dfMinX;
                psExtent->MinY = dfMinY;
                psExtent->MaxX = dfMaxX;
                psExtent->MaxY = dfMaxY;
                return OGRERR_NONE;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "List should have 4 values");
            }

            Py_DecRef(poRet);
        }
    }
    return OGRLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                      TranslateToOGRFeature()                         */
/************************************************************************/

OGRFeature* PythonPluginLayer::TranslateToOGRFeature(PyObject* poObj)
{
    if( poObj == Py_None )
        return nullptr;

    OGRFeature* poFeature = new OGRFeature(GetLayerDefn());

    PyObject* myBool = PyBool_FromLong(1);
    PyObject* myBoolType = PyObject_Type(myBool);
    PyObject* myInt = PyLong_FromLong(1);
    PyObject* myIntType = PyObject_Type(myInt);
    PyObject* myLong = PyLong_FromLongLong(1);
    PyObject* myLongType = PyObject_Type(myLong);
    PyObject* myFloat = PyFloat_FromDouble(1.0);
    PyObject* myFloatType = PyObject_Type(myFloat);

    auto poFields = PyDict_GetItemString(poObj, "fields");
    auto poGeometryFields = PyDict_GetItemString(poObj, "geometry_fields");
    auto poId = PyDict_GetItemString(poObj, "id");
    auto poStyleString = PyDict_GetItemString(poObj, "style");
    PyErr_Clear();

    if( poId && PyObject_IsInstance(poId, myLongType) )
    {
        poFeature->SetFID(
                static_cast<GIntBig>(PyLong_AsLongLong(poId)) );
    }
    else if( poId && PyObject_IsInstance(poId, myIntType) )
    {
        poFeature->SetFID(
                static_cast<GIntBig>(PyLong_AsLong(poId)) );
    }

    if( poStyleString && poStyleString != Py_None )
    {
        CPLString osValue = GetString(poStyleString);
        if( !ErrOccurredEmitCPLError() )
        {
            poFeature->SetStyleString(osValue);
        }
    }

    if ( poGeometryFields && poGeometryFields != Py_None )
    {
        PyObject *key = nullptr;
        PyObject *value = nullptr;
        size_t pos = 0;
        while ( PyDict_Next(poGeometryFields, &pos, &key, &value))
        {
            CPLString osKey = GetString(key);
            if( ErrOccurredEmitCPLError() )
            {
                break;
            }
            if( value != Py_None )
            {
                CPLString osValue = GetString(value);
                if( ErrOccurredEmitCPLError() )
                {
                    break;
                }
                const int idx = m_poFeatureDefn->GetGeomFieldIndex(osKey);
                if( idx >= 0 )
                {
                    OGRGeometry* poGeom = nullptr;
                    OGRGeometryFactory::createFromWkt(osValue.c_str(), nullptr, &poGeom);
                    if( poGeom )
                    {
                        const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(idx);
                        if( poGeomFieldDefn )
                            poGeom->assignSpatialReference(poGeomFieldDefn->GetSpatialRef());
                    }
                    poFeature->SetGeomFieldDirectly(idx, poGeom);
                }
            }
        }
    }

    PyObject *key = nullptr;
    PyObject *value = nullptr;
    size_t pos = 0;
    while ( poFields && poFields != Py_None &&
            PyDict_Next(poFields, &pos, &key, &value))
    {
        CPLString osKey = GetString(key);
        if( ErrOccurredEmitCPLError() )
        {
            break;
        }

        if( value == Py_None )
        {
            int idx = m_poFeatureDefn->GetFieldIndex(osKey);
            if( idx >= 0 )
            {
                poFeature->SetFieldNull(idx);
            }
        }
        else if(PyObject_IsInstance(value, myLongType) )
        {
            int idx = m_poFeatureDefn->GetFieldIndex(osKey);
            if( idx >= 0 )
            {
                poFeature->SetField(idx,
                        static_cast<GIntBig>(PyLong_AsLongLong(value)) );
            }
        }
        else if( PyObject_IsInstance(value, myBoolType) ||
                 PyObject_IsInstance(value, myIntType) )
        {
            int idx = m_poFeatureDefn->GetFieldIndex(osKey);
            if( idx >= 0 )
            {
                poFeature->SetField(idx,
                        static_cast<GIntBig>(PyLong_AsLong(value)) );
            }
        }
        else if( PyObject_IsInstance(value, myFloatType) )
        {
            int idx = m_poFeatureDefn->GetFieldIndex(osKey);
            if( idx >= 0 )
            {
                poFeature->SetField(idx, PyFloat_AsDouble(value) );
            }
        }
        else
        {
            int idx = m_poFeatureDefn->GetFieldIndex(osKey);
            if( idx >= 0 &&
                m_poFeatureDefn->GetFieldDefn(idx)->GetType() == OFTBinary )
            {
                Py_ssize_t nSize = PyBytes_Size(value);
                const char* pszBytes = PyBytes_AsString(value);
                poFeature->SetField(idx, static_cast<int>(nSize), const_cast<GByte*>(
                        reinterpret_cast<const GByte*>(pszBytes)));
                continue;
            }

            CPLString osValue = GetString(value);
            if( ErrOccurredEmitCPLError() )
            {
                break;
            }
            if( idx >= 0 )
            {
                poFeature->SetField(idx, osValue);
            }
        }
    }

    Py_DecRef(myBoolType);
    Py_DecRef(myBool);
    Py_DecRef(myIntType);
    Py_DecRef(myInt);
    Py_DecRef(myLongType);
    Py_DecRef(myLong);
    Py_DecRef(myFloatType);
    Py_DecRef(myFloat);

    return poFeature;
}

/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature* PythonPluginLayer::GetFeature(GIntBig nFID)
{
    GIL_Holder oHolder(false);

    if( m_pyFeatureByIdMethod )
    {
        PyObject* pyArgs = PyTuple_New(1);
        PyTuple_SetItem(pyArgs, 0, PyLong_FromLongLong(nFID));
        PyObject* pRet = PyObject_Call(m_pyFeatureByIdMethod, pyArgs, nullptr);
        Py_DecRef(pyArgs);
        if( ErrOccurredEmitCPLError() )
        {
            Py_DecRef(pRet);
            return nullptr;
        }
        auto poFeature = TranslateToOGRFeature(pRet);
        Py_DecRef(pRet);
        if( ErrOccurredEmitCPLError() )
        {
            return nullptr;
        }
        return poFeature;
    }
    return OGRLayer::GetFeature(nFID);
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

void PythonPluginLayer::ResetReading()
{
    m_bStopIteration = false;

    GIL_Holder oHolder(false);

    Py_DecRef(m_pyIterator);
    m_pyIterator = PyObject_GetIter(m_poLayer);
    CPL_IGNORE_RET_VAL(ErrOccurredEmitCPLError());
}

/************************************************************************/
/*                          GetNextFeature()                            */
/************************************************************************/

OGRFeature* PythonPluginLayer::GetNextFeature()
{
    GIL_Holder oHolder(false);

    if( m_bStopIteration )
        return nullptr;

    if( m_pyIterator == nullptr )
    {
        ResetReading();
        if( m_pyIterator == nullptr )
        {
            return nullptr;
        }
    }

    while( true )
    {
        PyObject* poRet = PyIter_Next(m_pyIterator);
        if( poRet == nullptr )
        {
            m_bStopIteration = true;
            CPL_IGNORE_RET_VAL( ErrOccurredEmitCPLError() );
            return nullptr;
        }

        auto poFeature = TranslateToOGRFeature(poRet);
        Py_DecRef(poRet);
        if( poFeature == nullptr )
        {
            return nullptr;
        }

        if( (m_bIteratorHonourSpatialFilter || m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ) )
            && (m_bIteratorHonourAttributeFilter || m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetLayerDefn()                               */
/************************************************************************/

OGRFeatureDefn* PythonPluginLayer::GetLayerDefn()
{
    if( m_poFeatureDefn )
        return m_poFeatureDefn;

    GIL_Holder oHolder(false);
    m_poFeatureDefn = new OGRFeatureDefn(GetName());
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->SetGeomType(wkbNone);

    GetFields();
    GetGeomFields();
    return m_poFeatureDefn;
}

/************************************************************************/
/*                           GetFields()                                */
/************************************************************************/

void PythonPluginLayer::GetFields()
{
    PyObject* poFields = PyObject_GetAttrString(m_poLayer, "fields" );
    if( ErrOccurredEmitCPLError() )
        return;
    if( PyCallable_Check(poFields) )
    {
        PyObject* poFieldsRes = CallPython(poFields);
        if (ErrOccurredEmitCPLError())
        {
            Py_DecRef(poFields);

            return;
        }
        Py_DecRef(poFields);
        poFields = poFieldsRes;
    }

    size_t nSize = PySequence_Size(poFields);
    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poFields);

        return;
    }
    for(size_t i = 0; i < nSize; i++ )
    {
        PyObject* poItem = PySequence_GetItem(poFields, i);
        if (poItem == nullptr || PyErr_Occurred())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "%s", GetPyExceptionString().c_str());
            Py_DecRef(poFields);

            return;
        }

        PyObject *key, *value;
        size_t pos = 0;
        CPLString osFieldName;
        OGRFieldType eType = OFTString;
        OGRFieldSubType eSubType = OFSTNone;
        while (PyDict_Next(poItem, &pos, &key, &value))
        {
            if( ErrOccurredEmitCPLError() )
            {
                Py_DecRef(poFields);

                return;
            }
            CPLString osKey = GetString(key);
            if( ErrOccurredEmitCPLError() )
            {
                Py_DecRef(poFields);

                return;
            }
            if( strcmp(osKey, "name") == 0 )
            {
                osFieldName = GetString(value);
                if( ErrOccurredEmitCPLError() )
                {
                    Py_DecRef(poFields);

                    return;
                }
            }
            else if( strcmp(osKey, "type") == 0 )
            {
                PyObject* myInt = PyLong_FromLong(1);
                PyObject* myIntType = PyObject_Type(myInt);
                if( PyObject_IsInstance(value, myIntType ) )
                {
                    int nType = static_cast<int>(PyLong_AsLong(value));
                    if( nType < 0 || nType > OFTMaxType )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                    "Wrong type: %d", nType);
                    }
                    else
                    {
                        eType = static_cast<OGRFieldType>(nType);
                        if( ErrOccurredEmitCPLError() )
                        {
                            Py_DecRef(poFields);

                            return;
                        }
                    }
                }
                else
                {
                    CPLString osValue = GetString(value);
                    if( ErrOccurredEmitCPLError() )
                    {
                        Py_DecRef(poFields);

                        return;
                    }
                    if( EQUAL( osValue, "String") )
                        eType = OFTString;
                    else if( EQUAL( osValue, "Integer") ||
                             EQUAL( osValue, "Integer32") ||
                             EQUAL( osValue, "Int32") )
                        eType = OFTInteger;
                    else if( EQUAL( osValue, "Boolean") )
                    {
                        eType = OFTInteger;
                        eSubType = OFSTBoolean;
                    }
                    else if( EQUAL( osValue, "Integer16") ||
                             EQUAL( osValue, "Int16") )
                    {
                        eType = OFTInteger;
                        eSubType = OFSTInt16;
                    }
                    else if( EQUAL( osValue, "Integer64") ||
                             EQUAL( osValue, "Int64") )
                        eType = OFTInteger64;
                    else if( EQUAL( osValue, "Real") )
                        eType = OFTReal;
                    else if( EQUAL( osValue, "Float") ||
                             EQUAL( osValue, "Float32") )
                    {
                        eType = OFTReal;
                        eSubType = OFSTFloat32;
                    }
                    else if( EQUAL( osValue, "Binary") )
                        eType = OFTBinary;
                    else if( EQUAL( osValue, "DateTime") )
                        eType = OFTDateTime;
                    else if( EQUAL( osValue, "Date") )
                        eType = OFTDate;
                    else if( EQUAL( osValue, "Time") )
                        eType = OFTTime;
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                    "Wrong type: %s", osValue.c_str());
                    }
                }
                Py_DecRef(myInt);
                Py_DecRef(myIntType);
            }
            else
            {
                CPLDebug("GDAL", "Unknown field property: %s",
                            osKey.c_str());
            }
        }

        if( !osFieldName.empty() )
        {
            OGRFieldDefn oFieldDefn( osFieldName, eType );
            oFieldDefn.SetSubType(eSubType);
            m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
    }

    Py_DecRef(poFields);
}

/************************************************************************/
/*                         GetGeomFields()                              */
/************************************************************************/

void PythonPluginLayer::GetGeomFields()
{
    PyObject* poFields = PyObject_GetAttrString(m_poLayer, "geometry_fields" );
    if( ErrOccurredEmitCPLError() )
        return;
    if( PyCallable_Check(poFields) )
    {
        PyObject* poFieldsRes = CallPython(poFields);
        if (ErrOccurredEmitCPLError())
        {
            Py_DecRef(poFields);

            return;
        }
        Py_DecRef(poFields);
        poFields = poFieldsRes;
    }

    size_t nSize = PySequence_Size(poFields);
    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poFields);

        return;
    }
    for(size_t i = 0; i < nSize; i++ )
    {
        PyObject* poItem = PySequence_GetItem(poFields, i);
        if (poItem == nullptr || PyErr_Occurred())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "%s", GetPyExceptionString().c_str());
            Py_DecRef(poFields);

            return;
        }

        PyObject *key, *value;
        size_t pos = 0;
        CPLString osFieldName, osSRS;
        OGRwkbGeometryType eType = wkbUnknown;
        while (PyDict_Next(poItem, &pos, &key, &value))
        {
            if( ErrOccurredEmitCPLError() )
            {
                Py_DecRef(poFields);

                return;
            }
            CPLString osKey = GetString(key);
            if( ErrOccurredEmitCPLError() )
            {
                Py_DecRef(poFields);

                return;
            }
            if( strcmp(osKey, "name") == 0 )
            {
                osFieldName = GetString(value);
                if( ErrOccurredEmitCPLError() )
                {
                    Py_DecRef(poFields);

                    return;
                }
            }
            else if( strcmp(osKey, "type") == 0 )
            {
                PyObject* myInt = PyLong_FromLong(1);
                PyObject* myIntType = PyObject_Type(myInt);
                if( PyObject_IsInstance(value, myIntType ) )
                {
                    eType = static_cast<OGRwkbGeometryType>(PyLong_AsLong(value));
                    if( ErrOccurredEmitCPLError() )
                    {
                        Py_DecRef(poFields);

                        return;
                    }
                }
                else
                {
                    CPLString osValue = GetString(value);
                    if( ErrOccurredEmitCPLError() )
                    {
                        Py_DecRef(poFields);

                        return;
                    }
                    eType = OGRFromOGCGeomType(osValue);
                    if( eType == wkbUnknown && !EQUAL(osValue, "Geometry") )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                    "Wrong type: %s", osValue.c_str());
                    }
                }
                Py_DecRef(myInt);
                Py_DecRef(myIntType);
            }
            else if( strcmp(osKey, "srs") == 0 )
            {
                if( value != Py_None )
                {
                    osSRS = GetString(value);
                    if( ErrOccurredEmitCPLError() )
                    {
                        Py_DecRef(poFields);

                        return;
                    }
                }
            }
            else
            {
                CPLDebug("GDAL", "Unknown geometry field property: %s",
                            osKey.c_str());
            }
        }

        OGRGeomFieldDefn oFieldDefn( osFieldName, eType );
        if( !osSRS.empty() )
        {
            OGRSpatialReference* poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            poSRS->SetFromUserInput(osSRS,
                                    OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS);
            oFieldDefn.SetSpatialRef(poSRS);
            poSRS->Release();
        }
        m_poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);
    }

    Py_DecRef(poFields);
}

/************************************************************************/
/*                          GetMetadata()                               */
/************************************************************************/

static char** GetMetadata(PyObject* obj, const char* pszDomain)
{
    if( !PyObject_HasAttrString(obj, "metadata") )
        return nullptr;
    PyObject* poMetadata = PyObject_GetAttrString(obj, "metadata" );
    CPLAssert(poMetadata);
    PyObject* poMethodRes;
    if( PyCallable_Check(poMetadata) )
    {
        PyObject* pyArgs = PyTuple_New(1);
        PyTuple_SetItem(pyArgs, 0, pszDomain && pszDomain[0] ?
            PyUnicode_FromString(pszDomain) : IncRefAndReturn(Py_None));
        poMethodRes = PyObject_Call(poMetadata, pyArgs, nullptr);
        Py_DecRef(pyArgs);
        Py_DecRef(poMetadata);

        if( ErrOccurredEmitCPLError() )
        {
            return nullptr;
        }
    }
    else
    {
        poMethodRes = poMetadata;
    }

    if( poMethodRes == Py_None )
    {
        Py_DecRef(poMethodRes);
        return nullptr;
    }
    char** papszMD = GetDict(poMethodRes);
    Py_DecRef(poMethodRes);
    return papszMD;
}

/************************************************************************/
/*                          GetMetadata()                               */
/************************************************************************/

char** PythonPluginLayer::GetMetadata(const char* pszDomain)
{
    GIL_Holder oHolder(false);
    if( pszDomain == nullptr )
        pszDomain = "";
    m_oMapMD[pszDomain] = CPLStringList(::GetMetadata(m_poLayer, pszDomain));
    return m_oMapMD[pszDomain].List();
}

/************************************************************************/
/*                         PythonPluginDataset                          */
/************************************************************************/

class PythonPluginDataset final: public GDALDataset
{
        PyObject* m_poDataset = nullptr;
        std::map<int, std::unique_ptr<OGRLayer>> m_oMapLayer{};
        std::map<CPLString, CPLStringList> m_oMapMD{};
        bool m_bHasLayersMember = false;

        PythonPluginDataset(const PythonPluginDataset&) = delete;
        PythonPluginDataset& operator= (const PythonPluginDataset&) = delete;

    public:

        PythonPluginDataset(GDALOpenInfo *poOpenInfo, PyObject* poDataset);
        ~PythonPluginDataset();

        int GetLayerCount() override;
        OGRLayer* GetLayer(int) override;
        char** GetMetadata(const char* pszDomain = "") override;
};

/************************************************************************/
/*                         PythonPluginDataset()                        */
/************************************************************************/

PythonPluginDataset::PythonPluginDataset(GDALOpenInfo *poOpenInfo,
                                         PyObject* poDataset) :
    m_poDataset(poDataset)
{
    SetDescription( poOpenInfo->pszFilename );

    GIL_Holder oHolder(false);

    const auto poLayers = PyObject_GetAttrString(m_poDataset, "layers" );
    PyErr_Clear();
    if( poLayers )
    {
        if( PySequence_Check(poLayers) )
        {
            m_bHasLayersMember = true;
            const int nSize = static_cast<int>(PySequence_Size(poLayers));
            for( int i = 0; i < nSize; i++ )
            {
                const auto poLayer = PySequence_GetItem(poLayers, i);
                Py_IncRef(poLayer);
                m_oMapLayer[i] = std::unique_ptr<PythonPluginLayer>(
                    new PythonPluginLayer(poLayer));
            }
        }
        Py_DecRef(poLayers);
    }
}

/************************************************************************/
/*                        ~PythonPluginDataset()                        */
/************************************************************************/

PythonPluginDataset::~PythonPluginDataset()
{
    GIL_Holder oHolder(false);

    if( m_poDataset && PyObject_HasAttrString(m_poDataset, "close") )
    {
        PyObject* poClose = PyObject_GetAttrString(m_poDataset, "close" );
        PyObject* pyArgs = PyTuple_New(0);
        Py_DecRef(PyObject_Call(poClose, pyArgs, nullptr));
        Py_DecRef(pyArgs);
        Py_DecRef(poClose);

        CPL_IGNORE_RET_VAL( ErrOccurredEmitCPLError() );
    }
    Py_DecRef(m_poDataset);
}

/************************************************************************/
/*                          GetLayerCount()                             */
/************************************************************************/

int PythonPluginDataset::GetLayerCount()
{
    if( m_bHasLayersMember )
        return static_cast<int>(m_oMapLayer.size());

    GIL_Holder oHolder(false);
    return GetIntRes(m_poDataset, "layer_count");
}

/************************************************************************/
/*                            GetLayer()                                */
/************************************************************************/

OGRLayer* PythonPluginDataset::GetLayer(int idx)
{
    if( idx < 0 )
        return nullptr;

    auto oIter = m_oMapLayer.find(idx);
    if( oIter != m_oMapLayer.end() )
        return m_oMapLayer[idx].get();

    if( m_bHasLayersMember )
        return nullptr;

    GIL_Holder oHolder(false);

    PyObject* poMethod = PyObject_GetAttrString(m_poDataset, "layer" );
    if (poMethod == nullptr || PyErr_Occurred())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
        return nullptr;
    }

    PyObject* poMethodRes = CallPython(poMethod, idx);
    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poMethod);
        return nullptr;
    }
    Py_DecRef(poMethod);

    if(  poMethodRes == Py_None )
    {
        m_oMapLayer[idx] = nullptr;
        Py_DecRef(poMethodRes);
        return nullptr;
    }
    m_oMapLayer[idx] = std::unique_ptr<PythonPluginLayer>(
        new PythonPluginLayer(poMethodRes));
    return m_oMapLayer[idx].get();
}

/************************************************************************/
/*                          GetMetadata()                               */
/************************************************************************/

char** PythonPluginDataset::GetMetadata(const char* pszDomain)
{
    GIL_Holder oHolder(false);
    if( pszDomain == nullptr )
        pszDomain = "";
    m_oMapMD[pszDomain] = CPLStringList(::GetMetadata(m_poDataset, pszDomain));
    return m_oMapMD[pszDomain].List();
}

/************************************************************************/
/*                          PythonPluginDriver                          */
/************************************************************************/

class PythonPluginDriver: public GDALDriver
{
        CPLMutex* m_hMutex = nullptr;
        CPLString m_osFilename;
        PyObject* m_poPlugin = nullptr;

        PythonPluginDriver(const PythonPluginDriver&) = delete;
        PythonPluginDriver& operator= (const PythonPluginDriver&) = delete;

        bool LoadPlugin();

        int Identify( GDALOpenInfo *);
        static int IdentifyEx(GDALDriver*, GDALOpenInfo *);

        GDALDataset* Open( GDALOpenInfo *);
        static GDALDataset* OpenEx(GDALDriver*, GDALOpenInfo *);

    public:
        PythonPluginDriver(const char* pszFilename,
                           const char* pszPluginName, char** papszMD);
        ~PythonPluginDriver();
};

/************************************************************************/
/*                            LoadPlugin()                              */
/************************************************************************/

bool PythonPluginDriver::LoadPlugin()
{
    CPLMutexHolder oMutexHolder(&m_hMutex);
    if( m_poPlugin )
        return true;
    if( !InitializePythonAndLoadGDALPythonDriverModule() )
        return false;
    GIL_Holder oHolder(false);

    CPLString osStr;
    VSILFILE* fp = VSIFOpenL(m_osFilename, "rb");
    VSIFSeekL(fp, 0, SEEK_END);
    auto nSize = VSIFTellL(fp);
    if( nSize > 10 * 1024 * 1024 )
    {
        VSIFCloseL(fp);
        return false;
    }
    VSIFSeekL(fp, 0, SEEK_SET);
    osStr.resize(static_cast<size_t>(nSize));
    VSIFReadL(&osStr[0], 1, static_cast<size_t>(nSize), fp);
    VSIFCloseL(fp);
    PyObject* poCompiledString = Py_CompileString(
        osStr,
        m_osFilename, Py_file_input);
    if( poCompiledString == nullptr || PyErr_Occurred() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Couldn't compile code:\n%s",
                 GetPyExceptionString().c_str());
        return false;
    }
    const CPLString osPluginModuleName(CPLGetBasename(m_osFilename));
    PyObject* poModule =
        PyImport_ExecCodeModule(osPluginModuleName, poCompiledString);
    Py_DecRef(poCompiledString);

    if( poModule == nullptr || PyErr_Occurred() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
        return false;
    }

    PyObject* poInstantiate = PyObject_GetAttrString(gpoGDALPythonDriverModule,
                                                     "_instantiate_plugin" );
    CPLAssert(poInstantiate);

    PyObject* pyArgs = PyTuple_New(1);
    PyTuple_SetItem(pyArgs, 0, poModule);
    PyObject* poPlugin = PyObject_Call(poInstantiate, pyArgs, nullptr);
    Py_DecRef(pyArgs);
    Py_DecRef(poInstantiate);

    if( ErrOccurredEmitCPLError() )
    {
        return false;
    }
    else
    {
        m_poPlugin = poPlugin;
        return true;
    }
}

/************************************************************************/
/*                       BuildIdentifyOpenArgs()                        */
/************************************************************************/

static void BuildIdentifyOpenArgs(GDALOpenInfo *poOpenInfo,
                                  PyObject*& pyArgs,
                                  PyObject*& pyKwargs)
{
    pyArgs = PyTuple_New(3);
    PyTuple_SetItem(pyArgs, 0, PyUnicode_FromString(poOpenInfo->pszFilename));
    PyTuple_SetItem(pyArgs, 1, PyBytes_FromStringAndSize(
                            poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes));
    PyTuple_SetItem(pyArgs, 2, PyLong_FromLong(poOpenInfo->nOpenFlags));
    pyKwargs = PyDict_New();
    PyObject* pyOpenOptions = PyDict_New();
    PyDict_SetItemString(pyKwargs, "open_options", pyOpenOptions);
    if( poOpenInfo->papszOpenOptions )
    {
        for( char** papszIter = poOpenInfo->papszOpenOptions; *papszIter; ++papszIter )
        {
            char* pszKey = nullptr;
            const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
            if( pszKey && pszValue )
            {
                auto pyValue = PyUnicode_FromString(pszValue);
                PyDict_SetItemString(pyOpenOptions, pszKey, pyValue);
                Py_DecRef(pyValue);
            }
            CPLFree(pszKey);
        }
    }
    Py_DecRef(pyOpenOptions);
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int PythonPluginDriver::Identify(GDALOpenInfo *poOpenInfo)
{
    if( m_poPlugin == nullptr )
    {
        if( !LoadPlugin() )
            return FALSE;
    }

    GIL_Holder oHolder(false);

    PyObject* poMethod = PyObject_GetAttrString(m_poPlugin, "identify" );
    if (poMethod == nullptr || PyErr_Occurred())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
        return 0;
    }

    PyObject* pyArgs = nullptr;
    PyObject* pyKwargs = nullptr;
    BuildIdentifyOpenArgs(poOpenInfo, pyArgs, pyKwargs);
    PyObject* poMethodRes = PyObject_Call(poMethod, pyArgs, pyKwargs);
    Py_DecRef(pyArgs);
    Py_DecRef(pyKwargs);

    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poMethod);
        return 0;
    }
    Py_DecRef(poMethod);

    int nRes = static_cast<int>(PyLong_AsLong(poMethodRes));
    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poMethodRes);
        return 0;
    }

    Py_DecRef(poMethodRes);
    return nRes;
}

/************************************************************************/
/*                            IdentifyEx()                              */
/************************************************************************/

int PythonPluginDriver::IdentifyEx(GDALDriver* poDrv, GDALOpenInfo *poOpenInfo)
{
    return reinterpret_cast<PythonPluginDriver*>(poDrv)->Identify(poOpenInfo);
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

GDALDataset* PythonPluginDriver::Open(GDALOpenInfo *poOpenInfo)
{
    if( m_poPlugin == nullptr )
    {
        if( !LoadPlugin() )
            return nullptr;
    }

    GIL_Holder oHolder(false);

    PyObject* poMethod = PyObject_GetAttrString(m_poPlugin, "open" );
    if (poMethod == nullptr || PyErr_Occurred())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s", GetPyExceptionString().c_str());
        return nullptr;
    }

    PyObject* pyArgs = nullptr;
    PyObject* pyKwargs = nullptr;
    BuildIdentifyOpenArgs(poOpenInfo, pyArgs, pyKwargs);
    PyObject* poMethodRes = PyObject_Call(poMethod, pyArgs, pyKwargs);
    Py_DecRef(pyArgs);
    Py_DecRef(pyKwargs);

    if( ErrOccurredEmitCPLError() )
    {
        Py_DecRef(poMethod);
        return nullptr;
    }
    Py_DecRef(poMethod);

    if( poMethodRes == Py_None )
    {
        Py_DecRef(poMethodRes);
        return nullptr;
    }
    return new PythonPluginDataset(poOpenInfo, poMethodRes);
}

/************************************************************************/
/*                              OpenEx()                                */
/************************************************************************/

GDALDataset* PythonPluginDriver::OpenEx(GDALDriver* poDrv,
                                        GDALOpenInfo *poOpenInfo)
{
    return reinterpret_cast<PythonPluginDriver*>(poDrv)->Open(poOpenInfo);
}

/************************************************************************/
/*                        PythonPluginDriver()                          */
/************************************************************************/

PythonPluginDriver::PythonPluginDriver(const char* pszFilename,
                                       const char* pszPluginName,
                                       char** papszMD) :
    m_hMutex(nullptr),
    m_osFilename(pszFilename),
    m_poPlugin(nullptr)
{
    SetDescription( pszPluginName );
    SetMetadata( papszMD );
    pfnIdentifyEx = IdentifyEx;
    pfnOpenWithDriverArg = OpenEx;
}

/************************************************************************/
/*                       ~PythonPluginDriver()                          */
/************************************************************************/

PythonPluginDriver::~PythonPluginDriver()
{
    if( m_hMutex )
        CPLDestroyMutex(m_hMutex);

    if( m_poPlugin )
    {
        GIL_Holder oHolder(false);
        Py_DecRef(m_poPlugin);
    }
}

/************************************************************************/
/*                         LoadPythonDriver()                           */
/************************************************************************/

static void LoadPythonDriver( const char* pszFilename )
{
    char** papszLines = CSLLoad2( pszFilename, 1000, 1000, nullptr );
    if( papszLines == nullptr )
    {
        return;
    }
    CPLString osPluginName;
    char** papszMD = nullptr;
    bool bAPIOK = false;
    constexpr int CURRENT_API_VERSION = 1;
    for( int i = 0; papszLines[i] != nullptr; i++ )
    {
        const char* pszLine = papszLines[i];
        if( !STARTS_WITH_CI(pszLine, "# gdal: DRIVER_") )
            continue;
        pszLine += strlen("# gdal: DRIVER_");

        const char* pszEqual = strchr(pszLine, '=');
        if( pszEqual == nullptr )
            continue;

        CPLString osKey(pszLine);
        osKey.resize( pszEqual - pszLine);
        osKey.Trim();

        CPLString osValue(pszEqual+1);
        osValue.Trim();

        char chQuote = 0;
        if( !osValue.empty() && (osValue[0] == '"' || osValue[0] == '\'') )
        {
            chQuote = osValue[0];
            osValue = osValue.substr(1);
        }
        if( !osValue.empty() && osValue[osValue.size()-1] == chQuote )
            osValue.resize(osValue.size()-1);
        if( EQUAL(osKey, "NAME") )
        {
            osPluginName = osValue;
        }
        else if( EQUAL(osKey, "SUPPORTED_API_VERSION") )
        {
            const CPLStringList aosTokens(CSLTokenizeString2( osValue, "[, ]", 0));
            for( int j = 0; j < aosTokens.size(); ++j )
            {
                if( atoi(aosTokens[j]) == CURRENT_API_VERSION )
                {
                    bAPIOK = true;
                    break;
                }
            }
        }
        else
        {
            papszMD = CSLSetNameValue(papszMD, osKey.c_str(),  osValue);
        }
    }
    papszMD = CSLSetNameValue(papszMD, "DRIVER_LANGUAGE", "PYTHON");
    CSLDestroy(papszLines);

    if( osPluginName.empty() )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Missing global # gdal: DRIVER_NAME declaration in %s", pszFilename);
    }
    else if( !bAPIOK )
    {
        CPLDebug("GDAL",
                 "Plugin %s does not declare # gdal: DRIVER_SUPPORTED_API_VERSION "
                 "or not at version %d",
                 osPluginName.c_str(),
                 CURRENT_API_VERSION);
    }
    else if( GDALGetDriverByName( osPluginName ) == nullptr )
    {
        GDALDriver* poDriver =
            new PythonPluginDriver(pszFilename, osPluginName, papszMD);
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
    CSLDestroy(papszMD);
}

/************************************************************************/
/*                        AutoLoadPythonDrivers()                       */
/************************************************************************/

/**
 * \brief Auto-load GDAL drivers from Python scripts.
 *
 * This function will automatically load drivers from Python scripts.
 * It searches them first from the directory pointed by the
 * GDAL_PYTHON_DRIVER_PATH configuration option. If not defined, it will
 * use GDAL_DRIVER_PATH. If not defined, it will use the path for
 * drivers hardcoded at build time.
 * Scripts must begin with gdal_ or ogr_ and end up with .py
 *
 * @since GDAL 3.1
 */

void GDALDriverManager::AutoLoadPythonDrivers()
{
    const char* pszPythonDriverPath =
        CPLGetConfigOption("GDAL_PYTHON_DRIVER_PATH", nullptr);
    if( pszPythonDriverPath == nullptr)
    {
        pszPythonDriverPath =
            CPLGetConfigOption("GDAL_DRIVER_PATH", nullptr);
    }
    char **papszSearchPaths = GetSearchPaths(pszPythonDriverPath);

/* -------------------------------------------------------------------- */
/*      Format the ABI version specific subdirectory to look in.        */
/* -------------------------------------------------------------------- */
    CPLString osABIVersion;

    osABIVersion.Printf( "%d.%d", GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR );

/* -------------------------------------------------------------------- */
/*      Scan each directory                                             */
/* -------------------------------------------------------------------- */
    std::vector<CPLString> aosPythonFiles;
    const int nSearchPaths = CSLCount(papszSearchPaths);
    for( int iDir = 0; iDir < nSearchPaths; ++iDir )
    {
        CPLString osABISpecificDir =
            CPLFormFilename( papszSearchPaths[iDir], osABIVersion, nullptr );

        VSIStatBufL sStatBuf;
        if( VSIStatL( osABISpecificDir, &sStatBuf ) != 0 )
            osABISpecificDir = papszSearchPaths[iDir];

        char** papszFiles = CPLReadDir(osABISpecificDir);
        for( int i = 0; papszFiles && papszFiles[i]; i++ )
        {
            if( (STARTS_WITH_CI(papszFiles[i], "gdal_") ||
                STARTS_WITH_CI(papszFiles[i], "ogr_") ) &&
                EQUAL(CPLGetExtension(papszFiles[i]), "py") )
            {
                aosPythonFiles.push_back(
                    CPLFormFilename( osABISpecificDir, papszFiles[i], nullptr ) );
            }
        }
        CSLDestroy(papszFiles);
    }
    CSLDestroy(papszSearchPaths);

    for( const auto& osPythonFile: aosPythonFiles )
    {
        LoadPythonDriver( osPythonFile );
    }
}

/************************************************************************/
/*                        CleanupPythonDrivers()                        */
/************************************************************************/

void GDALDriverManager::CleanupPythonDrivers()
{
    if( gpoGDALPythonDriverModule )
    {
        // On Windows, with pytest, GDALDestroy() can call this after having
        // stopped Python, so do not attempt any Python related action.
        if( Py_IsInitialized() )
        {
            GIL_Holder oHolder(false);
            Py_DecRef(Py_None);
            Py_DecRef(gpoGDALPythonDriverModule);
        }
        Py_None = nullptr;
        gpoGDALPythonDriverModule = nullptr;
    }
}

#endif
