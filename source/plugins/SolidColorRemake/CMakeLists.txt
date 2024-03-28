add_library(ffgl-plugin-solid-color-remake STATIC)
add_library(ffgl::plugin::solid-color-remake ALIAS ffgl-plugin-solid-color-remake)
target_sources(ffgl-plugin-solid-color-remake PRIVATE SolidColorRemake.h SolidColorRemake.cpp)
target_link_libraries(ffgl-plugin-solid-color-remake PRIVATE ffgl::sdk)

install(
    TARGETS     ffgl-plugin-solid-color-remake
    EXPORT      ffgl--plugin-solid-color-remake-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
