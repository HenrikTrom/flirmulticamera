#pragma once

#include <string>
#include "cpp_utils/jsontools.h"
#include <flirmulticamera/config.h>
#include <spdlog/spdlog.h>

namespace flirmulticamera {

constexpr int DOC_BUFFER = 65536;

struct CameraSettings
{
    double fps = 0.;
    int width = 0;
    int height = 0;
    std::string video_mode = "ErrorNotLoadedVideoMode";
    int binning_vertical = 0;
    std::string pixel_format = "ErrorNotLoadedPixelFormat";
    std::string master_serial = "ErrorNotLoadedMasterSerial";
    std::string master_line = "ErrorNotLoadedMasterLine";
    std::string slave_line = "ErrorNotLoadedSlaveLine";
    std::vector<std::string> SNs;
    std::vector<double> black_levels, gains, exposure_times;
    std::size_t master_cam_idx = 0;
    std::string save_dir = "";
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
