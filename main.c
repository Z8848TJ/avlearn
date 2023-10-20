#include <stdio.h>
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include <string.h>


void get_data();


int main() {
    get_data();
}

SwrContext* init_swr() {
    SwrContext *swr_ctx= NULL;

    swr_alloc_set_opts2(&swr_ctx,
                        &(AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO,   // 输出 channel 布局
                        AV_SAMPLE_FMT_S16,                           // 输出采样大小
                        48000,                                      // 输出采样率
                        &(AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO,  // 输入 channel 布局
                        AV_SAMPLE_FMT_S16,
                        88200,
                        0, NULL);

    if(!swr_ctx) {
        av_log(NULL, AV_LOG_INFO, "采样上下文创建失败");
    }

    if(swr_init(swr_ctx) < 0) {
        av_log(NULL, AV_LOG_INFO, "采样上下文初始化失败");
    }

    return swr_ctx;
}

// 打开编码器
AVCodecContext* open_coder() {

    // 创建编码器
    const AVCodec *codec = avcodec_find_encoder_by_name("libopus");


    // 创建 codec 上下文
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);

    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    codec_ctx->sample_rate = 48000;  // 采样率
    codec_ctx->ch_layout = (AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO;


    // 打开编码器
    if(avcodec_open2(codec_ctx, codec, NULL) < 0) {
        av_log(NULL, AV_LOG_INFO, "打开编码器失败");
        return NULL;
    }

    return codec_ctx;
}

void encode(AVCodecContext *ctx,
            AVFrame *frame,
            AVPacket *pkt,
            FILE *out_put) {

    int ret;
    // 将数据送到编码器
    ret = avcodec_send_frame(ctx, frame);

    // ret >= 0 说明数据设置成功
    while(ret >= 0) {
        // 获取编码后的音频数据，如果成功需要重复获取，直到失败为止。
        ret = avcodec_receive_packet(ctx, pkt);

        if (ret == (AVERROR(EAGAIN)) || ret == AVERROR_EOF) {
            return;
        } else if(ret < 0){
            printf("Error, encoding audio frame\n");
            exit(-1);
        }
        // 写入文件
        //fwrite(pkt.data, pkt.size, 1, outfile);
        fwrite(pkt->data, pkt->size, 1, out_put);

        // 执行刷盘
        fflush(out_put);
    }

}



AVFormatContext* open_device(char* in_format_name, char* device_name) {

    AVFormatContext  *fmt_ctx = NULL;

    AVDictionary  *options = NULL;


    // 获取格式
    const AVInputFormat *format = av_find_input_format(in_format_name);

    char errors[1024];

    int ret;

    // 注册所有设备
    avdevice_register_all();

    // 打开设备
    if((ret = avformat_open_input(&fmt_ctx, device_name, format, &options)) < 0) {
        av_strerror(ret, errors, 1024);

        return NULL;
    }

    return fmt_ctx;
}

// 对应这段命令：ffmpeg -f dshow -i audio="麦克风 (Realtek(R) Audio)" out.wav
void get_data() {

    // 创建输入缓存区
    uint8_t **src_data = NULL;
    int src_line_size = 0;
    av_samples_alloc_array_and_samples(
            &src_data,  // 输入缓存区地址
            &src_line_size, // 输入缓存区的大小
            2,         // 通道个数
            22050,     // 单通道采样率
            AV_SAMPLE_FMT_S16,  // 采样大小
            0);


    // 创建输出缓存区
    uint8_t **dst_data = NULL;
    int dst_line_size = 0;
    av_samples_alloc_array_and_samples(
            &dst_data,  // 输出缓存区地址
            &dst_line_size, // 输出缓存区的大小
            2,         // 通道个数
            12000,     // 单通道采样率
            AV_SAMPLE_FMT_S16,  // 采样大小
            0);


    AVFormatContext  *fmt_ctx = open_device("dshow", "麦克风 (Realtek(R) Audio)");

    AVPacket pkt;

    // 创建文件 wb+：写入二进制并创建文件
    char *out = "D:/document/source/audio.opus";
    FILE *outfile = fopen(out, "wb+");

    // 打开编码器
    AVCodecContext *codec_ctx = open_coder();

    if(!codec_ctx) {
        return;
    }

    // 音频输入数据
    AVFrame *frame = av_frame_alloc();
    if(!frame) {
        av_log(NULL, AV_LOG_INFO, "音频输入数据空间分配失败\n");
        return;
    }

    frame->nb_samples = 960; // 单通道一个音频帧的采样数
    frame->format = AV_SAMPLE_FMT_S16;
    frame->ch_layout = (AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO;


    av_frame_get_buffer(frame, 0); // buffer 的大小 22050 * 16 / 8 = 44100

    if(!frame->buf[0]) {
        av_log(NULL, AV_LOG_INFO, "音频输入数据缓存区分配失败\n");
        return;
    }

    // 音频输出数据
    AVPacket  *new_pkt = av_packet_alloc();
    if(!new_pkt) {
        av_log(NULL, AV_LOG_INFO, "音频输出数据缓存区分配失败\n");
        return;
    }

    // 重采样缓存区
    SwrContext* swr_ctx = init_swr();

    int count = 0;

    // 从音频设备中读取数据
    while(av_read_frame(fmt_ctx, &pkt) == 0 && count++ < 20) {

        av_log(NULL, AV_LOG_INFO, "packet size is %d(%p), count = %d \n", pkt.size, pkt.data, count);

        // 进行内存拷贝，按字节拷贝
        memcpy((void*)src_data[0], (void*)pkt.data, pkt.size);

        // 重采样
        swr_convert(swr_ctx,   // 从采样的上下文
                    dst_data, // 输出缓存区
                    12000, // 输出每个通道的采样数
                    (const uint8_t **) src_data, // 输入缓存区
                    22050);                 // 输入单个通道的采样数

        // 将重采样的数据拷贝到 frame 中
        memcpy((void *)frame->data[0], dst_data[0], dst_line_size);

        // 编码器编码
        encode(codec_ctx, frame, new_pkt, outfile);

        // 释放 pkt
        av_packet_unref(&pkt);
    }

    // 关闭文件
    fclose(outfile);

    // 释放输入输出缓存区
    if(src_data) {
        av_freep(&src_data[0]);
    }
    av_freep(src_data);

    if(dst_data) {
        av_freep(&dst_data[0]);
    }
    av_freep(dst_data);

    // 关闭上下文
    avformat_close_input(&fmt_ctx);
    // 释放重采样上线
    swr_free(&swr_ctx);

    av_log(NULL, AV_LOG_INFO, "finish! \n");

}
