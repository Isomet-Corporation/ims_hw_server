/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server - Compensation Downloader
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_compensation.cc $
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

#include "ims_hw_server_compensation.h"

#include <atomic>

#include "Compensation.h"
#include "IMSTypeDefs.h"
#include "SignalPath.h"

using namespace iMS;

class CompensationDownloadSupervisor : public IEventHandler
{
private:
  std::atomic<bool> m_downloading{ true };
  std::atomic<bool> m_error{ false };
public:
  void EventAction(void* sender, const int message, const int param)
  {
    switch (message)
      {
      case (CompensationEvents::DOWNLOAD_FINISHED) : {
	std::cout << "Compensation Download Finished!" << std::endl; 
	m_downloading.store(false); 
	break;
      }
      case (CompensationEvents::DOWNLOAD_ERROR) : {
	std::cout << "Download error on message " << param << std::endl;
	//	std::cout << "Compensation Download Error!" << std::endl; 
	m_error.store(true);
	m_downloading.store(false); 
	break;
      }
     }
  }
  bool Busy() const { return m_downloading.load(); };
  bool Error() const { return m_error.load(); };
};

class CompensationDownloadBuffer
{
 public:
  CompensationDownloadBuffer(std::shared_ptr<iMS::IMSSystem> ims, bool isXY);
  ~CompensationDownloadBuffer();

  unsigned int getContext() const {return m_id;}

  void AssignNextPoint(const iMS::CompensationPoint& pt);
  bool AllPointsAssigned() const;
  DownloadStatus& MutableStatus() { return m_status; }
  bool isXYPhase() { return m_isXY; }

  const iMS::CompensationTable& Compensation() const { return *p_comp; }
  iMS::CompensationTableDownload*& DL() { return p_compdl; }
  CompensationDownloadSupervisor*& DS() { return p_compds; }
 private:
  iMS::CompensationTable* p_comp;
  iMS::CompensationTableDownload* p_compdl;
  CompensationDownloadSupervisor* p_compds;

  bool m_isXY;
  DownloadStatus m_status;
  unsigned int m_id;
  static unsigned int m_IDCount;
  unsigned int m_pts_added, m_total_pts;
};

// Initialise ID Counter
unsigned int CompensationDownloadBuffer::m_IDCount = 0;

CompensationDownloadBuffer::CompensationDownloadBuffer(std::shared_ptr<iMS::IMSSystem> ims, bool isXY)
{
  p_comp = new iMS::CompensationTable(ims);
  p_compdl = nullptr;
  p_compds = nullptr;
  m_id = m_IDCount++;
  m_isXY = isXY;
  m_status.set_status(DownloadStatus::IDLE);
  m_status.set_progress(0);
  m_total_pts = (1 << ims->Synth().GetCap().LUTDepth);
  m_pts_added = 0;
}

CompensationDownloadBuffer::~CompensationDownloadBuffer()
{
  delete p_comp;
  if ((p_compds != nullptr) && (p_compdl != nullptr)) {
    p_compdl->CompensationTableDownloadEventUnsubscribe(CompensationEvents::DOWNLOAD_FINISHED, p_compds);
    p_compdl->CompensationTableDownloadEventUnsubscribe(CompensationEvents::DOWNLOAD_ERROR, p_compds);
  }
  if (p_compds != nullptr) delete p_compds;
  if (p_compdl != nullptr) delete p_compdl;
}

void CompensationDownloadBuffer::AssignNextPoint(const CompensationPoint& pt)
{
  if (m_pts_added < m_total_pts)
    (*p_comp)[m_pts_added++] = (pt);
}

bool CompensationDownloadBuffer::AllPointsAssigned() const
{
  return (m_pts_added >= m_total_pts);
}

bool HasBuffer(unsigned int id, const std::list<CompensationDownloadBuffer*>& buf_list)
{
  for (auto& b : buf_list)
    {
      if (b->getContext() == id)
	return true;
    }
  return false;
}

CompensationDownloadBuffer& GetBuffer(unsigned int id, const std::list<CompensationDownloadBuffer*>& buf_list)
{
  for (auto& b : buf_list)
    {
      if (b->getContext() == id)
	return *b;
    }
  return *(buf_list.front());
}

Status CompensationDownloadServiceImpl::create(ServerContext* context, const compensation_header* request,
		 DownloadHandle* response) {
  if (m_ims != nullptr && m_ims->Open()) {
    downloadCompList.push_back(new CompensationDownloadBuffer(m_ims, request->isxy()));
    response->set_id(downloadCompList.back()->getContext());
  }
  return Status::OK;
}

Status CompensationDownloadServiceImpl::add(ServerContext* context, ServerReader<compensation_point>* reader,
	     Empty* response) {
  compensation_point pt;
  if (m_ims != nullptr && m_ims->Open()) {
    while (reader->Read(&pt)) {
      // Create an CompensationPoint
      CompensationPoint comp_pt(pt.amplitude(), pt.phase(), pt.sync_dig(), pt.sync_anlg());
    
      if (HasBuffer(pt.context().id(), downloadCompList))
	GetBuffer(pt.context().id(), downloadCompList).AssignNextPoint(comp_pt);
    }
  }
  return Status::OK;
}

Status CompensationDownloadServiceImpl::download(ServerContext* context, const compensation_download* request,
					  DownloadStatus* response) {
  if (!m_ims || !m_ims->Open() || !HasBuffer(request->context().id(), downloadCompList)) return Status::OK;

  CompensationDownloadBuffer& buf = GetBuffer(request->context().id(), downloadCompList);
  RFChannel chan;
  if ((request->channel() < RFChannel::min) || (request->channel() > RFChannel::max)) {
    chan = RFChannel::all;
  } else {
    chan = RFChannel(request->channel());
  }

  if (!buf.AllPointsAssigned()) return Status::OK;

  CompensationTableDownload*& comp_dl = buf.DL();
  CompensationDownloadSupervisor*& comp_ds = buf.DS();
  if (comp_dl != nullptr) {
    delete comp_dl;
  }
  comp_dl = new CompensationTableDownload(m_ims, buf.Compensation(), chan);

  if (comp_ds != nullptr) {
    delete comp_ds;
  }
  comp_ds = new CompensationDownloadSupervisor();
  
  comp_dl->CompensationTableDownloadEventSubscribe(CompensationEvents::DOWNLOAD_FINISHED, comp_ds);
  comp_dl->CompensationTableDownloadEventSubscribe(CompensationEvents::DOWNLOAD_ERROR, comp_ds);

  // We assume since we are downloading a compensation table that we want to activate it, so turn off the Amplitude and Phase Bypass switches
  SignalPath sp(m_ims);
  sp.EnableImagePathCompensation(SignalPath::Compensation::ACTIVE, SignalPath::Compensation::ACTIVE);

  // Set XY Style Phase Compensation if required, or clear if not
  sp.EnableXYPhaseCompensation(buf.isXYPhase());

  DownloadStatus& sts = buf.MutableStatus();
  sts.set_status(DownloadStatus::DOWNLOADING);
  sts.set_uuid((const void*)buf.Compensation().GetUUID().data(), 16);
  comp_dl->StartDownload();

  *response = buf.MutableStatus();
  return Status::OK;
}

Status CompensationDownloadServiceImpl::dlstatus(ServerContext* context, const DownloadHandle* request,
					  DownloadStatus* response) {
  if (!m_ims || !m_ims->Open() || !HasBuffer(request->id(), downloadCompList)) return Status::OK;

  CompensationDownloadBuffer& buf = GetBuffer(request->id(), downloadCompList);
  DownloadStatus& sts = buf.MutableStatus();
  CompensationDownloadSupervisor*& ds = buf.DS();
  if ((sts.status() != DownloadStatus::IDLE) && 
      (ds != nullptr)) {
    if (ds->Busy()) {
      sts.set_status(DownloadStatus::DOWNLOADING);
    } else {
      if (ds->Error()) {
	sts.set_status(DownloadStatus::DL_ERROR);
      } else {
	sts.set_status(DownloadStatus::DL_FINISHED);
      }
    }
    sts.set_progress(0);
  }
  *response = sts;
  return Status::OK;
}


