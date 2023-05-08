//
// Created by consti10 on 13.07.22.
//

#include "wifi_command_helper.hpp"

#include "../lib/wifibroadcast/src/wifibroadcast-spdlog.h"

#include <sstream>

static std::shared_ptr<spdlog::logger> get_logger(){
  return wifibroadcast::log::create_or_get("w_helper");
}


std::string wifi::commandhelper::create_command_with_args(
    const std::string& command, const std::vector<std::string>& args) {
  std::stringstream ss;
  ss << command;
  for (const auto& arg : args) {
    ss << " " << arg;
  }
  return ss.str();
}

int wifi::commandhelper::run_command(const std::string& command,
                         const std::vector<std::string>& args,
                         bool print_debug) {
  const auto command_with_args = create_command_with_args(command, args);
  if (print_debug) {
    get_logger()->debug("run command begin [{}]",
                                      command_with_args);
  }
  // https://man7.org/linux/man-pages/man3/system.3.html
  const auto ret = std::system(command_with_args.c_str());
  // openhd::log::get_default()->debug("return code:{}",ret);
  if (ret < 0) {
    get_logger()->warn("Invalid command, return code {}", ret);
  }
  return ret;
}


bool wifi::commandhelper::rfkill_unblock_all() {
  get_logger()->info("rfkill_unblock_all");
  std::vector<std::string> args{"unblock","all"};
  bool success=run_command("rfkill",args);
  return success;
}

bool wifi::commandhelper::ip_link_set_card_state(const std::string &device,bool up) {
  get_logger()->info("ip_link_set_card_state {} up {}",device,up);
  std::vector<std::string> args{"link", "set", "dev",device, up ? "up" : "down"};
  bool success = run_command("ip", args);
  return success;
}

bool wifi::commandhelper::iw_enable_monitor_mode(const std::string &device) {
  get_logger()->info("iw_enable_monitor_mode {}",device);
  std::vector<std::string> args{"dev", device, "set", "monitor", "otherbss"};
  bool success = run_command("iw", args);
  success = run_command("airmon-ng",{"check","kill"}) && success;
  return success;
}

static std::string channel_width_as_iw_string(uint32_t channel_width){
  if(channel_width==5){
    return "5MHz";
  }else if(channel_width==10) {
    return "10Mhz";
  }else if(channel_width==20){
    return "HT20";
  }else if(channel_width==40){
    return "HT40+";
  }
  get_logger()->info("Invalid channel width {}, assuming HT20",channel_width);
  return "HT20";
}

bool wifi::commandhelper::iw_set_frequency_and_channel_width(const std::string &device, uint32_t freq_mhz,uint32_t channel_width) {
  const std::string iw_channel_width= channel_width_as_iw_string(channel_width);
  get_logger()->info("iw_set_frequency_and_channel_width {} {}Mhz {}",device,freq_mhz,iw_channel_width);
  std::vector<std::string> args{"dev", device, "set", "freq", std::to_string(freq_mhz), iw_channel_width};
  const auto ret = run_command("iw", args);
  if(ret!=0){
    get_logger()->warn("iw {}Mhz@{}Mhz not supported {}",freq_mhz,channel_width,ret);
    return false;
  }
  return true;
}

bool wifi::commandhelper::iw_set_tx_power(const std::string &device,uint32_t tx_power_mBm) {
  get_logger()->info("iw_set_tx_power {} {} mBm",device,tx_power_mBm);
  std::vector<std::string> args{"dev",device, "set", "txpower", "fixed", std::to_string(tx_power_mBm)};
  const auto ret = run_command("iw", args);
  if(ret!=0){
    get_logger()->warn("iw_set_tx_power failed {}",ret);
    return false;
  }
  return true;
}

bool wifi::commandhelper::iw_set_rate_mcs(const std::string &device,uint32_t mcs_index,bool is_2g) {
  get_logger()->info("iw_set_rate_mcs {} {} mBm",device,mcs_index);
  std::vector<std::string> args{"dev",device, "set", "bitrates",is_2g ? "ht-mcs-2.4":"ht-mcs-5",std::to_string(mcs_index)};
  const auto ret = run_command("iw", args);
  if(ret!=0){
    get_logger()->warn("iw_set_rate_mcs failed {}",ret);
    return false;
  }
  return true;
}

bool wifi::commandhelper::nmcli_set_device_managed_status(const std::string &device,bool managed){
  get_logger()->info("nmcli_set_device_managed_status {} managed:{}",device,managed);
  std::vector<std::string> arguments{"device","set",device,"managed"};
  if(managed){
    arguments.emplace_back("yes");
  }else{
    arguments.emplace_back("no");
  }
  bool success = run_command("nmcli",arguments);
  return success;
}

static std::string float_without_trailing_zeroes(const float value){
  std::stringstream ss;
  ss << std::noshowpoint << value;
  return ss.str();
}
