file(GLOB_RECURSE WHIP_ASSISTANT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(Whip-Assistant STATIC ${WHIP_ASSISTANT_SOURCES})
add_library(whip::assistant ALIAS Whip-Assistant)

whip_set_target_output(Whip-Assistant)
whip_configure_native_target(Whip-Assistant)

target_include_directories(Whip-Assistant PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/Whip/include"
    "${CMAKE_SOURCE_DIR}/Whip/vendor")

target_link_libraries(Whip-Assistant PUBLIC whip::glm)
target_link_libraries(Whip-Assistant PRIVATE whip::gemini_cpp)

if(WHP_ENABLE_GEMINI_CPP)
    target_compile_definitions(Whip-Assistant PRIVATE WHP_ENABLE_GEMINI_CPP=1)
endif()

if(WIN32)
    target_link_libraries(Whip-Assistant PRIVATE winhttp)
endif()

whip_suppress_external_link_warnings(Whip-Assistant)
