#ifndef WEBEVD_COLORRAMP_H
#define WEBEVD_COLORRAMP_H

#include "art/Framework/Services/Registry/ServiceMacros.h"

namespace evd
{
  class ColorRamp
  {
  public:
    ColorRamp(const fhicl::ParameterSet& pset);

    typedef std::array<unsigned char, 4> RGBA;
    /// \a z should be from -1 to +1 (though this function will clamp)
    RGBA GetRGBA(double z) const;

    RGBA GetRGBADigits(double z) const;

  protected:
  };

}

DECLARE_ART_SERVICE(evd::ColorRamp, LEGACY)

#endif
