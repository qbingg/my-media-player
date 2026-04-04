#include "PacketQueue.h"

PacketQueue::PacketQueue()
{
    // Qt自动初始化锁和条件变量，无需手动创建
}

int PacketQueue::packetPut(AVPacket *pkt)
{
    // 自动加锁/解锁，比SDL手动安全，不会死锁
    QMutexLocker locker(&mutex);

    // 入队（和原来逻辑一致）
    pkts.push_back(*pkt);
    size += pkt->size;

    // 唤醒等待的线程（对应SDL_CondSignal）
    cond.wakeOne();

    return 0;
}

int PacketQueue::packetGet(AVPacket *pkt, std::atomic<bool> &quit)
{
    int ret = 0;
    QMutexLocker locker(&mutex);

    for (;;) {
        if (!pkts.empty()) {
            // 取队首packet（逻辑不变）
            AVPacket &firstPkt = pkts.front();
            size -= firstPkt.size;
            *pkt = firstPkt;
            pkts.erase(pkts.begin());

            ret = 1;
            break;
        } else {
            // 带500ms超时等待（完全对应SDL_CondWaitTimeout）
            // wait返回false=超时，true=被唤醒
            cond.wait(&mutex, 500);
        }

        // 退出标记
        if (quit) {
            ret = -1;
            break;
        }
    }

    return ret;
}

void PacketQueue::packetFlush()
{
    QMutexLocker locker(&mutex);

    // 释放所有packet内存（和原来一致）
    for (auto &pkt : pkts) {
        av_packet_unref(&pkt);
    }
    pkts.clear();
    size = 0;
}

int PacketQueue::packetSize()
{
    return size;
}
