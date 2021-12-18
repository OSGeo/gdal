#
# script to build the C# DLLs using dotnet
#

find_package(Dotnet)

configure_file("${SOURCE_DIR}/dll_template.csproj" "${BUILD_DIR}/${TARGET_SUBDIR}/${TARGET}.csproj")

ADD_DOTNET("${BUILD_DIR}/${TARGET_SUBDIR}/${TARGET}.csproj")
