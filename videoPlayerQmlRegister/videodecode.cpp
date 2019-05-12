#include "videodecode.h"
#include <iostream>

#define MAX_AUDIO_SIZE (25 * 16 * 1024)
#define MAX_VIDEO_SIZE (25 * 256 * 1024)


//初始化包
void packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
    q->size = 0;
    q->packets_count = 0;
    q->first_pkt = NULL;
    q->last_pkt = NULL;
}

//存储音频包到队列
int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;

    //将共享内存里的AVPacket复制到独立的BUffer中
    if (av_dup_packet(pkt) < 0) {
        return -1;
    }
    //分配内存
    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;

    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex); //线程加锁

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->packets_count++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);  //解锁
    return 0;
}

//获取队列中的音频包
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->packets_count--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }

    }

    SDL_UnlockMutex(q->mutex);
    return ret;
}

//解码视频
int decodeVideo(void *arg)
{
    VideoState *vs = (VideoState *) arg;
    AVPacket pkt1;
    AVPacket *packet = &pkt1;

    //解码视频相关
    AVFrame *pFrame, *pFrameRGB;
    uint8_t *out_buffer_rgb;  //解码后的rgb数据

    static struct SwsContext *img_convert_ctx;  //解码后的视频格式转换

    AVCodecContext *pCodecCtx = vs->videoStream->codec;  //视频解码器

    //分配内存
    pFrame = av_frame_alloc();
    pFrameRGB = av_frame_alloc();

    int ret, got_picture,numBytes;

    //将解码后的YUV数据转换成RGB32
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                     pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                                     AV_PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

    numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, pCodecCtx->width,pCodecCtx->height);

    out_buffer_rgb = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture *) pFrameRGB, out_buffer_rgb, AV_PIX_FMT_RGB32,
                   pCodecCtx->width, pCodecCtx->height);

    while(1)
    {

        if (packet_queue_get(&vs->videoQueue, packet, 1) <= 0) break;//队列里面没有数据了  读取完毕了

        ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture,packet);

        if (got_picture) {
            sws_scale(img_convert_ctx,
                      (uint8_t const * const *) pFrame->data,
                      pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data,
                      pFrameRGB->linesize);
            QImage tmpImg((uchar *)out_buffer_rgb,pCodecCtx->width,pCodecCtx->height,QImage::Format_RGB32);
            QImage image = tmpImg.copy();
            vs->decoder->displayVideo(image);

        }
        av_free_packet(packet);
    }

    av_free(out_buffer_rgb);
    av_free(pFrameRGB);
    av_free(pFrame);
    return 0;
}

//void decodeAudio(VideoState *vs)
//{
//    AVCodecContext *audioCodecCtx;
//    audioCodecCtx = vs->audioCodecCtx;

//    static struct SwrContext *audio_convert_ctx;  //解码后的音频格式转换

//    audio_convert_ctx = swr_alloc_set_opts(nullptr,audioCodecCtx->channel_layout,
//                                           AV_SAMPLE_FMT_S16,
//                                           audioCodecCtx->sample_rate,
//                                           audioCodecCtx->channel_layout,
//                                           audioCodecCtx->sample_fmt,
//                                           audioCodecCtx->sample_rate,0,nullptr);
//    swr_init(audio_convert_ctx);
//}

VideoDecode::VideoDecode()
{
    mVideoName = "rtsp://10.253.169.236/movies/DaShengGuiLai.mkv";
}

void VideoDecode::startPlay()
{
    this->start();
}

void VideoDecode::displayVideo(QImage image)
{
    emit getOneFrame(image);
}

void VideoDecode::run()
{
    char video[512] = {0};
    strcpy(video,mVideoName.toUtf8().data());

    av_register_all();
    avformat_network_init();

    //一帧
    AVFormatContext *pFormatCtx;

    //视频
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;

    //音频
    AVCodecContext *aCodecCtx;
    AVCodec *aCodec;

    int audioStream = -1;
    int videoStream = -1;

    //分配存储空间
    pFormatCtx = avformat_alloc_context();

    //打开视频
    if (avformat_open_input(&pFormatCtx,video, NULL, NULL) != 0) {
        std::cout << "can't open the file. \n";
        return;
    }

    //获取视频信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        std::cout << "Could't find stream infomation.\n";
        return;
    }

    //循环查找视频中包含的流信息，
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        //视频流
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
        }
        //音频流
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO  && audioStream < 0)
        {
            audioStream = i;
        }
    }

    //如果为-1 说明没有找到流
    if (videoStream == -1) {
        std::cout << "Didn't find a video stream.\n";
        return;
    }

    if (audioStream == -1) {
        std::cout << "Didn't find a audio stream.\n";
        return;
    }
    mVideoState.pFormatCtx = pFormatCtx;

    //查找音频解码器
    aCodecCtx = pFormatCtx->streams[audioStream]->codec;
    aCodec = avcodec_find_decoder(aCodecCtx->codec_id);

    if (aCodec == NULL) {
        std::cout << "ACodec not found.\n";
        return;
    }

    //打开音频解码器
    if (avcodec_open2(aCodecCtx, aCodec, NULL) < 0) {
       std::cout << "Could not open audio codec.\n";
        return;
    }

    mVideoState.audioStream = pFormatCtx->streams[audioStream];

    //查找视频解码器
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if (pCodec == NULL) {
        std::cout << "PCodec not found.\n";
        return;
    }

    //打开视频解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        std::cout << "Could not open video codec.\n";
        return;
    }

    mVideoState.videoStream = pFormatCtx->streams[videoStream];
    packet_queue_init(&mVideoState.videoQueue);

    //创建线程解码视频
    SDL_CreateThread(decodeVideo,"decodeVideo",&mVideoState);

    mVideoState.decoder = this;

//    std::thread videoThread(decodeVideo,&mVideoState);
//    videoThread.detach();

    //存放读取的视频
    AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket));

    //输出视频信息
    av_dump_format(pFormatCtx, 0, video, 0);

    while (1) {

        //限制，当队列里的数据超过这个值时，暂停读取
        if (mVideoState.audioQueue.size > MAX_AUDIO_SIZE ||
                mVideoState.videoQueue.size > MAX_VIDEO_SIZE) {
            SDL_Delay(10);
            continue;
        }

        if(av_read_frame(pFormatCtx, packet) < 0)//视频已读完
        {
            break;
        }

        if(packet->stream_index == videoStream)
        {
            packet_queue_put(&mVideoState.videoQueue,packet);
        }
        else if (packet->stream_index == audioStream) {
            packet_queue_put(&mVideoState.audioQueue, packet);
        }
        else {
            av_free_packet(packet);
        }
    }

    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
}
