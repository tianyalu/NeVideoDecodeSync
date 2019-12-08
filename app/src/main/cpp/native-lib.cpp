#include <jni.h>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_sty_ne_video_decodesync_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(av_version_info());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_sty_ne_video_decodesync_NePlayer_native_1start(JNIEnv *env, jobject thiz, jstring path,
                                                        jobject surface) {
    const char* pathChar = env->GetStringUTFChars(path, 0);
    //FFmpeg 视频绘制 音频绘制
    //初始化网络模块
    avformat_network_init();
    //总上下文
    AVFormatContext * formatContext = avformat_alloc_context();
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "timeout", "3000000", 0); //3s
    int ret = avformat_open_input(&formatContext, pathChar, NULL, &opts); //0:成功 非0：失败
    if(ret) {
        return;
    }

    //视频流
    int video_stream_idx = -1;
    //通知FFmpeg将视频流解析处理
    avformat_find_stream_info(formatContext, NULL);
    for(int i = 0; i < formatContext->nb_streams; i++){
        if(formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }
    //视频流索引
    AVCodecParameters* codecpar = formatContext->streams[video_stream_idx]->codecpar;

    //解码器 h264  java 策略 key id
    AVCodec* dec = avcodec_find_decoder(codecpar->codec_id);
    //解码器的上下文
    AVCodecContext* codecContext = avcodec_alloc_context3(dec);
    //将解码器参数copy到解码器上下文
    avcodec_parameters_to_context(codecContext, codecpar);

    //打开解码器
    avcodec_open2(codecContext, dec, NULL);
    //解码-->yuv数据
    //AVPacket malloc() new AVPacket()
    AVPacket* packet = av_packet_alloc();
    //从视频流中读取数据包
    av_read_frame(formatContext, packet);


    env->ReleaseStringUTFChars(path, pathChar);
}