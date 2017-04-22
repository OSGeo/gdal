#ifndef TUT_RUNNER_H_GUARD
#define TUT_RUNNER_H_GUARD

#include <string>
#include <vector>
#include <set>
#include "tut_exception.hpp"

namespace tut
{

/**
 * Interface.
 * Test group operations.
 */
struct group_base
{
    virtual ~group_base()
    {
    }

    // execute tests iteratively
    virtual void rewind() = 0;
    virtual bool run_next(test_result &) = 0;

    // execute one test
    virtual bool run_test(int n, test_result &tr) = 0;
};


/**
 * Test runner callback interface.
 * Can be implemented by caller to update
 * tests results in real-time. User can implement
 * any of callback methods, and leave unused
 * in default implementation.
 */
struct callback
{
    /**
     * Default constructor.
     */
    callback()
    {
    }

    /**
     * Virtual destructor is a must for subclassed types.
     */
    virtual ~callback()
    {
    }

    /**
     * Called when new test run started.
     */
    virtual void run_started()
    {
    }

    /**
     * Called when a group started
     * @param name Name of the group
     */
    virtual void group_started(const std::string& /*name*/)
    {
    }

    /**
     * Called when a test finished.
     * @param tr Test results.
     */
    virtual void test_completed(const test_result& /*tr*/)
    {
    }

    /**
     * Called when a group is completed
     * @param name Name of the group
     */
    virtual void group_completed(const std::string& /*name*/)
    {
    }

    /**
     * Called when all tests in run completed.
     */
    virtual void run_completed()
    {
    }
private:
    callback(const callback &);
    void operator=(const callback&);
};

/**
 * Typedef for runner::list_groups()
 */
typedef std::vector<std::string> groupnames;
typedef std::set<callback*> callbacks;

/**
 * Test runner.
 */
class test_runner
{

public:

    /**
     * Constructor
     */
    test_runner()
    {
    }

    /**
     * Stores another group for getting by name.
     */
    void register_group(const std::string& name, group_base* gr)
    {
        if (gr == 0)
        {
            throw tut_error("group shall be non-null");
        }

        if (groups_.find(name) != groups_.end())
        {
            std::string msg("attempt to add already existent group " + name);
            // this exception terminates application so we use cerr also
            // TODO: should this message appear in stream?
            std::cerr << msg << std::endl;
            throw tut_error(msg);
        }

        groups_.insert( std::make_pair(name, gr) );
    }

    void set_callback(callback *cb)
    {
        clear_callbacks();
        insert_callback(cb);
    }

    /**
     * Stores callback object.
     */
    void insert_callback(callback* cb)
    {
        if(cb != NULL)
        {
            callbacks_.insert(cb);
        }
    }

    void erase_callback(callback* cb)
    {
        callbacks_.erase(cb);
    }

    void clear_callbacks()
    {
        callbacks_.clear();
    }

    /**
     * Returns callback list.
     */
    const callbacks &get_callbacks() const
    {
        return callbacks_;
    }

    void set_callbacks(const callbacks &cb)
    {
        callbacks_ = cb;
    }

    /**
     * Returns list of known test groups.
     */
    const groupnames list_groups() const
    {
        groupnames ret;
        const_iterator i = groups_.begin();
        const_iterator e = groups_.end();
        while (i != e)
        {
            ret.push_back(i->first);
            ++i;
        }
        return ret;
    }

    /**
     * Runs all tests in all groups.
     * @param callback Callback object if exists; null otherwise
     */
    void run_tests() const
    {
        cb_run_started_();

        const_iterator i = groups_.begin();
        const_iterator e = groups_.end();
        while (i != e)
        {
            cb_group_started_(i->first);
            run_all_tests_in_group_(i);
            cb_group_completed_(i->first);

            ++i;
        }

        cb_run_completed_();
    }

    /**
     * Runs all tests in specified group.
     */
    void run_tests(const std::string& group_name) const
    {
        cb_run_started_();

        const_iterator i = groups_.find(group_name);
        if (i == groups_.end())
        {
            cb_run_completed_();
            throw no_such_group(group_name);
        }

        cb_group_started_(group_name);
        run_all_tests_in_group_(i);
        cb_group_completed_(group_name);
        cb_run_completed_();
    }

    /**
     * Runs one test in specified group.
     */
    bool run_test(const std::string& group_name, int n, test_result &tr) const
    {
        cb_run_started_();

        const_iterator i = groups_.find(group_name);
        if (i == groups_.end())
        {
            cb_run_completed_();
            throw no_such_group(group_name);
        }

        cb_group_started_(group_name);

        bool t = i->second->run_test(n, tr);

        if(t && tr.result != test_result::dummy)
        {
            cb_test_completed_(tr);
        }

        cb_group_completed_(group_name);
        cb_run_completed_();

        return t;
    }

protected:

    typedef std::map<std::string, group_base*> groups;
    typedef groups::iterator iterator;
    typedef groups::const_iterator const_iterator;
    groups groups_;

    callbacks callbacks_;

private:
    friend class restartable_wrapper;

    void cb_run_started_() const
    {
        for(callbacks::const_iterator i = callbacks_.begin(); i != callbacks_.end(); ++i)
        {
            (*i)->run_started();
        }
    }

    void cb_run_completed_() const
    {
        for(callbacks::const_iterator i = callbacks_.begin(); i != callbacks_.end(); ++i)
        {
            (*i)->run_completed();
        }
    }

    void cb_group_started_(const std::string &group_name) const
    {
        for(callbacks::const_iterator i = callbacks_.begin(); i != callbacks_.end(); ++i)
        {
            (*i)->group_started(group_name);
        }
    }

    void cb_group_completed_(const std::string &group_name) const
    {
        for(callbacks::const_iterator i = callbacks_.begin(); i != callbacks_.end(); ++i)
        {
            (*i)->group_completed(group_name);
        }
    }

    void cb_test_completed_(const test_result &tr) const
    {
        for(callbacks::const_iterator i = callbacks_.begin(); i != callbacks_.end(); ++i)
        {
            (*i)->test_completed(tr);
        }
    }

    void run_all_tests_in_group_(const_iterator i) const
    {
        i->second->rewind();

        test_result tr;
        while(i->second->run_next(tr))
        {
            if(tr.result != test_result::dummy)
            {
                cb_test_completed_(tr);
            }

            if (tr.result == test_result::ex_ctor)
            {
                // test object ctor failed, skip whole group
                break;
            }
        }
    }
};

/**
 * Singleton for test_runner implementation.
 * Instance with name runner_singleton shall be implemented
 * by user.
 */
class test_runner_singleton
{
public:

    static test_runner& get()
    {
        static test_runner tr;
        return tr;
    }
};

extern test_runner_singleton runner;

}

#endif
