#pragma once

extern "C"{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/timestamp.h>
    #include <libavutil/opt.h>
    #include <libswscale/swscale.h>
}

#include <mutex>

namespace flirmulticamera {

// Class to manage an encoding session to encode videos in real time
class OnlineEncoder{

private:
    bool is_init;
    // Setting
    uint32_t Height;
    uint32_t Width;
    float FPS;
    // Encoding related contexts, buffers, streams, ...
    AVFormatContext *avfc;
    AVStream *avs;
    AVCodec *avc;
    AVCodecContext *avcc;
    AVFrame *input_frame, *conv_frame;
    AVPacket *output_packet;
    SwsContext* swsc;
    AVDictionary* muxer_opts;

    //Codec Pixel Formatting
    AVPixelFormat srcPxlFmt;
    int bytewidth;

    // Count frames and time stuff
    unsigned int frame_in;
    int ptsinterval;
    
    // Resulting filename
    std::string filename;

    // Mutex to manage access to non thread safe init.
    std::mutex* mut;

    // Codec name
    std::string codecName;

    /// receive packets available from the encoder and write them to file
    bool receive_and_write_packets(void);
public:
    /// Constructor
    OnlineEncoder(uint32_t Height, uint32_t Width, float FPS, std::mutex* mutex, std::string codecNamet, std::string pxlFormat);
    /// Destructor
    ~OnlineEncoder();

    /// Initialize the online encoder
    bool init(void);
    /// Open an output file and set stuff up
    bool open(std::string& fname);
    /// Flush the encoder and close the output
    bool close(void);
    /// Submit a frame for encoding
    bool encode(unsigned char* data, int linesize);
    // 
    void setAVPixelStuff(std::string pixelFormat);
};

} // namespace flirmulticamera
