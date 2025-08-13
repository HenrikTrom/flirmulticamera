#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <queue>

#include "hardware_constants.h"
#include "OnlineEncoder.h"
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"

namespace flirmulticamera {

class FlirVideoWriter
{
private:
    OnlineEncoder encoder;
    bool ShouldClose;
    void ThreadFunction(void);
    std::shared_ptr<std::thread> ThreadHandle;
    std::queue<Spinnaker::ImagePtr> FIFO;
    bool IsReadyFlag;
public:
    FlirVideoWriter(uint32_t Height, uint32_t Width, float FPS, std::mutex* mutex, std::string codecName, std::string pxlFormat);
    ~FlirVideoWriter();
    bool IsReady(void);
    void Open(std::string FileName);
    void Write(Spinnaker::ImagePtr img);
    void Close(void);
    void Terminate(void);
};

class VideoWriter
{
private:
    std::array<std::shared_ptr<FlirVideoWriter>, GLOBAL_CONST_NCAMS> writers;
    std::mutex mut;

    uint32_t width;
    uint32_t height;
    float fps;
    std::string codecName;
    std::string pxlFormat;
public:
    VideoWriter(uint32_t width, uint32_t height, float fps, std::string codecName, std::string pxlFormat);
    ~VideoWriter();
    void Open(std::vector<std::string> VideoNames);
    void Close(void);
    void Write(std::vector<Spinnaker::ImagePtr> &Frames);
    void Terminate(void);
};

} // namespace flirmulticamera