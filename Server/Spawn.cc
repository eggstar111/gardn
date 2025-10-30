#include <Server/Spawn.hh>

#include <Server/EntityFunctions.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>

#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <cmath>

Entity &alloc_drop(Simulation *sim, PetalID::T drop_id) {
    DEBUG_ONLY(assert(drop_id < PetalID::kNumPetals);)
    PetalTracker::add_petal(sim, drop_id);
    Entity &drop = sim->alloc_ent();
    drop.add_component(kPhysics);
    drop.set_radius(25);
    drop.set_angle(frand() * 0.2 - 0.1);
    drop.friction = 0.25;

    drop.add_component(kRelations);
    drop.set_team(NULL_ENTITY);

    drop.add_component(kDrop);
    drop.set_drop_id(drop_id);
    entity_set_despawn_tick(drop, 10 * (2 + PETAL_DATA[drop_id].rarity) * TPS);
    drop.immunity_ticks = TPS / 3;
    return drop;
}

static Entity &__alloc_mob(Simulation *sim, MobID::T mob_id, float x, float y, EntityID const team = NULL_ENTITY) {
    DEBUG_ONLY(assert(mob_id < MobID::kNumMobs);)
    struct MobData const &data = MOB_DATA[mob_id];
    float seed = frand();
    Entity &mob = sim->alloc_ent();
    mob.add_component(kPhysics);
    mob.set_radius(data.radius.get_single(seed));
    mob.set_angle(frand() * 2 * M_PI);
    mob.set_x(x);
    mob.set_y(y);
    mob.friction = DEFAULT_FRICTION;
    if (mob_id == MobID::kSoccer) {
        mob.knockback = 6;
        mob.friction = DEFAULT_FRICTION / 2;
    }
    mob.mass = (1 + mob.get_radius() / BASE_FLOWER_RADIUS) * (data.attributes.stationary ? 10000 : 1);
    if (mob_id == MobID::kAntHole)
        BitMath::set(mob.flags, EntityFlags::kNoFriendlyCollision);
    if (team == NULL_ENTITY)
        BitMath::set(mob.flags, EntityFlags::kHasCulling);
        
    mob.add_component(kRelations);
    mob.set_team(team);

    mob.add_component(kMob);
    mob.set_mob_id(mob_id);

    mob.add_component(kHealth);
    mob.health = mob.max_health = data.health.get_single(seed);
    mob.damage = data.damage;
    mob.poison_damage = data.attributes.poison_damage;
    mob.set_health_ratio(1);

    mob.detection_radius = data.attributes.aggro_radius;
    mob.score_reward = data.xp;

    mob.add_component(kName);
    mob.set_name(data.name);

    mob.base_entity = mob.id;
    if (mob_id == MobID::kDigger) {
        mob.add_component(kFlower);
        mob.set_angle(0);
        mob.set_color(ColorID::kGray);
    }
    if (mob_id == MobID::kFallenFlower) {
        mob.set_nametag_visible(1);
        mob.add_component(kFlower);
        mob.set_angle(0);
        mob.set_color(ColorID::kGray);
        mob.set_score(level_to_score(30));
        mob.set_loadout_count(loadout_slots_at_level(30));
        mob.health = mob.max_health = hp_at_level(30);
        std::vector<PetalID::T> fixed_loadout = {
           PetalID::kDahlia,
           PetalID::kSalt,
           PetalID::kDahlia,
           PetalID::kBubble,
           PetalID::kStinger,
           PetalID::kIris,
           PetalID::kStinger,
           PetalID::kDandelion,
        };
        if (frand() < 0.5)  mob.ff_ai = 1;
        if (mob.ff_ai == 1) {
            mob.set_score(level_to_score(45));
            mob.set_loadout_count(loadout_slots_at_level(45));
            mob.health = mob.max_health = hp_at_level(45);
           fixed_loadout = {
                       PetalID::kWing,
                       PetalID::kFaster,
                       PetalID::kWing,
                       PetalID::kSalt,
                       PetalID::kWing,
                       PetalID::kAzalea,
                       PetalID::kWing,
                       PetalID::kDandelion,
                       PetalID::kWing,
            };
        }
        // ����ɫ����
        for (uint32_t i = 0; i < fixed_loadout.size(); ++i) {
            PetalID::T pid = fixed_loadout[i];
            mob.set_inventory(i, pid);
            LoadoutSlot& slot = mob.loadout[i];
            mob.set_loadout_ids(i, pid);
            slot.update_id(sim, pid);
            slot.force_reload();
        }
        for (uint32_t i = 0; i < loadout_slots_at_level(30); ++i)
            PetalTracker::add_petal(sim, mob.get_inventory(i));
    }
    return mob;
}

Entity &alloc_mob(Simulation *sim, MobID::T mob_id, float x, float y, EntityID const team) {
    struct MobData const &data = MOB_DATA[mob_id];
    if (data.attributes.segments <= 1) {
        Entity &ent = __alloc_mob(sim, mob_id, x, y, team);
        if (mob_id == MobID::kAntHole) {
            std::vector<MobID::T> const spawns = { 
                MobID::kBabyAnt, MobID::kBabyAnt, MobID::kBabyAnt, 
                MobID::kWorkerAnt, MobID::kWorkerAnt, MobID::kSoldierAnt
            };
            for (MobID::T mob_id : spawns) {
                Vector rand = Vector::rand(ent.get_radius() * 2);
                Entity &ant = __alloc_mob(sim, mob_id, x + rand.x, y + rand.y, team);
                ant.set_parent(ent.id);
            }
        }
        else if (mob_id == MobID::kTargetDummy || mob_id == MobID::kFallenFlower) {
            ent.set_angle(0);
        }
        return ent;
    }
    else {
        Entity &head = __alloc_mob(sim, mob_id, x, y, team);
        //head.add_component(kSegmented);
        Entity *curr = &head;
        for (uint32_t i = 1; i < data.attributes.segments; ++i) {
            Entity &seg = __alloc_mob(sim, mob_id, x, y, team);
            seg.add_component(kSegmented);
            seg.seg_head = curr->id;
            seg.set_angle(curr->get_angle() + frand() * 0.1 - 0.05);
            seg.set_x(curr->get_x() - (curr->get_radius() + seg.get_radius()) * cosf(seg.get_angle()));
            seg.set_y(curr->get_y() - (curr->get_radius() + seg.get_radius()) * sinf(seg.get_angle()));
            curr = &seg;
        }
        return head;
    }
}

Entity &alloc_player(Simulation *sim, EntityID const team) {
    Entity &player = sim->alloc_ent();

    player.add_component(kPhysics);
    player.set_radius(BASE_FLOWER_RADIUS);
    player.friction = DEFAULT_FRICTION;
    player.mass = 1;

    player.add_component(kFlower);

    player.add_component(kRelations);
    player.set_team(team);

    player.add_component(kHealth);
    player.health = player.max_health = BASE_HEALTH;
    player.set_health_ratio(1);
    player.damage = BASE_BODY_DAMAGE;
    player.immunity_ticks = 1.0 * TPS;

    player.add_component(kScore);

    player.add_component(kName);
    player.set_nametag_visible(1);

    player.base_entity = player.id;
    return player;
}

Entity &alloc_petal(Simulation *sim, PetalID::T petal_id, Entity const &parent) {
    DEBUG_ONLY(assert(petal_id < PetalID::kNumPetals);)
    struct PetalData const &petal_data = PETAL_DATA[petal_id];
    Entity &petal = sim->alloc_ent();
    petal.add_component(kPhysics);
    petal.set_x(parent.get_x());
    petal.set_y(parent.get_y());
    petal.set_radius(petal_data.radius);
    if (petal_data.attributes.rotation_style == PetalAttributes::kPassiveRot)
        petal.set_angle(frand() * 2 * M_PI);
    petal.mass = petal_data.attributes.mass;
    petal.friction = DEFAULT_FRICTION * 1.5;
    petal.add_component(kRelations);
    petal.set_parent(parent.id);
    petal.set_team(parent.get_team());
    petal.set_color(parent.get_color());
    petal.add_component(kPetal);
    petal.set_petal_id(petal_id);
    petal.add_component(kHealth);
    petal.health = petal.max_health = petal_data.health;
    petal.damage = petal_data.damage;
    petal.set_health_ratio(1);
    petal.poison_damage = petal_data.attributes.poison_damage;
    petal.slow_inflict = TPS * petal_data.attributes.slow_inflict;
    petal.armor = petal_data.attributes.armor;

    if (parent.id == NULL_ENTITY) petal.base_entity = petal.id;
    else petal.base_entity = parent.id;
    return petal;
}

Entity &alloc_web(Simulation *sim, float radius, Entity const &parent) {
    Entity &web = sim->alloc_ent();
    web.add_component(kPhysics);
    web.set_x(parent.get_x());
    web.set_y(parent.get_y());
    web.set_angle(frand() * 2 * M_PI);
    web.set_radius(radius);
    web.mass = 1.0;
    web.friction = 1.0;
    web.add_component(kRelations);
    web.set_team(parent.get_team());
    web.set_parent(parent.id);
    web.set_color(parent.get_color());
    web.add_component(kWeb);
    entity_set_despawn_tick(web, 10 * TPS);
    return web;
}

Entity& alloc_poison_web(Simulation* sim, float radius, Entity const& parent) {
    Entity& poison_web = sim->alloc_ent();
    poison_web.add_component(kPhysics);
    poison_web.set_x(parent.get_x());
    poison_web.set_y(parent.get_y());
    poison_web.set_angle(frand() * 2 * M_PI);
    poison_web.set_radius(radius);
    poison_web.mass = 1.0;
    poison_web.friction = 1.0;
    poison_web.add_component(kRelations);
    poison_web.set_team(parent.get_team());
    poison_web.set_parent(parent.id);
    poison_web.add_component(kPoisonWeb);
    entity_set_despawn_tick(poison_web, 10 * TPS);
    return poison_web;
}

void player_spawn(Simulation *sim, Entity &camera, Entity &player) {
    camera.set_player(player.id);
    player.set_parent(camera.id);
    player.set_color(camera.get_color());
    uint32_t power = Map::difficulty_at_level(camera.get_respawn_level());
    ZoneDefinition const& zone = (player.get_color() == ColorID::kRed)
        ? MAP_DATA[6] 
        : MAP_DATA[0]; 
    float spawn_x = lerp(zone.left, zone.right, frand());
    float spawn_y = lerp(zone.top, zone.bottom, frand());
    camera.set_camera_x(spawn_x);
    camera.set_camera_y(spawn_y);
    player.set_x(spawn_x);
    player.set_y(spawn_y);
    player.set_score(level_to_score(camera.get_respawn_level()));
    player.set_loadout_count(loadout_slots_at_level(camera.get_respawn_level()));
    player.health = player.max_health = hp_at_level(camera.get_respawn_level());
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        PetalID::T id = camera.get_inventory(i);
        LoadoutSlot &slot = player.loadout[i];
        player.set_loadout_ids(i, id);
        slot.update_id(sim, id);
        slot.force_reload();
    }

    for (uint32_t i = player.get_loadout_count(); i < player.get_loadout_count() + MAX_SLOT_COUNT; ++i)
        player.set_loadout_ids(i, camera.get_inventory(i));

    //peaceful transfer, no petal tracking needed
    for (uint32_t i = 0; i < MAX_SLOT_COUNT * 2; ++i)
        camera.set_inventory(i, PetalID::kNone);
}