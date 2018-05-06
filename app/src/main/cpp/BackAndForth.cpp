//
/**
 * Created by zhandalin on 2018-05-02 10:40.
 * 说明:
 */
//

#include "BackAndForth.h"
#include "VideoUtil.h"

int BackAndForth::handleBackAndForth(const char *inputPath, const char *outputPath) {
    int ret = 0;
    ret = VideoUtil::openInputFileForSoft(inputPath, &in_fmt_ctx);
    if (ret < 0 || nullptr == in_fmt_ctx) {
        LOGE("BackAndForth openInputFileForSoft fail");
        return ret;
    }
    ret = VideoUtil::openOutputFile(in_fmt_ctx, &out_fmt_ctx, outputPath, false);
    if (ret < 0 || nullptr == out_fmt_ctx) {
        LOGE("BackAndForth openOutputFile fail");
        return ret;
    }
    ret = initEncode();
    if (ret < 0) {
        LOGD("initEncode fail");
        return ret;
    }
    ret = readPacket();
    if (ret < 0) {
        LOGD("readPacket fail");
        return ret;
    }
    endPts = ptsList.back();
    //先做倒序视频
    reverseVideo();
    LOGE("reverse done");
    //做正序视频
    normalVideo();
    LOGE("normal done");
    //刷新缓存
    flushEncodeBuffer();
    //释放资源
    releaseResource();

    return ret;
}

int BackAndForth::handleAGopFrame(list<AVPacket *> *aGopPacketList) {
    if (nullptr == aGopPacketList || aGopPacketList->empty() || nullptr == in_fmt_ctx ||
        nullptr == out_fmt_ctx) {
        LOGE("handleAGopFrame nullptr == aGopPacketList || aGopPacketList->empty() || nullptr == in_fmt_ctx ||nullptr == out_fmt_ctx");
        return -1;
    }
    avcodec_flush_buffers(inAVCodecContext);
    //解码一组视频
    int ret = 0, got_picture_ptr = 0;
    list<AVFrame *> videoFrameList;
    list<AVPacket *>::iterator start = aGopPacketList->begin();
    while (start != aGopPacketList->end()) {
        AVPacket *avPacket = *start;
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(inAVCodecContext, avFrame, &got_picture_ptr,
                                    avPacket);
        if (ret < 0) {
            LOGE("avcodec_decode_video2 fail");
            av_frame_unref(avFrame);
            break;
        }
        if (!got_picture_ptr) {
            start++;
            av_frame_unref(avFrame);
            continue;
        }
        AVFrame *outAVFrame = VideoUtil::allocVideoFrame(outAVCodecContext->pix_fmt,
                                                         outAVCodecContext->width,
                                                         outAVCodecContext->height);
        sws_scale(sws_video_ctx,
                  (const uint8_t *const *) avFrame->data,
                  avFrame->linesize, 0, outAVFrame->height,
                  outAVFrame->data,
                  outAVFrame->linesize);
        videoFrameList.push_front(outAVFrame);
        av_frame_unref(avFrame);
        start++;
    }
    //刷新缓存
    while (true) {
        AVPacket *decodeAVPacket = av_packet_alloc();
        av_init_packet(decodeAVPacket);
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(inAVCodecContext, avFrame, &got_picture_ptr, decodeAVPacket);
        if (ret < 0) {
            LOGE("flush avcodec_decode_video2 fail");
            av_packet_free(&decodeAVPacket);
            av_frame_unref(avFrame);
            break;
        }
        if (!got_picture_ptr) {
            av_packet_free(&decodeAVPacket);
            av_frame_unref(avFrame);
            break;
        }
        LOGD("------flush-------");
        AVFrame *outAVFrame = VideoUtil::allocVideoFrame(outAVCodecContext->pix_fmt,
                                                         outAVCodecContext->width,
                                                         outAVCodecContext->height);
        sws_scale(sws_video_ctx,
                  (const uint8_t *const *) avFrame->data,
                  avFrame->linesize, 0, outAVFrame->height,
                  outAVFrame->data,
                  outAVFrame->linesize);
        videoFrameList.push_front(outAVFrame);

        av_packet_free(&decodeAVPacket);
        av_frame_unref(avFrame);
    }

    list<AVFrame *>::iterator startFrame = videoFrameList.begin();
    while (startFrame != videoFrameList.end()) {
        AVPacket *avPacket = av_packet_alloc();
        av_init_packet(avPacket);
        AVFrame *avFrame = *startFrame;
        avFrame->pts = encodeIndex;
        avFrame->pkt_dts = encodeIndex;
        encodeIndex++;
        //编码一组视频
        ret = avcodec_encode_video2(outAVCodecContext, avPacket, avFrame,
                                    &got_picture_ptr);
        if (ret < 0) {
            LOGD("avcodec_encode_video2 fail");
            av_packet_unref(avPacket);
            break;
        }
        if (!got_picture_ptr) {
            startFrame++;
            av_packet_unref(avPacket);
            continue;
        }
        //复用之前的时间戳
        if (!ptsList.empty()) {
            avPacket->pts = ptsList.front();
            avPacket->dts = ptsList.front();
            avPacket->stream_index = out_fmt_ctx->streams[0]->index;
            ret = av_interleaved_write_frame(out_fmt_ctx, avPacket);
            if (ret < 0) {
                LOGE("Error muxing packet\n");
            }
            ptsList.pop_front();
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

int BackAndForth::initEncode() {
    int ret = 0;
    AVStream *inVideoStream = nullptr;
    for (int i = 0; i < in_fmt_ctx->nb_streams; ++i) {
        AVStream *avStream = in_fmt_ctx->streams[i];
        if (avStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            inAVCodecContext = avStream->codec;
            sws_video_ctx = sws_getContext(avStream->codecpar->width, avStream->codecpar->height,
                                           AV_PIX_FMT_YUV420P,
                                           avStream->codecpar->width, avStream->codecpar->height,
                                           AV_PIX_FMT_YUV420P,
                                           1, nullptr, nullptr, nullptr);
            inVideoStream = avStream;
            break;
        }
    }
    if (nullptr == inVideoStream) {
        LOGE("can't find inVideoStream");
        return -1;
    }
    for (int i = 0; i < out_fmt_ctx->nb_streams; ++i) {
        AVStream *avStream = out_fmt_ctx->streams[i];
        if (avStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            //double 时间
            avStream->duration *= 2;
            out_fmt_ctx->duration *= 2;
            avStream->time_base = inVideoStream->time_base;

            //开一个编码器
            AVCodec *avCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
            if (!avCodec) {
                LOGD("Could not find encoder for '%s'\n",
                     avcodec_get_name(AV_CODEC_ID_H264));
                return -1;
            }
            outAVCodecContext = avcodec_alloc_context3(avCodec);
            avcodec_parameters_to_context(outAVCodecContext, avStream->codecpar);

            int64_t bit_rate = VideoUtil::getBitRate(avStream->codecpar->width,
                                                     avStream->codecpar->height,
                                                     false);
            outAVCodecContext->bit_rate = bit_rate;
            outAVCodecContext->bit_rate_tolerance = (int) (bit_rate * 2);
            outAVCodecContext->time_base = (AVRational) {inVideoStream->avg_frame_rate.den,
                                                         inVideoStream->avg_frame_rate.num};

            outAVCodecContext->gop_size = 30;

            out_fmt_ctx->oformat->codec_tag = 0;
            outAVCodecContext->codec_tag = 0;
            if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                outAVCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            AVDictionary *param = NULL;
            av_dict_set(&param, "preset", "ultrafast", 0);
            outAVCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;

            ret = avcodec_open2(outAVCodecContext, avCodec, &param);
            if (ret < 0) {
                LOGD("avCodecContext Could not open video codec: %s\n", av_err2str(ret));
                return -1;
            }
            avcodec_parameters_from_context(avStream->codecpar, outAVCodecContext);
            avStream->codec = outAVCodecContext;
            break;
        }
    }
    return ret;
}

int BackAndForth::readPacket() {
    int ret = 0;
    //首先先取包,到时候自己实现seek逻辑
    while (true) {
        AVPacket *avPacket = av_packet_alloc();
        av_init_packet(avPacket);
        ret = av_read_frame(in_fmt_ctx, avPacket);
        if (ret < 0) {
            LOGD("读取完成");
            break;
        }
        AVStream *avStream = in_fmt_ctx->streams[avPacket->stream_index];
        if (avStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoPacketList.push_back(avPacket);
            ptsList.push_back(avPacket->pts);
            ptsList_2.push_back(avPacket->pts);
        }
    }
    if (videoPacketList.empty()) {
        LOGE("handleBackAndForth 无视频数据");
        return -1;
    }
    return 0;
}

int BackAndForth::reverseVideo() {
    list<AVPacket *>::reverse_iterator packetRStart = videoPacketList.rbegin();
    list<AVPacket *> aGopPacketList;
    while (packetRStart != videoPacketList.rend()) {
        AVPacket *avPacket = *packetRStart;
        aGopPacketList.push_front(avPacket);
        if (avPacket->flags == AV_PKT_FLAG_KEY) {
            handleAGopFrame(&aGopPacketList);
            aGopPacketList.clear();
        }
        packetRStart++;
    }
    return 0;
}

int BackAndForth::normalVideo() {
    LOGD("做正序视频");
    int ret = 0, got_picture_ptr = 0;
    avcodec_flush_buffers(inAVCodecContext);
    list<AVPacket *>::iterator start = videoPacketList.begin();
    while (start != videoPacketList.end()) {
        AVPacket *avPacket = *start;
        start++;
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(inAVCodecContext, avFrame, &got_picture_ptr,
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
        AVFrame *outAVFrame = VideoUtil::allocVideoFrame(outAVCodecContext->pix_fmt,
                                                         outAVCodecContext->width,
                                                         outAVCodecContext->height);
        sws_scale(sws_video_ctx,
                  (const uint8_t *const *) avFrame->data,
                  avFrame->linesize, 0, outAVFrame->height,
                  outAVFrame->data,
                  outAVFrame->linesize);
        av_frame_unref(avFrame);

        //编码
        AVPacket *encode_pkt = av_packet_alloc();
        outAVFrame->pts = encodeIndex;
        encodeIndex++;
        ret = avcodec_encode_video2(outAVCodecContext, encode_pkt, outAVFrame,
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
        } else if (!ptsList_2.empty()) {
            //保证是递增的
            int64_t pts = ptsList_2.front();
            if (pts <= 0)
                pts = 3;
            encode_pkt->pts = endPts + pts;
            encode_pkt->dts = endPts + pts;
            ptsList_2.pop_front();
        }
        encode_pkt->stream_index = out_fmt_ctx->streams[0]->index;
        ret = av_interleaved_write_frame(out_fmt_ctx, encode_pkt);
        if (ret < 0) {
            LOGE("Error muxing packet\n");
        }
        av_packet_free(&encode_pkt);
    }
    //刷新正序编码缓存
    LOGD("刷新正序编码缓存");
    while (true) {
        AVPacket *avPacket = av_packet_alloc();
        av_init_packet(avPacket);
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_decode_video2(inAVCodecContext, avFrame, &got_picture_ptr,
                                    avPacket);
        if (ret < 0 || !got_picture_ptr) {
            LOGD("-----flush 正序编码 end----");
            av_frame_unref(avFrame);
            break;
        }
        LOGD("-----flush 正序编码----");
        AVFrame *outAVFrame = VideoUtil::allocVideoFrame(outAVCodecContext->pix_fmt,
                                                         outAVCodecContext->width,
                                                         outAVCodecContext->height);
        sws_scale(sws_video_ctx,
                  (const uint8_t *const *) avFrame->data,
                  avFrame->linesize, 0, outAVFrame->height,
                  outAVFrame->data,
                  outAVFrame->linesize);

        //编码
        AVPacket *encode_pkt = av_packet_alloc();
        outAVFrame->pts = encodeIndex;
        encodeIndex++;
        ret = avcodec_encode_video2(outAVCodecContext, encode_pkt, outAVFrame,
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
        } else if (!ptsList_2.empty()) {
            encode_pkt->pts = endPts + ptsList_2.front();
            encode_pkt->dts = endPts + ptsList_2.front();
            ptsList_2.pop_front();
        }
        encode_pkt->stream_index = out_fmt_ctx->streams[0]->index;
        ret = av_interleaved_write_frame(out_fmt_ctx, encode_pkt);
        if (ret < 0) {
            LOGE("Error muxing packet\n");
        }
        av_packet_free(&encode_pkt);
    }
    return 0;
}

int BackAndForth::flushEncodeBuffer() {
    LOGD("刷新编码缓存");
    int ret = 0, got_picture_ptr = 0;
    AVPacket *encode_pkt = av_packet_alloc();
    while (true) {
        av_init_packet(encode_pkt);
        ret = avcodec_encode_video2(outAVCodecContext, encode_pkt, NULL,
                                    &got_picture_ptr);
        if (ret < 0 || !got_picture_ptr) {
            LOGD("-----flush_video end----");
            break;
        }
        LOGD("-----final flush----");
        if (!ptsList.empty()) {
            encode_pkt->pts = ptsList.front();
            encode_pkt->dts = ptsList.front();
            ptsList.pop_front();
        } else if (!ptsList_2.empty()) {
            encode_pkt->pts = endPts + ptsList_2.front();
            encode_pkt->dts = endPts + ptsList_2.front();
            ptsList_2.pop_front();
        }
        encode_pkt->stream_index = out_fmt_ctx->streams[0]->index;
        ret = av_interleaved_write_frame(out_fmt_ctx, encode_pkt);
        if (ret < 0) {
            LOGE("Error muxing packet\n");
        }
        av_packet_unref(encode_pkt);
    }
    ret = av_write_trailer(out_fmt_ctx);
    if (ret != 0) {
        LOGE("av_write_trailer fail");
    }
    return 0;
}

int BackAndForth::releaseResource() {
    return 0;
}
