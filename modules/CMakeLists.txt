cmake_minimum_required(VERSION 3.28)

add_library(rtaudio_modules)

set(RTAUDIO_MODULES
    RtAudio.cppm
)


if(NOT COMMAND configure_cpp_module_target)
    function(configure_cpp_module_target target)
        target_sources(${target} PUBLIC FILE_SET CXX_MODULES FILES ${RTAUDIO_MODULES})
    endfunction()
endif()

if(RTAUDIO_USE_NAMESPACE)
    target_compile_definitions(rtaudio_modules PRIVATE RTAUDIO_USE_NAMESPACE)
endif()

configure_cpp_module_target(rtaudio_modules)

target_link_libraries(rtaudio_modules
    PUBLIC
    rtaudio
)

target_include_directories(rtaudio_modules
    PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_compile_features(rtaudio_modules PUBLIC cxx_std_20)
