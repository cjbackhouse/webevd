#include "art/Framework/Services/Registry/ServiceRegistry.h"
#include "fhiclcpp/make_ParameterSet.h"
#include "fhiclcpp/intermediate_table.h"

#include "webevd/ArtSupport/ArtServiceHelper.h"

// The following is an ugly hack, but it is necessary for being able
// to set the services manager.
namespace art::test {
  void set_manager_for_tests(ServicesManager* manager)
  {
    ServiceRegistry::instance().setManager(manager);
  }
}

namespace {
  auto fully_processed(fhicl::ParameterSet&& pset)
  {
    // Make sure each service has a value for the "service_type" parameter
    fhicl::ParameterSet result;
    auto const& input = pset.has_key("services") ? pset.get<fhicl::ParameterSet>("services") : pset;
    auto const service_names = input.get_pset_names();
    for (auto const& service_name : service_names) {
      auto service_pset = input.get<fhicl::ParameterSet>(service_name);
      service_pset.put("service_type", service_name);
      result.put(service_name, service_pset);
    }
    return result;
  }
}

ArtServiceHelper::ArtServiceHelper(fhicl::ParameterSet&& pset) :
  activityRegistry_{},
  servicesManager_{std::move(pset), activityRegistry_}
{
  art::test::set_manager_for_tests(&servicesManager_);
  servicesManager_.forceCreation();
}

void ArtServiceHelper::load_services(std::string const& config)
{
  cet::filepath_lookup lookup{"FHICL_FILE_PATH"};
  fhicl::intermediate_table table;
  fhicl::parse_document(config, lookup, table);
  fhicl::ParameterSet pset;
  fhicl::make_ParameterSet(table, pset);
  static ArtServiceHelper helper{fully_processed(std::move(pset))};
}
