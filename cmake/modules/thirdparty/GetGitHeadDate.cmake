include(GetGitRevisionDescription)
function(get_git_head_date _var _repo_dir)
	if(NOT GIT_FOUND)
		find_package(Git QUIET)
	endif()
	get_git_head_revision(refspec hash "${_repo_dir}")
	if(NOT GIT_FOUND)
		set(${_var} "GIT-NOTFOUND" PARENT_SCOPE)
		return()
	endif()
	if(NOT hash)
		set(${_var} "HEAD-HASH-NOTFOUND" PARENT_SCOPE)
		return()
	endif()

	execute_process(COMMAND
		"${GIT_EXECUTABLE}"
		log -1 --date=format:'%Y%m%d' --format="%ad" --quiet HEAD --
		WORKING_DIRECTORY
		"${CMAKE_CURRENT_SOURCE_DIR}"
		RESULT_VARIABLE
		res
		OUTPUT_VARIABLE
		out
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(res EQUAL 0)
		string(REGEX MATCH "[0-9]+" NUM ${out})
		set(${_var} ${NUM} PARENT_SCOPE)
	else()
		set(${_var} "20189999" PARENT_SCOPE)
	endif()
endfunction()
