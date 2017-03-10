#ifndef TUT_EXCEPTION_H_GUARD
#define TUT_EXCEPTION_H_GUARD

#include <stdexcept>
#include "tut_result.hpp"

namespace tut
{

/**
 * The base for all TUT exceptions.
 */
struct tut_error : public std::exception
{
    tut_error(const std::string& msg)
        : err_msg(msg)
    {
    }

    virtual test_result::result_type result() const
    {
        return test_result::ex;
    }

    const char* what() const throw()
    {
        return err_msg.c_str();
    }

    ~tut_error() throw()
    {
    }

private:
    tut_error& operator=(const tut_error&);
    std::string err_msg;
};

/**
 * Group not found exception.
 */
struct no_such_group : public tut_error
{
    no_such_group(const std::string& grp)
        : tut_error(grp)
    {
    }

    ~no_such_group() throw()
    {
    }
};

/**
 * Internal exception to be throwed when
 * test constructor has failed.
 */
struct bad_ctor : public tut_error
{
    bad_ctor(const std::string& msg)
        : tut_error(msg)
    {
    }

    test_result::result_type result() const
    {
        return test_result::ex_ctor;
    }

    ~bad_ctor() throw()
    {
    }
};

/**
 * Exception to be throwed when ensure() fails or fail() called.
 */
struct failure : public tut_error
{
    failure(const std::string& msg)
        : tut_error(msg)
    {
    }

    test_result::result_type result() const
    {
        return test_result::fail;
    }

    ~failure() throw()
    {
    }
};

/**
 * Exception to be throwed when test desctructor throwed an exception.
 */
struct warning : public tut_error
{
    warning(const std::string& msg)
        : tut_error(msg)
    {
    }

    test_result::result_type result() const
    {
        return test_result::warn;
    }

    ~warning() throw()
    {
    }
};

/**
 * Exception to be throwed when test issued SEH (Win32)
 */
struct seh : public tut_error
{
    seh(const std::string& msg)
        : tut_error(msg)
    {
    }

    virtual test_result::result_type result() const
    {
        return test_result::term;
    }

    ~seh() throw()
    {
    }
};

/**
 * Exception to be throwed when child processes fail.
 */
struct rethrown : public failure
{
    explicit rethrown(const test_result &result)
        : failure(result.message), tr(result)
    {
    }

    virtual test_result::result_type result() const
    {
        return test_result::rethrown;
    }

    ~rethrown() throw()
    {
    }

    const test_result tr;
};

}

#endif
