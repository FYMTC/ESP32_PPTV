# main/ui/CMakeLists.txt

file(GLOB UI_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/access/*.c"
)
idf_component_register(
    SRCS ${UI_SRC}
    INCLUDE_DIRS "."
    REQUIRES lvgl
)