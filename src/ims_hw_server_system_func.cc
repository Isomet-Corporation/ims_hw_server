/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server - System Func
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_signal_path.cc $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2023-05-30 11:33:43 +0100 (Tue, 30 May 2023) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 563 $
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

#include "ims_hw_server_system_func.h"

#include "IMSTypeDefs.h"
#include "SystemFunc.h"

#include <cstdint>
#include <chrono>

using namespace iMS;

class SystemFuncServiceImpl::Internal
{
public:
  Internal(std::shared_ptr<IMSServerState> state) : m_state(state), sysfunc(nullptr) {}
  ~Internal() {if (sysfunc != nullptr) delete sysfunc;}
  std::shared_ptr<IMSServerState> m_state;
  bool _clockgenActive;

  bool OKtoProceed() {
    auto ims = m_state->get();
    if (!ims->Open()) {
      return false;
    }
    if (sysfunc == nullptr) {
      sysfunc = new SystemFunc(ims);
    }
    return true; 
  }

  SystemFunc* sysfunc;

  // Sync Delay parameters
};

SystemFuncServiceImpl::SystemFuncServiceImpl(std::shared_ptr<IMSServerState> state) : m_pImpl(new Internal(state)) 
{
  if (m_pImpl->OKtoProceed()) {
    // Initialise with no clock gen disabled
    m_pImpl->sysfunc->DisableClockGenerator();
  }
  m_pImpl->_clockgenActive = false;
}

SystemFuncServiceImpl::~SystemFuncServiceImpl() 
{
  delete m_pImpl;
  m_pImpl = nullptr;
}


  Status SystemFuncServiceImpl::en_clock_gen(ServerContext* context, const ClockGenerator* request,
					  Empty* response) {
    if (m_pImpl->OKtoProceed()) {

      ClockGenConfiguration ckgen, ckgen_old;

      ckgen_old = m_pImpl->sysfunc->GetClockGenConfiguration();

      ckgen.ClockFreq = kHz(request->clockfreq());
      ckgen.OscPhase = Degrees(request->oscphase());
      ckgen.DutyCycle = Percent(request->dutycycle());
      ckgen.AlwaysOn = request->alwayson();
      ckgen.GenerateTrigger = request->generatetrigger();
      ckgen.ClockPolarity = request->clockpolarity() ? Polarity::INVERSE : Polarity::NORMAL;
      ckgen.TrigPolarity = request->trigpolarity() ? Polarity::INVERSE : Polarity::NORMAL;

      m_pImpl->sysfunc->ConfigureClockGenerator(ckgen);

      if (m_pImpl->_clockgenActive) {
        std::cout << "Clock generator update:";
        if (ckgen_old.ClockFreq != ckgen.ClockFreq) std::cout << " Freq = " << request->clockfreq();
        if (ckgen_old.OscPhase != ckgen.OscPhase) std::cout << " Phase = " << request->oscphase();
        if (ckgen_old.DutyCycle != ckgen.DutyCycle) std::cout << " Duty = " << request->dutycycle();
        if (ckgen_old.AlwaysOn != ckgen.AlwaysOn) std::cout << " Always On = " << request->alwayson();
        if (ckgen_old.GenerateTrigger != ckgen.GenerateTrigger) std::cout << " Gen Trig = " << request->generatetrigger();
        if (ckgen_old.ClockPolarity != ckgen.ClockPolarity) std::cout << " Clk Pol = " << request->clockpolarity();
        if (ckgen_old.TrigPolarity != ckgen.TrigPolarity) std::cout << " Trig Pol = " << request->trigpolarity();
        std::cout << std::endl;
      } else {
        std::cout << "Clock generator enabled" << std::endl;
        std::cout << "  -> freq = " << request->clockfreq() << " KHz" << std::endl;
        std::cout << "  -> duty = " << request->dutycycle() << " %" << std::endl;
        std::cout << "  -> phs  = " << request->oscphase() << " deg" << std::endl;
        std::cout << "  -> always on = " << request->alwayson() << std::endl;
        std::cout << "  -> gen trig = " << request->generatetrigger() << std::endl;
	if (request->clockpolarity())
	  std::cout << "  -> clk pol = INVERSE" << std::endl;
	else
	  std::cout << "  -> clk pol = NORMAL" << std::endl;
	if (request->trigpolarity())
	  std::cout << "  -> trig pol = INVERSE" << std::endl;
	else
	  std::cout << "  -> trig pol = NORMAL" << std::endl;
      }

      m_pImpl->_clockgenActive = true;
    }
    return Status::OK;
  }

  Status SystemFuncServiceImpl::dis_clock_gen(ServerContext* context, const Empty* request,
					  Empty* response) {
    if (m_pImpl->OKtoProceed()) {

      m_pImpl->sysfunc->DisableClockGenerator();
      std::cout << "Clock Generator disabled" << std::endl;
      m_pImpl->_clockgenActive = false;
    }
    return Status::OK;
  }
