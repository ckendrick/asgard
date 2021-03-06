cmake_minimum_required (VERSION 3.13) # remove cray check below when >= 3.16

project (asgard
  VERSION 0.3.0
  LANGUAGES CXX
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "CrayLinuxEnvironment")
  if (CMAKE_VERSION VERSION_LESS "3.16")
    message (WARNING "To use Cray LibSci instead of building OpenBLAS, you must use CMake >= 3.16.")
  endif ()
endif ()

###############################################################################
## Set up the compiler and general global build options
###############################################################################
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

message (STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

# add compiler flags we always want to use
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -O3 -g")

# set up possible commandline input variable defaults (override with -D)
option (ASGARD_BUILD_TESTS "Build tests for asgard" ON)
option (ASGARD_BUILD_OPENBLAS "Download and build our own OpenBLAS" OFF)
option (ASGARD_BUILD_HDF5 "Download and build our own HDF5/HighFive" OFF)
option (ASGARD_HDF5_PATH "optional location of lib/ containing HDF5" "")
option (ASGARD_BUILD_PROFILE_DEPS "Download and build our own tools (e.g.  graphviz)" OFF)
option (ASGARD_LAPACK_PATH "optional location of include/ and lib/ containing LAPACK" "")
option (ASGARD_BLAS_PATH "optional location of include/ and lib/ containing BLAS" "")
option (ASGARD_PROFILE_GPROF "enable profiling support for using gprof" "")
option (ASGARD_PROFILE_XRAY "enable profiling support for using LLVM XRAY" "")
option (ASGARD_PROFILE_GPERF_CPU "enable cpu profiling support using gperftools" "")
option (ASGARD_PROFILE_GPERF_MEM "enable memory profiling support using gperftools" "")
option (ASGARD_PROFILE_PERF "enable profiling support for using linux perf" "")
option (ASGARD_PROFILE_VALGRIND "enable profiling support for using valgrind" "")
option (ASGARD_GRAPHVIZ_PATH "optional location of bin/ containing dot executable" "")
option (ASGARD_IO_HIGHFIVE "Use the HighFive HDF5 header library for I/O" OFF)
option (ASGARD_USE_CUDA "Optional CUDA support for asgard" OFF)
option (ASGARD_USE_OPENMP "Optional openMP support for asgard" ON)
option (ASGARD_USE_MPI "Optional distributed computing support for asgard" OFF)
option (ASGARD_USE_DOUBLE_PREC "enable or disable double-precision floating point" ON)
set (ASGARD_TESTING_RANKS "0" CACHE STRING "Override default number of ranks to use for testing")

if (NOT ASGARD_BLAS_PATH AND ASGARD_LAPACK_PATH)
  set (ASGARD_BLAS_PATH ${ASGARD_LAPACK_PATH})
endif()
if (NOT ASGARD_LAPACK_PATH AND ASGARD_BLAS_PATH)
  set (ASGARD_LAPACK_PATH ${ASGARD_BLAS_PATH})
endif()
###############################################################################
## Pull in external support as needed
###############################################################################


# sets OpenBLAS_PATH and LINALG_LIBS
include (${CMAKE_SOURCE_DIR}/contrib/contrib.cmake)
# sets PROFILE_DEPS and PROFILE_LIBS
include (${CMAKE_SOURCE_DIR}/profiling/profiling.cmake)
# sets HighFive_FOUND
include (${CMAKE_SOURCE_DIR}/contrib/io.cmake)

string(FIND "${BLAS_LIBRARIES}" "mkl" FOUND_MKL)
set(ASGARD_USE_MKL FALSE)
if(FOUND_MKL GREATER_EQUAL 0)
  set(ASGARD_USE_MKL TRUE)
endif()


if(ASGARD_USE_OPENMP)
  find_package(OpenMP REQUIRED)  
  if(ASGARD_USE_MKL)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	message(FATAL_ERROR 
	  "MKL and apple clang++ require conflicting 
	  openmp flags - build configuration not supported")
    endif()
  endif()
endif()

if(ASGARD_USE_CUDA)
  find_package(CUDA 9.0 REQUIRED) # eventually want to remove this - how to set min version with enable_language?
  include_directories(${CUDA_INCLUDE_DIRS})
  enable_language(CUDA) 
  set (CMAKE_CUDA_STANDARD 14)
  set (CMAKE_CUDA_STANDARD_REQUIRED ON)
endif()

# build component to interface with Ed's kronmult lib
add_library(kronmult_cuda SHARED src/device/kronmult_cuda.cpp)
if(ASGARD_USE_CUDA)
  set_source_files_properties( src/device/kronmult_cuda.cpp PROPERTIES LANGUAGE CUDA ) # no .cu extension
  set_target_properties( kronmult_cuda PROPERTIES CUDA_ARCHITECTURES OFF)
  set_target_properties( kronmult_cuda PROPERTIES COMPILE_FLAGS "-arch sm_70 -g -lineinfo --ptxas-options=-O3")
  set_target_properties( kronmult_cuda PROPERTIES LINK_FLAGS "-Wl,-rpath,${KRON_PATH}")
endif()
target_include_directories (kronmult_cuda PRIVATE ${KRON_PATH} ${CMAKE_BINARY_DIR})

if(ASGARD_USE_MKL)
  if(ASGARD_USE_CUDA)
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --compiler-options -fopenmp")
  else()
    target_compile_options (kronmult_cuda PRIVATE "-fopenmp") # CMAKE doesn't handle MKL openmp link properly
  endif()
endif()

if(ASGARD_USE_MPI)
  find_package(MPI REQUIRED)
  include_directories(${MPI_CXX_INCLUDE_DIRS})
endif()

###############################################################################
## Building asgard
#
# KEEP EVERYTHING LISTED IN ALPHABETICAL ORDER
#
# to add a component:
# 1) define the component in the components list
# 2) define any link dependencies if needed
###############################################################################

# define the components and how to build and link the components together
set (components
  basis
  batch
  boundary_conditions
  coefficients
  distribution
  elements
  fast_math
  kronmult
  lib_dispatch
  matlab_utilities
  pde
  permutations
  program_options
  quadrature
  solver
  tensors
  time_advance
  timer
  transformations
)
if (ASGARD_IO_HIGHFIVE)
  list (APPEND components io)
endif ()

foreach (component IN LISTS components)
  add_library (${component} src/${component}.cpp)
  target_include_directories (${component} PRIVATE ${CMAKE_BINARY_DIR})
  if(ASGARD_USE_MKL)
    target_compile_options (${component} PRIVATE "-fopenmp") # CMAKE doesn't handle MKL openmp link properly
  endif()
endforeach ()

if (build_hdf5)
  add_dependencies (io hdf5-ext)
endif ()

if (build_OpenBLAS)
  add_dependencies (tensors openblas-ext)
endif ()

if (build_kron)
  add_dependencies (kronmult_cuda kronmult-ext)
endif ()

target_link_libraries (basis PRIVATE matlab_utilities quadrature tensors)

target_link_libraries (batch PRIVATE lib_dispatch coefficients elements pde tensors)

target_link_libraries (boundary_conditions PRIVATE coefficients elements transformations
                       matlab_utilities fast_math )

if (ASGARD_USE_OPENMP AND NOT ASGARD_USE_MKL)
   target_link_libraries(batch PRIVATE OpenMP::OpenMP_CXX)
endif ()

target_link_libraries (coefficients
  PRIVATE pde matlab_utilities quadrature tensors transformations)

target_link_libraries(distribution PRIVATE elements lib_dispatch)
if (ASGARD_USE_MPI)
  target_link_libraries(distribution PRIVATE MPI::MPI_CXX)
endif ()

target_link_libraries (elements
  PRIVATE permutations program_options tensors)

target_link_libraries (fast_math
  PRIVATE lib_dispatch tensors)

target_link_libraries(kronmult PRIVATE batch elements kronmult_cuda timer)

if (ASGARD_USE_OPENMP AND NOT ASGARD_USE_MKL)
   target_link_libraries(kronmult PRIVATE OpenMP::OpenMP_CXX)
endif ()

if (ASGARD_USE_CUDA) 
   target_link_libraries(kronmult_cuda PRIVATE ${KRON_LIB})
else ()
   target_link_libraries(kronmult_cuda PRIVATE "-Wl,-rpath,${KRON_PATH} ${KRON_LIB}")
endif ()

if (ASGARD_USE_OPENMP AND NOT ASGARD_USE_MKL)
   target_link_libraries(kronmult_cuda PRIVATE OpenMP::OpenMP_CXX)
endif ()

if (ASGARD_IO_HIGHFIVE)
  target_link_libraries (io PUBLIC highfive tensors PRIVATE hdf5)
endif ()

target_link_libraries (lib_dispatch PRIVATE ${LINALG_LIBS})
if (ASGARD_USE_CUDA)
  target_link_libraries(lib_dispatch PRIVATE ${CUDA_LIBRARIES}
					     ${CUDA_CUBLAS_LIBRARIES})
endif()
if (ASGARD_USE_OPENMP AND NOT ASGARD_USE_MKL)
   target_link_libraries(lib_dispatch PRIVATE OpenMP::OpenMP_CXX)
endif ()


target_link_libraries (matlab_utilities PUBLIC tensors)

target_link_libraries (pde PRIVATE basis matlab_utilities tensors)

target_link_libraries (permutations PRIVATE matlab_utilities tensors)

target_link_libraries (program_options PRIVATE clara distribution)

target_link_libraries (quadrature PRIVATE matlab_utilities tensors)

target_link_libraries (solver PRIVATE distribution fast_math lib_dispatch tensors)

target_link_libraries (tensors PRIVATE lib_dispatch)
if (ASGARD_USE_CUDA)
  target_link_libraries (tensors PRIVATE ${CUDA_LIBRARIES})
endif ()

target_link_libraries (time_advance PRIVATE batch fast_math pde kronmult solver tensors timer boundary_conditions
                                    INTERFACE elements)

target_link_libraries (transformations
  PRIVATE basis batch distribution matlab_utilities pde program_options
  quadrature tensors)

# define the main application and its linking
add_executable (asgard src/main.cpp)

# link in components needed directly by main
set (main_app_link_deps
  batch
  boundary_conditions
  coefficients
  distribution
  elements
  matlab_utilities
  pde
  program_options
  quadrature
  tensors
  time_advance
  timer
  transformations
)
if (ASGARD_IO_HIGHFIVE)
  list (APPEND main_app_link_deps io)
endif()

if (ASGARD_USE_MPI)
  list (APPEND main_app_link_deps PRIVATE MPI::MPI_CXX)
endif()

foreach (link_dep IN LISTS main_app_link_deps)
  target_link_libraries (asgard PRIVATE ${link_dep})
endforeach ()

# build/link anything needed for profiling if requested
if (PROFILE_DEPS)
  add_dependencies (asgard ${PROFILE_DEPS})
endif ()
target_link_libraries (asgard PRIVATE ${PROFILE_LIBS})

# Generate header file from src/build_info.hpp.in
configure_file(
  src/build_info.hpp.in
  ${CMAKE_BINARY_DIR}/build_info.hpp
)
# Include the generated build_info.hpp
target_include_directories (asgard PRIVATE ${CMAKE_BINARY_DIR})

###############################################################################
## Testing asgard
#
###############################################################################
if (ASGARD_BUILD_TESTS)
  enable_testing ()

  # Define ctest tests and their executables
  add_library (tests_general testing/tests_general.cpp)
  target_link_libraries (tests_general PUBLIC Catch PRIVATE pde program_options )
  target_include_directories(tests_general PRIVATE ${CMAKE_BINARY_DIR})
  foreach (component IN LISTS components)
    add_executable (${component}-tests src/${component}_tests.cpp)
    target_include_directories (${component}-tests PRIVATE ${CMAKE_SOURCE_DIR}/testing)
    target_include_directories (${component}-tests PRIVATE ${CMAKE_BINARY_DIR})
    target_link_libraries (${component}-tests PRIVATE ${component} tests_general)
    
    # FIXME
    if (ASGARD_IO_HIGHFIVE AND "${component}" STREQUAL "io")
      target_link_libraries (io-tests PRIVATE highfive hdf5 tensors)
    endif ()

    if (ASGARD_USE_MPI)
      target_link_libraries (${component}-tests PRIVATE ${component} MPI::MPI_CXX)
      if ("${component}" STREQUAL "distribution" OR
          "${component}" STREQUAL "time_advance")

      set(test_ranks "4")
      if (ASGARD_USE_CUDA)
         set(test_ranks "1")
      endif ()
      if (${ASGARD_TESTING_RANKS})
         set(test_ranks ${ASGARD_TESTING_RANKS})
      endif ()
      add_test (NAME ${component}-test-mpi
                COMMAND mpirun -n ${test_ranks} ${component}-tests
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )
      endif()
    endif ()

    add_test (NAME ${component}-test
              COMMAND ${component}-tests
              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )
  endforeach ()

  add_executable (kronmult_cuda-tests src/device/kronmult_cuda_tests.cpp)
  target_include_directories (kronmult_cuda-tests PRIVATE ${CMAKE_SOURCE_DIR}/testing)
  target_include_directories (kronmult_cuda-tests PRIVATE ${CMAKE_BINARY_DIR})
  target_link_libraries (kronmult_cuda-tests PRIVATE coefficients kronmult_cuda tests_general)
  add_test (NAME kronmult_cuda-test
            COMMAND kronmult_cuda-tests
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )
else ()
  message (WARNING "warning: tests will not be built.")
endif ()
