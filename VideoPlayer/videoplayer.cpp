#include "videoplayer.h"

VideoPlayer::VideoPlayer()
{
    av_register_all(); //初始化FFMPEG  调用了这个才能正常使用编码器和解码器
    avformat_network_init(); //支持打开网络文件

    if (SDL_Init(SDL_INIT_AUDIO)) {
        fprintf(stderr,"Could not initialize SDL - %s. \n", SDL_GetError());
        exit(1);
    }

    //因为VideoPlayer::PlayerState是自定义的类型 要跨线程传递需要先注册一下
//    qRegisterMetaType<VideoDecoder::PlayerState>();

    m_videoDecoder = new VideoDecoder(this);
    connect(m_videoDecoder,&VideoDecoder::sig_getOneFrame,
            this,&VideoPlayer::slotGetOneFrame);
    connect(m_videoDecoder,&VideoDecoder::sig_totalTimeChanged,
            this,&VideoPlayer::slotTotalTimeChanged);
//    connect(m_videoDecoder,&VideoDecoder::sig_stateChanged,
//            this,&VideoPlayer::slotStateChanged);

    m_timer = new QTimer;
    connect(m_timer,&QTimer::timeout,this,&VideoPlayer::slotTimerTimeOut);
    m_timer->setInterval(500);

}

VideoPlayer::~VideoPlayer()
{
    m_videoDecoder->deleteLater();
}

void VideoPlayer::startPlay(QString path)
{
    m_videoDecoder->stop(true);
    m_videoDecoder->setFileName(path);
    m_timer->start();
}

void VideoPlayer::pause()
{
    m_videoDecoder->pause();
}

void VideoPlayer::play()
{
    m_videoDecoder->play();
}

void VideoPlayer::stop()
{
    m_timer->stop();
    m_videoDecoder->stop(true);
}

void VideoPlayer::sliderMoved(int position)
{
    m_videoDecoder->seek((qint64)position * 1000000);
}

void VideoPlayer::voiceSliderMoved(int value)
{
    m_videoDecoder->setVolume(value / 100.0);
}

void VideoPlayer::paint(QPainter *painter)
{
    painter->setBrush(Qt::black);
    painter->drawRect(0,0,this->width(),this->height());

    if(m_frame.size().width() <= 0) return;

    QImage img = m_frame.scaled(this->width(),this->height(),Qt::KeepAspectRatio);

    int x = this->width() - img.width();
    int y = this->height() - img.height();

    x /= 2;
    y /= 2;

    painter->drawImage(QPoint(x,y),img);
}

void VideoPlayer::slotGetOneFrame(QImage img)
{
    m_frame = img;
    update();
}

void VideoPlayer::slotTotalTimeChanged(qint64 uSec)
{
    qint64 sec = uSec/1000000;

    m_totalTime = sec;
    emit sigSliderTotalValue(sec);

    int hour = sec / 3600;
    int minutes = sec / 60 - hour * 60;
    int seconds = sec % 60;

    QString hStr = QString("00%1").arg(hour);
    QString mStr = QString("00%1").arg(minutes);
    QString sStr = QString("00%1").arg(seconds);

    QString str = QString("%1:%2:%3").arg(hStr.right(2)).arg(mStr.right(2)).arg(sStr.right(2));
    emit sigShowTotalTime(str);
}

void VideoPlayer::slotTimerTimeOut()
{
    if (QObject::sender() == m_timer)
    {

        qint64 sec = m_videoDecoder->getCurrentTime();

        emit sigSliderCurrentValue(sec);

        int hour = sec / 3600;
        int minutes = sec / 60 - hour * 60;
        int seconds = sec % 60;

        QString hStr = QString("00%1").arg(hour);
        QString mStr = QString("00%1").arg(minutes);
        QString sStr = QString("00%1").arg(seconds);

        QString str = QString("%1:%2:%3").arg(hStr.right(2)).arg(mStr.right(2)).arg(sStr.right(2));
        emit sigShowCurrentTime(str);

        if(m_totalTime == sec)
            emit sigVideoIsOver();
    }
}

