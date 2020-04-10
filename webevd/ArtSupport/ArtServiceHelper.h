// ArtServiceHelper.h

#ifndef ArtServiceHelper_H
#define ArtServiceHelper_H

// =================================================================
// The purpose of the 'ArtServiceHelper' class is to construct and
// manage a set of art services that normally appear in an art job.
// In some circumstances, the available means of testing a new
// algorithm may rely on functionality that has been expressed in
// terms of an art service.  In such a case, the ArtServiceHelper
// class can be used to initialize the required services and allow
// users to create art::ServiceHandle objects for the configured
// services.
//
// Unlike the art framework's services manager, the ArtServiceHelper
// is not aware of framework transitions.  Because of that, using some
// services outside of the context of the framework will not reflect
// the same behavior as using them within it.  For that reason, using
// such services outside of the framework is circumspect.  This must
// be taken into account when using this class.
//
// Configuration layout
// ====================
//
// The allowed configuration layout is of two forms.  The minimal form
// lists all desired services by themselves:
//
//   Service1: {...}
//   Service2: {...}
//   ServiceN: {...}
//
// The other form nests these services inside of a 'services' table,
// which supports cases when it is desired to use a configuration file
// that would be used for an art job:
//
//   services: {
//     Service1: {...}
//     Service2: {...}
//     ServiceN: {...}
//   }
//
// As in art, it is not necessary to specify the 'service_type'
// parameter for each service--the ArtServiceHelper class inserts
// those configuration parameters automatically.
//
// =================================================================

#include "art/Framework/Services/Registry/ActivityRegistry.h"
#include "art/Framework/Services/Registry/ServicesManager.h"

#include <iosfwd>
#include <map>
#include <string>

class ArtServiceHelper {
public:
  static void load_services(std::string const& config);

private:
  explicit ArtServiceHelper(fhicl::ParameterSet&& pset);
  art::ActivityRegistry activityRegistry_;
  art::ServicesManager servicesManager_;
};

#endif
