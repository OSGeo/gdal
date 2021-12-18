#
# script to build the C# DLLs using dotnet
#

configure_file("${SOURCE_DIR}/dll_template.csproj" "${BUILD_DIR}/${TARGET_SUBDIR}/${TARGET}.csproj")

ADD_DOTNET("${BUILD_DIR}/${TARGET_SUBDIR}/${TARGET}.csproj")
