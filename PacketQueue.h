#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#include <list>
#include <atomic>
#include <QMutex>
#include <QWaitCondition>
#include <QMutexLocker>

// FFmpeg头文件
#ifdef __cplusplus
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

class PacketQueue
{
public:
    PacketQueue();

    // 接口完全和原来一样！
    int packetPut(AVPacket *pkt);
    int packetGet(AVPacket *pkt, std::atomic<bool> &quit);
    void packetFlush();
    int packetSize();

private:
    std::list<AVPacket> pkts;
    std::atomic<int> size = 0;

    // Qt 原生线程同步（替换SDL）
    QMutex mutex;
    QWaitCondition cond;
};


#endif // PACKETQUEUE_H
