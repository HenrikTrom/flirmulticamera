#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <thread>
#include <atomic>
#include <algorithm>

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <flirmulticamera/config_parser.h>
#include <flirmulticamera/hardware_constants.h>


namespace flirmulticamera{


struct Frame
{
    Spinnaker::ImagePtr frameData;
    timespec Timestamp;
    int64_t imgTimestamp;
    uint64_t FrameCounter;
};

class FLirCameraImageEventHandler : public Spinnaker::ImageEventHandler
{
private:
    uint64_t FrameCounter;
    void OnImageEvent(Spinnaker::ImagePtr img);
    std::queue<Frame> FIFO;
    Spinnaker::ImagePtr last_img;
    double T;
    timespec PreviousTimestamp;
    int64_t imgPreviousTimestamp;
    int64_t offsetTimestamp;
    bool firstFrameFlag;
    std::atomic<bool> BufferingFlag;
    uint64_t last_ts;
    Spinnaker::CameraPtr pCam;
    // FIFO
public:
    std::string SN;
    FLirCameraImageEventHandler(){};
    FLirCameraImageEventHandler(Spinnaker::CameraPtr pCam, double T);
    ~FLirCameraImageEventHandler();
    bool IsFIFOEmpty(void);
    bool Get(Frame &frame);
    void Start(void);
    void Stop(void);
    void Pop(void);
    void setOffset(int64_t offset);
};

class FlirCameraHandler
{
private:
    std::array<std::string, GLOBAL_CONST_NCAMS> SNs;
    std::string MasterCamSN;
    std::string TopCamSN;
    const CameraSettings CamSettings;
    Spinnaker::SystemPtr system;
    Spinnaker::CameraList camList;
    timespec mean_ts;
    std::vector<FLirCameraImageEventHandler *> imageEventHandlers;

    bool SetCommand(Spinnaker::GenApi::INodeMap &NodeMap, std::string NodeName);
    bool SetEnumerationType(Spinnaker::GenApi::INodeMap &NodeMap, std::string NodeName, std::string EntryName);
    bool SetIntType(Spinnaker::GenApi::INodeMap &NodeMap, std::string NodeName, uint64_t Val);
    bool SetFloatType(Spinnaker::GenApi::INodeMap &NodeMap, std::string NodeName, double Val);
    bool SetBooleanType(Spinnaker::GenApi::INodeMap &NodeMap, std::string NodeName, bool Val);

    void ConfigureCommon(Spinnaker::CameraPtr pCam, Spinnaker::GenApi::INodeMap &nodeMap);
    void ConfigureMaster(Spinnaker::GenApi::INodeMap &nodeMap);
    void ConfigureSlave(Spinnaker::GenApi::INodeMap &nodeMap);

    void StartAcquisition(void);
    void StopAcquisition(void);

public:
    FlirCameraHandler(){};
    FlirCameraHandler(CameraSettings CamSettings);
    ~FlirCameraHandler();
    void change_exposure_test();
    void set_exposure(const int exposure_time);
    void set_size(const int width, const int height, const int binningvertival);
    void set_fps(const int fps);
    bool Configure(void);
    void Start(void);
    void Stop(void);
    // TODO overload/dynamic method
    bool Get(std::array<Frame, GLOBAL_CONST_NCAMS> &frame);
    bool IsFIFOEmpty(void);
};

} // namespace flirmulticamera