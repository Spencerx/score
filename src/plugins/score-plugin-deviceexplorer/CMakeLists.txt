project(score_plugin_deviceexplorer LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets Network Xml)

# Files & main target
### Plugin ###
set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DefaultProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/AddAddress.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/LoadDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/DeviceExplorerCommandFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Remove.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/RemoveNodes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Remove/RemoveAddress.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/ReplaceDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Update/UpdateAddressSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Update/UpdateDeviceSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/AddressSettingsFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressBoolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressImpulseSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressNoneSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressNumericSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressStringSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressListSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressVecSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPluginFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/NodeUpdateProxy.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Column.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerFilterProxyModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerMimeTypes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ExplorerModelProvider.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/AddressItemModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ExplorationWorker.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ExplorationWorkerWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ListeningManager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Widgets/AddressEditDialog.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Widgets/DeviceEditDialog.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/DefaultListeningHandlerFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/DefaultListeningHandler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandlerFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandlerFactoryList.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Panel/DeviceExplorerPanelDelegate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Panel/DeviceExplorerPanelFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Settings/ExplorerModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Settings/ExplorerPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Settings/ExplorerView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Settings/ExplorerFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DeviceList.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DeviceLogging.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/ApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_deviceexplorer.hpp"

)


set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPluginSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/AddAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Add/LoadDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Remove/RemoveAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Update/UpdateAddressSettings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Update/UpdateDeviceSettings.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/DeviceExplorerCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/Remove.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Commands/ReplaceDevice.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/AddressSettingsFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressListSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressBoolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressNoneSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressImpulseSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Common/AddressSettings/Widgets/AddressStringSettingsWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Panel/DeviceExplorerPanelDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Panel/DeviceExplorerPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/DeviceDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DocumentPlugin/NodeUpdateProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerFilterProxyModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ExplorationWorker.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Widgets/DeviceEditDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/Widgets/AddressEditDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/AddressItemModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/DeviceExplorerWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Explorer/ListeningManager.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandlerFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/ListeningHandlerFactoryList.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/DefaultListeningHandler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Listening/DefaultListeningHandlerFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Settings/ExplorerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Settings/ExplorerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Settings/ExplorerView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/DeviceList.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_deviceexplorer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/ApplicationPlugin.cpp"

#"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/ModelTest/dynamictreemodel.cpp"
#"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/ModelTest/modeltest.cpp"
#"${CMAKE_CURRENT_SOURCE_DIR}/Explorer/ModelTest/tst_modeltest.cpp"

)

add_library(${PROJECT_NAME}
      ${SRCS} ${HDRS}
      ${EXPLORER_RESOURCES_RCC} )
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(score_plugin_deviceexplorer
  PUBLIC
    score_lib_device
)

if(OSSIA_DNSSD)
  target_sources(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/ZeroConf/ZeroconfBrowser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Explorer/Widgets/ZeroConf/ZeroconfBrowser.cpp")

  target_link_libraries(${PROJECT_NAME} PRIVATE Servus_iface)
  target_include_directories(${PROJECT_NAME} PRIVATE ${OSSIA_3RDPARTY_FOLDER}/Servus)
  list(APPEND SCORE_FEATURES_LIST dnssd)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_PREFIX}::Core ${QT_PREFIX}::Network)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

setup_score_plugin(score_plugin_deviceexplorer)
#setup_score_tests(Tests)

