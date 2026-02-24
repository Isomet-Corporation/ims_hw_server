/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_compensation.h $
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

#include <grpc++/grpc++.h>

#include "compensation.grpc.pb.h"

#include <list>

#include "IMSSystem.h"

using grpc::Status;
using grpc::ServerContext;
using grpc::ServerReader;
using google::protobuf::Empty;

// Defined Messages
using ims_hw_server::compensation_header;
using ims_hw_server::compensation_point;
using ims_hw_server::compensation_download;
using ims_hw_server::DownloadStatus;
using ims_hw_server::DownloadHandle;

// Defined Services
using ims_hw_server::compensation_downloader;

class CompensationDownloadBuffer;

// Class to support the compensation downloader protobuf service
class CompensationDownloadServiceImpl final : public compensation_downloader::Service {
 public:
 CompensationDownloadServiceImpl(std::shared_ptr<iMS::IMSSystem> ims) : compensation_downloader::Service(), m_ims(ims) {}

  // The Services
  Status create(ServerContext* context, const compensation_header* request,
		DownloadHandle* response) override;
  Status add(ServerContext* context, ServerReader<compensation_point>* reader,
	     Empty* response) override;
  Status download(ServerContext* context, const compensation_download* request,
		  DownloadStatus* response) override;
  Status dlstatus(ServerContext* context, const DownloadHandle* request,
		  DownloadStatus* response) override;

 private:
  std::list<CompensationDownloadBuffer*> downloadCompList;
  std::shared_ptr<iMS::IMSSystem> m_ims;
};
