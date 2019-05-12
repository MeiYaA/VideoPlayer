#include "videoplayer.h"

VideoPlayer::VideoPlayer()
{
    decoder = new VideoDecode;
    connect(decoder,&VideoDecode::getOneFrame,this,&VideoPlayer::oneFrame);

    decoder->startPlay();
}

VideoPlayer::~VideoPlayer()
{
    decoder->deleteLater();
}

int VideoPlayer::vheight() const
{
    return mHeight;
}

void VideoPlayer::setVheight(int value)
{
    mHeight = value;
}

void VideoPlayer::paint(QPainter *painter)
{
    painter->setBrush(Qt::black);
    painter->drawRect(0,0,mWidth,mHeight);

    if(frame.size().width() <= 0) return;

    QImage img = frame.scaled(mWidth,mHeight,Qt::KeepAspectRatio);

    int x = mWidth - img.width();
    int y = mHeight - img.height();

    x /= 2;
    y /= 2;

    painter->drawImage(QPoint(x,y),img);
}

void VideoPlayer::oneFrame(QImage img)
{
    frame = img;
    update();
}

int VideoPlayer::vwidth() const
{
    return mWidth;
}

void VideoPlayer::setVwidth(int value)
{
    mWidth = value;
}

