# CMake generated Testfile for 
# Source directory: /Users/caq/src/aethercli-main
# Build directory: /Users/caq/src/aethercli-main/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_tests "/Users/caq/src/aethercli-main/build/aethercli_tests")
set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "/Users/caq/src/aethercli-main/CMakeLists.txt;103;add_test;/Users/caq/src/aethercli-main/CMakeLists.txt;0;")
subdirs("_deps/json-build")
subdirs("_deps/httplib-build")
subdirs("_deps/catch2-build")
