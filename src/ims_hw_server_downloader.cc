/*-----------------------------------------------------------------------------
/ Title      : iMS HW Server - Downloader
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/ims_hw_server/src/ims_hw_server_downloader.cc $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-09 11:02:55 +0000 (Thu, 09 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 661 $
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

#include "ims_hw_server_downloader.h"

#include <atomic>

#include "Image.h"
#include "IMSTypeDefs.h"
#include "ImageOps.h"

using namespace iMS;

class ImageDownloadSupervisor : public IEventHandler
{
private:
  std::atomic<bool> m_downloading{ true };
  std::atomic<bool> m_error{ false };
  std::atomic<int> m_handle{ -1 };
public:
  void EventAction(void* sender, const int message, const int param)
  {
    switch (message)
      {
      case (ImageDownloadEvents::DOWNLOAD_FINISHED) : {
	std::cout << "Image Download Finished!" << std::endl; 
	m_downloading.store(false); 
	break;
      }
      case (ImageDownloadEvents::DOWNLOAD_ERROR) : {
	/* std::cout << "Download error on message " << param << std::endl;*/ 
	std::cout << "Image Download Error!" << std::endl; 
	m_error.store(true);
	m_downloading.store(false); 
	break;
      }
      case (ImageDownloadEvents::IMAGE_DOWNLOAD_NEW_HANDLE) : {
	std::cout << "Image Download New Handle " <<  param << std::endl; 
	m_handle.store(param);
	break;
      }
      case (ImageDownloadEvents::DOWNLOAD_FAIL_MEMORY_FULL) : {
	std::cout << "Image Download Memory Full!" << std::endl; 
	m_error.store(true);
	m_downloading.store(false); 
	break;
      }
      case (ImageDownloadEvents::DOWNLOAD_FAIL_TRANSFER_ABORT) : {
	std::cout << "Image Download Memory Transfer Abort!" << std::endl; 
	m_error.store(true);
	m_downloading.store(false); 
	break;
      }
      }
  }
  bool Busy() const { return m_downloading.load(); };
  bool Error() const { return m_error.load(); };
  int Handle() const { return m_handle.load(); };
};

class ImageDownloadBuffer
{
 public:
  ImageDownloadBuffer(const std::string& name, const iMS::Frequency& int_osc, int ext_div);
  ~ImageDownloadBuffer();

  unsigned int getContext() const {return m_id;}

  void AddPoint(const iMS::ImagePoint& pt);
  void SetFormat(const iMS::ImageFormat& fmt);
  DownloadStatus& MutableStatus() { return m_status; }

  const iMS::Image& Image() const { return *p_img; }
  iMS::ImageDownload*& DL() { return p_imgdl; }
  ImageDownloadSupervisor*& DS() { return p_imgds; }
  iMS::ImageFormat*& Fmt() { return p_imgfmt; }
 private:
  iMS::Image* p_img;
  iMS::ImageDownload* p_imgdl;
  iMS::ImageFormat* p_imgfmt;
  ImageDownloadSupervisor* p_imgds;

  DownloadStatus m_status;
  unsigned int m_id;
  static unsigned int m_IDCount;
};

// Initialise ID Counter
unsigned int ImageDownloadBuffer::m_IDCount = 0;

ImageDownloadBuffer::ImageDownloadBuffer(const std::string& name, const Frequency& int_osc, int ext_div)
{
  p_img = new iMS::Image(name);
  p_img->ClockRate(int_osc);
  p_img->ExtClockDivide(ext_div);
  p_imgdl = nullptr;
  p_imgds = nullptr;
  p_imgfmt = nullptr;
  m_id = m_IDCount++;
  m_status.set_status(DownloadStatus::IDLE);
  m_status.set_progress(0);
}

ImageDownloadBuffer::~ImageDownloadBuffer()
{
  delete p_img;
  if ((p_imgds != nullptr) && (p_imgdl != nullptr)) {
    p_imgdl->ImageDownloadEventUnsubscribe(ImageDownloadEvents::DOWNLOAD_FINISHED, p_imgds);
    p_imgdl->ImageDownloadEventUnsubscribe(ImageDownloadEvents::DOWNLOAD_ERROR, p_imgds);
    p_imgdl->ImageDownloadEventUnsubscribe(ImageDownloadEvents::DOWNLOAD_FAIL_MEMORY_FULL, p_imgds);
    p_imgdl->ImageDownloadEventUnsubscribe(ImageDownloadEvents::DOWNLOAD_FAIL_TRANSFER_ABORT, p_imgds);
    p_imgdl->ImageDownloadEventUnsubscribe(ImageDownloadEvents::IMAGE_DOWNLOAD_NEW_HANDLE, p_imgds);
  }
  if (p_imgds != nullptr) delete p_imgds;
  if (p_imgdl != nullptr) delete p_imgdl;
  if (p_imgfmt != nullptr) delete p_imgfmt;
}

void ImageDownloadBuffer::AddPoint(const ImagePoint& pt)
{
  p_img->AddPoint(pt);
}

void ImageDownloadBuffer::SetFormat(const ImageFormat& fmt)
{
  if (p_imgfmt == nullptr) {
        p_imgfmt = new ImageFormat();
  }
  *p_imgfmt = fmt;
}

bool HasBuffer(unsigned int id, const std::list<ImageDownloadBuffer*>& buf_list)
{
  for (auto& b : buf_list)
    {
      if (b->getContext() == id)
	return true;
    }
  return false;
}

ImageDownloadBuffer& GetBuffer(unsigned int id, const std::list<ImageDownloadBuffer*>& buf_list)
{
  for (auto& b : buf_list)
    {
      if (b->getContext() == id)
	return *b;
    }
  return *(buf_list.front());
}

ImageDownloadServiceImpl::ImageDownloadServiceImpl(std::shared_ptr<iMS::IMSSystem> ims)
  : image_downloader::Service(), m_ims(ims), m_firstRun(true) {}

Status ImageDownloadServiceImpl::create(ServerContext* context, const image_header* request,
		 DownloadHandle* response) {
  if (m_ims != nullptr && m_ims->Open()) {
    downloadImageList.push_back(new ImageDownloadBuffer
				(request->name(), kHz(request->clock_rate()), request->ext_divide()));
    response->set_id(downloadImageList.back()->getContext());
  }
  return Status::OK;
}

Status ImageDownloadServiceImpl::add(ServerContext* context, ServerReader<image_point>* reader,
	     Empty* response) {
  image_point pt;
  if (m_ims != nullptr && m_ims->Open()) {
    while (reader->Read(&pt)) {
      // Create an ImagePoint
      ImagePoint img_pt(
			FAP(pt.freq_ch1(), pt.ampl_ch1(), pt.phs_ch1()),
			FAP(pt.freq_ch2(), pt.ampl_ch2(), pt.phs_ch2()),
			FAP(pt.freq_ch3(), pt.ampl_ch3(), pt.phs_ch3()),
			FAP(pt.freq_ch4(), pt.ampl_ch4(), pt.phs_ch4()),
			static_cast<float>(pt.synca1()), static_cast<float>(pt.synca2()), pt.syncd());
    
      if (HasBuffer(pt.context().id(), downloadImageList))
	GetBuffer(pt.context().id(), downloadImageList).AddPoint(img_pt);
    }
  }
  return Status::OK;
}

Status ImageDownloadServiceImpl::format(ServerContext* context, const image_format* request,
	     Empty* response) {
  iMS::ImageFormat fmt;

  fmt.Channels(request->channels());
  fmt.FreqBytes(request->freqbytes());
  fmt.AmplBytes(request->amplbytes());
  fmt.PhaseBytes(request->phasebytes());
  fmt.SyncBytes(request->syncbytes());
  fmt.EnableAmpl(request->enableampl());
  fmt.EnablePhase(request->enablephase());
  fmt.SyncAnlgChannels(request->syncanlgchannels());
  fmt.EnableSyncDig(request->enablesyncdig());
  fmt.CombineChannelPairs(request->combinechannelpairs());
  fmt.CombineAllChannels(request->combineallchannels());

  if (HasBuffer(request->context().id(), downloadImageList))
      GetBuffer(request->context().id(), downloadImageList).SetFormat(fmt);

  return Status::OK;
}

Status ImageDownloadServiceImpl::download(ServerContext* context, const DownloadHandle* request,
					  DownloadStatus* response) {
  if (!m_ims || !m_ims->Open() || !HasBuffer(request->id(), downloadImageList)) return Status::OK;

  if (m_firstRun)
  {
    // Workaround to iMSP Controllers < 1.3.41.
    // All downloads fail until a DMA Abort is issued that reinitialises the DMA Interrupts, due to the
    //  improper placement of the startup DMA Interrupt initialisation prior to Ethernet stack
    // Send a ForceStop command which implicitly causes a DMA Abort
    ImagePlayer pl(m_ims, Image());
    pl.Stop(ImagePlayer::StopStyle::IMMEDIATELY);
    m_firstRun = false;
  }

  ImageDownload*& img_dl = GetBuffer(request->id(), downloadImageList).DL();
  ImageDownloadSupervisor*& img_ds = GetBuffer(request->id(), downloadImageList).DS();
  if (img_dl == nullptr) {
    img_dl = new ImageDownload(m_ims, GetBuffer(request->id(), downloadImageList).Image());
  }

  // Set Format
  ImageFormat*& img_fmt = GetBuffer(request->id(), downloadImageList).Fmt();
  if (img_fmt != nullptr) {
        img_dl->SetFormat(*img_fmt);
  }

  if (img_ds == nullptr) {
    img_ds = new ImageDownloadSupervisor();
  }
  img_dl->ImageDownloadEventSubscribe(ImageDownloadEvents::DOWNLOAD_FINISHED, img_ds);
  img_dl->ImageDownloadEventSubscribe(ImageDownloadEvents::DOWNLOAD_ERROR, img_ds);
  img_dl->ImageDownloadEventSubscribe(ImageDownloadEvents::DOWNLOAD_FAIL_MEMORY_FULL, img_ds);
  img_dl->ImageDownloadEventSubscribe(ImageDownloadEvents::DOWNLOAD_FAIL_TRANSFER_ABORT, img_ds);
  img_dl->ImageDownloadEventSubscribe(ImageDownloadEvents::IMAGE_DOWNLOAD_NEW_HANDLE, img_ds);

  DownloadStatus& sts = GetBuffer(request->id(), downloadImageList).MutableStatus();
  sts.set_status(DownloadStatus::DOWNLOADING);
  sts.set_uuid((const void*)GetBuffer(request->id(), downloadImageList).Image().GetUUID().data(), 16);
  img_dl->StartDownload();

  *response = GetBuffer(request->id(), downloadImageList).MutableStatus();
  return Status::OK;
}

Status ImageDownloadServiceImpl::dlstatus(ServerContext* context, const DownloadHandle* request,
					  DownloadStatus* response) {
  if (!m_ims || !m_ims->Open() || !HasBuffer(request->id(), downloadImageList)) return Status::OK;

  DownloadStatus& sts = GetBuffer(request->id(), downloadImageList).MutableStatus();
  ImageDownloadSupervisor*& ds = GetBuffer(request->id(), downloadImageList).DS();
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


