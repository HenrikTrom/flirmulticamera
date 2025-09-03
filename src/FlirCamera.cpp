#include <flirmulticamera/FlirCamera.h>
#include <algorithm>
using namespace std;
using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;

namespace flirmulticamera{

FLirCameraImageEventHandler::FLirCameraImageEventHandler(CameraPtr pCam, double T)
{
    // Retrieve device serial number
    INodeMap &nodeMap = pCam->GetTLDeviceNodeMap();

    this->SN = "";
    CStringPtr ptrDeviceSerialNumber = nodeMap.GetNode("DeviceSerialNumber");
    if (IsAvailable(ptrDeviceSerialNumber) && IsReadable(ptrDeviceSerialNumber))
    {
        this->SN = ptrDeviceSerialNumber->GetValue();
    }
    this->FrameCounter = 0;
    this->BufferingFlag = false;
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    this->PreviousTimestamp = now;
    this->imgPreviousTimestamp = 0;
    pCam->TimestampLatch.Execute();
    this->offsetTimestamp = pCam->Timestamp.GetValue();
    this->T = T;
    this->firstFrameFlag = true;
    this->pCam = pCam; 
}

FLirCameraImageEventHandler::~FLirCameraImageEventHandler()
{
    
}

void FLirCameraImageEventHandler::OnImageEvent(ImagePtr img)
{
    timespec monotime{};
    clock_gettime(CLOCK_MONOTONIC, &monotime);
    // Check image retrieval status
    if (img->IsIncomplete())
    {
        cout << "Image incomplete with image status " << img->GetImageStatus() << "..." << endl;
    }
    else
    {

        if (this->BufferingFlag)
        {
            // convert and push into FIFO
            Frame frame;
            frame.Timestamp = monotime;
            frame.imgTimestamp = (img->GetChunkData().GetTimestamp() - this->offsetTimestamp);

            frame.FrameCounter = this->FrameCounter++;
            // validate the timestamp to see if there is a out of sync
            if (this->firstFrameFlag)
            {
                this->firstFrameFlag = false;
                this->PreviousTimestamp = frame.Timestamp;
                this->imgPreviousTimestamp = (img->GetChunkData().GetTimestamp()  - this->offsetTimestamp);
            }
            else
            {
                // T  = 1/framerate
                const double errorMargin = this->T * 0.2;
		        double timediff =  (frame.imgTimestamp - this->imgPreviousTimestamp) * 1e-9 - this->T;
                if (timediff > errorMargin)
                {
                    spdlog::warn("Missed a frame; FNo,SN,imTs: {} | {} | {} ms", frame.FrameCounter, this->SN, timediff*1e3);
                }
                this->PreviousTimestamp = frame.Timestamp;
                this->imgPreviousTimestamp = frame.imgTimestamp;
            }
            frame.frameData = img;
            this->FIFO.push(frame);
        }
        else
        {
            img->Release();
        }
    }
}

bool FLirCameraImageEventHandler::IsFIFOEmpty(void)
{
    return this->FIFO.empty();
}

bool FLirCameraImageEventHandler::Get(Frame &frame)
{
    if (this->FIFO.empty())
    {
        return false;
    }
    else
    {
        frame = this->FIFO.front();
        return true;
    }
}

void FLirCameraImageEventHandler::Start(void)
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    this->FrameCounter = 0;
    this->PreviousTimestamp = now;
    this->imgPreviousTimestamp = (this->pCam->Timestamp.GetValue() - this->offsetTimestamp);
    this->BufferingFlag = true;
}

void FLirCameraImageEventHandler::Stop(void)
{
    this->BufferingFlag = false;
    while (!this->FIFO.empty())
    {
        this->FIFO.pop();
    }
}

void FLirCameraImageEventHandler::Pop(void)
{
    if (!this->FIFO.empty())
    {
        this->FIFO.pop();
    }
}

void FLirCameraImageEventHandler::setOffset(int64_t offset){
    this->offsetTimestamp = offset;
}

// TODO dynamic
FlirCameraHandler::FlirCameraHandler(CameraSettings CamSettings) : CamSettings(CamSettings)
{
    for (int i = 0; i < GLOBAL_CONST_NCAMS; i++)
    {
        this->SNs.at(i) = std::string(GLOBAL_CONST_CAMERA_SERIAL_NUMBERS.at(i));
    }
    // TODO
    this->MasterCamSN = std::string(GLOBAL_CONST_MASTER_CAM_SERIAL);
    this->TopCamSN = std::string(GLOBAL_CONST_TOP_CAM_SERIAL);
}

FlirCameraHandler::~FlirCameraHandler()
{
    this->StopAcquisition();
    // unregister event handlers
    for (auto &eventhandler : this->imageEventHandlers)
    {
        delete eventhandler;
    }
    this->camList.Clear();
    this->system->ReleaseInstance();
}

bool FlirCameraHandler::SetCommand(Spinnaker::GenApi::INodeMap &NodeMap, std::string NodeName)
{
    CCommandPtr ptrNode = NodeMap.GetNode(NodeName.c_str());
    if (IsAvailable(ptrNode) && IsWritable(ptrNode))
    {
        ptrNode->Execute();
        return false;
    }
    else
    {
        cout << NodeName << " not available..." << endl;
        return true;
    }
}

bool FlirCameraHandler::SetEnumerationType(INodeMap &NodeMap, std::string NodeName, std::string EntryName)
{
    CEnumerationPtr ptrNode = NodeMap.GetNode(NodeName.c_str());
    if (IsAvailable(ptrNode) && IsWritable(ptrNode))
    {
        // Retrieve the desired entry node from the enumeration node
        CEnumEntryPtr ptrEntry = ptrNode->GetEntryByName(EntryName.c_str());
        if (IsAvailable(ptrEntry) && IsReadable(ptrEntry))
        {
            // Retrieve the integer value from the entry node
            int64_t ptrEntryVal = ptrEntry->GetValue();
            // Set integer as new value for enumeration node
            ptrNode->SetIntValue(ptrEntryVal);
        }
        else
        {
            cout << EntryName << " not available..." << endl;
            return false;
        }
    }
    else
    {
        cout << NodeName << " not available..." << endl;
        return false;
    }
    return true;
}

bool FlirCameraHandler::SetIntType(INodeMap &NodeMap, std::string NodeName, uint64_t Val)
{
    CIntegerPtr ptrNode = NodeMap.GetNode(NodeName.c_str());
    if (IsAvailable(ptrNode) && IsWritable(ptrNode))
    {
        ptrNode->SetValue(Val);
        return false;
    }
    else
    {
        cout << NodeName << " not available..." << endl;
        return true;
    }
}

bool FlirCameraHandler::SetFloatType(INodeMap &NodeMap, std::string NodeName, double Val)
{
    CFloatPtr ptrNode = NodeMap.GetNode(NodeName.c_str());
    if (IsAvailable(ptrNode) && IsWritable(ptrNode))
    {
        ptrNode->SetValue(Val);
        return false;
    }
    else
    {
        cout << NodeName << " not available..." << endl;
        return true;
    }
}

bool FlirCameraHandler::SetBooleanType(INodeMap &NodeMap, std::string NodeName, bool Val)
{
    CBooleanPtr ptrNode = NodeMap.GetNode(NodeName.c_str());
    if (!IsAvailable(ptrNode) || !IsWritable(ptrNode))
    {
        cout << NodeName << "AcquisitionFrameRateControl not available" << endl;
        return false;
    }
    else
    {
        ptrNode->SetValue(Val);
        return true;
    }
}

void FlirCameraHandler::ConfigureCommon(CameraPtr pCam, INodeMap &nodeMap)
{
    // this->CamSettings.
    this->SetEnumerationType(nodeMap, "UserSetSelector", "UserSet2");
    this->SetCommand(nodeMap, "UserSetLoad");
    // this->CamSettings.
    this->SetEnumerationType(nodeMap, "PixelFormat",  this->CamSettings.pixel_format);
    this->SetEnumerationType(nodeMap, "VideoMode", this->CamSettings.video_mode);
    // Can only access Binning if not in Mode0
    if (this->CamSettings.video_mode != "Mode0") {
        this->SetEnumerationType(nodeMap, "BinningControl", "Average");
        this->SetIntType(nodeMap, "BinningVertical", this->CamSettings.binning_vertical);
    }
    this->SetIntType(nodeMap, "OffsetX", this->CamSettings.offsetX);
    this->SetIntType(nodeMap, "OffsetY", this->CamSettings.offsetY);
    this->SetIntType(nodeMap, "Width", this->CamSettings.width);
    this->SetIntType(nodeMap, "Height", this->CamSettings.height);

    this->SetBooleanType(nodeMap, "BlackLevelClampingEnable", true);
    this->SetFloatType(nodeMap, "BlackLevel", this->CamSettings.black_level);

    this->SetEnumerationType(nodeMap, "GainAuto", "Off");
    this->SetFloatType(nodeMap, "Gain", this->CamSettings.gain);

    this->SetEnumerationType(nodeMap, "ExposureMode", "Timed");
    this->SetEnumerationType(nodeMap, "ExposureAuto", "Off");
    this->SetEnumerationType(nodeMap, "BalanceWhiteAuto", "Off");
    this->SetFloatType(nodeMap, "ExposureTime", this->CamSettings.exposure_time);

    //this->SetBooleanType(nodeMap, "ChunkModeActive", false);
    this->SetBooleanType(nodeMap, "ChunkModeActive", true);
    this->SetEnumerationType(nodeMap, "ChunkSelector", "Timestamp");
    this->SetBooleanType(nodeMap, "ChunkEnable", true);

    FLirCameraImageEventHandler *imgEventHandlerTmp = new FLirCameraImageEventHandler{pCam, 1.0 / this->CamSettings.fps};
    this->imageEventHandlers.push_back(imgEventHandlerTmp);
    pCam->RegisterEventHandler(*this->imageEventHandlers[this->imageEventHandlers.size() - 1]);

    // this->SetCommand(nodeMap, "TimestampLatch");

}

void FlirCameraHandler::ConfigureMaster(INodeMap &nodeMap)
{
    // GPIO& trigger
    this->SetEnumerationType(nodeMap, "LineSelector", std::string(GLOBAL_CONST_MASTER_LINE));
    this->SetEnumerationType(nodeMap, "LineMode", "Output");
    this->SetEnumerationType(nodeMap, "LineSource", "ExposureActive");

    this->SetEnumerationType(nodeMap, "TriggerMode", "Off");
    this->SetEnumerationType(nodeMap, "AcquisitionFrameRateAuto", "Off");
    this->SetBooleanType(nodeMap, "AcquisitionFrameRateEnabled", true);
    this->SetFloatType(nodeMap, "AcquisitionFrameRate", this->CamSettings.fps);
}

void FlirCameraHandler::ConfigureSlave(INodeMap &nodeMap)
{
    // GPIO& trigger
    this->SetEnumerationType(nodeMap, "LineSelector", std::string(GLOBAL_CONST_SLAVE_LINE));
    // this->SetEnumerationType(nodeMap, "LineMode", "Input");

    this->SetEnumerationType(nodeMap, "TriggerSelector", "FrameStart");
    this->SetEnumerationType(nodeMap, "TriggerMode", "On");
    this->SetEnumerationType(nodeMap, "TriggerSource", std::string(GLOBAL_CONST_SLAVE_LINE));
    this->SetEnumerationType(nodeMap, "TriggerActivation", "FallingEdge");
    this->SetEnumerationType(nodeMap, "TriggerOverlap", "ReadOut");
}

bool FlirCameraHandler::Configure(void)
{
    this->system = System::GetInstance();
    this->camList = this->system->GetCameras();
    // TODO Dynamic
    if (this->camList.GetSize() < GLOBAL_CONST_NCAMS)
    {
        spdlog::error(
            "Number of cameras detected ({}) < Number of cameras configured ({})",
            this->camList.GetSize(),
            GLOBAL_CONST_NCAMS
        );
        return false;
    }
    else{
        spdlog::info("Detected cameras >= configuration: {}", this->camList.GetSize());
    }
    if (this->camList.GetSize() == 0)
    {
        cout << "\tNo devices detected." << endl
                << endl;
        return false;
    }
    for (auto &SN_ordered : this->SNs)
    {
        spdlog::info("Configuring camera {}", SN_ordered);
        CameraPtr pCam = this->camList.GetBySerial(SN_ordered);
        pCam->Init();
        INodeMap &nodeMap = pCam->GetNodeMap();

        this->ConfigureCommon(pCam, nodeMap);

        if (SN_ordered == this->MasterCamSN)
        {
            this->ConfigureMaster(nodeMap);
        }
        else
        {
            this->ConfigureSlave(nodeMap);
        }
    }
    this->StartAcquisition();
    // Crucial sleep, fails otherwise
    //std::this_thread::sleep_for(std::chrono::seconds(2));
    spdlog::info("Starting Camera Acquisition");
    return true;
}

void FlirCameraHandler::StartAcquisition(void)
{
    for (auto &imgHandler : this->imageEventHandlers)
    {
        auto sn = imgHandler->SN;
        CameraPtr pCam = this->camList.GetBySerial(sn);
            pCam->BeginAcquisition();
            pCam->TimestampLatch.Execute();
            imgHandler->setOffset(pCam->Timestamp.GetValue());
    }
}

void FlirCameraHandler::StopAcquisition(void)
{
    this->system = System::GetInstance();
    this->camList = this->system->GetCameras();
    for (uint16_t i = 0; i < this->camList.GetSize(); i++)
    {
        // Select camera
        CameraPtr pCam = this->camList.GetByIndex(i);
        INodeMap &devNodeMap = pCam->GetTLDeviceNodeMap();
        CStringPtr SN = devNodeMap.GetNode("DeviceSerialNumber");
        pCam->EndAcquisition();
    }
}

void FlirCameraHandler::Start(void)
{
    for (auto& eventHandler : this->imageEventHandlers)
    {
        eventHandler->Start();
    }
}

void FlirCameraHandler::Stop(void)
{
    for (auto& eventHandler : this->imageEventHandlers)
    {
        eventHandler->Stop();
    }
}

// TODO ifdef fix_system_camera_size -> use array, else vector
// bool FlirCameraHandler::Get(std::vector<Frame> &frame)
bool FlirCameraHandler::Get(std::array<Frame, GLOBAL_CONST_NCAMS> &frame)
{
    bool result = true;
    bool detectedDroped = false;

    // If any Camera has no images at the moment return
    for (auto &img_handler : this->imageEventHandlers)
    {
        if (img_handler->IsFIFOEmpty())
        {
            return false;
        }
    }

    // Dont know, seems to do the same as above?
    for (std::size_t i = 0; i<frame.size(); i++)
    {
        Frame frame_one_cam;
        if (!this->imageEventHandlers.at(i)->Get(frame.at(i)))
        {
            result = false;
        }
    }

    // Sync camera streams, if unsynced drop frames for the cam with older timestamps to get back to the one with a frame drop.
    // do not return any images if we detected a frame drop
    // We need to do this with camera timestamps as the system timestamps vary between cams due to usb bus
    if (result)
    {
        // find the newest timestamp
        std::array<double, GLOBAL_CONST_NCAMS> ts_tests;
        for (std::size_t j = 0; j<this->imageEventHandlers.size(); j++)
        {
            int64_t ti = frame.at(j).imgTimestamp; 
	        ts_tests[j] = ti;
        }
	
        uint64_t tmax = *std::max_element(ts_tests.begin(), ts_tests.end());

        // check for 20% drift against the desired framerate
        uint64_t error_margin = 1/this->CamSettings.fps * 1e9 * 3 / 10;

        // drop a frame to align cams to newest timestamp
        for (std::size_t j = 0; j<this->imageEventHandlers.size(); j++){
            int64_t ti = frame.at(j).imgTimestamp; 
            if ((tmax-ti) > error_margin){
                size_t num2drop = ceil( (double) (tmax - ti) / (1/this->CamSettings.fps *1e9));
                spdlog::warn("Out of sync:  Difference against newest for {} : {} - {} = {} ({}), {} drop {}", 
                    this->imageEventHandlers.at(j)->SN ,
                    tmax*1e-6, 
                    ti*1e-6, 
                    (tmax-ti)*1e-6, 
                    error_margin*1e-6, 
                    (double) (tmax - ti) / (1/this->CamSettings.fps *1e9), 
                    num2drop);

                for(size_t k=0; k<num2drop; k++){
                    this->imageEventHandlers.at(j)->Pop();
                }
                detectedDroped = true;
            }
        }
        
        if (detectedDroped) {
            return false;
        }


        for (auto &img_handler : this->imageEventHandlers)
        {
            img_handler->Pop();
        }
    }

    return result;
}

bool FlirCameraHandler::IsFIFOEmpty(void)
{
    bool result = true;

    for (auto& Cam : imageEventHandlers)
    {
        result &= Cam->IsFIFOEmpty();
    }

    return result;
}

void FlirCameraHandler::change_exposure_test()
{
    static uint16_t i = 0;
    for (auto &SN_ordered : this->SNs)
    {
        CameraPtr pCam = this->camList.GetBySerial(SN_ordered);
        INodeMap &nodeMap = pCam->GetNodeMap();
        this->SetFloatType(nodeMap, "ExposureTime", this->CamSettings.exposure_time + i++ * 100);
    }
}

void FlirCameraHandler::set_exposure(const int exposure_time)
{
    for (auto &SN_ordered : this->SNs)
    {
        CameraPtr pCam = this->camList.GetBySerial(SN_ordered);
        INodeMap &nodeMap = pCam->GetNodeMap();
        this->SetFloatType(nodeMap, "ExposureTime", exposure_time);
    }
}

void FlirCameraHandler::set_size(const int width, const int height, const int binningvertival)
{
    for (auto &SN_ordered : this->SNs)
    {
        CameraPtr pCam = this->camList.GetBySerial(SN_ordered);
        INodeMap &nodeMap = pCam->GetNodeMap();
        this->SetFloatType(nodeMap, "Width", width);
        this->SetFloatType(nodeMap, "Height", height);
        this->SetFloatType(nodeMap, "BinningVertical", binningvertival);
    }
}

void FlirCameraHandler::set_fps(const int fps)
{
    for (auto &SN_ordered : this->SNs)
    {
        CameraPtr pCam = this->camList.GetBySerial(SN_ordered);
        INodeMap &nodeMap = pCam->GetNodeMap();
        this->SetFloatType(nodeMap, "AcquisitionFrameRate", fps);
    }
}

} // namespace flirmulticamera