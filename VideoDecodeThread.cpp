#include "VideoDecodeThread.h"

#include "mainwindow.h" //FFmpegPlayerCtx结构体前向声明后，还需要在.cpp包含该头文件

// static double get_audio_clock(FFmpegPlayerCtx *is)
// {
//     double pts;
//     int hw_buf_size, bytes_per_sec, n;
//
//     pts = is->audio_clock;
//     hw_buf_size = is->audio_buf_size - is->audio_buf_index;
//     bytes_per_sec = 0;
//     n = is->aCodecCtx->ch_layout.nb_channels * 2;
//
//     if(is->audio_stream) {
//         bytes_per_sec = is->aCodecCtx->sample_rate * n;
//     }
//
//     if (bytes_per_sec) {
//         pts -= (double)hw_buf_size / bytes_per_sec;
//     }
//     return pts;
// }

static void mySleep(int ms)
{
    auto start = std::chrono::high_resolution_clock::now();
    while (true){
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (duration >= ms) break;
    }
}

static double synchronize_video(FFmpegPlayerCtx *is, AVFrame *src_frame, double pts)
{
    double frame_delay;

    if(pts != 0) {
        // if we have pts, set video clock to it
        is->video_clock = pts;
    } else {
        // if we aren't given a pts, set it to the clock
        pts = is->video_clock;
    }
    // update the video clock
    frame_delay = av_q2d(is->vCodecCtx->time_base);
    // if we are repeating a frame, adjust clock accordingly
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    is->video_clock += frame_delay;

    return pts;
}

VideoDecodeThread::VideoDecodeThread(QObject *parent)
    : QThread(parent)
{

}

VideoDecodeThread::~VideoDecodeThread(){}

void VideoDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    is = ctx;
}

void VideoDecodeThread::stopThread()
{
    m_stop = 1;
}

void VideoDecodeThread::run()
{
    // while(!isInterruptionRequested())
    // {

    //     Sleep(1);
    // }

    // FFmpegPlayerCtx *is = playerCtx;
    AVPacket *packet = av_packet_alloc();
    AVCodecContext *pCodecCtx = is->vCodecCtx;

    int ret = -1;
    double pts = 0;

    AVFrame * pFrame = av_frame_alloc();
    AVFrame * pFrameRGB = av_frame_alloc();
    av_image_alloc(pFrameRGB->data, pFrameRGB->linesize, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, 32);

    while (true) {
        //m_stop
        if(m_stop){
            qDebug()<<"request quit while decode_loop";
            break;
        }

        //pause
        if (is->pause == PAUSE) {
            msleep(5);
            continue;
        }

        //flush_vctx
        if (is->flush_vctx) {
            // ff_log_line("avcodec_flush_buffers(vCodecCtx) for seeking");
            qDebug()<< "avcodec_flush_buffers(vCodecCtx) for seeking";
            avcodec_flush_buffers(is->vCodecCtx);
            is->flush_vctx = false;
            continue;
        }

        /*开始解码*/
        av_packet_unref(packet);
        //这是让m_stop=0；
        if (is->videoq.packetGet(packet, m_stop) < 0) {
            break;
        }

        // Decode video frame
        ret = avcodec_send_packet(pCodecCtx, packet);
        if (ret == 0) {
            ret = avcodec_receive_frame(pCodecCtx, pFrame);
        }

        if (packet->dts == AV_NOPTS_VALUE
            && pFrame->opaque && *(uint64_t*)pFrame->opaque != AV_NOPTS_VALUE) {
            pts = (double)*(uint64_t *)pFrame->opaque;
        } else if(packet->dts != AV_NOPTS_VALUE) {
            pts = (double)packet->dts;
        } else {
            pts = 0;
        }
        pts *= av_q2d(is->video_stream->time_base);

        // frame ready
        if (ret == 0) {
            ret = sws_scale(is->sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0,
                            pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

            pts = synchronize_video(is, pFrame, pts);

            if (ret == pCodecCtx->height) {
                // if (queue_picture(is, pFrameRGB, pts) < 0) {
                //     break;
                // }

                //投送画面
                // ====================== 核心：AVFrame转QImage ======================
                // 1. 用RGB裸数据构造QImage（浅拷贝）
                QImage qImg(
                    pFrameRGB->data[0],       // RGB数据首地址
                    pCodecCtx->width, pCodecCtx->height,            // 宽高
                    pFrameRGB->linesize[0],    // 行字节数（必须传，避免花屏）
                    QImage::Format_RGB888      // 对应FFmpeg AV_PIX_FMT_RGB24
                    );

                // 2. 深拷贝！解决线程中帧数据复用导致的花屏/崩溃（关键）
                QImage sendImg = qImg.copy();

                // 3. 投递画面到UI线程
                sendCurrentFrame(sendImg);

                // 计算视频帧间隔
                AVRational afr;
                double video_frame_delay;
                // 计算视频帧间隔
                afr = is->video_stream->avg_frame_rate;
                video_frame_delay = (double)afr.den / afr.num;
                int m_mySleep_ms = video_frame_delay *1000;
                qDebug() << "每帧播放时间ms：" << m_mySleep_ms;

                double audio_clock = get_audio_clock(is);
                QString str = QString::number(audio_clock,'d',8);
                qDebug()<<"audio_clock:"<<str <<"\t视频解码线程pts:"<<pts;

                // if (diff <= -sync_threshold) {
                //     delay = 0;
                // } else if (diff >= sync_threshold) {
                //     delay = 2 * delay;
                // }

                //如果视频画面慢了
                if((audio_clock - pts)>video_frame_delay){
                    mySleep(0);
                }
                else{
                    mySleep(m_mySleep_ms);
                }

            }
        }




    }

    // 释放线程内alloc的资源
    av_packet_free(&packet);
    av_frame_free(&pFrame);
    av_freep(&pFrameRGB->data[0]);//必须手动释放 av_image_alloc 分配的缓冲区！
    av_frame_free(&pFrameRGB);
}
