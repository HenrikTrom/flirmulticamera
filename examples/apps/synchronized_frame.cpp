#include <flirmulticamera/FlirCamera.h>
#include <string>
#include <opencv2/imgproc/imgproc.hpp> 
#include<opencv2/highgui/highgui.hpp>

using namespace flirmulticamera;

int main(int argc, char **argv)
{
    // load camera settings
    CameraSettings settings;
    if (argc == 2 )
    {
        std::string config_file = std::string(argv[1]);
        spdlog::info("Loading camera settings from {}", config_file);
        load_camera_settings(config_file, settings);
    }
    else
    {
        std::string config_file = std::string(CONFIG_DIR)+"/1024x768.json";
        spdlog::info("Loading camera settings from default: {}", config_file);
        load_camera_settings(config_file, settings);
    }
    if (settings.save_dir == ""){
        settings.save_dir=std::string(CONFIG_DIR)+"/../outputs";
    }

    // Overwrite the passed settings to work with opencv down below
    settings.pixel_format = "RGB8";
    // Only one frame -> 1fps
    settings.fps = 1;

    // init camera object
    FlirCameraHandler fcamhandler(
        settings
    );
    if (!fcamhandler.Configure())
    {
        spdlog::error("Could not configure FlirCameraHandler...");
        return 1;
    }

    fcamhandler.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    std::array<Frame, GLOBAL_CONST_NCAMS> imgs;

    size_t failed_iterations = 0;
    constexpr size_t max_faild_iterations = 1000;
    while(true)
    {
        if (fcamhandler.Get(imgs))
        {
            spdlog::info("Got images");
            break;
        }
        else
        {
            failed_iterations++;
            if (failed_iterations > max_faild_iterations)
            {
                spdlog::error("Max number of failed iterations reached: {}", max_faild_iterations);
                return 1;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    fcamhandler.Stop();
    
    bool success = true;
    for (uint8_t cidx = 0; cidx < imgs.size(); cidx++)
    {
        Frame &img = imgs.at(cidx);
        if (img.frameData == nullptr)
        {
            spdlog::error("Image is null...");
            success = false;
            continue;
        }
        {
            if (img.frameData->IsIncomplete())
            {
                spdlog::error("Image incomplete with image status {}...", img.frameData->GetImageStatus());
                success = false;
                continue;
            }
            else
            {
                cv::Mat cvImg(img.frameData->GetHeight(), img.frameData->GetWidth(), CV_8UC3, img.frameData->GetData());
                cv::cvtColor(cvImg, cvImg, cv::COLOR_RGB2BGR);
                std::string filename = settings.save_dir+"/"+std::string(GLOBAL_CONST_CAMERA_SERIAL_NUMBERS.at(cidx)) + ".jpg";
                cv::imwrite(filename, cvImg);
                spdlog::info("Saved image {}", filename);
            }
        }

        if (!success)
        {
            spdlog::error("Failed to run synchronized frame example");
            return 1;
        }
    }

    spdlog::info("Successfully ran synchronized frame example");

    return 0;
}



