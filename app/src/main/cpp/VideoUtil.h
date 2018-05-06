
/**
 * Created by zhandalin on 2017-08-09 20:46.
 * 说明:
 */


#ifndef BZFFMPEG_VIDEOUTIL_H
#define BZFFMPEG_VIDEOUTIL_H

extern "C" {
#include <libavformat/avformat.h>
};
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR , "main", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , "main", __VA_ARGS__)
class VideoUtil {
public:
    static int getVideoInfo(const char *videoPath, void (*sendMediaInfoCallBack)(
            int, int));

    static int getVideoWidth(const char *videoPath);

    static AVFrame *allocVideoFrame(enum AVPixelFormat pix_fmt, int width, int height);

    static AVFrame *allocAudioFrame(enum AVSampleFormat sample_fmt,
                                    uint64_t channel_layout,
                                    int sample_rate, int nb_samples);

    static int getVideoHeight(const char *videoPath);

    static int getVideoRotate(const char *videoPath);

    static int printVideoTimeStamp(const char *videoPath);

    static long getVideoDuration(const char *videoPath);

    static int openInputFile(const char *filename, AVFormatContext **in_fmt_ctx);

    static int openInputFileForSoft(const char *filename, AVFormatContext **in_fmt_ctx);

    //打开输入文件,同时探测解码器是否可用,并打开对应的编码器
    static int
    openInputFile(const char *filename, AVFormatContext **in_fmt_ctx, bool openVideoDecode,
                  bool openAudioDecode);

    static int openAVCodecContext(AVStream *inStream);

    //根据一个已有的视频信息打开一个视频
    static int openOutputFile(AVFormatContext *in_fmt_ctx, AVFormatContext **out_fmt_ctx,
                              const char *output_path, bool needAudio = true);

    static int64_t getBitRate(int videoWidth, int videoHeight, bool allFrameIsKey);

    static bool videoPacketSort(AVPacket *avPacket_1, AVPacket *avPacket_2) {
        if (NULL == avPacket_1 || NULL == avPacket_2)return false;
        return avPacket_1->dts < avPacket_2->dts;
    }
};


#endif //BZFFMPEG_VIDEOUTIL_H
