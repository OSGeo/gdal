#ifndef TUT_RESULT_H_GUARD
#define TUT_RESULT_H_GUARD

#include <string>

namespace tut
{

#if defined(TUT_USE_POSIX)
struct test_result_posix
{
    test_result_posix()
        : pid(getpid())
    {
    }

    pid_t pid;
};
#else
struct test_result_posix
{
};
#endif

/**
 * Return type of runned test/test group.
 *
 * For test: contains result of test and, possible, message
 * for failure or exception.
 */
struct test_result : public test_result_posix
{
    /**
     * Test group name.
     */
    std::string group;

    /**
     * Test number in group.
     */
    int test;

    /**
     * Test name (optional)
     */
    std::string name;

    /**
     * ok - test finished successfully
     * fail - test failed with ensure() or fail() methods
     * ex - test throwed an exceptions
     * warn - test finished successfully, but test destructor throwed
     * term - test forced test application to terminate abnormally
     */
    enum result_type
    {
        ok,
        fail,
        ex,
        warn,
        term,
        ex_ctor,
        rethrown,
        dummy
    };

    result_type result;

    /**
     * Exception message for failed test.
     */
    std::string message;
    std::string exception_typeid;

    /**
     * Default constructor.
     */
    test_result()
        : test(0),
          result(ok)
    {
    }

    /**
     * Constructor.
     */
    test_result(const std::string& grp, int pos,
                const std::string& test_name, result_type res)
        : group(grp),
          test(pos),
          name(test_name),
          result(res)
    {
    }

    /**
     * Constructor with exception.
     */
    test_result(const std::string& grp,int pos,
                const std::string& test_name, result_type res,
                const std::exception& ex)
        : group(grp),
          test(pos),
          name(test_name),
          result(res),
          message(ex.what()),
          exception_typeid(typeid(ex).name())
    {
    }

    /** Constructor with typeid.
    */
    test_result(const std::string& grp,int pos,
                const std::string& test_name, result_type res,
                const std::string& ex_typeid,
                const std::string& msg)
        : group(grp),
          test(pos),
          name(test_name),
          result(res),
          message(msg),
          exception_typeid(ex_typeid)
    {
    }
};

}

#endif
