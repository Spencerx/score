set(CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${SCORE_SDK}"
    "${SCORE_SDK}/lib"
    "${SCORE_SDK}/lib/cmake"
    "${SCORE_SDK}/lib/cmake/ossia"
    "${SCORE_SDK}/lib/cmake/score"
)

# Useful variables
set(SCORE_AVND_SOURCE_DIR "${SCORE_SDK}/lib/cmake/score")

# Find the clang version
file(GLOB CLANG_RESOURCE_DIR "${SCORE_SDK}/lib/clang/*")
list(GET CLANG_RESOURCE_DIR 0 CLANG_RESOURCE_DIR)
string(STRIP "${CLANG_RESOURCE_DIR}" CLANG_RESOURCE_DIR)

# Find the Qt version
file(GLOB QTCORE_FILES LIST_DIRECTORIES true "${SCORE_SDK}/include/qt/QtCore/*")

foreach(dir ${QTCORE_FILES})
  if(IS_DIRECTORY "${dir}")
    cmake_path(GET dir FILENAME QT_INCLUDE_VERSION)
  else()
    continue()
  endif()
endforeach()

# Create all the targets for the score plug-ins
foreach(_lib ${SCORE_PLUGINS})
  string(TOLOWER "${_lib}" _lib_lc)
  add_library(score_${_lib_lc} INTERFACE)
  target_compile_definitions(score_${_lib_lc} INTERFACE SCORE_${_lib})
endforeach()

if(IS_DIRECTORY "${SCORE_SDK}/include/x86_64-unknown-linux-gnu/c++/v1")
  target_compile_options(score_lib_base INTERFACE
    "SHELL:-Xclang -internal-isystem -Xclang ${SCORE_SDK}/include/x86_64-unknown-linux-gnu/c++/v1"
  )
endif()

set(CXX_VERSION_FLAG cxx_std_23)
set(CMAKE_CXX_STANDARD 23)

target_compile_options(score_lib_base INTERFACE
  -std=c++23
  -fPIC
)
target_compile_options(score_lib_base INTERFACE
  -nostdinc
  -nostdlib
  "SHELL:-Xclang -internal-isystem -Xclang ${SCORE_SDK}/include/c++/v1/"
  "SHELL:-Xclang -internal-isystem -Xclang ${SCORE_SDK}/include"
  "SHELL:-Xclang -internal-isystem -Xclang ${CLANG_RESOURCE_DIR}/include"
  "SHELL:-resource-dir ${CLANG_RESOURCE_DIR}"
)

if(APPLE)
  target_compile_options(score_lib_base INTERFACE
    -nostdinc++
  )
  target_include_directories(score_lib_base SYSTEM INTERFACE
    "${SCORE_SDK}/include/macos-sdks"
  )
endif()

# Only export plugin_instance
if(WIN32)
  target_link_libraries(score_lib_base INTERFACE
    "${SCORE_SDK}/lib/libscore.dll.a"
    ws2_32
  )
elseif(APPLE)
  target_link_libraries(score_lib_base INTERFACE
    -nostdlib++
    -Wl,-exported_symbol,_plugin_instance
  )

else()
  file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/retained-symbols.txt"
    CONTENT "plugin_instance"
  )
  target_link_libraries(score_lib_base INTERFACE
    -nostdlib++
    -Wl,--retain-symbols-file="${CMAKE_CURRENT_BINARY_DIR}/retained-symbols.txt"
  )
endif()

target_include_directories(score_lib_base SYSTEM INTERFACE
  "${SCORE_SDK}/include/score"
  "${SCORE_SDK}/include/qt"
  "${SCORE_SDK}/include/qt/QtCore"
  "${SCORE_SDK}/include/qt/QtCore/${QT_INCLUDE_VERSION}"
  "${SCORE_SDK}/include/qt/QtCore/${QT_INCLUDE_VERSION}/QtCore"
  "${SCORE_SDK}/include/qt/QtCore/${QT_INCLUDE_VERSION}/QtCore/private"
  "${SCORE_SDK}/include/qt/QtGui"
  "${SCORE_SDK}/include/qt/QtGui/${QT_INCLUDE_VERSION}"
  "${SCORE_SDK}/include/qt/QtGui/${QT_INCLUDE_VERSION}/QtGui"
  "${SCORE_SDK}/include/qt/QtGui/${QT_INCLUDE_VERSION}/QtGui/private"
  "${SCORE_SDK}/include/qt/QtWidgets"
  "${SCORE_SDK}/include/qt/QtWidgets/${QT_INCLUDE_VERSION}"
  "${SCORE_SDK}/include/qt/QtWidgets/${QT_INCLUDE_VERSION}/QtWidgets"
  "${SCORE_SDK}/include/qt/QtWidgets/${QT_INCLUDE_VERSION}/QtWidgets/private"
  "${SCORE_SDK}/include/qt/QtNetwork"
  "${SCORE_SDK}/include/qt/QtNetwork/${QT_INCLUDE_VERSION}"
  "${SCORE_SDK}/include/qt/QtNetwork/${QT_INCLUDE_VERSION}/QtNetwork"
  "${SCORE_SDK}/include/qt/QtNetwork/${QT_INCLUDE_VERSION}/QtNetwork/private"
  "${SCORE_SDK}/include/qt/QtQml"
  "${SCORE_SDK}/include/qt/QtQml/${QT_INCLUDE_VERSION}"
  "${SCORE_SDK}/include/qt/QtQml/${QT_INCLUDE_VERSION}/QtQml"
  "${SCORE_SDK}/include/qt/QtQml/${QT_INCLUDE_VERSION}/QtQml/private"
  "${SCORE_SDK}/include/qt/QtXml"
  "${SCORE_SDK}/include/qt/QtXml/${QT_INCLUDE_VERSION}"
  "${SCORE_SDK}/include/qt/QtXml/${QT_INCLUDE_VERSION}/QtXml"
  "${SCORE_SDK}/include/qt/QtXml/${QT_INCLUDE_VERSION}/QtXml/private"
  "${SCORE_SDK}/include/qt/QtWidgets"
  "${SCORE_SDK}/include/qt/QtWidgets/${QT_INCLUDE_VERSION}"
  "${SCORE_SDK}/include/qt/QtWidgets/${QT_INCLUDE_VERSION}/QtWidgets"
  "${SCORE_SDK}/include/qt/QtWidgets/${QT_INCLUDE_VERSION}/QtWidgets/private"
)

target_compile_definitions(score_lib_base INTERFACE
  BOOST_MATH_DISABLE_FLOAT128=1
  BOOST_ASIO_DISABLE_CONCEPTS=1
  BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
  BOOST_MULTI_INDEX_ENABLE_SAFE_MODE

  QT_NO_LINKED_LIST
  QT_NO_JAVA_STYLE_ITERATORS
  QT_NO_USING_NAMESPACE
  QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
  QT_USE_QSTRINGBUILDER

  QT_CORE_LIB
  QT_GUI_LIB
  QT_NETWORK_LIB
  QT_NO_KEYWORDS
  QT_QML_LIB
  QT_QUICK_LIB
  QT_SERIALPORT_LIB
  QT_STATICPLUGIN
  QT_SVG_LIB
  QT_WEBSOCKETS_LIB
  QT_WIDGETS_LIB
  QT_XML_LIB

  RAPIDJSON_HAS_STDSTRING=1
  # SCORE_DEBUG
  TINYSPLINE_DOUBLE_PRECISION
  SCORE_DYNAMIC_PLUGINS=1
  QT_STATIC=1

  SCORE_LIB_BASE
  SCORE_LIB_DEVICE
  SCORE_LIB_INSPECTOR
  SCORE_LIB_LOCALTREE
  SCORE_LIB_PROCESS
  SCORE_LIB_STATE
  SCORE_PLUGIN_AUDIO
  SCORE_PLUGIN_AUTOMATION
  SCORE_PLUGIN_AVND
  SCORE_PLUGIN_CURVE
  SCORE_PLUGIN_DATAFLOW
  SCORE_PLUGIN_DEVICEEXPLORER
  SCORE_PLUGIN_ENGINE
  SCORE_PLUGIN_GFX
  SCORE_PLUGIN_LIBRARY
  SCORE_PLUGIN_MEDIA
  SCORE_PLUGIN_SCENARIO
  SCORE_PLUGIN_TRANSPORT
)

function(score_common_setup)
  enable_testing()
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_AUTOUIC OFF)
  set(CMAKE_AUTOMOC OFF)
endfunction()

function(setup_score_plugin PluginName)
  set_target_properties(${PluginName} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/"
  )

  if(APPLE)
    set_target_properties(${PluginName} PROPERTIES
      SUFFIX .dylib
    )
  endif()

  if(WIN32)
    install(TARGETS "${PluginName}"
      RUNTIME DESTINATION .
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION imports
    )
  else()
    install(TARGETS "${PluginName}"
      LIBRARY DESTINATION .
    )
  endif()
endfunction()
