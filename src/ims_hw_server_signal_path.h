/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_signal_path.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2023-03-31 22:22:31 +0100 (Fri, 31 Mar 2023) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 557 $
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

#include "signal_path.grpc.pb.h"
#include "ims_hw_server_state.h"
#include "IMSSystem.h"

using grpc::Status;
using grpc::ServerContext;
using google::protobuf::Empty;
using google::protobuf::BoolValue;

// Defined Messages
using ims_hw_server::PowerSettings;
using ims_hw_server::ChannelPowerSettings;
using ims_hw_server::PhaseTunings;
using ims_hw_server::CalibrationTone;
using ims_hw_server::SyncMapping;
using ims_hw_server::SyncDelay;
using ims_hw_server::ChanDelay;
using ims_hw_server::MasterSwitch;
using ims_hw_server::EnhancedTone;
using ims_hw_server::EnhancedTone_ChannelSweep;
using ims_hw_server::SweepControl;
using ims_hw_server::SignalPathStatus;
using ims_hw_server::ChannelFlags;

// Defined Services
using ims_hw_server::signal_path;

class SignalPathServiceImpl final : public signal_path::Service {
 public:
  SignalPathServiceImpl(std::shared_ptr<IMSServerState> state);
  ~SignalPathServiceImpl();

  // The Services
  Status dds_power(ServerContext* context, const PowerSettings* request,
		   Empty* response) override;
  Status channel_power(ServerContext* context, const ChannelPowerSettings* request,
		   Empty* response) override;
  Status rf_enable(ServerContext* context, const MasterSwitch* request,
		   Empty* response) override;
  Status phase_tune(ServerContext* context, const PhaseTunings* request,
		   Empty* response) override;
  Status set_tone(ServerContext* context, const CalibrationTone* request,
		   Empty* response) override;
  Status clear_tone(ServerContext* context, const Empty* request,
		   Empty* response) override;
  Status hold_tone(ServerContext* context, const ChannelFlags* request,
		   Empty* response) override;
  Status free_tone(ServerContext* context, const ChannelFlags* request,
		   Empty* response) override;
  Status get_hold_state(ServerContext* context, const Empty* request,
		   ChannelFlags* response) override;
  Status set_enh_tone(ServerContext* context, const EnhancedTone* request,
		   Empty* response) override;
  Status control_enh_sweep(ServerContext* context, const SweepControl* request,
		   Empty* response) override;
  Status clear_enh_tone(ServerContext* context, const Empty* request,
		   Empty* response) override;
  Status set_sync_map(ServerContext* context, const SyncMapping* request,
		   Empty* response) override;
  Status set_sync_delay(ServerContext* context, const SyncDelay* request,
		   Empty* response) override;
  Status set_chan_delay(ServerContext* context, const ChanDelay* request,
		   Empty* response) override;
  Status get_status(ServerContext* context, const Empty* request,
           SignalPathStatus* response) override;
  Status clear_phase(ServerContext* context, const Empty* request, 
		     Empty* response) override;
  Status auto_clear(ServerContext* context, const BoolValue* request, 
		    Empty* response) override;
 private:
  class Internal;
  Internal *m_pImpl;
};
