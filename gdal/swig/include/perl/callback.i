%inline %{
    #ifndef SWIG
    typedef struct
    {
        SV *fct;
        SV *data;
    } SavedEnv;
    #endif
    int callback_d_cp_vp(double d, const char *cp, void *vp)
    {
        int count, ret;
        SavedEnv *env_ptr = (SavedEnv *)vp;
        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        XPUSHs(sv_2mortal(newSVnv(d)));
        XPUSHs(sv_2mortal(newSVpv(cp, 0)));
        if (env_ptr->data)
            XPUSHs(env_ptr->data);
        PUTBACK;
        count = call_sv(env_ptr->fct, G_SCALAR);
        SPAGAIN;
        if (count != 1) {
            fprintf(stderr, "The callback must return only one value.\n");
            return 0; /* interrupt */
        }
        ret = POPi;
        PUTBACK;
        FREETMPS;
        LEAVE;
        return ret;
    }
%}
