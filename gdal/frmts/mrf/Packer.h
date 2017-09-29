/*
Copyright 2016-2017 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Contributors:  Lucian Plesea
*/

#if !defined(PACKER_H)
#define PACKER_H
#include <cstring>
#include "marfa.h"

NAMESPACE_MRF_START
typedef struct storage_manager {
    char *buffer;
    size_t size;
} storage_manager;
   
// A base class that provides import and export functions based on storage managers
// Default implementation is a straight copy
class Packer {
public:
    virtual ~Packer() {}
    virtual int load(storage_manager *src, storage_manager *dst)
    {
        if (dst->size < src->size)
            return false;
        std::memcpy(dst->buffer, src->buffer, src->size);
        dst->size -= src->size; // Adjust the destination size
        return true;
    }

    virtual int store(storage_manager *src, storage_manager *dst)
    {
        return load(src, dst);
    }
};
NAMESPACE_MRF_END
#endif
