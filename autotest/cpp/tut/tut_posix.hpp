#ifndef TUT_FORK_H_GUARD
#define TUT_FORK_H_GUARD

#if defined(TUT_USE_POSIX)
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>

#include <cstring>
#include <cstdlib>
#include <map>
#include <iterator>
#include <functional>

#include "tut_result.hpp"
#include "tut_assert.hpp"
#include "tut_runner.hpp"

namespace tut
{

template<typename, int>
class test_group;

template<typename T>
class test_object;

class test_group_posix
{
private:
    template<typename, int>
    friend class test_group;

    template<typename T>
    void send_result_(const T *obj, const test_result &tr)
    {
        if(obj->get_pipe_() == -1)
        {
            return;
        }

        if(tr.result != test_result::ok)
        {
            std::stringstream ss;
            ss << int(tr.result) << "\n"
                << tr.group << "\n"
                << tr.test << "\n"
                << tr.name << "\n"
                << tr.exception_typeid << "\n";
            std::copy( tr.message.begin(), tr.message.end(), std::ostreambuf_iterator<char>(ss.rdbuf()) );

            int size = ss.str().length();
            int w = write(obj->get_pipe_(), ss.str().c_str(), size);
            ensure_errno("write() failed", w == size);
        }
    }
};

template<typename T>
struct tut_posix
{
    pid_t fork()
    {
        test_object<T> *self = dynamic_cast< tut::test_object<T>* >(this);
        ensure("trying to call 'tut_fork' in ctor of test object", self != NULL);

        return self->fork_();
    }

    pid_t waitpid(pid_t pid, int *status, int flags = 0)
    {
        test_object<T> *self = dynamic_cast< tut::test_object<T>* >(this);
        ensure("trying to call 'tut_waitpid' in ctor of test object", self != NULL);

        return self->waitpid_(pid, status, flags);
    }

    void ensure_child_exit(pid_t pid, int exit_status = 0)
    {
        test_object<T> *self = dynamic_cast< tut::test_object<T>* >(this);
        ensure("trying to call 'ensure_child_exit' in ctor of test object", self != NULL);

        int status;
        self->waitpid_(pid, &status);

        self->ensure_child_exit_(status, exit_status);
    }


    void ensure_child_signal(pid_t pid, int signal = SIGTERM)
    {
        test_object<T> *self = dynamic_cast< tut::test_object<T>* >(this);
        ensure("trying to call 'ensure_child_signal' in ctor of test object", self != NULL);

        int status;
        self->waitpid_(pid, &status);

        self->ensure_child_signal_(status, signal);
    }

    std::set<pid_t> get_pids() const
    {
        using namespace std;

        const test_object<T> *self = dynamic_cast< const tut::test_object<T>* >(this);
        ensure("trying to call 'get_pids' in ctor of test object", self != NULL);

        return self->get_pids_();
    }

    virtual ~tut_posix()
    {
    }

};

class test_object_posix
{
public:
    typedef std::map<pid_t, int> pid_map;

    /**
     * Default constructor
     */
    test_object_posix()
        : pipe_(-1)
    {
    }


    virtual ~test_object_posix()
    {
        // we have forked
        if(pipe_ != -1)
        {
            // in child, force exit
            std::exit(0);
        }

        if(!pids_.empty())
        {
            std::stringstream ss;

            // in parent, reap children
            for(std::map<pid_t, int>::iterator i = pids_.begin(); i != pids_.end(); ++i)
            {
                try {
                    kill_child_(i->first);
                } catch(const rethrown &ex) {
                    ss << std::endl << "child " << ex.tr.pid << " has thrown an exception: " << ex.what();
                } catch(const failure &ex) {
                    ss << std::endl << ex.what();
                }
            }

            if(!ss.str().empty())
            {
                fail(ss.str().c_str());
            }
        }
    }

private:
    template<typename T>
    friend struct tut_posix;

    friend class test_group_posix;

    int get_pipe_() const
    {
        return pipe_;
    }


    pid_t fork_()
    {
        // create pipe
        int fds[2];
        ensure_errno("pipe() failed", ::pipe(fds) == 0);

        pid_t pid = ::fork();

        ensure_errno("fork() failed", pid >= 0);

        if(pid != 0)
        {
            // in parent, register pid
            ensure("duplicated child", pids_.insert( std::make_pair(pid, fds[0]) ).second);

            // close writing side
            close(fds[1]);
        }
        else
        {
            // in child, shutdown reporter
            tut::runner.get().clear_callbacks();

            // close reading side
            close(fds[0]);
            pipe_ = fds[1];
        }

        return pid;
    }

    void kill_child_(pid_t pid)
    {
        int status;

        if(waitpid_(pid, &status, WNOHANG) == pid)
        {
            ensure_child_exit_(status, 0);
            return;
        }

        if(::kill(pid, SIGTERM) != 0)
        {
            if(errno == ESRCH)
            {
                // no such process
                return;
            }
            else
            {
                // cannot kill, we are in trouble
                std::stringstream ss;
                char e[1024];
                ss << "child " << pid << " could not be killed with SIGTERM, " << strerror_r(errno, e, sizeof(e)) << std::endl;
                fail(ss.str());
            }
        }

        if(waitpid_(pid, &status, WNOHANG) == pid)
        {
            // child killed, check signal
            ensure_child_signal_(status, SIGTERM);

            ensure_equals("child process exists after SIGTERM", ::kill(pid, 0), -1);
            return;
        }

        // child seems to be still exiting, give it some time
        sleep(2);

        if(waitpid_(pid, &status, WNOHANG) != pid)
        {
            // child is still running, kill it
            if(::kill(pid, SIGKILL) != 0)
            {
                if(errno == ESRCH)
                {
                    // no such process
                    return;
                }
                else
                {
                    std::stringstream ss;
                    char e[1024];
                    ss << "child " << pid << " could not be killed with SIGKILL, " << strerror_r(errno, e, sizeof(e)) << std::endl;
                    fail(ss.str());
                }
            }

            ensure_equals("wait after SIGKILL", waitpid_(pid, &status), pid);
            ensure_child_signal_(status, SIGKILL);

            ensure_equals("child process exists after SIGKILL", ::kill(pid, 0), -1);

            std::stringstream ss;
            ss << "child " << pid << " had to be killed with SIGKILL";
            fail(ss.str());
        }
    }

    test_result receive_result_(std::istream &ss, pid_t pid)
    {
        test_result tr;

        int type;
        ss >> type;
        tr.result = test_result::result_type(type);
        ss.ignore(1024, '\n');

        std::getline(ss, tr.group);
        ss >> tr.test;
        ss.ignore(1024, '\n');
        std::getline(ss, tr.name);
        std::getline(ss, tr.exception_typeid);
        std::copy( std::istreambuf_iterator<char>(ss.rdbuf()),
                   std::istreambuf_iterator<char>(),
                   std::back_inserter(tr.message) );

        tr.pid = pid;

        return tr;
    }

    struct fdclose
    {
        fdclose(int fd): fd_(fd) { }
        ~fdclose()
        {
            close(fd_);
        }
    private:
        int fd_;
    };

    pid_t waitpid_(pid_t pid, int *status, int flags = 0)
    {

        ensure("trying to wait for unknown pid", pids_.count(pid) > 0);

        pid_t p = ::waitpid(pid, status, flags);
        if( (flags & WNOHANG) && (p != pid) )
        {
            return p;
        }

        // read child result from pipe
        fd_set fdset;
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        FD_ZERO(&fdset);

        int pipe = pids_[pid];
        fdclose guard(pipe);

        FD_SET(pipe, &fdset);

        int result = select(pipe+1, &fdset, NULL, NULL, &tv);
        ensure_errno("sanity check on select() failed", result >= 0);

        if(result > 0)
        {
            ensure("sanity check on FD_ISSET() failed", FD_ISSET(pipe, &fdset) );

            std::stringstream ss;

            //TODO: max failure length
            char buffer[1024];
            int r = read(pipe, buffer, sizeof(buffer));
            ensure_errno("sanity check on read() failed", r >= 0);

            if(r > 0)
            {
                ss.write(buffer, r);
                throw rethrown( receive_result_(ss, pid) );
            }
        }

        return pid;
    }

    void ensure_child_exit_(int status, int exit_status)
    {
        if(WIFSIGNALED(status))
        {
            std::stringstream ss;
            ss << "child killed by signal " << WTERMSIG(status)
                << ": expected exit with code " << exit_status;

            throw failure(ss.str().c_str());
        }

        if(WIFEXITED(status))
        {
            if(WEXITSTATUS(status) != exit_status)
            {
                std::stringstream ss;
                ss << "child exited, expected '"
                    << exit_status
                    << "' actual '"
                    << WEXITSTATUS(status)
                    << '\'';

                throw failure(ss.str().c_str());
            }
        }

        if(WIFSTOPPED(status))
        {
            std::stringstream ss;
            ss << "child stopped by signal " << WTERMSIG(status)
                << ": expected exit with code " << exit_status;
            throw failure(ss.str().c_str());
        }
    }

    void ensure_child_signal_(int status, int signal)
    {
        if(WIFSIGNALED(status))
        {
            if(WTERMSIG(status) != signal)
            {
                std::stringstream ss;
                ss << "child killed by signal, expected '"
                    << signal
                    << "' actual '"
                    << WTERMSIG(status)
                    << '\'';
                throw failure(ss.str().c_str());
            }
        }

        if(WIFEXITED(status))
        {
            std::stringstream ss;
            ss << "child exited with code " << WEXITSTATUS(status)
                << ": expected signal " << signal;

            throw failure(ss.str().c_str());
        }

        if(WIFSTOPPED(status))
        {
            std::stringstream ss;
            ss << "child stopped by signal " << WTERMSIG(status)
                << ": expected kill by signal " << signal;

            throw failure(ss.str().c_str());
        }
    }

    std::set<pid_t> get_pids_() const
    {
        using namespace std;

        set<pid_t> pids;
        for(pid_map::const_iterator i = pids_.begin(); i != pids_.end(); ++i)
        {
            pids.insert( i->first );
        }

        return pids;
    }

    pid_map         pids_;
    int             pipe_;
};

} // namespace tut

#else

namespace tut
{

struct test_object_posix
{
};

struct test_group_posix
{
    template<typename T>
    void send_result_(const T*, const test_result &)
    {
    }
};

} // namespace tut

#endif


#endif

