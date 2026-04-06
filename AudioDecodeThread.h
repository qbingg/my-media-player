#ifndef AUDIODECODETHREAD_H
#define AUDIODECODETHREAD_H

#include <QThread>
#include <Windows.h>
#include <SDL.h>    // 必须包含SDL头文件
struct FFmpegPlayerCtx;
// 前置声明SDL回调（也可以放在cpp）
static void FN_Audio_Cb(void *userdata, Uint8 *stream, int len);
class AudioDecodeThread : public QThread
{
    Q_OBJECT
public:
    explicit AudioDecodeThread(QObject *parent = nullptr);
    ~AudioDecodeThread();

    void setPlayerCtx(FFmpegPlayerCtx *ctx);
    void getAudioData(unsigned char *stream, int len);
    int audio_decode_frame(FFmpegPlayerCtx *is, double *pts_ptr);

    void stopThread();
signals:
    void sendMessage();

private:
    FFmpegPlayerCtx *is = nullptr;
    std::atomic<bool> m_stop = false; // 线程停止标志（类成员！）
    SDL_AudioDeviceID m_audio_dev = 0; // SDL音频设备ID
protected:
    void run()override;
};

#endif // AUDIODECODETHREAD_H
