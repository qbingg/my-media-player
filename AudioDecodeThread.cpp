#include "AudioDecodeThread.h"

#include "mainwindow.h" //FFmpegPlayerCtx结构体前向声明后，还需要在.cpp包含该头文件

AudioDecodeThread::AudioDecodeThread(QObject *parent)
    : QThread(parent)
{

}

AudioDecodeThread::~AudioDecodeThread(){}

void AudioDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    is = ctx;
}

void AudioDecodeThread::getAudioData(unsigned char *stream, int len)
{
    // decoder is not ready or in pause state, output silence
    if (!is->aCodecCtx || is->pause == PAUSE) {
        memset(stream, 0, len);
        return;
    }

    int len1, audio_size;
    double pts;

    while(len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
            audio_size = audio_decode_frame(is, &pts);
            if (audio_size < 0) {
                is->audio_buf_size = 1024;
                memset(is->audio_buf, 0, is->audio_buf_size);
            } else {
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;
        }

        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;

        memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
}

int AudioDecodeThread::audio_decode_frame(FFmpegPlayerCtx *is, double *pts_ptr)
{
    int len1, data_size = 0, n;
    AVPacket *pkt = is->audio_pkt;
    double pts;
    int ret = 0;

    for(;;) {
        while (is->audio_pkt_size > 0) {
            ret = avcodec_send_packet(is->aCodecCtx, pkt);
            if(ret != 0) {
                // error: just skip frame
                is->audio_pkt_size = 0;
                break;
            }

            // TODO process multiframe output by one packet
            av_frame_unref(is->audio_frame);
            ret = avcodec_receive_frame(is->aCodecCtx, is->audio_frame);
            if (ret != 0) {
                // error: just skip frame
                is->audio_pkt_size = 0;
                break;
            }

            if (ret == 0) {
                int upper_bound_samples = swr_get_out_samples(is->swr_ctx, is->audio_frame->nb_samples);

                uint8_t *out[4] = {0};
                out[0] = (uint8_t*)av_malloc(upper_bound_samples * 2 * 2);

                // number of samples output per channel
                int samples = swr_convert(is->swr_ctx,
                                          out,
                                          upper_bound_samples,
                                          (const uint8_t**)is->audio_frame->data,
                                          is->audio_frame->nb_samples
                                          );
                if (samples > 0) {
                    memcpy(is->audio_buf, out[0], samples * 2 * 2);
                }

                av_free(out[0]);

                data_size = samples * 2 * 2;
            }

            len1 = pkt->size;
            is->audio_pkt_data += len1;
            is->audio_pkt_size -= len1;

            if (data_size <= 0) {
                // No data yet, need more frames
                continue;
            }

            pts = is->audio_clock;
            *pts_ptr = pts;
            n = 2 * is->aCodecCtx->ch_layout.nb_channels;
            is->audio_clock += (double)data_size / (double)(n * (is->aCodecCtx->sample_rate));

            return data_size;
        }

        //这是让m_stop=0；
        std::atomic<bool> m_stop=0;
        if (m_stop) {
            qDebug() <<"request quit while decode audio";
            return -1;
        }

        //暂时不考虑seek的事情
        // if (is->flush_actx) {
        //     is->flush_actx = false;
        //     qDebug() << "avcodec_flush_buffers(aCodecCtx) for seeking";
        //     avcodec_flush_buffers(is->aCodecCtx);
        //     continue;
        // }

        av_packet_unref(pkt);

        if (is->audioq.packetGet(pkt, m_stop) < 0) {
            return -1;
        }

        is->audio_pkt_data = pkt->data;
        is->audio_pkt_size = pkt->size;

        if (pkt->pts != AV_NOPTS_VALUE) {
            is->audio_clock = av_q2d(is->audio_stream->time_base) * pkt->pts;
            // qDebug() << "is->audio_st->time_base 等于：" << av_q2d(is->audio_stream->time_base)
            //           << "\tpkt->pts 等于：" << pkt->pts
            //           << "\tis->audio_clock等于:" << is->audio_clock ;
        }
    }
}

// ====================== 核心：SDL音频回调（不变） ======================
static void FN_Audio_Cb(
    void *userdata,
    Uint8 *stream,
    int len
    )
{
    AudioDecodeThread *dt = (AudioDecodeThread*)userdata;
    dt->getAudioData(stream, len);
}
void AudioDecodeThread::run()
{
    // do nothing

    // while(!isInterruptionRequested())
    // {
    //
    //     Sleep(1);
    // }
    qDebug() << "音频解码线程启动";

    // 1. 检查上下文是否初始化
    if (!is || !is->aCodecCtx) {
        qDebug() << "音频上下文未初始化，线程退出";
        return;
    }

    // 2. 初始化SDL音频子系统
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        qDebug() << "SDL音频初始化失败：" << SDL_GetError();
        return;
    }

    // 3. 配置SDL音频参数（必须和FFmpeg重采样格式完全一致！）
    SDL_AudioSpec spec = {0};
    spec.freq = is->aCodecCtx->sample_rate;        // 采样率（44100/48000）
    spec.format = AUDIO_S16LSB;                    // 采样格式：16位有符号小端（和你的swr输出一致）
    spec.channels = is->aCodecCtx->ch_layout.nb_channels; // 声道数
    spec.samples = 4096;                           // SDL缓冲区大小（推荐4096/8192）
    spec.callback = FN_Audio_Cb;                   // 绑定SDL回调
    spec.userdata = this;                          // 传递当前线程对象

    // 4. 打开SDL音频设备
    SDL_AudioSpec obtained;
    m_audio_dev = SDL_OpenAudioDevice(nullptr, 0, &spec, &obtained, 0);
    if (m_audio_dev == 0) {
        qDebug() << "打开SDL音频设备失败：" << SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }

    // 5. 启动音频播放（解除暂停）
    SDL_PauseAudioDevice(m_audio_dev, 0);
    qDebug() << "SDL音频设备启动成功，开始播放";

    // 6. 线程主循环：保持线程存活，等待停止信号
    while (!m_stop && !isInterruptionRequested()) {
        QThread::msleep(10); // 轻量休眠，不占用CPU
    }

    // 7. 线程退出：清理资源
    // stopAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    qDebug() << "音频线程安全退出";
}
