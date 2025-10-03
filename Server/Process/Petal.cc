#include <Server/Process.hh>

#include <Server/EntityFunctions.hh>
#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <cmath>

void tick_petal_behavior(Simulation *sim, Entity &petal) {
    if (petal.pending_delete) return;
    if (!sim->ent_alive(petal.get_parent())) {
        sim->request_delete(petal.id);
        return;
    }
    Entity &player = sim->get_ent(petal.get_parent());
    struct PetalData const &petal_data = PETAL_DATA[petal.get_petal_id()];
    if (petal_data.attributes.rotation_style == PetalAttributes::kPassiveRot) {
        //simulate on clientside
        float rot_amt =(petal.get_petal_id() == PetalID::kWing || petal.get_petal_id() == PetalID::kTriWing)? 10.0f: 1.0f;
        if (petal.id.id % 2) petal.set_angle(petal.get_angle() + rot_amt / TPS);
        else petal.set_angle(petal.get_angle() - rot_amt / TPS);
    } else if (petal_data.attributes.rotation_style == PetalAttributes::kFollowRot && !(BitMath::at(petal.flags, EntityFlags::kIsDespawning))) {
        Vector delta(petal.get_x() - player.get_x(), petal.get_y() - player.get_y());
        petal.set_angle(delta.angle());
    }
    if (BitMath::at(petal.flags, EntityFlags::kIsDespawning)) {
        switch (petal.get_petal_id()) {
            case PetalID::kMissile: 
            case PetalID::kDandelion:
            case PetalID::kBullet:
            case PetalID::kDestroyerBullet:{
                petal.acceleration.unit_normal(petal.get_angle()).set_magnitude(4 * PLAYER_ACCELERATION);
                break;
            }
            case PetalID::kMoon: {
                petal.acceleration.unit_normal(petal.get_angle()).set_magnitude(2 * PLAYER_ACCELERATION);
                break;
            }
            case PetalID::kDrone: {
                petal.acceleration.unit_normal(petal.get_angle()).set_magnitude(2.5 * PLAYER_ACCELERATION);
                break;
            }
            default:
                petal.acceleration.set(0,0);
                break;
        }
    }
    else if (petal_data.attributes.secondary_reload > 0) {
        if (petal.secondary_reload > petal_data.attributes.secondary_reload * TPS) {
            if (petal_data.attributes.burst_heal > 0 && player.health < player.max_health && player.dandy_ticks == 0) {
                Vector delta(player.get_x() - petal.get_x(), player.get_y() - petal.get_y());
                if (delta.magnitude() < petal.get_radius()) {
                    inflict_heal(sim, player, petal_data.attributes.burst_heal);
                    sim->request_delete(petal.id);
                    return;
                }
                delta.set_magnitude(PLAYER_ACCELERATION * 4);
                petal.acceleration = delta;
            }
            switch (petal.get_petal_id()) {
                case PetalID::kMissile:
                case PetalID::kDandelion:
                case PetalID::kBullet:
                case PetalID::kDestroyerBullet:
                    if (BitMath::at(player.input, InputFlags::kAttacking)) {
                        petal.acceleration.unit_normal(petal.get_angle()).set_magnitude(4 * PLAYER_ACCELERATION);
                        entity_set_despawn_tick(petal, 3 * TPS);
                    }
                    break;
                case PetalID::kTriweb:
                case PetalID::kPoisonWeb:
                case PetalID::kWeb: {
                    if (BitMath::at(player.input, InputFlags::kAttacking)) {
                        Vector delta(petal.get_x() - player.get_x(), petal.get_y() - player.get_y());
                        petal.friction = DEFAULT_FRICTION;
                        float angle = delta.angle();
                        if (petal.get_petal_id() == PetalID::kTriweb) angle += frand() - 0.5;
                        petal.acceleration.unit_normal(angle).set_magnitude(30 * PLAYER_ACCELERATION);
                        entity_set_despawn_tick(petal, 0.6 * TPS);
                    } else if (BitMath::at(player.input, InputFlags::kDefending))
                        entity_set_despawn_tick(petal, 0.6 * TPS);
                    break;
                }
                case PetalID::kBubble:
                    if (BitMath::at(player.input, InputFlags::kDefending)) {
                        Vector v(player.get_x() - petal.get_x(), player.get_y() - petal.get_y());
                        v.set_magnitude(PLAYER_ACCELERATION * 20);
                        player.velocity += v;
                        sim->request_delete(petal.id);
                    }
                    break;
                case PetalID::kPollen:
                    if (BitMath::at(player.input, InputFlags::kAttacking) || BitMath::at(player.input, InputFlags::kDefending)) {
                        petal.friction = DEFAULT_FRICTION;
                        entity_set_despawn_tick(petal, 4.0 * TPS);
                    }
                    break;
                case PetalID::kPeas:
                case PetalID::kPoisonPeas:
                case PetalID::kPoisonPeas2:
                    if (BitMath::at(player.input, InputFlags::kAttacking)) {
                        Vector delta(petal.get_x() - player.get_x(), petal.get_y() - player.get_y());
                        petal.friction = DEFAULT_FRICTION / 10;
                        petal.acceleration.unit_normal(delta.angle()).set_magnitude(10 * PLAYER_ACCELERATION);
                        entity_set_despawn_tick(petal, TPS);
                    }
                    break;
                case PetalID::kMoon: {
                    if (BitMath::at(player.input, InputFlags::kAttacking)) {
                        petal.acceleration.unit_normal(petal.get_angle()).set_magnitude(4 * PLAYER_ACCELERATION);
                        entity_set_despawn_tick(petal, 10 * TPS);
                    }
                    break;
                }
                case PetalID::kDrone: {
                    petal.acceleration.unit_normal(petal.get_angle()).set_magnitude(4 * PLAYER_ACCELERATION);
                    entity_set_despawn_tick(petal, 15 * TPS);
                }
                default:
                    break;
            }
        } else petal.secondary_reload++;
    }
}