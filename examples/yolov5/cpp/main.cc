// Copyright (c) 2023 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>  // 使用ctime替代chrono，兼容性更好

#include "yolov5.h"
#include "image_utils.h"
#include "file_utils.h"
#include "image_drawing.h"

#if defined(RV1106_1103) 
    #include "dma_alloc.hpp"
#endif

/*-------------------------------------------
                  Main Function
-------------------------------------------*/
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("%s <model_path> <image_path>\n", argv[0]);
        return -1;
    }

    const char *model_path = argv[1];
    const char *image_path = argv[2];

    int ret;
    rknn_app_context_t rknn_app_ctx;
    std::memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));

    // 将所有需要使用的变量在goto之前声明
    float avg_confidence = 0.0f;
    float conf_threshold = 0.5f;  // 置信度阈值设为0.5
    int valid_detections = 0;
    image_buffer_t src_image;
    std::memset(&src_image, 0, sizeof(image_buffer_t));
    object_detect_result_list od_results;
    std::memset(&od_results, 0, sizeof(object_detect_result_list));
    char text[256];
    
    // FPS相关变量 - 必须在goto之前声明
    clock_t total_start = 0, total_end = 0, infer_start = 0, infer_end = 0;
    double total_time_ms = 0.0;
    double inference_time_ms = 0.0;
    float total_fps = 0.0f;
    float inference_fps = 0.0f;
    
    init_post_process();

    // 启动总计时
    total_start = clock();

    ret = init_yolov5_model(model_path, &rknn_app_ctx);
    if (ret != 0)
    {
        printf("init_yolov5_model fail! ret=%d model_path=%s\n", ret, model_path);
        goto out;
    }

    ret = read_image(image_path, &src_image);

#if defined(RV1106_1103) 
    //RV1106 rga requires that input and output bufs are memory allocated by dma
    ret = dma_buf_alloc(RV1106_CMA_HEAP_PATH, src_image.size, &rknn_app_ctx.img_dma_buf.dma_buf_fd, 
                       (void **) & (rknn_app_ctx.img_dma_buf.dma_buf_virt_addr));
    std::memcpy(rknn_app_ctx.img_dma_buf.dma_buf_virt_addr, src_image.virt_addr, src_image.size);
    dma_sync_cpu_to_device(rknn_app_ctx.img_dma_buf.dma_buf_fd);
    std::free(src_image.virt_addr);
    src_image.virt_addr = (unsigned char *)rknn_app_ctx.img_dma_buf.dma_buf_virt_addr;
    src_image.fd = rknn_app_ctx.img_dma_buf.dma_buf_fd;
    rknn_app_ctx.img_dma_buf.size = src_image.size;
#endif

    if (ret != 0)
    {
        printf("read image fail! ret=%d image_path=%s\n", ret, image_path);
        goto out;
    }

    // 开始推理计时
    infer_start = clock();
    
    ret = inference_yolov5_model(&rknn_app_ctx, &src_image, &od_results);
    
    // 结束推理计时
    infer_end = clock();
    inference_time_ms = ((double)(infer_end - infer_start) / CLOCKS_PER_SEC) * 1000.0;
    
    if (ret != 0)
    {
        printf("inference_yolov5_model fail! ret=%d\n", ret);
        goto out;
    }

    // 计算推理FPS
    if (inference_time_ms > 0)
    {
        inference_fps = 1000.0f / inference_time_ms;
    }

    // 过滤置信度低于0.5的结果
    for (int i = 0; i < od_results.count; i++)
    {
        object_detect_result *det_result = &(od_results.results[i]);
        if (det_result->prop >= conf_threshold)
        {
            valid_detections++;
        }
    }

    printf("\n原始检测数量: %d\n", od_results.count);
    printf("置信度阈值(0.5)过滤后数量: %d\n", valid_detections);

    // 重置avg_confidence
    avg_confidence = 0.0f;
    
    // 画框和概率（只绘制置信度>=0.5的检测结果）
    for (int i = 0; i < od_results.count; i++)
    {
        object_detect_result *det_result = &(od_results.results[i]);
        
        // 过滤置信度低于0.5的检测结果
        if (det_result->prop < conf_threshold)
        {
            continue;
        }
        
        printf("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det_result->cls_id),
               det_result->box.left, det_result->box.top,
               det_result->box.right, det_result->box.bottom,
               det_result->prop);
        
        // 累加置信度用于计算平均值
        avg_confidence += det_result->prop;
        
        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;

        draw_rectangle(&src_image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);

        snprintf(text, sizeof(text), "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
        draw_text(&src_image, text, x1, y1 - 20, COLOR_RED, 10);
    }

    // 计算平均置信度（只计算>=0.5的）
    if (valid_detections > 0)
    {
        avg_confidence = avg_confidence / valid_detections;
        
        // 结束总计时
        total_end = clock();
        total_time_ms = ((double)(total_end - total_start) / CLOCKS_PER_SEC) * 1000.0;
        
        // 计算总FPS
        if (total_time_ms > 0)
        {
            total_fps = 1000.0f / total_time_ms;
        }
        
        // 在图片左上角显示检测结果统计信息
        snprintf(text, sizeof(text), "Objects: %d, Avg Conf: %.1f%%", 
                valid_detections, avg_confidence * 100);
        
        // 绘制在图片左上角，使用不同的颜色以区别于检测框标签
        draw_text(&src_image, text, 10, 30, COLOR_GREEN, 12);
        
        // 显示FPS信息（在统计信息下方）
        snprintf(text, sizeof(text), "FPS: %.1f (Infer: %.1f)", total_fps, inference_fps);
        draw_text(&src_image, text, 10, 60, COLOR_YELLOW, 12);
        
        // 在控制台输出详细统计信息
        printf("\n========================================\n");
        printf("Detection Summary (Confidence >= 0.5):\n");
        printf("========================================\n");
        printf("Total Objects: %d\n", valid_detections);
        printf("Average Confidence: %.3f (%.1f%%)\n", avg_confidence, avg_confidence * 100);
        printf("\nPerformance Statistics:\n");
        printf("  Total Processing Time: %.2f ms\n", total_time_ms);
        printf("  Inference Time: %.2f ms\n", inference_time_ms);
        printf("  Total FPS: %.1f\n", total_fps);
        printf("  Inference FPS: %.1f\n", inference_fps);
        printf("========================================\n");
    }
    else
    {
        // 结束总计时
        total_end = clock();
        total_time_ms = ((double)(total_end - total_start) / CLOCKS_PER_SEC) * 1000.0;
        
        // 计算总FPS
        if (total_time_ms > 0)
        {
            total_fps = 1000.0f / total_time_ms;
        }
        
        // 如果没有检测到任何对象
        snprintf(text, sizeof(text), "No objects detected");
        draw_text(&src_image, text, 10, 30, COLOR_RED, 12);
        
        // 显示FPS信息
        snprintf(text, sizeof(text), "FPS: %.1f (Infer: %.1f)", total_fps, inference_fps);
        draw_text(&src_image, text, 10, 60, COLOR_YELLOW, 12);
        
        printf("\n========================================\n");
        printf("No objects detected with confidence >= 0.5 in the image.\n");
        printf("\nPerformance Statistics:\n");
        printf("  Total Processing Time: %.2f ms\n", total_time_ms);
        printf("  Inference Time: %.2f ms\n", inference_time_ms);
        printf("  Total FPS: %.1f\n", total_fps);
        printf("  Inference FPS: %.1f\n", inference_fps);
        printf("========================================\n");
    }

    write_image("out.png", &src_image);

out:
    deinit_post_process();

    ret = release_yolov5_model(&rknn_app_ctx);
    if (ret != 0)
    {
        printf("release_yolov5_model fail! ret=%d\n", ret);
    }

    if (src_image.virt_addr != nullptr)
    {
#if defined(RV1106_1103) 
        dma_buf_free(rknn_app_ctx.img_dma_buf.size, &rknn_app_ctx.img_dma_buf.dma_buf_fd, 
                rknn_app_ctx.img_dma_buf.dma_buf_virt_addr);
#else
        std::free(src_image.virt_addr);
#endif
    }

    return 0;
}
