/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Parts of OGRLayer dealing with Arrow C interface
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRLAYERARROW_H_DEFINED
#define OGRLAYERARROW_H_DEFINED

#include "cpl_port.h"

#include <map>
#include <string>

#include "ogr_recordbatch.h"

constexpr const char *ARROW_EXTENSION_NAME_KEY = "ARROW:extension:name";
constexpr const char *ARROW_EXTENSION_METADATA_KEY = "ARROW:extension:metadata";
constexpr const char *EXTENSION_NAME_OGC_WKB = "ogc.wkb";
constexpr const char *EXTENSION_NAME_GEOARROW_WKB = "geoarrow.wkb";
constexpr const char *EXTENSION_NAME_ARROW_JSON = "arrow.json";

// GetArrowStream(GAS) options
constexpr const char *GAS_OPT_DATETIME_AS_STRING = "DATETIME_AS_STRING";

std::map<std::string, std::string>
    CPL_DLL OGRParseArrowMetadata(const char *pabyMetadata);

bool CPL_DLL OGRCloneArrowArray(const struct ArrowSchema *schema,
                                const struct ArrowArray *array,
                                struct ArrowArray *out_array);

bool CPL_DLL OGRCloneArrowSchema(const struct ArrowSchema *schema,
                                 struct ArrowSchema *out_schema);

/** C++ wrapper on top of ArrowArrayStream */
class OGRArrowArrayStream
{
  public:
    /** Constructor: instantiate an empty ArrowArrayStream  */
    inline OGRArrowArrayStream()
    {
        memset(&m_stream, 0, sizeof(m_stream));
    }

    /** Destructor: call release() on the ArrowArrayStream if not already done */
    inline ~OGRArrowArrayStream()
    {
        clear();
    }

    /** Call release() on the ArrowArrayStream if not already done */
    // cppcheck-suppress functionStatic
    inline void clear()
    {
        if (m_stream.release)
        {
            m_stream.release(&m_stream);
            m_stream.release = nullptr;
        }
    }

    /** Return the raw ArrowArrayStream* */
    inline ArrowArrayStream *get()
    {
        return &m_stream;
    }

    /** Get the schema */
    // cppcheck-suppress functionStatic
    inline int get_schema(struct ArrowSchema *schema)
    {
        return m_stream.get_schema(&m_stream, schema);
    }

    /** Get the next ArrowArray batch */
    // cppcheck-suppress functionStatic
    inline int get_next(struct ArrowArray *array)
    {
        return m_stream.get_next(&m_stream, array);
    }

    /** Move assignment operator */
    inline OGRArrowArrayStream &operator=(OGRArrowArrayStream &&other)
    {
        if (this != &other)
        {
            clear();
            memcpy(&m_stream, &(other.m_stream), sizeof(m_stream));
            // Reset other content, in particular its "release" member
            // as per https://arrow.apache.org/docs/format/CDataInterface.html#moving-an-array
            memset(&(other.m_stream), 0, sizeof(m_stream));
        }
        return *this;
    }

  private:
    struct ArrowArrayStream m_stream
    {
    };

    OGRArrowArrayStream(const OGRArrowArrayStream &) = delete;
    OGRArrowArrayStream(OGRArrowArrayStream &&) = delete;
    OGRArrowArrayStream &operator=(const OGRArrowArrayStream &) = delete;
};

#endif  // OGRLAYERARROW_H_DEFINED
