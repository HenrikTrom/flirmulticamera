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
    // this->PreviousTimestamp = now;
    this->imgPreviousTimestamp = 0;
    pCam->TimestampLatch.Execute();
    this->offsetTimestamp = pCam->Timestamp.GetValue();
    this->T = T;
    this->MarginOfError = T;
    this->firstFrameFlag = true;
    this->pCam = pCam; 
}

FLirCameraImageEventHandler::~FLirCameraImageEventHandler()
{
    
}

void FLirCameraImageEventHandler::OnImageEvent(ImagePtr img)
{   
    // Check image retrieval status
    if (img->IsIncomplete()) {
        // TODO
        cout << "Image incomplete with image status " << img->GetImageStatus() << "..." << endl;
    }
    else {
        this->last_ts = img->GetTimeStamp();
        if (this->BufferingFlag) {
            // convert and push into FIFO
            Frame frame;
            clock_gettime(CLOCK_MONOTONIC, &frame.Timestamp);
            frame.imgTimestamp = (img->GetTimeStamp() + this->offsetTimestamp);
            frame.FrameCounter = this->FrameCounter++;
            if (this->FrameCounter % 3000 == 0) {
                this->setOffset();
            }
            // validate the timestamp to see if there is a out of sync
            if (this->firstFrameFlag) {
                this->firstFrameFlag = false;
                this->imgPreviousTimestamp = frame.imgTimestamp - this->T;
            }
            else {
                // T  = 1/framerate
                const double errorMargin = this->T * 0.2; // 20% of frame rate
		        double timediff =  (frame.imgTimestamp - this->imgPreviousTimestamp) * 1e-9 - this->T;
                if (timediff > errorMargin) {
                    spdlog::warn(
                        "Missed a frame; FNo,SN,imTs: {} | {} | {} ms", 
                        frame.FrameCounter, this->SN, timediff*1e3
                    );
                }
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
    // this->PreviousTimestamp = now;
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

void FLirCameraImageEventHandler::setOffset() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    this->pCam->TimestampLatch.Execute();
    this->offsetTimestamp = (ts.tv_nsec + (int64_t) ts.tv_sec * 1e9) - pCam->Timestamp.GetValue();;
}

FlirCameraHandler::FlirCameraHandler(CameraSettings CamSettings) : CamSettings(CamSettings)
{
    this->MasterCamSN = this->CamSettings.SNs.at(this->CamSettings.master_cam_idx);
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

void FlirCameraHandler::ConfigureCommon(CameraPtr pCam, INodeMap &nodeMap, const std::size_t &idx)
{
    this->SetEnumerationType(nodeMap, "UserSetSelector", "UserSet2");
    this->SetCommand(nodeMap, "UserSetLoad");
    this->SetEnumerationType(nodeMap, "PixelFormat",  this->CamSettings.pixel_format);
    this->SetEnumerationType(nodeMap, "VideoMode", this->CamSettings.video_mode);
    // Can only access Binning if not in Mode0
    if (this->CamSettings.video_mode != "Mode0") {
        this->SetEnumerationType(nodeMap, "BinningControl", "Average");
        this->SetIntType(nodeMap, "BinningVertical", this->CamSettings.binning_vertical);
    }
    this->SetIntType(nodeMap, "OffsetX", this->CamSettings.offsets_x.at(idx));
    this->SetIntType(nodeMap, "OffsetY", this->CamSettings.offsets_y.at(idx));
    this->SetIntType(nodeMap, "Width", this->CamSettings.width);
    this->SetIntType(nodeMap, "Height", this->CamSettings.height);

    this->SetBooleanType(nodeMap, "BlackLevelClampingEnable", true);
    this->SetFloatType(nodeMap, "BlackLevel", this->CamSettings.black_levels.at(idx));

    this->SetEnumerationType(nodeMap, "GainAuto", "Off");
    this->SetFloatType(nodeMap, "Gain", this->CamSettings.gains.at(idx));

    this->SetEnumerationType(nodeMap, "ExposureMode", "Timed");
    this->SetEnumerationType(nodeMap, "ExposureAuto", "Off");
    this->SetEnumerationType(nodeMap, "BalanceWhiteAuto", "Off");
    this->SetFloatType(nodeMap, "ExposureTime", this->CamSettings.exposure_times.at(idx));

    this->SetBooleanType(nodeMap, "ChunkModeActive", false);
    // this->SetBooleanType(nodeMap, "ChunkModeActive", true);
    // this->SetEnumerationType(nodeMap, "ChunkSelector", "Timestamp");
    // this->SetBooleanType(nodeMap, "ChunkEnable", true);

    FLirCameraImageEventHandler *imgEventHandlerTmp = new FLirCameraImageEventHandler{pCam, 1.0 / this->CamSettings.fps};
    this->imageEventHandlers.push_back(imgEventHandlerTmp);
    pCam->RegisterEventHandler(*this->imageEventHandlers[this->imageEventHandlers.size() - 1]);
}

void FlirCameraHandler::ConfigureMaster(INodeMap &nodeMap)
{
    // GPIO& trigger
    this->SetEnumerationType(nodeMap, "LineSelector", this->CamSettings.master_line);
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
    this->SetEnumerationType(nodeMap, "LineSelector", this->CamSettings.slave_line);

    this->SetEnumerationType(nodeMap, "TriggerSelector", "FrameStart");
    this->SetEnumerationType(nodeMap, "TriggerMode", "On");
    this->SetEnumerationType(nodeMap, "TriggerSource", this->CamSettings.slave_line);
    this->SetEnumerationType(nodeMap, "TriggerActivation", "FallingEdge");
    this->SetEnumerationType(nodeMap, "TriggerOverlap", "ReadOut");
}

bool FlirCameraHandler::Configure(void)
{
    this->system = System::GetInstance();
    this->camList = this->system->GetCameras();
    if (this->camList.GetSize() < this->CamSettings.SNs.size())
    {
        spdlog::error(
            "Number of cameras detected ({}) < Number of cameras configured ({})",
            this->camList.GetSize(),
            this->CamSettings.SNs.size()
        );
        return false;
    }
    else{
        spdlog::info(
            "Detected cameras ({}) >= configuration: ({})", 
            this->camList.GetSize(),
            this->CamSettings.SNs.size()
        );
    }
    if (this->camList.GetSize() == 0)
    {
        cout << "\tNo devices detected." << endl
                << endl;
        return false;
    }

    for (std::size_t cidx = 0; cidx<this->CamSettings.SNs.size(); cidx++) {
        spdlog::info("Configuring camera {}", this->CamSettings.SNs.at(cidx));
        CameraPtr pCam = this->camList.GetBySerial(this->CamSettings.SNs.at(cidx));
        pCam->Init();
        INodeMap &nodeMap = pCam->GetNodeMap();

        this->ConfigureCommon(pCam, nodeMap, cidx);

        if (this->CamSettings.SNs.at(cidx) == this->MasterCamSN)
        {
            this->ConfigureMaster(nodeMap);
        }
        else
        {
            this->ConfigureSlave(nodeMap);
        }
    }
    this->StartAcquisition();
    // Crucial sleep to stabilize synchronized capture, fails otherwise
    std::this_thread::sleep_for(std::chrono::seconds(2));
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
        imgHandler->setOffset();
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

#ifdef ENV_DEFINED_CAMERA_COUNT
bool FlirCameraHandler::Get(std::array<Frame, GLOBAL_CONST_NCAMS> &frame)
{ 
    bool result = true;
    bool detectedDroped = false;

    // Make sure all cams have at least one image
    for (std::size_t i = 0; i<this->imageEventHandlers.size(); i++)
    {
        if (!this->imageEventHandlers.at(i)->Get(frame.at(i))) {
            return false;
        }
    }

    // Sync camera streams, if unsynced drop frames for the cam with older timestamps to get back to the one with a frame drop.
    // do not return any images if we detected a frame drop
    // We need to do this with camera timestamps as the system timestamps vary between cams due to usb bus
    // find the newest timestamp
    std::array<double, GLOBAL_CONST_NCAMS> ts_tests;
    for (std::size_t j = 0; j<this->imageEventHandlers.size(); j++) {
        int64_t ti = frame.at(j).imgTimestamp; 
        ts_tests.at(j) = ti;
    }

    int64_t tmax = (int64_t) *std::max_element(ts_tests.begin(), ts_tests.end());

    // check for 20% drift against the desired framerate
    int64_t error_margin = 1/this->CamSettings.fps * 1e9 * 3 / 10;

    // drop a frame to align cams to newest timestamp
    for (std::size_t j = 0; j<this->imageEventHandlers.size(); j++){
        int64_t &ti = frame.at(j).imgTimestamp; 
        if ( (tmax-ti) > error_margin ){
            spdlog::warn("Out of sync:  Difference against newest for {} : {} ms - {} ms = {} ms ({}), {} drop 1", 
                this->imageEventHandlers.at(j)->SN ,
                tmax*1e-6, 
                ti*1e-6, 
                (tmax-ti)*1e-6, 
                error_margin*1e-6, 
                (double) (tmax - ti) / (1/this->CamSettings.fps *1e9)
            );

            this->imageEventHandlers.at(j)->Pop();

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

    return result;
}
#endif

bool FlirCameraHandler::Get(std::vector<Frame> &frame)
{
    frame.clear();
    bool result = true;
    bool detectedDroped = false;

    // Make sure all cams have at least one image
    for (std::size_t i = 0; i<this->imageEventHandlers.size(); i++)
    {
        Frame frame_one_cam;
        if (!this->imageEventHandlers.at(i)->Get(frame_one_cam)) {
            return false;
        }
        else {
            frame.push_back(frame_one_cam);
        }
    }

    // Sync camera streams, if unsynced drop frames for the cam with older timestamps to get back to the one with a frame drop.
    // do not return any images if we detected a frame drop
    // We need to do this with camera timestamps as the system timestamps vary between cams due to usb bus
    // find the newest timestamp
    std::array<double, GLOBAL_CONST_NCAMS> ts_tests;
    for (std::size_t j = 0; j<this->imageEventHandlers.size(); j++) {
        int64_t ti = frame.at(j).imgTimestamp; 
        ts_tests.at(j) = ti;
    }

    int64_t tmax = (int64_t) *std::max_element(ts_tests.begin(), ts_tests.end());

    // check for 20% drift against the desired framerate
    int64_t error_margin = 1/this->CamSettings.fps * 1e9 * 3 / 10;

    // drop a frame to align cams to newest timestamp
    for (std::size_t j = 0; j<this->imageEventHandlers.size(); j++){
        int64_t &ti = frame.at(j).imgTimestamp; 
        if ( (tmax-ti) > error_margin ){
            spdlog::warn("Out of sync:  Difference against newest for {} : {} ms - {} ms = {} ms ({}), {} drop 1", 
                this->imageEventHandlers.at(j)->SN ,
                tmax*1e-6, 
                ti*1e-6, 
                (tmax-ti)*1e-6, 
                error_margin*1e-6, 
                (double) (tmax - ti) / (1/this->CamSettings.fps *1e9)
            );

            this->imageEventHandlers.at(j)->Pop();

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
    for (std::size_t cidx = 0; cidx<this->CamSettings.SNs.size(); cidx++)
    {
        CameraPtr pCam = this->camList.GetBySerial(this->CamSettings.SNs.at(cidx));
        INodeMap &nodeMap = pCam->GetNodeMap();
        this->SetFloatType(nodeMap, "ExposureTime", this->CamSettings.exposure_times.at(cidx) + i++ * 100);
    }
}

void FlirCameraHandler::set_exposure(const int exposure_time)
{
    for (auto &SN : this->CamSettings.SNs)
    {
        CameraPtr pCam = this->camList.GetBySerial(SN);
        INodeMap &nodeMap = pCam->GetNodeMap();
        this->SetFloatType(nodeMap, "ExposureTime", exposure_time);
    }
}

void FlirCameraHandler::set_size(const int width, const int height, const int binningvertival)
{
    for (auto &SN : this->CamSettings.SNs)
    {
        CameraPtr pCam = this->camList.GetBySerial(SN);
        INodeMap &nodeMap = pCam->GetNodeMap();
        this->SetFloatType(nodeMap, "Width", width);
        this->SetFloatType(nodeMap, "Height", height);
        this->SetFloatType(nodeMap, "BinningVertical", binningvertival);
    }
}

void FlirCameraHandler::set_fps(const int fps)
{
    for (auto &SN : this->CamSettings.SNs)
    {
        CameraPtr pCam = this->camList.GetBySerial(SN);
        INodeMap &nodeMap = pCam->GetNodeMap();
        this->SetFloatType(nodeMap, "AcquisitionFrameRate", fps);
    }
}

} // namespace flirmulticamera