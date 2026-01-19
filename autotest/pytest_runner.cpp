#include <string>
// Avoid issue with /usr/include/python3.14/pyconfig-64.h from Fedora
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
#include <Python.h>

int main(int argc, char **argv)
{
    std::string args;
    if (argc > 1)
    {
        args.append("[");
        for (int i = 1; i < argc; i++)
        {
            if (i > 2)
                args.append(",");
            args.append("\"");
            args.append(argv[i]);
            args.append("\"");
        }
        args.append("]");
    }
    std::string pycode = "import pytest\\npytest.main(" + args + ")\\n";

    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    config.install_signal_handlers = 0;
    PyStatus status;
    status = PyConfig_SetBytesString(&config, &config.program_name, argv[0]);
    if (PyStatus_Exception(status))
    {
        PyConfig_Clear(&config);
        if (argc >= 0)
            Py_ExitStatusException(status);
        return 1;
    }
    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status))
    {
        PyConfig_Clear(&config);
        if (argc >= 0)
            Py_ExitStatusException(status);
        return 1;
    }
    PyConfig_Clear(&config);

    PyRun_SimpleString(&*pycode.begin());
    Py_Finalize();
    return 0;
}
