#include "webevd/WebEVD/TruthText.h"

// This file is adapted from NOvA's EventDisplay/SimulationDrawer.cxx and
// Style.cxx

#include "nusimdata/SimulationBase/MCTruth.h"

namespace evd
{
  // -------------------------------------------------------------------------
  /// Convert PDG code to a latex string (root-style)
  const char* LatexName(int pdgcode)
  {
    switch(pdgcode){
    case  22:   return "#gamma";
    case -11:   return "e^{+}";
    case  11:   return "e^{-}";
    case  13:   return "#mu";
    case -15:   return "#bar{#tau}";
    case  15:   return "#tau";
    case -13:   return "#bar{#mu}";
    case  12:   return "#nu_{e}";
    case  14:   return "#nu_{#mu}";
    case  16:   return "#nu_{#tau}";
    case -12:   return "#bar{#nu}_{e}";
    case -14:   return "#bar{#nu}_{#mu}";
    case -16:   return "#bar{#nu}_{#tau}";
    case  111:  return "#pi^{0}";
    case  211:  return "#pi^{+}";
    case -211:  return "#pi^{-}";;
    case  221:  return "#eta";
    case  331:  return "#eta'";
    case  130:  return "K^{0}_{L}";
    case  310:  return "K^{0}_{S}";
    case  311:  return "K^{0}";
    case -311:  return "#bar{K}^{0}";
    case  321:  return "K^{+}";
    case -321:  return "K^{-}";
    case  2112: return "n";
    case  2212: return "p";
    case -2112: return "#bar{n}";
    case -2212: return "#bar{p}";
    case  2224: return "#Delta^{++}";
    case  3122: return "#Lambda^{0}";
    case  3222: return "#Sigma^{+}";
    case -3222: return "#Sigma^{-}";

    case 1000010020: return "^{2}H";
    case 1000010030: return "^{3}H";
    case 1000020030: return "^{3}He";
    case 1000020040: return "^{4}He";
    case 1000040070: return "^{7}Be";
    case 1000040080: return "^{8}Be";
    case 1000040100: return "^{10}Be";
    case 1000050090: return "^{9}B";
    case 1000050100: return "^{10}B";
    case 1000050110: return "^{11}B";
    case 1000060100: return "^{10}C";
    case 1000060110: return "^{11}C";
    case 1000060120: return "^{12}C";
    case 1000060130: return "^{13}C";
    case 1000060140: return "^{14}C";
    case 1000070150: return "^{15}N";
    case 1000100220: return "^{22}Ne";
    case 1000140300: return "^{30}Si";
    case 1000150330: return "^{33}P";
    case 1000160340: return "^{34}S";
    case 1000160350: return "^{35}S";
    case 1000160360: return "^{36}S";
    case 1000170350: return "^{35}Cl";
    case 1000170360: return "^{36}Cl";
    case 1000170370: return "^{37}Cl";
    case 1000180380: return "^{38}Ar";
    case 1000180400: return "^{40}Ar";
    case 1000210480: return "^{48}Sc";
    case 1000260560: return "^{56}Fe";
    case 1000220480: return "^{48}Ti";
    case 1000080160: return "^{16}O";
    case 1000070140: return "^{14}N";
    case 1000110230: return "^{23}Na";
    case 1000130270: return "^{27}Al";
    case 1000140280: return "^{28}Si";
    case 1000200400: return "^{40}Ca";
    case 1000561370: return "^{137}Ba";
    default:
      static char buf[256];
      sprintf(buf,"X_{%d}", pdgcode);
      return buf;
    }
    return 0;
  }

  // --------------------------------------------------------------------------
  std::string ShortInteractionSuffix(int iType)
  {
    switch(iType){
    case simb::kQE:
    case simb::kCCQE:
      return " (QE)";

    case simb::kRes:
    case simb::kResCCNuProtonPiPlus:
    case simb::kResCCNuNeutronPi0:
    case simb::kResCCNuNeutronPiPlus:
    case simb::kResCCNuBarNeutronPiMinus:
    case simb::kResCCNuBarProtonPi0:
    case simb::kResCCNuBarProtonPiMinus:
      return " (RES)";

    case simb::kDIS:
    case simb::kCCDIS:
      return " (DIS)";

    case simb::kCoh:
    case simb::kCCCOH:
      return " (COH)";

    case simb::kNCQE:
    case simb::kNCDIS:
    case simb::kResNCNuProtonPi0:
    case simb::kResNCNuProtonPiPlus:
    case simb::kResNCNuNeutronPi0:
    case simb::kResNCNuNeutronPiMinus:
    case simb::kResNCNuBarProtonPi0:
    case simb::kResNCNuBarProtonPiPlus:
    case simb::kResNCNuBarNeutronPi0:
    case simb::kResNCNuBarNeutronPiMinus:
      return " (NC)";

    case simb::kElectronScattering:
      return " (ES)";

    case simb::kInverseBetaDecay:
      return " (IBD)";

    default: // simb::kNuElectronElastic and simb::kInverseMuDecay
      return "";
    }
  }

  // -------------------------------------------------------------------------
  std::string MCTruthShortText(const simb::MCTruth& truth)
  {
    std::string mctext;

    bool firstin  = true;
    bool firstout = true;
    std::string origin;
    std::string incoming;
    std::string outgoing;
    // Label cosmic rays -- others are pretty obvious
    if(truth.Origin() == simb::kCosmicRay) { origin = "c-ray: "; }

    for(int j = 0; j < truth.NParticles(); ++j){
      const simb::MCParticle& p = truth.GetParticle(j);

      const unsigned int bufsize = 1024;
      char buf[bufsize];

      if(p.P() > 0.05){
        snprintf(buf, bufsize, "%s [%.1f GeV/c]",
                 LatexName(p.PdgCode()), p.P());
      }
      else{
        snprintf(buf, bufsize, "%s",
                 LatexName(p.PdgCode()));
      }
      if(p.StatusCode() == 0){
        if(!firstin) incoming += " + ";
        incoming += buf;
        firstin = false;
      }
      if(p.StatusCode() == 1){
        if(!firstout) outgoing += " + ";
        outgoing += buf;
        firstout = false;
      }
    } // loop on j particles

    if(origin.empty() && incoming.empty()) {
      mctext += outgoing;
    }
    else{
      const int intType = truth.GetNeutrino().InteractionType();
      const std::string suffix = ShortInteractionSuffix(intType);
      const std::string newtext = origin+incoming+" -> "+outgoing+suffix;
      mctext += newtext;
    }

    return mctext;
  }
} // end namespace
