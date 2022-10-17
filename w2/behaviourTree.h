#pragma once

#include <flecs.h>
#include <memory>
#include "blackboard.h"

enum BehResult
{
  BEH_SUCCESS,
  BEH_FAIL,
  BEH_RUNNING
};

enum class EventType {
    HoardAlert
};

struct Event {
    EventType type;
    void* custom_data;
};

struct BehNode
{
  virtual ~BehNode() {}
  virtual BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) = 0;
  virtual void react(flecs::world &, flecs::entity, Blackboard &, Event) {}
};

struct BehaviourTree
{
  std::unique_ptr<BehNode> root = nullptr;

  BehaviourTree() = default;
  BehaviourTree(BehNode *r) : root(r) {}

  BehaviourTree(const BehaviourTree &bt) = delete;
  BehaviourTree(BehaviourTree &&bt) = default;

  BehaviourTree &operator=(const BehaviourTree &bt) = delete;
  BehaviourTree &operator=(BehaviourTree &&bt) = default;

  ~BehaviourTree() = default;

  void update(flecs::world &ecs, flecs::entity entity, Blackboard &bb)
  {
    root->update(ecs, entity, bb);
  }

  void event(flecs::world &ecs, flecs::entity entity, Blackboard &bb, Event event){
    root->react(ecs, entity, bb, event);
  }
};

