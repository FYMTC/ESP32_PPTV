set(EXTRA_COMPONENT_DIRS ${PROJECT_SOURCE_DIR}/components)
idf_component_register(SRCS "tests.cpp" "main.cpp" "init_tasks.cpp" "system_low_power_design.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl pthread thread_manager driver system lvgl_app ui
                    )