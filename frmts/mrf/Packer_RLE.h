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

#include "Packer.h"

NAMESPACE_MRF_START
// A RLE codec based on use of 0xC3 as marker code
class RLEC3Packer:public Packer {
public:
    virtual int load(storage_manager *src, storage_manager *dst) override;
    virtual int store(storage_manager *src, storage_manager *dst) override;
};
NAMESPACE_MRF_END
