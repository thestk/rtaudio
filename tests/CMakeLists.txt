include_directories(..)
if (WIN32)
    include_directories(../include)
endif (WIN32)

# Copy the runtime DLLs next to a test executable so it runs from the build
# tree. Defined on every platform (it is called unconditionally below) and a
# no-op outside Windows.
function(copy_runtime_deps target)
    if (WIN32)
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}>
                $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

list(GET LIB_TARGETS 0 LIBRTAUDIO)

add_executable(audioprobe audioprobe.cpp)
target_link_libraries(audioprobe ${LIBRTAUDIO} ${LINKLIBS})
copy_runtime_deps(audioprobe) # just one to copy the dll in the tests/ dir

add_executable(playsaw playsaw.cpp)
target_link_libraries(playsaw ${LIBRTAUDIO} ${LINKLIBS})

add_executable(playraw playraw.cpp)
target_link_libraries(playraw ${LIBRTAUDIO} ${LINKLIBS})

add_executable(record record.cpp)
target_link_libraries(record ${LIBRTAUDIO} ${LINKLIBS})

add_executable(duplex duplex.cpp)
target_link_libraries(duplex ${LIBRTAUDIO} ${LINKLIBS})

add_executable(apinames apinames.cpp)
target_link_libraries(apinames ${LIBRTAUDIO} ${LINKLIBS})

add_executable(testall testall.cpp)
target_link_libraries(testall ${LIBRTAUDIO} ${LINKLIBS})

add_executable(teststops teststops.cpp)
target_link_libraries(teststops ${LIBRTAUDIO} ${LINKLIBS})

add_test(NAME apinames COMMAND apinames)
