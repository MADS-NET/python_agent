#   ____        _   _                                          _   
#  |  _ \ _   _| |_| |__   ___  _ __     __ _  __ _  ___ _ __ | |_ 
#  | |_) | | | | __| '_ \ / _ \| '_ \   / _` |/ _` |/ _ \ '_ \| __|
#  |  __/| |_| | |_| | | | (_) | | | | | (_| | (_| |  __/ | | | |_ 
#  |_|    \__, |\__|_| |_|\___/|_| |_|  \__,_|\__, |\___|_| |_|\__|
#         |___/                               |___/                
# A MADS agent implementing a Python interpreter
cmake_minimum_required(VERSION 3.20)
project(python_agent VERSION 1.2.0 LANGUAGES CXX)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)

# PROJECT SETTINGS #############################################################
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

# Get MADS prefix
execute_process(
  COMMAND mads -p
  OUTPUT_VARIABLE MADS_PREFIX
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "MADS_PREFIX: ${MADS_PREFIX}")
include_directories(${MADS_PREFIX}/include)
link_directories(${MADS_PREFIX}/lib)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE ${MADS_PREFIX})
endif()
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}") 


# DEPENDENCIES #################################################################
include(FetchContent)
FetchContent_Declare(cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG        v3.1.1
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(toml
  GIT_REPOSITORY https://github.com/pbosetti/tomlplusplus.git
  GIT_TAG        050c7a0
  GIT_SHALLOW    TRUE
)

set(BUILD_TESTING OFF CACHE INTERNAL "")
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(cppy3 
  GIT_REPOSITORY https://github.com/pbosetti/cppy3.git
  GIT_TAG        HEAD
  GIT_SHALLOW    TRUE
)


FetchContent_Declare(rang
  GIT_REPOSITORY https://github.com/pbosetti/rang.git
  GIT_TAG        HEAD
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(cxxopts toml json cppy3 rang)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/.venv/Scripts/python.exe")
  # Prefer the repository-local virtual environment on Windows.
  set(Python3_FIND_VIRTUALENV FIRST)
  set(Python3_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/.venv/Scripts/python.exe")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
include_directories(
  ${plugin_SOURCE_DIR}/src
  ${cppy3_SOURCE_DIR}
  ${Python3_INCLUDE_DIRS}
  ${toml_SOURCE_DIR}/include
  ${rang_SOURCE_DIR}/include
  ${cxxopts_SOURCE_DIR}/include
  ${json_SOURCE_DIR}/include
)
if(WIN32)
  # Prefer fetched headers over any versions shipped in MADS include paths.
  include_directories(BEFORE
    ${toml_SOURCE_DIR}/include
    ${cxxopts_SOURCE_DIR}/include
  )
endif()


# BUILD SETTINGS ###############################################################
if (APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
  include_directories(/opt/homebrew/include)
  link_directories(/opt/homebrew/lib)
else()
  set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib;/usr/local/lib")
endif()

add_executable(python_agent ${SRC_DIR}/main/python.cpp)
if(WIN32)
  # Prevent legacy winsock.h declarations from conflicting with winsock2.h.
  target_compile_definitions(python_agent PRIVATE _WINSOCKAPI_)
  link_directories(${MADS_PREFIX}/lib ${MADS_PREFIX}/bin)
  target_link_libraries(python_agent PRIVATE cppy3 Python3::Python Ws2_32 bcrypt Secur32 Crypt32 Dnsapi IPHLPAPI MadsCore)
else()
  target_link_libraries(python_agent PRIVATE cppy3 MadsCore)
endif()
set_target_properties(python_agent PROPERTIES
  OUTPUT_NAME mads-python
)
list(APPEND TARGET_LIST python_agent)


# INSTALL ######################################################################
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE ${USR_DIR})
endif()
install(TARGETS ${TARGET_LIST}
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)
