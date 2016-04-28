%fragment("number_to_PyObject", "header") %{
    PyObject *number_to_PyObject(gma_number_t *nr) {
        if (nr->is_unsigned())
            return PyInt_FromLong(nr->value_as_unsigned());
        else if (nr->is_integer())
            return PyInt_FromLong(nr->value_as_int());
        return PyFloat_FromDouble(nr->value_as_double());
    }
%}


%typemap(out,fragment="number_to_PyObject") gma_number_t * {
    $result = number_to_PyObject($1);
}

%typemap(typecheck,precedence=200) gma_bins_t * {
}

%typemap(typecheck,precedence=201) gma_pair_t * {
}

%typemap(in) gma_bins_t * {
}

%typemap(in) gma_pair_t * {
}

%typemap(out,fragment="number_to_PyObject") gma_histogram_t * {
}
