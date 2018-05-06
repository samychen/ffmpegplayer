#include <jni.h>
#include <string>
#include <android/log.h>
#include <queue>
#include <list>

#define LOG_I(...) __android_log_print(ANDROID_LOG_ERROR , "main", __VA_ARGS__)

#include <iostream>
#include "BackAndForth.h"

#define GOP_SIZE 30

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "libavfilter/avfilter.h"
#include "libavutil/opt.h"
}
using namespace std;

list<AVPacket *> packetList;
list<AVPacket *> packetList_2;
list<AVPacket *> packetList_3;
list<int64_t> ptsList;
list<int64_t> ptsList_2;
list<int64_t> ptsList_3;
int64_t lastpts;
AVFormatContext *ifmt_ctx = nullptr;
AVFormatContext *ofmt_ctx = nullptr;
SwsContext *swsContext = nullptr;
int videoindex = -1;
long encodeIndex = 0;
AVCodecContext *dec_ctx = nullptr;
AVCodecContext *enc_ctx = nullptr;

void swsConvert(int ret, list<AVFrame *> &videoFrameList, AVFrame *avFrame);

void printErr(int result) {
    char *error_info;
    av_strerror(result, error_info, 1024);
    __android_log_print(ANDROID_LOG_INFO, "main", "异常信息：%s", error_info);
}

void tr_init() {
    av_register_all();
    avcodec_register_all();
    av_log_set_level(AV_LOG_ERROR);
}

int tr_oneninput(const char *fileName) {
    ifmt_ctx = avformat_alloc_context();
    int ret = avformat_open_input(&ifmt_ctx, fileName, NULL, NULL);
    if (ret < 0) {
        return ret;
    }
    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if (ret >= 0) {
        LOG_I("open input stream successfully");
    }
    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream = ifmt_ctx->streams[i];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;
        if (!dec) {
            LOG_I("Failed to find decoder for stream #%u\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }
        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            LOG_I("Failed to allocate the decoder context for stream #%u\n", i);
            return AVERROR(ENOMEM);
        }
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (ret < 0) {
            LOG_I("Failed to copy decoder parameters to input decoder context "
                          "for stream #%u\n", i);
            return ret;
        }
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, NULL, NULL);
            if (ret < 0) {
                LOG_I("Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
        if (stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
        }
        stream->codec = codec_ctx;
    }
    return ret;
}

int tr_openOutput(AVFormatContext *in_fmt_ctx, AVFormatContext **out_fmt_ctx, const char *fileName) {
    int ret = 0;
    ret = avformat_alloc_output_context2(out_fmt_ctx, NULL, NULL, fileName);
    if (ret < 0) {
        LOG_I("alloc_output_context2 fail");
        return ret;
    }
    if (!out_fmt_ctx) {
        return -1;
    }

    for (int i = 0; i < in_fmt_ctx->nb_streams; ++i) {
        AVStream *in_stream = in_fmt_ctx->streams[i];
        if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        AVStream *out_stream = avformat_new_stream(*out_fmt_ctx, in_stream->codec->codec);
        if (!out_stream) {
            LOG_I("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            return ret;
        }
        ret = av_dict_copy(&out_stream->metadata, in_stream->metadata, AV_DICT_IGNORE_SUFFIX);
        if (ret < 0) {
            LOG_I("Failed allocating output stream\n");
            return ret;
        }
        out_stream->time_base = in_stream->time_base;

        avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (ret < 0) {
            LOG_I("Failed to copy context from input to output stream codec context\n");
            return ret;
        }
        ret = avcodec_parameters_from_context(out_stream->codecpar, in_stream->codec);
        if (ret < 0) {
            LOG_I("Could not copy the stream parameters\n");
            return -1;
        }
        (*out_fmt_ctx)->oformat->codec_tag = 0;
        out_stream->codec->codec_tag = 0;
        if ((*out_fmt_ctx)->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    av_dump_format(*out_fmt_ctx, 0, fileName, 1);

    /* open the output file, if needed */
    if (!((*out_fmt_ctx)->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&(*out_fmt_ctx)->pb, fileName, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOG_I("Could not open '%s': %s\n", fileName,
                 av_err2str(ret));
            return ret;
        }
    }
    /* Write the stream header, if any. */
    ret = avformat_write_header(*out_fmt_ctx, NULL);
    if (ret < 0) {
        LOG_I("Error occurred when opening output file: %s\n",
             av_err2str(ret));
        return ret;
    }
    return 0;
}

void tr_closeInput() {
    if (ifmt_ctx != NULL) {
        avformat_close_input(&ifmt_ctx);
    }
}

void tr_closeOutput() {
    if (ofmt_ctx != NULL) {
        for (int i = 0; i < ofmt_ctx->nb_streams; i++) {
            AVCodecContext *codecContext = ofmt_ctx->streams[i]->codec;
            avcodec_close(codecContext);
        }
        avformat_close_input(&ofmt_ctx);
    }
}

int64_t getBitRate(int videoWidth, int videoHeight, bool allFrameIsKey) {
    //bit_rate 设置
    int bitrateP = videoWidth > videoHeight ? videoWidth : videoHeight;
    //以720p为标准,每秒1M
    int64_t bit_rate = 1024 * 1024 * 8 * 2;//每秒1M;
    if (!allFrameIsKey) {//如果不全是关键帧,那么码率降低
        bit_rate /= 2;
    }
    return (int64_t) (bitrateP / 720.f * bit_rate);
}
int initEncoder() {
    int ret = 0;
    AVStream *inVideoStream;
    for (int i = 0; i < ifmt_ctx->nb_streams; ++i) {
        AVStream *avStream = ifmt_ctx->streams[i];
        if (avStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            dec_ctx = avStream->codec;
            inVideoStream = avStream;
            swsContext = sws_getContext(avStream->codecpar->width, avStream->codecpar->height,
                                        AV_PIX_FMT_YUV420P,
                                        avStream->codecpar->width, avStream->codecpar->height,
                                        AV_PIX_FMT_YUV420P,
                                        1, NULL, NULL, NULL);
            break;
        }
    }
    if (nullptr == inVideoStream) {
        LOG_I("can't find inVideoStream");
        return -1;
    }
    for (int i = 0; i < ofmt_ctx->nb_streams; ++i) {
        AVStream *avStream = ofmt_ctx->streams[i];
        if (avStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            avStream->duration *= 2;
            ofmt_ctx->duration *= 2;
            avStream->time_base = inVideoStream->time_base;
            AVCodec *avCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
            if (!avCodec) {
                LOG_I("Could not find encoder for '%s'\n",
                      avcodec_get_name(AV_CODEC_ID_H264));
                return -1;
            }
            enc_ctx = avcodec_alloc_context3(avCodec);
            avcodec_parameters_to_context(enc_ctx, avStream->codecpar);
            int64_t bit_rate = getBitRate(avStream->codecpar->width,avStream->codecpar->height,
                                          false);
            enc_ctx->bit_rate = bit_rate;
            enc_ctx->bit_rate_tolerance = (int) (bit_rate * 2);
            enc_ctx->time_base = (AVRational) {inVideoStream->avg_frame_rate.den,
                                               inVideoStream->avg_frame_rate.num};

            enc_ctx->gop_size = GOP_SIZE;
            ofmt_ctx->oformat->codec_tag = 0;
            enc_ctx->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            AVDictionary *param = NULL;
            av_dict_set(&param, "preset", "ultrafast", 0);
            enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
            ret = avcodec_open2(enc_ctx, avCodec, &param);
            if (ret < 0) {
                LOG_I("avCodecContext Could not open video codec: %s\n", av_err2str(ret));
                return -1;
            }
            avcodec_parameters_from_context(avStream->codecpar, enc_ctx);
            avStream->codec = enc_ctx;
            break;
        }
    }
    return ret;
}

int flush_encode(AVCodecContext *codexCtx) {
    int ret, got_picture_ptr = 0;
    AVPacket *enc_pkt = av_packet_alloc();
    if (!(codexCtx->codec->capabilities &
          CODEC_CAP_DELAY)) {
        LOG_I("capabilities error");
        return -1;
    }
    LOG_I("start while");
    while (true) {
        LOG_I("Flushing stream #%u encoder\n");
        av_init_packet(enc_pkt);
        ret = avcodec_encode_video2(codexCtx, enc_pkt, NULL,
                                    &got_picture_ptr);
        if (ret < 0 || !got_picture_ptr) {
            break;
        }
        if (!ptsList.empty()) {
            enc_pkt->pts = ptsList.front();
            enc_pkt->dts = ptsList.front();
            ptsList.pop_front();
        } else if (!ptsList_2.empty()) {
            enc_pkt->pts = lastpts + ptsList_2.front();
            enc_pkt->dts = lastpts + ptsList_2.front();
            ptsList_2.pop_front();
        }
        LOG_I("编码成功1帧！\n");
        enc_pkt->stream_index = ofmt_ctx->streams[0]->index;
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
        if (ret < 0) {
            LOG_I("Error muxing packet\n");
        }
        av_packet_unref(enc_pkt);
    }
    return ret;
}

int encodeGopGroup(list<AVPacket *> *aGopPacketList) {
    if (NULL == aGopPacketList || aGopPacketList->empty() || NULL == ifmt_ctx || NULL == ofmt_ctx) {
        LOG_I("encodeGopGroup packet error");
        return -1;
    }
    avcodec_flush_buffers(dec_ctx);
    int ret = 0, got_picture_ptr = 0;
    list<AVFrame *> videoFrameList;
    list<AVPacket *>::iterator curpacket = aGopPacketList->begin();
    while (curpacket != aGopPacketList->end()) {
        AVPacket *avPacket = *curpacket;
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(dec_ctx, avFrame, &got_picture_ptr, avPacket);
        if (ret < 0) {
            LOG_I("avcodec_decode_video2 fail");
            av_frame_unref(avFrame);
            break;
        }
        if (!got_picture_ptr) {
            curpacket++;
            av_frame_unref(avFrame);
            continue;
        }
        swsConvert(ret, videoFrameList, avFrame);
        curpacket++;
    }
    //刷新解码缓存
    while (true) {
        AVPacket *decodeAVPacket = av_packet_alloc();
        av_init_packet(decodeAVPacket);
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(dec_ctx, avFrame, &got_picture_ptr, decodeAVPacket);
        if (ret < 0 || (!got_picture_ptr)) {
            LOG_I("flush avcodec_decode_video2 fail");
            av_packet_free(&decodeAVPacket);
            av_frame_unref(avFrame);
            break;
        }
        swsConvert(ret, videoFrameList, avFrame);
        av_packet_free(&decodeAVPacket);
    }
    //编码
    list<AVFrame *>::iterator startFrame = videoFrameList.begin();
    while (startFrame != videoFrameList.end()) {
        AVPacket *avPacket = av_packet_alloc();
        av_init_packet(avPacket);
        AVFrame *avFrame = *startFrame;
        avFrame->pts = encodeIndex;
        avFrame->pkt_dts = encodeIndex;
        encodeIndex++;
        ret = avcodec_encode_video2(enc_ctx, avPacket, avFrame,
                                    &got_picture_ptr);
        if (ret < 0) {
            LOG_I("avcodec_encode_video2 fail");
            av_packet_unref(avPacket);
            break;
        }
        if (!got_picture_ptr) {
            startFrame++;
            av_packet_unref(avPacket);
            continue;
        }
        if (!ptsList_2.empty()) {
            avPacket->pts = lastpts+ptsList_2.front();
            avPacket->dts = lastpts+ptsList_2.front();
            avPacket->stream_index = ofmt_ctx->streams[0]->index;
            ret = av_interleaved_write_frame(ofmt_ctx, avPacket);
            if (ret < 0) {
                LOG_I("Error muxing packet\n");
            }
            ptsList_2.pop_front();
        }
        av_packet_unref(avPacket);
        startFrame++;
    }
    //释放内存
    startFrame = videoFrameList.begin();
    while (startFrame != videoFrameList.end()) {
        av_frame_free(&(*startFrame));
        startFrame++;
    }
    return 0;
}

void swsConvert(int ret, list<AVFrame *> &videoFrameList, AVFrame *avFrame) {
    AVFrame *outAVFrame = av_frame_alloc();
    outAVFrame->format = enc_ctx->pix_fmt;
    outAVFrame->width = enc_ctx->width;
    outAVFrame->height = enc_ctx->height;
    ret = av_frame_get_buffer(outAVFrame, 4);
    if (ret < 0) {
        printErr(ret);
    }
    sws_scale(swsContext,
                  (const uint8_t *const *) avFrame->data,
                  avFrame->linesize, 0, outAVFrame->height,
                  outAVFrame->data,
                  outAVFrame->linesize);
    videoFrameList.push_front(outAVFrame);
    av_frame_unref(avFrame);
}


void normalTranscode() {
    int ret = 0;
    list<AVPacket *>::iterator normalPacket = packetList.begin();
    int got_picture_ptr=0;
    while (normalPacket != packetList.end()) {
        AVPacket *avPacket = *normalPacket;
        normalPacket++;
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(dec_ctx, avFrame, &got_picture_ptr,
                                    avPacket);
        if (ret < 0) {
            LOGE("avcodec_decode_video2 fail");
            av_frame_unref(avFrame);
            break;
        }
        if (!got_picture_ptr) {
            av_frame_unref(avFrame);
            continue;
        }
        AVFrame *outAVFrame = av_frame_alloc();
        outAVFrame->format = enc_ctx->pix_fmt;
        outAVFrame->width = enc_ctx->width;
        outAVFrame->height = enc_ctx->height;
        ret = av_frame_get_buffer(outAVFrame, 4);
        if (ret < 0) {
            LOGE("Could not allocate frame data.\n");
        }
        sws_scale(swsContext,
                  (const uint8_t *const *) avFrame->data,
                  avFrame->linesize, 0, outAVFrame->height,
                  outAVFrame->data,
                  outAVFrame->linesize);

        av_frame_unref(avFrame);
        AVPacket *encode_pkt = av_packet_alloc();
        outAVFrame->pts = encodeIndex;
        encodeIndex++;
        ret = avcodec_encode_video2(enc_ctx, encode_pkt, outAVFrame,
                                    &got_picture_ptr);
        av_frame_unref(outAVFrame);
        if (ret < 0) {
            LOGD("avcodec_encode_video2 fail");
            av_packet_unref(encode_pkt);
            break;
        }
        if (!got_picture_ptr) {
            av_packet_unref(encode_pkt);
            continue;
        }
        if (!ptsList.empty()) {
            encode_pkt->pts = ptsList.front();
            encode_pkt->dts = ptsList.front();
            ptsList.pop_front();
        }
        encode_pkt->stream_index = ofmt_ctx->streams[0]->index;
        ret = av_interleaved_write_frame(ofmt_ctx, encode_pkt);
        if (ret < 0) {
            LOGE("Error muxing packet\n");
        }
        av_packet_free(&encode_pkt);
    }
    //刷新缓存
    while (true) {
        AVPacket *avPacket = av_packet_alloc();
        av_init_packet(avPacket);
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(dec_ctx, avFrame, &got_picture_ptr,
                                    avPacket);
        if (ret < 0 || !got_picture_ptr) {
            LOGD("刷新缓存结束");
            av_frame_unref(avFrame);
            break;
        }
        AVFrame *outAVFrame = av_frame_alloc();
        outAVFrame->format = enc_ctx->pix_fmt;
        outAVFrame->width = enc_ctx->width;
        outAVFrame->height = enc_ctx->height;
        ret = av_frame_get_buffer(outAVFrame, 4);
        if (ret < 0) {
            LOGE("Could not allocate frame data.\n");
        }
        sws_scale(swsContext,
                  (const uint8_t *const *) avFrame->data,
                  avFrame->linesize, 0, outAVFrame->height,
                  outAVFrame->data,
                  outAVFrame->linesize);

        //编码
        AVPacket *encode_pkt = av_packet_alloc();
        outAVFrame->pts = encodeIndex;
        encodeIndex++;
        ret = avcodec_encode_video2(enc_ctx, encode_pkt, outAVFrame,
                                    &got_picture_ptr);
        av_frame_unref(outAVFrame);
        if (ret < 0) {
            LOGD("avcodec_encode_video2 fail");
            av_packet_unref(encode_pkt);
            break;
        }
        if (!got_picture_ptr) {
            av_packet_unref(encode_pkt);
            continue;
        }
        if (!ptsList.empty()) {
            encode_pkt->pts = ptsList.front();
            encode_pkt->dts = ptsList.front();
            ptsList.pop_front();
        }
        encode_pkt->stream_index = ofmt_ctx->streams[0]->index;
        ret = av_interleaved_write_frame(ofmt_ctx, encode_pkt);
        if (ret < 0) {
            LOGE("Error muxing packet\n");
        }
        av_packet_free(&encode_pkt);
    }
}

void reverseTranscode() {
    list<AVPacket *>::reverse_iterator reversePacket = packetList.rbegin();
    list<AVPacket *> gopGroupList;
    while (reversePacket != packetList.rend()) {
        AVPacket *avPacket = *reversePacket;
        gopGroupList.push_front(avPacket);
        if (avPacket->flags == AV_PKT_FLAG_KEY) {
            encodeGopGroup(&gopGroupList);
            gopGroupList.clear();
        }
        reversePacket++;
    }
}

extern "C"
JNIEXPORT void JNICALL Java_com_example_gx_ffmpegplayer_MainActivity_test(
        JNIEnv *env, jobject jobj, jstring jinputFilePath, jstring joutputFilePath) {
    const char *cinputFilePath = env->GetStringUTFChars(jinputFilePath, NULL);
    const char *coutputFilePath = env->GetStringUTFChars(joutputFilePath, NULL);
    int ret;
    tr_init();
    ret = tr_oneninput(cinputFilePath);
    if (ret < 0 || NULL == ifmt_ctx) {
        LOG_I("openinput fail");
        return;
    }
    ret = tr_openOutput(ifmt_ctx, &ofmt_ctx, coutputFilePath);
    if (ret < 0) {
        return;
    }
    ret = initEncoder();
    if (ret < 0) {
        LOG_I("initEncoder fail");
        return;
    }
    LOG_I("open output success");
    while (true) {
        AVPacket *av_packet = av_packet_alloc();
        av_init_packet(av_packet);
        ret = av_read_frame(ifmt_ctx, av_packet);
        if (ret < 0) {
            LOG_I("读取完成");
            break;
        }
        if (av_packet->stream_index == videoindex) {
            ptsList.push_back(av_packet->pts);
            ptsList_2.push_back(av_packet->pts);
            packetList.push_back(av_packet);
        }
    }
    lastpts = ptsList.back();
    LOG_I("lastpts=%lld", lastpts);
    //正序保存
    normalTranscode();
    //倒序取出
    reverseTranscode();
    flush_encode(enc_ctx);
    av_write_trailer(ofmt_ctx);
//    ofmt_ctx->duration = ifmt_ctx->duration;
    avcodec_close(dec_ctx);
    avcodec_close(enc_ctx);
    tr_closeInput();
    tr_closeOutput();
    avformat_free_context(ifmt_ctx);
    avformat_free_context(ofmt_ctx);
    sws_freeContext(swsContext);
    ptsList.clear();
    ptsList_2.clear();
    packetList.clear();
    env->ReleaseStringUTFChars(jinputFilePath, cinputFilePath);
    env->ReleaseStringUTFChars(joutputFilePath, coutputFilePath);
}

int encodeGopGroup2(list<AVPacket *> *aGopPacketList,list<int64_t> *ptsList,int64_t endpts) {
    if (NULL == aGopPacketList || aGopPacketList->empty() || NULL == ifmt_ctx || NULL == ofmt_ctx) {
        LOG_I("encodeGopGroup packet error");
        return -1;
    }
    avcodec_flush_buffers(dec_ctx);
    int ret = 0, got_picture_ptr = 0;
    list<AVFrame *> videoFrameList;
    list<AVPacket *>::iterator curpacket = aGopPacketList->begin();
    while (curpacket != aGopPacketList->end()) {
        AVPacket *avPacket = *curpacket;
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(dec_ctx, avFrame, &got_picture_ptr, avPacket);
        if (ret < 0) {
            LOG_I("avcodec_decode_video2 fail");
            av_frame_unref(avFrame);
            break;
        }
        if (!got_picture_ptr) {
            curpacket++;
            av_frame_unref(avFrame);
            continue;
        }
        swsConvert(ret, videoFrameList, avFrame);
        curpacket++;
    }
    //刷新解码缓存
    while (true) {
        AVPacket *decodeAVPacket = av_packet_alloc();
        av_init_packet(decodeAVPacket);
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(dec_ctx, avFrame, &got_picture_ptr, decodeAVPacket);
        if (ret < 0 || (!got_picture_ptr)) {
            LOG_I("flush avcodec_decode_video2 fail");
            av_packet_free(&decodeAVPacket);
            av_frame_unref(avFrame);
            break;
        }
        swsConvert(ret, videoFrameList, avFrame);
        av_packet_free(&decodeAVPacket);
    }
    //编码
    list<AVFrame *>::iterator startFrame = videoFrameList.begin();
    while (startFrame != videoFrameList.end()) {
        AVPacket *avPacket = av_packet_alloc();
        av_init_packet(avPacket);
        AVFrame *avFrame = *startFrame;
        avFrame->pts = encodeIndex;
        avFrame->pkt_dts = encodeIndex;
        encodeIndex++;
        ret = avcodec_encode_video2(enc_ctx, avPacket, avFrame,
                                    &got_picture_ptr);
        if (ret < 0) {
            LOG_I("avcodec_encode_video2 fail");
            av_packet_unref(avPacket);
            break;
        }
        if (!got_picture_ptr) {
            startFrame++;
            av_packet_unref(avPacket);
            continue;
        }
        if (!(*ptsList).empty()) {
            avPacket->pts = endpts+(*ptsList).front();
            avPacket->dts = endpts+(*ptsList).front();
            avPacket->stream_index = ofmt_ctx->streams[0]->index;
            ret = av_interleaved_write_frame(ofmt_ctx, avPacket);
            if (ret < 0) {
                LOG_I("Error muxing packet\n");
            }
            (*ptsList).pop_front();
        }
        av_packet_unref(avPacket);
        startFrame++;
    }
    //释放内存
    startFrame = videoFrameList.begin();
    while (startFrame != videoFrameList.end()) {
        av_frame_free(&(*startFrame));
        startFrame++;
    }
    return 0;
}
void normalTranscode(list<AVPacket *> *normallist,list<int64_t> *ptsList){
    int ret = 0;
    list<AVPacket *>::iterator normalPacket = (*normallist).begin();
    int got_picture_ptr=0;
    while (normalPacket != (*normallist).end()) {
        AVPacket *avPacket = *normalPacket;
        normalPacket++;
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(dec_ctx, avFrame, &got_picture_ptr,
                                    avPacket);
        if (ret < 0) {
            LOGE("avcodec_decode_video2 fail");
            av_frame_unref(avFrame);
            break;
        }
        if (!got_picture_ptr) {
            av_frame_unref(avFrame);
            continue;
        }
        AVFrame *outAVFrame = av_frame_alloc();
        outAVFrame->format = enc_ctx->pix_fmt;
        outAVFrame->width = enc_ctx->width;
        outAVFrame->height = enc_ctx->height;
        ret = av_frame_get_buffer(outAVFrame, 4);
        if (ret < 0) {
            LOGE("Could not allocate frame data.\n");
        }
        sws_scale(swsContext,
                  (const uint8_t *const *) avFrame->data,
                  avFrame->linesize, 0, outAVFrame->height,
                  outAVFrame->data,
                  outAVFrame->linesize);

        av_frame_unref(avFrame);
        AVPacket *encode_pkt = av_packet_alloc();
        outAVFrame->pts = encodeIndex;
        encodeIndex++;
        ret = avcodec_encode_video2(enc_ctx, encode_pkt, outAVFrame,
                                    &got_picture_ptr);
        av_frame_unref(outAVFrame);
        if (ret < 0) {
            LOGD("avcodec_encode_video2 fail");
            av_packet_unref(encode_pkt);
            break;
        }
        if (!got_picture_ptr) {
            av_packet_unref(encode_pkt);
            continue;
        }
        if (!(*ptsList).empty()) {
            encode_pkt->pts = (*ptsList).front();
            encode_pkt->dts = (*ptsList).front();
            (*ptsList).pop_front();
        }
        encode_pkt->stream_index = ofmt_ctx->streams[0]->index;
        ret = av_interleaved_write_frame(ofmt_ctx, encode_pkt);
        if (ret < 0) {
            LOGE("Error muxing packet\n");
        }
        av_packet_free(&encode_pkt);
    }
    //刷新缓存
    while (true) {
        AVPacket *avPacket = av_packet_alloc();
        av_init_packet(avPacket);
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(dec_ctx, avFrame, &got_picture_ptr,
                                    avPacket);
        if (ret < 0 || !got_picture_ptr) {
            LOGD("刷新缓存结束");
            av_frame_unref(avFrame);
            break;
        }
        AVFrame *outAVFrame = av_frame_alloc();
        outAVFrame->format = enc_ctx->pix_fmt;
        outAVFrame->width = enc_ctx->width;
        outAVFrame->height = enc_ctx->height;
        ret = av_frame_get_buffer(outAVFrame, 4);
        if (ret < 0) {
            LOGE("Could not allocate frame data.\n");
        }
        sws_scale(swsContext,
                  (const uint8_t *const *) avFrame->data,
                  avFrame->linesize, 0, outAVFrame->height,
                  outAVFrame->data,
                  outAVFrame->linesize);

        //编码
        AVPacket *encode_pkt = av_packet_alloc();
        outAVFrame->pts = encodeIndex;
        encodeIndex++;
        ret = avcodec_encode_video2(enc_ctx, encode_pkt, outAVFrame,
                                    &got_picture_ptr);
        av_frame_unref(outAVFrame);
        if (ret < 0) {
            LOGD("avcodec_encode_video2 fail");
            av_packet_unref(encode_pkt);
            break;
        }
        if (!got_picture_ptr) {
            av_packet_unref(encode_pkt);
            continue;
        }
        if (!(*ptsList).empty()) {
            encode_pkt->pts = (*ptsList).front();
            encode_pkt->dts = (*ptsList).front();
            (*ptsList).pop_front();
        }
        encode_pkt->stream_index = ofmt_ctx->streams[0]->index;
        ret = av_interleaved_write_frame(ofmt_ctx, encode_pkt);
        if (ret < 0) {
            LOGE("Error muxing packet\n");
        }
        av_packet_free(&encode_pkt);
    }
}
void reverseTranscode(list<AVPacket *> *reverselist,list<int64_t> *ptsList) {
    list<AVPacket *>::reverse_iterator reversePacket = (*reverselist).rbegin();
    list<AVPacket *> gopGroupList;
    while (reversePacket != (*reverselist).rend()) {
        AVPacket *avPacket = *reversePacket;
        gopGroupList.push_front(avPacket);
        if (avPacket->flags == AV_PKT_FLAG_KEY) {
            encodeGopGroup2(&gopGroupList,ptsList,(*ptsList).back());
            gopGroupList.clear();
        }
        reversePacket++;
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_gx_ffmpegplayer_MainActivity_testseek(JNIEnv *env, jobject instance,
                                                       jstring inputFile_, jstring decodeFile_,
                                                       jlong starttime, jlong endtime) {
    const char *inputFile = env->GetStringUTFChars(inputFile_, 0);
    const char *outputFile = env->GetStringUTFChars(decodeFile_, 0);
    int ret;
    tr_init();
    ret = tr_oneninput(inputFile);
    LOG_I("videoindex=%d",videoindex);
    if (ret < 0 || NULL == ifmt_ctx) {
        LOG_I("openinput fail");
        return;
    }
    ret = tr_openOutput(ifmt_ctx, &ofmt_ctx, outputFile);
    if (ret < 0) {
        return;
    }
    ret = initEncoder();
    if (ret < 0) {
        LOG_I("initEncoder fail");
        return;
    }
    LOG_I("open output success");
    while (true) {
        AVPacket *av_packet = av_packet_alloc();
        av_init_packet(av_packet);
        ret = av_read_frame(ifmt_ctx, av_packet);
        if (ret < 0) {
            LOG_I("读取完成");
            break;
        }
        LOG_I("===");
        if (av_packet->stream_index == videoindex) {
            if (av_packet->pts < starttime){
                //正常转码
                LOG_I("正常转码1");
                ptsList.push_back(av_packet->pts);
                packetList.push_back(av_packet);
            } else if (av_packet->pts <endtime) {
                //倒序
                LOG_I("倒序");
                ptsList_2.push_back(av_packet->pts);
                packetList_2.push_back(av_packet);
            } else {
                //正常转码
                LOG_I("正常转码2");
                ptsList_3.push_back(av_packet->pts);
                packetList_3.push_back(av_packet);
            }
        }
    }
    lastpts = ptsList_3.back();
    LOG_I("lastpts=%lld", lastpts);//351702
    //正序第一段
    normalTranscode(&packetList,&ptsList);
    //倒序第二段
    reverseTranscode(&packetList_2,&ptsList_2);
    //正序最后一段
    normalTranscode(&packetList_3,&ptsList_3);
    flush_encode(enc_ctx);
    av_write_trailer(ofmt_ctx);
    avcodec_close(dec_ctx);
    avcodec_close(enc_ctx);
    tr_closeInput();
    tr_closeOutput();
    avformat_free_context(ifmt_ctx);
    avformat_free_context(ofmt_ctx);
    sws_freeContext(swsContext);
    ptsList.clear();
    ptsList_2.clear();
    ptsList_3.clear();
    packetList.clear();
    packetList_2.clear();
    packetList_3.clear();
    env->ReleaseStringUTFChars(inputFile_, inputFile);
    env->ReleaseStringUTFChars(decodeFile_, outputFile);
}