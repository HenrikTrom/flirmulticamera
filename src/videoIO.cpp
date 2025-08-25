#include "flirmulticamera/videoIO.h"

namespace flirmulticamera {

// FLIR VIDEO WRITER 
void FlirVideoWriter::ThreadFunction(void)
{
    if (!this->encoder.init())
    {
        return;
    }
    this->IsReadyFlag = true;
    while (!this->ShouldClose || !this->FIFO.empty())
    {
        if (!this->FIFO.empty())
        {
            auto &data = this->FIFO.front();
            this->encoder.encode((unsigned char*)data->GetData(), data->GetWidth());
            data->Release();
            this->FIFO.pop();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

FlirVideoWriter::FlirVideoWriter(uint32_t Height, uint32_t Width, float FPS, std::mutex* mutex, std::string codecName, std::string pxlFormat) : 
encoder(Height, Width, FPS, mutex, codecName, pxlFormat)
{
    this->ShouldClose = false;
    this->IsReadyFlag = false;
    this->ThreadHandle.reset(new std::thread{&FlirVideoWriter::ThreadFunction, this});
}

FlirVideoWriter::~FlirVideoWriter()
{

}

bool FlirVideoWriter::IsReady(void)
{
    return this->IsReadyFlag;
}

void FlirVideoWriter::Write(Spinnaker::ImagePtr img)
{
    this->FIFO.push(img);
}

void FlirVideoWriter::Open(std::string FileName)
{
    if (!this->encoder.init())
    {
        return;
    }
    this->encoder.open(FileName);
}

void FlirVideoWriter::Close(void)
{
    while(!this->FIFO.empty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    this->encoder.close();
}

void FlirVideoWriter::Terminate(void)
{
    this->ShouldClose = true;
    this->ThreadHandle->join();
}

// VIDEO WRITER
VideoWriter::VideoWriter(uint32_t width, uint32_t height, float fps, std::string codecName, std::string pxlFormat)
{
    this->width = width;
    this->height = height;
    this->fps = fps;
    this->codecName = codecName;
    this->pxlFormat = pxlFormat;
}

VideoWriter::~VideoWriter()
{

}

void VideoWriter::Open(std::vector<std::string> VideoNames)
{
    for (std::size_t i = 0; i < GLOBAL_CONST_NCAMS; i++)
    {
        this->writers.at(i).reset(
            new FlirVideoWriter{this->height, this->width, this->fps, &this->mut, this->codecName, this->pxlFormat}
        );
    }
    bool result{true};
    do
    {
        result = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (auto writer : this->writers)
        {   
            result &= writer->IsReady();
        }
    }
    while(!result);

    for (uint8_t i = 0; i < this->writers.size(); i++)
    {
        this->writers[i]->Open(VideoNames[i]);
    }
}

void VideoWriter::Close(void)
{
    for (auto writer : this->writers)
    {
        writer->Close();
        writer->Terminate();
    }
}

void VideoWriter::Write(std::vector<Spinnaker::ImagePtr> &Frames)
{
    for (uint8_t i = 0; i < Frames.size(); i++)
    {
        this->writers[i]->Write(Frames[i]);
    }
}

void VideoWriter::Terminate(void)
{
    for (auto& flir_writer : this->writers)
    {
        flir_writer->Terminate();
    }
}

} // namespace flirmulticamera