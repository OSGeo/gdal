#
# script to build the C# DLLs using dotnet
#

configure_file(dll_template.csproj ${TARGET}.csproj)

ADD_DOTNET(${TARGET}.csproj)
