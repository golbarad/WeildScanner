cmake_minimum_required(VERSION 3.16)

project(WeildScanner VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(PARENT_DIR "${CMAKE_SOURCE_DIR}" DIRECTORY)
#путь к установкам камеры
set(CAMERA_PARAMS  "${PARENT_DIR}/settings/camera.xml")
add_compile_definitions(PATH_NAME_CAMERA_PARAMS_FILE="${CAMERA_PARAMS}")
set(DETECT_PARAMS "${PARENT_DIR}/settings/obj_params.xml")
add_compile_definitions(PATH_NAME_AUTOPILOT_PARAMS_FILE="${DETECT_PARAMS}")
add_compile_definitions(PATH_NAME_FACE_MODELS_DIR="${PARENT_DIR}/settings")

#Opencv собирается как динмаическая билиотека
add_definitions(-DOPENCV_LINK_AS_SHARED)

#путь к opencv
set(OPENCV_DIR "J:/cv-qt/opencv_5.0.0/build/install")
set(OPENCV_INCLUDE "${OPENCV_DIR}/include")
set(OPENCV_LIBS "${OPENCV_DIR}/x64/mingw/lib")

find_package(OpenCV REQUIRED)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES WeildScanner_ru_RU.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        appstructs.cpp
        appstructs.h
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(WeildScanner MANUAL_FINALIZATION
        ${PROJECT_SOURCES}        
        apcamera/cameraopencv.cpp
        apcamera/cameraopencv.h
        apcamera/easyimageview.cpp
        apcamera/easyimageview.h
        apcamera/frameprocessingpool.cpp
        apcamera/frameprocessingpool.h
        apcamera/frametapeprocessing.cpp
        apcamera/frametapeprocessing.h
        apcamera/frametapesetzeropos.cpp
        apcamera/frametapesetzeropos.h
        apcamera/navigationview.cpp
        apcamera/navigationview.h
        apcamera/faceprocessingpool.h
        apcamera/faceprocessingpool.cpp
        logger/logger.cpp
        logger/logger.h
        apcamera/faceprocessing.h
        apcamera/faceprocessing.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET WeildScanner APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(WeildScanner SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(WeildScanner
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()


target_include_directories(WeildScanner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/apcamera
    ${CMAKE_CURRENT_SOURCE_DIR}/logger
    ${OpenCV_INCLUDE_DIRS}
)

target_compile_definitions(WeildScanner PRIVATE OPENCV_LINK_AS_SHARED)

#find_package(OpenCV REQUIRED)
target_link_libraries(WeildScanner PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core ${OpenCV_LIBS}
)

file(GLOB OPENCV_RUNTIME_DLLS "${OPENCV_DIR}/x64/mingw/bin/libopencv_*.dll")
if(OPENCV_RUNTIME_DLLS)
    add_custom_command(TARGET WeildScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENCV_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:WeildScanner>
    )
endif()

file(GLOB FACE_MODEL_FILES "${PARENT_DIR}/settings/*.onnx")
if(FACE_MODEL_FILES)
    add_custom_command(TARGET WeildScanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FACE_MODEL_FILES}
            $<TARGET_FILE_DIR:WeildScanner>
    )
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.WeildScanner)
endif()
set_target_properties(WeildScanner PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS WeildScanner
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(WeildScanner)
endif()
