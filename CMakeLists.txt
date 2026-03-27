cmake_minimum_required(VERSION 3.14)
project(coup_cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Core library ──────────────────────────────────────────────────────────────
add_library(coup_lib STATIC src/coup.cpp)
target_include_directories(coup_lib PUBLIC include)
target_compile_options(coup_lib PRIVATE -Wall -Wextra -O2)

# ── GoogleTest via FetchContent ───────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)

set(INSTALL_GTEST        OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ── C++ test suite ────────────────────────────────────────────────────────────
enable_testing()

add_executable(coup_tests tests/coup_test.cpp)
target_link_libraries(coup_tests
    PRIVATE
        coup_lib
        GTest::gtest_main
        GTest::gmock
)
target_compile_options(coup_tests PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(coup_tests)

# ── Python bindings (optional – requires pybind11) ────────────────────────────
# Build:
#   pip install pybind11
#   cmake -DBUILD_PYTHON_BINDINGS=ON ..
#   cmake --build .
# Test:
#   ctest -R py::          (run only Python tests)
#   ctest                  (run everything)
#
option(BUILD_PYTHON_BINDINGS "Build pybind11 Python module" OFF)

if(BUILD_PYTHON_BINDINGS)

    # ── 1. Build the .so ──────────────────────────────────────────────────────
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    find_package(pybind11 REQUIRED)

    pybind11_add_module(coup_py src/bindings.cpp)
    target_link_libraries(coup_py PRIVATE coup_lib)
    target_compile_options(coup_py PRIVATE -O2)

    install(TARGETS coup_py LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python)

    # ── 2. Register individual pytest functions with CTest ────────────────────
    #
    # WHY NOT execute_process(pytest --collect-only)?
    #   --collect-only requires importing the module, which requires the .so to
    #   exist.  At configure time the .so has not been built yet, so collection
    #   fails silently and no tests are registered.
    #
    # SOLUTION: parse `def test_*` directly from the source file at configure
    # time — no import needed.  Each function gets its own CTest entry that
    # runs `pytest path/to/file.py::test_name -v`, which is the exact node-ID
    # syntax pytest understands (unlike `-k` which does substring matching and
    # can match multiple tests).

    set(PYTEST_FILE "${CMAKE_SOURCE_DIR}/tests/python/test_bindings.py")

    # Read the file and extract every `def test_<name>(` line.
    file(READ "${PYTEST_FILE}" _pytest_src)
    string(REGEX MATCHALL "def (test_[A-Za-z0-9_]+)\\(" _matches "${_pytest_src}")

    if(NOT _matches)
        message(WARNING "No test functions found in ${PYTEST_FILE}. "
                        "Check the file path and that functions follow the `def test_*` convention.")
    endif()

    foreach(_match IN LISTS _matches)
        # Extract just the function name from "def test_foo("
        string(REGEX REPLACE "def (test_[A-Za-z0-9_]+)\\(" "\\1" _func "${_match}")

        # CTest name:  py::test_foo  (mirrors GoogleTest's Suite.TestName style)
        # Command:     pytest tests/test_bindings.py::test_foo -v
        # PYTHONPATH points at the build dir so `import coup_py` resolves the .so
        add_test(
            NAME    "py::${_func}"
            COMMAND ${Python3_EXECUTABLE} -m pytest
                        "${PYTEST_FILE}::${_func}"
                        -v --tb=short
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        )
        set_tests_properties("py::${_func}" PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}"
            # If the .so hasn't been built yet the test will fail with ImportError
            # rather than silently passing — this is intentional.
        )
    endforeach()

    message(STATUS "Registered ${CMAKE_MATCH_COUNT} Python tests with CTest "
                   "(prefix py::, PYTHONPATH=${CMAKE_BINARY_DIR})")

endif()
