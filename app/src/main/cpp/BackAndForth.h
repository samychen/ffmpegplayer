//
/**
 * Created by zhandalin on 2018-05-02 10:40.
 * 说明:来回效果处理
 */
//

#ifndef BZFFMPEG_BACKANDFORTH_H
#define BZFFMPEG_BACKANDFORTH_H

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
};
#include <android/log.h>
#include <list>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR , "main", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , "main", __VA_ARGS__)

using namespace std;

class BackAndForth {
public:
    int handleBackAndForth(const char *inputPath, const char *outputPath);

private:
    AVFormatContext *in_fmt_ctx = nullptr;
    AVCodecContext *inAVCodecContext = nullptr;
    AVCodecContext *outAVCodecContext = nullptr;

    AVFormatContext *out_fmt_ctx = nullptr;
    list<AVPacket *> videoPacketList;
    list<int64_t> ptsList;
    list<int64_t> ptsList_2;
    SwsContext *sws_video_ctx = NULL;
    int64_t endPts = 0;
    long encodeIndex = 0;

    int handleAGopFrame(list<AVPacket *> *aGopPacketList);

    int initEncode();

    int readPacket();

    int reverseVideo();

    int normalVideo();

    int flushEncodeBuffer();

    int releaseResource();
};


#endif //BZFFMPEG_BACKANDFORTH_H
