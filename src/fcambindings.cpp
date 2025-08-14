#include "flirmulticamera/fcambindings.h"
#include <opencv2/imgproc.hpp>

namespace flirmulticamera{

FlirCameraWrapper::FlirCameraWrapper(const std::string& config_file)
{
    load_camera_settings(config_file, settings_);

    this->fcamhandler_.reset(new FlirCameraHandler(settings_));
    if (!this->fcamhandler_->Configure())
    {
        throw std::runtime_error("Could not configure FlirCameraHandler...");
    }
}

void FlirCameraWrapper::start() {
    this->fcamhandler_->Start();
}

void FlirCameraWrapper::stop() {
    this->fcamhandler_->Stop();
}

std::vector<cv::Mat> FlirCameraWrapper::get_images() {
    std::vector<flirmulticamera::Frame> imgs;
    std::vector<cv::Mat> result;
    while (true){
        if (this->fcamhandler_->Get(imgs)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (auto& frame : imgs) {
        if (frame.frameData && !frame.frameData->IsIncomplete()) {
            cv::Mat img(frame.frameData->GetHeight(), frame.frameData->GetWidth(), CV_8UC3, frame.frameData->GetData());
            cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
            result.push_back(img);
        }
    }
    return result;
}

} // namespace flirmulticamera