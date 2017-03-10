#ifndef TUT_H_GUARD
#define TUT_H_GUARD

#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <typeinfo>

#if defined(linux)
#define TUT_USE_POSIX
#endif

#include "tut_exception.hpp"
#include "tut_result.hpp"
#include "tut_posix.hpp"
#include "tut_assert.hpp"
#include "tut_runner.hpp"

#if defined(TUT_USE_SEH)
#include <windows.h>
#include <winbase.h>
#endif

/**
 * Template Unit Tests Framework for C++.
 * http://tut.dozen.ru
 *
 * @author Vladimir Dyuzhev, Vladimir.Dyuzhev@gmail.com
 */
namespace tut
{

/**
 * Test object. Contains data test run upon and default test method
 * implementation. Inherited from Data to allow tests to
 * access test data as members.
 */
template <class Data>
class test_object : public Data, public test_object_posix
{
public:

    /**
     * Default constructor
     */
    test_object()
    {
    }

    void set_test_name(const std::string& current_test_name)
    {
        current_test_name_ = current_test_name;
    }

    const std::string& get_test_name() const
    {
        return current_test_name_;
    }

    void set_test_id(int current_test_id)
    {
        current_test_id_ = current_test_id;
    }

    int get_test_id() const
    {
        return current_test_id_;
    }

    /**
     * Default do-nothing test.
     */
    template <int n>
    void test()
    {
        called_method_was_a_dummy_test_ = true;
    }

    /**
     * The flag is set to true by default (dummy) test.
     * Used to detect usused test numbers and avoid unnecessary
     * test object creation which may be time-consuming depending
     * on operations described in Data::Data() and Data::~Data().
     * TODO: replace with throwing special exception from default test.
     */
    bool called_method_was_a_dummy_test_;

private:
    int             current_test_id_;
    std::string     current_test_name_;

    test_object(test_object const&); // = delete
    test_object& operator=(test_object const&); // = delete
};


/**
 * Walks through test tree and stores address of each
 * test method in group. Instantiation stops at 0.
 */
template <class Test, class Group, int n>
struct tests_registerer
{
    static void reg(Group& group)
    {
        group.reg(n, &Test::template test<n>);
        tests_registerer<Test, Group, n - 1>::reg(group);
    }
};

template <class Test, class Group>
struct tests_registerer<Test, Group, 0>
{
    static void reg(Group&)
    {
    }
};

/**
 * Test group; used to recreate test object instance for
 * each new test since we have to have reinitialized
 * Data base class.
 */
template <class Data, int MaxTestsInGroup = 50>
class test_group : public group_base, public test_group_posix
{
    const char* name_;

    typedef void (test_object<Data>::*testmethod)();
    typedef std::map<int, testmethod> tests;
    typedef typename tests::iterator tests_iterator;
    typedef typename tests::const_iterator tests_const_iterator;
    typedef typename tests::const_reverse_iterator
    tests_const_reverse_iterator;
    typedef typename tests::size_type size_type;

    tests tests_;
    tests_iterator current_test_;

	enum seh_result
	{
		SEH_OK,
		SEH_CTOR,
		SEH_TEST,
		SEH_DUMMY
	};

    /**
     * Exception-in-destructor-safe smart-pointer class.
     */
    template <class T>
    class safe_holder
    {
        T* p_;
        bool permit_throw_in_dtor;

        safe_holder(const safe_holder&);
        safe_holder& operator=(const safe_holder&);

    public:
        safe_holder()
            : p_(0),
              permit_throw_in_dtor(false)
        {
        }

        ~safe_holder()
        {
            release();
        }

        T* operator->() const
        {
            return p_;
        }

        T* get() const
        {
            return p_;
        }

        /**
         * Tell ptr it can throw from destructor. Right way is to
         * use std::uncaught_exception(), but some compilers lack
         * correct implementation of the function.
         */
        void permit_throw()
        {
            permit_throw_in_dtor = true;
        }

        /**
         * Specially treats exceptions in test object destructor;
         * if test itself failed, exceptions in destructor
         * are ignored; if test was successful and destructor failed,
         * warning exception throwed.
         */
        void release()
        {
            try
            {
                if (delete_obj() == false)
                {
                    throw warning("destructor of test object raised"
                        " an SEH exception");
                }
            }
            catch (const std::exception& ex)
            {
                if (permit_throw_in_dtor)
                {
                    std::string msg = "destructor of test object raised"
                        " exception: ";
                    msg += ex.what();
                    throw warning(msg);
                }
            }
            catch( ... )
            {
                if (permit_throw_in_dtor)
                {
                    throw warning("destructor of test object raised an"
                        " exception");
                }
            }
        }

        /**
         * Re-init holder to get brand new object.
         */
        void reset()
        {
            release();
            permit_throw_in_dtor = false;
            p_ = new T();
        }

        bool delete_obj()
        {
#if defined(TUT_USE_SEH)
            __try
            {
#endif
                T* p = p_;
                p_ = 0;
                delete p;
#if defined(TUT_USE_SEH)
            }
            __except(handle_seh_(::GetExceptionCode()))
            {
                if (permit_throw_in_dtor)
                {
                    return false;
                }
            }
#endif
            return true;
        }
    };

public:

    typedef test_object<Data> object;

    /**
     * Creates and registers test group with specified name.
     */
    test_group(const char* name)
        : name_(name)
    {
        // register itself
        runner.get().register_group(name_,this);

        // register all tests
        tests_registerer<object, test_group, MaxTestsInGroup>::reg(*this);
    }

    /**
     * This constructor is used in self-test run only.
     */
    test_group(const char* name, test_runner& another_runner)
        : name_(name)
    {
        // register itself
        another_runner.register_group(name_, this);

        // register all tests
        tests_registerer<test_object<Data>, test_group,
            MaxTestsInGroup>::reg(*this);
    };

    /**
     * Registers test method under given number.
     */
    void reg(int n, testmethod tm)
    {
        tests_[n] = tm;
    }

    /**
     * Reset test position before first test.
     */
    void rewind()
    {
        current_test_ = tests_.begin();
    }

    /**
     * Runs next test.
     */
    bool run_next(test_result &tr)
    {
        if (current_test_ == tests_.end())
        {
            return false;
        }

        // find next user-specialized test
        safe_holder<object> obj;
        while (current_test_ != tests_.end())
        {
            tests_iterator current_test = current_test_++;

            if(run_test_(current_test, obj, tr) && tr.result != test_result::dummy)
            {
                return true;
            }
        }

        return false;
    }

    /**
     * Runs one test by position.
     */
    bool run_test(int n, test_result &tr)
    {
        if (tests_.rbegin() == tests_.rend() ||
            tests_.rbegin()->first < n)
        {
            return false;
        }

        // withing scope; check if given test exists
        tests_iterator ti = tests_.find(n);
        if (ti == tests_.end())
        {
            return false;
        }

        safe_holder<object> obj;
        return run_test_(ti, obj, tr);
    }

    /**
     * VC allows only one exception handling type per function,
     * so I have to split the method.
     */
    bool run_test_(const tests_iterator& ti, safe_holder<object>& obj, test_result &tr)
    {
        std::string current_test_name;

        tr = test_result(name_, ti->first, current_test_name, test_result::ok);

        try
        {
            switch (run_test_seh_(ti->second, obj, current_test_name, ti->first))
			{
				case SEH_CTOR:
					throw bad_ctor("seh");
					break;

				case SEH_TEST:
					throw seh("seh");
					break;

				case SEH_DUMMY:
					tr.result = test_result::dummy;
					break;

				case SEH_OK:
					// ok
					break;
            }
        }
        catch (const rethrown& ex)
        {
            tr = ex.tr;
            tr.result = test_result::rethrown;
        }
        catch (const tut_error& ex)
        {
            tr.result = ex.result();
            tr.exception_typeid = typeid(ex).name();
            tr.message = ex.what();
        }
        catch (const std::exception& ex)
        {
            tr.result = test_result::ex;
            tr.exception_typeid = typeid(ex).name();
            tr.message = ex.what();
        }
        catch (...)
        {
            // test failed with unknown exception
            tr.result = test_result::ex;
        }

        if (obj.get())
        {
            tr.name = obj->get_test_name();

            // try to report to parent, if exists
            send_result_(obj.get(), tr);
        }
        else
        {
            tr.name = current_test_name;
        }

        return true;
    }

    /**
     * Runs one under SEH if platform supports it.
     */
    seh_result run_test_seh_(testmethod tm, safe_holder<object>& obj,
                             std::string& current_test_name, int current_test_id)
    {
#if defined(TUT_USE_SEH)
        __try
        {
#endif
        if (obj.get() == 0)
        {
            reset_holder_(obj);
        }

        obj->called_method_was_a_dummy_test_ = false;

#if defined(TUT_USE_SEH)

            __try
            {
#endif
                obj.get()->set_test_id(current_test_id);
                (obj.get()->*tm)();
#if defined(TUT_USE_SEH)
            }
            __except(handle_seh_(::GetExceptionCode()))
            {
                current_test_name = obj->get_test_name();
                return SEH_TEST;
            }
#endif

        if (obj->called_method_was_a_dummy_test_)
        {
            // do not call obj.release(); reuse object
            return SEH_DUMMY;
        }

        current_test_name = obj->get_test_name();
        obj.permit_throw();
        obj.release();
#if defined(TUT_USE_SEH)
        }
        __except(handle_seh_(::GetExceptionCode()))
        {
			return SEH_CTOR;
        }
#endif
        return SEH_OK;
    }

    void reset_holder_(safe_holder<object>& obj)
    {
        try
        {
            obj.reset();
        }
        catch (const std::exception& ex)
        {
            throw bad_ctor(ex.what());
        }
        catch (...)
        {
            throw bad_ctor("test constructor has generated an exception;"
                " group execution is terminated");
        }
    }
};

#if defined(TUT_USE_SEH)
/**
 * Decides should we execute handler or ignore SE.
 */
inline int handle_seh_(DWORD excode)
{
    switch(excode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_GUARD_PAGE:
    case EXCEPTION_INVALID_HANDLE:
        return EXCEPTION_EXECUTE_HANDLER;
    };

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
}

#endif

