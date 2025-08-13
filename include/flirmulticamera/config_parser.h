#pragma once

#include <string>
#include <cpp_utils/jsontools.h>
#include <flirmulticamera/hardware_constants.h>
#include <spdlog/spdlog.h>

namespace flirmulticamera {

constexpr int DOC_BUFFER = 65536;

struct CameraSettings
{
    int width = 0;
    int height = 0;
    int offsetX = 0;
    int offsetY = 0;
    int binning_vertical = 0;
    std::string pixel_format = "ErrorNotLoadedPixelFormat";
    std::string video_mode = "ErrorNotLoadedVideoMode";
    double fps = 0.;
    double black_level = 0.;
    double gain = 0.;
    double exposure_time = 0.;
    std::string save_dir = "";
    std::string codec = "";
};

/**
 * Loads settings .json as rapidjson document and checks it against schema
 *
 * @param [in] type identifer of settings doc (Stream, Record, ...)
 * @param [out] settings Settings object to store data
 * \return true if settings could get loaded, false otherwise.
 */
bool load_camera_settings(const std::string& settings_path, CameraSettings& settings);

} // namespace flirmulticamera
