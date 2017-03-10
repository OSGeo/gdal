#ifndef TUT_CONSOLE_REPORTER
#define TUT_CONSOLE_REPORTER

#include <tut.hpp>
#include <cassert>

/**
 * Template Unit Tests Framework for C++.
 * http://tut.dozen.ru
 *
 * @author Vladimir Dyuzhev, Vladimir.Dyuzhev@gmail.com
 */
namespace
{

std::ostream& operator<<(std::ostream& os, const tut::test_result& tr)
{
    switch(tr.result)
    {
    case tut::test_result::ok:
        os << '.';
        break;
    case tut::test_result::fail:
        os << '[' << tr.test << "=F]";
        break;
    case tut::test_result::ex_ctor:
        os << '[' << tr.test << "=C]";
        break;
    case tut::test_result::ex:
        os << '[' << tr.test << "=X]";
        break;
    case tut::test_result::warn:
        os << '[' << tr.test << "=W]";
        break;
    case tut::test_result::term:
        os << '[' << tr.test << "=T]";
        break;
    case tut::test_result::rethrown:
        os << '[' << tr.test << "=P]";
        break;
    case tut::test_result::dummy:
        assert(!"Should never be called");
    }

    return os;
}

} // end of namespace

namespace tut
{

/**
 * Default TUT callback handler.
 */
class console_reporter : public tut::callback
{
    std::string current_group;
    typedef std::vector<tut::test_result> not_passed_list;
    not_passed_list not_passed;
    std::ostream& os;

public:

    int ok_count;
    int exceptions_count;
    int failures_count;
    int terminations_count;
    int warnings_count;

    console_reporter()
        : os(std::cout)
    {
        init();
    }

    console_reporter(std::ostream& out)
        : os(out)
    {
        init();
    }

    void run_started()
    {
        init();
    }

    void test_completed(const tut::test_result& tr)
    {
        if (tr.group != current_group)
        {
            os << std::endl << tr.group << ": " << std::flush;
            current_group = tr.group;
        }

        os << tr << std::flush;

        // update global statistics
        switch (tr.result) {
            case test_result::ok:
                ok_count++;
                break;
            case test_result::fail:
            case test_result::rethrown:
                failures_count++;
                break;
            case test_result::ex:
            case test_result::ex_ctor:
                exceptions_count++;
                break;
            case test_result::warn:
                warnings_count++;
                break;
            case test_result::term:
                terminations_count++;
                break;
            case tut::test_result::dummy:
                assert(!"Should never be called");
        } // switch

        if (tr.result != tut::test_result::ok)
        {
            not_passed.push_back(tr);
        }
    }

    void run_completed()
    {
        os << std::endl;

        if (not_passed.size() > 0)
        {
            not_passed_list::const_iterator i = not_passed.begin();
            while (i != not_passed.end())
            {
                tut::test_result tr = *i;

                os << std::endl;

                os << "---> " << "group: " << tr.group
                << ", test: test<" << tr.test << ">"
                << (!tr.name.empty() ? (std::string(" : ") + tr.name) : std::string())
                << std::endl;

#if defined(TUT_USE_POSIX)
                if(tr.pid != getpid())
                {
                    os << "     child pid: " << tr.pid << std::endl;
                }
#endif
                os << "     problem: ";
                switch(tr.result)
                {
                case test_result::rethrown:
                    os << "assertion failed in child" << std::endl;
                    break;
                case test_result::fail:
                    os << "assertion failed" << std::endl;
                    break;
                case test_result::ex:
                case test_result::ex_ctor:
                    os << "unexpected exception" << std::endl;
                    if( tr.exception_typeid != "" )
                    {
                        os << "     exception typeid: "
                        << tr.exception_typeid << std::endl;
                    }
                    break;
                case test_result::term:
                    os << "would be terminated" << std::endl;
                    break;
                case test_result::warn:
                    os << "test passed, but cleanup code (destructor) raised"
                        " an exception" << std::endl;
                    break;
                default:
                    break;
                }

                if (!tr.message.empty())
                {
                    if (tr.result == test_result::fail)
                    {
                        os << "     failed assertion: \"" << tr.message << "\""
                            << std::endl;
                    }
                    else
                    {
                        os << "     message: \"" << tr.message << "\""
                            << std::endl;
                    }
                }

                ++i;
            }
        }

        os << std::endl;

        os << "tests summary:";
        if (terminations_count > 0)
        {
            os << " terminations:" << terminations_count;
        }
        if (exceptions_count > 0)
        {
            os << " exceptions:" << exceptions_count;
        }
        if (failures_count > 0)
        {
            os << " failures:" << failures_count;
        }
        if (warnings_count > 0)
        {
            os << " warnings:" << warnings_count;
        }
        os << " ok:" << ok_count;
        os << std::endl;
    }

    bool all_ok() const
    {
        return not_passed.empty();
    }

private:

    void init()
    {
        ok_count = 0;
        exceptions_count = 0;
        failures_count = 0;
        terminations_count = 0;
        warnings_count = 0;
        not_passed.clear();
    }
};

}

#endif
