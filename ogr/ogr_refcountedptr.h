/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Smart pointer around a class that has built-in reference counting.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_REFCOUNTEDPTR_INCLUDED
#define OGR_REFCOUNTEDPTR_INCLUDED

/*! @cond Doxygen_Suppress */

#include <cstddef>
#include <utility>

/** Base (non-instantiable) class for OGRRefCountedPtr.
 *
 * Only intended to be used as the base class for OGRRefCountedPtr.
 * Done that way so its constructor is protected and force specializations
 * of redefining a public one (calling it)
 */
template <class T> struct OGRRefCountedPtrBase
{
  public:
    /** Destructor.
     *
     * Release the raw pointer, that is decrease its reference count and delete
     * the object when it reaches zero.
     */
    inline ~OGRRefCountedPtrBase()
    {
        reset(nullptr);
    }

    /** Copy constructor.
     *
     * Increases the reference count
     */
    inline OGRRefCountedPtrBase(const OGRRefCountedPtrBase &other)
        : OGRRefCountedPtrBase(other.m_poRawPtr, true)
    {
    }

    /** Copy constructor
     *
     * Set the raw pointer to the one used by other, and increase the reference
     * count.
     */
    // cppcheck-suppress operatorEqVarError
    inline OGRRefCountedPtrBase &operator=(const OGRRefCountedPtrBase &other)
    {
        if (this != &other)
        {
            reset(other.m_poRawPtr);
        }
        return *this;
    }

    /** Move constructor
     *
     * Borrows the raw pointer managed by other, without changing its reference
     * count, and set the managed raw pointer of other to null.
     */
    inline OGRRefCountedPtrBase(OGRRefCountedPtrBase &&other)
    {
        std::swap(m_poRawPtr, other.m_poRawPtr);
    }

    /** Move assignment operator.
     *
     * Release the current managed raw pointer and borrow the
     * one from other.
     * Does not change the reference count of the borrowed raw pointer.
     */
    inline OGRRefCountedPtrBase &operator=(OGRRefCountedPtrBase &&other)
    {
        reset(nullptr);
        std::swap(m_poRawPtr, other.m_poRawPtr);
        return *this;
    }

    /** Reset the managed raw pointer.
     *
     * Release the current managed raw pointer and manages a new one.
     * By default, increases the reference count of the new raw pointer (when
     * not null).
     */
    inline void reset(T *poRawPtr = nullptr, bool add_ref = true)
    {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        if (m_poRawPtr)
            m_poRawPtr->Release();
        m_poRawPtr = poRawPtr;
        if (m_poRawPtr && add_ref)
            m_poRawPtr->Reference();
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    }

    /** Returns the raw pointer without changing its reference count */
    inline T *get() const
    {
        return m_poRawPtr;
    }

    /** Returns a reference to the raw pointer without changing its reference
     * count.
     *
     * Must be only called when get() != nullptr.
     */
    inline T &operator*() const
    {
        return *m_poRawPtr;
    }

    /** Forwards the access to a member or a call to a method of the raw
     * pointer.
     */
    inline T *operator->() const
    {
        return m_poRawPtr;
    }

    /** Returns whether the raw pointer is null. */
    inline explicit operator bool() const
    {
        return m_poRawPtr != nullptr;
    }

  protected:
    inline explicit OGRRefCountedPtrBase(T *poRawPtr = nullptr,
                                         bool add_ref = true)
        : m_poRawPtr(poRawPtr)
    {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        if (m_poRawPtr && add_ref)
            m_poRawPtr->Reference();
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    }

  private:
    T *m_poRawPtr{};
};

/** Smart pointer around a class that has built-in reference counting.
 *
 * It uses the Reference() and Release() methods of the wrapped class for
 * reference counting. The reference count is increased when assigning a raw
 * pointer to the smart pointer, and decreased when releasing it.
 * Somewhat similar to https://www.boost.org/doc/libs/latest/libs/smart_ptr/doc/html/smart_ptr.html#intrusive_ptr
 *
 * Only meant for T = OGRFeatureDefn and OGRSpatialReference
 */
template <class T> struct OGRRefCountedPtr : public OGRRefCountedPtrBase<T>
{
};

template <class T>
inline bool operator==(const OGRRefCountedPtr<T> &lhs, std::nullptr_t)
{
    return lhs.get() == nullptr;
}

template <class T>
inline bool operator==(std::nullptr_t, const OGRRefCountedPtr<T> &rhs)
{
    return rhs.get() == nullptr;
}

template <class T>
inline bool operator!=(const OGRRefCountedPtr<T> &lhs, std::nullptr_t)
{
    return lhs.get() != nullptr;
}

template <class T>
inline bool operator!=(std::nullptr_t, const OGRRefCountedPtr<T> &rhs)
{
    return rhs.get() != nullptr;
}

/*! @endcond */

#endif /* OGR_REFCOUNTEDPTR_INCLUDED */
