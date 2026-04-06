#ifndef VIDEODECODETHREAD_H
#define VIDEODECODETHREAD_H

#include <QThread>
#include <Windows.h>

struct FFmpegPlayerCtx;

class VideoDecodeThread : public QThread
{
    Q_OBJECT
public:
    explicit VideoDecodeThread(QObject *parent = nullptr);
    ~VideoDecodeThread();

    void setPlayerCtx(FFmpegPlayerCtx *ctx);

    void stopThread();
signals:
    void sendCurrentFrame(QImage);
private:
    FFmpegPlayerCtx *is = nullptr;

    std::atomic<bool> m_stop = 0;
protected:
    void run()override;
};


#endif // VIDEODECODETHREAD_H
