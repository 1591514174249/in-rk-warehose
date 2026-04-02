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
#include <map>
#include <cmath>
#include <algorithm>
#include <sys/time.h> 

// --- 新增：C++ 多线程支持 ---
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

#include "rtsp_demo.h"
#include "luckfox_mpi.h"
#include "yolo11.h"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#if defined(RV1106_1103) 
    #include "dma_alloc.hpp"
#endif

// ==========================================
// 全局配置变量 (默认值)
// ==========================================
int g_width             = 720;
int g_height            = 480;
int g_model_input_size  = 640;
char g_model_path[256]  = "./model/yolo11.rknn";

float g_high_threshold      = 0.55f;
float g_low_threshold       = 0.10f;
float g_match_iou_threshold = 0.25f;
float g_velocity_alpha      = 0.3f;
float g_count_line_center_pct = 0.60f;
int   g_zone_buffer         = 40;
float g_roi_x_min_pct       = 0.30f;
float g_roi_x_max_pct       = 0.70f;

// 内部计算用变量 (注意：多线程模式下，这些全局变量不再用于推理线程的计算)
int model_width;
int model_height;

int g_exit_code = 0;
int g_copper_num = 0;
static std::atomic<bool> run_flag(true); // 使用 atomic 保证线程安全

// ==========================================
// 结构体定义
// ==========================================
struct SimpleObj {
    cv::Point2f center;
    cv::Rect box;
    float conf;
    int state; // 0: Unknown, 1: Ready, 2: Done
    
    float vx;
    float vy;
    float last_cy;
    bool is_matched; 
};

// --- 新增：管道帧结构体 ---
struct PipelineFrame {
    cv::Mat input_data; // Letterbox 后的 640x640 数据
    float scale;        // 该帧的缩放比例
    int dw;             // 该帧的左边距
    int dh;             // 该帧的上边距
    long timestamp;     // 可选：用于调试延迟
};

// ==========================================
// 配置文件读取函数
// ==========================================
void load_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("[WARN] Config file '%s' not found. Using defaults.\n", filename);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char key[128], value[128];
        if (sscanf(line, "%[^=]=%s", key, value) == 2) {
            if (strcmp(key, "width") == 0) g_width = atoi(value);
            else if (strcmp(key, "height") == 0) g_height = atoi(value);
            else if (strcmp(key, "model_input_size") == 0) g_model_input_size = atoi(value);
            else if (strcmp(key, "model_path") == 0) strncpy(g_model_path, value, 255);
            else if (strcmp(key, "high_threshold") == 0) g_high_threshold = atof(value);
            else if (strcmp(key, "low_threshold") == 0) g_low_threshold = atof(value);
            else if (strcmp(key, "match_iou_threshold") == 0) g_match_iou_threshold = atof(value);
            else if (strcmp(key, "velocity_alpha") == 0) g_velocity_alpha = atof(value);
            else if (strcmp(key, "count_line_center_pct") == 0) g_count_line_center_pct = atof(value);
            else if (strcmp(key, "zone_buffer") == 0) g_zone_buffer = atoi(value);
            else if (strcmp(key, "roi_x_min_pct") == 0) g_roi_x_min_pct = atof(value);
            else if (strcmp(key, "roi_x_max_pct") == 0) g_roi_x_max_pct = atof(value);
        }
    }
    fclose(file);
    printf("[INFO] Config loaded from %s\n", filename);
}

void signal_handler(int sig) {
    run_flag = false;
}

long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// ==========================================
// 线程安全队列类
// ==========================================
class FrameQueue {
private:
    std::queue<PipelineFrame> queue;
    std::mutex m;
    std::condition_variable cv;
    size_t max_size;

public:
    FrameQueue(size_t size) : max_size(size) {}

    // 找到 FrameQueue 类，替换 push 函数为：
    void push(PipelineFrame frame) {
    std::unique_lock<std::mutex> lock(m);
    
    // 如果队列满了，踢掉最旧的，给新的腾位置
    // 这样主线程永远不需要等待 (cv.wait)
    if (queue.size() >= max_size) {
        queue.pop(); 
    }

    queue.push(frame);
    lock.unlock();
    cv.notify_one();
}

    bool pop(PipelineFrame& frame) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [this] { return !queue.empty() || !run_flag; });
        if (queue.empty() && !run_flag) return false;
        
        frame = queue.front();
        queue.pop();
        lock.unlock();
        cv.notify_one();
        return true;
    }
    
    void wake_all() {
        cv.notify_all();
    }
};

// 实例化全局队列 (缓冲区设为 2 帧，平衡延迟与吞吐)
FrameQueue g_infer_queue(2);

// 全局推理结果容器
std::vector<SimpleObj> g_latest_results;
std::mutex g_result_mutex;
std::atomic<float> g_infer_fps(0.0f); // 用于显示真实推理FPS

// ==========================================
// 辅助函数 (修改版：支持多线程传参)
// ==========================================

// 修改 letterbox，不再依赖全局变量，而是输出到 PipelineFrame 所需参数
cv::Mat letterbox_thread_safe(cv::Mat input, float* out_scale, int* out_dw, int* out_dh) {
    float scale_x = (float)model_width / (float)input.cols;
    float scale_y = (float)model_height / (float)input.rows;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    
    int inputWidth = (int)((float)input.cols * scale);
    int inputHeight = (int)((float)input.rows * scale);
    
    int leftPadding = (model_width - inputWidth) / 2;
    int topPadding = (model_height - inputHeight) / 2;
    
    cv::Mat inputScale;
    cv::resize(input, inputScale, cv::Size(inputWidth, inputHeight), 0, 0, cv::INTER_LINEAR);
    
    cv::Mat letterboxImage(model_height, model_width, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Rect roi(leftPadding, topPadding, inputWidth, inputHeight);
    inputScale.copyTo(letterboxImage(roi));

    // 输出参数
    *out_scale = scale;
    *out_dw = leftPadding;
    *out_dh = topPadding;

    return letterboxImage;
}

// 线程安全的坐标映射
void mapCoordinates_thread_safe(int *x, int *y, float scale, int leftPadding, int topPadding) {
    int mx = *x - leftPadding;
    int my = *y - topPadding;
    *x = (int)((float)mx / scale);
    *y = (int)((float)my / scale);
}

float calculate_iou(const cv::Rect& box1, const cv::Rect& box2) {
    int x1 = std::max(box1.x, box2.x);
    int y1 = std::max(box1.y, box2.y);
    int x2 = std::min(box1.x + box1.width, box2.x + box2.width);
    int y2 = std::min(box1.y + box1.height, box2.y + box2.height);

    if (x1 >= x2 || y1 >= y2) return 0.0f;

    float intersection = (float)((x2 - x1) * (y2 - y1));
    float area1 = (float)(box1.width * box1.height);
    float area2 = (float)(box2.width * box2.height);
    
    return intersection / (area1 + area2 - intersection);
}

void match_and_update_with_filter(std::vector<SimpleObj>& tracks, 
                                  std::vector<SimpleObj>& detections, 
                                  float iou_thresh) {
    for (auto& t : tracks) {
        if (t.is_matched) continue; 
        int best_idx = -1;
        float max_iou = 0.0f; 
        for (size_t i = 0; i < detections.size(); i++) {
            if (detections[i].is_matched) continue; 
            float iou = calculate_iou(t.box, detections[i].box);
            if (iou > max_iou) {
                max_iou = iou;
                best_idx = i;
            }
        }
        if (best_idx != -1 && max_iou > iou_thresh) {
            t.is_matched = true;                    
            detections[best_idx].is_matched = true; 
            
            float error_x = detections[best_idx].center.x - t.center.x;
            float error_y = detections[best_idx].center.y - t.center.y;
            
            t.vx += g_velocity_alpha * error_x;
            t.vy += g_velocity_alpha * error_y;
            
            t.box = detections[best_idx].box;
            t.center = detections[best_idx].center;
            t.conf = detections[best_idx].conf;
        }
    }
}

void draw_state_machine_ui(cv::Mat& img, int center_y, int width, int height, bool is_triggered, int roi_min_x, int roi_max_x) {
    int start_line = center_y + g_zone_buffer; 
    int finish_line = center_y - g_zone_buffer; 

    cv::line(img, cv::Point(0, start_line), cv::Point(width, start_line), cv::Scalar(255, 0, 0), 2);
    
    cv::Scalar finish_color = is_triggered ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
    int thickness = is_triggered ? 4 : 2;
    cv::line(img, cv::Point(0, finish_line), cv::Point(width, finish_line), finish_color, thickness);
    
    cv::line(img, cv::Point(roi_min_x, 0), cv::Point(roi_min_x, height), cv::Scalar(255, 0, 0), 2);
    cv::line(img, cv::Point(roi_max_x, 0), cv::Point(roi_max_x, height), cv::Scalar(255, 0, 0), 2);

    if (is_triggered) {
        cv::putText(img, "+1 COUNTED!", cv::Point(roi_min_x + 10, finish_line - 10), 
            cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);
    }
}

void draw_target_info(cv::Mat& img, const SimpleObj& obj) {
    cv::Scalar box_color;
    if (obj.state == 1) box_color = cv::Scalar(0, 255, 255); 
    else if (obj.state == 2) box_color = cv::Scalar(0, 255, 0);   
    else box_color = cv::Scalar(0, 0, 255); 

    cv::rectangle(img, obj.box, box_color, 1);
    
    char text[64];
    const char* state_str = (obj.state == 1) ? "R" : (obj.state == 2 ? "D" : "U");
    sprintf(text, "%s %.0f", state_str, obj.conf * 100);

    cv::putText(img, text, cv::Point(obj.box.x, obj.box.y - 5),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, box_color, 1);
    
    cv::Point2f p_end = obj.center + cv::Point2f(obj.vx * 3, obj.vy * 3);
    cv::arrowedLine(img, obj.center, p_end, cv::Scalar(255,0,0), 1);
}

// ==========================================
// ★★★ 新增：推理线程函数 ★★★
// ==========================================
void infer_worker_thread(rknn_app_context_t* ctx) {
    long last_time = get_current_time_ms();
    int frame_cnt = 0;

    while (run_flag) {
        PipelineFrame p_frame;
        // 1. 从队列取数据 (阻塞等待)
        if (!g_infer_queue.pop(p_frame)) {
            continue; // 如果退出或没数据
        }

        // 2. 拷贝数据到 NPU 输入
        memcpy(ctx->input_mems[0]->virt_addr, p_frame.input_data.data, model_width * model_height * 3);

        // 3. 执行推理
        object_detect_result_list od_results;
        inference_yolo11_model(ctx, &od_results);

        // 4. 后处理 (生成临时结果)
        std::vector<SimpleObj> detected_objs;
        int roi_pixel_min = (int)(g_width * g_roi_x_min_pct);
        int roi_pixel_max = (int)(g_width * g_roi_x_max_pct);

        for(int i = 0; i < od_results.count; i++) {
            object_detect_result *det = &(od_results.results[i]);
            if (det->prop < g_low_threshold) continue;
            
            // 使用 p_frame 中携带的 scale 参数还原坐标
            int x1 = det->box.left, y1 = det->box.top, x2 = det->box.right, y2 = det->box.bottom;
            mapCoordinates_thread_safe(&x1, &y1, p_frame.scale, p_frame.dw, p_frame.dh);
            mapCoordinates_thread_safe(&x2, &y2, p_frame.scale, p_frame.dw, p_frame.dh);
            
            float cx = (x1 + x2) / 2.0f;
            float cy = (y1 + y2) / 2.0f;

            if (cx < roi_pixel_min || cx > roi_pixel_max) continue;

            SimpleObj obj;
            obj.box = cv::Rect(x1, y1, x2-x1, y2-y1);
            obj.center = cv::Point2f(cx, cy);
            obj.conf = det->prop;
            obj.state = 0; 
            obj.vx = 0.0f; 
            obj.vy = 0.0f;
            obj.is_matched = false;
            obj.last_cy = cy; 

            detected_objs.push_back(obj);
        }

        // 5. 更新全局检测结果
        {
            std::lock_guard<std::mutex> lock(g_result_mutex);
            g_latest_results = detected_objs;
        }

        // 6. 统计推理 FPS
        frame_cnt++;
        long now = get_current_time_ms();
        if (now - last_time >= 1000) {
            g_infer_fps = (float)frame_cnt * 1000.0f / (float)(now - last_time);
            frame_cnt = 0;
            last_time = now;
            // printf("[Worker] Inference FPS: %.1f\n", g_infer_fps.load());
        }
    }
}

// ==========================================
// 主函数
// ==========================================
int main(int argc, char *argv[]) {
    // 1. 读取配置
    load_config("./config.conf");
    model_width  = g_model_input_size;
    model_height = g_model_input_size;

    system("RkLunch-stop.sh");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc > 1) strncpy(g_model_path, argv[1], 255);
    
    printf("Producer-Consumer Mode Enabled.\n");
    printf("Model: %s\n", g_model_path);
    printf("Resolution: %dx%d\n", g_width, g_height);
    
    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));
    if (init_yolo11_model(g_model_path, &rknn_app_ctx) != 0) return -1;
    init_post_process();
    
    // --- 内存池初始化 ---
    MB_POOL_CONFIG_S PoolCfg;
    memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
    PoolCfg.u64MBSize = g_width * g_height * 3;
    PoolCfg.u32MBCnt = 1;
    PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
    MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg);
    MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, g_width * g_height * 3, RK_TRUE);
    
    VIDEO_FRAME_INFO_S h264_frame;
    memset(&h264_frame, 0, sizeof(VIDEO_FRAME_INFO_S));
    h264_frame.stVFrame.u32Width = g_width;
    h264_frame.stVFrame.u32Height = g_height;
    h264_frame.stVFrame.u32VirWidth = g_width;
    h264_frame.stVFrame.u32VirHeight = g_height;
    h264_frame.stVFrame.enPixelFormat = RK_FMT_RGB888; 
    h264_frame.stVFrame.u32FrameFlag = 160;
    h264_frame.stVFrame.pMbBlk = src_Blk;
    unsigned char *frame_data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);
    
#if defined(RV1106_1103)
    size_t model_input_mem_size = model_width * model_height * 3;
    int dma_fd;
    void *dma_buffer;
    dma_buf_alloc(RV1106_CMA_HEAP_PATH, model_input_mem_size, &dma_fd, &dma_buffer);
    rknn_app_ctx.img_dma_buf.dma_buf_virt_addr = (char *)dma_buffer;
    rknn_app_ctx.img_dma_buf.dma_buf_fd = dma_fd;
    rknn_app_ctx.img_dma_buf.size = model_input_mem_size;
#endif

    SAMPLE_COMM_ISP_Init(0, RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, "/etc/iqfiles");
    SAMPLE_COMM_ISP_Run(0);
    RK_MPI_SYS_Init();
    rtsp_demo_handle g_rtsplive = create_rtsp_demo(554);
    rtsp_session_handle g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
    rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());
    vi_dev_init(); vi_chn_init(0, g_width, g_height); venc_init(0, g_width, g_height, RK_VIDEO_ID_AVC);
    
    VENC_STREAM_S stFrame; stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
    VIDEO_FRAME_INFO_S stViFrame;
    RK_U32 H264_TimeRef = 0;

    std::vector<SimpleObj> prev_objs; 
    
    int count_line_center = g_height * g_count_line_center_pct; 
    int roi_pixel_min = (int)(g_width * g_roi_x_min_pct);
    int roi_pixel_max = (int)(g_width * g_roi_x_max_pct);
    int line_flash_timer = 0;

    long last_fps_time = get_current_time_ms();
    int fps_frame_count = 0;
    float current_display_fps = 0.0f;

    // ★★★ 启动推理消费者线程 ★★★
    std::thread infer_worker(infer_worker_thread, &rknn_app_ctx);
    infer_worker.detach(); // 在后台运行

    // ==================
    // 主循环 (生产者)
    // ==================
    while (run_flag) {
        if (RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1) == RK_SUCCESS) {
            void *vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);
            cv::Mat yuv420sp(stViFrame.stVFrame.u32Height * 1.5, stViFrame.stVFrame.u32Width, CV_8UC1, vi_data);
            cv::Mat bgr(g_height, g_width, CV_8UC3, frame_data); 
            cv::cvtColor(yuv420sp, bgr, cv::COLOR_YUV420sp2BGR);
            
            // 1. 预处理 (CPU 计算，主线程做)
            PipelineFrame pipe_frame;
            cv::Mat resize_img;
            cv::resize(bgr, resize_img, cv::Size(g_width, g_height), 0, 0, cv::INTER_LINEAR);
            
            // 使用线程安全版 Letterbox
            pipe_frame.input_data = letterbox_thread_safe(resize_img, &pipe_frame.scale, &pipe_frame.dw, &pipe_frame.dh);
            // 这里 Mat 必须是深拷贝(letterbox_thread_safe 返回的是新 Mat，所以是安全的)
            
            // 2. 将数据推入队列 (供消费者使用)
            g_infer_queue.push(pipe_frame);

            // 3. 获取最新的推理结果 (非阻塞)
            std::vector<SimpleObj> high_score_dets;
            std::vector<SimpleObj> low_score_dets;
            
            // 从全局结果中提取并分类
            {
                std::lock_guard<std::mutex> lock(g_result_mutex);
                for(const auto& obj : g_latest_results) {
                     if (obj.conf >= g_high_threshold) high_score_dets.push_back(obj);
                     else low_score_dets.push_back(obj);
                }
            }

            // 4. 执行追踪逻辑 (Tracking)
            // 先让旧目标根据速度预测新位置 (Prediction)
            for (auto& t : prev_objs) {
                t.is_matched = false;
                t.last_cy = t.center.y; 
                t.center.x += t.vx;
                t.center.y += t.vy;
                t.box.x += t.vx;
                t.box.y += t.vy;
            }

            // 执行匹配 (Correction)
            match_and_update_with_filter(prev_objs, high_score_dets, g_match_iou_threshold);
            match_and_update_with_filter(prev_objs, low_score_dets, g_match_iou_threshold);

            std::vector<SimpleObj> curr_objs_final;
            
            // 业务逻辑 (计数等) - 使用全局配置
            int start_line_y = count_line_center + g_zone_buffer;  
            int finish_line_y = count_line_center - g_zone_buffer; 
            bool triggered_this_frame = false;

            for (auto& t : prev_objs) {
                if (t.is_matched) {
                    if (t.state == 2 && t.center.y > (finish_line_y + 10)) {
                        t.state = 1; 
                    }
                    if (t.state == 0 && t.center.y > start_line_y) {
                         t.state = 1; 
                    }
                    
                    bool crossed = (t.last_cy > finish_line_y) && (t.center.y <= finish_line_y);
                    
                    if (crossed) {
                        if (t.state != 2) {
                            g_copper_num++;
                            triggered_this_frame = true;
                            t.state = 2; 
                            printf(">>> CROSSING! Total: %d, Vy: %.1f\n", g_copper_num, t.vy);
                        }
                    }
                    else if (t.center.y < finish_line_y && t.state == 1) {
                        g_copper_num++;
                        triggered_this_frame = true;
                        t.state = 2;
                    }

                    curr_objs_final.push_back(t);
                    draw_target_info(resize_img, t);
                }
            }

            // 添加新出现的检测框
            for (auto& det : high_score_dets) {
                if (!det.is_matched) {
                    if (det.center.y > start_line_y) det.state = 1; 
                    curr_objs_final.push_back(det);
                    draw_target_info(resize_img, det);
                }
            }
            
            prev_objs = curr_objs_final;
            
            // 5. 绘制 UI
            if (triggered_this_frame) line_flash_timer = 15;
            if (line_flash_timer > 0) line_flash_timer--;
            
            draw_state_machine_ui(resize_img, count_line_center, g_width, g_height, 
                                  (line_flash_timer > 0), roi_pixel_min, roi_pixel_max);

            // 6. FPS 统计与显示
            fps_frame_count++;
            long current_time = get_current_time_ms();
            if (current_time - last_fps_time >= 1000) {
                current_display_fps = (float)fps_frame_count * 1000.0f / (float)(current_time - last_fps_time);
                fps_frame_count = 0;
                last_fps_time = current_time;
            }

            char fps_text[64];
            // 显示格式: DisplayFPS / InferenceFPS
            sprintf(fps_text, "FPS: %.0f / %.0f", current_display_fps, g_infer_fps.load());
            cv::putText(resize_img, fps_text, cv::Point(20, 40), 
                        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);

            cv::rectangle(resize_img, cv::Point(g_width - 240, 0), cv::Point(g_width, 40), cv::Scalar(255, 255, 255), -1);
            char stats_text[64];
            sprintf(stats_text, "Copper: %d", g_copper_num);
            cv::Scalar text_color = (line_flash_timer > 0) ? cv::Scalar(0, 200, 0) : cv::Scalar(0, 0, 0);
            cv::putText(resize_img, stats_text, cv::Point(g_width - 230, 28),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, text_color, 2);

            // 7. 发送视频流
            memcpy(frame_data, resize_img.data, g_width * g_height * 3);
            h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
            h264_frame.stVFrame.u64PTS = TEST_COMM_GetNowUs(); 
            RK_MPI_VENC_SendFrame(0, &h264_frame, -1);
            if (RK_MPI_VENC_GetStream(0, &stFrame, -1) == RK_SUCCESS) {
                if (g_rtsplive && g_rtsp_session) {
                    void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
                    rtsp_tx_video(g_rtsp_session, (uint8_t *)pData, stFrame.pstPack->u32Len, stFrame.pstPack->u64PTS);
                    rtsp_do_event(g_rtsplive);
                }
                RK_MPI_VENC_ReleaseStream(0, &stFrame);
            }
            RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
        }
    }

    // 唤醒所有线程以便安全退出
    g_infer_queue.wake_all(); 
    
    // 清理资源
    RK_MPI_MB_ReleaseMB(src_Blk); RK_MPI_MB_DestroyPool(src_Pool);
    RK_MPI_VI_DisableChn(0, 0); RK_MPI_VI_DisableDev(0);
    SAMPLE_COMM_ISP_Stop(0); RK_MPI_VENC_StopRecvFrame(0); 
    RK_MPI_VENC_DestroyChn(0); free(stFrame.pstPack);
    if (g_rtsplive) rtsp_del_demo(g_rtsplive);
    RK_MPI_SYS_Exit();
#if defined(RV1106_1103)
    if (dma_buffer) dma_buf_free(model_input_mem_size, &dma_fd, dma_buffer);
#endif
    release_yolo11_model(&rknn_app_ctx);
    deinit_post_process();
    
    printf("CODE=%d\n", g_exit_code);
    printf("COPPER_NUM=%d\n", g_copper_num);
    fflush(stdout);

    return g_exit_code;
}