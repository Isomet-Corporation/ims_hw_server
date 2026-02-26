/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_tonebuffer.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2017-09-11 23:55:34 +0100 (Mon, 11 Sep 2017) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 300 $
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

#include <grpc++/grpc++.h>

#include "tone_buffer.grpc.pb.h"
#include "ims_hw_server_state.h"

#include <list>

#include "IMSSystem.h"
#include "SignalPath.h"

using grpc::Status;
using grpc::ServerContext;
using grpc::ServerReader;
using google::protobuf::Empty;

// Defined Messages
using ims_hw_server::tonebuffer_header;
using ims_hw_server::tonebuffer_entry;
using ims_hw_server::tonebuffer_params;
using ims_hw_server::tonebuffer_index;
using ims_hw_server::DownloadStatus;
using ims_hw_server::DownloadHandle;

// Defined Services
using ims_hw_server::tonebuffer_downloader;
using ims_hw_server::tonebuffer_manager;

class ToneBufferDownloadBuffer;

// Class to support the tone buffer downloader protobuf service
class ToneBufferDownloadServiceImpl final : public tonebuffer_downloader::Service {
 public:
 ToneBufferDownloadServiceImpl(std::shared_ptr<IMSServerState> state) : tonebuffer_downloader::Service(), m_state(state) {}

  // The Services
  Status create(ServerContext* context, const tonebuffer_header* request,
		DownloadHandle* response) override;
  Status add(ServerContext* context, ServerReader<tonebuffer_entry>* reader,
	     Empty* response) override;
  Status download(ServerContext* context, const DownloadHandle* request,
		  DownloadStatus* response) override;
  Status dlstatus(ServerContext* context, const DownloadHandle* request,
		  DownloadStatus* response) override;

 private:
  std::list<ToneBufferDownloadBuffer*> downloadTbufList;
  std::shared_ptr<IMSServerState> m_state;
};

// Class to support the tone buffer manager protobuf service
class ToneBufferManagerServiceImpl final : public tonebuffer_manager::Service {
 public:
 ToneBufferManagerServiceImpl(std::shared_ptr<IMSServerState> state) : tonebuffer_manager::Service(), m_state(state), m_sp(nullptr) {}
  ~ToneBufferManagerServiceImpl() {if (m_sp != nullptr) {delete m_sp; m_sp = nullptr; } }

  // The Services
  Status start(ServerContext* context, const tonebuffer_params* request,
	       Empty* response);
  Status select(ServerContext* context, const tonebuffer_index* request,
		Empty* response);
  Status stop(ServerContext* context, const Empty* request,
	      Empty* response);

 private:
  iMS::SignalPath* m_sp;
  std::shared_ptr<IMSServerState> m_state;
};
