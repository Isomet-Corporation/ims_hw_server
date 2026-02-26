/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server - Player
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_player.cc $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2021-09-22 17:29:58 +0100 (Wed, 22 Sep 2021) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 509 $
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

#include "ims_hw_server_player.h"

#include <atomic>

#include "IMSTypeDefs.h"
#include "SignalPath.h"

using namespace iMS;

class PlaybackSupervisor : public IEventHandler
{
private:
	std::atomic<bool> m_playing{ false };
	std::atomic<bool> m_complete{ false };
	std::atomic<int> m_progress{ -1 };
public:
	void EventAction(void* sender, const int message, const int param)
	{
		switch (message)
		{
		case (ImagePlayerEvents::IMAGE_STARTED): {
			std::cout << "Image Playback Started" << std::endl;
			m_playing.store(true);
			m_complete.store(false);
			break;
		}
		case (ImagePlayerEvents::POINT_PROGRESS): {
			//	std::cout << "Progress: " << param << std::endl; 
			m_progress.store(param);
			break;
		}
		case (ImagePlayerEvents::IMAGE_FINISHED): {
			std::cout << "Image Playback Finished" << std::endl;
			m_playing.store(false);
			m_complete.store(true);
			break;
		}
		}
	}
	bool Playing() const { return m_playing.load(); };
	bool Complete() const { return m_complete.load(); };
	int Progress() const { return m_progress.load(); };
};

class PlaybackHandler {
public:
	PlaybackHandler(std::shared_ptr<IMSServerState> state) : m_state(state) {}
	~PlaybackHandler() {
		if (player != nullptr) {
			player->ImagePlayerEventUnsubscribe(ImagePlayerEvents::POINT_PROGRESS, ps.get());
			player->ImagePlayerEventUnsubscribe(ImagePlayerEvents::IMAGE_STARTED, ps.get());
			player->ImagePlayerEventUnsubscribe(ImagePlayerEvents::IMAGE_FINISHED, ps.get());

			player.reset(nullptr);
			ps.reset(nullptr);
		}
	}

	bool StartPlaybackInternalClock(const iMS::ImageTableEntry& ite, const iMS::ImagePlayer::PlayConfiguration& cfg, iMS::kHz ClockRate, iMS::ImagePlayer::ImageTrigger start_trig);
	bool StartPlaybackExternalClock(const iMS::ImageTableEntry& ite, const iMS::ImagePlayer::PlayConfiguration& cfg, int ExtDivide, iMS::ImagePlayer::ImageTrigger start_trig);
	void StopPlayback(iMS::ImagePlayer::StopStyle stop);

	bool IsPlaying() { if (ps != nullptr) return ps->Playing(); else return false; }
	bool IsComplete() { if (ps != nullptr) return ps->Complete(); else return false; }
	int Progress() {
		if (ps != nullptr) {
			if (player != nullptr) {
				bool ret = player->GetProgress();
				if (!ret) {
					//std::cout << "GetProgress() call failed!" << std::endl;
					return -1;
				}
			}
			return ps->Progress();
		}
		else return -1;
	}
private:
	std::unique_ptr<ImagePlayer> player;
	std::unique_ptr<PlaybackSupervisor> ps;
	std::shared_ptr<IMSServerState> m_state;
};

bool PlaybackHandler::StartPlaybackInternalClock(const iMS::ImageTableEntry& ite, const iMS::ImagePlayer::PlayConfiguration& cfg, iMS::kHz ClockRate, iMS::ImagePlayer::ImageTrigger start_trig) {
    bool res = false;
    auto ims = m_state->get();
    if (ims->Open()) {
        if (player != nullptr) {
            if (player->GetProgress()) return false; // Playback in progress!

            player->ImagePlayerEventUnsubscribe(ImagePlayerEvents::POINT_PROGRESS, ps.get());
            player->ImagePlayerEventUnsubscribe(ImagePlayerEvents::IMAGE_STARTED, ps.get());
            player->ImagePlayerEventUnsubscribe(ImagePlayerEvents::IMAGE_FINISHED, ps.get());

            player.reset(nullptr);
            ps.reset(nullptr);
        }
        player.reset(new ImagePlayer(ims, ite, cfg, ClockRate));

        // Terminate any outstanding playbacks
        player->Stop(ImagePlayer::StopStyle::IMMEDIATELY);

        // Reset Channel Phase
        {
        SignalPath sp(ims);
        sp.PhaseResync();
        }

        ps.reset(new PlaybackSupervisor());
        player->ImagePlayerEventSubscribe(ImagePlayerEvents::POINT_PROGRESS, ps.get());
        player->ImagePlayerEventSubscribe(ImagePlayerEvents::IMAGE_STARTED, ps.get());
        player->ImagePlayerEventSubscribe(ImagePlayerEvents::IMAGE_FINISHED, ps.get());

        res = player->Play(start_trig);
        if (res) {
        std::cout << "Starting Playback" << std::endl;
        }
        else {
        std::cout << "Playback Failed" << std::endl;
        }
        // Get first progress report
        player->GetProgress();
    }
	return res;
}

bool PlaybackHandler::StartPlaybackExternalClock(const iMS::ImageTableEntry& ite, const iMS::ImagePlayer::PlayConfiguration& cfg, int ExtDivide, iMS::ImagePlayer::ImageTrigger start_trig) {
    bool res = false;
    auto ims = m_state->get();
    if (ims->Open()) {
        if (player != nullptr) {
            if (player->GetProgress()) return false; // Playback in progress!

            player->ImagePlayerEventUnsubscribe(ImagePlayerEvents::POINT_PROGRESS, ps.get());
            player->ImagePlayerEventUnsubscribe(ImagePlayerEvents::IMAGE_STARTED, ps.get());
            player->ImagePlayerEventUnsubscribe(ImagePlayerEvents::IMAGE_FINISHED, ps.get());

            player.reset(nullptr);
            ps.reset(nullptr);
        }
        player.reset(new ImagePlayer(ims, ite, cfg, ExtDivide));

        // Terminate any outstanding playbacks
        player->Stop(ImagePlayer::StopStyle::IMMEDIATELY);

        // Reset Channel Phase
        {
        SignalPath sp(ims);
        sp.PhaseResync();
        }

        ps.reset(new PlaybackSupervisor());
        player->ImagePlayerEventSubscribe(ImagePlayerEvents::POINT_PROGRESS, ps.get());
        player->ImagePlayerEventSubscribe(ImagePlayerEvents::IMAGE_STARTED, ps.get());
        player->ImagePlayerEventSubscribe(ImagePlayerEvents::IMAGE_FINISHED, ps.get());

        std::cout << "Starting Playback" << std::endl;
        res = player->Play(start_trig);

        // Get first progress report
        player->GetProgress();
    }
	return res;
}

void PlaybackHandler::StopPlayback(iMS::ImagePlayer::StopStyle stop) {
    auto ims = m_state->get();
    if (ims->Open()) {
        	if (player == nullptr) {
            player.reset(new ImagePlayer(ims, Image()));
        }
        player->Stop(stop);
    }
}

ImagePlayerServiceImpl::ImagePlayerServiceImpl(std::shared_ptr<IMSServerState> state) : image_player::Service(), m_state(state), p_pbh(new PlaybackHandler(state)) {}

ImagePlayerServiceImpl::~ImagePlayerServiceImpl() {
	delete p_pbh;
	p_pbh = nullptr;
}

bool ImagePlayerServiceImpl::StartPlayback(const PlayConfiguration* request, iMS::ImagePlayer::ImageTrigger trig)
{
    auto ims = m_state->get();
    bool ok = false;
    if (ims->Open()) {    

        // Enable or Disable Compensation
        SignalPath* sp = new SignalPath(ims);
        sp->EnableImagePathCompensation(
            request->ampl_comp_enabled() ? iMS::SignalPath::Compensation::ACTIVE : iMS::SignalPath::Compensation::BYPASS,
            request->phase_comp_enabled() ? iMS::SignalPath::Compensation::ACTIVE : iMS::SignalPath::Compensation::BYPASS
        );

        // Stop any lingering Tone Buffer Playback
        sp->UpdateLocalToneBuffer(iMS::SignalPath::ToneBufferControl::OFF);

        delete sp;

        // Retrieve Image Table Entry from index table
        if (ims->Ctlr().GetCap().FastImageTransfer) {}
        ImageTableViewer itv(ims);
        for (int i = 0; i < itv.Entries(); i++) {
            if (strncmp((const char *)itv[i].UUID().data(), request->img().uuid().c_str(), 16) == 0) {
                iMS::ImagePlayer::PlayConfiguration cfg;
                switch (request->trig()) {
                case PlayConfiguration::TRIG_CONTINUOUS: cfg.trig = ImagePlayer::ImageTrigger::CONTINUOUS; break;
                case PlayConfiguration::TRIG_HOST: cfg.trig = ImagePlayer::ImageTrigger::HOST; break;
                case PlayConfiguration::TRIG_EXTERNAL: cfg.trig = ImagePlayer::ImageTrigger::EXTERNAL; break;
                case PlayConfiguration::TRIG_POST_DELAY: cfg.trig = ImagePlayer::ImageTrigger::POST_DELAY; break;
                }
                switch (request->rpts()) {
                case PlayConfiguration::RPTS_NONE: cfg.rpts = ImagePlayer::Repeats::NONE; break;
                case PlayConfiguration::RPTS_PROGRAM: cfg.rpts = ImagePlayer::Repeats::PROGRAM; break;
                case PlayConfiguration::RPTS_FOREVER: cfg.rpts = ImagePlayer::Repeats::FOREVER; break;
                }
                cfg.n_rpts = request->n_rpts();
                cfg.del = ImagePlayer::PlayConfiguration::post_delay(static_cast<uint16_t>((request->post_delay() + 0.00005) * 10000.0));
                if (request->int_ext() == PlayConfiguration::CLK_INTERNAL) {
                    cfg.int_ext = ImagePlayer::PointClock::INTERNAL;
                    ok = p_pbh->StartPlaybackInternalClock(itv[i], cfg, iMS::kHz(request->clock_rate()), trig);
                }
                else {
                    cfg.int_ext = ImagePlayer::PointClock::EXTERNAL;
                    cfg.clk_pol = (request->clk_pol() == PlayConfiguration::POL_NORMAL) ?
                        ImagePlayer::Polarity::NORMAL : ImagePlayer::Polarity::INVERSE;
                    cfg.trig_pol = (request->trig_pol() == PlayConfiguration::POL_NORMAL) ?
                        ImagePlayer::Polarity::NORMAL : ImagePlayer::Polarity::INVERSE;
                    ok = p_pbh->StartPlaybackExternalClock(itv[i], cfg, request->ext_divide(), trig);
                }
                if (ok) this->idx = itv[i].Handle();
            } 
        }
    }
	return ok;
}

Status ImagePlayerServiceImpl::play(ServerContext* context, const PlayConfiguration* request,
	PlaybackStatus* response) {
    auto ims = m_state->get();
    if (ims->Open()) {
		if (!this->StartPlayback(request, iMS::ImagePlayer::ImageTrigger::CONTINUOUS)) {
			response->set_status(PlaybackStatus::PB_ERROR);
		}
		else {
			response->set_status(PlaybackStatus::PB_PENDING);
		}
	}
	return Status::OK;
}

Status ImagePlayerServiceImpl::play_on_ext_trigger(ServerContext* context, const PlayConfiguration* request,
	PlaybackStatus* response) {
    auto ims = m_state->get();
    if (ims->Open()) {
		if (!this->StartPlayback(request, iMS::ImagePlayer::ImageTrigger::EXTERNAL)) {
			response->set_status(PlaybackStatus::PB_ERROR);
		}
		else {
			response->set_status(PlaybackStatus::PB_PENDING);
		}
	}
	return Status::OK;
}

Status ImagePlayerServiceImpl::stop(ServerContext* context, const Empty* request,
	PlaybackStatus* response) {
    auto ims = m_state->get();
    if (ims->Open()) {
		p_pbh->StopPlayback(iMS::ImagePlayer::StopStyle::GRACEFULLY);
		response->set_status(PlaybackStatus::PB_STOPPED);
	}
	return Status::OK;
}

Status ImagePlayerServiceImpl::stop_immediately(ServerContext* context, const Empty* request,
	PlaybackStatus* response) {
    auto ims = m_state->get();
    if (ims->Open()) {
		p_pbh->StopPlayback(iMS::ImagePlayer::StopStyle::IMMEDIATELY);
		response->set_status(PlaybackStatus::PB_STOPPED);
	}
	return Status::OK;
}

Status ImagePlayerServiceImpl::plstatus(ServerContext* context, const Empty* request,
	PlaybackStatus* response) {
    auto ims = m_state->get();
    if (ims->Open()) {
		if (p_pbh->IsPlaying()) response->set_status(PlaybackStatus::PB_PLAYING);
		else if (p_pbh->IsComplete()) response->set_status(PlaybackStatus::PB_FINISHED);
		else response->set_status(PlaybackStatus::PB_STOPPED);
		response->mutable_id()->set_id(idx);
		response->set_progress(p_pbh->Progress());
	}
	return Status::OK;
}

