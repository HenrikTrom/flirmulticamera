#include "flirmulticamera/config_parser.h"


namespace flirmulticamera {

bool load_camera_settings(const std::string& settings_path, CameraSettings& settings) {
    spdlog::info("Loading camera settings from {}", settings_path);
    Document settings_doc;
    const bool success = cpp_utils::load_json_with_schema(
        settings_path,
        std::string(CONFIG_DIR)+"/CameraSettings.Schema.json",
        DOC_BUFFER, settings_doc
    );
    // global settings
    settings.fps = settings_doc["fps"].GetDouble();
    settings.height = settings_doc["img_height"].GetInt();
    settings.width = settings_doc["img_width"].GetInt();
    settings.video_mode = settings_doc["video_mode"].GetString();
    settings.binning_vertical = settings_doc["binning_vertical"].GetDouble();
    settings.pixel_format = settings_doc["pixel_format"].GetString();
    settings.master_serial = settings_doc["master_serial"].GetString();
    settings.master_line = settings_doc["master_line"].GetString();
    settings.slave_line = settings_doc["slave_line"].GetString();
    settings.codec = settings_doc["codec"].GetString();
    // local settings
    std::size_t count = 0;
    std::string SNs_, black_levels_, gains_, exposure_times_, offsets_x_, offsets_y_;
    for (const auto &cam : settings_doc["cams"].GetArray()) {
        std::string serial = cam["serial"].GetString();
        settings.SNs.push_back(serial);
        double bl = cam["black_level"].GetDouble();
        settings.black_levels.push_back(bl);
        double g = cam["gain"].GetDouble();
        settings.gains.push_back(g);
        double et = cam["exposure_time"].GetDouble();
        settings.exposure_times.push_back(et);
        int ox = cam["img_offsetX"].GetDouble();
        settings.offsets_x.push_back(ox);
        int oy = cam["img_offsetY"].GetDouble();
        settings.offsets_y.push_back(oy);
        if (serial == settings.master_serial) {
            settings.master_cam_idx = count;
        }
        count ++;
        SNs_ += serial+std::string(" ");
        black_levels_ += std::to_string(bl)+std::string(" ");
        gains_ += std::to_string(g)+std::string(" ");
        exposure_times_ += std::to_string(et)+std::string(" ");
        offsets_x_ += std::to_string(ox)+std::string(" ");
        offsets_y_ += std::to_string(oy)+std::string(" ");
    }
    settings.save_dir = settings_doc["save_dir"].GetString();

    // Logging Settings
    std::string msg = std::string("Camera settings:") +
        std::string("\n\t- fps: {}\n\t- width: {}\n\t- height: {}\n\t- video_mode: {}") +
        std::string("\n\t- binning_vertical: {}\n\t- pixel_format: {}") +
        std::string("\n\t- master_serial: {}\n\t- master_line: {}\n\t- slave_line: {}") +
        std::string("\n\t- serials: {}\n\t- black_level: {}\n\t- gains: {}\n\t- exposure_time: {}") +
        std::string("\n\t- offsets_x: {}\n\t- offsets_y: {}");
    spdlog::info(msg,
        settings.fps, 
        settings.width, 
        settings.height, 
        settings.video_mode, 
        settings.binning_vertical, 
        settings.pixel_format, 
        settings.master_serial, 
        settings.master_line,
        settings.slave_line, 
        SNs_, 
        black_levels_, 
        gains_, 
        exposure_times_,
        offsets_x_,
        offsets_y_
    );
    // set master cam idx
    for (std::size_t cidx = 0; cidx<settings.SNs.size(); cidx++) {
        if (settings.SNs.at(cidx) == settings.master_serial) {
            settings.master_cam_idx = cidx;
        }
    } 

    #ifdef ENV_DEFINED_CAMERA_COUNT
    if (GLOBAL_CONST_NCAMS != settings.SNs.size()) {
        std::string msg = "Environment Variable FLIR_CAMERA_COUNT ({}) != number of cameras set in {}";
        spdlog::error(msg, GLOBAL_CONST_NCAMS, settings_path);
        throw std::runtime_error("Settings missmatch");
    }
    #endif
    return success;
}

} // namespace flirmulticamera