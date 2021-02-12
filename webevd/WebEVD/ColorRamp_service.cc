// Chris Backhouse - bckhouse@fnal.gov

#include "webevd/WebEVD/ColorRamp.h"

namespace evd
{
  //--------------------------------------------------------------------
  ColorRamp::ColorRamp(const fhicl::ParameterSet& pset) :
    fDigitStops(pset.get<Stops_t>("DigitStops")),
    fWireStops (pset.get<Stops_t>("WireStops")),
    fDigitMaxADC(pset.get<int>("DigitMaxADC")),
    fWireMaxADC (pset.get<int>("WireMaxADC"))
  {

  }

  //--------------------------------------------------------------------
  evd::ColorRamp::RGBA ColorRamp::InterpolateRGBA(double z0, RGBA c0,
                                                  double z1, RGBA c1,
                                                  double z) const
  {
    const double f = (z-z0)/(z1-z0);

    RGBA ret;
    for(int c = 0; c < 4; ++c){
      const double v = c0[c] * (1-f) + c1[c] * f; // interpolate
      ret[c] = std::max(0, std::min(int(v+.5), 255)); // clamp
    }

    return ret;
  }

  //--------------------------------------------------------------------
  evd::ColorRamp::RGBA ColorRamp::InterpolateRGBA(const Stops_t& stops,
                                                  double z) const
  {
    if(z <= stops.front().first) return stops.front().second;
    if(z >= stops.back().first) return stops.back().second;

    // Find the first >= element
    const auto i1 = std::lower_bound(stops.begin(), stops.end(), z,
                                     [](const std::pair<double, RGBA>& x,
                                        const double& z)
                                     {return x.first < z;});
    // So this is the last < element
    const auto i0 = i1-1;

    return InterpolateRGBA(i0->first, i0->second,
                           i1->first, i1->second,
                           z);
  }

  //--------------------------------------------------------------------
  evd::ColorRamp::RGBA ColorRamp::GetRGBAWires(int adc) const
  {
    return InterpolateRGBA(fWireStops, double(adc)/fWireMaxADC);
  }

  //--------------------------------------------------------------------
  evd::ColorRamp::RGBA ColorRamp::GetRGBADigits(int adc) const
  {
    return InterpolateRGBA(fDigitStops, double(adc)/fDigitMaxADC);
  }
}

DEFINE_ART_SERVICE(evd::ColorRamp)
