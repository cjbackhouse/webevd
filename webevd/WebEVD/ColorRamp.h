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

    /// \a z should be from 0 to +1 (though this function will clamp)
    RGBA GetRGBAWires(double z) const;
    /// \a z should be from -1 to +1 (though this function will clamp)
    RGBA GetRGBADigits(double z) const;

  protected:
    typedef std::vector<std::pair<double, RGBA>> Stops_t;

    RGBA InterpolateRGBA(double z0, RGBA c0,
                         double z1, RGBA c1,
                         double z) const;
    RGBA InterpolateRGBA(const Stops_t& stops, double z) const;

    Stops_t fDigitStops, fWireStops;
  };

}

DECLARE_ART_SERVICE(evd::ColorRamp, LEGACY)

#endif
