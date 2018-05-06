
/**
 * Created by zhandalin on 2017-08-09 20:46.
 * 说明: 视频工具类
 */


#include "VideoUtil.h"
//#include "bzcommon.h"

AVFrame *VideoUtil::allocVideoFrame(enum AVPixelFormat pix_fmt, int width, int height) {
//    LOGD("alloc_picture");
    AVFrame *picture = NULL;
    int ret;


    picture = av_frame_alloc();
    if (!picture)
        return NULL;
    picture->format = pix_fmt;
    picture->width = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 4);
    if (ret < 0) {
        LOGE("Could not allocate frame data.\n");
    }
    return picture;
}

AVFrame *VideoUtil::allocAudioFrame(enum AVSampleFormat sample_fmt,
                                    uint64_t channel_layout,
                                    int sample_rate, int nb_samples) {
//    LOGD("alloc_audio_frame");
    AVFrame *frame = av_frame_alloc();
    int ret;
    if (!frame) {
        LOGD("Error allocating an audio frame\n");
        return NULL;
    }
    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            LOGD("Error allocating an audio buffer\n");
        }
    }
    return frame;
}


int VideoUtil::getVideoWidth(const char *videoPath) {
    AVFormatContext *in_fmt_ctx = NULL;
    int ret = 0;
    if ((ret = avformat_open_input(&in_fmt_ctx, videoPath, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    if ((ret = avformat_find_stream_info(in_fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    int temp = 0;
    for (int i = 0; i < in_fmt_ctx->nb_streams; i++) {
        AVStream *stream;
        stream = in_fmt_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            temp = stream->codecpar->width;
            break;
        }
    }
    if (NULL != in_fmt_ctx)
        avformat_close_input(&in_fmt_ctx);
    return temp;
}

int VideoUtil::getVideoHeight(const char *videoPath) {
    AVFormatContext *in_fmt_ctx = NULL;
    int ret = 0;
    if ((ret = avformat_open_input(&in_fmt_ctx, videoPath, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    if ((ret = avformat_find_stream_info(in_fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    int temp = 0;
    for (int i = 0; i < in_fmt_ctx->nb_streams; i++) {
        AVStream *stream;
        stream = in_fmt_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            temp = stream->codecpar->height;
            break;
        }
    }
    if (NULL != in_fmt_ctx)
        avformat_close_input(&in_fmt_ctx);
    return temp;
}

int VideoUtil::getVideoRotate(const char *videoPath) {
    AVFormatContext *in_fmt_ctx = NULL;
    int ret = 0;
    if ((ret = avformat_open_input(&in_fmt_ctx, videoPath, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    if ((ret = avformat_find_stream_info(in_fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    int videoRotate = 0;
    for (int i = 0; i < in_fmt_ctx->nb_streams; i++) {
        AVStream *stream;
        stream = in_fmt_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVDictionaryEntry *entry = av_dict_get(stream->metadata, "rotate", NULL,
                                                   AV_DICT_IGNORE_SUFFIX);
            if (NULL != entry) {
                videoRotate = atoi(entry->value);
            }
            break;
        }
    }
    if (NULL != in_fmt_ctx)
        avformat_close_input(&in_fmt_ctx);
    return videoRotate;
}

long VideoUtil::getVideoDuration(const char *videoPath) {
    AVFormatContext *in_fmt_ctx = NULL;
    int ret = 0;
    if ((ret = avformat_open_input(&in_fmt_ctx, videoPath, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    if ((ret = avformat_find_stream_info(in_fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    long videoDuration = 0;
    for (int i = 0; i < in_fmt_ctx->nb_streams; i++) {
        AVStream *stream;
        stream = in_fmt_ctx->streams[i];
        long temp = (long) (stream->duration * 1000 * stream->time_base.num /
                            stream->time_base.den);
        if (temp > videoDuration)
            videoDuration = temp;
    }
    if (NULL != in_fmt_ctx)
        avformat_close_input(&in_fmt_ctx);

    return videoDuration;
}

int VideoUtil::getVideoInfo(const char *videoPath, void (*sendMediaInfoCallBack)(
        int, int)) {
    if (NULL == sendMediaInfoCallBack)
        return -1;
    AVFormatContext *in_fmt_ctx = NULL;
    int ret = 0;
    if ((ret = avformat_open_input(&in_fmt_ctx, videoPath, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    if ((ret = avformat_find_stream_info(in_fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    long videoDuration = 0;
    int height = 0, width = 0, rotate = 0;
    for (int i = 0; i < in_fmt_ctx->nb_streams; i++) {
        AVStream *stream;
        stream = in_fmt_ctx->streams[i];
        long temp = (long) (stream->duration * 1000 * stream->time_base.num /
                            stream->time_base.den);
        if (temp > videoDuration)
            videoDuration = temp;
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            width = stream->codecpar->width;
            height = stream->codecpar->height;
            AVDictionaryEntry *entry = av_dict_get(stream->metadata, "rotate", NULL,
                                                   AV_DICT_IGNORE_SUFFIX);
            if (NULL != entry) {
                rotate = atoi(entry->value);
            }
        }

    }
    if (NULL != in_fmt_ctx)
        avformat_close_input(&in_fmt_ctx);
//    sendMediaInfoCallBack(MEDIA_INFO_WHAT_VIDEO_DURATION, (int) videoDuration);
//    sendMediaInfoCallBack(MEDIA_INFO_WHAT_VIDEO_ROTATE, rotate);
//    sendMediaInfoCallBack(MEDIA_INFO_WHAT_VIDEO_WIDTH, width);
//    sendMediaInfoCallBack(MEDIA_INFO_WHAT_VIDEO_HEIGHT, height);

    return 0;
}

int VideoUtil::printVideoTimeStamp(const char *videoPath) {
    AVFormatContext *in_fmt_ctx = NULL;
    int ret = 0;
    if ((ret = avformat_open_input(&in_fmt_ctx, videoPath, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    if ((ret = avformat_find_stream_info(in_fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    int videoIndex = 0;
    for (int i = 0; i < in_fmt_ctx->nb_streams; i++) {
        AVStream *stream;
        stream = in_fmt_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            LOGD("video index=%d", i);
            videoIndex = i;
            AVCodec *avCodec = avcodec_find_decoder(stream->codecpar->codec_id);
            if (NULL == avCodec) {
                LOGE("can't find_decoder");
                return -1;
            }
            AVCodecContext *codec_ctx = avcodec_alloc_context3(avCodec);
            if (NULL == codec_ctx) {
                LOGE("can't avcodec_alloc_context3");
                return -1;
            }
            avcodec_parameters_to_context(codec_ctx, stream->codecpar);

            /* Reencode video & audio and remux subtitles etc. */
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
                || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                /* Open decoder */
                ret = avcodec_open2(codec_ctx,
                                    NULL, NULL);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                    return ret;
                }
            }
            stream->codec = codec_ctx;
        }
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            LOGD("--audio-- index=%d", i);
        }
    }

    AVStream *in_stream = in_fmt_ctx->streams[videoIndex];
    AVFrame *videoFrame = VideoUtil::allocVideoFrame(in_stream->codec->pix_fmt,
                                                     in_stream->codecpar->width,
                                                     in_stream->codecpar->height);

    FILE *yuvFile = fopen("/sdcard/Filter/out.yuv", "wb");
    int got_picture_ptr = 0;
    size_t ySize = (size_t) (in_stream->codecpar->width * in_stream->codecpar->height);
    AVPacket *decode_pkt = av_packet_alloc();
    while (av_read_frame(in_fmt_ctx, decode_pkt) >= 0) {
        LOGD("stream_index=%d\tpts=%lld\tdts=%lld\tduration=%lld", decode_pkt->stream_index,
             decode_pkt->pts, decode_pkt->dts, decode_pkt->duration);

//        if (decode_pkt->stream_index == videoIndex) {
//            ret = avcodec_decode_video2(in_stream->codec, videoFrame, &got_picture_ptr,
//                                        decode_pkt);
//            if (ret < 0) {
//                LOGD("avcodec_decode_video2 fail");
//            }
//            LOGD("stream_index=%d\tpts=%lld\tdts=%lld\tduration=%lld", videoIndex,
//                 videoFrame->pts, videoFrame->pkt_dts, videoFrame->pkt_duration);
//
//            fwrite(videoFrame->data[0], ySize, 1, yuvFile);
//
//            fwrite(videoFrame->data[1], ySize / 4, 1, yuvFile);
//            fwrite(videoFrame->data[2], ySize / 4, 1, yuvFile);
//        }
        av_init_packet(decode_pkt);
    }
    fflush(yuvFile);
    fclose(yuvFile);

    if (NULL != in_fmt_ctx)
        avformat_close_input(&in_fmt_ctx);
    return 0;
}

int
VideoUtil::openInputFile(const char *filename, AVFormatContext **in_fmt_ctx) {
    return openInputFile(filename, in_fmt_ctx, true, true);
}

int
VideoUtil::openInputFile(const char *filename, AVFormatContext **in_fmt_ctx, bool openVideoDecode,
                         bool openAudioDecode) {
    if (NULL == filename)
        return -1;
    int ret;
    unsigned int i;

    if ((ret = avformat_open_input(in_fmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    if ((ret = avformat_find_stream_info(*in_fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    for (i = 0; i < (*in_fmt_ctx)->nb_streams; i++) {
        AVStream *stream = (*in_fmt_ctx)->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (openVideoDecode) {
                ret = openAVCodecContext(stream);
            } else {
                stream->codec = NULL;
            }
        }
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (openAudioDecode) {
                ret = openAVCodecContext(stream);
            } else {
                stream->codec = NULL;
            }
        }
    }
    //如果有一个流没有打开, 那么dump就会有问题
//    av_dump_format((*in_fmt_ctx), 0, filename, 0);
    return ret;
}

int VideoUtil::openInputFileForSoft(const char *filename, AVFormatContext **in_fmt_ctx) {
    if (NULL == filename)
        return -1;
    int ret;
    unsigned int i;

    if ((ret = avformat_open_input(in_fmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
//    if ((ret = avformat_find_stream_info((*in_fmt_ctx), NULL)) < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
//        return ret;
//    }

    for (i = 0; i < (*in_fmt_ctx)->nb_streams; i++) {
        AVStream *inStream = (*in_fmt_ctx)->streams[i];
        AVCodec *avCodec = avcodec_find_decoder(inStream->codecpar->codec_id);
        if (NULL == avCodec) {
            LOGE("can't find_decoder");
            return -1;
        }
        AVCodecContext *codec_ctx = avcodec_alloc_context3(avCodec);
        if (NULL == codec_ctx) {
            LOGE("can't avcodec_alloc_context3");
            return -1;
        }
        avcodec_parameters_to_context(codec_ctx, inStream->codecpar);
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx,
                                NULL, NULL);
            if (ret < 0) {
                LOGE("Failed to open decoder for stream");
                return ret;
            }
        }
        inStream->codec = codec_ctx;
    }
    av_dump_format((*in_fmt_ctx), 0, filename, 0);
    return ret;
}


int VideoUtil::openAVCodecContext(AVStream *inStream) {
    int ret = 0;
    AVCodecID codec_id = inStream->codecpar->codec_id;
    AVCodec *avCodec = NULL;
    bool api_18 = false;
#ifdef USEOPENGL3
    api_18 = true;
#endif
    if (codec_id == AV_CODEC_ID_H264 && api_18) {//换成硬解码
        avCodec = avcodec_find_decoder_by_name("h264_mediacodec");
//        avCodec = avcodec_find_decoder(codec_id);
    } else {
        avCodec = avcodec_find_decoder(codec_id);
    }

    if (NULL == avCodec) {
        LOGE("can't find_decoder");
        return -1;
    }
    AVCodecContext *codec_ctx = avcodec_alloc_context3(avCodec);
    if (NULL == codec_ctx) {
        LOGE("can't avcodec_alloc_context3");
        return -1;
    }
    avcodec_parameters_to_context(codec_ctx, inStream->codecpar);

    /* Reencode video & audio and remux subtitles etc. */
    if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
        || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        /* Open decoder */
        ret = avcodec_open2(codec_ctx,
                            NULL, NULL);
        if (ret < 0) {
            LOGE("Failed to open decoder for stream openAVCodecContext");
            return ret;
        }
    }
    avcodec_close(inStream->codec);
    inStream->codec = codec_ctx;
    return ret;
}

int64_t VideoUtil::getBitRate(int videoWidth, int videoHeight, bool allFrameIsKey) {
    //bit_rate 设置
    int bitrateP = videoWidth > videoHeight ? videoWidth : videoHeight;
    //以720p为标准,每秒1M
    int64_t bit_rate = 1024 * 1024 * 8 * 2;//每秒1M;
    if (!allFrameIsKey) {//如果不全是关键帧,那么码率降低
        bit_rate /= 2;
    }
    return (int64_t) (bitrateP / 720.f * bit_rate);
}

int VideoUtil::openOutputFile(AVFormatContext *in_fmt_ctx, AVFormatContext **out_fmt_ctx,
                              const char *output_path, bool needAudio) {
    int ret = 0;
    ret = avformat_alloc_output_context2(out_fmt_ctx, NULL, NULL, output_path);
    if (ret < 0) {
        LOGE("alloc_output_context2 fail");
        return ret;
    }
    if (!out_fmt_ctx) {
        return -1;
    }

    for (int i = 0; i < in_fmt_ctx->nb_streams; ++i) {
        AVStream *in_stream = in_fmt_ctx->streams[i];
        if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !needAudio) {
            continue;
        }
        AVStream *out_stream = avformat_new_stream(*out_fmt_ctx, in_stream->codec->codec);
        if (!out_stream) {
            LOGD("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            return ret;
        }
        ret = av_dict_copy(&out_stream->metadata, in_stream->metadata, AV_DICT_IGNORE_SUFFIX);
        if (ret < 0) {
            LOGE("Failed allocating output stream\n");
            return ret;
        }
        out_stream->time_base = in_stream->time_base;

        avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (ret < 0) {
            LOGD("Failed to copy context from input to output stream codec context\n");
            return ret;
        }
        ret = avcodec_parameters_from_context(out_stream->codecpar, in_stream->codec);
        if (ret < 0) {
            LOGD("Could not copy the stream parameters\n");
            return -1;
        }
        (*out_fmt_ctx)->oformat->codec_tag = 0;
        out_stream->codec->codec_tag = 0;
        if ((*out_fmt_ctx)->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    av_dump_format(*out_fmt_ctx, 0, output_path, 1);

    /* open the output file, if needed */
    if (!((*out_fmt_ctx)->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&(*out_fmt_ctx)->pb, output_path, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGD("Could not open '%s': %s\n", output_path,
                 av_err2str(ret));
            return ret;
        }
    }
    /* Write the stream header, if any. */
    ret = avformat_write_header(*out_fmt_ctx, NULL);
    if (ret < 0) {
        LOGD("Error occurred when opening output file: %s\n",
             av_err2str(ret));
        return ret;
    }
    return 0;
}
