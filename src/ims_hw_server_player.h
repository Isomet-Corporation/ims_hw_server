/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_player.h $
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

#include "image_player.grpc.pb.h"
#include "ims_hw_server_state.h"

#include "Image.h"
#include "ImageOps.h"
#include "IMSSystem.h"

using grpc::Status;
using grpc::ServerContext;
using google::protobuf::Empty;

// Defined Messages
using ims_hw_server::PlayConfiguration;
using ims_hw_server::PlaybackStatus;

// Defined Services
using ims_hw_server::image_player;

class PlaybackHandler;

// Class to support the image_downloader protobuf service
class ImagePlayerServiceImpl final : public image_player::Service {
 public:
  ImagePlayerServiceImpl(std::shared_ptr<IMSServerState> state);
  ~ImagePlayerServiceImpl();

  // The Services
  Status play(ServerContext* context, const PlayConfiguration* request,
		PlaybackStatus* response) override;
  Status play_on_ext_trigger(ServerContext* context, const PlayConfiguration* request,
		PlaybackStatus* response) override;
  Status stop(ServerContext* context, const Empty* request,
		PlaybackStatus* response) override;
  Status stop_immediately(ServerContext* context, const Empty* request,
		PlaybackStatus* response) override;
  Status plstatus(ServerContext* context, const Empty* request,
		PlaybackStatus* response) override;

 private:
  bool StartPlayback(const PlayConfiguration* request, iMS::ImagePlayer::ImageTrigger trig);

  PlaybackHandler* p_pbh;
  iMS::ImageIndex idx { -1 };
  std::shared_ptr<IMSServerState> m_state;
};
