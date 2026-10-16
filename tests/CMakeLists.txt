# Fetches Catch2 and builds the unit test suite. Gated behind
# PYTHON_AGENT_BUILD_TESTS in the parent CMakeLists, so a normal
# configure/build/package never pays for cloning and compiling it.
include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
  GIT_SHALLOW    TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_executable(python_agent_tests test_python_interpreter.cpp)
target_link_libraries(python_agent_tests PRIVATE cppy3::cppy3 Mads::Mads Catch2::Catch2WithMain)
target_compile_definitions(python_agent_tests PRIVATE
  PYTHON_AGENT_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

include(Catch)
catch_discover_tests(python_agent_tests)
