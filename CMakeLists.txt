cmake_minimum_required(VERSION 3.21)
project(andong-viewer VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if (MSVC)
  add_compile_options(/MP)
  # /Zc:__cplusplus 없이는 C++20 을 줘도 __cplusplus 가 199711 로 보고된다(ka-hgis 실측).
  add_compile_options(/Zc:__cplusplus /utf-8 /permissive-)
  add_compile_options(/W4 /external:anglebrackets /external:W0)
  # MSVC c2.dll 내부 오류를 피하려고 LTO 는 끈다(ka-hgis 와 같은 이유).
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
endif()

option(ANDONG_BUILD_TESTS "Build tests" ON)

if (EXISTS "C:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "C:/OSGeo4W")
elseif (EXISTS "D:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "D:/OSGeo4W")
elseif (EXISTS "A:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "A:/OSGeo4W")
endif()
set(OSGEO4W_ROOT "${OSGEO4W_ROOT_DEFAULT}" CACHE PATH "OSGeo4W root")
set(QGIS_PREFIX_PATH "${OSGEO4W_ROOT}/apps/qgis-dev" CACHE PATH "QGIS prefix")
list(PREPEND CMAKE_PREFIX_PATH "${OSGEO4W_ROOT}/apps/Qt6" "${OSGEO4W_ROOT}/apps/Qt6/lib/cmake")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml Network Svg Test)

set(AV_INCLUDES
  ${CMAKE_SOURCE_DIR}/src
  ${QGIS_PREFIX_PATH}/include
  ${OSGEO4W_ROOT}/apps/gdal-dev/include
  ${OSGEO4W_ROOT}/include
)
set(AV_LIBDIRS
  ${QGIS_PREFIX_PATH}/lib
  ${OSGEO4W_ROOT}/apps/gdal-dev/lib
  ${OSGEO4W_ROOT}/lib
)
set(AV_DEFS KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")

# 안동 전용 코어. ka-hgis 의 ka_core 를 통째로 끌어오지 않는다.
add_library(andong_core STATIC
  src/core/AndongPack.cpp
  src/core/AndongPack.h
  src/core/LayerOps.cpp
  src/core/LayerOps.h
)
target_include_directories(andong_core PUBLIC ${AV_INCLUDES})
target_link_directories(andong_core PUBLIC ${AV_LIBDIRS})
target_link_libraries(andong_core PUBLIC
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::Network Qt6::Svg qgis_core qgis_gui gdal_i)
target_compile_definitions(andong_core PUBLIC ${AV_DEFS})

add_executable(andong-viewer WIN32
  src/AndongMain.cpp
  src/AndongWindow.cpp
  src/AndongWindow.h
)
target_link_libraries(andong-viewer PRIVATE andong_core)
if(MSVC)
  target_compile_options(andong-viewer PRIVATE "$<$<CONFIG:Release>:/Zi>" "$<$<CONFIG:Release>:/FS>")
  target_link_options(andong-viewer PRIVATE
    "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
endif()

# 실행 폴더에 자료를 깔아 둔다. 위성 mbtiles 는 있을 때만 복사한다(288MB).
add_custom_command(TARGET andong-viewer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:andong-viewer>/data/andong"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:andong-viewer>/data/theme"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/andong/andong_city.geojson"
    "$<TARGET_FILE_DIR:andong-viewer>/data/andong/andong_city.geojson"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/theme/andong.qss"
    "$<TARGET_FILE_DIR:andong-viewer>/data/theme/andong.qss"
)
if(EXISTS "${CMAKE_SOURCE_DIR}/data/andong/andong_mark.png")
  add_custom_command(TARGET andong-viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/data/andong/andong_mark.png"
      "$<TARGET_FILE_DIR:andong-viewer>/data/andong/andong_mark.png")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/data/andong/satellite.mbtiles")
  add_custom_command(TARGET andong-viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/data/andong/satellite.mbtiles"
      "$<TARGET_FILE_DIR:andong-viewer>/data/andong/satellite.mbtiles")
endif()

if (ANDONG_BUILD_TESTS)
  enable_testing()
  add_executable(andong_tests tests/test_andong.cpp)
  set_target_properties(andong_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(andong_tests PRIVATE andong_core Qt6::Test)
  add_test(NAME andong_pack COMMAND andong_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  # ctest 는 OSGeo4W PATH 없이 돌면 0xc0000135(DLL 없음)로 죽는다.
  if (WIN32)
    set(_av_path
      "${QGIS_PREFIX_PATH}/bin" "${OSGEO4W_ROOT}/apps/Qt6/bin"
      "${OSGEO4W_ROOT}/apps/gdal-dev/bin" "${OSGEO4W_ROOT}/apps/pdal-dev/bin"
      "${OSGEO4W_ROOT}/bin")
    string(JOIN "\;" _av_path_esc ${_av_path})
    set_tests_properties(andong_pack PROPERTIES
      ENVIRONMENT "PATH=${_av_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen")
  endif()
endif()
