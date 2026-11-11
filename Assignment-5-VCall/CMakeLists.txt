add_library(a5lib A5Lib.cpp)

add_executable(vcall VCall.cpp)
target_link_libraries(vcall PRIVATE
        ${SVF_LIB}
        ${LLVM_LIB}
        a5lib
        )
set_target_properties(vcall PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})