#include <flirmulticamera/FlirCamera.h>
#include <string>
#include <opencv2/imgproc/imgproc.hpp> 
#include<opencv2/highgui/highgui.hpp>
#include <flirmulticamera/videoIO.h>
#include <cpp_utils/clitools.h>

using namespace flirmulticamera;

int main(int argc, char **argv)
{
    // load camera settings
    std::size_t n_capture_frames=30;
    CameraSettings settings;
    if (argc == 2 || argc == 3)
    {
        std::string config_file = std::string(argv[1]);
        load_camera_settings(config_file, settings);
        if (argc == 3)
        {
            n_capture_frames = std::stoul(argv[2]);
            spdlog::info("Recording frames: {}", n_capture_frames);
        }
    }
    else
    {
        spdlog::info("Recording default frames: {}", n_capture_frames);
        std::string config_file = std::string(CONFIG_DIR)+"/1024x768.json";
        load_camera_settings(config_file, settings);
    }
    if (settings.save_dir == ""){
        settings.save_dir=std::string(CONFIG_DIR)+"/../outputs";
    }
    // init camera object
    FlirCameraHandler fcamhandler(
        settings
    );
    if (!fcamhandler.Configure())
    {
        spdlog::error("Could not configure FlirCameraHandler...");
        return 1;
    }

    VideoWriter writer{
        static_cast<uint32_t>(settings.width), 
        static_cast<uint32_t>(settings.height), 
        static_cast<float>(settings.fps),
        static_cast<std::string>(settings.codec),
        static_cast<std::string>(settings.pixel_format)
    };
    std::vector<std::string> video_filenames{GLOBAL_CONST_NCAMS};
    
    for (int cidx = 0; cidx<GLOBAL_CONST_NCAMS; cidx++)
    {
        video_filenames.at(cidx) = settings.save_dir+"/"+std::string(GLOBAL_CONST_CAMERA_SERIAL_NUMBERS.at(cidx))+".mp4";
        spdlog::info("Opening file name: {}", video_filenames.at(cidx));
    }
    writer.Open(video_filenames);
    
    std::array<Frame, GLOBAL_CONST_NCAMS> imgs;
    std::size_t framecounter{1};
    fcamhandler.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    cpp_utils::ProgressBar progress_bar(n_capture_frames);
    while(framecounter <= n_capture_frames)
    {
        if (fcamhandler.Get(imgs))
        {
            progress_bar.update(framecounter);
            std::vector<Spinnaker::ImagePtr> buffer{};
            for (auto &frame : imgs)
            {
                buffer.push_back(frame.frameData);
            }
            writer.Write(buffer);
            framecounter++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    writer.Close();
    fcamhandler.Stop();

    spdlog::info("Successfully ran synchronized video example");

    return 0;
}



