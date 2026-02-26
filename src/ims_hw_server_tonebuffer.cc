/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server - ToneBuffer Downloader
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_tonebuffer.cc $
/ Author     : $Author: dave $
/ Tbufany    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2019-03-12 12:23:33 +0000 (Tue, 12 Mar 2019) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 403 $
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

#include "ims_hw_server_tonebuffer.h"

#include <atomic>

#include "ToneBuffer.h"
#include "IMSTypeDefs.h"

using namespace iMS;

class ToneBufferDownloadSupervisor : public IEventHandler
{
private:
  std::atomic<bool> m_downloading{ true };
  std::atomic<bool> m_error{ false };
public:
  void EventAction(void* sender, const int message, const int param)
  {
    switch (message)
      {
      case (ToneBufferEvents::DOWNLOAD_FINISHED) : {
	std::cout << "ToneBuffer Download Finished!" << std::endl; 
	m_downloading.store(false); 
	break;
      }
      case (ToneBufferEvents::DOWNLOAD_ERROR) : {
	std::cout << "Download error on message " << param << std::endl;
	//	std::cout << "ToneBuffer Download Error!" << std::endl; 
	m_error.store(true);
	//	m_downloading.store(false); 
	break;
      }
     }
  }
  bool Busy() const { return m_downloading.load(); };
  void Reset() { m_downloading.store(true); m_error.store(false); }
  bool Error() const { return m_error.load(); };
};

class ToneBufferDownloadBuffer
{
 public:
  ToneBufferDownloadBuffer(unsigned int first, unsigned int last);
  ~ToneBufferDownloadBuffer();

  unsigned int getContext() const {return m_id;}

  void AssignNextPoint(const iMS::TBEntry& pt);
  bool AllPointsAssigned() const;
  DownloadStatus& MutableStatus() { return m_status; }

  const iMS::ToneBuffer& ToneBuffer() const { return *p_tbuf; }
  iMS::ToneBufferDownload*& DL() { return p_tbufdl; }
  ToneBufferDownloadSupervisor*& DS() { return p_tbufds; }
  bool Download();
 private:
  iMS::ToneBuffer* p_tbuf;
  iMS::ToneBufferDownload* p_tbufdl;
  ToneBufferDownloadSupervisor* p_tbufds;

  DownloadStatus m_status;
  unsigned int m_id;
  static unsigned int m_IDCount;
  unsigned int m_pts_added, m_total_pts, m_first, m_last;
};

// Initialise ID Counter
unsigned int ToneBufferDownloadBuffer::m_IDCount = 0;

ToneBufferDownloadBuffer::ToneBufferDownloadBuffer(unsigned int first, unsigned int last)
{
  p_tbuf = new iMS::ToneBuffer();
  p_tbufdl = nullptr;
  p_tbufds = nullptr;
  m_id = m_IDCount++;
  m_status.set_status(DownloadStatus::IDLE);
  m_status.set_progress(0);
  m_total_pts = (last - first) + 1;
  m_first = first;
  m_last = last;
  m_pts_added = 0;
}

ToneBufferDownloadBuffer::~ToneBufferDownloadBuffer()
{
  delete p_tbuf;
  if ((p_tbufds != nullptr) && (p_tbufdl != nullptr)) {
    p_tbufdl->ToneBufferDownloadEventUnsubscribe(ToneBufferEvents::DOWNLOAD_FINISHED, p_tbufds);
    p_tbufdl->ToneBufferDownloadEventUnsubscribe(ToneBufferEvents::DOWNLOAD_ERROR, p_tbufds);
  }
  if (p_tbufds != nullptr) delete p_tbufds;
  if (p_tbufdl != nullptr) delete p_tbufdl;
}

void ToneBufferDownloadBuffer::AssignNextPoint(const TBEntry& pt)
{
  if (m_pts_added < m_total_pts)
    (*p_tbuf)[m_first + m_pts_added++] = (pt);
}

bool ToneBufferDownloadBuffer::AllPointsAssigned() const
{
  return (m_pts_added >= m_total_pts);
}

bool ToneBufferDownloadBuffer::Download()
{
  if (p_tbufdl == nullptr)
    return false;
  return p_tbufdl->StartDownload(p_tbuf->cbegin()+m_first, p_tbuf->cbegin()+m_last+1);
}

bool HasBuffer(unsigned int id, const std::list<ToneBufferDownloadBuffer*>& buf_list)
{
  for (auto& b : buf_list)
    {
      if (b->getContext() == id)
	return true;
    }
  return false;
}

ToneBufferDownloadBuffer& GetBuffer(unsigned int id, const std::list<ToneBufferDownloadBuffer*>& buf_list)
{
  for (auto& b : buf_list)
    {
      if (b->getContext() == id)
	return *b;
    }
  return *(buf_list.front());
}

Status ToneBufferDownloadServiceImpl::create(ServerContext* context, const tonebuffer_header* request, DownloadHandle* response) {
    auto ims = m_state->get();
    if (ims->Open()) {
    downloadTbufList.push_back(new ToneBufferDownloadBuffer(request->first(), request->last()));
    response->set_id(downloadTbufList.back()->getContext());
  }
  return Status::OK;
}

Status ToneBufferDownloadServiceImpl::add(ServerContext* context, ServerReader<tonebuffer_entry>* reader, Empty* response) {
  tonebuffer_entry pt;
    auto ims = m_state->get();
    if (ims->Open()) {
    while (reader->Read(&pt)) {
      // Create a TBEntry
      TBEntry tbuf_pt(FAP(pt.freq_ch1(), pt.ampl_ch1(), pt.phs_ch1()),
			FAP(pt.freq_ch2(), pt.ampl_ch2(), pt.phs_ch2()),
			FAP(pt.freq_ch3(), pt.ampl_ch3(), pt.phs_ch3()),
			FAP(pt.freq_ch4(), pt.ampl_ch4(), pt.phs_ch4()));
    
      if (HasBuffer(pt.context().id(), downloadTbufList))
	GetBuffer(pt.context().id(), downloadTbufList).AssignNextPoint(tbuf_pt);
    }
  }
  return Status::OK;
}

Status ToneBufferDownloadServiceImpl::download(ServerContext* context, const DownloadHandle* request,
					  DownloadStatus* response) {
    auto ims = m_state->get();
    if (!ims->Open() || !HasBuffer(request->id(), downloadTbufList)) {
    if (!ims) std::cout << "ims is NULL" << std::endl;
    if (!ims->Open()) std::cout << "Connection to iMS is not open" << std::endl;
    if (!HasBuffer(request->id(), downloadTbufList)) std::cout << "Invalid Tone Buffer ID" << std::endl;
    return Status::OK;
  }
  
  ToneBufferDownloadBuffer& buf = GetBuffer(request->id(), downloadTbufList);
  if (!buf.AllPointsAssigned()) {
    std::cout << "All points not yet assigned" << std::endl;
    return Status::OK;
  }

  ToneBufferDownload*& tbuf_dl = buf.DL();
  ToneBufferDownloadSupervisor*& tbuf_ds = buf.DS();
  if (tbuf_dl == nullptr) {
    tbuf_dl = new ToneBufferDownload(ims, buf.ToneBuffer());
  }

  if (tbuf_ds == nullptr) {
    tbuf_ds = new ToneBufferDownloadSupervisor();
  }
  tbuf_dl->ToneBufferDownloadEventSubscribe(ToneBufferEvents::DOWNLOAD_FINISHED, tbuf_ds);
  tbuf_dl->ToneBufferDownloadEventSubscribe(ToneBufferEvents::DOWNLOAD_ERROR, tbuf_ds);

  DownloadStatus& sts = buf.MutableStatus();
  sts.set_status(DownloadStatus::DOWNLOADING);
  if (!buf.Download()) {
    std::cout << "ToneBuffer Download Failed" << std::endl;
    sts.set_status(DownloadStatus::DL_ERROR);
    return Status::OK;
  }

  *response = buf.MutableStatus();
  std::cout << "ToneBuffer Download Started" << std::endl;
  return Status::OK;
}

int retries = 0;

Status ToneBufferDownloadServiceImpl::dlstatus(ServerContext* context, const DownloadHandle* request,
					  DownloadStatus* response) {
    auto ims = m_state->get();
    if (!ims->Open() || !HasBuffer(request->id(), downloadTbufList)) return Status::OK;

  ToneBufferDownloadBuffer& buf = GetBuffer(request->id(), downloadTbufList);
  DownloadStatus& sts = buf.MutableStatus();
  ToneBufferDownloadSupervisor*& ds = buf.DS();
  if ((sts.status() != DownloadStatus::IDLE) && (ds != nullptr)) {
    if (ds->Busy()) {
      sts.set_status(DownloadStatus::DOWNLOADING);
    } else {
      if (ds->Error()) {
	if (retries<5) {
	  //	sts.set_status(DownloadStatus::DL_ERROR);
	  std::cout << "Retrying...(" << retries++ << ")" << std::endl;
	  ds->Reset();
	  sts.set_status(DownloadStatus::DOWNLOADING);
	  buf.Download();
	} else {
	  sts.set_status(DownloadStatus::DL_ERROR);
	  retries = 0;
	}
      } else {
	sts.set_status(DownloadStatus::DL_FINISHED);
      }
    }
    sts.set_progress(0);
  }
  *response = sts;
  return Status::OK;
}

Status ToneBufferManagerServiceImpl::start(ServerContext* context, const tonebuffer_params* request, Empty* response)
{
  SignalPath::ToneBufferControl tbc;
  SignalPath::Compensation AmplComp;
  SignalPath::Compensation PhaseComp;

    auto ims = m_state->get();
    if (ims->Open()) {

    if (m_sp == nullptr) {
        m_sp = new SignalPath(ims);
    }

    AmplComp = (request->amplitude_compensation_enabled() ? 
            SignalPath::Compensation::ACTIVE : SignalPath::Compensation::BYPASS);
    PhaseComp = (request->phase_compensation_enabled() ? 
            SignalPath::Compensation::ACTIVE : SignalPath::Compensation::BYPASS);
    switch(request->control())
        {
        case tonebuffer_params::HOST: tbc = SignalPath::ToneBufferControl::HOST; break;
        case tonebuffer_params::EXTERNAL: tbc = SignalPath::ToneBufferControl::EXTERNAL; break;
        case tonebuffer_params::EXTERNAL_EXTENDED: tbc = SignalPath::ToneBufferControl::EXTERNAL_EXTENDED; break;
        //    case tonebuffer_params::OFF: tbc = SignalPath::ToneBufferControl::OFF; break;
        }

    m_sp->UpdateLocalToneBuffer(tbc, 0, AmplComp, PhaseComp);
    }
  return Status::OK;
}

Status ToneBufferManagerServiceImpl::select(ServerContext* context, const tonebuffer_index* request, Empty* response)
{
  if (!m_sp) return Status::OK;
  m_sp->UpdateLocalToneBuffer(request->index());
  
  return Status::OK;
}

Status ToneBufferManagerServiceImpl::stop(ServerContext* context, const Empty* request, Empty* response)
{
  if (!m_sp) return Status::OK;
  m_sp->UpdateLocalToneBuffer(SignalPath::ToneBufferControl::OFF);
  return Status::OK;
}
