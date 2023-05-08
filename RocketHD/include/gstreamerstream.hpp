#ifndef GSTREAMERSTREAM_H
#define GSTREAMERSTREAM_H

#include  <gst/gst.h>

#include <array>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../lib/wifibroadcast/src/WBTransmitter.h"
#include "wb_link.hpp"

// Implementation of OHD CameraStream for pretty much everything, using
// gstreamer.
// NOTE: What we are doing here essentially is creating a big gstreamer pipeline string and then
// executing this pipeline. This makes development easy (since you can just test the pipeline(s) manually
// using gst-launch and add settings and more this way) but you are encouraged to use other approach(es) if they
// better fit your needs (see CameraStream.h)
class GStreamerStream{
 public:
  GStreamerStream(std::shared_ptr<WBLink> wb_link);
  ~GStreamerStream();
  void setup();
 private:
  void stop_cleanup_restart();
  // Utils when settings are changed (most of them require a full restart of the pipeline)
  void restart_after_new_setting();
  void restartIfStopped();
  void initGstreamerOrThrow();
 public:
  std::string createDebug();
  // Set gst state to PLAYING
  void start();
  // Set gst state to PAUSED
  void stop();
  // Set gst state to GST_STATE_NULL and properly cleanup the pipeline.
  void cleanup_pipe();
 private:
  // We cannot create the debug state while performing a restart
  std::mutex m_pipeline_mutex;
  // points to a running gst pipeline instance (unless in stopped & cleaned up state)
  GstElement *m_gst_pipeline = nullptr;
  // The pipeline that is started in the end
  std::stringstream m_pipeline_content;
  // To reduce the time on the param callback(s) - they need to return immediately to not block the param server
  void restart_async();
  std::mutex m_async_thread_mutex;
  std::unique_ptr<std::thread> m_async_thread =nullptr;
  std::shared_ptr<spdlog::logger> m_console;
  std::chrono::steady_clock::time_point m_stream_creation_time=std::chrono::steady_clock::now();
 private:
  // The stuff here is to pull the data out of the gstreamer pipeline, such that we can forward it to the WB link
  void on_new_rtp_frame_fragment(std::shared_ptr<std::vector<uint8_t>> fragment,uint64_t dts);
  std::vector<std::shared_ptr<std::vector<uint8_t>>> m_frame_fragments;
  void on_new_rtp_fragmented_frame(std::vector<std::shared_ptr<std::vector<uint8_t>>> frame_fragments);
  // pull samples (fragments) out of the gstreamer pipeline
  GstElement *m_app_sink_element = nullptr;
  bool m_pull_samples_run=false;
  std::unique_ptr<std::thread> m_pull_samples_thread;
  void loop_pull_samples();
  std::shared_ptr<WBLink> m_wb_link;
};

#endif
