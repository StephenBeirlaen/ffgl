add_library(ffgl-plugin-dmx-playback STATIC)
add_library(ffgl::plugin::dmx-playback ALIAS ffgl-plugin-dmx-playback)
target_sources(ffgl-plugin-dmx-playback PRIVATE DmxPlayback.h DmxPlayback.cpp)
target_link_libraries(ffgl-plugin-dmx-playback PRIVATE ffgl::sdk)

install(
    TARGETS     ffgl-plugin-dmx-playback
    EXPORT      ffgl--plugin-dmx-playback-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
