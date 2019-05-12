#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QQuickPaintedItem>
#include <QPainter>
#include <QPaintEvent>
#include "videodecode.h"

class VideoPlayer : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(int vwidth READ vwidth WRITE setVwidth NOTIFY vwidthChanged)
    Q_PROPERTY(int vheight READ vheight WRITE setVheight NOTIFY vheightChanged)
public:
    explicit VideoPlayer();
    ~VideoPlayer();

    int vwidth() const;
    void setVwidth(int value);

    int vheight() const;
    void setVheight(int value);

protected:
    void paint(QPainter *painter);

public slots:
    void oneFrame(QImage img);

signals:
    void vwidthChanged();
    void vheightChanged();

private:
    QImage frame;
    VideoDecode *decoder;
    int mWidth;
    int mHeight;
};

#endif // VIDEOPLAYER_H
