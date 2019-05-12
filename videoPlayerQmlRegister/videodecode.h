#ifndef VIDEODECODE_H
#define VIDEODECODE_H

#include <QThread>
#include <QImage>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/pixfmt.h"
#include "libswresample/swresample.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_audio.h"
#include "SDL2/SDL_types.h"
#include "SDL2/SDL_name.h"
#include "SDL2/SDL_main.h"
#include "SDL2/SDL_config.h"
#include "SDL2/SDL_mutex.h"
}

//存放音频包队列
typedef struct PacketQueue{
    AVPacketList *first_pkt,*last_pkt;
    int packets_count;
    int size;
    SDL_mutex *mutex;    //音频解码线程锁
    SDL_cond *cond;
} PacketQueue;

class VideoDecode;

typedef struct VideoState{
    //一帧
    AVFormatContext *pFormatCtx;

    //视频
    AVCodecContext *videoCodecCtx;
    AVCodec *videoCodec;
    AVStream *videoStream; //视频流
    PacketQueue videoQueue;  //视频队列

    //音频
    AVCodecContext *audioCodecCtx;
    AVCodec *audioCodec;
    AVStream *audioStream; //音频流
    PacketQueue audioQueue;  //存储音频队列

    VideoDecode *decoder;

} VideoState;

class VideoDecode : public QThread
{
    Q_OBJECT
public:
    VideoDecode();

    void startPlay();
    void displayVideo(QImage image);

protected:
    void run();   //重载函数

signals:
    void getOneFrame(QImage);

private:
    QString mVideoName;
    VideoState mVideoState;
};

#endif // VIDEODECODE_H
