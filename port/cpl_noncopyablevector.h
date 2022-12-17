/******************************************************************************
 *
 * Project:  CPL
 * Purpose:  Implementation of a std::vector<> without copy capabilities
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef CPL_NONCOPYABLEVECOTR_H_INCLUDED
#define CPL_NONCOPYABLEVECOTR_H_INCLUDED

#include <vector>

namespace cpl
{

/** Derived class of std::vector<> without copy capabilities */
template <class T> struct NonCopyableVector : public std::vector<T>
{
    /** Constructor
     * @param siz Initial size of vector.
     */
    explicit inline NonCopyableVector(size_t siz = 0) : std::vector<T>(siz)
    {
    }

    /** Move constructor */
    NonCopyableVector(NonCopyableVector &&) = default;

    /** Move assignment operator */
    NonCopyableVector &operator=(NonCopyableVector &&) = default;

  private:
    NonCopyableVector(const NonCopyableVector &) = delete;
    NonCopyableVector &operator=(const NonCopyableVector &) = delete;
};

}  // namespace cpl

#endif  // CPL_NONCOPYABLEVECOTR_H_INCLUDED
