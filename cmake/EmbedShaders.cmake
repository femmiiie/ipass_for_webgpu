function(ipass_embed_shaders)
  cmake_parse_arguments(ARG "" "OUTPUT" "SOURCES;NAMES" ${ARGN})

  if(NOT ARG_OUTPUT)
    message(FATAL_ERROR "ipass_embed_shaders: OUTPUT is required")
  endif()

  if(NOT ARG_SOURCES)
    message(FATAL_ERROR "ipass_embed_shaders: SOURCES is required")
  endif()

  if(NOT ARG_NAMES)
    message(FATAL_ERROR "ipass_embed_shaders: NAMES is required")
  endif()

  list(LENGTH ARG_SOURCES _src_count)
  list(LENGTH ARG_NAMES _name_count)

  if(NOT _src_count EQUAL _name_count)
    message(FATAL_ERROR
      "ipass_embed_shaders: SOURCES (${_src_count}) and NAMES (${_name_count}) "
      "must have equal length"
    )
  endif()

  file(WRITE "${ARG_OUTPUT}"
    "#pragma once\n#include <string>\nnamespace embedded_shaders {\n"
  )

  math(EXPR _last "${_src_count} - 1")

  foreach(IDX RANGE ${_last})
    list(GET ARG_SOURCES ${IDX} _shader_file)
    list(GET ARG_NAMES ${IDX} _var_name)
    file(READ "${_shader_file}" _content)
    file(APPEND "${ARG_OUTPUT}"
      "inline const std::string ${_var_name} = R\"WGSL(\n${_content})WGSL\";\n"
    )
  endforeach()

  file(APPEND "${ARG_OUTPUT}" "} // namespace embedded_shaders\n")
endfunction()
