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

#include "system_func.grpc.pb.h"
#include "IMSSystem.h"

using grpc::Status;
using grpc::ServerContext;
using google::protobuf::Empty;

// Defined Messages
using ims_hw_server::ClockGenerator;

// Defined Services
using ims_hw_server::sys_func;

class SystemFuncServiceImpl final : public sys_func::Service {
 public:
  SystemFuncServiceImpl(std::shared_ptr<iMS::IMSSystem> ims);
  ~SystemFuncServiceImpl();

  // The Services
  Status en_clock_gen(ServerContext* context, const ClockGenerator* request,
		   Empty* response) override;
  Status dis_clock_gen(ServerContext* context, const Empty* request,
		   Empty* response) override;
 private:
  class Internal;
  Internal *m_pImpl;
};
