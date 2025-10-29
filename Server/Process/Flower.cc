#include <Server/Process.hh>

#include <Server/EntityFunctions.hh>
#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <cmath>

struct PlayerBuffs {
    float extra_rot;
    float extra_range;
    float heal;
    float extra_vision;
    float extra_health;
    float movement_speed = 1;
    float reduce_reload = 1;
    float extra_body_damage = 0;
    float extra_radius = 0;
    float poison_armor = 0;
    float damage_reflection = 0;
    uint8_t yinyang_count;
    uint8_t is_poisonous : 1;
    uint8_t has_cutter : 1;
    uint8_t equip_flags;
};

static void redirect_to_nearby_enemy(Simulation* sim, Vector& target_pos, Entity& self) {
    Entity* nearest_enemy = nullptr;
    float nearest_dist_sq = 999999;

    sim->for_each<kFlower>([&](Simulation* s, Entity& flower) {
        if (flower.get_team() == self.get_team()) return;

        float dx = flower.get_x() - target_pos.x;
        float dy = flower.get_y() - target_pos.y;
        float dist_sq = dx * dx + dy * dy;

        float threshold = 30.0f + flower.get_radius();
        if (dist_sq < threshold * threshold) {
            if (!nearest_enemy || dist_sq < nearest_dist_sq) {
                nearest_enemy = &flower;
                nearest_dist_sq = dist_sq;
            }
        }
        });

    if (nearest_enemy) {
        target_pos.x = nearest_enemy->get_x();
        target_pos.y = nearest_enemy->get_y();
    }
}

static struct PlayerBuffs _get_petal_passive_buffs(Simulation *sim, Entity &player) {
    struct PlayerBuffs buffs = {0};
    if (player.has_component(kMob) && player.get_mob_id() != MobID::kFallenFlower) return buffs;
    player.set_equip_flags(0);
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        LoadoutSlot const &slot = player.loadout[i];
        PetalID::T slot_petal_id = slot.get_petal_id();
        struct PetalData const &petal_data = PETAL_DATA[slot_petal_id];
        if (petal_data.attributes.equipment != EquipmentFlags::kNone)
            player.set_equip_flags(player.get_equip_flags() | (1 << petal_data.attributes.equipment));
    
        if (slot_petal_id == PetalID::kCutter) {
            buffs.has_cutter = 1;
        }
        else if (slot_petal_id == PetalID::kCorruption) {
            buffs.extra_health += petal_data.attributes.extra_health;
            player.set_radius(BASE_FLOWER_RADIUS * 1.15);
        }
        else if (slot_petal_id == PetalID::kYinYang) {
            ++buffs.yinyang_count;
        }
        if (petal_data.attributes.reduce_reload) buffs.reduce_reload *= petal_data.attributes.reduce_reload;
        buffs.extra_range += petal_data.attributes.extra_range;
        buffs.extra_vision = std::max(buffs.extra_vision, petal_data.attributes.extra_vision);
        buffs.movement_speed += petal_data.attributes.movement_speed;
        buffs.extra_body_damage += petal_data.attributes.extra_body_damage;
        buffs.extra_radius += petal_data.attributes.extra_radius;
        buffs.damage_reflection += petal_data.attributes.damage_reflection;
        buffs.poison_armor += petal_data.attributes.poison_armor / TPS;
        if (!player.loadout[i].already_spawned) continue;
        buffs.extra_health += petal_data.attributes.extra_health;
        buffs.extra_rot += petal_data.attributes.extra_rot;
        if (slot_petal_id == PetalID::kLeaf) 
            buffs.heal += petal_data.attributes.constant_heal / TPS;
        else if (slot_petal_id == PetalID::kYucca && BitMath::at(player.input, InputFlags::kDefending) && !BitMath::at(player.input, InputFlags::kAttacking)) 
            buffs.heal += petal_data.attributes.constant_heal / TPS;
        else if (slot_petal_id == PetalID::kPoisonCactus) {
            buffs.is_poisonous = 1;
        }
    }
    return buffs;
}

static uint32_t _get_petal_rotation_count(Simulation *sim, Entity &player) {
    uint32_t count = 0;
    for (uint8_t i = 0; i < player.get_loadout_count(); ++i) {
        LoadoutSlot const &slot = player.loadout[i];
        struct PetalData const &petal_data = PETAL_DATA[slot.get_petal_id()];
        if (petal_data.attributes.clump_radius > 0)
            ++count;
        else {
            for (uint32_t j = 0; j < slot.size(); ++j) {
                if (!sim->ent_alive(slot.petals[j].ent_id))
                    ++count;
                else if (!sim->get_ent(slot.petals[j].ent_id).has_component(kMob))
                    ++count;
            }
        }
    }
    return count;
}

void tick_player_behavior(Simulation *sim, Entity &player) {
    if (player.pending_delete) return;
    DEBUG_ONLY(assert(player.max_health > 0);)
    PlayerBuffs const buffs = _get_petal_passive_buffs(sim, player);
    float health_ratio = player.health / player.max_health;
    if (!player.has_component(kMob) || player.get_mob_id() == MobID::kFallenFlower) {
        player.max_health = hp_at_level(score_to_level(player.get_score())) + buffs.extra_health;
        player.damage = BASE_BODY_DAMAGE + buffs.extra_body_damage;
        player.set_radius(BASE_FLOWER_RADIUS + buffs.extra_radius);
        player.poison_armor = buffs.poison_armor;
        player.damage_reflection = buffs.damage_reflection;
    }
    if (player.hunter != 0) {
        uint32_t existing_count = 0;

        // �������� mob��ͳ�� prey Ϊ��ǰ��ҵ�����
        sim->for_each<kMob>([&](Simulation* sim_ptr, Entity& mob) {
            if (mob.prey == player.id) {
                ++existing_count;
            }
            });

        int to_spawn = player.hunter - existing_count;
        if (to_spawn > 0) {
            float radius = 1000.0f;
            float start_angle = frand() * 2.0f * M_PI; // �����ʼ�Ƕ�
            float angle_step = 2.0f * M_PI / to_spawn;
            const float angle_increment = 20.0f * M_PI / 180.0f; // ��ǽʱ���� 20 ��

            for (int i = 0; i < to_spawn; ++i) {
                float angle = start_angle + i * angle_step;

                for (int attempt = 0; attempt < 18; ++attempt) { // ��ೢ�� 18 �� (360��)
                    float x = player.get_x() + radius * cosf(angle);
                    float y = player.get_y() + radius * sinf(angle);

                    // ����Ƿ�Խ��
                    if (x >= MAP_DATA[0].left && x <= MAP_DATA[6].right &&
                        y >= MAP_DATA[0].top && y <= MAP_DATA[0].bottom) {
                        // �Ϸ�λ�ã����� mob
                        Entity& mob = alloc_mob(sim, MobID::kFallenFlower, x, y, NULL_ENTITY);
                        mob.prey = player.id;
                        BitMath::set(mob.flags, EntityFlags::kNoDrops);
                        mob.set_name("Hunter");
                        break; // ���ɳɹ����˳�����ѭ��
                    }

                    angle = fmod(angle + angle_increment, 2.0f * M_PI);
                    if (angle < 0.0f) angle += 2.0f * M_PI; 
                }
            }
        }
    }
    player.health = health_ratio * player.max_health;
    if (buffs.heal > 0)
        inflict_heal(sim, player, buffs.heal);
    if (buffs.is_poisonous)
        player.poison_damage = {10.0, 2};
    else
        player.poison_damage = {0, 0};
    
    float rot_pos = 0;
    uint32_t rotation_count = _get_petal_rotation_count(sim, player);
    //maybe use delta mode for face flags?
    player.set_face_flags(0);

    if (sim->ent_alive(player.get_parent())) {
        Entity &camera = sim->get_ent(player.get_parent());
        camera.set_fov(BASE_FOV * (1 - buffs.extra_vision));
    }
    player.speed_ratio *= buffs.movement_speed;
    PetalID::T unstackable_spawned[MAX_SLOT_COUNT];
    uint32_t unstackable_count = 0;
    DEBUG_ONLY(assert(player.get_loadout_count() <= MAX_SLOT_COUNT);)
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        LoadoutSlot &slot = player.loadout[i];
        //player.set_loadout_ids(i, slot.id);
        //other way around. loadout_ids should dictate loadout
        if (slot.get_petal_id() != player.get_loadout_ids(i) || player.get_overlevel_timer() >= PETAL_DISABLE_DELAY * TPS)
            slot.update_id(sim, player.get_loadout_ids(i));
        PetalID::T slot_petal_id = slot.get_petal_id();
        struct PetalData const &petal_data = PETAL_DATA[slot_petal_id];
        DEBUG_ONLY(assert(petal_data.count <= MAX_PETALS_IN_CLUMP);)

            // ����Ƿ�Ϊ unstackable ����
            if (petal_data.attributes.unstackable) {
                bool already_spawned = false;

                // ���������������ж��Ƿ��Ѿ�����
                for (uint32_t j = 0; j < unstackable_count; ++j) {
                    if (unstackable_spawned[j] == slot_petal_id) {
                        already_spawned = true;
                        break;
                    }
                }

                if (already_spawned) {
                    // ɾ������Ļ���
                    sim->request_delete(slot.petals[0].ent_id);
                    slot.reset();
                    player.set_loadout_reloads(i, 0);
                    continue;
                }

                // ��һ�γ��ָû��꣬��¼
                unstackable_spawned[unstackable_count++] = slot_petal_id;
            }

        if (slot_petal_id == PetalID::kNone || petal_data.count == 0)
            continue;
        //if overleveled timer too large
        if (player.get_overlevel_timer() >= PETAL_DISABLE_DELAY * TPS) {
            player.set_loadout_reloads(i, 0);
            continue;
        }
        float min_reload = 1;
        for (uint32_t j = 0; j < slot.size(); ++j) {
            LoadoutPetal &petal_slot = slot.petals[j];
            if (!sim->ent_alive(petal_slot.ent_id)) {
                petal_slot.ent_id = NULL_ENTITY;
                game_tick_t reload_time = (petal_data.reload * TPS);
                reload_time *= buffs.reduce_reload;
                if (!slot.already_spawned) reload_time += TPS;
                float this_reload = reload_time == 0 ? 1 : (float) petal_slot.reload / reload_time;
                min_reload = std::min(min_reload, this_reload);
                if (petal_slot.reload >= reload_time) {
                    petal_slot.ent_id = alloc_petal(sim, slot_petal_id, player).id;
                    petal_slot.reload = 0;
                    slot.already_spawned = 1;
                } 
                else
                    ++petal_slot.reload;
            }
            if (sim->ent_alive(petal_slot.ent_id)) {
                Entity &petal = sim->get_ent(petal_slot.ent_id);
                //only do this if petal not despawning
                if (petal.has_component(kPetal) && !(BitMath::at(petal.flags, EntityFlags::kIsDespawning))) {
                    //petal rotation behavior
                    Vector wanting;
                    Vector delta(player.get_x() - petal.get_x(), player.get_y() - petal.get_y());
                    if (rotation_count > 0)
                        wanting.unit_normal(2 * M_PI * rot_pos / rotation_count + player.heading_angle);

                    float range = player.get_radius() + 40;
                    if (BitMath::at(player.input, InputFlags::kAttacking)) { 
                        if (petal_data.attributes.defend_only == 0) 
                            range = player.get_radius() + 100 + buffs.extra_range;
                        if (petal.get_petal_id() == PetalID::kWing || petal.get_petal_id() == PetalID::kTriWing) {
                            float wave = sinf((float) petal.lifetime / (0.4 * TPS));
                            wave = wave * wave;
                            range += wave * 120;
                        }
                    }
                    else if (BitMath::at(player.input, InputFlags::kDefending)) range = player.get_radius() + 15;
                    wanting *= range;
                    if (petal_data.attributes.clump_radius > 0) {
                        Vector secondary;
                        secondary.unit_normal(2 * M_PI * j / petal_data.count + player.heading_angle * 0.2)
                        .set_magnitude(petal_data.attributes.clump_radius;
                        wanting += secondary;
                    }
                    wanting += delta;
                    wanting *= 0.5;
                    Vector final_target = Vector(petal.get_x() + wanting.x, petal.get_y() + wanting.y);

                    // ֻ�о߱��������ԵĻ���ų����ض���
                    if (petal_data.attributes.lock) {
                        redirect_to_nearby_enemy(sim, final_target, petal);
                    }

                    // ���ռ��ٶ�
                    petal.acceleration = final_target - Vector(petal.get_x(), petal.get_y());
                    game_tick_t sec_reload_ticks = petal_data.attributes.secondary_reload * TPS;
                    if (petal_data.attributes.spawns != MobID::kNumMobs &&
                        petal.secondary_reload > sec_reload_ticks) {
                        uint8_t spawn_id = petal_data.attributes.spawns;
                        Entity &mob = alloc_mob(sim, spawn_id, petal.get_x(), petal.get_y(), petal.get_team());
                        mob.set_parent(player.id);
                        mob.set_color(player.get_color());
                        mob.base_entity = player.id;
                        BitMath::set(mob.flags, EntityFlags::kDieOnParentDeath);
                        BitMath::set(mob.flags, EntityFlags::kNoDrops);
                        if (petal_data.attributes.spawn_count == 0) {
                            petal_slot.ent_id = mob.id;
                            sim->request_delete(petal.id);
                            break;
                        } else {
                            entity_set_despawn_tick(mob, sec_reload_ticks * petal_data.attributes.spawn_count);
                            petal.secondary_reload = 0;
                            //needed
                            mob.set_parent(petal.id);
                            mob.base_entity = player.id;
                        }
                    }
                } else {
                    //if petal is a mob, or detached (IsDespawning)
                    if (BitMath::at(petal.flags, EntityFlags::kIsDespawning))
                        petal_slot.ent_id = NULL_ENTITY;
                    if (petal.has_component(kMob))
                        --rot_pos;
                }
            }
            //spread out
            if (petal_data.attributes.clump_radius == 0) ++rot_pos;
        }
        //clump
        if (petal_data.attributes.clump_radius > 0) ++rot_pos;
        player.set_loadout_reloads(i, min_reload * 255);
    };
    if (BitMath::at(player.input, InputFlags::kAttacking)) 
        player.set_face_flags(player.get_face_flags() | (1 << FaceFlags::kAttacking));
    else if (BitMath::at(player.input, InputFlags::kDefending))
        player.set_face_flags(player.get_face_flags() | (1 << FaceFlags::kDefending));
    if (player.poison_ticks > 0)
        player.set_face_flags(player.get_face_flags() | (1 << FaceFlags::kPoisoned));
    if (player.dandy_ticks > 0)
        player.set_face_flags(player.get_face_flags() | (1 << FaceFlags::kDandelioned));
    if (buffs.yinyang_count != MAX_SLOT_COUNT) {
        switch (buffs.yinyang_count % 3) {
            case 0:
                player.heading_angle += (BASE_PETAL_ROTATION_SPEED + buffs.extra_rot) / TPS;
                break;
            case 1:
                player.heading_angle -= (BASE_PETAL_ROTATION_SPEED + buffs.extra_rot) / TPS;
                break;
            default:
                break;
        }
    } else 
        player.heading_angle += 10 * (BASE_PETAL_ROTATION_SPEED + buffs.extra_rot) / TPS;
}