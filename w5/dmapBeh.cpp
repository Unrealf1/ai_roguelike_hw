#include "dmapBeh.h"
#include "ecsTypes.h"

flecs::entity create_player_approacher(flecs::entity e)
{
  e.set(DmapWeights{{{"approach_map", {1.f, 1.f}}}});
  return e;
}

flecs::entity create_player_fleer(flecs::entity e)
{
  e.set(DmapWeights{{{"flee_map", {1.f, 1.f}}}});
  return e;
}

flecs::entity create_archer(flecs::entity e)
{
  e.set(DmapWeights{{
          {"player_near", {1.f, 1.f}},
          {"gen_player_visibility", {2.f, 1.f}},
          {"approach_map", {0.1f, 1.f}}
  }});
  e.set(RangedAttack{4, 10});
  return e;
}

flecs::entity create_hive_follower(flecs::entity e)
{
  e.set(DmapWeights{{{"hive_map", {1.f, 1.f}}}});
  return e;
}

flecs::entity create_hive_monster(flecs::entity e)
{
  e.set(DmapWeights{{{"hive_map", {1.f, 1.f}}, {"approach_map", {1.8, 0.8f}}}});
  return e;
}

