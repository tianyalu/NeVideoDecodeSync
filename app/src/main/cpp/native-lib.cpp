#include <jni.h>
#include <string>
#include <unistd.h>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <android/native_window_jni.h>
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
    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
    const char* pathChar = env->GetStringUTFChars(path, 0);
    av_register_all();
    //FFmpeg 视频绘制 音频绘制
    //初始化网络模块
    avformat_network_init();
    //总上下文
    AVFormatContext * formatContext = avformat_alloc_context();
//    AVInputFormat* iformat=av_find_input_format("h264");
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "timeout", "3000000", 0); //3s
    int ret = avformat_open_input(&formatContext, pathChar, NULL, &opts); //0:成功 非0：失败
//    int ret = avformat_open_input(&formatContext, pathChar, NULL, NULL); //0:成功 非0：失败
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
    /**
     * flags参数含义：
     * 重视速度：fast_bilinear, point
     * 重视质量：cubic, spline, lanczos
     * 缩小：
     * 重视质量：gauss, bilinear
     * 重视锐度：cubic, spline, lanczos
     */
    SwsContext* swsContext = sws_getContext(codecContext->width,codecContext->height,codecContext->pix_fmt,
            codecContext->width, codecContext->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, 0, 0, 0);
    ANativeWindow_setBuffersGeometry(nativeWindow, codecContext->width, codecContext->height,
            WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer outBuffer;

    while(av_read_frame(formatContext, packet) >= 0) {
        avcodec_send_packet(codecContext, packet);
        //FFmpeg malloc
        AVFrame* frame =  av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if(ret == AVERROR(EAGAIN)) {
            continue;
        }else if(ret < 0) {
            avcodec_close(codecContext);
            break;
        }

        //接收容器(指针数组，是一个数组，里面存放多个指针)
        uint8_t* dst_data[4];
        //每一行的首地址
        int dst_linesize[4];
        av_image_alloc(dst_data, dst_linesize, codecContext->width, codecContext->height,
                AV_PIX_FMT_RGBA, 1);
        //绘制
        //Avframe yum --> image === dest_data ==>渲染surfaceView
        sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height, dst_data,
                dst_linesize);
        ANativeWindow_lock(nativeWindow, &outBuffer, NULL);
        //渲染 --> 把dest_data行拷贝到缓冲区，参考show/line_copy.png
        uint8_t* firstWindow = static_cast<uint8_t *>(outBuffer.bits);
        //输入源（rgb)的
        uint8_t* src_data = dst_data[0];
        //拿到一行有多少个字节 RGBA
        int destStride = outBuffer.stride * 4;
        int src_linesize = dst_linesize[0];
        for(int i = 0; i < outBuffer.height; i++) {
            //内存拷贝来进行渲染
            memcpy(firstWindow+ i*destStride, src_data+i*src_linesize, destStride);
        }

        ANativeWindow_unlockAndPost(nativeWindow);
        usleep(1000 * 16);
        av_frame_free(&frame);
    }


    env->ReleaseStringUTFChars(path, pathChar);
}