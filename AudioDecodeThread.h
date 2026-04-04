#ifndef AUDIODECODETHREAD_H
#define AUDIODECODETHREAD_H

#include <QThread>
#include <Windows.h>

struct FFmpegPlayerCtx;

class AudioDecodeThread : public QThread
{
    Q_OBJECT
public:
    explicit AudioDecodeThread(QObject *parent = nullptr);
    ~AudioDecodeThread();

    void setPlayerCtx(FFmpegPlayerCtx *ctx);
    void getAudioData(unsigned char *stream, int len);
    int audio_decode_frame(FFmpegPlayerCtx *is, double *pts_ptr);
signals:
    void sendMessage();

private:
    FFmpegPlayerCtx *is = nullptr;
protected:
    void run()override;
};

#endif // AUDIODECODETHREAD_H
