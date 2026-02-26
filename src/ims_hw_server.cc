/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server.cc $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-09 10:25:37 +0000 (Thu, 09 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 660 $
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
#include <memory>
#include <string>
#include <ctime>
#include <sstream>

#include <grpc++/grpc++.h>

#include "ims_system.grpc.pb.h"
#include "image_table.grpc.pb.h"
#include "filesystem_table.grpc.pb.h"
#include "app_version.grpc.pb.h"

#include "ims_hw_server_state.h"

#include "ConnectionList.h"
#include "ImageOps.h"
#include "Filesystem.h"
#include "LibVersion.h"

#include "ims_hw_server_tonebuffer.h"
#include "ims_hw_server_downloader.h"
#include "ims_hw_server_player.h"
#include "ims_hw_server_signal_path.h"
#include "ims_hw_server_compensation.h"
#include "ims_hw_server_system_func.h"

#define MAJOR_VERSION 1
#define MINOR_VERSION 5

/*
 * Uncomment these defines to include the types of interface on which you wish to communicate with the iMS
 */
//#define  ENABLE_SERIAL_PORT_SCAN
#define  ENABLE_ETHERNET_SCAN
#define  ENABLE_USB_SCAN

/*
 * Uncomment these defines to restrict iMS scan to specific interface ports (using conventional interface type port naming)
 * Note that it is possible to add additional ports by inserting extra lines of "PortMask.push_back(...)" code below
 */
// #define SERIAL_PORT_MASK "COM3"
// #define ETHERNET_PORT_MASK  "192.168.2.100"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
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
using ims_hw_server::VersionInfo;
using ims_hw_server::scan_include;
using ims_hw_server::conn_settings;

// Defined Services
using ims_hw_server::hw_list;
using ims_hw_server::image_table_viewer;
using ims_hw_server::filesystem_table_viewer;
using ims_hw_server::app_version;

using namespace iMS;

// access the iMS System object
static std::shared_ptr<IMSServerState> imsState;

// Class to support the software version reporting server
class AppVersionServiceImpl final : public app_version::Service {
 public:
  AppVersionServiceImpl() : app_version::Service() {}

  Status app(ServerContext* context, const Empty* request, VersionInfo* response) override
  {
    std::stringstream ss;
    ss << MAJOR_VERSION << "." << MINOR_VERSION << "." << PATCH_VERSION;
    response->set_major(MAJOR_VERSION);
    response->set_minor(MINOR_VERSION);
    response->set_patch(PATCH_VERSION);
    response->set_as_string(ss.str());
    return Status::OK;    
  }

  Status lib(ServerContext* context, const Empty* request, VersionInfo* response) override
  {
    response->set_major(LibVersion::GetMajor());
    response->set_minor(LibVersion::GetMinor());
    response->set_patch(LibVersion::GetPatch());
    response->set_as_string(LibVersion::GetVersion());
    return Status::OK;    
  }
};

// Class to support the hw_list protobuf service
// Allows the client to: scan, connect to and disconnect from an iMS System
// which is then stored in global state for use by other services
class HWListServiceImpl final : public hw_list::Service {
 public:
  HWListServiceImpl() : hw_list::Service(),
    p_list (new ConnectionList())
      {
	auto& modules = p_list->modules();   // returns a ListBase<string>

	std::cout << "Serial port scan ";
#if !defined(ENABLE_SERIAL_PORT_SCAN)
	std::cout << "disabled" << std::endl;
	if (std::find(modules.begin(), modules.end(), "CM_SERIAL") != modules.end()) p_list->config("CM_SERIAL").IncludeInScan = false;
#else
	std::cout << "enabled ";
	// default is enabled.
#if defined(SERIAL_PORT_MASK)
	std::cout << "on port " << SERIAL_PORT_MASK << std::endl;
	if (std::find(modules.begin(), modules.end(), "CM_SERIAL") != modules.end()) p_list->config("CM_SERIAL").PortMask.push_back(SERIAL_PORT_MASK);
#else
	std::cout << "on all available ports" << std::endl;
#endif
#endif

	std::cout << "Ethernet port scan ";
#if !defined(ENABLE_ETHERNET_SCAN)
	std::cout << "disabled" << std::endl;
	if (std::find(modules.begin(), modules.end(), "CM_ETH") != modules.end()) p_list->config("CM_ETH").IncludeInScan = false;
#else
	std::cout << "enabled ";
	// default is enabled.
#if defined(ETHERNET_PORT_MASK)
	std::cout << "on port " << ETHERNET_PORT_MASK << std::endl;
	if (std::find(modules.begin(), modules.end(), "CM_ETH") != modules.end()) p_list->config("CM_ETH").PortMask.push_back(ETHERNET_PORT_MASK);
#else
	std::cout << "on all available ports" << std::endl;
#endif
#endif

	std::cout << "USB port scan ";
#if !defined(ENABLE_USB_SCAN)
	std::cout << "disabled" << std::endl;
	if (std::find(modules.begin(), modules.end(), "CM_USBSS") != modules.end()) p_list->config("CM_USBSS").IncludeInScan = false;
	if (std::find(modules.begin(), modules.end(), "CM_USBLITE") != modules.end()) p_list->config("CM_USBLITE").IncludeInScan = false;
#else
	std::cout << "enabled" << std::endl;
#endif
      }

  void include_modules(bool enet, bool usb, bool rs422)
  {
    auto& modules = p_list->modules();   // returns a ListBase<string>

    std::cout << "Serial port scan " << rs422 ? " enabled" : " disabled";
    if (std::find(modules.begin(), modules.end(), "CM_SERIAL") != modules.end()) p_list->config("CM_SERIAL").IncludeInScan = rs422;

    std::cout << "Ethernet port scan " << enet ? " enabled" : " disabled";
    if (std::find(modules.begin(), modules.end(), "CM_ETH") != modules.end()) p_list->config("CM_ETH").IncludeInScan = enet;

    std::cout << "USB port scan " << usb ? " enabled" : " disabled";
    if (std::find(modules.begin(), modules.end(), "CM_USBSS") != modules.end()) p_list->config("CM_USBSS").IncludeInScan = usb;
    if (std::find(modules.begin(), modules.end(), "CM_USBLITE") != modules.end()) p_list->config("CM_USBLITE").IncludeInScan = usb;
  }

  Status scanfull(ServerContext* context, const conn_settings* request,
                  ServerWriter<ims_system>* writer) override {
      if (request->has_include()) {
	include_modules(request->include().cm_enet(), request->include().cm_usb(), request->include().cm_serial());
      }

      Empty emptyReq;
      return this->scan(context, &emptyReq, writer);
  }

  Status scan(ServerContext* context, const Empty* request,
                  ServerWriter<ims_system>* writer) override {
    this->reset(context, nullptr, nullptr);
    std::cout << "Scanning for connected iMS Systems. Please wait. . . ." << std::endl;

    fulliMSList = p_list->Scan();

    std::cout << "Scan returned " << fulliMSList.size() << " systems." << std::endl;

    for (auto ims : fulliMSList) {
        ims_system new_ims;
      new_ims.set_connport(ims->ConnPort());
      if (ims->Synth().IsValid()) {
	// Set Model and Description
	new_ims.mutable_synth()->set_model(ims->Synth().Model());
	new_ims.mutable_synth()->set_description(ims->Synth().Description());

	// Set Firmware Version / Build Time
	new_ims.mutable_synth()->mutable_version()->set_major(ims->Synth().GetVersion().major);
	new_ims.mutable_synth()->mutable_version()->set_minor(ims->Synth().GetVersion().minor);
	new_ims.mutable_synth()->mutable_version()->set_revision(ims->Synth().GetVersion().revision);
	struct tm build_date = ims->Synth().GetVersion().build_date;
	new_ims.mutable_synth()->mutable_version()->mutable_build_time()->set_seconds(mktime(&build_date));

	// Set Capabilities
	new_ims.mutable_synth()->mutable_cap()->set_lower_freq(ims->Synth().GetCap().lowerFrequency);
	new_ims.mutable_synth()->mutable_cap()->set_upper_freq(ims->Synth().GetCap().upperFrequency);
	new_ims.mutable_synth()->mutable_cap()->set_freq_bits(ims->Synth().GetCap().freqBits);
	new_ims.mutable_synth()->mutable_cap()->set_ampl_bits(ims->Synth().GetCap().amplBits);
	new_ims.mutable_synth()->mutable_cap()->set_phs_bits(ims->Synth().GetCap().phaseBits);
	new_ims.mutable_synth()->mutable_cap()->set_lut_depth(ims->Synth().GetCap().LUTDepth);
	new_ims.mutable_synth()->mutable_cap()->set_lut_ampl_bits(ims->Synth().GetCap().LUTAmplBits);
	new_ims.mutable_synth()->mutable_cap()->set_lut_phs_bits(ims->Synth().GetCap().LUTPhaseBits);
	new_ims.mutable_synth()->mutable_cap()->set_lut_synca_bits(ims->Synth().GetCap().LUTSyncABits);
	new_ims.mutable_synth()->mutable_cap()->set_lut_syncd_bits(ims->Synth().GetCap().LUTSyncDBits);
	new_ims.mutable_synth()->mutable_cap()->set_channels(ims->Synth().GetCap().channels);
	new_ims.mutable_synth()->mutable_cap()->set_remote_upgrade(ims->Synth().GetCap().RemoteUpgrade);
	new_ims.mutable_synth()->mutable_cap()->set_channel_comp(ims->Synth().GetCap().ChannelComp);
      } else new_ims.mutable_synth()->set_model("Not Available");

      if (ims->Ctlr().IsValid()) {
	// Set Model and Description
	new_ims.mutable_ctlr()->set_model(ims->Ctlr().Model());
	new_ims.mutable_ctlr()->set_description(ims->Ctlr().Description());

	// Set Firmware Version / Build Time
	new_ims.mutable_ctlr()->mutable_version()->set_major(ims->Ctlr().GetVersion().major);
	new_ims.mutable_ctlr()->mutable_version()->set_minor(ims->Ctlr().GetVersion().minor);
	new_ims.mutable_ctlr()->mutable_version()->set_revision(ims->Ctlr().GetVersion().revision);
	struct tm build_date = ims->Ctlr().GetVersion().build_date;
	new_ims.mutable_ctlr()->mutable_version()->mutable_build_time()->set_seconds(mktime(&build_date));

	// Set Capabilities
	new_ims.mutable_ctlr()->mutable_cap()->set_n_synth(ims->Ctlr().GetCap().nSynthInterfaces);
	new_ims.mutable_ctlr()->mutable_cap()->set_fast_transfer(ims->Ctlr().GetCap().FastImageTransfer);
	new_ims.mutable_ctlr()->mutable_cap()->set_max_img_size(ims->Ctlr().GetCap().MaxImageSize);
	new_ims.mutable_ctlr()->mutable_cap()->set_sim_playback(ims->Ctlr().GetCap().SimultaneousPlayback);
	new_ims.mutable_ctlr()->mutable_cap()->set_max_img_rate(ims->Ctlr().GetCap().MaxImageRate);
	new_ims.mutable_ctlr()->mutable_cap()->set_remote_upgrade(ims->Ctlr().GetCap().RemoteUpgrade);
      } else new_ims.mutable_ctlr()->set_model("Not Available");
      writer->Write(new_ims);
    }
    return Status::OK;
  }

  Status connect(ServerContext* context, const ims_system* request,
		 ConnectionStatus* response) override {
    response->set_isopen(false);

    for (auto ims: fulliMSList) {
      if (ims->ConnPort() == request->connport()) {
	    ims->Connect();
	    if (ims->Open()) {
            std::cout << "Connected to iMS System " << ims->ConnPort() << std::endl;
            response->set_isopen(true);
            imsState->set(ims);
        } else {
            std::cout << "Connection to iMS System " << ims->ConnPort() << ": Attempt failed" << std::endl;
        }
      }
    }
    return Status::OK;
  }

  Status disconnect(ServerContext* context, const ims_system* request,
		 ConnectionStatus* response) override {
    auto ims = imsState->get();
    if (ims->Open()) {
      ims->Disconnect();
      if (ims->Open()) {
	std::cout << "Disconection from iMS System: Attempt failed" << std::endl;
      } else {
	std::cout << "Disconnected from iMS System" << std::endl;
	response->set_isopen(false);
      }
    }
    return Status::OK;
  }

  Status reset(ServerContext* context, const Empty* request,
		 Empty* response) override {
    auto ims = imsState->get();
    if (ims->Open()) {
        ims->Disconnect();
        if (ims->Open()) {
            std::cout << "Disconection from iMS System: Attempt failed" << std::endl;
        } else {
            std::cout << "Disconnected from iMS System" << std::endl;
        }
    }
    fulliMSList.clear();
    return Status::OK;
  }

 private:
  std::unique_ptr<ConnectionList> p_list;
  std::vector<std::shared_ptr<IMSSystem>> fulliMSList;
};

// Class to support the image_table gRPC service
class ImageTableViewerServiceImpl final : public image_table_viewer::Service
{
 public:
  ImageTableViewerServiceImpl() : image_table_viewer::Service()
    {
    }

  Status image_detail(ServerContext* context, const ims_system* request,
		      ServerWriter<image_table_entry>* writer) override {
    auto ims = imsState->get();
    if (ims->Open()) {
      ImageTableViewer itv(ims);
      for (int i=0; i<itv.Entries(); i++) {
	image_table_entry ite;
	ite.set_handle(itv[i].Handle());
	ite.set_addr(itv[i].Address());
	ite.set_n_pts(itv[i].NPts());
	ite.set_size(itv[i].Size());
	ite.set_fmt(itv[i].Format());
	ite.set_name(itv[i].Name());
	ite.set_uuid((const void*)itv[i].UUID().data(), 16);

	writer->Write(ite);
      }
      std::cout << "Returning ImageTable with " << itv.Entries() << " entries" << std::endl;
    }
    return Status::OK;
  }
};

// Class to support the filesystem_table gRPC service
class FilesystemTableViewerServiceImpl final : public filesystem_table_viewer::Service
{
 public:
  FilesystemTableViewerServiceImpl() : filesystem_table_viewer::Service()
    {
    }

  Status fileentry_detail(ServerContext* context, const ims_system* request,
		      ServerWriter<filesystem_table_entry>* writer) override {
    auto ims = imsState->get();
    if (ims->Open()) {
      FileSystemTableViewer fstv(ims);
      for (int i=0; i<fstv.Entries(); i++) {
	filesystem_table_entry fste;
	fste.set_isdefault(fstv[i].IsDefault());
	fste.set_addr(fstv[i].Address());
	fste.set_length(fstv[i].Length());
	fste.set_type(static_cast<int>(fstv[i].Type()));
	fste.set_name(fstv[i].Name());

	writer->Write(fste);
      }
      std::cout << "Returning FileSystemTable with " << fstv.Entries() << " entries" << std::endl;
    }
    return Status::OK;
  }

  Status is_valid(ServerContext* context, const ims_system* request,
		      FilesystemValid* response) override {
    auto ims = imsState->get();
    if (ims->Open()) {
      FileSystemTableViewer fstv(ims);
      response->set_valid(fstv.IsValid());
      std::cout << "Returning System FileSystemTable state: " << (fstv.IsValid() ? "valid" : "not valid") << std::endl;
    }
    return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:28241");

  HWListServiceImpl service;
  ImageTableViewerServiceImpl imgtbl_service;
  FilesystemTableViewerServiceImpl fst_service;
  ImageDownloadServiceImpl imgdl_service(imsState);
  SignalPathServiceImpl sigpath_service(imsState);
  ImagePlayerServiceImpl imgpl_service(imsState);
  CompensationDownloadServiceImpl compdl_service(imsState);
  AppVersionServiceImpl appver_service;
  ToneBufferDownloadServiceImpl tbdl_service(imsState);
  ToneBufferManagerServiceImpl tbmgr_service(imsState);
  SystemFuncServiceImpl sysfunc_service(imsState);

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  builder.RegisterService(&imgtbl_service);
  builder.RegisterService(&fst_service);
  builder.RegisterService(&imgdl_service);
  builder.RegisterService(&imgpl_service);
  builder.RegisterService(&sigpath_service);
  builder.RegisterService(&compdl_service);
  builder.RegisterService(&appver_service);
  builder.RegisterService(&tbdl_service);
  builder.RegisterService(&tbmgr_service);
  builder.RegisterService(&sysfunc_service);

  // Report Server version
  std::time_t t = std::time(nullptr);
  std::tm *const pTInfo = std::localtime(&t);
  std::cout << "iMS Hardware Server " << MAJOR_VERSION << "." << MINOR_VERSION << "." << PATCH_VERSION << std::endl;;
  std::cout << "(c) " << 1900 + pTInfo->tm_year << " Isomet Corporation. All Rights Reserved." << std::endl;
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  imsState = std::make_shared<IMSServerState>();
  RunServer();

  return 0;
}
