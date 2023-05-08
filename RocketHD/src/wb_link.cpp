#include "wb_link.hpp"
#include "wifi_command_helper.hpp"

#include <utility>

WBLink::WBLink(RadiotapHeader::UserSelectableParams radioTapHeaderParams, TOptions options)
    : m_options(std::move(options)),
      m_radioTapHeaderParams(radioTapHeaderParams)
{
  m_console=spdlog::stdout_color_mt("wblink");
  m_console->set_level(spdlog::level::debug);
  assert(m_console);
  m_console->info("Broadcast card:{}",m_options.wlan);
  takeover_cards_monitor_mode();
  configure_cards();
  configure_video();
}

WBLink::~WBLink() {
  m_console->debug("WBLink::~WBLink() begin");
  m_wb_video_tx.reset();
  // give the monitor mode cards back to network manager
  wifi::commandhelper::nmcli_set_device_managed_status(m_options.wlan, true);
  m_console->debug("WBLink::~WBLink() end");
}

void WBLink::takeover_cards_monitor_mode() {
  m_console->debug( "takeover_cards_monitor_mode() begin");
  // We need to take "ownership" from the system over the cards used for monitor mode / wifibroadcast.
  // This can be different depending on the OS we are running on - in general, we try to go for the following with openhd:
  // Have network manager running on the host OS - the nice thing about network manager is that we can just tell it
  // to ignore the cards we are doing wifibroadcast with, instead of killing all processes that might interfere with
  // wifibroadcast and therefore making other networking incredibly hard.
  // Tell network manager to ignore the cards we want to do wifibroadcast on
  wifi::commandhelper::nmcli_set_device_managed_status(m_options.wlan, false);
  wifi::commandhelper::rfkill_unblock_all();
  // TODO: sometimes this happens:
  // 1) Running openhd fist time: pcap_compile doesn't work (fatal error)
  // 2) Running openhd second time: works
  // I cannot find what's causing the issue - a sleep here is the worst solution, but r.n the only one I can come up with
  // perhaps we'd need to wait for network manager to finish switching to ignoring the monitor mode cards ?!
  std::this_thread::sleep_for(std::chrono::seconds(1));
  // now we can enable monitor mode on the given cards.
  wifi::commandhelper::ip_link_set_card_state(m_options.wlan, false);
  wifi::commandhelper::iw_enable_monitor_mode(m_options.wlan);
  wifi::commandhelper::ip_link_set_card_state(m_options.wlan, true);
  m_console->debug("takeover_cards_monitor_mode() end");
}

void WBLink::configure_cards() {
  m_console->debug("configure_cards() begin");
  apply_txpower();
  m_console->debug("configure_cards() end");
}

void WBLink::configure_video() {
  // Video is unidirectional, aka always goes from air pi to ground pi
  m_wb_video_tx = create_wb_tx();
}

std::unique_ptr<WBTransmitter> WBLink::create_wb_tx() {
  return std::make_unique<WBTransmitter>(m_radioTapHeaderParams, m_options);
}

std::string WBLink::createDebug()const{
  std::stringstream ss;
  ss<<"VidTx: "<<m_wb_video_tx->createDebugState();
  return ss.str();
}

void WBLink::apply_txpower() {
  const auto before=std::chrono::steady_clock::now();
  // requires corresponding driver workaround for dynamic tx power
  wifi::commandhelper::iw_set_tx_power(m_options.wlan,22);
  const auto delta=std::chrono::steady_clock::now()-before;
  m_console->debug("Changing tx power took {}",MyTimeHelper::R(delta));
}

bool WBLink::set_tx_power_rtl8812au(int tx_power_index_override){
  m_console->debug("set_tx_power_rtl8812au {}index",tx_power_index_override);
  // m_settings->unsafe_get_settings().wb_rtl8812au_tx_pwr_idx_override=tx_power_index_override;
  // m_settings->persist();
  // apply_txpower();
  return true;
}

bool WBLink::set_mcs_index(int mcs_index) {
  m_console->debug("set_mcs_index {}",mcs_index);
  m_wb_video_tx->update_mcs_index(mcs_index);
  return true;
}

bool WBLink::set_video_fec_block_length(const int block_length) {
  m_console->debug("set_video_fec_block_length {}",block_length);
  m_wb_video_tx->update_fec_k(block_length);
  return true;
}

bool WBLink::set_video_fec_percentage(int fec_percentage) {
  m_console->debug("set_video_fec_percentage {}",fec_percentage);
  m_wb_video_tx->update_fec_percentage(fec_percentage);
  return true;
}

static uint32_t get_micros(std::chrono::nanoseconds ns){
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(ns).count());
}

void WBLink::transmit_video_data(std::vector<std::shared_ptr<std::vector<uint8_t>>> frame_fragments){
  m_wb_video_tx->try_enqueue_block(frame_fragments, 100);
}
