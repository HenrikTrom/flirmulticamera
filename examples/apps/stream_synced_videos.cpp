#include <flirmulticamera/FlirCamera.h>
#include <string>
#include <opencv2/imgproc/imgproc.hpp> 
#include <opencv2/highgui/highgui.hpp>
#include <flirmulticamera/videoIO.h>
#include <cpp_utils/clitools.h>

#define SCREEN_WIDTH 3840
#define SCREEN_HEIGHT 2160

using namespace flirmulticamera;


int main(int argc, char **argv)
{

    CameraSettings settings;
    if (argc == 2)
    {
        std::string config_file = std::string(argv[1]);
        load_camera_settings(config_file, settings);
    } else {
        spdlog::error("You need to pass a config file!");
        return -1;
    }

    // init camera object
    FlirCameraHandler fcamhandler(
        settings
    );
    if (!fcamhandler.Configure())
    {
        spdlog::error("Could not configure FlirCameraHandler...");
        return -1;
    }


    std::array<std::string, GLOBAL_CONST_NCAMS> windowNames;
    std::array<Frame, GLOBAL_CONST_NCAMS> imgs;

    // When using window thread waitkey does not work
    //cv::startWindowThread();
    const int windowHeight = SCREEN_HEIGHT / GLOBAL_CONST_NCAMS;
    int windowWidth = settings.width;
    if (SCREEN_WIDTH - settings.width > SCREEN_WIDTH/3){
        windowWidth = SCREEN_WIDTH/3;
    }

    for (int cidx = 0; cidx<GLOBAL_CONST_NCAMS; cidx++)
    {
        windowNames.at(cidx) = GLOBAL_CONST_CAMERA_SERIAL_NUMBERS.at(cidx);
        spdlog::info("Created window: {}", windowNames.at(cidx));
        // Create resizable window
        cv::namedWindow(windowNames.at(cidx), cv::WINDOW_NORMAL);
        // Move windows
        cv::moveWindow(windowNames.at(cidx), SCREEN_WIDTH - windowWidth, cidx * windowHeight);
        // Optional: Resize window if needed
        cv::resizeWindow(windowNames.at(cidx), windowWidth, windowHeight);

    }

    fcamhandler.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    spdlog::info("Hit 'q' to stop streaming.");
    while (cv::waitKey(1) != 113) {
       

        // Display the frames
        if (fcamhandler.Get(imgs))
        {
            // i++;
            // progress_bar.update(i);
            for (int cidx = 0; cidx<GLOBAL_CONST_NCAMS; cidx++)
            {
                auto frame = imgs.at(cidx);
                cv::Mat cvImg;
                if (settings.pixel_format == "RGB8"){
                    cvImg = cv::Mat(frame.frameData->GetHeight(), frame.frameData->GetWidth(), CV_8UC3, frame.frameData->GetData());
                    cv::cvtColor(cvImg, cvImg, cv::COLOR_RGB2BGR);
                }
                else if (settings.pixel_format == "BayerGB8")
                {
                    cvImg = cv::Mat(frame.frameData->GetHeight(), frame.frameData->GetWidth(), CV_8UC1, frame.frameData->GetData());
                    cv::cvtColor(cvImg, cvImg, cv::COLOR_BayerGB2RGB);
                }
                // else if (settings.pixel_format == "YCbCr422_8_CbYCrY"){
                //     cvImg = cv::Mat(frame.frameData->GetHeight(), frame.frameData->GetWidth(), CV_8UC3, frame.frameData->GetData());
                //     cv::cvtColor(cvImg, cvImg, cv::COLOR_YCrCb2BGR);
                // }
                else {
                    spdlog::error("No conversion defined for this pixel format.");
                    exit(EXIT_FAILURE);
                }
                
    
                cv::imshow(windowNames.at(cidx), cvImg);
            }
            
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }


    fcamhandler.Stop();

    spdlog::info("Goodbye");
    cv::destroyAllWindows();
    return 0;

}