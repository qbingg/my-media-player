#ifndef MYDEMUXTHREAD_H
#define MYDEMUXTHREAD_H

#include <QString>
#include <QFileInfo>
#include <QObject>
#include <QImage>
#include <QThread>
#include <QDateTime>
#include <Windows.h>
// #include "mainwindow.h"

struct FFmpegPlayerCtx;

class MyDemuxThread : public QThread
{
    Q_OBJECT
public:
    explicit MyDemuxThread(QObject *parent = nullptr);
    ~MyDemuxThread();

    void setPlayerCtx(FFmpegPlayerCtx *ctx);

    int initDemuxThread();
    int stream_open(FFmpegPlayerCtx *is, int media_type);
signals:
    void sendMessage();

private:
    FFmpegPlayerCtx *is = nullptr;

private:
    const char* src_filename = NULL;
    // int open_codec_context(int* stream_idx,AVCodecContext** dec_ctx, AVFormatContext* fmt_ctx, enum AVMediaType type);

protected:
    // void run()override;
};


#endif // MYDEMUXTHREAD_H
