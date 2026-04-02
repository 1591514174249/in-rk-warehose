// Copyright (c) 2024 by Rockchip Electronics Co., Ltd. All Rights Reserved.

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "rtsp_demo.h"
#include "luckfox_mpi.h"
#include "yolo11.h"  // 改为yolo11的头文件

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#if defined(RV1106_1103) 
    #include "dma_alloc.hpp"
#endif

// 显示分辨率
#define DISP_WIDTH  720
#define DISP_HEIGHT 480

// 全局变量
int width    = DISP_WIDTH;
int height   = DISP_HEIGHT;

// 模型输入尺寸 - 根据你的YOLO11模型调整
int model_width = 640;   // YOLO11标准输入尺寸
int model_height = 640;  

// letterbox参数
float scale = 1.0f;
int leftPadding = 0;
int topPadding = 0;

// 信号处理标志
static bool run_flag = true;

// 信号处理函数
void signal_handler(int sig) {
    printf("Received signal %d, exiting...\n", sig);
    run_flag = false;
}

// Letterbox处理函数
cv::Mat letterbox(cv::Mat input) {
    float scale_x = (float)model_width / (float)width;
    float scale_y = (float)model_height / (float)height;
    scale = scale_x < scale_y ? scale_x : scale_y;
    
    int inputWidth = (int)((float)width * scale);
    int inputHeight = (int)((float)height * scale);
    
    leftPadding = (model_width - inputWidth) / 2;
    topPadding = (model_height - inputHeight) / 2;
    
    cv::Mat inputScale;
    cv::resize(input, inputScale, cv::Size(inputWidth, inputHeight), 0, 0, cv::INTER_LINEAR);
    cv::Mat letterboxImage(640, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Rect roi(leftPadding, topPadding, inputWidth, inputHeight);
    inputScale.copyTo(letterboxImage(roi));
    
    return letterboxImage;
}

// 坐标映射回原始图像
void mapCoordinates(int *x, int *y) {
    int mx = *x - leftPadding;
    int my = *y - topPadding;

    *x = (int)((float)mx / scale);
    *y = (int)((float)my / scale);
}

// FPS计算
typedef struct {
    time_t start_time;
    int frame_count;
    double fps;
} fps_counter_t;

void update_fps(fps_counter_t *counter) {
    counter->frame_count++;
    time_t current_time = time(NULL);
    double elapsed = difftime(current_time, counter->start_time);
    
    if (elapsed >= 1.0) {
        counter->fps = counter->frame_count / elapsed;
        counter->frame_count = 0;
        counter->start_time = current_time;
    }
}

int main(int argc, char *argv[]) {
    // 停止可能的现有服务
    system("RkLunch-stop.sh");
    
    RK_S32 s32Ret = 0; 
    int sX,sY,eX,eY;

    char text[16];
    
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 参数解析
    const char *model_path = "./model/yolo11.rknn";  // YOLO11模型
    if (argc > 1) {
        model_path = argv[1];
    }
    
    printf("Starting YOLO11 camera inference with model: %s\n", model_path);
    
    // 初始化YOLO11模型
    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));

    int ret = init_yolo11_model(model_path, &rknn_app_ctx);
    if (ret != 0) {
        printf("init_yolo11_model fail! ret=%d model_path=%s\n", ret, model_path);
        return -1;
    }
    printf("YOLO11 model initialized successfully!\n");
    
    init_post_process();
    
    // H264帧结构
    VENC_STREAM_S stFrame;    
    stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
    RK_U64 H264_PTS = 0;
    RK_U32 H264_TimeRef = 0; 
    VIDEO_FRAME_INFO_S stViFrame;
    
    // 创建内存池
    MB_POOL_CONFIG_S PoolCfg;
    memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
    PoolCfg.u64MBSize = width * height * 3;
    PoolCfg.u32MBCnt = 1;
    PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
    MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg);
    printf("Create Pool success !\n");
    
    // 从内存池获取内存块
    MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, width * height * 3, RK_TRUE);
    if (!src_Blk) {
        printf("Failed to get memory block!\n");
        return -1;
    }
    
    // 构建H264帧
    VIDEO_FRAME_INFO_S h264_frame;
    h264_frame.stVFrame.u32Width = width;
    h264_frame.stVFrame.u32Height = height;
    h264_frame.stVFrame.u32VirWidth = width;
    h264_frame.stVFrame.u32VirHeight = height;
    h264_frame.stVFrame.enPixelFormat = RK_FMT_RGB888; 
    h264_frame.stVFrame.u32FrameFlag = 160;
    h264_frame.stVFrame.pMbBlk = src_Blk;
    
    // 创建OpenCV Mat用于处理图像
    unsigned char *frame_data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);
    cv::Mat display_frame(cv::Size(width, height), CV_8UC3, frame_data);
    
    // RV1106特殊处理
#if defined(RV1106_1103)
    // 为模型输入分配DMA内存
    size_t model_input_size = model_width * model_height * 3;
    int dma_fd;
    void *dma_buffer;
    
    ret = dma_buf_alloc(RV1106_CMA_HEAP_PATH, model_input_size, &dma_fd, &dma_buffer);
    if (ret != 0) {
        printf("Failed to allocate DMA buffer!\n");
        return -1;
    }
    
    // 更新rknn_app_ctx中的DMA缓冲区信息
    rknn_app_ctx.img_dma_buf.dma_buf_virt_addr = (char *)dma_buffer;
    rknn_app_ctx.img_dma_buf.dma_buf_fd = dma_fd;
    rknn_app_ctx.img_dma_buf.size = model_input_size;
#endif
    
    // 初始化ISP
    RK_BOOL multi_sensor = RK_FALSE;    
    const char *iq_dir = "/etc/iqfiles";
    rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
    SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
    SAMPLE_COMM_ISP_Run(0);
    printf("ISP initialized successfully!\n");
    
    // 初始化RKMPI系统
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        printf("RK_MPI_SYS_Init failed!\n");
        return -1;
    }
    printf("RKMPI system initialized successfully!\n");
    
    // 初始化RTSP服务
    rtsp_demo_handle g_rtsplive = NULL;
    rtsp_session_handle g_rtsp_session;
    g_rtsplive = create_rtsp_demo(554);
    g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
    rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());
    printf("RTSP server started on rtsp://<ip>:554/live/0\n");
    
    // 初始化VI（视频输入）
    vi_dev_init();
    vi_chn_init(0, width, height);
    printf("Video input initialized successfully!\n");
    
    // 初始化VENC（视频编码）
    RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
    venc_init(0, width, height, enCodecType);
    printf("Video encoder initialized successfully!\n");
    
    // FPS计数器
    fps_counter_t fps_counter;
    fps_counter.start_time = time(NULL);
    fps_counter.frame_count = 0;
    fps_counter.fps = 0.0;
    
    printf("Starting video inference loop... Press Ctrl+C to exit.\n");
    
    // 主循环
    while (run_flag) {
        // 获取VI帧
        h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
        h264_frame.stVFrame.u64PTS = TEST_COMM_GetNowUs(); 
        
        s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
        if (s32Ret == RK_SUCCESS) {
            // 获取原始YUV数据
            void *vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);
            
            // YUV420sp转BGR
            cv::Mat yuv420sp(stViFrame.stVFrame.u32Height + stViFrame.stVFrame.u32Height / 2, 
                            stViFrame.stVFrame.u32Width, CV_8UC1, vi_data);
            cv::Mat bgr(height, width, CV_8UC3, frame_data);
            
            cv::cvtColor(yuv420sp, bgr, cv::COLOR_YUV420sp2BGR);
            cv::resize(bgr, display_frame, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
            
            
            // 创建letterbox图像用于推理
            cv::Mat letterboxImage = letterbox(display_frame);
            
            // 准备模型输入
// #if defined(RV1106_1103)
//             // 使用DMA缓冲区
//             memcpy(rknn_app_ctx.img_dma_buf.dma_buf_virt_addr, 
//                    letterboxImage.data, 
//                    model_width * model_height * 3);
//             dma_sync_cpu_to_device(rknn_app_ctx.img_dma_buf.dma_buf_fd);
            
//             // 更新输入内存
//             rknn_app_ctx.input_mems[0]->virt_addr = rknn_app_ctx.img_dma_buf.dma_buf_virt_addr;
//             rknn_app_ctx.input_mems[0]->fd = rknn_app_ctx.img_dma_buf.dma_buf_fd;
// #else
//             // 普通内存复制
//             memcpy(rknn_app_ctx.input_mems[0]->virt_addr, 
//                    letterboxImage.data, 
//                    model_width * model_height * 3);
// #endif
            memcpy(rknn_app_ctx.input_mems[0]->virt_addr, 
                   letterboxImage.data, 
                   model_width * model_height * 3);
            
            // 执行YOLO11推理
            object_detect_result_list od_results;
            ret = inference_yolo11_model(&rknn_app_ctx, &od_results);
            if (ret == 0) {
                // 绘制检测结果
                update_fps(&fps_counter);
            }

            for(int i = 0; i < od_results.count; i++)
			{				
				if(od_results.count >= 1)
				{
					object_detect_result *det_result = &(od_results.results[i]);
	
					sX = (int)(det_result->box.left   );	
					sY = (int)(det_result->box.top 	  );	
					eX = (int)(det_result->box.right  );	
					eY = (int)(det_result->box.bottom );
					mapCoordinates(&sX,&sY);
					mapCoordinates(&eX,&eY);
					
					printf("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det_result->cls_id),
							 sX, sY, eX, eY, det_result->prop);

					cv::rectangle(display_frame,cv::Point(sX ,sY),
								        cv::Point(eX ,eY),
										cv::Scalar(0,255,0),3);
					sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
					cv::putText(display_frame,text,cv::Point(sX, sY - 8),
										   cv::FONT_HERSHEY_SIMPLEX,1,
										   cv::Scalar(0,255,0),1);
				}
			}
            
            // 复制处理后的图像数据到编码缓冲区
            memcpy(frame_data, display_frame.data, width * height * 3);
        }
        
        // 编码H264
        RK_MPI_VENC_SendFrame(0, &h264_frame, -1);
        
        // 获取编码后的流
        s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);
        if (s32Ret == RK_SUCCESS && g_rtsplive && g_rtsp_session) {
            // 发送到RTSP
            void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
            rtsp_tx_video(g_rtsp_session, 
                         (uint8_t *)pData, 
                         stFrame.pstPack->u32Len,
                         stFrame.pstPack->u64PTS);
            rtsp_do_event(g_rtsplive);
        }
        
        // 释放帧
        if (s32Ret == RK_SUCCESS) {
            RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
            RK_MPI_VENC_ReleaseStream(0, &stFrame);
        }
    }
    
    printf("Stopping video inference...\n");
    
    // 清理资源
    printf("Cleaning up resources...\n");
    
    // 释放内存块和内存池
    RK_MPI_MB_ReleaseMB(src_Blk);
    RK_MPI_MB_DestroyPool(src_Pool);
    
    // 停止VI
    RK_MPI_VI_DisableChn(0, 0);
    RK_MPI_VI_DisableDev(0);
    
    // 停止ISP
    SAMPLE_COMM_ISP_Stop(0);
    
    // 停止VENC
    RK_MPI_VENC_StopRecvFrame(0);
    RK_MPI_VENC_DestroyChn(0);
    
    // 释放H264包内存
    free(stFrame.pstPack);
    
    // 关闭RTSP
    if (g_rtsplive) {
        rtsp_del_demo(g_rtsplive);
    }
    
    // 退出RKMPI系统
    RK_MPI_SYS_Exit();
    
    // RV1106 DMA内存释放
#if defined(RV1106_1103)
    if (dma_buffer) {
        dma_buf_free(model_input_size, &dma_fd, dma_buffer);
    }
#endif
    
    // 释放YOLO11模型
    release_yolo11_model(&rknn_app_ctx);
    deinit_post_process();
    
    printf("YOLO11 camera inference stopped successfully.\n");
    
    return 0;
}