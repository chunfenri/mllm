//
// Created by ey on 23-12-18.
//

#include "Convolution.hpp"

void conv2d_fp32_VALID(Tensor* input, Tensor* output, Tensor* kernel, bool support_bias, Tensor* bias, int stride_h, int stride_w) {
    int in_height = input->head();
    int in_width = input->dimension();
    int in_channel = input->sequence();
    int kernel_h = kernel->head();
    int kernel_w = kernel->dimension();
    int out_height = output->head();
    int out_width = output->dimension();
    int out_channel = kernel->batch();
    for (int b = 0; b < input->batch(); ++b) {
#pragma omp parallel for num_threads(4)
        for (int out_ch = 0; out_ch < out_channel; ++out_ch) {
            for (int out_h = 0; out_h < out_height; ++out_h) {
                for (int out_w = 0; out_w < out_width; ++out_w) {
                    int blk_h = out_h * stride_h;
                    int blk_w = out_w * stride_w;
                    // set value;
                    float value = 0;
                    for (int in_ch = 0; in_ch < in_channel; ++in_ch) {
                        for (int k_h = 0; k_h < kernel_h; ++k_h) {
                            float* kernel_p = kernel->ptrAt<float>(out_ch, k_h, in_ch, 0);
                            float tmp_value;
                            vec_dot_fp32(kernel_w, &tmp_value, kernel_p, input->ptrAt<float>(b, blk_h+k_h, in_ch, blk_w+0));
                            value += tmp_value;
                        }
                    }
                    if (support_bias) {
                        value += *bias->ptrAt<float>(0, 0, 0, out_ch);
                    }
                    *output->ptrAt<float>(b, out_h, out_ch, out_w) = value;
                }
            }
        }
    }
}


void conv2d_fp32_SAME(Tensor* input, Tensor* output, Tensor* kernel, bool support_bias, Tensor* bias, int stride_h, int stride_w, int padding_h, int padding_w) {
    int padding_top = padding_h ;
    int padding_left = padding_w ;
    
    int in_height = input->head();
    int in_width = input->dimension();
    int in_channel = input->sequence();
    int kernel_h = kernel->head();
    int kernel_w = kernel->dimension();
    int out_height = output->head();
    int out_width = output->dimension();
    int out_channel = kernel->batch();
    for (int b = 0; b < input->batch(); ++b) {
#pragma omp parallel for num_threads(4)
        for (int out_ch = 0; out_ch < out_channel; ++out_ch) {
            for (int out_h = 0; out_h < out_height; ++out_h) {
                for (int out_w = 0; out_w < out_width; ++out_w) {
                    int blk_h = out_h * stride_h - padding_top;
                    int blk_w = out_w * stride_w - padding_left;
                    // set value;
                    int start_k_h = 0;
                    int start_k_w = 0;
                    float value = 0;
                    if(blk_h<0) {
                        assert(padding_top = -blk_h);
                        start_k_h += padding_top;
                    }
                    int vec_dot_n = kernel_w;
                    if (blk_w<0) {
                        assert(padding_left = -blk_w);
                        start_k_w += padding_left;
                        vec_dot_n -= padding_left;
                    }
                    if(blk_w+ kernel_w > in_width) {
                        vec_dot_n -= (blk_w+ kernel_w - in_width);
                    }
                    for (int in_ch = 0; in_ch < in_channel; ++in_ch) {
                        for (int k_h = start_k_h; k_h < kernel_h & blk_h+k_h<in_height ; ++k_h) {
                            float* kernel_p = kernel->ptrAt<float>(out_ch, k_h, in_ch, 0+start_k_w);
                            float tmp_value;
                            vec_dot_fp32(vec_dot_n, &tmp_value, kernel_p, input->ptrAt<float>(b, blk_h+k_h, in_ch, blk_w+start_k_w));
                            value += tmp_value;
                        }
                    }
                    if (support_bias) {
                        value += *bias->ptrAt<float>(0, 0, 0, out_ch);
                    }
                    *output->ptrAt<float>(b, out_h, out_ch, out_w) = value;
                }
            }
        }
    }
}