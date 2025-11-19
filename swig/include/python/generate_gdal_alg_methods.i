/* Dynamically generates gdal.alg module. */

%pythoncode %{

def _generate_gdal_alg_methods():
    """Dynamically generates gdal.alg.{X}.{Y}.{func} methods from GDAL algorithms"""

    import copy
    import os
    import types
    import typing
    from typing import Callable, List, Union, Optional

    gdal_module = sys.modules[__name__]

    def _get_type_hint(arg, for_input):

        import sys
        if sys.version_info >= (3, 9, 0):
            pathlike = "os.PathLike[str]"
        else:
            pathlike = "os.PathLike"
        if arg.GetType() == GAAT_BOOLEAN:
            type_hint = "bool"
        elif arg.GetType() == GAAT_INTEGER:
            type_hint = "int"
        elif arg.GetType() == GAAT_INTEGER_LIST:
            type_hint = "Union[List[int], int]"
        elif arg.GetType() == GAAT_REAL:
            type_hint = "float"
        elif arg.GetType() == GAAT_REAL_LIST:
            type_hint = "Union[List[float], float]"
        elif arg.GetType() == GAAT_STRING:
            if arg.GetName() in ("input", "dataset"):
                type_hint = f"Union[str, {pathlike}]"
            else:
                type_hint = "str"
        elif arg.GetType() == GAAT_STRING_LIST:
            type_hint = "Union[List[str], dict, str]"
        elif arg.GetType() == GAAT_DATASET:
            if for_input:
                type_hint = f"Union[gdal.Dataset, str, {pathlike}]"
            else:
                type_hint = "gdal.ArgDatasetValue"
        elif arg.GetType() == GAAT_DATASET_LIST:
            type_hint = f"Union[List[gdal.Dataset], List[str], List[{pathlike}]]"
        return type_hint

    def register_alg(alg, path, parent_module):
        name = alg.GetName()
        new_path = path + [name]

        if alg.HasSubAlgorithms():
            if path:
                submodule = types.ModuleType(parent_module.__name__ + "." + name)
                submodule.__doc__ = alg.GetDescription()
                setattr(parent_module, name, submodule)
            else:
                submodule = parent_module
            for subalg in alg.GetSubAlgorithmNames():
                register_alg(alg.InstantiateSubAlgorithm(subalg), new_path, submodule)
        else:
            name_sanitized = name.replace('-', '_')
            assert name_sanitized.isidentifier()

            args = ""
            kwargs = "{"
            parameters = ""
            for pass_idx in (1, 2):
                for arg_name in alg.GetArgNames():
                    arg = alg.GetArg(arg_name)
                    if arg.IsInput() and not arg.IsHiddenForAPI():
                        is_required = (arg.IsRequired() or arg_name == "pipeline")
                        if pass_idx == 1 and not is_required:
                            continue
                        elif pass_idx == 2 and is_required:
                            continue
                        if args:
                            args += ", "
                            kwargs += ", "

                        arg_name_sanitized = arg_name.replace('-', '_')
                        if arg_name_sanitized[0:1].isdigit():
                            arg_name_sanitized = '_' + arg_name_sanitized

                        assert arg_name_sanitized.isidentifier()

                        args += arg_name_sanitized
                        type_hint = _get_type_hint(arg, for_input=True)
                        if not is_required:
                            type_hint = f"Optional[{type_hint}]=None"
                        args += f": {type_hint}"

                        kwargs += f'"{arg_name_sanitized}": {arg_name_sanitized}'

                        parameters += "       "
                        parameters += arg_name_sanitized
                        parameters += ": "
                        parameters += type_hint
                        parameters += "\n"
                        parameters += "       "
                        parameters += "    "
                        parameters += arg.GetDescription()
                        parameters += "\n"

            if args:
                args += ", "
                kwargs += ", "
            args += "progress: Optional[Callable[[float, str, object], bool]]=None"
            kwargs += '"progress": progress'
            parameters += "       "
            parameters += "progress"
            parameters += ": "
            parameters += "Optional[Callable[[float, str, object], bool]]=None"
            parameters += "\n"
            parameters += "       "
            parameters += "    "
            parameters += "Progress callback"
            parameters += "\n"

            kwargs += "}"

            output_parameters = ""
            for arg_name in alg.GetArgNames():
                arg = alg.GetArg(arg_name)
                if arg.IsOutput() and not arg.IsHiddenForAPI():

                    arg_name_sanitized = arg_name.replace('-', '_')
                    if arg_name_sanitized[0:1].isdigit():
                        arg_name_sanitized = '_' + arg_name_sanitized

                    assert arg_name_sanitized.isidentifier()

                    output_parameters += "       "
                    output_parameters += arg_name_sanitized
                    output_parameters += ": "
                    output_parameters += _get_type_hint(arg, for_input=False)
                    output_parameters += "\n"
                    output_parameters += "       "
                    output_parameters += "    "
                    output_parameters += arg.GetDescription()
                    output_parameters += "\n"


            func_code = f"""
def {name_sanitized}({args}):
    kwargs = {kwargs}
    kwargs = {{k: v for k, v in kwargs.items() if v is not None}}
    return gdal.Run({new_path}, **kwargs)
"""
            func_globals = copy.copy(parent_module.__dict__)
            extra = {"gdal": gdal_module, "os": os, "List": List, "Union": Union, "Optional": Optional, "Callable": Callable}
            for k,v in extra.items():
                func_globals[k] = v
            exec(func_code, func_globals)

            parent_module.__dict__[name_sanitized] = func_globals[name_sanitized]

            # Register doc string
            parent_module.__dict__[name_sanitized].__doc__ = f"""{alg.GetDescription()}

       Consult {alg.GetHelpFullURL()} for more details.

       Parameters
       ----------
{parameters}

       Output parameters
       -----------------
{output_parameters}
"""

    #  Module that lazily registers its submodules and methods
    class GdalAlgLazyModule(types.ModuleType):
        """Root module to access GDAL algorithms"""
        _registered_algs = False

        def _register_algs(self):
            if not self._registered_algs:
                self._registered_algs = True
                reg = _gdal.GetGlobalAlgorithmRegistry()
                register_alg(reg.InstantiateAlg("gdal"), [], alg)

        def __getattribute__(self, name):
            if name not in ("_registered_algs", "_register_algs") and not object.__getattribute__(self, "_registered_algs"):
                self._register_algs()
            return object.__getattribute__(self, name)

        def __dir__(self):
            if not self._registered_algs:
                self._register_algs()
            return list(super().__dir__())

    # Registers gdal.alg module
    gdal_module.alg = GdalAlgLazyModule("gdal.alg")

_generate_gdal_alg_methods()

def reregister_gdal_alg():
    """Destroys current gdal.alg module and recreates it"""

    delattr(sys.modules[__name__], "alg")
    _generate_gdal_alg_methods()

%}
