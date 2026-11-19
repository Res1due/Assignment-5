add_library(a5lib A5Lib.cpp)

add_executable(vcall VCall.cpp)
target_link_libraries(vcall PRIVATE
        a5lib
        ${SVF_LIB}
        ${LLVM_LIB}
        )
set_target_properties(vcall PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})