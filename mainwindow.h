#ifndef MAINWINDOW_H
#define MAINWINDOW_H

extern "C"{
//demux_decode.c的头文件
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <libswscale/swscale.h>  // （视频）必须包含这个头文件

//ffmpeg-simple-player的头文件
#include <libswresample/swresample.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}
#include <QFileInfo>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QMimeData>

#include <QMainWindow>
#include "MyDemuxThread.h"
#include "PacketQueue.h"
#include "AudioDecodeThread.h"
#include <QTimer>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_AUDIO_FRAME_SIZE 192000
enum PauseState {
    UNPAUSE = 0,
    PAUSE = 1
};
struct FFmpegPlayerCtx {
    char            filename[1024];
    /*解封装*/
    AVFormatContext *formatCtx = nullptr;
    AVCodecContext *aCodecCtx = nullptr;
    AVStream        *audio_stream = nullptr;
    int             audio_stream_idx = -1;
    AVFrame         *audio_frame = nullptr;
    AVPacket        *audio_pkt = nullptr;
    // ? 这是音频的什么？
    SwsContext      *sws_ctx = nullptr;
    SwrContext      *swr_ctx = nullptr;

    PacketQueue     audioq;

    /*解码*/
    std::atomic<int> pause = UNPAUSE;
    uint8_t         audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    unsigned int    audio_buf_size = 0;
    unsigned int    audio_buf_index = 0;

    uint8_t         *audio_pkt_data = nullptr;
    int             audio_pkt_size = 0;

    // for sync
    double          audio_clock = 0.0;
    double          frame_timer = 0.0;
    double          frame_last_pts = 0.0;
    double          frame_last_delay = 0.0;
    double          video_clock = 0.0;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    int initPlayer();
protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
private slots:
    void on_btnPlay_clicked();

private:
    Ui::MainWindow *ui;

    FFmpegPlayerCtx playerCtx;

    QFileInfo m_fileInfo;//目前只用于给playerCtx赋值

    MyDemuxThread *m_demuxThread = nullptr;
    AudioDecodeThread *m_audioDecodeThread = nullptr;

};
#endif // MAINWINDOW_H
