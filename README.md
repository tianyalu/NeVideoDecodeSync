# NeVideoDecodeSync 视频解码及同步处理
## 一、概念
### 1.1 视频解码处理流程（理论层）
图一：  
![image](https://github.com/tianyalu/NeVideoDecodeSync/blob/master/show/video_transfer_process1.png)  
图二：  
![image](https://github.com/tianyalu/NeVideoDecodeSync/blob/master/show/video_transfer_process2.png)  

### 1.2 视频解码处理流程（代码层）
![image](https://github.com/tianyalu/NeVideoDecodeSync/blob/master/show/video_decode_process1.png)   
![image](https://github.com/tianyalu/NeVideoDecodeSync/blob/master/show/video_decode_process2.png)  
 
### 二、核心代码
#### 2.1 CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.4.1)

# file(GLOB SOURCE src/main/cpp/*.cpp)
file(GLOB SOURCE ${CMAKE_SOURCE_DIR}/*.cpp)
add_library( # Sets the name of the library.
        neplayer
        SHARED
        ${SOURCE})

find_library( # Sets the name of the path variable.
        log-lib
        log)

# include_directories(src/main/cpp/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
set(my_lib_path ${CMAKE_SOURCE_DIR}/../../../libs/${CMAKE_ANDROID_ARCH_ABI})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${my_lib_path}")

# message(WARNING "cmake_source_dir = ${CMAKE_SOURCE_DIR}") #E:/AndroidWangYiCloud/NDKWorkspace/NeVideoDecodeSync/app/src/main/cpp

target_link_libraries( # Specifies the target library.
        neplayer
        # avcodec avfilter avformat avutil swresample swscale
        avfilter avformat avcodec avutil swresample swscale
        ${log-lib}
        android #系统库,在 D:\AndroidDev\AndroidStudio\sdk\ndk-bundle\platforms\android-21\arch-arm\usr\lib
        z
        OpenSLES)
```
#### 2.2 native-lib.cpp
```c++
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

    //释放资源
    avcodec_close(codecContext);
    env->ReleaseStringUTFChars(path, pathChar);
}
```
#### 2.3 MainActivity.java
```java
public class MainActivity extends AppCompatActivity {
    private static final int MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE = 1;
    private static final String FILE_DIR = Environment.getExternalStorageDirectory() + File.separator
            + "sty" + File.separator; //  /storage/emulated/0/sty/
    private SurfaceView surfaceView;
    private SeekBar seekBar;
    private Button btnOpen;

    private NePlayer nePlayer;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("neplayer");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        initView();
        requestPermission();
    }

    private void initView() {
        surfaceView = findViewById(R.id.surface_view);
        seekBar = findViewById(R.id.seek_bar);
        btnOpen = findViewById(R.id.btn_open);

        nePlayer = new NePlayer();
        nePlayer.setSurfaceView(surfaceView);

        btnOpen.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                open();
            }
        });
    }

    private void open() {
        File file = new File(FILE_DIR, "input.mp4");
        nePlayer.start(file.getAbsolutePath());
    }

    private void requestPermission() {
        if(ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED){
            if(ActivityCompat.shouldShowRequestPermissionRationale(this,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE)){
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE);
            }else {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        switch (requestCode) {
            case MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE: {
                if(grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED){
                    Log.i("sty", "onRequestPermissionResult granted");
                }else {
                    Log.i("sty", "onRequestPermissionResult denied");
                    showWarningDialog();
                }
                break;
            }
            default:
                break;
        }
    }

    private void showWarningDialog() {
        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle("警告")
                .setMessage("请前往设置->应用—>PermissionDemo->权限中打开相关权限，否则功能无法正常使用！")
                .setPositiveButton("确定", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {

                        //finish();
                    }
                }).show();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
}
```

### 三、踩坑
#### 3.1 undefined reference 
项目运行时报错信息如下：  
```text
E:\AndroidWangYiCloud\NDKWorkspace\NeVideoDecodeSync\app\build\intermediates\cmake\debug\obj\armeabi-v7a\libneplayer.so CMakeFiles/neplayer.dir/native-lib.cpp.o  -lavcodec -lavfilter -lavformat -lavutil -lswresample -lswscale -llog -landroid -lz -lOpenSLES -latomic -lm && cd ."
  libavformat/utils.c:5554: error: undefined reference to 'av_bitstream_filter_filter'
  libavformat/codec2.c:74: error: undefined reference to 'avpriv_codec2_mode_bit_rate'
  libavformat/codec2.c:75: error: undefined reference to 'avpriv_codec2_mode_frame_size'
  libavformat/codec2.c:76: error: undefined reference to 'avpriv_codec2_mode_block_align'
  libavformat/spdifdec.c:63: error: undefined reference to 'av_adts_header_parse'
  clang++: error: linker command failed with exit code 1 (use -v to see invocation)
  ninja: build stopped: subcommand failed.
```
**原因：** 引入的FFmpeg静态库顺序有问题，后面的库会用到前面库的方法。  
**解决方案：**  
CMakeLists.txt文件中target_link_libraries参数中的  
```text
avcodec avfilter avformat avutil swresample swscale
```
改为：  
```text
avfilter avformat avcodec avutil swresample swscale
```

#### 3.2 `avformat_open_input()` 方法返回-13
解决上面的坑后，终于能正常编译了，但是安装到手机上后，点击“打开”按钮却播放不了。经过调试后发现是`avformat_open_input()`
方法返回-13了，查网上说是权限问题，我本以为在Manifest文件中添加`android.permission.WRITE_EXTERNAL_STORAGE`权限
便万事大吉了，结果还是被现实打了脸。后来想了想，这个权限对于Android6.0以上是危险权限，可能需要动态申请，于是在MainActivity
中动态申请权限，结果真的解决问题了--这个应该是自己坑了自己，唉！  
**总结：**  
除了在Manifest文件中添加权限外，还有在MainActivity中动态申请权限。

### 四、遗留的问题
* 疑惑一：视频播放速度明显加快，不知何故。  
* 疑惑二：视频播放结束后Activity自动关闭了，不知何故。