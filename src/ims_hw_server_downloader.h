/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_downloader.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-09 11:57:00 +0000 (Thu, 09 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 663 $
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

#include "image_player.grpc.pb.h"

#include <list>

#include "IMSSystem.h"

using grpc::Status;
using grpc::ServerContext;
using grpc::ServerReader;
using google::protobuf::Empty;

// Defined Messages
using ims_hw_server::image_header;
using ims_hw_server::image_point;
using ims_hw_server::image_format;
using ims_hw_server::DownloadStatus;
using ims_hw_server::DownloadHandle;

// Defined Services
using ims_hw_server::image_downloader;

class ImageDownloadBuffer;

// Class to support the image_downloader protobuf service
class ImageDownloadServiceImpl final : public image_downloader::Service {
 public:
 ImageDownloadServiceImpl(std::shared_ptr<iMS::IMSSystem> ims);

  // The Services
  Status create(ServerContext* context, const image_header* request,
		DownloadHandle* response) override;
  Status add(ServerContext* context, ServerReader<image_point>* reader,
	     Empty* response) override;
  Status format(ServerContext* context, const image_format* request,
	     Empty* response) override;
  Status download(ServerContext* context, const DownloadHandle* request,
		  DownloadStatus* response) override;
  Status dlstatus(ServerContext* context, const DownloadHandle* request,
		  DownloadStatus* response) override;

 private:
  bool m_firstRun;
  std::list<ImageDownloadBuffer*> downloadImageList;
  std::shared_ptr<iMS::IMSSystem> m_ims;
};
