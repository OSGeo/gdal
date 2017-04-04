# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author: Dmitriy Baryshnikov (aka Bishop), polimax@mail.ru
# ******************************************************************************
# * Copyright (C) 2012 Bishop
# * 
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included
# * in all copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# * DEALINGS IN THE SOFTWARE.
# ******************************************************************************

macro(apps_macro APP_NAME APP_CSOURCES APP_HHEADERS)
	project (${APP_NAME})
	add_executable(${APP_NAME} ${APP_CSOURCES} ${APP_HHEADERS})

	set_target_properties(${APP_NAME}
		PROPERTIES PROJECT_LABEL ${APP_NAME}
		VERSION ${GDAL_VERSION}
		SOVERSION 1
		ARCHIVE_OUTPUT_DIRECTORY ${GDAL_ROOT_BINARY_DIR}
		LIBRARY_OUTPUT_DIRECTORY ${GDAL_ROOT_BINARY_DIR}
		RUNTIME_OUTPUT_DIRECTORY ${GDAL_ROOT_BINARY_DIR} 
		)

	target_link_libraries(${APP_NAME} ${GDAL_LIB_NAME})

	install(TARGETS ${APP_NAME} DESTINATION bin)
endmacro()


