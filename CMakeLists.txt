cmake_minimum_required(VERSION 3.16)
project(WeildScanner VERSION 0.2 LANGUAGES CXX)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Keep the existing local OpenCV installation as a default; allow a toolchain override.
if(NOT OpenCV_DIR AND EXISTS "J:/cv-qt/opencv_5.0.0/build/install/OpenCVConfig.cmake")
    set(OpenCV_DIR "J:/cv-qt/opencv_5.0.0/build/install" CACHE PATH "OpenCV package directory")
endif()
find_package(OpenCV REQUIRED)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
get_filename_component(PARENT_DIR "${CMAKE_SOURCE_DIR}" DIRECTORY)
add_compile_definitions(OPENCV_LINK_AS_SHARED NOMINMAX
    PATH_NAME_CAMERA_PARAMS_FILE="${PARENT_DIR}/settings/camera.xml"
    PATH_NAME_FACE_MODELS_DIR="${PARENT_DIR}/settings")

# Reusable module, still in the existing apcamera directory and Qt/OpenCV stack.
# add_library(weild_module STATIC
#     apcamera/weilddetect.h apcamera/weilddetect.cpp apcamera/weildparameters.cpp
#     apcamera/weildtracker.h apcamera/weildtracker.cpp
#     apcamera/weildsynthetic.h apcamera/weildsynthetic.cpp
#     apcamera/frameweildprocessing.h apcamera/frameweildprocessing.cpp
#     apcamera/weildprocessingpool.h apcamera/weildprocessingpool.cpp
#     apcamera/weildrobotoutput.h apcamera/weildrobotoutput.cpp
#     apcamera/cameraopencv.h apcamera/cameraopencv.cpp)
# target_include_directories(weild_module PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
# target_link_libraries(weild_module PUBLIC Qt${QT_VERSION_MAJOR}::Widgets ${OpenCV_LIBS})

set(PROJECT_SOURCES
    main.cpp mainwindow.cpp mainwindow.h mainwindow.ui appstructs.cpp appstructs.h
    apcamera/easyimageview.cpp apcamera/easyimageview.h
    weild_processor/weildprofileview.cpp weild_processor/weildprofileview.h
    weild_processor/weilddetect.h weild_processor/weilddetect.cpp weild_processor/weildparameters.cpp
    weild_processor/weildtracker.h weild_processor/weildtracker.cpp
    weild_processor/weildsynthetic.h weild_processor/weildsynthetic.cpp
    weild_processor/frameweildprocessing.h weild_processor/frameweildprocessing.cpp
    weild_processor/weildprocessingpool.h weild_processor/weildprocessingpool.cpp
    weild_processor/weildrobotoutput.h weild_processor/weildrobotoutput.cpp
    apcamera/cameraopencv.h apcamera/cameraopencv.cpp
    logger/logger.cpp logger/logger.h)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(WeildScanner MANUAL_FINALIZATION ${PROJECT_SOURCES})
else()
    add_executable(WeildScanner ${PROJECT_SOURCES})
endif()
target_include_directories(WeildScanner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/apcamera
    ${CMAKE_CURRENT_SOURCE_DIR}/logger
    ${OpenCV_INCLUDE_DIRS})
target_link_libraries(WeildScanner PRIVATE Qt${QT_VERSION_MAJOR}::Widgets ${OpenCV_LIBS})
set_target_properties(WeildScanner PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)

# Copy the OpenCV DLLs for this installation, without encoding its version in names.
file(GLOB OPENCV_RUNTIME_DLLS "${OpenCV_DIR}/x64/mingw/bin/libopencv_*.dll")
if(OPENCV_RUNTIME_DLLS)
    add_custom_command(TARGET WeildScanner POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OPENCV_RUNTIME_DLLS} $<TARGET_FILE_DIR:WeildScanner>)
endif()
#include(CTest)
#if(BUILD_TESTING)
    #add_executable(weild_tests tests/weild_tests.cpp)
    #target_link_libraries(weild_tests PRIVATE weild_module)
    #add_test(NAME weild_module_tests COMMAND weild_tests)
    #set_tests_properties(weild_module_tests PROPERTIES TIMEOUT 30)
#endif()
include(GNUInstallDirs)
install(TARGETS WeildScanner BUNDLE DESTINATION . LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(WeildScanner)
endif()
