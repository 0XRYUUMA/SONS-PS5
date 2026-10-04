add_library(libSceIme SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceIme PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceIme PRIVATE libSceKeyboard)
configure_windows_unwind(libSceIme)
