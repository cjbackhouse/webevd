#ifndef WEBEVD_THREADSAFEGALLERYEVENT_H
#define WEBEVD_THREADSAFEGALLERYEVENT_H

#include "gallery/Event.h"

#include <mutex>

namespace evd
{
  /// gallery::Event is not designed to be threadsafe. Serializing calls
  /// through this wrapper should be more efficient than giving up on threading
  /// as a whole. This way we can still write and transfer the PNGs in parallel
  /// for example.
  class ThreadsafeGalleryEvent
  {
  public:
    template<class PROD> using HandleT = gallery::Event::template HandleT<PROD>;

    ThreadsafeGalleryEvent(const gallery::Event* evt) : fEvt(evt)
    {
    }

    template<class T>
    bool getByLabel(const art::InputTag& tag, gallery::Handle<T>& result) const
    {
      std::lock_guard guard(fLock);
      return fEvt->getByLabel(tag, result);
    }

    template<class T>
    std::vector<art::InputTag> getInputTags() const
    {
      std::lock_guard guard(fLock);
      return fEvt->getInputTags<T>();
    }

    const art::EventAuxiliary& eventAuxiliary() const
    {
      std::lock_guard guard(fLock);
      return fEvt->eventAuxiliary();
    }

  protected:
    mutable std::mutex fLock;
    const gallery::Event* fEvt;
  };
}

#endif
