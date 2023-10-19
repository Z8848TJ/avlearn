#include <stdio.h>
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include <string.h>


void get_data();

void create_file();

int main() {
    get_data();
}

SwrContext* init_swr() {
    SwrContext *swr_ctx= NULL;

    swr_alloc_set_opts2(&swr_ctx,
                        &(AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO,   // 输出 channel 布局
                        AV_SAMPLE_FMT_S32,                           // 输出采样大小
                        44100,                                      // 输出采样率
                        &(AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO,  // 输入 channel 布局
                        AV_SAMPLE_FMT_S16,
                        44100,
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
    codec_ctx->sample_rate = 44100;
    codec_ctx->ch_layout = (AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO;

    // 打开编码器
    if(avcodec_open2(codec_ctx, codec, NULL) < 0) {
        av_log(NULL, AV_LOG_INFO, "打开编码器失败");
        return NULL;
    }

    return codec_ctx;
}


// 对应这段命令：ffmpeg -f dshow -i audio="麦克风 (Realtek(R) Audio)" out.wav
void get_data() {
    char errors[1024];

    AVFormatContext  *fmt_ctx = NULL;

    char *device_name = "audio=麦克风 (Realtek(R) Audio)";

    AVDictionary  *options = NULL;

    // 注册音频设备
    avdevice_register_all();

    // 获取格式
    const AVInputFormat *format = av_find_input_format("dshow");

    int ret;

    // 打开设备
    if((ret = avformat_open_input(&fmt_ctx, device_name, format, &options)) < 0) {
        av_strerror(ret, errors, 1024);

        return;
    }

    AVPacket pkt;

    // 创建文件 wb+：写入二进制并创建文件
    char *out = "D:/document/source/audio.pcm";
    FILE *outfile = fopen(out, "wb+");

    AVCodecContext *codec_ctx = open_coder();

    // 重采样缓存区
    SwrContext* swr_ctx = init_swr();

    int count = 0;

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
            22050,     // 单通道采样率
            AV_SAMPLE_FMT_S32,  // 采样大小
            0);

    // 从音频设备中读取数据
    while(av_read_frame(fmt_ctx, &pkt) == 0 && count++ < 20) {

        av_log(NULL, AV_LOG_INFO, "packet size is %d(%p), count = %d \n", pkt.size, pkt.data, count);

        // 进行内存拷贝，按字节拷贝
        memcpy((void*)src_data[0], (void*)pkt.data, pkt.size);

        // 重采样
        swr_convert(swr_ctx,   // 从采样的上下文
                    dst_data, // 输出缓存区
                    22050, // 输出每个通道的采样数
                    (const uint8_t **) src_data, // 输入缓存区
                    22050);                 // 输入单个通道的采样数

        // 写入文件
        //fwrite(pkt.data, pkt.size, 1, outfile);
        fwrite(dst_data[0], dst_line_size, 1, outfile);

        // 执行刷盘
        fflush(outfile);

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



void create_file() {
    char *out = "D:\\document\\source\\audio.txt";
    FILE *outfile = fopen(out, "w");

    fputs("hello world", outfile);

    fclose(outfile);
}
