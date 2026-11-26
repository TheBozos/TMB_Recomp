// Save states: TMB's section (tmb_state.h). The engine saves the runtime, its libraries (989SND included) and the host;
// this adds what TMB keeps itself: what the game told its hooks once, the native renderer's retained records, and the
// overrides' own counters. Caches decoded from guest memory are dropped on a load.
#include "tmb_state.h"

#include "tmb_scene.h"

#include "bozo/host/free_host.h"
#include "bozo/state.h"

#include <string>
#include <type_traits>

namespace tmb
{
    void overridesState(bozo::state::Archive &a); // tmb_overrides.cpp
    namespace meshes { void state(bozo::state::Archive &); }
    namespace textures { void state(bozo::state::Archive &); }
    namespace particles { void state(bozo::state::Archive &); }
    namespace cheats { void state(bozo::state::Archive &); }
    namespace twod { void stateLoaded(); }
    namespace menu { void stateLoaded(); }

    void addStateSection(bozo::host::FreeHost &host, Runtime *runtime)
    {
        host.stateSections()->add({"tmb",
                                   [runtime](bozo::state::Archive &a) {
                                       overridesState(a);
                                       // The native hierarchy's retained records (the frames, lights, renderer
                                       // parameters the game's VU programs kept between draws).
                                       static_assert(std::is_trivially_copyable_v<scene::Scene>);
                                       a.begin("scene");
                                       bozo::state::pod(a, scene::get(runtime));
                                       a.end();
                                       a.begin("meshes");
                                       meshes::state(a);
                                       a.end();
                                       a.begin("textures");
                                       textures::state(a);
                                       a.end();
                                       a.begin("particles");
                                       particles::state(a);
                                       a.end();
                                       cheats::state(a);
                                       if (a.loading())
                                       {
                                           twod::stateLoaded();
                                           menu::stateLoaded();
                                       }
                                   },
                                   [] { return std::string(); }});
    }
}
