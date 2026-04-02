cmake_minimum_required(VERSION 3.10)

project(rknn_yolo11_demo)

set(LUCKFOX_SDK_PATH /home/linf1998/luckfox-pico)

link_directories(
    ${LUCKFOX_SDK_PATH}/media/mpp/release_mpp_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib
    ${LUCKFOX_SDK_PATH}/media/samples/example/common/lib
    ${LUCKFOX_SDK_PATH}/media/common_algorithm/common_algorithm/misc/lib/arm-rockchip830-linux-uclibcgnueabihf
    ${LUCKFOX_SDK_PATH}/media/iva/iva/librockiva/rockiva-rv1106-Linux/lib
    ${LUCKFOX_SDK_PATH}/media/iva/out/lib
    ${LUCKFOX_SDK_PATH}/media/out/lib
)

if(NOT DEFINED TARGET_SOC)
    set(TARGET_SOC "rv1106" CACHE STRING "Target SOC (rv1106, rv1103, rk1808, etc.)")
    message(STATUS "TARGET_SOC not set, defaulting to ${TARGET_SOC}")
endif()
message(STATUS "Building for TARGET_SOC: ${TARGET_SOC}")

# 设置OpenCV的配置路径（根据你找到的位置）
set(OpenCV_DIR
    ${CMAKE_CURRENT_LIST_DIR}/../../../3rdparty/opencv/opencv-linux-armhf/share/OpenCV
)

# 查找OpenCV - 使用opencv4
find_package(OpenCV REQUIRED)

if(OpenCV_FOUND)
    message(STATUS "Found OpenCV: ${OpenCV_VERSION}")
    message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
else()
    message(FATAL_ERROR "OpenCV not found!")
endif()

set(OpenCV_LIBS
    ${CMAKE_CURRENT_LIST_DIR}/../../../3rdparty/opencv/opencv-linux-armhf/lib/libopencv_core.a
    ${CMAKE_CURRENT_LIST_DIR}/../../../3rdparty/opencv/opencv-linux-armhf/lib/libopencv_imgproc.a
    ${CMAKE_CURRENT_LIST_DIR}/../../../3rdparty/opencv/opencv-linux-armhf/lib/libopencv_imgcodecs.a
    ${CMAKE_CURRENT_LIST_DIR}/../../../3rdparty/opencv/opencv-linux-armhf/lib/libopencv_calib3d.a
)

set(CAROTENE_INCLUDE /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo)
set(CAROTENE_LIBS
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libcarotene.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_calib3d.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_features2d.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_flann.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_gapi.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_highgui.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_imgcodecs.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_imgproc.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_ml.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_objdetect.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_photo.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_stitching.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_ts.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_video.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_videoio.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib/libopencv_core.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/libade.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/libittnotify.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/liblibjpeg-turbo.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/liblibopenjp2.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/liblibpng.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/liblibprotobuf.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/liblibtiff.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/liblibwebp.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/libquirc.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/libtegra_hal.a
   /home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/opencv-linux-armhf1106/lib2/libzlib.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/libappend1.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/libappend2.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/libappend3.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/libchromiumbase64.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/librkdepth.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/librkdepth_sc132.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/librkdepth_sc2355.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/librkdepth_vga.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/librknnmrt.a
   #/home/linf1998/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/lib/libsmart_unlock.a
)
# 包含目录应该在target_include_directories中设置，而不是用include_directories
# include_directories(${OpenCV_INCLUDE_DIRS})  # 这行移到下面

if (ENABLE_ASAN)
	message(STATUS "BUILD WITH ADDRESS SANITIZER")
	set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
	set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
	set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif ()

set(rknpu_yolo11_file rknpu2/yolo11.cc)

if (TARGET_SOC STREQUAL "rv1106" OR TARGET_SOC STREQUAL "rv1103")
    add_definitions(-DRV1106_1103)
    set(rknpu_yolo11_file rknpu2/yolo11_rv1106_1103.cc)
    #dma
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/allocator/dma)
endif()

if(TARGET_SOC STREQUAL "rk1808" OR TARGET_SOC STREQUAL "rv1109" OR TARGET_SOC STREQUAL "rv1126")
    add_definitions(-DRKNPU1)
    set(rknpu_yolo11_file rknpu1/yolo11.cc)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/ 3rdparty.out)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/ utils.out)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

file(GLOB SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

# 修正：这里应该是PROJECT_NAME_camera_mpi，但后面链接的是PROJECT_NAME
add_executable(${PROJECT_NAME}
    main.cc
    postprocess.cc
    luckfox_mpi.cc
    ${rknpu_yolo11_file}
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/librga/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rkaiq/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rkaiq/algos/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rkaiq/common/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rkaiq/ipc_server/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rkaiq/iq_parser/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rkaiq/iq_parser_v2/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rkaiq/uAPI2/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rkaiq/xcore/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rkaiq/smartIr/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../utils/rockchip/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/allocator/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/allocator/dma
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../3rdparty/allocator/drm
    ${OpenCV_INCLUDE_DIRS}
    ${LIBRKNNRT_INCLUDES}
    ${CAROTENE_INCLUDE}
)

target_link_libraries(${PROJECT_NAME}
    # 你的工具库
    imageutils
    fileutils
    imagedrawing

    # RKNN SDK
    rknnmrt
    rockiva
    sample_comm
    rockit
    rockchip_mpp
    rkaiq
    rtsp
    rga

    # OpenCV 静态库
    #${OpenCV_LIBS}
    ${CAROTENE_LIBS}

    # 系统库，顺序不能乱
    pthread
    m
    dl
    rt
    stdc++
)

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(${PROJECT_NAME}
    log
)
endif()

message(STATUS "!!!!!!!!!!!CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} Threads::Threads)
endif()

# 添加OpenCV包含目录和其他目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}  # 添加OpenCV包含目录
    ${LIBRKNNRT_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../
)


# Currently zero copy only supports rknpu2, v1103/rv1103b/rv1106 supports zero copy by default
if (NOT (TARGET_SOC STREQUAL "rv1106" OR TARGET_SOC STREQUAL "rv1103" OR TARGET_SOC STREQUAL "rk1808" 
    OR TARGET_SOC STREQUAL "rv1109" OR TARGET_SOC STREQUAL "rv1126" OR TARGET_SOC STREQUAL "rv1103b"))
    add_executable(${PROJECT_NAME}_zero_copy
        main.cc
        postprocess.cc
        rknpu2/yolo11_zero_copy.cc
    )

    target_compile_definitions(${PROJECT_NAME}_zero_copy PRIVATE ZERO_COPY)

    target_link_libraries(${PROJECT_NAME}_zero_copy
        imageutils
        fileutils
        imagedrawing    
        ${LIBRKNNRT}
        dl
    )

    if (CMAKE_SYSTEM_NAME STREQUAL "Android")
        target_link_libraries(${PROJECT_NAME}_zero_copy
        log
    )
    endif()

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(THREADS_PREFER_PTHREAD_FLAG ON)
        find_package(Threads REQUIRED)
        target_link_libraries(${PROJECT_NAME}_zero_copy Threads::Threads)
    endif()

    target_include_directories(${PROJECT_NAME}_zero_copy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LIBRKNNRT_INCLUDES}
    )
    install(TARGETS ${PROJECT_NAME}_zero_copy DESTINATION .)
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION .)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../model/bus.jpg DESTINATION model)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../model/coco_80_labels_list.txt DESTINATION model)
file(GLOB RKNN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../model/*.rknn")
install(FILES ${RKNN_FILES} DESTINATION model)
