#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QQuickPaintedItem>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include "videodecoder.h"

class VideoPlayer : public QQuickPaintedItem
{
    Q_OBJECT
public:
    explicit VideoPlayer();
    ~VideoPlayer();

    Q_INVOKABLE void startPlay(QString path);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();

    Q_INVOKABLE void sliderMoved(int position);
    Q_INVOKABLE void voiceSliderMoved(int value);

protected:
    void paint(QPainter *painter);

public slots:
    //播放器相关的槽函数
    void slotGetOneFrame(QImage img);
    void slotTotalTimeChanged(qint64 uSec);
    void slotTimerTimeOut();

signals:
    void sigShowTotalTime(QString totalTime);
    void sigShowCurrentTime(QString currentTime);
    void sigSliderTotalValue(qint64 value);
    void sigSliderCurrentValue(qint64 currentvalue);
    void sigVideoIsOver();

private:
    qint64 m_totalTime;
    QImage m_frame;   //当前图像
    VideoDecoder *m_videoDecoder;   //解码器
    QTimer *m_timer;   //定时器，获取当前视频时间
};

#endif // VIDEOPLAYER_H
