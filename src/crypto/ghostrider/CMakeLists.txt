cmake_minimum_required(VERSION 3.10)
project(GhostRider)

set(SPHLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common/sphlib)

set(HEADERS
    ${SPHLIB_DIR}/sph_types.h
    ${SPHLIB_DIR}/sph_blake.h
    ${SPHLIB_DIR}/sph_bmw.h
    ${SPHLIB_DIR}/sph_cubehash.h
    ${SPHLIB_DIR}/sph_echo.h
    ${SPHLIB_DIR}/sph_fugue.h
    ${SPHLIB_DIR}/sph_groestl.h
    ${SPHLIB_DIR}/sph_hamsi.h
    ${SPHLIB_DIR}/sph_jh.h
    ${SPHLIB_DIR}/sph_keccak.h
    ${SPHLIB_DIR}/sph_luffa.h
    ${SPHLIB_DIR}/sph_sha2.h
    ${SPHLIB_DIR}/sph_shabal.h
    ${SPHLIB_DIR}/sph_shavite.h
    ${SPHLIB_DIR}/sph_simd.h
    ${SPHLIB_DIR}/sph_skein.h
    ${SPHLIB_DIR}/sph_whirlpool.h
    ghostrider.h
)

set(SOURCES
    ${SPHLIB_DIR}/sph_blake.c
    ${SPHLIB_DIR}/sph_bmw.c
    ${SPHLIB_DIR}/sph_cubehash.c
    ${SPHLIB_DIR}/sph_echo.c
    ${SPHLIB_DIR}/sph_fugue.c
    ${SPHLIB_DIR}/sph_groestl.c
    ${SPHLIB_DIR}/sph_hamsi.c
    ${SPHLIB_DIR}/sph_jh.c
    ${SPHLIB_DIR}/sph_keccak.c
    ${SPHLIB_DIR}/sph_luffa.c
    ${SPHLIB_DIR}/sph_shabal.c
    ${SPHLIB_DIR}/sph_shavite.c
    ${SPHLIB_DIR}/sph_simd.c
    ${SPHLIB_DIR}/sph_sha2.c
    ${SPHLIB_DIR}/sph_skein.c
    ${SPHLIB_DIR}/sph_whirlpool.c
    ghostrider.cpp
)

if (CMAKE_C_COMPILER_ID MATCHES MSVC)
    set_source_files_properties(${SPHLIB_DIR}/sph_blake.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_bmw.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_cubehash.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_echo.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_fugue.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_groestl.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_hamsi.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_jh.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_keccak.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_luffa.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_shabal.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_shavite.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_simd.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_sha2.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_skein.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_whirlpool.c PROPERTIES COMPILE_FLAGS_RELEASE "/O1 /Oi /Os")
elseif (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
    set_source_files_properties(${SPHLIB_DIR}/sph_blake.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_bmw.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_cubehash.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_echo.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_fugue.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_groestl.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_hamsi.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_jh.c PROPERTIES COMPILE_FLAGS "-Os -fno-tree-vrp")
    set_source_files_properties(${SPHLIB_DIR}/sph_keccak.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_luffa.c PROPERTIES COMPILE_FLAGS "-Os -Wno-unused-const-variable")
    set_source_files_properties(${SPHLIB_DIR}/sph_shabal.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_shavite.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_simd.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_sha2.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_skein.c PROPERTIES COMPILE_FLAGS "-Os")
    set_source_files_properties(${SPHLIB_DIR}/sph_whirlpool.c PROPERTIES COMPILE_FLAGS "-Os")
endif()

include_directories(${SPHLIB_DIR})
include_directories(.)
include_directories(../..)
include_directories(${UV_INCLUDE_DIR})

add_library(ghostrider STATIC ${HEADERS} ${SOURCES})
