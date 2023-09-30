#=================== SDL3 ===================

set(SDL3_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL)
add_subdirectory(${SDL3_DIR})

#=================== nativefiledialog ===================

add_subdirectory(nativefiledialog)

#=================== faust ===================

# See [the docs](https://faustdoc.grame.fr/manual/embedding/#using-libfaust-with-the-llvm-backend) for general help.

set(MSVC_STATIC OFF CACHE STRING "Use static runtimes with MSVC" FORCE)
set(INCLUDE_STATIC OFF CACHE STRING "Include static library" FORCE)
set(INCLUDE_EXECUTABLE OFF CACHE STRING "Include runtime executable" FORCE)
set(INCLUDE_DYNAMIC ON CACHE STRING "Include dynamic library" FORCE)
set(INCLUDE_OSC OFF CACHE STRING "Include Faust OSC library" FORCE)
set(INCLUDE_HTTP OFF CACHE STRING "Include Faust HTTPD library" FORCE)

set(ASMJS_BACKEND OFF CACHE STRING "Include ASMJS backend" FORCE)
set(C_BACKEND OFF CACHE STRING "Include C backend" FORCE)
set(CPP_BACKEND OFF CACHE STRING "Include CPP backend" FORCE)
set(FIR_BACKEND OFF CACHE STRING "Include FIR backend" FORCE)
set(INTERP_BACKEND OFF CACHE STRING "Include INTERPRETER backend" FORCE)
set(JAVA_BACKEND OFF CACHE STRING "Include JAVA backend" FORCE)
set(JS_BACKEND OFF CACHE STRING "Include JAVASCRIPT backend" FORCE)
set(LLVM_BACKEND COMPILER DYNAMIC CACHE STRING "Include LLVM backend" FORCE)
set(OLDCPP_BACKEND OFF CACHE STRING "Include old CPP backend" FORCE)
set(RUST_BACKEND OFF CACHE STRING "Include RUST backend" FORCE)
set(WASM_BACKEND OFF CACHE STRING "Include WASM backend" FORCE)

include_directories(faust/compiler/generator/c)
include_directories(faust/compiler/generator/cpp)
add_subdirectory(faust/build EXCLUDE_FROM_ALL)
target_compile_definitions(dynamiclib PUBLIC LLVM_BUILD_UNIVERSAL=1)

#=================== CDT ===================

add_subdirectory(CDT/CDT)

#=================== tetgen ===================

add_subdirectory(tetgen)
