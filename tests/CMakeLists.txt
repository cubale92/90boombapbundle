# One offline test runner per plug-in. Each compiles the plug-in sources directly
# (no plug-in wrapper), renders audio through every factory preset and can save
# a screenshot of the editor.
foreach(P IN LISTS ERA_PLUGINS)
    juce_add_console_app(Test${P} PRODUCT_NAME "Test${P}")
    target_sources(Test${P} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/TestMain.cpp
        ${ERA_SHARED_SOURCES}
        ${PROJECT_SOURCE_DIR}/plugins/${P}/${P}Processor.cpp
        ${PROJECT_SOURCE_DIR}/plugins/${P}/${P}Editor.cpp
        ${PROJECT_SOURCE_DIR}/plugins/${P}/${P}Presets.cpp)
    target_include_directories(Test${P} PRIVATE ${PROJECT_SOURCE_DIR}/shared ${PROJECT_SOURCE_DIR}/plugins/${P})
    target_compile_definitions(Test${P} PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 ERA_VERSION_STRING="${PROJECT_VERSION}" ERA_PLUGIN_NAME="${P}")
    target_link_libraries(Test${P} PRIVATE juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endforeach()
