#include "gstreamerstream.hpp"

#include <gst/gst.h>
#include <unistd.h>

#include <regex>
#include <vector>

#include "gst_appsink_helper.hpp"
#include "rtp_eof_helper.hpp"

static std::string gst_state_change_return_to_string(GstStateChangeReturn & gst_state_change_return){
  return fmt::format("{}",gst_element_state_change_return_get_name(gst_state_change_return));
}

static std::string gst_element_get_current_state_as_string(GstElement * element){
  GstState state;
  GstState pending;
  const auto timeout=std::chrono::seconds(1);
  auto returnValue = gst_element_get_state(element, &state, &pending, std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count());
  return fmt::format("Gst state: ret:{} state:{} pending:{}",
                                    gst_state_change_return_to_string(returnValue),
                                    gst_element_state_get_name(state),
                                    gst_element_state_get_name(pending));
}

void GStreamerStream::initGstreamerOrThrow() {
  GError *error = nullptr;
  if (!gst_init_check(nullptr, nullptr, &error)) {
    m_console->error("gst_init_check() failed: {}",error->message);
    g_error_free(error);
    throw std::runtime_error("GStreamer initialization failed");
  }
}

GStreamerStream::GStreamerStream(std::shared_ptr<WBLink> wb_link)
: m_wb_link(std::move(wb_link))
{
  m_console=spdlog::stdout_color_mt("gstreamer");
  m_console->set_level(spdlog::level::debug);
  m_console->debug("GStreamerStream::GStreamerStream()");
  initGstreamerOrThrow();
  m_console->debug("GStreamerStream::GStreamerStream done");
}

GStreamerStream::~GStreamerStream() {
  // they are safe to call, regardless if we are already in cleaned up state or not
  GStreamerStream::stop();
  GStreamerStream::cleanup_pipe();
}

void GStreamerStream::setup() {
  m_console->debug("GStreamerStream::setup() begin");
  m_pipeline_content.str("");
  m_pipeline_content.clear();
  m_pipeline_content << "v4l2src device=/dev/video0 ! "
  "image/jpeg,width=1920,height=1080,framerate=30/1 ! "
  "queue ! avdec_mjpeg ! videoconvert ! video/x-raw,width=1920,height=1080,framerate=30/1,format=YUY2 ! "
  "videobox bottom=-8 ! queue ! mpph265enc bps=8000000 ! h265parse ! rtph265pay config-interval=-1 mtu=1024 ! "
  "appsink drop=true name=out_appsink";
  m_console->debug("Starting pipeline:[{}]",m_pipeline_content.str());
  // Protect against unwanted use - stop and free the pipeline first
  assert(m_gst_pipeline == nullptr);
  // Now start the (as a string) built pipeline
  GError *error = nullptr;
  m_gst_pipeline = gst_parse_launch(m_pipeline_content.str().c_str(), &error);
  m_console->debug("GStreamerStream::setup() end");
  m_stream_creation_time=std::chrono::steady_clock::now();
  if (error) {
    m_console->error( "Failed to create pipeline: {}",error->message);
    return;
  }
  // we pull data out of the gst pipeline as cpu memory buffer(s) using the gstreamer "appsink" element
  m_app_sink_element=gst_bin_get_by_name(GST_BIN(m_gst_pipeline), "out_appsink");
  assert(m_app_sink_element);
  m_pull_samples_run= true;
  m_pull_samples_thread=std::make_unique<std::thread>(&GStreamerStream::loop_pull_samples, this);
}

void GStreamerStream::stop_cleanup_restart() {
  stop();
  cleanup_pipe();
  setup();
  start();
}

std::string GStreamerStream::createDebug(){
  // std::unique_lock<std::mutex> lock(m_pipeline_mutex, std::try_to_lock);
  // if(!lock.owns_lock()){
  //   // We can just discard statistics data during a re-start
  //   return "GStreamerStream::No debug during restart\n";
  // }
  std::stringstream ss;
  GstState state;
  GstState pending;
  auto returnValue = gst_element_get_state(m_gst_pipeline, &state, &pending, 1000000000);
  ss << "GStreamerStream State:"<< returnValue << "." << state << "." << pending << ".";
  return ss.str();
}

void GStreamerStream::start() {
  m_console->debug("GStreamerStream::start()");
  if(!m_gst_pipeline){
    m_console->warn("gst_pipeline==null");
    return;
  }
  gst_element_set_state(m_gst_pipeline, GST_STATE_PLAYING);
  m_console->debug(gst_element_get_current_state_as_string(m_gst_pipeline));
}

void GStreamerStream::stop() {
  m_console->debug("GStreamerStream::stop()");
  if(!m_gst_pipeline){
    m_console->debug("gst_pipeline==null");
    return;
  }
  auto res=gst_element_set_state(m_gst_pipeline, GST_STATE_PAUSED);
  m_console->debug(gst_element_get_current_state_as_string(m_gst_pipeline));
}

void GStreamerStream::cleanup_pipe() {
  m_console->debug("GStreamerStream::cleanup_pipe() begin");
  if(m_pull_samples_thread){
    m_console->debug("terminating appsink poll thread begin");
    m_pull_samples_run= false;
    if(m_pull_samples_thread->joinable())m_pull_samples_thread->join();
    m_pull_samples_thread= nullptr;
    m_console->debug("terminating appsink poll thread end");
  }
  if(!m_gst_pipeline){
    m_console->debug("gst_pipeline==null");
    return;
  }
  // Jan 22: Confirmed this hangs quite a lot of pipeline(s) - removed for that reason
  /*m_console->debug("send EOS begin");
  // according to @Alex W we need a EOS signal here to properly shut down the pipeline
  if(!gst_element_send_event (m_gst_pipeline, gst_event_new_eos())){
    m_console->info("error gst_element_send_event eos"); // No idea what that means
  }else{
    m_console->info("success gst_element_send_event eos");
  }*/
  // TODO do we need to wait until the pipeline is actually in state NULL ?
  auto res=gst_element_set_state(m_gst_pipeline, GST_STATE_NULL);
  m_console->debug(gst_element_get_current_state_as_string(m_gst_pipeline));
  gst_object_unref (m_gst_pipeline);
  m_gst_pipeline =nullptr;
  m_console->debug("GStreamerStream::cleanup_pipe() end");
}

void GStreamerStream::restartIfStopped() {
  std::lock_guard<std::mutex> guard(m_pipeline_mutex);
  if(!m_gst_pipeline){
    m_console->debug("gst_pipeline==null");
    return;
  }
  const auto elapsed_since_start=std::chrono::steady_clock::now()-m_stream_creation_time;
  if(elapsed_since_start<std::chrono::seconds(5)){
    // give the cam X seconds in the beginning to properly start before restarting
    return;
  }
  GstState state;
  GstState pending;
  auto returnValue = gst_element_get_state(m_gst_pipeline, &state, &pending, 1000000000); // timeout in ns
  if (returnValue == 0) {
    m_console->debug("Panic gstreamer pipeline state is not running, restarting camera stream");
    // We fully restart the whole pipeline, since some issues might not be fixable by just setting paused
    // This will also show up in QOpenHD (log level >= warn), but we are limited by the n of characters in mavlink
    m_console->warn("Restarting camera, check your parameters / connection");
    stop_cleanup_restart();
    m_console->debug("Restarted");
  }
}

void GStreamerStream::on_new_rtp_fragmented_frame(std::vector<std::shared_ptr<std::vector<uint8_t>>> frame_fragments) {
  //m_console->debug("Got frame with {} fragments",frame_fragments.size());
  if(m_wb_link){
    m_wb_link->transmit_video_data(frame_fragments);
  }else{
    m_console->debug("No transmit interface");
  }
}

void GStreamerStream::on_new_rtp_frame_fragment(std::shared_ptr<std::vector<uint8_t>> fragment,uint64_t dts) {
  m_frame_fragments.push_back(fragment);
  bool is_last_fragment_of_frame=false;
  if(rtp_eof_helper::h265_end_block(fragment->data(),fragment->size())){
    is_last_fragment_of_frame= true;
  }
  if(m_frame_fragments.size()>1000){
    // Most likely something wrong with the "find end of frame" workaround
    m_console->debug("No end of frame found after 1000 fragments");
    is_last_fragment_of_frame= true;
  }
  if(is_last_fragment_of_frame){
    on_new_rtp_fragmented_frame(m_frame_fragments);
    m_frame_fragments.resize(0);
  }
}

void GStreamerStream::loop_pull_samples() {
  assert(m_app_sink_element);
  auto cb=[this](std::shared_ptr<std::vector<uint8_t>> fragment,uint64_t dts){
    on_new_rtp_frame_fragment(fragment,dts);
  };
  loop_pull_appsink_samples(m_pull_samples_run,m_app_sink_element,cb);
  m_frame_fragments.resize(0);
}
