#ifndef STREAMS_H
#define STREAMS_H

#include <array>
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include "../lib/wifibroadcast/src/UdpWBReceiver.hpp"
#include "../lib/wifibroadcast/src/UdpWBTransmitter.hpp"

/**
 * This class takes a list of cards supporting monitor mode (only 1 card on air) and
 * is responsible for configuring the given cards and then setting up all the Wifi-broadcast streams needed for OpenHD.
 * In the end, we have a link that has some broadcast characteristics for video (video is always broadcast from air to ground)
 * but also a bidirectional link (without re-transmission(s)) for telemetry.
 * This class assumes a corresponding instance on the air or ground unit, respective.
 */
class WBLink{
 public:
  /**
   * @param broadcast_cards list of discovered wifi card(s) that support monitor mode & are injection capable. Needs to be at least
   * one card, and only one card on an air unit. The given cards need to support monitor mode and either 2.4G or 5G wifi.
   * In the case where there are multiple card(s), the first given card is used for transmission & receive, the other card(s) are not used
   * for transmission, only for receiving.
   * @param opt_action_handler global openhd action handler, optional (can be nullptr during testing of specific modules instead
   * of testing a complete running openhd instance)
   */
  WBLink(RadiotapHeader::UserSelectableParams radioTapHeaderParams, TOptions options);
  WBLink(const WBLink&)=delete;
  WBLink(const WBLink&&)=delete;
  ~WBLink();
  // Verbose string about the current state.
  [[nodiscard]] std::string createDebug()const;
 private:
  bool set_tx_power_rtl8812au(int tx_power_index_override);
  // set the tx power of all wifibroadcast cards. For rtl8812au, uses the tx power index
  // for other cards, uses the mW value
  void apply_txpower();
  // change the MCS index (only supported by rtl8812au)
  // guaranteed to return immediately (Doesn't need iw or something similar)
  // If the hw supports changing the mcs index, and the mcs index is valid, apply it and return true
  // Leave untouched and return false otherwise.
  bool set_mcs_index(int mcs_index);
  // These do not "break" the bidirectional connectivity and therefore
  // can be changed easily on the fly
  bool set_video_fec_block_length(int block_length);
  bool set_video_fec_percentage(int fec_percentage);
  bool set_enable_wb_video_variable_bitrate(int value);
  bool set_max_fec_block_size_for_platform(int value);
  // Make sure no processes interfering with monitor mode run on the given cards,
  // then sets them to monitor mode
  void takeover_cards_monitor_mode();
  // set the right frequency, channel width and tx power. Cards need to be in monitor mode already !
  void configure_cards();
  // start telemetry and video rx/tx stream(s)
  void configure_telemetry();
  void configure_video();
  std::unique_ptr<WBTransmitter> create_wb_tx();
 public:
  // Called by the camera stream on the air unit only
  // transmit video data via wifibradcast
  void transmit_video_data(std::vector<std::shared_ptr<std::vector<uint8_t>>> frame_fragments);
 private:
  RadiotapHeader::UserSelectableParams m_radioTapHeaderParams;
  const TOptions m_options;
  std::shared_ptr<spdlog::logger> m_console;
  // For video, on air there are only tx instances, on ground there are only rx instances.
  std::unique_ptr<WBTransmitter> m_wb_video_tx;
  std::string m_device_name;
};

#endif
