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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setAcceptDrops(true);// 开启对整个窗口的拖放操作的支持
}

MainWindow::~MainWindow()
{
    delete ui;
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
    // init ctx
    playerCtx.audio_frame = av_frame_alloc();
    playerCtx.audio_pkt = av_packet_alloc();

    //QStirng转const char*
    // 显式创建QByteArray，生命周期=函数体，避免临时对象销毁
    // QByteArray byteArr = m_fileInfo.absoluteFilePath().toUtf8();//需要保证inputFilename生命周期在muxingAVideo函数体内
    // std::string stdStr = byteArr.constData(); // 指向栈变量的内存，安全
    // strncpy(playerCtx.filename, stdStr.c_str(), stdStr.size());
    QByteArray filePath = m_fileInfo.absoluteFilePath().toUtf8();
    // 一行搞定：自动截断、自动补\0、永不溢出
    snprintf(playerCtx.filename, sizeof(playerCtx.filename), "%s", filePath.constData());


    // create demux thread
    m_demuxThread = new MyDemuxThread;
    m_demuxThread->setPlayerCtx(&playerCtx);
    if (m_demuxThread->initDemuxThread() != 0) {
        // ff_log_line("DemuxThread init Failed.");
        qDebug()<< "DemuxThread init Failed.";
        // return -1;
        return;
    }
    QMessageBox::information(this, "", "初始化解封装线程");
    m_demuxThread->start();

    // create audio decode thread
    m_audioDecodeThread = new AudioDecodeThread;
    m_audioDecodeThread->setPlayerCtx(&playerCtx);
    QMessageBox::information(this, "", "初始化解码线程");
    m_audioDecodeThread->start();

    // QTimer *t = new QTimer(this);
    // connect(t,&QTimer::timeout,this,[=](){
    //     double audio_clock = get_audio_clock(&playerCtx);
    //
    //     QString str = QString::number(audio_clock,'d',8);
    //
    //     qDebug()<<audio_clock<<str;
    // });
    // t->start(1000);

    // create video decode thread
    m_videoDecodeThread = new VideoDecodeThread;
    m_videoDecodeThread->setPlayerCtx(&playerCtx);
    QMessageBox::information(this, "", "初始化解码线程");

    connect(m_videoDecodeThread,&VideoDecodeThread::sendCurrentFrame,[=](QImage qimg){
        ui->widget->setPixmap(QPixmap::fromImage(qimg));
    });

    m_videoDecodeThread->start();




}

