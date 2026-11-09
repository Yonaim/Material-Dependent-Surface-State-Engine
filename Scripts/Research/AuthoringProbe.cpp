// Read-only capability probe for authoring fixtures. It does not render or measure human effort.
#include "AssetManager/Loaders/SRProfileLoader.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "Rendering/Surface/DemoSurfaceEffects.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

int main(int argc,char** argv)
try
{
    if(argc!=3)throw std::invalid_argument("Usage: MDSS_AuthoringProbe cases.json output.json");
    nlohmann::json Cases;std::ifstream(argv[1])>>Cases;
    nlohmann::json Output=nlohmann::json::array();
    for(const auto& C:Cases)
    {
        auto Asset=MDSS::Asset::FSRProfileLoader::Load(0,C.at("profile").get<std::string>());
        const auto& P=Asset->GetData();
        MDSS::SurfaceState::FSurfaceStateRegistry Registry({P});
        const auto Bindings=MDSS::Rendering::ResolveDemoSurfaceStates(Registry);
        const auto Name=C.at("state").get<std::string>();
        const auto State=Registry.GetStateId(Name);
        const bool bWetnessBound=State!=MDSS::SurfaceState::InvalidStateId&&Bindings.AbsorbedWetness==State;
        nlohmann::json Names=nlohmann::json::array();
        for(unsigned I=0;I<Registry.GetStateCount();++I)Names.push_back(Registry.GetStateName(I));
        Output.push_back({{"id",C.at("id")},{"profile_loaded",true},{"registered",State!=MDSS::SurfaceState::InvalidStateId},
            {"state",Name},{"channels",Names},{"rule_count",P.Reactions.size()},
            {"builtin_absorbed_wetness_binding",bWetnessBound},
            {"expected_builtin_absorbed_wetness_binding",C.at("expected_builtin_absorbed_wetness_binding")},
            {"scope","Actual profile loader/registry/appearance binding; no rendered-frame or author-performance evidence"}});
        if(bWetnessBound!=C.at("expected_builtin_absorbed_wetness_binding").get<bool>())throw std::runtime_error("Unexpected appearance binding");
    }
    std::ofstream F(argv[2]);F<<Output.dump(2);if(!F)throw std::runtime_error("Output failed");
    std::cout<<Output.size()<<" profile/appearance binding cases verified\n";return 0;
}
catch(const std::exception& E){std::cerr<<E.what()<<'\n';return 1;}
