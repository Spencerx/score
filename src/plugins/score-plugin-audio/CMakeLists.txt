project(score_plugin_audio LANGUAGES CXX)

score_common_setup()

# Files & main target
set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Model.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Presenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/View.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Factory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/JackInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/ALSAInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/ALSAMiniAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/ALSAPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/ASIOPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/CoreAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/CoreAudioPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/DummyInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/GenericMiniAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/GenericPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/MMEPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/PipeWireInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/PortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/SDLInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/WASAPIPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/WDMKSPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioPreviewExecutor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioTick.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_audio.hpp"
    )
set(SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Model.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/View.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/ALSAInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/ALSAPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/GenericPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/JackInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/PipeWireInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioPreviewExecutor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioTick.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_audio.cpp"
)


add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

if(OSSIA_PROTOCOL_AUDIO)
    target_sources(${PROJECT_NAME} PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioDevice.hpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioDevice.cpp"
    )
endif()

target_link_libraries(${PROJECT_NAME}
        PRIVATE
          ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
          score_lib_base score_lib_device score_lib_state score_plugin_scenario score_plugin_deviceexplorer
          ossia
)

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND NOT WIN32)
  include(deps/zita-alsa-pcmi)
  if(TARGET zita-alsa-pcmi)
    target_link_libraries(${PROJECT_NAME} PRIVATE "$<BUILD_INTERFACE:zita-alsa-pcmi>" "$<BUILD_INTERFACE:kfr>")
    list(APPEND SCORE_FEATURES_LIST alsa_pcm)
  endif()
endif()

if(OSSIA_ENABLE_PORTAUDIO)
  target_link_libraries(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:PortAudio::PortAudio>)
  list(APPEND SCORE_FEATURES_LIST portaudio)
endif()

if(OSSIA_ENABLE_JACK)
  target_link_libraries(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:jack::jack>)
  list(APPEND SCORE_FEATURES_LIST jack)
endif()

if(OSSIA_ENABLE_PIPEWIRE)
  target_link_libraries(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:pipewire::pipewire>)
  list(APPEND SCORE_FEATURES_LIST pipewire)
endif()

if(OSSIA_ENABLE_SDL)
  target_link_libraries(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:ossia::sdl2>)
  list(APPEND SCORE_FEATURES_LIST sdl)
endif()

if(LINUX OR BSD OR APPLE)
  target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${OSSIA_3RDPARTY_FOLDER}/miniaudio")
  if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework AudioUnit" "$<BUILD_INTERFACE:kfr>")
  endif()
endif()
setup_score_plugin(${PROJECT_NAME})
