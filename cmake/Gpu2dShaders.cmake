# GLSL compiled to SPIR-V by glslc at build time, written as comma-separated words for a C++ array
# initialiser (`#include "name.stage.inc"`). Shared *.glsl files beside the sources are #included
# with GL_GOOGLE_include_directive and rebuild every shader that may use them.

find_program(GLSLC glslc)

# gpu2d_compile_shaders(<target> <source>...): compiles each source (stage from its extension)
# into <binary dir>/shaders/<name>.inc and puts that directory on <target>'s private include path.
function(gpu2d_compile_shaders target)
  if(NOT GLSLC)
    message(FATAL_ERROR
      "glslc is needed to compile the shaders. Install it with `sudo dnf install glslc` "
      "(Fedora) or `sudo apt install glslc` (Debian/Ubuntu)")
  endif()
  set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
  set(outputs)
  foreach(source IN LISTS ARGN)
    get_filename_component(source "${source}" ABSOLUTE)
    get_filename_component(name "${source}" NAME)
    get_filename_component(source_dir "${source}" DIRECTORY)
    file(GLOB shared CONFIGURE_DEPENDS "${source_dir}/*.glsl")
    set(output "${output_dir}/${name}.inc")
    add_custom_command(OUTPUT "${output}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
      COMMAND "${GLSLC}" -mfmt=num -o "${output}" "${source}"
      DEPENDS "${source}" ${shared}
      COMMENT "Compiling ${name} to SPIR-V")
    list(APPEND outputs "${output}")
  endforeach()
  target_sources(${target} PRIVATE ${outputs})
  target_include_directories(${target} PRIVATE "${output_dir}")
endfunction()
