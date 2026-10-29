# Builds SDL_shadercross's CLI (SPIR-V to MSL only, no DXC) and the SPIRV-Cross it links into
# PREFIX, from pinned and checksummed source archives; SDL_shadercross publishes no releases.
#   cmake -DPREFIX=<dir> -P cmake/ProvisionShadercross.cmake
# then configure gpu2d with -DGPU2D_SHADERCROSS=<dir>/bin/shadercross (or <dir>/bin on PATH).
# Needs a C/C++ compiler, CMake, Ninja and SDL3's CMake package; the tool keeps an rpath to that
# SDL3's folder. An existing PREFIX is reused.

cmake_minimum_required(VERSION 3.22)

set(shadercross_commit "1ff05bec573988a98ef9e0260b4da44f512b8367")
set(shadercross_sha256 "60aae35ff71bd1d5f85b363e9d077a86bb27dd36333c42685e56c2fd541174d2")
# The SPIRV-Cross commit SDL_shadercross's external/ submodule names at that revision.
set(spirv_cross_commit "1a6169566c73d3da552748fc372fe2bbb856e46e")
set(spirv_cross_sha256 "0f295b214b164e42a1d21537c8da7b44569806c16220dda9798558edfaacd11e")

if(NOT PREFIX)
  message(FATAL_ERROR "Pass the install prefix: cmake -DPREFIX=<dir> -P ${CMAKE_SCRIPT_MODE_FILE}")
endif()
get_filename_component(PREFIX "${PREFIX}" ABSOLUTE)
if(EXISTS "${PREFIX}/bin/shadercross")
  message(STATUS "shadercross is already in ${PREFIX}/bin")
  return()
endif()
set(work "${PREFIX}/work")

function(fetch name url sha256)
  set(archive "${work}/download/${name}.tar.gz")
  file(DOWNLOAD "${url}" "${archive}" EXPECTED_HASH "SHA256=${sha256}" TLS_VERIFY ON)
  file(REMOVE_RECURSE "${work}/src/${name}")
  file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${work}/src/${name}")
  file(GLOB top "${work}/src/${name}/*")
  set(${name}_source "${top}" PARENT_SCOPE)
endfunction()

function(build source binary)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${binary}" -G Ninja
      -DCMAKE_BUILD_TYPE=Release "-DCMAKE_INSTALL_PREFIX=${PREFIX}" "-DCMAKE_PREFIX_PATH=${PREFIX}"
      -DCMAKE_INSTALL_LIBDIR=lib "-DCMAKE_INSTALL_RPATH=${PREFIX}/lib"
      -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=ON ${ARGN}
    COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${CMAKE_COMMAND}" --build "${binary}" --target install
    COMMAND_ERROR_IS_FATAL ANY)
endfunction()

fetch(spirv_cross
  "https://github.com/KhronosGroup/SPIRV-Cross/archive/${spirv_cross_commit}.tar.gz"
  "${spirv_cross_sha256}")
fetch(shadercross
  "https://github.com/libsdl-org/SDL_shadercross/archive/${shadercross_commit}.tar.gz"
  "${shadercross_sha256}")

build("${spirv_cross_source}" "${work}/build/spirv_cross"
  -DSPIRV_CROSS_SHARED=ON -DSPIRV_CROSS_STATIC=ON -DSPIRV_CROSS_CLI=ON
  -DSPIRV_CROSS_ENABLE_TESTS=OFF -DSPIRV_CROSS_ENABLE_HLSL=ON -DSPIRV_CROSS_ENABLE_MSL=ON)
build("${shadercross_source}" "${work}/build/shadercross"
  -DSDLSHADERCROSS_DXC=OFF -DSDLSHADERCROSS_VENDORED=OFF -DSDLSHADERCROSS_SPIRVCROSS_SHARED=ON
  -DSDLSHADERCROSS_CLI=ON -DSDLSHADERCROSS_INSTALL=ON)
file(REMOVE_RECURSE "${work}")
message(STATUS "shadercross installed in ${PREFIX}/bin")
