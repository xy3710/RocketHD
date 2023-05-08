#include <fcntl.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "../lib/wifibroadcast/src/HelperSources/SchedulingHelper.hpp"
#include "../lib/wifibroadcast/src/WBTransmitter.h"
#include "gstreamerstream.hpp"

int main(int argc, char *const *argv) {
  int opt;
  TOptions options{};

  RadiotapHeader::UserSelectableParams wifiParams{20, false, 0, false, 3};
  SchedulingHelper::setThreadParamsMaxRealtime();

  try {
    options.wlan = "usb-ac56-1";
    options.use_block_queue = true;
    options.radio_port = 60;
    options.tx_fec_options.overhead_percentage = 50;
    options.tx_fec_options.fixed_k = 0;
    std::shared_ptr<WBLink> wb_link  = std::make_shared<WBLink>(wifiParams, options);
    GStreamerStream gstreamerstream = GStreamerStream(wb_link);
    gstreamerstream.setup();
    gstreamerstream.start();
    while (true){
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::cout << wb_link->createDebug() << std::endl;
      std::cout << gstreamerstream.createDebug() << std::endl;
    }
  } catch (std::runtime_error &e) {
    fprintf(stderr, "Error: %s\n", e.what());
    exit(1);
  }
  return 0;
}
