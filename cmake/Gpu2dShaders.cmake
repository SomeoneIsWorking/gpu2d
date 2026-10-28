# GLSL compiled to SPIR-V by glslc at build time, written as comma-separated words for a C++ array
# initialiser (`#include "name.stage.inc"`). Shared *.glsl files beside the sources are #included
# with GL_GOOGLE_include_directive and rebuild every shader that may use them.
#
# With GPU2D_SHADER_MSL (on for Apple targets, where SDL GPU is Metal) the same SPIR-V is also
# translated to Metal Shading Language by SDL_shadercross, which binds resources the way SDL's
# Metal backend expects, and written as a C++ raw string literal (`#include "name.stage.msl.inc"`);
# the target gets GPU2D_SHADER_MSL defined. MSL entry points are `main0`.

# Script mode, run by the build: wraps the MSL file MSL as a raw string literal in OUTPUT.
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  file(READ "${MSL}" text)
  file(WRITE "${OUTPUT}" "R\"gpu2d_msl(${text})gpu2d_msl\"\n")
  return()
endif()

find_program(GLSLC glslc)
option(GPU2D_SHADER_MSL "Also translate shaders to MSL for SDL GPU's Metal backend" ${APPLE})
if(GPU2D_SHADER_MSL)
  find_program(GPU2D_SHADERCROSS shadercross)
endif()
set(GPU2D_SHADERS_MODULE "${CMAKE_CURRENT_LIST_FILE}")

# gpu2d_compile_shaders(<target> <source>...): compiles each source (stage from its extension)
# into <binary dir>/shaders/<name>.inc (and <name>.msl.inc with GPU2D_SHADER_MSL) and puts that
# directory on <target>'s private include path.
function(gpu2d_compile_shaders target)
  if(NOT GLSLC)
    message(FATAL_ERROR
      "glslc is needed to compile the shaders. Install it with `sudo dnf install glslc` "
      "(Fedora), `sudo apt install glslc` (Debian/Ubuntu) or `brew install shaderc` (macOS)")
  endif()
  if(GPU2D_SHADER_MSL AND NOT GPU2D_SHADERCROSS)
    message(FATAL_ERROR
      "SDL_shadercross is needed to translate the shaders to MSL. Build the pinned one with "
      "`cmake -DPREFIX=<dir> -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ProvisionShadercross.cmake` "
      "and configure with -DGPU2D_SHADERCROSS=<dir>/bin/shadercross")
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
    if(GPU2D_SHADER_MSL)
      get_filename_component(extension "${source}" LAST_EXT)
      if(extension STREQUAL ".vert")
        set(stage vertex)
      elseif(extension STREQUAL ".frag")
        set(stage fragment)
      else()
        message(FATAL_ERROR "${name}: MSL translation knows .vert and .frag shaders only")
      endif()
      set(msl "${output_dir}/${name}.msl")
      add_custom_command(OUTPUT "${msl}.inc" "${msl}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
        COMMAND "${GLSLC}" -o "${output_dir}/${name}.spv" "${source}"
        COMMAND "${GPU2D_SHADERCROSS}" "${output_dir}/${name}.spv" --source SPIRV --dest MSL
          --stage ${stage} --output "${msl}"
        COMMAND "${CMAKE_COMMAND}" "-DMSL=${msl}" "-DOUTPUT=${msl}.inc"
          -P "${GPU2D_SHADERS_MODULE}"
        DEPENDS "${source}" ${shared} "${GPU2D_SHADERS_MODULE}"
        COMMENT "Translating ${name} to MSL")
      list(APPEND outputs "${msl}.inc")
    endif()
  endforeach()
  target_sources(${target} PRIVATE ${outputs})
  target_include_directories(${target} PRIVATE "${output_dir}")
  if(GPU2D_SHADER_MSL)
    target_compile_definitions(${target} PRIVATE GPU2D_SHADER_MSL)
  endif()
endfunction()
