//
// Created by consti10 on 03.05.22.
//

#ifndef WIFIBROADCAST_UDPWFIBROADCASTWRAPPER_HPP
#define WIFIBROADCAST_UDPWFIBROADCASTWRAPPER_HPP

#include "../lib/wifibroadcast/src/WBTransmitter.h"
#include "../lib/wifibroadcast/src/HelperSources/SocketHelper.hpp"

#include <memory>
#include <thread>
#include <mutex>
#include <utility>
#include <list>

#include "rtp_eof_helper.hpp"

/**
 * Creates a WB Transmitter that gets its input data stream from an UDP Port
 */
class UDPBlockedWBTransmitter {
 public:
  UDPBlockedWBTransmitter(RadiotapHeader::UserSelectableParams radiotapHeaderParams,
                   TOptions options1,
                   const std::string &client_addr,
                   int client_udp_port,
                   std::optional<int> wanted_recv_buff_size=std::nullopt) {
    options1.use_block_queue= true;
    wbTransmitter = std::make_unique<WBTransmitter>(radiotapHeaderParams, std::move(options1));
    udpReceiver = std::make_unique<SocketHelper::UDPReceiver>(client_addr,
        client_udp_port,
        [this](const uint8_t *payload,
               const std::size_t payloadSize) {
          on_new_udp_packet(payload,payloadSize);
        },wanted_recv_buff_size);
  }
  /**
   * Loop until an error occurs.
   * Blocks the calling thread.
   */
  void loopUntilError() {
    udpReceiver->loopUntilError();
  }
  /**
   * Start looping in the background, creates a new thread.
   */
  void runInBackground() {
    udpReceiver->runInBackground();
  }
  void stopBackground(){
    udpReceiver->stopBackground();
  }
  WBTransmitter& get_wb_tx(){
    return *wbTransmitter;
  }
 private:
  std::unique_ptr<WBTransmitter> wbTransmitter;
  std::unique_ptr<SocketHelper::UDPReceiver> udpReceiver;
  std::vector<std::shared_ptr<std::vector<uint8_t>>> frame_fragments;
  void on_new_udp_packet(const uint8_t *payload,const std::size_t payloadSize){
    auto shared=std::make_shared<std::vector<uint8_t>>(payload,payload+payloadSize);
    frame_fragments.push_back(shared);
    if(rtp_eof_helper::h265_end_block(payload,payloadSize)){
      wbTransmitter->try_enqueue_block(frame_fragments, 128);
      frame_fragments.resize(0);
    }
  }
};


#endif //WIFIBROADCAST_UDPWFIBROADCASTWRAPPER_HPP
