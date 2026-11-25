add_executable(counter main.c)

if(WIN32)
    target_sources(counter PRIVATE plat_win.c)
else()
    target_sources(counter PRIVATE plat_posix.c)
    find_package(Threads REQUIRED)
    target_link_libraries(counter PRIVATE Threads::Threads)
    #на старом линуксе shm_open лежит в librt
    if(NOT APPLE)
        target_link_libraries(counter PRIVATE rt)
    endif()
endif()

#копии запускаются библиотекой из 1 лабы
target_link_libraries(counter PRIVATE bgprocess)
