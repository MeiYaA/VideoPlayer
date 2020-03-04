#include "videodecoder.h"
#include <stdio.h>
#include <QDebug>

//清空队列
static void flush_packet_queue(PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1)
    {
        pkt1 = pkt->next;

        if(pkt1->pkt.data != (uint8_t *)"FLUSH")
        {

        }
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);

    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    SDL_UnlockMutex(q->mutex);
}

static void clear_packet_queue(PacketQueue *q) {
    flush_packet_queue(q);
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

void init_packet_queue(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
    q->size = 0;
    q->nb_packets = 0;
    q->first_pkt = NULL;
    q->last_pkt = NULL;
}

int put_packet_queue(PacketQueue *q, AVPacket *pkt) {

    AVPacketList *pkt1;
    if (av_dup_packet(pkt) < 0) {
        return -1;
    }
    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}

static int get_packet_queue(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
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

static int decode_audio_frame(VideoState *vs, double *pts_ptr)
{
    int len1, len2, decoded_data_size;
    AVPacket *pkt = &vs->audio_pkt;
    int got_frame = 0;
    int64_t dec_channel_layout;
    int wanted_nb_samples, resampled_data_size, n;

    double pts;

    for (;;) {

        while (vs->audio_pkt_size > 0) {

            if (vs->isPause == true) //判断暂停
            {
                return -1;
                SDL_Delay(10);
                continue;
            }

            if (!vs->audio_frame) {
                if (!(vs->audio_frame = av_frame_alloc())) {
                    return AVERROR(ENOMEM);
                }
            } else
                av_frame_unref(vs->audio_frame);

            len1 = avcodec_decode_audio4(vs->audio_st->codec, vs->audio_frame,
                                         &got_frame, pkt);
            if (len1 < 0) {
                // error, skip the frame
                vs->audio_pkt_size = 0;
                break;
            }

            vs->audio_pkt_data += len1;
            vs->audio_pkt_size -= len1;

            if (!got_frame)
                continue;

            /* 计算解码出来的桢需要的缓冲大小 */
            decoded_data_size = av_samples_get_buffer_size(NULL,
                                                           vs->audio_frame->channels, vs->audio_frame->nb_samples,
                                                           (AVSampleFormat)vs->audio_frame->format, 1);

            dec_channel_layout =
                    (vs->audio_frame->channel_layout
                     && vs->audio_frame->channels
                     == av_get_channel_layout_nb_channels(
                         vs->audio_frame->channel_layout)) ?
                        vs->audio_frame->channel_layout :
                        av_get_default_channel_layout(
                            vs->audio_frame->channels);

            wanted_nb_samples = vs->audio_frame->nb_samples;

            if (vs->audio_frame->format != vs->audio_src_fmt
                    || dec_channel_layout != vs->audio_src_channel_layout
                    || vs->audio_frame->sample_rate != vs->audio_src_freq
                    || (wanted_nb_samples != vs->audio_frame->nb_samples
                        && !vs->swr_ctx)) {
                if (vs->swr_ctx)
                    swr_free(&vs->swr_ctx);
                vs->swr_ctx = swr_alloc_set_opts(NULL,
                                                 vs->audio_tgt_channel_layout, (AVSampleFormat)vs->audio_tgt_fmt,
                                                 vs->audio_tgt_freq, dec_channel_layout,
                                                 (AVSampleFormat)vs->audio_frame->format, vs->audio_frame->sample_rate,
                                                 0, NULL);
                if (!vs->swr_ctx || swr_init(vs->swr_ctx) < 0) {
                    //fprintf(stderr,"swr_init() failed\n");
                    break;
                }
                vs->audio_src_channel_layout = dec_channel_layout;
                vs->audio_src_channels = vs->audio_st->codec->channels;
                vs->audio_src_freq = vs->audio_st->codec->sample_rate;
                vs->audio_src_fmt = vs->audio_st->codec->sample_fmt;
            }

            /* 这里我们可以对采样数进行调整，增加或者减少，一般可以用来做声画同步 */
            if (vs->swr_ctx) {
                const uint8_t **in =
                        (const uint8_t **) vs->audio_frame->extended_data;
                uint8_t *out[] = { vs->audio_buf2 };
                if (wanted_nb_samples != vs->audio_frame->nb_samples) {
                    if (swr_set_compensation(vs->swr_ctx,
                                             (wanted_nb_samples - vs->audio_frame->nb_samples)
                                             * vs->audio_tgt_freq
                                             / vs->audio_frame->sample_rate,
                                             wanted_nb_samples * vs->audio_tgt_freq
                                             / vs->audio_frame->sample_rate) < 0) {
                        //fprintf(stderr,"swr_set_compensation() failed\n");
                        break;
                    }
                }

                len2 = swr_convert(vs->swr_ctx, out,
                                   sizeof(vs->audio_buf2) / vs->audio_tgt_channels
                                   / av_get_bytes_per_sample(vs->audio_tgt_fmt),
                                   in, vs->audio_frame->nb_samples);
                if (len2 < 0) {
                    //fprintf(stderr,"swr_convert() failed\n");
                    break;
                }
                if (len2
                        == sizeof(vs->audio_buf2) / vs->audio_tgt_channels
                        / av_get_bytes_per_sample(vs->audio_tgt_fmt)) {
                    //fprintf(stderr,"warning: audio buffer vs probably too small\n");
                    swr_init(vs->swr_ctx);
                }
                vs->audio_buf = vs->audio_buf2;
                resampled_data_size = len2 * vs->audio_tgt_channels
                        * av_get_bytes_per_sample(vs->audio_tgt_fmt);
            } else {
                resampled_data_size = decoded_data_size;
                vs->audio_buf = vs->audio_frame->data[0];
            }

            pts = vs->audio_clock;
            *pts_ptr = pts;
            n = 2 * vs->audio_st->codec->channels;
            vs->audio_clock += (double) resampled_data_size
                    / (double) (n * vs->audio_st->codec->sample_rate);


            if (vs->seek_flag_audio)
            {
                //发生了跳转 则跳过关键帧到目的时间的这几帧
                if (vs->audio_clock < vs->seek_time)
                {
                    break;
                }
                else
                {
                    vs->seek_flag_audio = 0;
                }
            }
            // We have data, return it and come back for more later
            return resampled_data_size;
        }

        if (pkt->data)
            av_free_packet(pkt);
        memset(pkt, 0, sizeof(*pkt));

        if (vs->quit)
        {
            flush_packet_queue(&vs->audioQueue);
            return -1;
        }

        if (vs->isPause == true) //判断暂停
        {
            return -1;
        }

        if (get_packet_queue(&vs->audioQueue, pkt, 0) <= 0)
        {
            return -1;
        }

        //收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
        if(strcmp((char*)pkt->data,FLUSH_DATA) == 0)
        {
            avcodec_flush_buffers(vs->audio_st->codec);
            av_free_packet(pkt);
            continue;
        }

        vs->audio_pkt_data = pkt->data;
        vs->audio_pkt_size = pkt->size;

        /* if update, update the audio clock w/pts */
        if (pkt->pts != AV_NOPTS_VALUE) {
            vs->audio_clock = av_q2d(vs->audio_st->time_base) * pkt->pts;
        }
    }
    return 0;
}

//buf为需要调节音量的音频数据块首地址指针，size为长度，uRepeat为重复次数，通常设为1，vol为增益倍数,可以小于1
void raiseVolume(char* buf, int size, int uRepeat, double vol)
{
    if (!size)
    {
        return;
    }
    for (int i = 0; i < size; i += 2)
    {
        short wData;
        wData = MAKEWORD(buf[i], buf[i + 1]);
        long dwData = wData;
        for (int j = 0; j < uRepeat; j++)
        {
            dwData = dwData * vol;
            if (dwData < -0x8000)
            {
                dwData = -0x8000;
            }
            else if (dwData > 0x7FFF)
            {
                dwData = 0x7FFF;
            }
        }
        wData = LOWORD(dwData);
        buf[i] = LOBYTE(wData);
        buf[i + 1] = HIBYTE(wData);
    }
}

//音频解码回调函数
static void audio_callback(void *userdata, Uint8 *stream, int len) {
    VideoState *vs = (VideoState *) userdata;

    int len1, audio_data_size;

    double pts;
    //len是由SDL传入的SDL缓冲区的大小，如果这个缓冲未满，我们就一直往里填充数据
    while (len > 0) {
        /*  audio_buf_index 和 audio_buf_size 标示我们自己用来放置解码出来的数据的缓冲区，这些数据待copy到SDL缓冲区， 当audio_buf_index >= audio_buf_size的时候意味着我们的缓冲为空，没有数据可供复制，这时候需要调用audio_decode_frame来解码出更多的桢数据 */
        if (vs->audio_buf_index >= vs->audio_buf_size) {

            audio_data_size = decode_audio_frame(vs, &pts);

            /* audio_data_size < 0 标示没能解码出数据，我们默认播放静音 */
            if (audio_data_size < 0) {
                /* silence */
                vs->audio_buf_size = 1024;
                /* 清零，静音 */
                if (vs->audio_buf == NULL) return;
                memset(vs->audio_buf, 0, vs->audio_buf_size);
            } else {
                vs->audio_buf_size = audio_data_size;
            }
            vs->audio_buf_index = 0;
        }
        /*  查看stream可用空间，决定一次copy多少数据，剩下的下次继续copy */
        len1 = vs->audio_buf_size - vs->audio_buf_index;
        if (len1 > len) {
            len1 = len;
        }

        if (vs->audio_buf == NULL) return;

        if (vs->isMute) //静音
        {
            memset(vs->audio_buf + vs->audio_buf_index, 0, len1);
        }
        else
        {
            raiseVolume((char*)vs->audio_buf + vs->audio_buf_index, len1, 1, vs->mVolume);
        }

        memcpy(stream, (uint8_t *) vs->audio_buf + vs->audio_buf_index, len1);

        len -= len1;
        stream += len1;
        vs->audio_buf_index += len1;
    }
}

static double get_audio_clock(VideoState *vs)
{
    double pts;
    int hw_buf_size, bytes_per_sec, n;

    pts = vs->audio_clock; /* maintained in the audio thread */
    hw_buf_size = vs->audio_buf_size - vs->audio_buf_index;
    bytes_per_sec = 0;
    n = vs->audio_st->codec->channels * 2;
    if(vs->audio_st)
    {
        bytes_per_sec = vs->audio_st->codec->sample_rate * n;
    }
    if(bytes_per_sec)
    {
        pts -= (double)hw_buf_size / bytes_per_sec;
    }
    return pts;
}

static double synchronize_video(VideoState *vs, AVFrame *src_frame, double pts) {

    double frame_delay;

    if (pts != 0) {
        /* if we have pts, set video clock to it */
        vs->video_clock = pts;
    } else {
        /* if we aren't given a pts, set it to the clock */
        pts = vs->video_clock;
    }
    /* update the video clock */
    frame_delay = av_q2d(vs->video_st->codec->time_base);
    /* if we are repeating a frame, adjust clock accordingly */
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    vs->video_clock += frame_delay;
    return pts;
}

int open_audio_stream_component(VideoState *vs, int stream_index)
{
    AVFormatContext *ic = vs->ic;
    AVCodecContext *codecCtx;
    AVCodec *codec;

    int64_t wanted_channel_layout = 0;
    int wanted_nb_channels;

    if (stream_index < 0 || stream_index >= ic->nb_streams) {
        return -1;
    }

    codecCtx = ic->streams[stream_index]->codec;
    wanted_nb_channels = codecCtx->channels;
    if (!wanted_channel_layout
            || wanted_nb_channels
            != av_get_channel_layout_nb_channels(
                wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(
                    wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }

    /* 把设置好的参数保存到大结构中 */
    vs->audio_src_fmt = vs->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
    vs->audio_src_freq = vs->audio_tgt_freq = 44100;
    vs->audio_src_channel_layout = vs->audio_tgt_channel_layout =
            wanted_channel_layout;
    vs->audio_src_channels = vs->audio_tgt_channels = 2;

    codec = avcodec_find_decoder(codecCtx->codec_id);
    if (!codec || (avcodec_open2(codecCtx, codec, NULL) < 0)) {
        fprintf(stderr,"Unsupported codec!\n");
        return -1;
    }
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (codecCtx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        vs->audio_st = ic->streams[stream_index];
        vs->audio_buf_size = 0;
        vs->audio_buf_index = 0;
        memset(&vs->audio_pkt, 0, sizeof(vs->audio_pkt));
        break;
    default:
        break;
    }
    return 0;
}

int decode_video_thread(void *arg)
{
    VideoState *vs = (VideoState *) arg;
    AVPacket pkt1, *packet = &pkt1;

    int ret, got_picture, numBytes;

    double video_pts = 0; //当前视频的pts
    double audio_pts = 0; //音频pts


    //解码视频相关
    AVFrame *pFrame, *pFrameRGB;
    uint8_t *out_buffer_rgb; //解码后的rgb数据
    struct SwsContext *img_convert_ctx;  //用于解码后的视频格式转换

    AVCodecContext *pCodecCtx = vs->video_st->codec; //视频解码器

    pFrame = av_frame_alloc();
    pFrameRGB = av_frame_alloc();

    //这里我们改成了 将解码后的YUV数据转换成RGB32
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                     pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                                     AV_PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

    numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, pCodecCtx->width,pCodecCtx->height);

    out_buffer_rgb = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture *) pFrameRGB, out_buffer_rgb, AV_PIX_FMT_RGB32,
                   pCodecCtx->width, pCodecCtx->height);

    while(1)
    {
        if (vs->quit)
        {
            flush_packet_queue(&vs->videoQueue); //清空队列
            break;
        }
        if (vs->isPause == true) //判断暂停
        {
            SDL_Delay(10);
            continue;
        }
        if (get_packet_queue(&vs->videoQueue, packet, 0) <= 0)
        {
            if (vs->readFinished)
            {
                //队列里面没有数据了且读取完毕了
                break;
            }
            else
            {
                SDL_Delay(1); //队列只是暂时没有数据而已
                continue;
            }
        }
        //收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
        if(strcmp((char*)packet->data,FLUSH_DATA) == 0)
        {
            avcodec_flush_buffers(vs->video_st->codec);
            av_free_packet(packet);
            continue;
        }

        ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture,packet);

        if (ret < 0) {
            av_free_packet(packet);
            continue;
        }

        if (packet->dts == AV_NOPTS_VALUE && pFrame->opaque&& *(uint64_t*) pFrame->opaque != AV_NOPTS_VALUE)
        {
            video_pts = *(uint64_t *) pFrame->opaque;
        }
        else if (packet->dts != AV_NOPTS_VALUE)
        {
            video_pts = packet->dts;
        }
        else
        {
            video_pts = 0;
        }

        video_pts *= av_q2d(vs->video_st->time_base);
        video_pts = synchronize_video(vs, pFrame, video_pts);

        if (vs->seek_flag_video)
        {
            //发生了跳转 则跳过关键帧到目的时间的这几帧
            if (video_pts < vs->seek_time)
            {
                av_free_packet(packet);
                continue;
            }
            else
            {
                vs->seek_flag_video = 0;
            }
        }

        while(1)
        {
            if (vs->quit)
            {
                break;
            }

            if (vs->readFinished && vs->audioQueue.size == 0)
            {//读取完了 且音频数据也播放完了 就剩下视频数据了  直接显示出来了 不用同步了
                break;
            }

            audio_pts = vs->audio_clock;

            //主要是 跳转的时候 我们把video_clock设置成0了
            //因此这里需要更新video_pts
            //否则当从后面跳转到前面的时候 会卡在这里
            video_pts = vs->video_clock;

            if (video_pts <= audio_pts) break;

            int delayTime = (video_pts - audio_pts) * 1000;

            delayTime = delayTime > 5 ? 5:delayTime;

            SDL_Delay(delayTime);
        }

        if (got_picture) {
            sws_scale(img_convert_ctx,
                      (uint8_t const * const *) pFrame->data,
                      pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data,
                      pFrameRGB->linesize);

            //把这个RGB数据 用QImage加载
            QImage tmpImg((uchar *)out_buffer_rgb,pCodecCtx->width,pCodecCtx->height,QImage::Format_RGB32);
            QImage image = tmpImg.copy(); //把图像复制一份 传递给界面显示
            //            QImage image = tmpImg.convertToFormat(QImage::Format_RGB888,Qt::NoAlpha); //去掉透明的部分 有些奇葩的视频会透明
            vs->player->disPlayVideo(image); //调用激发信号的函数
        }

        av_free_packet(packet);

    }
    av_free(pFrame);
    av_free(pFrameRGB);
    av_free(out_buffer_rgb);

    sws_freeContext(img_convert_ctx);

    if (!vs->quit)
    {
        vs->quit = true;
    }

    vs->videoThreadFinished = true;
    return 0;
}

VideoDecoder::VideoDecoder(VideoPlayer *vp)
{
    memset(&m_videoState,0,sizeof(VideoState));

    m_videoPlayer = vp;
    m_playerState = Stop;
    m_audioID = 0;
    m_isMute = false;
    m_volume = 1;
}

VideoDecoder::~VideoDecoder()
{
    //关闭SDL音频播放设备
    if (m_audioID != 0)
    {
        SDL_LockAudioDevice(m_audioID);
        SDL_CloseAudioDevice(m_audioID);
        SDL_UnlockAudioDevice(m_audioID);
        m_audioID = 0;
    }
    clear();
}

bool VideoDecoder::setFileName(QString path)
{
    if (m_playerState != Stop)
    {
        return false;
    }

    m_fileName = path;

    memset(&m_videoState,0,sizeof(VideoState));

    this->start(); //启动线程

    return true;
}

bool VideoDecoder::play()
{
    m_videoState.isPause = false;

    if (m_playerState != Pause)
    {
        return false;
    }

    m_playerState = Playing;
    emit sig_stateChanged(Playing);

    return true;
}

bool VideoDecoder::pause()
{
    m_videoState.isPause = true;

    if (m_playerState != Playing)
    {
        return false;
    }

    m_playerState = Pause;

    emit sig_stateChanged(Pause);

    return true;
}

bool VideoDecoder::stop(bool isWait)
{
    if (m_playerState == Stop)
    {
        return false;
    }

    m_playerState = Stop;
    m_videoState.quit = true;
    if (isWait)
    {
        while(!m_videoState.readThreadFinished)
        {
            SDL_Delay(3);
        }
    }
    return true;
}

void VideoDecoder::seek(int64_t pos)
{
    if(!m_videoState.seek_req)
    {
        m_videoState.seek_pos = pos;
        m_videoState.seek_req = 1;
    }
}

void VideoDecoder::setVolume(float value)
{
    m_volume = value;
    m_videoState.mVolume = value;
}

int64_t VideoDecoder::getTotalTime()
{
    return m_videoState.ic->duration;
}

double VideoDecoder::getCurrentTime()
{
    return m_videoState.audio_clock;
}

void VideoDecoder::disPlayVideo(QImage img)
{
    emit sig_getOneFrame(img);  //发送信号
}

void VideoDecoder::run()
{
    char file_path[1280] = {0};
    strcpy(file_path,m_fileName.toUtf8().data());

    memset(&m_videoState,0,sizeof(VideoState)); //为了安全起见  先将结构体的数据初始化成0了

    m_videoState.isMute = m_isMute;
    m_videoState.mVolume = m_volume;

    VideoState *is = &m_videoState;

    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;

    AVCodecContext *aCodecCtx;
    AVCodec *aCodec;

    int audioStream ,videoStream, i;


    //Allocate an AVFormatContext.
    pFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&pFormatCtx, file_path, NULL, NULL) != 0) {
        printf("can't open the file. \n");
        return;
    }

    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Could't find stream infomation.\n");
        return;
    }

    videoStream = -1;
    audioStream = -1;

    //循环查找视频中包含的流信息，
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
        }
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO  && audioStream < 0)
        {
            audioStream = i;
        }
    }

    //如果videoStream为-1 说明没有找到视频流
    if (videoStream == -1) {
        printf("Didn't find a video stream.\n");
        return;
    }

    if (audioStream == -1) {
        printf("Didn't find a audio stream.\n");
        return;
    }

    is->ic = pFormatCtx;
    is->videoStream = videoStream;
    is->audioStream = audioStream;

    emit sig_totalTimeChanged(getTotalTime());

    if (audioStream >= 0) {
        /* 所有设置SDL音频流信息的步骤都在这个函数里完成 */
        open_audio_stream_component(&m_videoState, audioStream);
    }

    //查找音频解码器
    aCodecCtx = pFormatCtx->streams[audioStream]->codec;
    aCodec = avcodec_find_decoder(aCodecCtx->codec_id);

    if (aCodec == NULL) {
        printf("ACodec not found.\n");
        return;
    }

    //打开音频解码器
    if (avcodec_open2(aCodecCtx, aCodec, NULL) < 0) {
        printf("Could not open audio codec.\n");
        return;
    }

    is->audio_st = pFormatCtx->streams[audioStream];

    //查找视频解码器
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if (pCodec == NULL) {
        printf("PCodec not found.\n");
        return;
    }

    //打开视频解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open video codec.\n");
        return;
    }

    is->video_st = pFormatCtx->streams[videoStream];

    init_packet_queue(&m_videoState.audioQueue);
    init_packet_queue(&m_videoState.videoQueue);


    //创建一个线程专门用来解码视频
    is->video_tid = SDL_CreateThread(decode_video_thread, "decode_video_thread", &m_videoState);


    is->player = this;

    AVPacket *packet = (AVPacket *) malloc(sizeof(AVPacket)); //分配一个packet 用来存放读取的视频
//    av_new_packet(packet, y_size); //av_read_frame 会给它分配空间 因此这里不需要了

//    av_dump_format(pFormatCtx, 0, file_path, 0); //输出视频信息

    qDebug()<<__FUNCTION__<<is->quit;
    m_playerState = Playing;
    emit sig_stateChanged(Playing);

    openSDL();

    SDL_LockAudioDevice(m_audioID);
    SDL_PauseAudioDevice(m_audioID,0);
    SDL_UnlockAudioDevice(m_audioID);

    while (1)
    {
        if (is->quit)
        {
            //停止播放了
            break;
        }

        if (is->seek_req)
        {
            int stream_index = -1;
            int64_t seek_target = is->seek_pos;

            if (is->videoStream >= 0)
                stream_index = is->videoStream;
            else if (is->audioStream >= 0)
                stream_index = is->audioStream;

            AVRational aVRational = {1, AV_TIME_BASE};
            if (stream_index >= 0) {
                seek_target = av_rescale_q(seek_target, aVRational,
                        pFormatCtx->streams[stream_index]->time_base);
            }

            if (av_seek_frame(is->ic, stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
                fprintf(stderr, "%s: error while seeking\n",is->ic->filename);
            } else {
                if (is->audioStream >= 0) {
                    AVPacket *packet = (AVPacket *) malloc(sizeof(AVPacket)); //分配一个packet
                    av_new_packet(packet, 10);
                    strcpy((char*)packet->data,FLUSH_DATA);
                    flush_packet_queue(&is->audioQueue); //清除队列
                    put_packet_queue(&is->audioQueue, packet); //往队列中存入用来清除的包
                }
                if (is->videoStream >= 0) {
                    AVPacket *packet = (AVPacket *) malloc(sizeof(AVPacket)); //分配一个packet
                    av_new_packet(packet, 10);
                    strcpy((char*)packet->data,FLUSH_DATA);
                    flush_packet_queue(&is->videoQueue); //清除队列
                    put_packet_queue(&is->videoQueue, packet); //往队列中存入用来清除的包
                    is->video_clock = 0;
                }
            }
            is->seek_req = 0;
            is->seek_time = is->seek_pos / 1000000.0;
            is->seek_flag_audio = 1;
            is->seek_flag_video = 1;

            if (is->isPause)
            {
                is->isNeedPause = true;
                is->isPause = false;
            }

        }

        //这里做了个限制  当队列里面的数据超过某个大小的时候 就暂停读取  防止一下子就把视频读完了，导致的空间分配不足
        /* 这里audioq.size是指队列中的所有数据包带的音频数据的总量或者视频数据总量，并不是包的数量 */
        //这个值可以稍微写大一些
//        qDebug()<<__FUNCTION__<<is->audioq.size<<MAX_AUDIO_SIZE<<is->videoq.size<<MAX_VIDEO_SIZE;
        if (is->audioQueue.size > MAX_AUDIO_SIZE || is->videoQueue.size > MAX_VIDEO_SIZE) {
            SDL_Delay(10);
            continue;
        }

//        qDebug()<<__FUNCTION__<<"is->isPause"<<is->isPause;

        if (is->isPause == true)
        {
            SDL_Delay(10);
            continue;
        }

        if (av_read_frame(pFormatCtx, packet) < 0)
        {
            is->readFinished = true;

//            qDebug()<<__FUNCTION__<<"av_read_frame<0";

            if (is->quit)
            {
                break; //解码线程也执行完了 可以退出了
            }

            SDL_Delay(10);
            continue;
        }

        if (packet->stream_index == videoStream)
        {
            put_packet_queue(&is->videoQueue, packet);
            //这里我们将数据存入队列 因此不调用 av_free_packet 释放
        }
        else if( packet->stream_index == audioStream )
        {
            put_packet_queue(&is->audioQueue, packet);
            //这里我们将数据存入队列 因此不调用 av_free_packet 释放
        }
        else
        {
            // Free the packet that was allocated by av_read_frame
            av_free_packet(packet);
        }
    }


    //文件读取结束 跳出循环的情况
    //等待播放完毕
    while (!is->quit) {
        SDL_Delay(100);
    }

    if (m_playerState != Stop) //不是外部调用的stop 是正常播放结束
    {
        stop();
    }

    SDL_LockAudioDevice(m_audioID);
    SDL_PauseAudioDevice(m_audioID,1);
    SDL_UnlockAudioDevice(m_audioID);

    closeSDL();

 qDebug()<<__FUNCTION__<<"444";
    while(!m_videoState.videoThreadFinished)
    {
        msleep(10);
    } //确保视频线程结束后 再销毁队列

    avcodec_close(aCodecCtx);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);

    free(packet);

    clear_packet_queue(&m_videoState.videoQueue);
    clear_packet_queue(&m_videoState.audioQueue);

    is->readThreadFinished = true;

    emit sig_stateChanged(Stop);
}

int VideoDecoder::openSDL()
{
    VideoState *vs = &m_videoState;

    SDL_AudioSpec wanted_spec, spec;
    int64_t wanted_channel_layout = 0;
    int wanted_nb_channels = 2;
    int samplerate = 44100;

    if (!wanted_channel_layout
            || wanted_nb_channels
            != av_get_channel_layout_nb_channels(
                wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(
                    wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }

    wanted_spec.channels = av_get_channel_layout_nb_channels(
                wanted_channel_layout);
    wanted_spec.freq = samplerate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        //fprintf(stderr,"Invalid sample rate or channel count!\n");
        return -1;
    }
    wanted_spec.format = AUDIO_S16SYS; // 具体含义请查看“SDL宏定义”部分
    wanted_spec.silence = 0;            // 0指示静音
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;  // 自定义SDL缓冲区大小
    wanted_spec.callback = audio_callback;        // 音频解码的关键回调函数
    wanted_spec.userdata = vs;                    // 传给上面回调函数的外带数据

    int num = SDL_GetNumAudioDevices(0);
    for (int i=0;i<num;i++)
    {
        m_audioID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(i,0), false, &wanted_spec, &spec,0);
        if (m_audioID > 0)
        {
            break;
        }
    }

    /* 检查实际使用的配置（保存在spec,由SDL_OpenAudio()填充） */
    if (spec.format != AUDIO_S16SYS) {
        qDebug()<<"SDL advvsed audio format %d vs not supported!"<<spec.format;
        return -1;
    }

    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            fprintf(stderr,"SDL advvsed channel count %d vs not supported!\n",spec.channels);
            return -1;
        }
    }

    vs->audio_hw_buf_size = spec.size;

    /* 把设置好的参数保存到大结构中 */
    vs->audio_src_fmt = vs->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
    vs->audio_src_freq = vs->audio_tgt_freq = spec.freq;
    vs->audio_src_channel_layout = vs->audio_tgt_channel_layout =
            wanted_channel_layout;
    vs->audio_src_channels = vs->audio_tgt_channels = spec.channels;

    vs->audio_buf_size = 0;
    vs->audio_buf_index = 0;
    memset(&vs->audio_pkt, 0, sizeof(vs->audio_pkt));

    return 0;
}

void VideoDecoder::closeSDL()
{
    if (m_audioID > 0)
    {
        SDL_CloseAudioDevice(m_audioID);
    }

    m_audioID = -1;
}

void VideoDecoder::clear()
{
    if (m_videoState.swr_ctx != NULL)
    {
        swr_free(&m_videoState.swr_ctx);
        m_videoState.swr_ctx = NULL;
    }

    if (m_videoState.audio_frame!= NULL) {
        av_frame_free(&m_videoState.audio_frame);
        m_videoState.audio_frame = NULL;
    }
}
