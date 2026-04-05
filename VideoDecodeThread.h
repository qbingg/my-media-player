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
signals:
    void sendMessage();

private:
    FFmpegPlayerCtx *is = nullptr;
protected:
    void run()override;
};


#endif // VIDEODECODETHREAD_H
