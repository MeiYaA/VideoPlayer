#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include <QThread>
#include <QImage>
#include "videodefine.h"

class VideoPlayer;

class VideoDecoder : public QThread
{
    Q_OBJECT
public:
    enum PlayerState
    {
        Playing,
        Pause,
        Stop
    };

    explicit VideoDecoder(VideoPlayer *vp);
    ~VideoDecoder();

    bool setFileName(QString path);
    bool play();
    bool pause();
    bool stop(bool isWait = false); //参数表示是否等待所有的线程执行完毕再返回
    void seek(int64_t pos); //单位是微秒,进度条

    void setMute(bool isMute){m_isMute = isMute;}  //设置静音
    void setVolume(float value);  //调整音量
    int64_t getTotalTime(); //单位微秒
    double getCurrentTime(); //单位秒

    void disPlayVideo(QImage img);

signals:
    void sig_getOneFrame(QImage); //每获取到一帧图像 就发送此信号
    void sig_stateChanged(VideoDecoder::PlayerState state);
    void sig_totalTimeChanged(qint64 uSec); //获取到视频时长的时候激发此信号

protected:
    void run();

private:
    int openSDL();
    void closeSDL();
    void clear();

    QString m_fileName;
    VideoState m_videoState;    //视频解码状态
    PlayerState m_playerState;  //播放状态

    VideoPlayer *m_videoPlayer;  //显示视频

    bool m_isMute;  //静音
    float m_volume;  //0~1 超过1 表示放大倍数，音量

    SDL_AudioDeviceID m_audioID;
};

#endif // VIDEODECODER_H
