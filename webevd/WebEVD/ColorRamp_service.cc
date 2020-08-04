// Chris Backhouse - bckhouse@fnal.gov

#include "webevd/WebEVD/ColorRamp.h"

namespace evd
{
  //--------------------------------------------------------------------
  ColorRamp::ColorRamp(const fhicl::ParameterSet& pset)
  {
  }

  //--------------------------------------------------------------------
  evd::ColorRamp::RGBA ColorRamp::GetRGBA(double z) const
  {
    z = std::max(-1., std::min(z, +1.)); // clamp

    // dark green
    return {0, 128, 0, (unsigned char)(255*fabs(z))};
  }

  //--------------------------------------------------------------------
  evd::ColorRamp::RGBA ColorRamp::GetRGBADigits(double z) const
  {
    z = std::max(-1., std::min(z, +1.)); // clamp

    if(z < 0){
      return {0, 0, 255, (unsigned char)(255*fabs(z))}; // blue
    }
    else{
      return {255, 0, 0, (unsigned char)(255*fabs(z))}; // red
    }
  }
}

DEFINE_ART_SERVICE(evd::ColorRamp)
