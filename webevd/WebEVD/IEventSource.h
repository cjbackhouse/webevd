#ifndef WEBEVD_IEVENTSOURCE_H
#define WEBEVD_IEVENTSOURCE_H

#include "canvas/Persistency/Provenance/EventID.h"

#include <string>

namespace evd
{
  template<class T> class IEventSource
  {
  public:
    virtual ~IEventSource() {}
    virtual const T* FirstEvent(const std::string& fname,
                                art::EventID* next = 0) = 0;

    /// If there is no next or previous event, that argument will not be
    /// updated
    virtual const T* GetEvent(const std::string& fname,
                              art::EventID id,
                              art::EventID* next = 0,
                              art::EventID* prev = 0) = 0;
  };
}

#endif
