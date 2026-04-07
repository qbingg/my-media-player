#include "mainwindow.h"
#include "ui_mainwindow.h"

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

void stream_seek(FFmpegPlayerCtx *is, int64_t pos, int rel)
{
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
        is->seek_req = 1;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setAcceptDrops(true);// 开启对整个窗口的拖放操作的支持

    ui->btnPause->setCheckable(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::initPlayer()
{
    /* 清理旧的线程和上下文 */
    qDebug()<<"清理旧的线程和上下文。。。。。。";
    if(m_videoDecodeThread){
        // m_videoDecodeThread->requestInterruption();
        m_videoDecodeThread->stopThread();
        m_videoDecodeThread->wait();
        delete m_videoDecodeThread;
        m_videoDecodeThread = nullptr;
    }
    qDebug()<<"已关闭视频线程";
    if(m_audioDecodeThread){
        // m_audioDecodeThread->requestInterruption();
        m_audioDecodeThread->stopThread();
        m_audioDecodeThread->wait();
        delete m_audioDecodeThread;
        m_audioDecodeThread = nullptr;
    }
    qDebug()<<"已关闭音频线程";
    if(m_demuxThread){
        m_demuxThread->requestInterruption();
        m_demuxThread->wait();
        delete m_demuxThread;
        m_demuxThread = nullptr;
    }
    qDebug()<<"已关闭解封装线程";
    if(m_checkCurrentSecTimer){
        m_checkCurrentSecTimer->stop();
        delete m_checkCurrentSecTimer;
        m_checkCurrentSecTimer =nullptr;
    }
    if(playerCtx){
        delete playerCtx;
        playerCtx = nullptr;
    }

    // 验证输入文件是否为空，是不是文件
    if(m_fileInfo.absoluteFilePath().isEmpty()){
        qDebug()<<"输入视频文件为空。。。";
        return -1;
    }
    if(!m_fileInfo.isFile()){
        qDebug()<<"输入不是文件。。。";
        return -1;
    }

    /* 为新的视频文件初始化播放器 */
    playerCtx = new FFmpegPlayerCtx;
    // init ctx
    playerCtx->audio_frame = av_frame_alloc();
    playerCtx->audio_pkt = av_packet_alloc();

    // 获取新的视频文件路径 // QString转char filename[1024]
    QByteArray filePath = m_fileInfo.absoluteFilePath().toUtf8();
    // 一行搞定：自动截断、自动补\0、永不溢出
    snprintf(playerCtx->filename, sizeof(playerCtx->filename), "%s", filePath.constData());

    // create demux thread
    m_demuxThread = new MyDemuxThread;
    m_demuxThread->setPlayerCtx(playerCtx);
    if (m_demuxThread->initDemuxThread() != 0) {
        qDebug()<< "DemuxThread init Failed.";
        return -1;
    }

    // create audio decode thread
    m_audioDecodeThread = new AudioDecodeThread;
    m_audioDecodeThread->setPlayerCtx(playerCtx);

    // create video decode thread
    m_videoDecodeThread = new VideoDecodeThread;
    m_videoDecodeThread->setPlayerCtx(playerCtx);
    connect(m_videoDecodeThread,&VideoDecodeThread::sendCurrentFrame,this,[=](QImage qimg){
        ui->widget->setPixmap(QPixmap::fromImage(qimg));
    });

    // 获取新容器总时间，和使用QTimer每秒获取音频时钟
    int64_t totalMicroSec =playerCtx->formatCtx->duration;//AVFormatContext: int64_t duration: 	Duration of the stream, in AV_TIME_BASE fractional seconds.
    ui->totalSecLabel->setText("/ "+QString::number((totalMicroSec / AV_TIME_BASE),'d',8));
    ui->horizontalSlider->setMaximum((totalMicroSec / AV_TIME_BASE));
    ui->horizontalSlider->setValue(0);
    m_checkCurrentSecTimer = new QTimer(this);
    connect(m_checkCurrentSecTimer,&QTimer::timeout,this,[=](){
        double audio_clock = get_audio_clock(playerCtx);
        QString str = QString::number(audio_clock,'d',8);
        ui->currentSecLabel->setText(str);

        //如果用户正在拖拽Slider，暂时不更新
        if(m_userIsUsingSlider){
            qDebug()<<"用户正在拖拽Slider，暂时不更新";
        }else{
            //这一步是为了更新进度条时，防被动触发on_horizontalSlider_valueChanged
            const QSignalBlocker blocker(ui->horizontalSlider);

            ui->horizontalSlider->setValue(audio_clock);
        }

        qDebug()<<audio_clock<<str;
    });

    return 0;
}

void MainWindow::seekRelative(double offsetSec)
{
    //参考自：ffmpeg-simple-player的void FFmpegPlayer::onKeyEvent(SDL_Event *e)
    double incr, pos;

    incr = offsetSec;

    if (true) {
        pos = get_audio_clock(playerCtx);
        pos += incr;
        if (pos < 0) {
            pos = 0;
        }
        // ff_log_line("seek to %lf v:%lf a:%lf", pos, get_audio_clock(&playerCtx), get_audio_clock(&playerCtx));
        qDebug() <<"seek to "<<pos
                 <<" v:"<<get_audio_clock(playerCtx)
                 <<" a:"<<get_audio_clock(playerCtx);
        stream_seek(playerCtx, (int64_t)(pos * AV_TIME_BASE), (int)incr);
    }

}

void MainWindow::seekAbsolute(double offsetSec)
{
    //参考自：ffmpeg-simple-player的void FFmpegPlayer::onKeyEvent(SDL_Event *e)
    double incr, pos;

    incr = offsetSec;

    if (true) {
        pos = 0;//get_audio_clock(playerCtx);
        pos += incr;
        if (pos < 0) {
            pos = 0;
        }
        // ff_log_line("seek to %lf v:%lf a:%lf", pos, get_audio_clock(&playerCtx), get_audio_clock(&playerCtx));
        qDebug() <<"seek to "<<pos
                 <<" v:"<<get_audio_clock(playerCtx)
                 <<" a:"<<get_audio_clock(playerCtx);
        stream_seek(playerCtx, (int64_t)(pos * AV_TIME_BASE), (int)incr);
    }

}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction(); // 接受默认的拖放行为
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();

    // 确保文件数量仅为一个
    if (urls.size() != 1) {
        if (urls.size() > 1) {
            QMessageBox::warning(this, "warning", "请拖入单个文件");
        }
        return;
    }

    const QUrl& url = urls.first();
    QString filePath = url.toLocalFile();

    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "warning", "请拖入有效的文件");
        return;
    }

    //获取文件信息，并显示到标题栏上
    setWindowTitle(fileInfo.absoluteFilePath());

    m_fileInfo = fileInfo;
}

void MainWindow::on_btnPlay_clicked()
{
    if(initPlayer() != 0)
        return;

    m_demuxThread->start();
    m_audioDecodeThread->start();
    m_videoDecodeThread->start();
    m_checkCurrentSecTimer->start(1000);
    ui->volumeSlider->setValue(50);//每个视频播放默认50音量->0.5f ，如果不是这样的话，需要考虑每次初始化容器时，setVolume一次让音频解码线程m_volume与ui的Slider一致才行。


    // // init ctx
    // playerCtx.audio_frame = av_frame_alloc();
    // playerCtx.audio_pkt = av_packet_alloc();

    // //QStirng转const char*
    // // 显式创建QByteArray，生命周期=函数体，避免临时对象销毁
    // // QByteArray byteArr = m_fileInfo.absoluteFilePath().toUtf8();//需要保证inputFilename生命周期在muxingAVideo函数体内
    // // std::string stdStr = byteArr.constData(); // 指向栈变量的内存，安全
    // // strncpy(playerCtx.filename, stdStr.c_str(), stdStr.size());
    // QByteArray filePath = m_fileInfo.absoluteFilePath().toUtf8();
    // // 一行搞定：自动截断、自动补\0、永不溢出
    // snprintf(playerCtx.filename, sizeof(playerCtx.filename), "%s", filePath.constData());


    // // create demux thread
    // m_demuxThread = new MyDemuxThread;
    // m_demuxThread->setPlayerCtx(&playerCtx);
    // if (m_demuxThread->initDemuxThread() != 0) {
    //     // ff_log_line("DemuxThread init Failed.");
    //     qDebug()<< "DemuxThread init Failed.";
    //     // return -1;
    //     return;
    // }
    // QMessageBox::information(this, "", "初始化解封装线程");
    // m_demuxThread->start();

    // // create audio decode thread
    // m_audioDecodeThread = new AudioDecodeThread;
    // m_audioDecodeThread->setPlayerCtx(&playerCtx);
    // QMessageBox::information(this, "", "初始化解码线程");
    // m_audioDecodeThread->start();

    // // create video decode thread
    // m_videoDecodeThread = new VideoDecodeThread;
    // m_videoDecodeThread->setPlayerCtx(&playerCtx);
    // QMessageBox::information(this, "", "初始化解码线程");

    // connect(m_videoDecodeThread,&VideoDecodeThread::sendCurrentFrame,[=](QImage qimg){
    //     ui->widget->setPixmap(QPixmap::fromImage(qimg));
    // });

    // m_videoDecodeThread->start();

    // //获取容器总时间，和使用QTimer每秒获取音频时钟
    // int64_t totalMicroSec =playerCtx.formatCtx->duration;
    // ui->totalSecLabel->setText("/ "+QString::number((totalMicroSec / 1000000),'d',8));
    // ui->horizontalSlider->setMaximum((totalMicroSec / 1000000));
    // ui->horizontalSlider->setValue(0);
    // QTimer *t = new QTimer(this);
    // connect(t,&QTimer::timeout,this,[=](){
    //     double audio_clock = get_audio_clock(&playerCtx);

    //     QString str = QString::number(audio_clock,'d',8);

    //     qDebug()<<audio_clock<<str;

    //     ui->currentSecLabel->setText(str);
    //     ui->horizontalSlider->setValue(audio_clock);
    // });
    // t->start(1000);

}


void MainWindow::on_btnPause_clicked()
{
    if(!playerCtx){
        qDebug()<<"容器为空。。。";

        ui->btnPause->setChecked(false);

        return;
    }

    if(ui->btnPause->isChecked()){
        //暂停播放
        ui->btnPause->setText("继续");

        playerCtx->pause = PAUSE;
    }else{
        //继续播放
        ui->btnPause->setText("暂停");

        playerCtx->pause = UNPAUSE;
    }
}


void MainWindow::on_btnRewind_clicked()
{
    if(!playerCtx){
        qDebug()<<"容器为空。。。";
        return;
    }

    seekRelative(-10.0);
}


void MainWindow::on_btnForward_clicked()
{
    if(!playerCtx){
        qDebug()<<"容器为空。。。";
        return;
    }

    seekRelative(10.0);
}


void MainWindow::on_volumeSlider_valueChanged(int value)
{
    if((!playerCtx)&&(!m_audioDecodeThread)){
        qDebug()<<"容器为空。。。";
        return;
    }

    float f = value / static_cast<double>(100);// 0~100 -> 0.0f~1.0f
    m_audioDecodeThread->setVolume(f);
}


void MainWindow::on_horizontalSlider_sliderPressed()
{
    //按下Slider拖拽时，只会触发一次
    qDebug()<<"on_horizontalSlider_sliderPressed()";

    m_userIsUsingSlider = true;
}


void MainWindow::on_horizontalSlider_sliderReleased()
{
    //松开Slider拖拽时，只会触发一次
    qDebug()<<"on_horizontalSlider_sliderReleased()"<<"值："<<ui->horizontalSlider->value();

    //通过seekAbsolute进度条跳转至 n 秒 最近的 I 帧
    seekAbsolute(ui->horizontalSlider->value());

    m_userIsUsingSlider = false;
}

