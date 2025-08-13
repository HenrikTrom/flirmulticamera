#include "flirmulticamera/OnlineEncoder.h"
#include <iostream>

#define MBITRATE 4
#define MAX_MBITRATE 0
#define MIN_MBITRATE 0
#define VIDEO_INDEX 0
#define DEST_PIXFMT AV_PIX_FMT_YUV444P 

namespace flirmulticamera {

OnlineEncoder::OnlineEncoder(uint32_t Height, uint32_t Width, float FPS, std::mutex* mutex, std::string codecName, std::string pxlFormat)
    : frame_in(0), filename("undefined")
{
    this->is_init = false;
    this->Height = Height;
    this->Width = Width;
    this->FPS = FPS;
    this->codecName = codecName;
    this->mut = mutex;
    setAVPixelStuff(pxlFormat);
}

OnlineEncoder::~OnlineEncoder()
{
    if(&avcc != NULL){
        avcodec_free_context(&avcc);
    }
    avcc = NULL;
}

bool OnlineEncoder::init(void)
{
    if (!this->is_init)
    {
        this->is_init = true;
        AVRational input_framerate = {(int) round(this->FPS), 1};

        avc = avcodec_find_encoder_by_name(codecName.c_str());
        if(!avc){
            std::cerr << "could not find the proper codec" << std::endl;
            return false;
        }

        avcc = avcodec_alloc_context3(avc);
        if(!avcc){
            std::cerr << "could not allocated memory for codec context" << std::endl; 
            return false;
        }

        av_opt_set(avcc->priv_data, "preset", "medium", 0);
        av_opt_set(avcc->priv_data, "rc", "constqp", 0);
        av_opt_set(avcc->priv_data, "qp", "15", 0);

        av_log_set_level(AV_LOG_WARNING);

        avcc->height = this->Height;
        avcc->width = this->Width;
        avcc->pix_fmt = DEST_PIXFMT;
        avcc->bit_rate = MBITRATE * 1024 * 1024;
        avcc->rc_min_rate = MIN_MBITRATE * 1024 * 1024;
        avcc->rc_max_rate = MAX_MBITRATE * 1024 * 1024;
        avcc->time_base = av_inv_q(input_framerate);
        
        // For some reason, this method is not thread safe -> mutex
        mut->lock();
        auto res = avcodec_open2(avcc, avc, NULL);
        mut->unlock();
        if(res < 0){
            std::cerr << "could not open the codec" << std::endl;
            return false;
        }

        input_frame = av_frame_alloc();
        conv_frame = av_frame_alloc();
        if(!input_frame || !conv_frame){
            std::cerr << "failed to allocated memory for AVFrame" << std::endl;
            return false;
        }
        input_frame->width = this->Width;
        input_frame->height = this->Height;
        conv_frame->width = this->Width;
        conv_frame->height = this->Height;
        conv_frame->format = DEST_PIXFMT;
        if (av_frame_get_buffer(conv_frame, 0) < 0){
            std::cerr << "failed to allocate image buffer" << std::endl;
            return false;
        }

        swsc = sws_getContext(this->Width, this->Height, this->srcPxlFmt, this->Width, this->Height, DEST_PIXFMT, 0, NULL, NULL, 0);

        output_packet = av_packet_alloc();
        if(!output_packet){
            std::cerr << "could not allocate memory for output packet" << std::endl;
            return false;
        }
    }

    return true;
}

bool OnlineEncoder::open(std::string& fname)

{
    filename = fname;
    avformat_alloc_output_context2(&avfc, NULL, NULL, filename.c_str());
    if(!avfc){
        std::cerr << "could not allocate memory for output format" << std::endl;
        return false;
    }
    if(avfc->oformat->flags & AVFMT_GLOBALHEADER){
        avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    if(!(avfc->oformat->flags & AVFMT_NOFILE)){
        if(avio_open(&avfc->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0){
            std::cerr << "could not open the output file" << std::endl;
            return false;
        }
    }

    avs = avformat_new_stream(avfc, NULL);
    avcodec_parameters_from_context(avs->codecpar, avcc);
    avs->time_base = avcc->time_base;

    muxer_opts = NULL;

    if(avformat_write_header(avfc, &muxer_opts) < 0){
        std::cerr << "an error occurred when opening output file" << std::endl;
        return false;
    }
    AVRational temp = avs->time_base;
    ptsinterval = temp.den / temp.num * avcc->time_base.num / avcc->time_base.den;
    return true;

}

bool OnlineEncoder::close()
{
    if(avcodec_send_frame(avcc, NULL)){
        return false;
    }
    receive_and_write_packets();

    av_write_trailer(avfc);

    sws_freeContext(swsc);

    if(muxer_opts != NULL){
        av_dict_free(&muxer_opts);
        muxer_opts = NULL;
    }

    av_frame_unref(input_frame);
    av_frame_unref(conv_frame);

    if(input_frame != NULL){
        av_frame_free(&input_frame);
        input_frame = NULL;
    }

    if(conv_frame != NULL){
        av_frame_free(&conv_frame);
        conv_frame = NULL;
    }

    avformat_free_context(avfc); avfc = NULL;

    if(avcodec_open2(avcc, avc, NULL) < 0){
        std::cerr << "could not open the codec" << std::endl;
        return false;
    }


    if(output_packet != NULL){
        av_packet_free(&output_packet);
        output_packet = NULL;
    }

    if(&avcc != NULL){
            avcodec_close(avcc);
            avcodec_free_context(&avcc);
        }
    avc = NULL;
    avcc = NULL;

    return true;
}

bool OnlineEncoder::receive_and_write_packets()
{
    int response = 0;
    while(response == 0){
        response = avcodec_receive_packet(avcc, output_packet);

        if(response == AVERROR(EAGAIN) || response == AVERROR_EOF){
            break;
        }
        else if(response < 0){
            std::cerr << "Encoder error Nr. 2" << std::endl;
            return false; 
        }

        output_packet->stream_index = VIDEO_INDEX;
        output_packet->duration = ptsinterval;
        
        if(av_interleaved_write_frame(avfc, output_packet) != 0){
            std::cerr << "Failed to write package to video stream. Most probably the recording is unusable from now on. Please restart recorder." << std::endl;
        }
    }
    return true;
}

bool OnlineEncoder::encode(unsigned char* data, int linesize)
{
    input_frame->data[0] = data;
    input_frame->linesize[0] = linesize * bytewidth;
    input_frame->extended_data = &(input_frame->data[0]);
    input_frame->pts = frame_in * ptsinterval;
    frame_in++;

    sws_scale(swsc, input_frame->data,
                  input_frame->linesize, 0, input_frame->height, 
                  conv_frame->data, conv_frame->linesize);

    // send packet to the encoder
    conv_frame->pts = frame_in * ptsinterval;
    int response = avcodec_send_frame(avcc, conv_frame);
    if(response == AVERROR_EOF){
        std::cerr << "Encoder is already getting flushed, no more frames accepted" << std::endl;
        return false;
    }
    else if(response < 0){
        std::cerr << "Encoder error: " << response << std::endl;
    }

    // receive packets and write them to file
    response = receive_and_write_packets();
    if(response == false){
        return false;
    }

    av_packet_unref(output_packet);

    return true;
}

void OnlineEncoder::setAVPixelStuff(std::string pixelFormat){
    if(pixelFormat == "RGB8"){
        srcPxlFmt = AV_PIX_FMT_RGB24;
        bytewidth = 3;
    }
    else if(pixelFormat == "YCbCr422_8_CbYCrY"){
        srcPxlFmt = AV_PIX_FMT_UYVY422;
        bytewidth = 2;
    }
    else if(pixelFormat == "BayerGB8"){
        srcPxlFmt = AV_PIX_FMT_BAYER_GBRG8;
        bytewidth = 1;
    }
    else if(pixelFormat == "RS2_FORMAT_YUYV"){
        srcPxlFmt = AV_PIX_FMT_YUYV422;
        bytewidth = 2;
    }
}

} // namespace flirmulticamera