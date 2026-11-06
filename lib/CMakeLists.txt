add_library(bgprocess SHARED src/bgprocess_common.c)

if(WIN32)
    target_sources(bgprocess PRIVATE src/bgprocess_win.c)
else()
    target_sources(bgprocess PRIVATE src/bgprocess_posix.c)
endif()

#видно ток с BGPROCESS_EXPORT
set_target_properties(bgprocess PROPERTIES C_VISIBILITY_PRESET hidden)

include(GenerateExportHeader)
generate_export_header(bgprocess)

target_include_directories(bgprocess
    PUBLIC  include ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE src)
