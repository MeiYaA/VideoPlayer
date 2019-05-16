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

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#define SDL_AUDIO_BUFFER_SIZE 1024  // 自定义SDL缓冲区大小

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
    AVFormatContext *formatCtx;

    //视频
    AVCodecContext *videoCodecCtx;
    AVCodec *videoCodec;
    AVStream *videoStream; //视频流
    PacketQueue videoQueue;  //视频队列

    //音频
    unsigned int audioBufferSize;
    unsigned int audioBufferIndex;
    SDL_AudioDeviceID audioID;    //音频设备ID
    int audio_hw_buffer_size;

    AVCodecContext *audioCodecCtx;
    AVCodec *audioCodec;
    AVStream *audioStream; //音频流
    PacketQueue audioQueue;  //存储音频队列
    AVPacket audioPacket; //音频包
    AVFrame *audioFrame; //解码音频使用的缓存
    int audioPacketSize;  //音频包数
    uint8_t *audioPacketData;  //音频包解码的数据大小

    enum AVSampleFormat audioSourceFormat;
    enum AVSampleFormat audioTargetFormat;

    int audioSourceChannels;  //声道数
    int audioTargetChannels;
    int64_t audioSourceChannelLayout; //通道数
    int64_t audioTargetChannelLayout;

    int audioSourceSampleRate; //样本率
    int audioTargetSampleRate;

    uint8_t *audioBuffer; //音频缓冲
    DECLARE_ALIGNED(16,uint8_t,audioSourceBuffer) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4]; //解码音频缓冲区

    struct SwrContext *swrCtx; //用于解码后的音频格式转换

    double audioClock; //音频时间
    double videoClock;

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
