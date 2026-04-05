#include "MyDemuxThread.h"

#include "mainwindow.h" //FFmpegPlayerCtx结构体前向声明后，还需要在.cpp包含该头文件

MyDemuxThread::MyDemuxThread(QObject *parent)
    : QThread(parent)
{

}

MyDemuxThread::~MyDemuxThread(){}

void MyDemuxThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    is = ctx;
}

int MyDemuxThread::initDemuxThread()
{
    AVFormatContext *formatCtx = NULL;
    if (avformat_open_input(&formatCtx, is->filename, NULL, NULL) != 0) {
        // ff_log_line("avformat_open_input Failed.");
        qDebug()<<"avformat_open_input Failed."<<is->filename;
        return -1;
    }

    is->formatCtx = formatCtx;

    if (avformat_find_stream_info(formatCtx, NULL) < 0) {
        // ff_log_line("avformat_find_stream_info Failed.");
        qDebug()<<"avformat_find_stream_info Failed.";
        return -1;
    }

    av_dump_format(formatCtx, 0, is->filename, 0);

    if (stream_open(is, AVMEDIA_TYPE_AUDIO) < 0) {
        // ff_log_line("open audio stream Failed.");
        qDebug()<<"open audio stream Failed.";
        return -1;
    }

    if (stream_open(is, AVMEDIA_TYPE_VIDEO) < 0) {
        // ff_log_line("open video stream Failed.");
        qDebug()<<"open video stream Failed.";
        return -1;
    }

    return 0;
}

int MyDemuxThread::stream_open(FFmpegPlayerCtx *is, int media_type)
{
    AVFormatContext *formatCtx = is->formatCtx;
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;

    int stream_index = av_find_best_stream(formatCtx, (AVMediaType)media_type, -1, -1, (const AVCodec **)&codec, 0);
    if (stream_index < 0 || stream_index >= (int)formatCtx->nb_streams) {
        // ff_log_line("Cannot find a audio stream in the input file\n");
        qDebug()<< "Cannot find a audio stream in the input file\n" ;
        return -1;
    }

    codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, formatCtx->streams[stream_index]->codecpar);

    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        // ff_log_line("Failed to open codec for stream #%d\n", stream_index);
        qDebug()<< "Failed to open codec for stream :#" << stream_index;
        return -1;
    }

    switch(codecCtx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_stream_idx = stream_index;
        is->aCodecCtx = codecCtx;
        is->audio_stream = formatCtx->streams[stream_index];
        is->swr_ctx = swr_alloc();

        av_opt_set_chlayout(is->swr_ctx, "in_chlayout", &codecCtx->ch_layout, 0);
        av_opt_set_int(is->swr_ctx, "in_sample_rate",       codecCtx->sample_rate, 0);
        av_opt_set_sample_fmt(is->swr_ctx, "in_sample_fmt", codecCtx->sample_fmt, 0);

        AVChannelLayout outLayout;
        // use stereo
        av_channel_layout_default(&outLayout, 2);

        av_opt_set_chlayout(is->swr_ctx, "out_chlayout", &outLayout, 0);
        av_opt_set_int(is->swr_ctx, "out_sample_rate",       48000, 0);
        av_opt_set_sample_fmt(is->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
        swr_init(is->swr_ctx);

        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream_idx = stream_index;
        is->vCodecCtx   = codecCtx;
        is->video_stream    = formatCtx->streams[stream_index];
        is->frame_timer = (double)av_gettime() / 1000000.0;
        is->frame_last_delay = 40e-3;
        is->sws_ctx = sws_getContext(
            codecCtx->width,
            codecCtx->height,
            codecCtx->pix_fmt,
            codecCtx->width,
            codecCtx->height,
            AV_PIX_FMT_RGB24,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
            );
        break;
    default:
        break;
    }

    return 0;
}

void MyDemuxThread::run()
{
    AVPacket *packet = av_packet_alloc();

    while (true) {
        //m_stop
        if(isInterruptionRequested()){
            qDebug()<<"request quit while decode_loop";
            break;
        }

        // begin seek
        if(0){

        }

        // 检查队列pkt的数量
        // qDebug()<<"检查队列pkt的数量："<<is->audioq.packetSize();
        qDebug()<<"检查视频队列pkt的数量："<<is->videoq.packetSize();
        if (is->audioq.packetSize() > MAX_AUDIOQ_SIZE || is->videoq.packetSize() > MAX_VIDEOQ_SIZE) {
            msleep(10);// SDL_Delay(10);
            continue;
        }

        // 从ctx中获取pkt
        if (av_read_frame(is->formatCtx, packet) < 0) {
            qDebug()<< "av_read_frame error";
            break;
        }

        if (packet->stream_index == is->video_stream_idx) {
            is->videoq.packetPut(packet);
        } else if (packet->stream_index == is->audio_stream_idx) {
            is->audioq.packetPut(packet);
        } else {
            av_packet_unref(packet);
        }
    }

    // while (!m_stop)
    while(!isInterruptionRequested())
    {
        Sleep(100);
    }

    av_packet_free(&packet);
}


// int MyDemuxThread::open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, AVMediaType type)
// {
//     int ret, stream_index;
//     AVStream* st;
//     const AVCodec* dec = NULL;

//     ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
//     if (ret < 0) {
//         fprintf(stderr, "Could not find %s stream in input file '%s'\n",
//                 av_get_media_type_string(type), src_filename);
//         return ret;
//     }
//     else {
//         stream_index = ret;
//         st = fmt_ctx->streams[stream_index];

//         /* find decoder for the stream */
//         /* 为流找到解码器 */
//         dec = avcodec_find_decoder(st->codecpar->codec_id);
//         if (!dec) {
//             fprintf(stderr, "Failed to find %s codec\n",
//                     av_get_media_type_string(type));
//             return AVERROR(EINVAL);
//         }

//         /* Allocate a codec context for the decoder */
//         /* 为解码器分配编解码器上下文 */
//         *dec_ctx = avcodec_alloc_context3(dec);
//         if (!*dec_ctx) {
//             fprintf(stderr, "Failed to allocate the %s codec context\n",
//                     av_get_media_type_string(type));
//             return AVERROR(ENOMEM);
//         }

//         /* Copy codec parameters from input stream to output codec context */
//         /* 将编解码器参数从输入流复制到输出编解码器上下文中 */
//         if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
//             fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
//                     av_get_media_type_string(type));
//             return ret;
//         }

//         /* Init the decoders */
//         /* 初始化解码器 */
//         if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
//             fprintf(stderr, "Failed to open %s codec\n",
//                     av_get_media_type_string(type));
//             return ret;
//         }
//         *stream_idx = stream_index;
//     }

//     return 0;
// }

// void MyThread::run()
// {
//     AVFormatContext *fmt_ctx = NULL;
//     AVCodecContext *audio_dec_ctx;

//     AVStream *audio_stream = NULL;



//     // uint8_t *video_dst_data[4] = {NULL};
//     // int      video_dst_linesize[4];
//     // int video_dst_bufsize;

//     int audio_stream_idx = -1;
//     AVFrame *frame = NULL;
//     AVPacket *pkt = NULL;
//     // int video_frame_count = 0;
//     // int audio_frame_count = 0;

//     /*main函数起始*/
//     int ret = 0;

//     //QStirng转const char*
//     // 显式创建QByteArray，生命周期=函数体，避免临时对象销毁
//     QByteArray byteArr = m_inputFileInfo.absoluteFilePath().toUtf8();//需要保证inputFilename生命周期在muxingAVideo函数体内
//     src_filename = byteArr.constData(); // 指向栈变量的内存，安全

//     /* open input file, and allocate format context */
//     /* 打开输入文件，并分配格式上下文 */
//     if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
//         fprintf(stderr, "Could not open source file %s\n", src_filename);
//         exit(1);
//     }

//     /* retrieve stream information */
//     /* 获取流信息 */
//     if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
//         fprintf(stderr, "Could not find stream information\n");
//         exit(1);
//     }


//     if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
//         audio_stream = fmt_ctx->streams[audio_stream_idx];
//         // audio_dst_file = fopen(audio_dst_filename, "wb");
//         // if (!audio_dst_file) {
//         //     fprintf(stderr, "Could not open destination file %s\n", audio_dst_filename);
//         //     ret = 1;
//         //     goto end;
//         // }
//     }

//     /* dump input information to stderr */
//     av_dump_format(fmt_ctx, 0, src_filename, 0);

//     if (!audio_stream) {
//         fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
//         ret = 1;
//         goto end;
//     }
//     frame = av_frame_alloc();
//     if (!frame) {
//         fprintf(stderr, "Could not allocate frame\n");
//         ret = AVERROR(ENOMEM);
//         goto end;
//     }
//     pkt = av_packet_alloc();
//     if (!pkt) {
//         fprintf(stderr, "Could not allocate packet\n");
//         ret = AVERROR(ENOMEM);
//         goto end;
//     }

//     // if (video_stream)
//     //     printf("Demuxing video from file '%s' into '%s'\n", src_filename, video_dst_filename);
//     if (audio_stream)
//         qDebug()<<"Demuxing audio from file:"<<m_inputFileInfo.absoluteFilePath();

//     /* read frames from the file */
//     while (av_read_frame(fmt_ctx, pkt) >= 0) {

//         av_packet_unref(pkt);
//         if (ret < 0)
//             break;
//     }

//     /* flush the decoders */
//     // if (video_dec_ctx)
//     //     decode_packet(video_dec_ctx, NULL);
//     if (audio_dec_ctx)
//         decode_packet(audio_dec_ctx, NULL);

//     printf("Demuxing succeeded.\n");

//     // if (video_stream) {
//     //     printf("Play the output video file with the command:\n"
//     //            "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
//     //            av_get_pix_fmt_name(pix_fmt), width, height,
//     //            video_dst_filename);
//     // }

//     if (audio_stream) {
//         enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
//         int n_channels = audio_dec_ctx->ch_layout.nb_channels;
//         const char *fmt;

//         if (av_sample_fmt_is_planar(sfmt)) {
//             const char *packed = av_get_sample_fmt_name(sfmt);
//             printf("Warning: the sample format the decoder produced is planar "
//                    "(%s). This example will output the first channel only.\n",
//                    packed ? packed : "?");
//             sfmt = av_get_packed_sample_fmt(sfmt);
//             n_channels = 1;
//         }

//         if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
//             goto end;

//         printf("Play the output audio file with the command:\n"
//                "ffplay -f %s -ac %d -ar %d %s\n",
//                fmt, n_channels, audio_dec_ctx->sample_rate,
//                audio_dst_filename);
//     }

// end:
//     // avcodec_free_context(&video_dec_ctx);
//     avcodec_free_context(&audio_dec_ctx);
//     avformat_close_input(&fmt_ctx);
//     // if (video_dst_file)
//     //     fclose(video_dst_file);
//     // if (audio_dst_file)
//     //     fclose(audio_dst_file);
//     av_packet_free(&pkt);
//     av_frame_free(&frame);
//     // av_free(video_dst_data[0]);

//     // return ret < 0;
// }
