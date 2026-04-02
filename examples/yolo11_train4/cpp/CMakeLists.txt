cmake_minimum_required(VERSION 3.10)

project(rknn_yolo11_demo)

set(LUCKFOX_SDK_PATH /home/linf1998/luckfox-pico)

# =========================================================
# 核心逻辑修改：设置 uClibc 编译器自带的标准库路径
# =========================================================
set(TOOLCHAIN_SYSROOT "/home/linf1998/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/arm-rockchip830-linux-uclibcgnueabihf/sysroot/usr/lib")

# =========================================================
# 核心逻辑修改：修正硬件库路径
# =========================================================
set(RKAIQ_PATH "${LUCKFOX_SDK_PATH}/media/isp/release_camera_engine_rkaiq_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib")
set(RGA_PATH   "${LUCKFOX_SDK_PATH}/media/rga/release_rga_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib")
set(ROCKIT_PATH "${LUCKFOX_SDK_PATH}/media/rockit/rockit/lib/lib32")
set(SAMPLE_COMM_PATH "${LUCKFOX_SDK_PATH}/media/samples/example/common/lib")

link_directories(
    ${LUCKFOX_SDK_PATH}/media/mpp/release_mpp_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib
    ${LUCKFOX_SDK_PATH}/media/samples/example/common/lib
    ${LUCKFOX_SDK_PATH}/media/common_algorithm/common_algorithm/misc/lib/arm-rockchip830-linux-uclibcgnueabihf
    ${LUCKFOX_SDK_PATH}/media/iva/iva/librockiva/rockiva-rv1106-Linux/lib
    ${LUCKFOX_SDK_PATH}/media/iva/out/lib
    ${LUCKFOX_SDK_PATH}/media/out/lib
    ${ROCKIT_PATH}
    ${RKAIQ_PATH}
    ${RGA_PATH}
    ${SAMPLE_COMM_PATH}
    ${TOOLCHAIN_SYSROOT}
)

# =========================================================
# 核心逻辑修改：通过 rpath-link 解决库之间的二次依赖
# =========================================================
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link=${TOOLCHAIN_SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link=${RGA_PATH}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link=${RKAIQ_PATH}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link=${ROCKIT_PATH}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link=${SAMPLE_COMM_PATH}")

if(NOT DEFINED TARGET_SOC)
    set(TARGET_SOC "rv1106" CACHE STRING "Target SOC (rv1106, rv1103, rk1808, etc.)")
    message(STATUS "TARGET_SOC not set, defaulting to ${TARGET_SOC}")
endif()
message(STATUS "Building for TARGET_SOC: ${TARGET_SOC}")

# 设置OpenCV的配置路径
set(OpenCV_DIR
    ${CMAKE_CURRENT_LIST_DIR}/../../../3rdparty/opencv/opencv-linux-armhf/share/OpenCV
)

# 查找OpenCV
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

# =========================================================
# 警告：请确认以下路径是否真实存在于你的系统中
# 如果你没有下载 rk_smart_door 项目，这一部分可能会导致后续链接报错
# =========================================================
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
)

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
    imageutils
    fileutils
    imagedrawing
    rknnmrt
    rockiva
    sample_comm
    rockit
    rockchip_mpp
    rkaiq
    rtsp
    rga
    ${CAROTENE_LIBS}
    pthread
    m
    dl
    rt
    stdc++
)

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(${PROJECT_NAME} log)
endif()

message(STATUS "!!!!!!!!!!!CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} Threads::Threads)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    ${LIBRKNNRT_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../
)

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
        target_link_libraries(${PROJECT_NAME}_zero_copy log)
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