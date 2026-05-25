if(NOT DEFINED WHP_COPY_SOURCE)
    message(FATAL_ERROR "WHP_COPY_SOURCE is required.")
endif()

if(NOT DEFINED WHP_COPY_DESTINATION)
    message(FATAL_ERROR "WHP_COPY_DESTINATION is required.")
endif()

if(NOT IS_DIRECTORY "${WHP_COPY_SOURCE}")
    message(FATAL_ERROR "Copy source is not a directory: ${WHP_COPY_SOURCE}")
endif()

file(MAKE_DIRECTORY "${WHP_COPY_DESTINATION}")
file(GLOB_RECURSE _whip_copy_entries LIST_DIRECTORIES true "${WHP_COPY_SOURCE}/*")

foreach(_source_entry IN LISTS _whip_copy_entries)
    file(RELATIVE_PATH _relative_path "${WHP_COPY_SOURCE}" "${_source_entry}")
    set(_destination_entry "${WHP_COPY_DESTINATION}/${_relative_path}")

    if(IS_DIRECTORY "${_source_entry}")
        file(MAKE_DIRECTORY "${_destination_entry}")
    else()
        get_filename_component(_destination_dir "${_destination_entry}" DIRECTORY)
        file(MAKE_DIRECTORY "${_destination_dir}")
        file(COPY_FILE "${_source_entry}" "${_destination_entry}" ONLY_IF_DIFFERENT RESULT _copy_result)

        if(NOT _copy_result STREQUAL "0")
            message(FATAL_ERROR "Failed to copy ${_source_entry} to ${_destination_entry}: ${_copy_result}")
        endif()
    endif()
endforeach()
