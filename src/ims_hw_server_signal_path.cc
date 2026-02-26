/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server - Signal Path
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_signal_path.cc $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2024-11-07 15:35:26 +0000 (Thu, 07 Nov 2024) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 631 $
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

#include "ims_hw_server_signal_path.h"

#include "IMSTypeDefs.h"
#include "SignalPath.h"
#include "SystemFunc.h"
#include "Auxiliary.h"

#include <cstdint>
#include <chrono>

using namespace iMS;

class SignalPathServiceImpl::Internal
{
public:
  Internal(std::shared_ptr<IMSServerState> state) : m_state(state), sigpath(nullptr) {}
  ~Internal() {if (sigpath != nullptr) delete sigpath;}
  std::shared_ptr<IMSServerState> m_state;
  bool _enhTonePlaying;
  bool _calibTonePlaying;

  bool OKtoProceed() {
    auto ims = m_state->get();
    if (!ims->Open()) {
      return false;
    }
    if (sigpath == nullptr) {
      sigpath = new SignalPath(ims);
    }
    return true; 
  }

  SignalPath* sigpath;

  // Sync Delay parameters
  std::chrono::nanoseconds sync_delay;
  std::chrono::nanoseconds chan12_delay;
  std::chrono::nanoseconds chan34_delay;
  std::chrono::nanoseconds pulse_len;
  iMS::SignalPath::SYNC_DIG_MODE pulse_mode = iMS::SignalPath::SYNC_DIG_MODE::LEVEL;
  bool invert = true;
};

SignalPathServiceImpl::SignalPathServiceImpl(std::shared_ptr<IMSServerState> state) : m_pImpl(new Internal(state)) 
{
  if (m_pImpl->OKtoProceed()) {
    // Initialise with no tone playing
    m_pImpl->sigpath->ClearEnhancedToneMode();
    for (int i=RFChannel::min; i<RFChannel::max; i++) {
	m_pImpl->sigpath->ClearEnhancedToneChannel(i);
    }

    m_pImpl->sigpath->ClearTone();
  }
  m_pImpl->_enhTonePlaying = false;
  m_pImpl->_calibTonePlaying = false;
  m_pImpl->sync_delay = std::chrono::nanoseconds::max();
  m_pImpl->chan12_delay = std::chrono::nanoseconds::max();
  m_pImpl->chan34_delay = std::chrono::nanoseconds::max();
  m_pImpl->pulse_len = std::chrono::nanoseconds::max();
}

SignalPathServiceImpl::~SignalPathServiceImpl() 
{
  delete m_pImpl;
  m_pImpl = nullptr;
}


  Status SignalPathServiceImpl::dds_power(ServerContext* context, const PowerSettings* request,
					  Empty* response) {
    if (m_pImpl->OKtoProceed()) {
        auto ims = m_pImpl->m_state->get();
      m_pImpl->sigpath->UpdateDDSPowerLevel(Percent(request->ddspower()));
      if ((ims->Synth().Model() == "iMS4") || (ims->Synth().Model() == "iMS4b")) {
	std::cout << "Set DDS Power: DDS(" << request->ddspower() << "%) Wiper 1(" << request->wiper1power() << ") Wiper 2(" << request->wiper2power() <<
	  ") Src=" << PowerSettings::AmplitudeControl_Name(request->src()) << std::endl;
	m_pImpl->sigpath->UpdateRFAmplitude(SignalPath::AmplitudeControl::WIPER_1, Percent(request->wiper1power()));
	m_pImpl->sigpath->UpdateRFAmplitude(SignalPath::AmplitudeControl::WIPER_2, Percent(request->wiper2power()));
	switch (request->src())
	  {
	  case PowerSettings::OFF: m_pImpl->sigpath->SwitchRFAmplitudeControlSource(SignalPath::AmplitudeControl::OFF); break;
	  case PowerSettings::EXTERNAL: m_pImpl->sigpath->SwitchRFAmplitudeControlSource(SignalPath::AmplitudeControl::EXTERNAL); break;
	  case PowerSettings::WIPER_1: m_pImpl->sigpath->SwitchRFAmplitudeControlSource(SignalPath::AmplitudeControl::WIPER_1); break;
	  case PowerSettings::WIPER_2: m_pImpl->sigpath->SwitchRFAmplitudeControlSource(SignalPath::AmplitudeControl::WIPER_2); break;
	  }
      } else {
	std::cout << "Set DDS Power: DDS(" << request->ddspower() << "%)" << std::endl;
      }
    }
    return Status::OK;
  }

static inline std::string ChanString(RFChannel chan) {return (chan.IsAll()) ? std::string("All Channels") : std::string("Channel ") + std::to_string(chan);}

  Status SignalPathServiceImpl::channel_power(ServerContext* context, const ChannelPowerSettings* request,
					  Empty* response) {
    if (m_pImpl->OKtoProceed()) {
        auto ims = m_pImpl->m_state->get();
      if ((ims->Synth().Model() == "iMS4") || (ims->Synth().Model() == "iMS4b")) {
	std::cout << "Independent channel power settings not supported on this version of Synthesiser. Please use dds_power() instead" << std::endl;
      } else {
	RFChannel chan = RFChannel::all;
	if (request->channel() >= 1 && request->channel() <= 4) {
	  chan = RFChannel(request->channel());
	}
	if (request->src() == ChannelPowerSettings::INTERNAL) {
	  m_pImpl->sigpath->UpdateRFAmplitude(SignalPath::AmplitudeControl::INDEPENDENT, Percent(request->powerlevel()), chan);
	  m_pImpl->sigpath->SwitchRFAmplitudeControlSource(SignalPath::AmplitudeControl::INDEPENDENT, chan); 
	  std::cout << "Set " << ChanString(chan) << " Level: (" << request->powerlevel() << "%)" << std::endl;
	} else {
	  m_pImpl->sigpath->UpdateRFAmplitude(SignalPath::AmplitudeControl::EXTERNAL, Percent(0.0), chan);
	  m_pImpl->sigpath->SwitchRFAmplitudeControlSource(SignalPath::AmplitudeControl::EXTERNAL, chan); 
	  std::cout << "Set " << ChanString(chan) << ": External Modulation" << std::endl;
	}
      }
    }
    return Status::OK;
  }

static inline std::string EnString(bool en) {return en ? std::string("Enable") : std::string("Disable");}

  Status SignalPathServiceImpl::rf_enable(ServerContext* context, const MasterSwitch* request,
		   Empty* response) {
    if (m_pImpl->OKtoProceed()) {
        auto ims = m_pImpl->m_state->get();
    std::cout << "Master Switch Enable: Amplifier(" << EnString(request->amplifieren()) << 
      ") RF Channel 1/2(" << EnString(request->rfchannel12en()) << ") RF Channel 3/4(" <<
      EnString(request->rfchannel34en()) << ") External Eqt(" << EnString(request->externalen()) << ")" << std::endl;
      SystemFunc sf(ims);
      sf.EnableAmplifier(request->amplifieren());
      sf.EnableRFChannels(request->rfchannel12en(), request->rfchannel34en());
      sf.EnableExternal(request->externalen());
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::phase_tune(ServerContext* context, const PhaseTunings* request,
		   Empty* response) {
    std::cout << "Set Phase Tuning" << std::endl;
    if (m_pImpl->OKtoProceed()) {
      m_pImpl->sigpath->UpdatePhaseTuning(RFChannel(1), Degrees(request->ch1()));
      m_pImpl->sigpath->UpdatePhaseTuning(RFChannel(2), Degrees(request->ch2()));
      m_pImpl->sigpath->UpdatePhaseTuning(RFChannel(3), Degrees(request->ch3()));
      m_pImpl->sigpath->UpdatePhaseTuning(RFChannel(4), Degrees(request->ch4()));
      m_pImpl->sigpath->SetChannelReversal(request->reverse());
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::set_tone(ServerContext* context, const CalibrationTone* request,
		   Empty* response) {
    if (m_pImpl->OKtoProceed()) {
      FAP fap(request->freq(), request->ampl(), request->phs());
      std::cout << "Set Calibration Tone:" << fap.freq << "MHz " << fap.ampl << "% " << fap.phase << "deg" << std::endl;
      m_pImpl->sigpath->SetCalibrationTone(fap);
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::clear_tone(ServerContext* context, const Empty* request,
		   Empty* response) {
    if (m_pImpl->OKtoProceed()) {
      std::cout << "Clear Calibration Tone" << std::endl;
      // First set tone to zero amplitude so no tone residue after leaving STM
      FAP fap(0.0, 0.0, 0.0);
      m_pImpl->sigpath->SetCalibrationTone(fap);
      // Then leave STM
      m_pImpl->sigpath->ClearTone();
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::hold_tone(ServerContext* context, const ChannelFlags* request,
		   Empty* response) {
    if (m_pImpl->OKtoProceed()) {
      std::cout << "Calibration Tone HELD on {" << (request->ch1() ? " Ch1" : "")
		<< (request->ch2() ? " Ch2" : "")
		<< (request->ch3() ? " Ch3" : "")
		<< (request->ch4() ? " Ch4" : "")
		<< " }" << std::endl;

      if (request->ch1()) m_pImpl->sigpath->SetCalibrationChannelLock(RFChannel(1));
      if (request->ch2()) m_pImpl->sigpath->SetCalibrationChannelLock(RFChannel(2));
      if (request->ch3()) m_pImpl->sigpath->SetCalibrationChannelLock(RFChannel(3));
      if (request->ch4()) m_pImpl->sigpath->SetCalibrationChannelLock(RFChannel(4));
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::free_tone(ServerContext* context, const ChannelFlags* request,
		   Empty* response) {
    if (m_pImpl->OKtoProceed()) {
      std::cout << "Calibration Tone FREED on {" << (request->ch1() ? " Ch1" : "")
		<< (request->ch2() ? " Ch2" : "")
		<< (request->ch3() ? " Ch3" : "")
		<< (request->ch4() ? " Ch4" : "")
		<< " }" << std::endl;

      if (request->ch1()) m_pImpl->sigpath->ClearCalibrationChannelLock(RFChannel(1));
      if (request->ch2()) m_pImpl->sigpath->ClearCalibrationChannelLock(RFChannel(2));
      if (request->ch3()) m_pImpl->sigpath->ClearCalibrationChannelLock(RFChannel(3));
      if (request->ch4()) m_pImpl->sigpath->ClearCalibrationChannelLock(RFChannel(4));
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::get_hold_state(ServerContext* context, const Empty* request,
			ChannelFlags* response) {
    if (m_pImpl->OKtoProceed()) {
      response->clear_ch1();
      response->clear_ch2();
      response->clear_ch3();
      response->clear_ch4();

      if (m_pImpl->sigpath->GetCalibrationChannelLockState(RFChannel(1))) response->set_ch1(true);
      if (m_pImpl->sigpath->GetCalibrationChannelLockState(RFChannel(2))) response->set_ch2(true);
      if (m_pImpl->sigpath->GetCalibrationChannelLockState(RFChannel(3))) response->set_ch3(true);
      if (m_pImpl->sigpath->GetCalibrationChannelLockState(RFChannel(4))) response->set_ch4(true);
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::set_enh_tone(ServerContext* context, const EnhancedTone* request,
		   Empty* response) {
    if (m_pImpl->OKtoProceed()) {
        auto ims = m_pImpl->m_state->get();
      if ( ((ims->Synth().Model() == "iMS4") && (ims->Synth().GetVersion().revision < 57)) || 
      ((ims->Synth().Model() == "iMS4b") && (ims->Synth().GetVersion().revision < 68)) ||
      ((ims->Synth().Model() == "iMS4c") && (ims->Synth().GetVersion().revision < 68)) ||
      ((ims->Synth().Model() == "iMS4d") && (ims->Synth().GetVersion().revision < 148)) ) {
	std::cout << "Enhanced Tone Mode not supported on " << ims->Synth().Model() << 
	  " f/w version " << ims->Synth().GetVersion().revision << std::endl;
	return Status::OK;
      }

      // Enough data to set all 4 channels
      int chan_i = 0;
      SweepTone swt[RFChannel::max];
      while (chan_i < request->chan_size()) {
	const EnhancedTone_ChannelSweep& sweep = request->chan(chan_i);
	swt[chan_i].start().freq = sweep.start_freq();
	swt[chan_i].end().freq = sweep.end_freq();
	swt[chan_i].start().ampl = sweep.ampl();
	swt[chan_i].start().phase = sweep.start_phs();
	swt[chan_i].end().phase = sweep.end_phs();
	switch(sweep.mode())
	  {
	    case EnhancedTone_ChannelSweep::NoSweep: swt[chan_i].mode() = ENHANCED_TONE_MODE::NO_SWEEP; break;
 	    case EnhancedTone_ChannelSweep::FrequencyDwell: swt[chan_i].mode() = ENHANCED_TONE_MODE::FREQUENCY_DWELL; break;
 	    case EnhancedTone_ChannelSweep::FrequencyNoDwell: swt[chan_i].mode() = ENHANCED_TONE_MODE::FREQUENCY_NO_DWELL; break;
 	    case EnhancedTone_ChannelSweep::FrequencyFastMod: swt[chan_i].mode() = ENHANCED_TONE_MODE::FREQUENCY_FAST_MOD; break;
 	    case EnhancedTone_ChannelSweep::PhaseDwell: swt[chan_i].mode() = ENHANCED_TONE_MODE::PHASE_DWELL; break;
 	    case EnhancedTone_ChannelSweep::PhaseNoDwell: swt[chan_i].mode() = ENHANCED_TONE_MODE::PHASE_NO_DWELL; break;
 	    case EnhancedTone_ChannelSweep::PhaseFastMod: swt[chan_i].mode() = ENHANCED_TONE_MODE::PHASE_FAST_MOD; break;
 	    default: swt[chan_i].mode() = ENHANCED_TONE_MODE::NO_SWEEP; break;
	  }
	swt[chan_i].up_ramp() = std::chrono::microseconds(sweep.up_ramp());
	swt[chan_i].down_ramp() = std::chrono::microseconds(sweep.down_ramp());
	swt[chan_i].n_steps() = sweep.n_steps();
	switch(sweep.dac_ref())
	  {
	    case EnhancedTone_ChannelSweep::FullScale: swt[chan_i].scaling() = DAC_CURRENT_REFERENCE::FULL_SCALE; break;
	    case EnhancedTone_ChannelSweep::HalfScale: swt[chan_i].scaling() = DAC_CURRENT_REFERENCE::HALF_SCALE; break;
	    case EnhancedTone_ChannelSweep::QuarterScale: swt[chan_i].scaling() = DAC_CURRENT_REFERENCE::QUARTER_SCALE; break;
	    case EnhancedTone_ChannelSweep::EighthScale: swt[chan_i].scaling() = DAC_CURRENT_REFERENCE::EIGHTH_SCALE; break;
	    default: swt[chan_i].scaling() = DAC_CURRENT_REFERENCE::FULL_SCALE; break;
	  }
	  
	chan_i++;

	if (chan_i >= RFChannel::max) break;
      }
      if (request->chan_size() >= RFChannel::max) {
	m_pImpl->sigpath->SetEnhancedToneMode(swt[0], swt[1], swt[2], swt[3]);
	std::cout << "Set Enhanced Tone Mode: " << EnhancedTone_ChannelSweep::Mode_Name(request->chan(0).mode()) << std::endl;
      } else {
	// Set individual channels only
	for (int i=0; i<request->chan_size(); i++) {
	  m_pImpl->sigpath->SetEnhancedToneChannel(RFChannel(i+1), swt[i]);
	  std::cout << "Set Enhanced Tone Channel(" << i+1 << "): " << EnhancedTone_ChannelSweep::Mode_Name(request->chan(0).mode()) << std::endl;
	}
      }
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::control_enh_sweep(ServerContext* context, const SweepControl* request, Empty* response) {
    if (m_pImpl->OKtoProceed()) {
        auto ims = m_pImpl->m_state->get();
      Auxiliary aux(ims);
      if (request->manual_trigger_enable()) {
	uint8_t prfl = 0;
	if (ims->Synth().Model() == "iMS4b") {
	  // reordered channel layout
	  if (request->trig_ch1()) prfl |= 2;
	  if (request->trig_ch2()) prfl |= 1;
	  if (request->trig_ch3()) prfl |= 8;
	  if (request->trig_ch4()) prfl |= 4;
	} else {
	  if (request->trig_ch1()) prfl |= 1;
	  if (request->trig_ch2()) prfl |= 2;
	  if (request->trig_ch3()) prfl |= 4;
	  if (request->trig_ch4()) prfl |= 8;
	}
	aux.SetDDSProfile(Auxiliary::DDS_PROFILE::HOST, prfl);
      } else {
	aux.SetDDSProfile(Auxiliary::DDS_PROFILE::EXTERNAL);
	std::cout << "Linear Sweep Control set to External input" << std::endl;
      }
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::clear_enh_tone(ServerContext* context, const Empty* request,
		   Empty* response) {
    if (m_pImpl->OKtoProceed()) {
      // Do both Mode and 4 Channels
      m_pImpl->sigpath->ClearEnhancedToneMode();
      for (int i=RFChannel::min; i<RFChannel::max; i++) {
	m_pImpl->sigpath->ClearEnhancedToneChannel(i);
      }
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::set_sync_map(ServerContext* context, const SyncMapping* request,
		   Empty* response) {
    SignalPath::SYNC_SINK sink;
    SignalPath::SYNC_SRC src;
    if (m_pImpl->OKtoProceed()) {
      std::cout << "Set Synchronous Output Mapping: " << SyncMapping::SyncSource_Name(request->src()) << " <==> " << SyncMapping::SyncSink_Name(request->sink()) << std::endl;
      switch(request->sink()) {
        case SyncMapping::AnalogA: sink = SignalPath::SYNC_SINK::ANLG_A; break;
        case SyncMapping::AnalogB: sink = SignalPath::SYNC_SINK::ANLG_B; break;
        case SyncMapping::Digital: sink = SignalPath::SYNC_SINK::DIG; break;
      }
      switch(request->src()) {
        case SyncMapping::FrequencyCh1: src = SignalPath::SYNC_SRC::FREQUENCY_CH1; break;
        case SyncMapping::FrequencyCh2: src = SignalPath::SYNC_SRC::FREQUENCY_CH2; break;
        case SyncMapping::FrequencyCh3: src = SignalPath::SYNC_SRC::FREQUENCY_CH3; break;
        case SyncMapping::FrequencyCh4: src = SignalPath::SYNC_SRC::FREQUENCY_CH4; break;
        case SyncMapping::AmplitudeCh1: src = SignalPath::SYNC_SRC::AMPLITUDE_CH1; break;
        case SyncMapping::AmplitudeCh2: src = SignalPath::SYNC_SRC::AMPLITUDE_CH2; break;
        case SyncMapping::AmplitudeCh3: src = SignalPath::SYNC_SRC::AMPLITUDE_CH3; break;
        case SyncMapping::AmplitudeCh4: src = SignalPath::SYNC_SRC::AMPLITUDE_CH4; break;
        case SyncMapping::AmplitudePreCompCh1: src = SignalPath::SYNC_SRC::AMPLITUDE_PRE_COMP_CH1; break;
        case SyncMapping::AmplitudePreCompCh2: src = SignalPath::SYNC_SRC::AMPLITUDE_PRE_COMP_CH2; break;
        case SyncMapping::AmplitudePreCompCh3: src = SignalPath::SYNC_SRC::AMPLITUDE_PRE_COMP_CH3; break;
        case SyncMapping::AmplitudePreCompCh4: src = SignalPath::SYNC_SRC::AMPLITUDE_PRE_COMP_CH4; break;
        case SyncMapping::PhaseCh1: src = SignalPath::SYNC_SRC::PHASE_CH1; break;
        case SyncMapping::PhaseCh2: src = SignalPath::SYNC_SRC::PHASE_CH2; break;
        case SyncMapping::PhaseCh3: src = SignalPath::SYNC_SRC::PHASE_CH3; break;
        case SyncMapping::PhaseCh4: src = SignalPath::SYNC_SRC::PHASE_CH4; break;
        case SyncMapping::LookupFieldCh1: src = SignalPath::SYNC_SRC::LOOKUP_FIELD_CH1; break;
        case SyncMapping::LookupFieldCh2: src = SignalPath::SYNC_SRC::LOOKUP_FIELD_CH2; break;
        case SyncMapping::LookupFieldCh3: src = SignalPath::SYNC_SRC::LOOKUP_FIELD_CH3; break;
        case SyncMapping::LookupFieldCh4: src = SignalPath::SYNC_SRC::LOOKUP_FIELD_CH4; break;
        case SyncMapping::ImageAnalogA: src = SignalPath::SYNC_SRC::IMAGE_ANLG_A; break;
        case SyncMapping::ImageAnalogB: src = SignalPath::SYNC_SRC::IMAGE_ANLG_B; break;
        case SyncMapping::ImageDigital: src = SignalPath::SYNC_SRC::IMAGE_DIG; break;
      }
      m_pImpl->sigpath->AssignSynchronousOutput(sink, src);
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::set_chan_delay(ServerContext* context, const ChanDelay* request,
		      Empty* response) {
    if (m_pImpl->OKtoProceed()) {
      std::chrono::nanoseconds len12 = std::chrono::nanoseconds(request->delay12());
      std::chrono::nanoseconds len34 = std::chrono::nanoseconds(request->delay34());

      if ((len12 != m_pImpl->chan12_delay) || (len34 != m_pImpl->chan34_delay))
      {
        m_pImpl->chan12_delay = len12;
        m_pImpl->chan34_delay = len34;
        std::cout << "Set RF Channel Pair Delays: 1/2 (X) = " << len12.count() << "ns. 3/4 (Y) = " << len34.count() << "ns. " << std::endl;

        m_pImpl->sigpath->SetChannelDelay(len12, len34);
      }
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::set_sync_delay(ServerContext* context, const SyncDelay* request,
					     Empty* response) {
    if (m_pImpl->OKtoProceed()) {
      std::chrono::nanoseconds len = std::chrono::nanoseconds(request->delay());
      std::chrono::nanoseconds pulse = std::chrono::nanoseconds(request->pulselength());

      bool IsPulse = (request->pulselength() > 0);
      bool Invert = (request->invert());

      int bits_size = request->pulseenbits().size();
      iMS::SignalPath::SYNC_DIG_MODE pulse_mode = request->pulseenall() ? iMS::SignalPath::SYNC_DIG_MODE::PULSE : iMS::SignalPath::SYNC_DIG_MODE::LEVEL;

      if (len != m_pImpl->sync_delay || pulse != m_pImpl->pulse_len)
      {
        m_pImpl->sync_delay = len;
        m_pImpl->pulse_len = pulse;
        std::cout << "Set Synchronous Output Delay: " << len.count() << "ns. ";
        std::cout << "Pulsed = " << IsPulse;
        if (IsPulse) {
          std::cout << ". Width = " << pulse.count() << "ns.";
        }
        std::cout << std::endl;

        m_pImpl->sigpath->ConfigureSyncDigitalOutput(len, pulse);
      }

      if ( (pulse_mode == iMS::SignalPath::SYNC_DIG_MODE::LEVEL) || 
           (m_pImpl->pulse_mode == iMS::SignalPath::SYNC_DIG_MODE::LEVEL))
      {
        m_pImpl->pulse_mode = pulse_mode;
        m_pImpl->sigpath->SyncDigitalOutputMode(pulse_mode);
        std::cout << "Sync Output Mode: " << ((pulse_mode == iMS::SignalPath::SYNC_DIG_MODE::PULSE) ? "PULSE" : "LEVEL") << std::endl;
      }

      if (m_pImpl->pulse_mode == iMS::SignalPath::SYNC_DIG_MODE::LEVEL)
      {
        for (int i=0; i<bits_size; i++)
        {
          if (request->pulseenbits(i))
          {
            m_pImpl->sigpath->SyncDigitalOutputMode(iMS::SignalPath::SYNC_DIG_MODE::PULSE, i);
          }
        }
      }

      if (Invert != m_pImpl->invert)
      {
        m_pImpl->invert = Invert;
        m_pImpl->sigpath->SyncDigitalOutputInvert(Invert);
        std::cout << "Sync Output " << (Invert ? "" : "Not ") << "Inverted" << std::endl;
      }
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::get_status(ServerContext* context, const Empty* request,
					   SignalPathStatus* response) {
    response->set_calibrationtoneplaying(m_pImpl->_calibTonePlaying);
    response->set_enhancedtoneplaying(m_pImpl->_enhTonePlaying);
    return Status::OK;
  }

  Status SignalPathServiceImpl::clear_phase(ServerContext* context, const Empty* request, 
		     Empty* response)
  {
    if (m_pImpl->OKtoProceed()) {
      std::cout << ">> Manual Phase Resync <<" << std::endl;
      m_pImpl->sigpath->PhaseResync();
    }
    return Status::OK;
  }

  Status SignalPathServiceImpl::auto_clear(ServerContext* context, const BoolValue* request, 
		    Empty* response) 
  {
    if (m_pImpl->OKtoProceed()) {
      std::cout << "Auto Channel Phase Resync : " << std::boolalpha << request->value() << std::endl;
      m_pImpl->sigpath->AutoPhaseResync(request->value());
    }
    return Status::OK;
  }
