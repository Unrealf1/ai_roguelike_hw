#include "aiLibrary.h"
#include "ecsTypes.h"
#include "aiUtils.h"
#include "flecs/addons/cpp/mixins/query/impl.hpp"
#include "math.h"
#include "raylib.h"
#include "blackboard.h"
#include <algorithm>
#include <array>
#include <iostream>


struct CompoundNode : public BehNode
{
  std::vector<BehNode*> nodes;

  CompoundNode(std::vector<BehNode*> nodes) : nodes(std::move(nodes)) {}
  CompoundNode() = default;

  virtual ~CompoundNode()
  {
    for (BehNode *node : nodes)
      delete node;
    nodes.clear();
  }

  CompoundNode &pushNode(BehNode *node)
  {
    nodes.push_back(node);
    return *this;
  }
  void react(flecs::world &ecs, flecs::entity entity, Blackboard & bb, Event event) override {
    for (auto& node : nodes) {
        node->react(ecs, entity, bb, event);
    }
  }
};

template<size_t Arity>
struct FixedArityCompoundNode : public BehNode
{
public:
  FixedArityCompoundNode(std::array<std::unique_ptr<BehNode>, Arity>&& nodes) : m_children(std::move(nodes)) {}
  void react(flecs::world &ecs, flecs::entity entity, Blackboard & bb, Event event) override {
    for (auto& node : m_children) {
        node->react(ecs, entity, bb, event);
    }
  }
protected:
  std::array<std::unique_ptr<BehNode>, Arity> m_children;
};

// Run all children one by one
// fail on first fail
// success on all success
struct Sequence : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      if (res != BEH_SUCCESS)
        return res;
    }
    return BEH_SUCCESS;
  }
};

// Run all children one by one
// fail on all fail
// success on first success
struct Selector : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      if (res != BEH_FAIL)
        return res;
    }
    return BEH_FAIL;
  }
};

// Run all children one by one
// fail on all fail
// success on any success
struct OrNode : public CompoundNode
{
  using CompoundNode::CompoundNode;
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    bool anySuccess = false;
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      if (res == BEH_SUCCESS) {
        anySuccess = true;
      } else if (res == BEH_RUNNING) {
        return res;
      }
    }
    return anySuccess ? BEH_SUCCESS : BEH_FAIL;
  }
};

// Run all children at once
// running while any of the children running
// success on all success; fail on any fail
struct Parallel : public CompoundNode
{
  using CompoundNode::CompoundNode;
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    bool allSuccess = true;
    bool allFinished = true;
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      
      if (res == BEH_FAIL) {
        allSuccess = false;
      } else if (res == BEH_RUNNING) {
        allFinished = false;
      }
    }
    return !allFinished 
            ? BEH_RUNNING 
            : allSuccess 
              ? BEH_SUCCESS 
              : BEH_FAIL;
  }
};

struct NotNode : public FixedArityCompoundNode<1> {
  using FixedArityCompoundNode::FixedArityCompoundNode;
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override {
    auto res = m_children.front()->update(ecs, entity, bb);
    if (res == BEH_FAIL) {
        return BEH_SUCCESS;
    } else if (res == BEH_SUCCESS) {
        return BEH_FAIL;
    } 
    return res;
  }
};

struct MoveToEntity : public BehNode
{
  size_t entityBb = size_t(-1); // wraps to 0xff...
  MoveToEntity(flecs::entity entity, const char *bb_name)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos)
    {
      flecs::entity targetEntity = bb.get<flecs::entity>(entityBb);
      if (!targetEntity.is_alive())
      {
        res = BEH_FAIL;
        return;
      }

      targetEntity.get([&](const Position &target_pos)
      {
        if (pos != target_pos)
        {
          a.action = move_towards(pos, target_pos);
          res = BEH_RUNNING;
        }
        else
          res = BEH_SUCCESS;
      });
    });
    return res;
  }
};

struct IsLowHp : public BehNode
{
  float threshold = 0.f;
  IsLowHp(float thres) : threshold(thres) {}

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &) override
  {
    BehResult res = BEH_SUCCESS;
    entity.get([&](const Hitpoints &hp)
    {
      res = hp.hitpoints < threshold ? BEH_SUCCESS : BEH_FAIL;
    });
    return res;
  }
};

template<typename... QueryTypes>
struct FindClosestOf : public BehNode
{
  using Query = flecs::query<Position, QueryTypes...>;

  size_t entityBb = size_t(-1);
  float distance = 0;
  const Query m_query;
  FindClosestOf(flecs::entity entity, float in_dist, const char *bb_name, const Query& query) 
      : distance(in_dist), m_query(query)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_FAIL;
    entity.set([&](const Position &pos)
    {
      flecs::entity closest;
      float closestDist = FLT_MAX;
      Position closestPos;
      m_query.each([&](flecs::entity e, const Position &other_pos, const QueryTypes&...)
      {
        float curDist = dist(other_pos, pos);
        if (curDist < closestDist)
        {
          closestDist = curDist;
          closestPos = other_pos;
          closest = e;
        }
      });
      if (ecs.is_valid(closest) && closestDist <= distance)
      {
        bb.set<flecs::entity>(entityBb, closest);
        res = BEH_SUCCESS;
      }
    });
    return res;
  }

};

struct FindEnemy : public BehNode
{
  size_t entityBb = size_t(-1);
  float distance = 0;
  FindEnemy(flecs::entity entity, float in_dist, const char *bb_name) : distance(in_dist)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_FAIL;
    static auto enemiesQuery = ecs.query<const Position, const Team>();
    entity.set([&](const Position &pos, const Team &t)
    {
      flecs::entity closestEnemy;
      float closestDist = FLT_MAX;
      Position closestPos;
      enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
      {
        if (t.team == et.team)
          return;
        float curDist = dist(epos, pos);
        if (curDist < closestDist)
        {
          closestDist = curDist;
          closestPos = epos;
          closestEnemy = enemy;
        }
      });
      if (ecs.is_valid(closestEnemy) && closestDist <= distance)
      {
        bb.set<flecs::entity>(entityBb, closestEnemy);
        res = BEH_SUCCESS;
      }
    });
    return res;
  }
};

struct FindWaypoint : public BehNode
{
  size_t entityBb = size_t(-1);
  FindWaypoint(flecs::entity entity, const char *bb_name)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    auto res = BEH_FAIL;
    entity.set([&](const Position &pos, CurrentWaypoint& waypoint)
    {
      waypoint.value.get([&](const Position& wpos, const NextWaypoint& next_waypoint){
        if (pos.x == wpos.x && pos.y == wpos.y) {
          waypoint.value = next_waypoint.value;
        }
      });
      if (ecs.is_valid(waypoint.value)) {
        res = BEH_SUCCESS;
        bb.set<flecs::entity>(entityBb, waypoint.value);
      }
    });
    return res;
  }
};

struct Flee : public BehNode
{
  size_t entityBb = size_t(-1);
  Flee(flecs::entity entity, const char *bb_name)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos)
    {
      flecs::entity targetEntity = bb.get<flecs::entity>(entityBb);
      if (!targetEntity.is_alive())
      {
        res = BEH_FAIL;
        return;
      }
      targetEntity.get([&](const Position &target_pos)
      {
        a.action = inverse_move(move_towards(pos, target_pos));
      });
    });
    return res;
  }
};

struct Patrol : public BehNode
{
  size_t pposBb = size_t(-1);
  float patrolDist = 1.f;
  Patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
    : patrolDist(patrol_dist)
  {
    pposBb = reg_entity_blackboard_var<Position>(entity, bb_name);
    entity.set([&](Blackboard &bb, const Position &pos)
    {
      bb.set<Position>(pposBb, pos);
    });
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos)
    {
      Position patrolPos = bb.get<Position>(pposBb);
      if (dist(pos, patrolPos) > patrolDist)
        a.action = move_towards(pos, patrolPos);
      else
        a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1); // do a random walk
    });
    return res;
  }
};

struct HoardListener : public BehNode
{
  size_t target_index = size_t(-1);
  size_t flag_index = size_t(-1);
  HoardListener(flecs::entity entity, const char *bb_name)
  {
    target_index = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
    flag_index = reg_entity_blackboard_var<bool>(entity, bb_name);
    entity.set([&](Blackboard &bb)
    {
      bb.set<bool>(flag_index, false);
    });
  }

  BehResult update(flecs::world &, flecs::entity, Blackboard &) override
  {
      return BEH_FAIL;
  }
  void react(flecs::world &, flecs::entity, Blackboard &bb, Event event) override {
    if (event.type == EventType::HoardAlert) {
      std::cout << "setting hoard target" << std::endl;
      bb.set<bool>(flag_index, true);
      bb.set<flecs::entity>(target_index, *static_cast<flecs::entity*>(event.custom_data));
    }
  }
};

struct AlertHoard : public BehNode
{
  float m_alert_dist;
  size_t m_bb_index;
  AlertHoard(flecs::entity entity, float alert_dist, const char *bb_name)
    : m_alert_dist(alert_dist)
  {
    m_bb_index = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
      auto enemy = bb.get<flecs::entity>(m_bb_index);
      if (!ecs.is_valid(enemy) || !enemy.is_alive()) {
        return BEH_FAIL;
      }
      size_t alert_num = 0;
      auto hasBehTree = ecs.query<const Position, BehaviourTree, Blackboard>();
      entity.get([&](const Position& e_pos){
          hasBehTree.each([&](flecs::entity e,const Position& pos, BehaviourTree& beh_tree, Blackboard& other_bb) {
            if (m_alert_dist >= dist(e_pos, pos)) {
                ++alert_num;
                beh_tree.event(ecs, e, other_bb, {
                    EventType::HoardAlert,
                    &enemy        
                });
            }
          });
      });
      std::cout << "alerting hoard... (" << alert_num << ") alerted" << std::endl;
      return BEH_SUCCESS;
  }
};


BehNode *hoard_listener(flecs::entity entity, const char *bb_name)
{
    return new HoardListener(entity, bb_name);
}

BehNode *alert_hoard(flecs::entity entity, float dist, const char *bb_name)
{
    return new AlertHoard(entity, dist, bb_name);
}

BehNode *sequence(const std::vector<BehNode*> &nodes)
{
  Sequence *seq = new Sequence;
  for (BehNode *node : nodes)
    seq->pushNode(node);
  return seq;
}

BehNode *selector(const std::vector<BehNode*> &nodes)
{
  Selector *sel = new Selector;
  for (BehNode *node : nodes)
    sel->pushNode(node);
  return sel;
}

BehNode *or_node(const std::vector<BehNode*> &nodes)
{
  return new OrNode(nodes);
}

BehNode *not_node(BehNode* node)
{
    return new NotNode({std::unique_ptr<BehNode>(node)});
}

BehNode *parallel(const std::vector<BehNode*> &nodes)
{
    return new Parallel(nodes);
}

BehNode *move_to_entity(flecs::entity entity, const char *bb_name)
{
  return new MoveToEntity(entity, bb_name);
}

BehNode *is_low_hp(float thres)
{
  return new IsLowHp(thres);
}

BehNode *find_enemy(flecs::entity entity, float dist, const char *bb_name)
{
  return new FindEnemy(entity, dist, bb_name);
}

BehNode *find_waypoint(flecs::entity entity, const char *bb_name)
{
  return new FindWaypoint(entity, bb_name);
}

BehNode *find_powerup(const flecs::world& ecs, flecs::entity entity, float dist, const char *bb_name)
{
  static const auto query = ecs.query<Position, PowerupAmount>();
  return new FindClosestOf(entity, dist, bb_name, query);
}

BehNode *find_heal(const flecs::world& ecs, flecs::entity entity, float dist, const char *bb_name)
{
  static const auto query = ecs.query<Position, HealAmount>();
  return new FindClosestOf(entity, dist, bb_name, query);
}

BehNode *flee(flecs::entity entity, const char *bb_name)
{
  return new Flee(entity, bb_name);
}

BehNode *patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
{
  return new Patrol(entity, patrol_dist, bb_name);
}

