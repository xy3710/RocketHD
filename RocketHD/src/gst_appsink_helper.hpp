//
// Created by consti10 on 06.12.22.
//

#ifndef GST_APPSINK_HELPER_H_
#define GST_APPSINK_HELPER_H_

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

static std::shared_ptr<std::vector<uint8_t>> gst_copy_buffer(GstBuffer* buffer){
  assert(buffer);
  const auto buff_size = gst_buffer_get_size(buffer);
  //openhd::log::get_default()->debug("Got buffer size {}", buff_size);
  auto ret = std::make_shared<std::vector<uint8_t>>(buff_size);
  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_READ);
  assert(map.size == buff_size);
  std::memcpy(ret->data(), map.data, buff_size);
  gst_buffer_unmap(buffer, &map);
  return ret;
}

// based on https://github.com/Samsung/kv2streamer/blob/master/kv2streamer-lib/gst-wrapper/GstAppSinkPipeline.cpp
/**
 * Helper to pull data out of a gstreamer pipeline
 * @param keep_looping if set to false, method returns after max timeout_ns
 * @param app_sink_element the Gst App Sink to pull data from
 * @param out_cb fragments are forwarded via this cb
 */
static void loop_pull_appsink_samples(bool& keep_looping,GstElement *app_sink_element,
                                      const std::function<void(std::shared_ptr<std::vector<uint8_t>> fragment,uint64_t dts)>& out_cb){
  assert(app_sink_element);
  assert(out_cb);
  const uint64_t timeout_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(100)).count();
  while (keep_looping){
    GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(app_sink_element),timeout_ns);
    if (sample) {
      GstBuffer* buffer = gst_sample_get_buffer(sample);
      if (buffer) {
        auto buff_copy=gst_copy_buffer(buffer);
        out_cb(buff_copy,buffer->dts);
      }
      gst_sample_unref(sample);
    }
  }
}

#endif
