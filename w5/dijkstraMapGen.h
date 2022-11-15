#pragma once
#include <vector>
#include <flecs.h>
#include <iostream>
#include "ecsTypes.h"

namespace dmaps
{
  void gen_player_approach_map(flecs::world &ecs, std::vector<float> &map);
  void gen_player_flee_map(flecs::world &ecs, std::vector<float> &map);
  void gen_hive_pack_map(flecs::world &ecs, std::vector<float> &map);
  void gen_player_visibility_map(flecs::world &ecs, std::vector<float> &map);
  void gen_player_near_map(flecs::world &ecs, std::vector<float> &map, int quad_size);
};

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

inline std::ostream& operator<<(std::ostream& ostr, const Position& pos) {
    return ostr << pos.x << ' ' << pos.y;
}

