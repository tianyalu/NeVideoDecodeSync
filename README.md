# NeVideoDecodeSync 视频解码及同步处理
## 一、概念
### 1.1 视频传输处理流程（理论层）
图一：  
![image](https://github.com/tianyalu/NeVideoDecodeSync/blob/master/show/video_transfer_process1.png)  
图二：  
![image](https://github.com/tianyalu/NeVideoDecodeSync/blob/master/show/video_transfer_process2.png)  

### 1.1 视频解码处理流程（代码层）
![image](https://github.com/tianyalu/NeVideoDecodeSync/blob/master/show/video_decode_process1.png)   
![image](https://github.com/tianyalu/NeVideoDecodeSync/blob/master/show/video_decode_process2.png)  
 
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
原因：引入的FFmpeg静态库顺序有问题，后面的库会用到前面库的方法。  
解决方案：  
CMakeLists.txt文件中target_link_libraries参数中的  
```cmake
avcodec avfilter avformat avutil swresample swscale
```
改为：  
```cmake
avfilter avformat avcodec avutil swresample swscale
```