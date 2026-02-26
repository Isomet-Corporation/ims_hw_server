/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server Test Client
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_test_client.cc $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2019-03-08 23:22:46 +0000 (Fri, 08 Mar 2019) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 386 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2015 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/


#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <ctime>
#include <conio.h>

#include <grpc++/grpc++.h>

// iMS API
#include "ConnectionList.h"
#include "Image.h"
#include "ImageOps.h"
#include "IMSTypeDefs.h"
#include "SignalPath.h"
#include "Compensation.h"
#include "ToneBuffer.h"

// gRPC Service Definition
#include "ims_system.grpc.pb.h"
#include "image_table.grpc.pb.h"
#include "filesystem_table.grpc.pb.h"
#include "image_player.grpc.pb.h"
#include "signal_path.grpc.pb.h"
#include "compensation.grpc.pb.h"
#include "tone_buffer.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Status;
using google::protobuf::Empty;

// Defined Messages
using ims_hw_server::ConnectionStatus;
using ims_hw_server::ims_system;
using ims_hw_server::ims_system_controller;
using ims_hw_server::ims_system_synthesiser;
using ims_hw_server::image_table_entry;
using ims_hw_server::filesystem_table_entry;
using ims_hw_server::FilesystemValid;
using ims_hw_server::image_header;
using ims_hw_server::image_point;
using ims_hw_server::DownloadStatus;
using ims_hw_server::DownloadHandle;
using ims_hw_server::PlayConfiguration;
using ims_hw_server::PlaybackStatus;
using ims_hw_server::PowerSettings;
using ims_hw_server::PhaseTunings;
using ims_hw_server::CalibrationTone;
using ims_hw_server::EnhancedTone;
using ims_hw_server::EnhancedTone_ChannelSweep;
using ims_hw_server::SweepControl;
using ims_hw_server::SyncMapping;
using ims_hw_server::MasterSwitch;
using ims_hw_server::compensation_header;
using ims_hw_server::compensation_point;
using ims_hw_server::compensation_download;
using ims_hw_server::tonebuffer_header;
using ims_hw_server::tonebuffer_entry;
using ims_hw_server::tonebuffer_params;
using ims_hw_server::tonebuffer_index;
using ims_hw_server::FileStoreSettings;
using ims_hw_server::FileStoreStatus;

// Defined Services
using ims_hw_server::hw_list;
using ims_hw_server::image_table_viewer;
using ims_hw_server::filesystem_table_viewer;
using ims_hw_server::image_downloader;
using ims_hw_server::image_player;
using ims_hw_server::signal_path;
using ims_hw_server::compensation_downloader;
using ims_hw_server::tonebuffer_downloader;
using ims_hw_server::tonebuffer_manager;

using namespace iMS;

// Workaround because libxml is compiled for VC12.0 (2013)
//extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }

class HWClient {
 public:
  HWClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(hw_list::NewStub(channel)) {}

  // Assambles the client's payload, sends it and presents the response back
  // from the server.
  bool GetHWList() {
    // Data we are sending to the server.
    Empty request;
    
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    std::unique_ptr<ClientReader<ims_system> > reader(stub_->scan(&context, request));

    int count=0;
    while (reader->Read(&myiMS)) {
      std::cout << "Found iMS "
                << myiMS.connport() << " with Synthesiser "
		<< myiMS.synth().model() << " and Controller "
		<< myiMS.ctlr().model() << std::endl;

      if (myiMS.synth().model() != "Not Available") {
	std::cout << "Synthesiser parameters:" << std::endl;
	std::cout << "  - model      : " << myiMS.synth().model() << std::endl;
	std::cout << "  - description: " << myiMS.synth().description() << std::endl;
	std::cout << "  - FW version : " << myiMS.synth().version().major() << "." << myiMS.synth().version().minor() << "." << myiMS.synth().version().revision() << std::endl;
	time_t build_time = myiMS.synth().version().build_time().seconds();
	std::cout << "  - Build Date : " << asctime(localtime(&build_time));
	std::cout << "  Capabilities: " << std::endl;
	std::cout << "    - Lower Freq : " << myiMS.synth().cap().lower_freq() << "MHz" << std::endl;
	std::cout << "    - Upper Freq : " << myiMS.synth().cap().upper_freq() << "MHz" << std::endl;
	std::cout << "    - Freq Bits  : " << myiMS.synth().cap().freq_bits() << std::endl;
	std::cout << "    - Ampl Bits  : " << myiMS.synth().cap().ampl_bits() << std::endl;
	std::cout << "    - Phase Bits : " << myiMS.synth().cap().phs_bits() << std::endl;
	std::cout << "    - LUT Depth  : " << myiMS.synth().cap().lut_depth() << std::endl;
	std::cout << "    - LUT Ampl Bits   : " << myiMS.synth().cap().lut_ampl_bits() << std::endl;
	std::cout << "    - LUT Phase Bits  : " << myiMS.synth().cap().lut_phs_bits() << std::endl;
	std::cout << "    - LUT Sync A Bits : " << myiMS.synth().cap().lut_synca_bits() << std::endl;
	std::cout << "    - LUT Sync D Bits : " << myiMS.synth().cap().lut_syncd_bits() << std::endl;
      }

      if (myiMS.ctlr().model() != "Not Available") {
	std::cout << "Controller parameters:" << std::endl;
	std::cout << "  - model      : " << myiMS.ctlr().model() << std::endl;
	std::cout << "  - description: " << myiMS.ctlr().description() << std::endl;
	std::cout << "  - FW version : " << myiMS.ctlr().version().major() << "." << myiMS.ctlr().version().minor() << "." << myiMS.ctlr().version().revision() << std::endl;
	time_t build_time = myiMS.ctlr().version().build_time().seconds();
	std::cout << "  - Build Date : " << asctime(localtime(&build_time));
	std::cout << "  Capabilities: " << std::endl;
	std::cout << "    - # Synthesiser Interfaces      : " << myiMS.ctlr().cap().n_synth() << std::endl;
	std::cout << "    - Supports Fast Transfer        : " << (myiMS.ctlr().cap().fast_transfer() ? "true" : "false") << std::endl;
	std::cout << "    - Maximum Image Size            : " << myiMS.ctlr().cap().max_img_size() << " points" << std::endl;
	std::cout << "    - Simultaneous Download/Playback: " << (myiMS.ctlr().cap().sim_playback() ? "true" : "false")  << std::endl;
	std::cout << "    - Maximum Image Rate            : " << (myiMS.ctlr().cap().max_img_rate() / 1000.0) << "kHz" << std::endl;
      }
      count++;
    }
    Status status = reader->Finish();

    // To pass the test, comms status must be OK and must have found at least one iMS
    return (status.ok() && count) ;
  }

  // Connect to HW.  GetHWList must have already been called successfully.
  bool HWConnect() 
  {
    ConnectionStatus connState;
    ClientContext context;
    Status status = stub_->connect(&context, myiMS, &connState);
    return (status.ok() && connState.isopen());
  }

  bool HWDisconnect()
  {
    ConnectionStatus connState;
    ClientContext context;
    Status status = stub_->disconnect(&context, myiMS, &connState);
    return (status.ok() && !connState.isopen());
  }

  const ims_system& ims() { return myiMS; }

 private:
  std::unique_ptr<hw_list::Stub> stub_;
  ims_system myiMS;
};

class ImageDownloadClient {
 public:
  ImageDownloadClient(std::shared_ptr<grpc::Channel> channel, const ims_system& ims)
    : stub_(image_downloader::NewStub(channel)), myiMS(ims) {}

  bool Download(iMS::Image& img)
  {
    ClientContext context[3];
    DownloadHandle hnd;
    Empty empty_msg;

    image_header img_hdr;
    img_hdr.set_clock_rate(img.ClockRate() / 1000.0);
    img_hdr.set_ext_divide(img.ExtClockDivide());
    img_hdr.set_name(img.Name());

    Status status = stub_->create(&context[0], img_hdr, &hnd);
    if (!status.ok()) {
        std::cout << "Error creating image_downloader context" << std::endl;
        return false;
    }
    
    std::cout << "Using download id " << hnd.id() << std::endl;
    std::unique_ptr<ClientWriter<image_point> > writer(
        stub_->add(&context[1], &empty_msg));
    for (const auto& it : img) {
      image_point pt;
      *(pt.mutable_context()) = hnd;

      FAP fap = it.GetFAP(RFChannel(1));
      pt.set_freq_ch1((double)fap.freq);
      pt.set_ampl_ch1((double)fap.ampl);
      pt.set_phs_ch1((double)fap.phase);

      fap = it.GetFAP(RFChannel(2));
      pt.set_freq_ch2((double)fap.freq);
      pt.set_ampl_ch2((double)fap.ampl);
      pt.set_phs_ch2((double)fap.phase);

      fap = it.GetFAP(RFChannel(3));
      pt.set_freq_ch3((double)fap.freq);
      pt.set_ampl_ch3((double)fap.ampl);
      pt.set_phs_ch3((double)fap.phase);

      fap = it.GetFAP(RFChannel(4));
      pt.set_freq_ch4((double)fap.freq);
      pt.set_ampl_ch4((double)fap.ampl);
      pt.set_phs_ch4((double)fap.phase);

      pt.set_synca1((double)it.GetSyncA(0));
      pt.set_synca2((double)it.GetSyncA(1));
      pt.set_syncd(it.GetSyncD());

      if (!writer->Write(pt)) {
        // Broken stream.
        std::cout << "Stream broken. Write failed" << std::endl;
        writer->WritesDone();    // always finish
        writer->Finish();
        return false;
      }
    }
    writer->WritesDone();
    status = writer->Finish();
    if (!status.ok()) {
        std::cout << "Failed to finish write" << std::endl;
        return false;
    }

    // Start Download
    status = stub_->download(&context[2], hnd, &dlstat);
    if (!status.ok()) {
        std::cout << "Failed to start download" << std::endl;
        return false;
    }

    // Monitor result
    int timeout = 0;
    while (1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      ClientContext clc;
      status = stub_->dlstatus(&clc, hnd, &dlstat);
      if (!status.ok()) {
        std::cout << "dlstatus msg failed" << std::endl;
        return false;
      }
      if ((int)dlstat.status() >= (int)DownloadStatus::DL_FINISHED) {
	if (dlstat.status() == DownloadStatus::DL_FINISHED) {
	  std::cout << "Download complete. New image handle: " << (int)hnd.id() << std::endl;
	  return true;
	} else {
	  std::cout << "Error in download" << std::endl;
	  return false;
	}
      } else {
	if (timeout++ >= 20) {
	  std::cout << "Download exceeded 10seconds." << std::endl;
	  return false;
	}
      }
    }
  }

  const std::string& GetUUID() const { return dlstat.uuid(); }
 private:
  std::unique_ptr<image_downloader::Stub> stub_;
  const ims_system& myiMS;
  DownloadStatus dlstat;
};

class ImagePlayerClient {
 public:
  ImagePlayerClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(image_player::NewStub(channel)) {}

  bool PlaybackTest(image_table_entry& ite) {
    ClientContext context[3];
    PlaybackStatus plstat;
    Empty empty_msg;

    // Stop any existing playback
    Status status = stub_->stop_immediately(&context[0], empty_msg, &plstat);
    if (!status.ok() || (plstat.status() != PlaybackStatus::PB_STOPPED)) return false;

    // Play Indefinitely
    PlayConfiguration cfg;
    cfg.mutable_img()->CopyFrom(ite);
    cfg.set_int_ext(PlayConfiguration::CLK_INTERNAL);
    cfg.set_trig(PlayConfiguration::TRIG_CONTINUOUS);
    cfg.set_rpts(PlayConfiguration::RPTS_FOREVER);
    cfg.set_post_delay(0.0);
    cfg.set_clock_rate(0.5);
    cfg.set_ampl_comp_enabled(true);
    cfg.set_phase_comp_enabled(true);

    status = stub_->play(&context[1], cfg, &plstat);
    if (!status.ok() || (plstat.status() != PlaybackStatus::PB_PENDING)) return false;

    std::cout << "Press any key to end playback..." << std::endl;
    std::cout << "Playback Progress: " ;
    unsigned int last_prog = 0;
    char c = '\0';
    std::cout << "Is there a frequency ramp on the RF output? [y/n] " << std::endl;
    do {
      if (_kbhit()) c = _getch();
      std::this_thread::sleep_for(std::chrono::milliseconds(135));
      ClientContext clc;
      status = stub_->plstatus(&clc, empty_msg, &plstat);
      if (!status.ok() || (plstat.status() != PlaybackStatus::PB_PLAYING)) return false;
      std::cout << ((last_prog > plstat.progress()) ? "*" : ".");
      last_prog = plstat.progress();
    } while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N'));

    status = stub_->stop(&context[2], empty_msg, &plstat);
    if (!status.ok() || (plstat.status() != PlaybackStatus::PB_STOPPED)) return false;
    
    return ((c == 'y') || (c == 'Y')) ? true : false;
  }

 private:
  std::unique_ptr<image_player::Stub> stub_;
};

class CompensationDownloadClient {
 public:
  CompensationDownloadClient(std::shared_ptr<grpc::Channel> channel, const ims_system& ims)
    : stub_(compensation_downloader::NewStub(channel)), myiMS(ims) {}

  bool Download(iMS::CompensationTable& comp)
  {
    ClientContext context[3];
    DownloadHandle* hnd = new DownloadHandle();
    Empty empty_msg;

    compensation_header comp_hdr;
    comp_hdr.set_n_pts(1 << myiMS.synth().cap().lut_depth());
    Status status = stub_->create(&context[0], comp_hdr, hnd);
    if (!status.ok()) return false;
    
    std::unique_ptr<ClientWriter<compensation_point> > writer(
        stub_->add(&context[1], &empty_msg));
    for (CompensationTable::const_iterator it = comp.cbegin(); it != comp.cend(); ++it) {
      compensation_point pt;
      *(pt.mutable_context()) = *hnd;

      pt.set_amplitude(it->Amplitude());
      pt.set_phase(it->Phase());
      pt.set_sync_dig(it->SyncDig());
      pt.set_sync_anlg(it->SyncAnlg());

      if (!writer->Write(pt)) {
        // Broken stream.
        return false;
      }
    }
    writer->WritesDone();
    status = writer->Finish();
    if (!status.ok()) return false;

    // Start Download
    compensation_download cdh;
    *(cdh.mutable_context()) = *hnd;
    cdh.set_channel(0x2468);  // send to all channels (global)
    status = stub_->download(&context[2], cdh, &dlstat);
    if (!status.ok()) return false;

    // Monitor result
    int timeout = 0;
    while (1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      ClientContext clc;
      status = stub_->dlstatus(&clc, *hnd, &dlstat);
      if (!status.ok()) return false;
      if ((int)dlstat.status() >= (int)DownloadStatus::DL_FINISHED) {
	if (dlstat.status() == DownloadStatus::DL_FINISHED) {
	  std::cout << "Compensation Download complete. New handle: " << (int)hnd->id() << std::endl;
	  return true;
	} else {
	  std::cout << "Error in download" << std::endl;
	  return false;
	}
      } else {
	if (timeout++ >= 50) {
	  std::cout << "Download exceeded 25 seconds." << std::endl;
	  return false;
	}
      }
    }
  }

  const std::string& GetUUID() const { return dlstat.uuid(); }
 private:
  std::unique_ptr<compensation_downloader::Stub> stub_;
  const ims_system& myiMS;
  DownloadStatus dlstat;
};


class SignalPathClient {
public:
  SignalPathClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(signal_path::NewStub(channel)) {}

  bool SignalPathTest() {
    ClientContext context[9];
    Empty empty_msg[2];    

    // Set a tone
    CalibrationTone tone;
    tone.set_freq(70.0);
    tone.set_ampl(100.0);
    tone.set_phs(0.0);
    
    grpc::Status status = stub_->set_tone(&context[0], tone, &empty_msg[0]);
    if (!status.ok()) return false;

    PowerSettings power;
    power.set_ddspower(75.0);
    power.set_wiper1power(70.0);
    power.set_wiper2power(70.0);
    power.set_src(PowerSettings::WIPER_1);

    status = stub_->dds_power(&context[1], power, &empty_msg[0]);
    if (!status.ok()) return false;

    char c;
    std::cout << "Is there a 70MHz tone on the output? [y/n] ";
    do {
      c = _getch();
    } while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N'));
    std::cout << std::endl;
    if ((c != 'y') && (c != 'Y')) return false;

    c = '\0';
    std::cout << "Is power level toggling (DDS Power)? [y/n] ";
    do {
      if (_kbhit()) c = _getch();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      ClientContext clc;
      if (power.ddspower() == 75.0) power.set_ddspower(10.0);
      else power.set_ddspower(75.0);
      status = stub_->dds_power(&clc, power, &empty_msg[0]);
      if (!status.ok()) return false;
    } while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N'));
    std::cout << std::endl;
    if ((c != 'y') && (c != 'Y')) return false;

    c = '\0';
    power.set_ddspower(75.0);
    std::cout << "What about now (Wiper 1)? [y/n] ";
    do {
      if (_kbhit()) c = _getch();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      ClientContext clc;
      if (power.wiper1power() == 70.0) power.set_wiper1power(10.0);
      else power.set_wiper1power(70.0);
      status = stub_->dds_power(&clc, power, &empty_msg[0]);
      if (!status.ok()) return false;
    } while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N'));
    std::cout << std::endl;
    if ((c != 'y') && (c != 'Y')) return false;

    c = '\0';
    power.set_wiper1power(70.0);
    power.set_src(PowerSettings::WIPER_2);
    std::cout << "And now (Wiper 2)? [y/n] ";
    do {
      if (_kbhit()) c = _getch();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      ClientContext clc;
      if (power.wiper2power() == 70.0) power.set_wiper2power(10.0);
      else power.set_wiper2power(70.0);
      status = stub_->dds_power(&clc, power, &empty_msg[0]);
      if (!status.ok()) return false;
    } while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N'));
    std::cout << std::endl;
    if ((c != 'y') && (c != 'Y')) return false;

    c = '\0';
    power.set_ddspower(0.0);
    power.set_src(PowerSettings::OFF);
    status = stub_->dds_power(&context[2], power, &empty_msg[0]);
    if (!status.ok()) return false;
    std::cout << "Are we at minimum power? [y/n] ";
    do {
      c = _getch();
    } while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N'));
    std::cout << std::endl;
    if ((c != 'y') && (c != 'Y')) return false;

    c = '\0';
    MasterSwitch mastSw;
    mastSw.set_amplifieren(true);
    mastSw.set_rfchannel12en(true);
    mastSw.set_rfchannel34en(true);
    mastSw.set_externalen(true);
    status = stub_->rf_enable(&context[3], mastSw, &empty_msg[0]);
    if (!status.ok()) return false;
    std::cout << "Good time to test the amplifier channels. Are they enabled (all 4)? [y/n] ";
    do {
      c = _getch();
    } while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N'));
    std::cout << std::endl;
    if ((c != 'y') && (c != 'Y')) return false;
    mastSw.set_amplifieren(false);
    mastSw.set_rfchannel12en(false);
    mastSw.set_rfchannel34en(false);
    mastSw.set_externalen(false);
    status = stub_->rf_enable(&context[4], mastSw, &empty_msg[0]);

    c = '\0';
    power.set_wiper2power(70.0);
    power.set_src(PowerSettings::WIPER_1);
    status = stub_->dds_power(&context[5], power, &empty_msg[0]);
    if (!status.ok()) return false;

    status = stub_->clear_tone(&context[6], empty_msg[1], &empty_msg[0]);
    if (!status.ok()) return false;
    std::cout << "Ok. They're disabled again.  Is the Tone now off? [y/n] ";
    do {
      c = _getch();
    } while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N'));
    std::cout << std::endl;
    if ((c != 'y') && (c != 'Y')) return false;

    EnhancedTone enh_tone;
    for (int i=RFChannel::min; i<=RFChannel::max; i++) {
      EnhancedTone_ChannelSweep* sweep = enh_tone.add_chan();
      sweep->set_start_freq(i*12.5);
      sweep->set_end_freq(i*25.0);
      sweep->set_ampl(100.0);
      sweep->set_up_ramp(1000000);
      sweep->set_down_ramp(2000000);
      sweep->set_n_steps(1000000);
      sweep->set_mode(EnhancedTone_ChannelSweep::FrequencyDwell);
      sweep->set_dac_ref(EnhancedTone_ChannelSweep::FullScale);
    }
    status = stub_->set_enh_tone(&context[7], enh_tone, &empty_msg[0]);
    if (!status.ok()) return false;
    
    std::cout << "I've set up a frequency linear sweep on each output channel at multiples of 12.5-25MHz, rising 1 sec, falling 2sec" << std::endl;
    std::cout << "Hit space to toggle, 'y' to pass, 'n' to fail" << std::endl;
    bool tog_state = false;
    SweepControl swc;
    swc.set_manual_trigger_enable(true);
    do {
      swc.set_trig_ch1(tog_state);
      swc.set_trig_ch2(tog_state);
      swc.set_trig_ch3(tog_state);
      swc.set_trig_ch4(tog_state);
      ClientContext clc;
      status = stub_->control_enh_sweep(&clc, swc, &empty_msg[0]);
      if (!status.ok()) return false;

      do {
	c = _getch();
      } while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N') && (c != ' '));
      if ( (c=='y') || (c== 'Y') ) break;
      else
	switch (c) {
	  case 'n':
  	  case 'N':
	    stub_->clear_enh_tone(&context[8], empty_msg[1], &empty_msg[0]);
	    return false;
	case ' ': {
	  tog_state = !tog_state;
	}
	  break;
	}
      
    } while (1);
    status = stub_->clear_enh_tone(&context[8], empty_msg[1], &empty_msg[0]);
    if (!status.ok()) return false;

    return true;
  }

 private:
  std::unique_ptr<signal_path::Stub> stub_;
};

class ImageTableClient {
 public:
  ImageTableClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(image_table_viewer::NewStub(channel)) {}

  bool DisplayImageTable(const ims_system& ims)
  {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    image_table_entry ite;
    int count=0;
    ite_list.clear();
    std::unique_ptr<ClientReader<image_table_entry> > reader(stub_->image_detail(&context, ims));
    while (reader->Read(&ite)) {
      ite_list.push_back(ite);
      std::cout << "Image[" << count << "] id:" << ite.handle() <<
	" Addr: 0x" << std::setw(8) << std::setfill('0') << std::hex << ite.addr() <<
	" Points: " << std::dec << std::setw(9) << std::setfill(' ') << ite.n_pts() <<
	" ByteLength: " << std::setw(9) << std::setfill(' ') << ite.size() <<
	" Format Code: " << std::hex << ite.fmt() <<
	" UUID: ";
      int j;
      std::cout << std::hex;
      for (j = 0; j < 4; j++) std::cout << std::setfill('0') << std::setw(2) << ((uint32_t)ite.uuid().c_str()[j] & 0xff);
      std::cout << "-";
      for (j = 4; j < 6; j++) std::cout << std::setfill('0') << std::setw(2) << ((uint32_t)ite.uuid().c_str()[j] & 0xff);
      std::cout << "-";
      for (j = 6; j < 8; j++) std::cout << std::setfill('0') << std::setw(2) << ((uint32_t)ite.uuid().c_str()[j] & 0xff);
      std::cout << "-";
      for (j = 8; j < 10; j++) std::cout << std::setfill('0') << std::setw(2) << ((uint32_t)ite.uuid().c_str()[j] & 0xff);
      std::cout << "-";
      for (j = 10; j < 16; j++) std::cout << std::setfill('0') << std::setw(2) << ((uint32_t)ite.uuid().c_str()[j] & 0xff);
      std::cout << " Name: " << ite.name();
      std::cout << std::dec << std::endl;
      
      count++;
    }
    Status status = reader->Finish();

    // To pass the test, comms status must be OK and must have found at least one Image Entry
    return (status.ok() && count) ;
  }

  image_table_entry* get_entry(std::string& uuid)
  {
    for (std::vector<image_table_entry>::iterator it = ite_list.begin(); it != ite_list.end(); ++it)
      {
	if (it->uuid() == uuid) return &(*it);
      }
    return nullptr;
  }
 private:
  std::unique_ptr<image_table_viewer::Stub> stub_;
  std::vector<image_table_entry> ite_list;
};

class FileSystemTableClient {
 public:
  FileSystemTableClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(filesystem_table_viewer::NewStub(channel)) {}

  bool DisplayFileSystemTable(const ims_system& ims)
  {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context1, context2;

    filesystem_table_entry fste;
    int count=0;
    std::unique_ptr<ClientReader<filesystem_table_entry> > reader(stub_->fileentry_detail(&context1, ims));
    while (reader->Read(&fste)) {
      std::cout << "FST[" << std::setfill('0') << std::setw(2) << std::dec << count << "]" <<
	std::setfill(' ') << (fste.isdefault() ? "*" : " ") <<
	" : Type " << std::setw(2) << fste.type() <<
	" Addr: 0x" << std::hex << std::setw(5) << std::setfill('0') << fste.addr() <<
	" Len: " << std::dec << std::setw(6) << fste.length() <<
	" Name: " << fste.name() <<
	std::endl;
      
      count++;
    }
    Status status = reader->Finish();
    if (!status.ok()) return false;

    // To pass the test, comms status must be OK but filesystem may or may not be valid
    FilesystemValid fsv;
    status = stub_->is_valid(&context2, ims, &fsv);
    return (status.ok()) ;
  }
 private:
  std::unique_ptr<filesystem_table_viewer::Stub> stub_;
};


class ToneBufferDownloadClient {
 public:
  ToneBufferDownloadClient(std::shared_ptr<grpc::Channel> channel, const ims_system& ims)
    : stub_(tonebuffer_downloader::NewStub(channel)), myiMS(ims) {}

  bool Download(iMS::ToneBuffer& tbuf)
  {
    ClientContext context[3];
    DownloadHandle hnd;
    Empty empty_msg;

    tonebuffer_header tbuf_hdr;
    tbuf_hdr.set_first(0);
    tbuf_hdr.set_last(255);
    Status status = stub_->create(&context[0], tbuf_hdr, &hnd);
    if (!status.ok()) return false;
    
    std::unique_ptr<ClientWriter<tonebuffer_entry> > writer(
        stub_->add(&context[1], &empty_msg));
    for (ToneBuffer::const_iterator it = tbuf.cbegin(); it != tbuf.cend(); ++it) {
      tonebuffer_entry pt;
      *(pt.mutable_context()) = hnd;

      FAP fap = it->GetFAP(RFChannel(1));
      pt.set_freq_ch1((double)fap.freq);
      pt.set_ampl_ch1((double)fap.ampl);
      pt.set_phs_ch1((double)fap.phase);

      fap = it->GetFAP(RFChannel(2));
      pt.set_freq_ch2((double)fap.freq);
      pt.set_ampl_ch2((double)fap.ampl);
      pt.set_phs_ch2((double)fap.phase);

      fap = it->GetFAP(RFChannel(3));
      pt.set_freq_ch3((double)fap.freq);
      pt.set_ampl_ch3((double)fap.ampl);
      pt.set_phs_ch3((double)fap.phase);

      fap = it->GetFAP(RFChannel(4));
      pt.set_freq_ch4((double)fap.freq);
      pt.set_ampl_ch4((double)fap.ampl);
      pt.set_phs_ch4((double)fap.phase);

      if (!writer->Write(pt)) {
        // Broken stream.
        return false;
      }
    }
    writer->WritesDone();
    status = writer->Finish();
    if (!status.ok()) return false;

    // Start Download
    status = stub_->download(&context[2], hnd, &dlstat);
    if (!status.ok()) return false;

    // Monitor result
    int timeout = 0;
    while (1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      ClientContext clc;
      status = stub_->dlstatus(&clc, hnd, &dlstat);
      if (!status.ok()) return false;
      if ((int)dlstat.status() >= (int)DownloadStatus::DL_FINISHED) {
	if (dlstat.status() == DownloadStatus::DL_FINISHED) {
	  std::cout << "Tone Buffer Download complete. New handle: " << (int)hnd.id() << std::endl;
	  return true;
	} else {
	  std::cout << "Error in download" << std::endl;
	  return false;
	}
      } else {
	if (timeout++ >= 120) {
	  std::cout << "Download exceeded 60 seconds." << std::endl;
	  return false;
	}
      }
    }
  }

 private:
  std::unique_ptr<tonebuffer_downloader::Stub> stub_;
  const ims_system& myiMS;
  DownloadStatus dlstat;
};


class ToneBufferManagerClient {
 public:
  ToneBufferManagerClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(tonebuffer_manager::NewStub(channel)) {}

  bool ToneBufferTest() {
    std::vector<ClientContext*> context;
    Empty empty_msg;

    // Start with First tone in table, no compensation
    tonebuffer_params params;
    params.clear_amplitude_compensation_enabled();
    params.clear_phase_compensation_enabled();
    params.set_control(tonebuffer_params::HOST);

    std::string user_input;
    for (int i=0; i<2; i++)
      {
	context.push_back(new ClientContext());
	Status status = stub_->start(context.back(), params, &empty_msg);
	if (!status.ok()) return false;

	std::cout << "Select a tone buffer index [0-255], [y|n] to pass or fail test " << std::endl;
	do {
	  std::cin >> user_input;
	  if ( (user_input == "y") || (user_input == "Y") || (user_input == "n") || (user_input == "N") )
	    break;
	  else {
	    int index = 0;
	    try {
	      index = std::stoi(user_input.c_str(), NULL, 0);
	    }
	    catch (std::invalid_argument)
	      {
		continue;
	      }
	    index = (index > 255) ? 255 : (index < 0) ? 0 : index;
	    
	    tonebuffer_index tbi;
	    tbi.set_index(index);
	    context.push_back(new ClientContext());
	    Status status = stub_->select(context.back(), tbi, &empty_msg);
	    if (!status.ok()) return false;
	  }
	}
	while(1);
	
	context.push_back(new ClientContext());
	status = stub_->stop(context.back(), empty_msg, &empty_msg);
	if (!status.ok()) return false;

	if ((user_input == "n") || (user_input == "N")) break;

	if (!i){
	  std::cout << "Now repeating the test with compensation table in signal path" << std::endl;
	  params.set_amplitude_compensation_enabled(true);
	  params.set_phase_compensation_enabled(true);
	}
      }
    for(std::vector<ClientContext*>::iterator it = context.begin(); it != context.end(); ++it)
      delete *it;
    return ((user_input == "y") || (user_input == "Y")) ? true : false;
  }

 private:
  std::unique_ptr<tonebuffer_manager::Stub> stub_;
};

int main(int argc, char** argv) {
  int test_no = 1;
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " [server hostname/IP address]" << std::endl;
    return -1;
  } 
  std::string hostname(argv[1]);
  hostname += ":28241";

  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 28241). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(hostname, grpc::InsecureChannelCredentials());
  HWClient hwlist(channel);

  std::cout << "----------Test " << test_no++ << ": Scan and Connect ---------------------------" << std::endl;
  if (!hwlist.GetHWList()) {
      std::cout << "hw_list::scan rpc failed." << std::endl;
      return -1;
  }
  if (!hwlist.HWConnect()) {
      std::cout << "hw_list::connect rpc failed." << std::endl;
      return -1;
  }
  std::cout << std::endl;

  std::cout << "----------Test " << test_no++ << ": Create Test Image and Download  -----------" << std::endl;
  std::string dl_uuid;
  {
    Image img;
    FAP fap1(MHz(70.0), Percent(100.0), Degrees(180.0));
    const int nTestPts = 1000;
    for (int i = 0; i < nTestPts; i++)
      {
	fap1.freq = MHz(50.0 + (90.0 - 50.0) * ((double)i / nTestPts));
	img.InsertPoint(img.end(), ImagePoint(fap1, (float)i/nTestPts, (i==0)));
      }
    img.ClockRate(kHz(0.5));
    img.ExtClockDivide(32);
    img.Name() = "test image";

    ImageDownloadClient idc(channel, hwlist.ims());

    if (!idc.Download(img)) {
      std::cout << "image_downloader service usage failed." << std::endl;
      return -1;
    } else {
      std::cout << "image_downloader service usage test passed." << std::endl;
      dl_uuid = idc.GetUUID();
    }
    std::cout << std::endl;

  }

  std::cout << "----------Test " << test_no++ << ": Display Image Table -----------------------" << std::endl;
  ImageTableClient itc(channel);
  {
    bool result = itc.DisplayImageTable(hwlist.ims());
    if (itc.get_entry(dl_uuid) == nullptr) {
      std::cout << "Downloaded image not in Image Table." << std::endl;
      result = false;
    }
    if (!result) {
      std::cout << "image_table_viewer::image_detail rpc failed." << std::endl;
      return -1;
    }
    std::cout << std::endl;
  }

  std::cout << "----------Test " << test_no++ << ": Read File System Table ---------------------" << std::endl;
  {
    FileSystemTableClient fstc(channel);
    if (!fstc.DisplayFileSystemTable(hwlist.ims())) {
      std::cout << "filesystem_table_viewer::fileentry_detail rpc failed." << std::endl;
      return -1;
    }
    std::cout << std::endl;
  }

  std::cout << "----------Test " << test_no++ << ": Check Signal Path --------------------------" << std::endl;
  {
    SignalPathClient spc(channel);
    if (!spc.SignalPathTest()) {
      std::cout << "Signal Path test failed." << std::endl;
      return -1;
    }
    std::cout << std::endl;
  }

  std::cout << "----------Test " << test_no++ << ": Create Compensation Table and Download  ----" << std::endl;
  std::string compdl_uuid;
  {
    CompensationTable comp;
    const int nTestPts = (1 << hwlist.ims().synth().cap().lut_depth());
    for (int i = 0; i < nTestPts; i++)
      {
	double x = ((double)i / ((double)nTestPts/2));
	comp[i].Amplitude( Percent(((i >= (nTestPts/2)) ? 2.0 - x : x) * 100.0) );
      }

    CompensationDownloadClient cdc(channel, hwlist.ims());

    if (!cdc.Download(comp)) {
      std::cout << "compensation_downloader service usage failed." << std::endl;
      return -1;
    } else {
      std::cout << "compensation_downloader service usage test passed." << std::endl;
      compdl_uuid = cdc.GetUUID();
    }
    std::cout << std::endl;

  }

  std::cout << "----------Test " << test_no++ << ": Play Downloaded Image ----------------------" << std::endl;
  {
    ImagePlayerClient ipc(channel);
    if (!ipc.PlaybackTest(*itc.get_entry(dl_uuid))) {
      std::cout << "playback test failed." << std::endl;
      return -1;
    }
    std::cout << std::endl;
  }

  std::cout << "----------Test " << test_no++ << ": Download a Tone Buffer ---------------------" << std::endl;
  {
    ToneBuffer tbuf;
    int i=0;
    double lf = hwlist.ims().synth().cap().lower_freq();
    double uf = hwlist.ims().synth().cap().upper_freq();
    for(ToneBuffer::iterator it = tbuf.begin(); it != tbuf.end(); ++it)
      {
	FAP fap(MHz(lf + (uf-lf) * ((double)i++ / tbuf.Size())), Percent(100.0), Degrees(0.0));
	it->SetAll(fap);
      }

    ToneBufferDownloadClient tbdc(channel, hwlist.ims());

    if (!tbdc.Download(tbuf)) {
      std::cout << "tonebuffer_downloader service usage failed." << std::endl;
      return -1;
    } else {
      std::cout << "tonebuffer_downloader service usage test passed." << std::endl;
    }
    std::cout << std::endl;

  }

  std::cout << "----------Test " << test_no++ << ": Play Tone Buffer  --------------------------" << std::endl;
  {
    ToneBufferManagerClient tbmc(channel);
    if (!tbmc.ToneBufferTest()) {
      std::cout << "tone buffer manager test failed." << std::endl;
      return -1;
    }
    std::cout << std::endl;
  }

  std::cout << "----------Test " << test_no++ << ": Disconnect ---------------------------------" << std::endl;
  if (!hwlist.HWDisconnect()) {
      std::cout << "hw_list::disconnect rpc failed." << std::endl;
      return -1;
  }
  std::cout << std::endl;

  std::cout << "Test PASSED." << std::endl;
  return 0;
}
