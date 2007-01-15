#ifndef TUT_H_GUARD
#define TUT_H_GUARD

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

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
   * Exception to be throwed when attempted to execute 
   * missed test by number.
   */
  struct no_such_test : public std::logic_error
  {
    no_such_test() : std::logic_error("no such test"){};
  };

  /**
   * No such test and passed test number is higher than
   * any test number in current group. Used in one-by-one
   * test running when upper bound is not known.
   */
  struct beyond_last_test : public no_such_test
  {
    beyond_last_test(){};
  };

  /**
   * Group not found exception.
   */
  struct no_such_group : public std::logic_error
  {
    no_such_group(const std::string& grp) : 
      std::logic_error(grp){};
  };

  /**
   * Internal exception to be throwed when 
   * no more tests left in group or journal.
   */
  struct no_more_tests
  {
    no_more_tests(){};
  };

  /**
   * Internal exception to be throwed when 
   * test constructor has failed.
   */
  struct bad_ctor : public std::logic_error
  {
    bad_ctor(const std::string& msg) : 
      std::logic_error(msg){};
  };

  /**
   * Exception to be throwed when ensure() fails or fail() called.
   */
  class failure : public std::logic_error
  {
    public:
      failure(const std::string& msg) : std::logic_error(msg){};
  };

  /**
   * Exception to be throwed when test desctructor throwed an exception.
   */
  class warning : public std::logic_error
  {
    public:
      warning(const std::string& msg) : std::logic_error(msg){};
  };

  /**
   * Exception to be throwed when test issued SEH (Win32)
   */
  class seh : public std::logic_error
  {
    public:
      seh(const std::string& msg) : std::logic_error(msg){};
  };

  /**
   * Return type of runned test/test group.
   *
   * For test: contains result of test and, possible, message
   * for failure or exception.
   */
  struct test_result
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
    typedef enum { ok, fail, ex, warn, term, ex_ctor } result_type;
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
      : test(0),result(ok)
    {
    }

    /**
     * Constructor.
     */
    test_result(const std::string& grp, int pos, 
    	const std::string& test_name, result_type res)
      : group(grp), test(pos), name(test_name), result(res)
    {
    }

    /**
     * Constructor with exception.
     */
    test_result(const std::string& grp,int pos, 
    	const std::string& test_name, result_type res,
        const std::exception& ex)
      : group(grp), test(pos), name(test_name), result(res),
        message(ex.what()),exception_typeid(typeid(ex).name())
    {
    }
  };

  /**
   * Interface.
   * Test group operations.
   */
  struct group_base
  {
    virtual ~group_base(){};

    // execute tests iteratively
    virtual void rewind() = 0;
    virtual test_result run_next() = 0;

    // execute one test
    virtual test_result run_test(int n) = 0;
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
     * Virtual destructor is a must for subclassed types.
     */
    virtual ~callback(){};

    /**
     * Called when new test run started.
     */
    virtual void run_started(){};

    /**
     * Called when a group started
     * @param name Name of the group
     */
    virtual void group_started(const std::string& /*name*/){};

    /**
     * Called when a test finished.
     * @param tr Test results.
     */
    virtual void test_completed(const test_result& /*tr*/){};

    /**
     * Called when a group is completed
     * @param name Name of the group
     */
    virtual void group_completed(const std::string& /*name*/){};

    /**
     * Called when all tests in run completed.
     */
    virtual void run_completed(){};
  };

  /**
   * Typedef for runner::list_groups()
   */
  typedef std::vector<std::string> groupnames;

  /**
   * Test runner.
   */
  class test_runner
  {
    protected:
      typedef std::map<std::string,group_base*> groups;
      typedef groups::iterator iterator;
      typedef groups::const_iterator const_iterator;
      groups groups_;

      callback  default_callback_;
      callback* callback_;

    public:
    /**
     * Constructor
     */
    test_runner() : callback_(&default_callback_)
    {
    }

    /**
     * Stores another group for getting by name.
     */
    void register_group(const std::string& name,group_base* gr)
    {
      if( gr == 0 )
      {
        throw std::invalid_argument("group shall be non-null");
      }

      groups::iterator found = groups_.find(name);
      if( found != groups_.end() )
      {
        std::string msg("attempt to add already existent group "+name);
        // this exception terminates application so we use cerr also
        std::cerr << msg << std::endl;
        throw std::logic_error(msg);
      }

      groups_[name] = gr;
    }

    /**
     * Stores callback object.
     */
    void set_callback(callback* cb)
    {
      callback_ = cb==0? &default_callback_:cb;
    }

    /**
     * Returns callback object.
     */
    callback& get_callback() const
    {
      return *callback_;
    }

    /**
     * Returns list of known test groups.
     */
    const groupnames list_groups() const
    {
      groupnames ret;
      const_iterator i = groups_.begin();
      const_iterator e = groups_.end();
      while( i != e )
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
      callback_->run_started();

      const_iterator i = groups_.begin();
      const_iterator e = groups_.end();
      while( i != e )
      {
        callback_->group_started(i->first);
        try
        {
          run_all_tests_in_group_(i);
        }
        catch( const no_more_tests& )
        {
          callback_->group_completed(i->first);
        }

        ++i;
      }

      callback_->run_completed();
    }

    /**
     * Runs all tests in specified group.
     */
    void run_tests(const std::string& group_name) const
    {
      callback_->run_started();

      const_iterator i = groups_.find(group_name);
      if( i == groups_.end() )
      {
        callback_->run_completed();
        throw no_such_group(group_name);
      }

      callback_->group_started(group_name);
      try
      {
        run_all_tests_in_group_(i);
      }
      catch( const no_more_tests& )
      {
        // ok
      }

      callback_->group_completed(group_name);
      callback_->run_completed();
    }

    /**
     * Runs one test in specified group.
     */
    test_result run_test(const std::string& group_name,int n) const
    {
      callback_->run_started();

      const_iterator i = groups_.find(group_name);
      if( i == groups_.end() )
      {
        callback_->run_completed();
        throw no_such_group(group_name);
      }

      callback_->group_started(group_name);
      try
      {
        test_result tr = i->second->run_test(n);
        callback_->test_completed(tr);
        callback_->group_completed(group_name);
        callback_->run_completed();
        return tr;
      }
      catch( const beyond_last_test& )
      {
        callback_->group_completed(group_name);
        callback_->run_completed();
        throw;
      }      
      catch( const no_such_test& )
      {
        callback_->group_completed(group_name);
        callback_->run_completed();
        throw;
      }
    }

    private:
    void run_all_tests_in_group_(const_iterator i) const
    {
      i->second->rewind();
      for( ;; )
      {
        test_result tr = i->second->run_next();
        callback_->test_completed(tr);

	if( tr.result == test_result::ex_ctor )
	{
          throw no_more_tests();
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

  /**
   * Test object. Contains data test run upon and default test method 
   * implementation. Inherited from Data to allow tests to  
   * access test data as members.
   */
  template <class Data>
  class test_object : public Data
  {
    public:
    /**
     * Default constructor
     */
    test_object(){};

    /**
     * The flag is set to true by default (dummy) test.
     * Used to detect usused test numbers and avoid unnecessary
     * test object creation which may be time-consuming depending
     * on operations described in Data::Data() and Data::~Data().
     * TODO: replace with throwing special exception from default test.
     */
    bool called_method_was_a_dummy_test_;
    
    void set_test_name(const std::string& current_test_name)
    {
    	current_test_name_ = current_test_name; 
    }
    
    const std::string& get_test_name() const
    {
    	return current_test_name_;
    }

    /**
     * Default do-nothing test.
     */
    template <int n>
    void test()
    {
      called_method_was_a_dummy_test_ = true;
    }
    
    private:
    
    	std::string current_test_name_;
  };

  namespace 
  {
    /**
     * Tests provided condition.
     * Throws if false.
     */
    void ensure(bool cond)
    {
       if( !cond ) throw failure("");
    }

    /**
     * Tests provided condition.
     * Throws if false.
     */
    template<typename T>
    void ensure(const T msg,bool cond)
    {
       if( !cond ) throw failure(msg);
    }

    /**
     * Tests two objects for being equal.
     * Throws if false.
     *
     * NB: both T and Q must have operator << defined somewhere, or
     * client code will not compile at all!
     */
    template <class T,class Q>
    void ensure_equals(const char* msg,const Q& actual,const T& expected)
    {
      if( expected != actual )
      {
        std::stringstream ss;
        ss << (msg?msg:"") << (msg?": ":"") << "expected " << expected << " actual " << actual;
        throw failure(ss.str().c_str());
      }
    }

    template <class T,class Q>
    void ensure_equals(const Q& actual,const T& expected)
    {
      ensure_equals<>(0,actual,expected);
    }

    /**
     * Tests two objects for being at most in given distance one from another.
     * Borders are excluded.
     * Throws if false.
     *
     * NB: T must have operator << defined somewhere, or
     * client code will not compile at all! Also, T shall have
     * operators + and -, and be comparable.
     */
    template <class T>
    void ensure_distance(const char* msg,const T& actual,const T& expected,const T& distance)
    {
      if( expected-distance >= actual || expected+distance <= actual )
      {
        std::stringstream ss;
        ss << (msg?msg:"") << (msg?": ":"") << "expected [" << expected-distance << ";" 
           << expected+distance << "] actual " << actual;
        throw failure(ss.str().c_str());
      }
    }

    template <class T>
    void ensure_distance(const T& actual,const T& expected,const T& distance)
    {
      ensure_distance<>(0,actual,expected,distance);
    }

    /**
     * Unconditionally fails with message.
     */
    void fail(const char* msg="")
    {
      throw failure(msg);
    }
  }

  /**
   * Walks through test tree and stores address of each
   * test method in group. Instantiation stops at 0.
   */
  template <class Test,class Group,int n>
  struct tests_registerer
  {
    static void reg(Group& group)
    {
      group.reg(n,&Test::template test<n>);
      tests_registerer<Test,Group,n-1>::reg(group);
    }
  };

  template<class Test,class Group>
  struct tests_registerer<Test,Group,0>
  {
    static void reg(Group&){};
  };

  /**
   * Test group; used to recreate test object instance for
   * each new test since we have to have reinitialized 
   * Data base class.
   */
  template <class Data,int MaxTestsInGroup = 50>
  class test_group : public group_base
  {
    const char* name_;

    typedef void (test_object<Data>::*testmethod)();
    typedef std::map<int,testmethod> tests;
    typedef typename tests::iterator tests_iterator;
    typedef typename tests::const_iterator tests_const_iterator;
    typedef typename tests::const_reverse_iterator 
                     tests_const_reverse_iterator;
    typedef typename tests::size_type size_type;

    tests tests_;
    tests_iterator current_test_;

    /**
     * Exception-in-destructor-safe smart-pointer class.
     */
    template <class T>
    class safe_holder
    {
      T* p_;
      bool permit_throw_in_dtor;

      safe_holder(const safe_holder&);
      safe_holder& operator = (const safe_holder&);

      public:
      safe_holder() : p_(0),permit_throw_in_dtor(false)
      { 
      }

      ~safe_holder()
      {
        release();
      }

      T* operator -> () const { return p_; };
      T* get() const { return p_; };

      /**
       * Tell ptr it can throw from destructor. Right way is to
       * use std::uncaught_exception(), but some compilers lack
       * correct implementation of the function.
       */
      void permit_throw(){ permit_throw_in_dtor = true; }

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
          if( delete_obj() == false )
          {
            throw warning("destructor of test object raised an SEH exception");
          }
        }
        catch( const std::exception& ex )
        {
          if( permit_throw_in_dtor ) 
          {
            std::string msg = "destructor of test object raised exception: ";
            msg += ex.what();
            throw warning(msg);
          }
        }
        catch( ... )
        {
          if( permit_throw_in_dtor )
          {
            throw warning("destructor of test object raised an exception");
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
          if( permit_throw_in_dtor )
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
      tests_registerer<object,test_group,MaxTestsInGroup>::reg(*this);
    };

    /**
     * This constructor is used in self-test run only.
     */
    test_group(const char* name,test_runner& another_runner)
      : name_(name)
    {
      // register itself
      another_runner.register_group(name_,this); 
    
      // register all tests
      tests_registerer<test_object<Data>,
                       test_group,MaxTestsInGroup>::reg(*this);
    };

    /**
     * Registers test method under given number.
     */
    void reg(int n,testmethod tm)
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
    test_result run_next()
    {
      if( current_test_ == tests_.end() )
      {
        throw no_more_tests();
      }

      // find next user-specialized test
      safe_holder<object> obj;
      while( current_test_ != tests_.end() )
      {
        try
        {
          return run_test_(current_test_++,obj);
        }
        catch( const no_such_test& )
        {
          continue; 
        }
      } 

      throw no_more_tests();
    }

    /**
     * Runs one test by position.
     */
    test_result run_test(int n)
    {
      // beyond tests is special case to discover upper limit
      if( tests_.rbegin() == tests_.rend() ) throw beyond_last_test();
      if( tests_.rbegin()->first < n ) throw beyond_last_test();

      // withing scope; check if given test exists
      tests_iterator ti = tests_.find(n);
      if( ti == tests_.end() ) throw no_such_test();

      safe_holder<object> obj;
      return run_test_(ti,obj);
    }

  private:
    /**
     * VC allows only one exception handling type per function,
     * so I have to split the method
     */
    test_result run_test_(const tests_iterator& ti,safe_holder<object>& obj)
    {
	  std::string current_test_name;
      try
      {
        if( run_test_seh_(ti->second,obj,current_test_name) == false )
          throw seh("seh");
      }
      catch(const no_such_test&)
      {
        throw;
      }
      catch(const warning& ex)
      {
        // test ok, but destructor failed
		  if (obj.get()) current_test_name = obj->get_test_name();
        test_result tr(name_,ti->first, current_test_name, test_result::warn,ex);
        return tr;
      }
      catch(const failure& ex)
      {
        // test failed because of ensure() or similar method
		  if (obj.get()) current_test_name = obj->get_test_name();
        test_result tr(name_,ti->first, current_test_name, test_result::fail,ex);
        return tr;
      }
      catch(const seh& ex)
      {
        // test failed with sigsegv, divide by zero, etc
		  if (obj.get()) current_test_name = obj->get_test_name();
        test_result tr(name_,ti->first, current_test_name, test_result::term,ex);
        return tr;
      }
      catch(const bad_ctor& ex)
      {
        // test failed because test ctor failed; stop the whole group
		  if (obj.get()) current_test_name = obj->get_test_name();
        test_result tr(name_,ti->first, current_test_name, test_result::ex_ctor,ex);
        return tr;
      }
      catch(const std::exception& ex)
      {
        // test failed with std::exception
		  if (obj.get()) current_test_name = obj->get_test_name();
        test_result tr(name_,ti->first, current_test_name, test_result::ex,ex);
        return tr;
      }
      catch(...)
      {
        // test failed with unknown exception
		  if (obj.get()) current_test_name = obj->get_test_name();
        test_result tr(name_,ti->first, current_test_name, test_result::ex);
        return tr;
      }

      // test passed
      test_result tr(name_,ti->first, current_test_name, test_result::ok);
      return tr;
    }

    /**
     * Runs one under SEH if platform supports it.
     */
	bool run_test_seh_(testmethod tm,safe_holder<object>& obj, std::string& current_test_name)    {
#if defined(TUT_USE_SEH)
      __try
      {
#endif
        if( obj.get() == 0 ) 
	{
          reset_holder_(obj);
	}
        obj->called_method_was_a_dummy_test_ = false;

#if defined(TUT_USE_SEH)
        __try
        {
#endif
          (obj.get()->*tm)();
#if defined(TUT_USE_SEH)
        }
        __except(handle_seh_(::GetExceptionCode()))
        {
          // throw seh("SEH");
			current_test_name = obj->get_test_name();
          return false;
        }
#endif

        if( obj->called_method_was_a_dummy_test_ )
        {
          // do not call obj.release(); reuse object
          throw no_such_test();
        }

		current_test_name = obj->get_test_name();
        obj.permit_throw();
        obj.release();
#if defined(TUT_USE_SEH)
      }
      __except(handle_seh_(::GetExceptionCode()))
      {
        return false;
      }
#endif
      return true;
    }

    void reset_holder_(safe_holder<object>& obj)
    {
      try 
      {
        obj.reset();
      }
      catch(const std::exception& ex)
      {
        throw bad_ctor(ex.what());
      }
      catch(...)
      {
        throw bad_ctor("test constructor has generated an exception; group execution is terminated");
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

