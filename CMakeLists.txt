#   ____        _   _                                          _   
#  |  _ \ _   _| |_| |__   ___  _ __     __ _  __ _  ___ _ __ | |_ 
#  | |_) | | | | __| '_ \ / _ \| '_ \   / _` |/ _` |/ _ \ '_ \| __|
#  |  __/| |_| | |_| | | | (_) | | | | | (_| | (_| |  __/ | | | |_ 
#  |_|    \__, |\__|_| |_|\___/|_| |_|  \__,_|\__, |\___|_| |_|\__|
#         |___/                               |___/                
# A MADS agent implementing a Python interpreter
cmake_minimum_required(VERSION 3.28)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(python_agent VERSION 2.3.0 LANGUAGES CXX)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)

# PROJECT SETTINGS #############################################################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

# Get MADS prefix
set(MADS_ROOT "" CACHE PATH "MADS installation prefix")
if(MADS_ROOT)
  set(MADS_PREFIX "${MADS_ROOT}")
else()
  execute_process(
    COMMAND mads -p
    OUTPUT_VARIABLE MADS_PREFIX
    RESULT_VARIABLE MADS_PREFIX_RESULT
    ERROR_VARIABLE MADS_PREFIX_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT MADS_PREFIX_RESULT STREQUAL "0" OR MADS_PREFIX STREQUAL "")
    message(FATAL_ERROR
      "Could not determine MADS prefix. Set MADS_ROOT or ensure `mads -p` works. "
      "${MADS_PREFIX_ERROR}")
  endif()
endif()
file(TO_CMAKE_PATH "${MADS_PREFIX}" MADS_PREFIX)
message(STATUS "MADS_PREFIX: ${MADS_PREFIX}")
# Only look into MADS_PREFIX, so that we never pick up a different MADS install
# than the one `mads -p` (or MADS_ROOT) points to.
find_package(Mads 2.5 CONFIG REQUIRED PATHS "${MADS_PREFIX}" NO_DEFAULT_PATH)
message(STATUS "Mads version: ${Mads_VERSION} (${Mads_DIR})")
# nlohmann/json, toml++, cxxopts and rang are taken from the MADS install, so
# that this agent is compiled against the very same headers as libMadsCore.
get_target_property(MADS_INCLUDE_DIR Mads::Mads INTERFACE_INCLUDE_DIRECTORIES)
foreach(_mads_hdr agent.hpp nlohmann/json.hpp toml++/toml.hpp cxxopts.hpp rang.hpp)
  if(NOT EXISTS "${MADS_INCLUDE_DIR}/${_mads_hdr}")
    message(FATAL_ERROR "MADS install in ${MADS_INCLUDE_DIR} does not provide ${_mads_hdr}")
  endif()
endforeach()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE "${MADS_PREFIX}")
endif()
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")


# BUNDLED PYTHON OPTION ########################################################
# When ON, link against a relocatable python-build-standalone distribution and
# ship it inside the package, so the result is self-contained and version-pinned
# (used by CI releases). When OFF (default), behave exactly as before, using the
# system/venv Python found by find_package(Python3).
option(PYTHON_AGENT_BUNDLE_PYTHON
  "Use a relocatable python-build-standalone CPython instead of system/venv Python3"
  OFF)
set(PYTHON_BUNDLE_DIR "" CACHE PATH
  "Path to an extracted, pip-populated python-build-standalone install_only distribution \
(the directory containing bin/python3 on Unix or python.exe on Windows). \
Required when PYTHON_AGENT_BUNDLE_PYTHON=ON.")


# DEPENDENCIES #################################################################
include(FetchContent)
FetchContent_Declare(cppy3
  GIT_REPOSITORY https://github.com/pbosetti/cppy3.git
  GIT_TAG        2.0.0
  GIT_SHALLOW    TRUE
  EXCLUDE_FROM_ALL
)

# When bundling, steer find_package(Python3) to the python-build-standalone
# distribution. These MUST be set before FetchContent_MakeAvailable(cppy3)
# below: cppy3's own CMakeLists.txt calls find_package(Python3 ...) during
# add_subdirectory, and FindPython3 re-resolves per call (no cross-call cache),
# so the hints must already be visible in this parent scope at that point.
if(PYTHON_AGENT_BUNDLE_PYTHON)
  if(NOT PYTHON_BUNDLE_DIR)
    message(FATAL_ERROR
      "PYTHON_AGENT_BUNDLE_PYTHON=ON requires -DPYTHON_BUNDLE_DIR=<path>, "
      "pointing at an extracted python-build-standalone install_only(_stripped) "
      "distribution with the desired packages (e.g. numpy) already pip-installed.")
  endif()
  file(TO_CMAKE_PATH "${PYTHON_BUNDLE_DIR}" PYTHON_BUNDLE_DIR)
  if(WIN32)
    set(_pbs_landmark "${PYTHON_BUNDLE_DIR}/python.exe")
    set(Python3_EXECUTABLE "${PYTHON_BUNDLE_DIR}/python.exe")
  else()
    set(_pbs_landmark "${PYTHON_BUNDLE_DIR}/bin/python3")
    set(Python3_EXECUTABLE "${PYTHON_BUNDLE_DIR}/bin/python3")
  endif()
  if(NOT EXISTS "${_pbs_landmark}")
    message(FATAL_ERROR
      "PYTHON_BUNDLE_DIR='${PYTHON_BUNDLE_DIR}' does not look like a "
      "python-build-standalone distribution: expected '${_pbs_landmark}'.")
  endif()
  set(Python3_ROOT_DIR "${PYTHON_BUNDLE_DIR}")
  set(Python3_FIND_STRATEGY LOCATION)
  set(Python3_FIND_FRAMEWORK NEVER)   # harmless off-Apple
  set(Python3_FIND_REGISTRY NEVER)    # harmless off-Windows
  message(STATUS "PYTHON_AGENT_BUNDLE_PYTHON: ON, using ${PYTHON_BUNDLE_DIR}")
endif()

FetchContent_MakeAvailable(cppy3)

if(NOT PYTHON_AGENT_BUNDLE_PYTHON
    AND WIN32 AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/.venv/Scripts/python.exe")
  # Prefer the repository-local virtual environment on Windows.
  set(Python3_FIND_VIRTUALENV FIRST)
  set(Python3_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/.venv/Scripts/python.exe")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

if(PYTHON_AGENT_BUNDLE_PYTHON)
  # Ensure no system/venv Python leaked into the resolution.
  get_filename_component(_found_root "${Python3_EXECUTABLE}" DIRECTORY)
  if(NOT WIN32)
    get_filename_component(_found_root "${_found_root}" DIRECTORY)  # strip /bin
  endif()
  get_filename_component(_found_root "${_found_root}" REALPATH)
  get_filename_component(_expected_root "${PYTHON_BUNDLE_DIR}" REALPATH)
  if(NOT _found_root STREQUAL _expected_root)
    message(FATAL_ERROR
      "find_package(Python3) resolved '${Python3_EXECUTABLE}', outside the "
      "requested bundle '${PYTHON_BUNDLE_DIR}'. A system/venv Python leaked in.")
  endif()
endif()
include_directories(
  ${SRC_DIR}
  ${Python3_INCLUDE_DIRS}
)

if(UNIX)
  add_compile_options(-Wno-deprecated-literal-operator)
endif()

# BUILD SETTINGS ###############################################################
if (APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
  if(PYTHON_AGENT_BUNDLE_PYTHON)
    list(APPEND CMAKE_INSTALL_RPATH "@executable_path/../python-runtime/lib")
  endif()
  include_directories(/opt/homebrew/include)
  link_directories(/opt/homebrew/lib)
else()
  set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib;/usr/local/lib")
  if(PYTHON_AGENT_BUNDLE_PYTHON)
    list(APPEND CMAKE_INSTALL_RPATH "\$ORIGIN/../python-runtime/lib")
  endif()
endif()

add_executable(python_agent ${SRC_DIR}/main/python.cpp)
if(PYTHON_AGENT_BUNDLE_PYTHON)
  target_compile_definitions(python_agent PRIVATE PYTHON_AGENT_BUNDLED)
endif()
if(WIN32)
  # Prevent legacy winsock.h declarations from conflicting with winsock2.h.
  target_compile_definitions(python_agent PRIVATE _WINSOCKAPI_)
  target_link_libraries(python_agent PRIVATE cppy3::cppy3 Python3::Python Ws2_32 bcrypt Secur32 Crypt32 Dnsapi IPHLPAPI Mads::Mads)
else()
  target_link_libraries(python_agent PRIVATE cppy3::cppy3 Mads::Mads)
endif()
set_target_properties(python_agent PROPERTIES
  OUTPUT_NAME mads-python
  CXX_SCAN_FOR_MODULES OFF
)
list(APPEND TARGET_LIST python_agent)


# TESTS #########################################################################
# Opt-in: enable with -DPYTHON_AGENT_BUILD_TESTS=ON, then run `ctest` from the
# build directory. See tests/test_python_interpreter.cpp for why each
# TEST_CASE has to run as its own process.
option(PYTHON_AGENT_BUILD_TESTS "Build the unit test suite (fetches Catch2)" OFF)
if(PYTHON_AGENT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()


# INSTALL ######################################################################
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE ${USR_DIR})
endif()
install(TARGETS ${TARGET_LIST}
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)

if(PYTHON_AGENT_BUNDLE_PYTHON)
  # Ship the whole self-contained CPython distribution next to the executable.
  install(DIRECTORY "${PYTHON_BUNDLE_DIR}/"
    DESTINATION python-runtime
    USE_SOURCE_PERMISSIONS
  )
  if(WIN32)
    # mads-python.exe statically imports these at OS-loader time, before any of
    # our code (and thus any PYTHONHOME/PATH manipulation) runs. The Windows
    # loader searches the executable's own directory first, so they must sit in
    # bin/ next to mads-python.exe.
    file(GLOB PYTHON_AGENT_BUNDLE_CORE_DLLS
      "${PYTHON_BUNDLE_DIR}/python3.dll"
      "${PYTHON_BUNDLE_DIR}/python3??.dll"
      "${PYTHON_BUNDLE_DIR}/vcruntime*.dll"
    )
    install(FILES ${PYTHON_AGENT_BUNDLE_CORE_DLLS} DESTINATION bin)
  endif()
endif()



# PACKAGE #######################################################################
string(TOLOWER "${CMAKE_SYSTEM_NAME}" PACKAGE_OS_NAME)
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
  list(LENGTH CMAKE_OSX_ARCHITECTURES PACKAGE_OSX_ARCHITECTURES_COUNT)
  if(PACKAGE_OSX_ARCHITECTURES_COUNT GREATER 1)
    set(PACKAGE_ARCH_NAME "universal")
  else()
    set(PACKAGE_ARCH_NAME "${CMAKE_OSX_ARCHITECTURES}")
  endif()
else()
  set(PACKAGE_ARCH_NAME "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(PACKAGE_ARCH_NAME STREQUAL "")
  set(PACKAGE_ARCH_NAME "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()
if(PACKAGE_ARCH_NAME STREQUAL "")
  set(PACKAGE_ARCH_NAME "unknown")
endif()
string(TOLOWER "${PACKAGE_ARCH_NAME}" PACKAGE_ARCH_NAME)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "MADS")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_FILE_NAME
  "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${PACKAGE_OS_NAME}-${PACKAGE_ARCH_NAME}")
include(CPack)
