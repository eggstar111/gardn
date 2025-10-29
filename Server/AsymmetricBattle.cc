#include "AsymmetricBattle.hh"
#include <Server/Game.hh>         // GameInstance (Server::game)
#include <Server/EntityFunctions.hh>    // inflict_damage, DamageType
#include <Server/Spawn.hh>   
#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>           // MobID, ColorID
#include <chrono>
#include <cmath>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <climits>


struct AsymmetricBattle::AsymmetricBattleInternal {
    explicit AsymmetricBattleInternal(GameInstance* game)
        : game_instance(game),
        started(false),
        finished(false),
        restart_message_sent(false),
        last_broadcast_second(-1),
        elapsed(0),
        winner_color(-1),
        red_score(0),
        yellow_score(0) {
    }

    void tick() {
        using namespace std::chrono;

        // ����Ѿ��������������ɱ��ʤ������Ҳ�����
        if (finished) {
            ++elapsed;
            if (elapsed >= 10 * TPS) {
                game_instance->broadcast_message("SERVER RESTARTING...");
                throw;  // ����ԭ�߼���ǿ���˳������Զ�����
            }

            kill_losers_continuously();
            return;
        }

        Simulation& sim = game_instance->simulation;
        auto& team_manager = game_instance->get_team_manager();

        // ----- ��������ƽ�⣨����ԭ���߼��� -----
        {
            Entity& yellow_team = sim.get_ent(team_manager.get_team(0));
            Entity& red_team = sim.get_ent(team_manager.get_team(1));

            uint32_t yellow_count = yellow_team.player_count;
            uint32_t red_count = red_team.player_count;
            if (red_count  > yellow_count + 1) {
                Entity* worst_player = nullptr;
                int lowest_score = INT_MAX;

                // �ҷ�����͵ĺ�����
                for (uint16_t i = 0; i < ENTITY_CAP; ++i) {
                    EntityID id(i, 0);
                    Entity& ent = sim.get_ent(id);
                    if (!ent.has_component(kFlower)) continue;
                    if (ent.get_color() != ColorID::kRed) continue;

                    int score = ent.get_score();
                    if (score < lowest_score) {
                        lowest_score = score;
                        worst_player = &ent;
                    }
                }

                if (worst_player) {
                    Entity& old_camera = sim.get_ent(worst_player->get_parent());
                    old_camera.set_color(ColorID::kYellow);
                    old_camera.set_team(yellow_team.id);
                    old_camera.set_player(worst_player->id);
                    
                    worst_player->set_color(ColorID::kYellow);
                    worst_player->set_team(yellow_team.id);
                    worst_player->set_parent(old_camera.id);
                    worst_player->health = 0;
                    yellow_team.player_count++;
                    red_team.player_count--;
                    game_instance->broadcast_message("A RED player has been forced to join Yellow for balance");
                }
            }
            if (yellow_count > red_count + 1) {
                Entity* worst_player = nullptr;
                int lowest_score = INT_MAX;

                // �ҷ�����͵Ļƶ����
                for (uint16_t i = 0; i < ENTITY_CAP; ++i) {
                    EntityID id(i, 0);
                    Entity& ent = sim.get_ent(id);
                    if (!ent.has_component(kFlower)) continue;
                    if (ent.get_color() != ColorID::kYellow) continue;

                    int score = ent.get_score();
                    if (score < lowest_score) {
                        lowest_score = score;
                        worst_player = &ent;
                    }
                }

                if (worst_player) {
                    Entity& old_camera = sim.get_ent(worst_player->get_parent());

                    // ת���ɺ��
                    old_camera.set_color(ColorID::kRed);
                    old_camera.set_team(red_team.id);
                    old_camera.set_player(worst_player->id);
                    worst_player->set_color(ColorID::kRed);
                    worst_player->set_team(red_team.id);
                    worst_player->set_parent(old_camera.id);
                    worst_player->health = 0;
                    yellow_team.player_count--;
                    red_team.player_count++;

                    game_instance->broadcast_message("A Yellow player has been forced to join RED for balance");
                }
            }
        }

        // ----- ������ϲ����� soccer��������һ�� soccer �� MAP_DATA[3] ���Ĵ� -----
        {
            bool soccer_exists = false;
            Entity* soccer_ent = nullptr;

            for (uint16_t i = 0; i < ENTITY_CAP; ++i) {
                EntityID id(i, 0);
                if (!sim.ent_exists(id)) continue;
                if (!sim.ent_alive(id)) continue;
                Entity& ent = sim.get_ent(id);
                if (ent.get_mob_id() == MobID::kSoccer) {
                    soccer_exists = true;
                    soccer_ent = &ent;
                    break;
                }
            }

            if (!soccer_exists) {
                // ʹ�� MAP_DATA[3] ��������ΪĬ�����ɵ㣨ƥ��������Ĺ�ʽ��i ȡ 3��
                float cx = (MAP_DATA[3].left + MAP_DATA[3].right) * 0.5f;
                float cy = (MAP_DATA[3].top + MAP_DATA[3].bottom) * 0.5f;

                Entity& mob = alloc_mob(&sim, MobID::kSoccer, cx, cy, NULL_ENTITY);
                mob.set_parent(NULL_ENTITY);
                mob.set_color(ColorID::kGray);
                mob.base_entity = NULL_ENTITY;
            }
            else {
                // ----- ���� soccer �����ж� -----
                Entity& soccer = *soccer_ent;
                float sx = soccer.get_x();

                // ������������ţ�x < MAP_DATA[0].right�� => ��ӽ���
                if (sx < MAP_DATA[0].right) {
                    red_score += 1;
                    // ���������õ��г���MAP_DATA[3] ���ģ�
                    float cx = (MAP_DATA[3].left + MAP_DATA[3].right) * 0.5f;
                    float cy = (MAP_DATA[3].top + MAP_DATA[3].bottom) * 0.5f;
                    soccer.set_x(cx);
                    soccer.set_y(cy);
                    game_instance->broadcast_message(std::string("RED SCORED! Score: RED ") + std::to_string(red_score) + " - YELLOW " + std::to_string(yellow_score));
                }
                // ���������ƶ��ţ�x > MAP_DATA[6].left�� => �ƶӽ���
                else if (sx > MAP_DATA[6].left) {
                    yellow_score += 1;
                    float cx = (MAP_DATA[3].left + MAP_DATA[3].right) * 0.5f;
                    float cy = (MAP_DATA[3].top + MAP_DATA[3].bottom) * 0.5f;
                    soccer.set_x(cx);
                    soccer.set_y(cy);
                    game_instance->broadcast_message(std::string("YELLOW SCORED! Score: RED ") + std::to_string(red_score) + " - YELLOW " + std::to_string(yellow_score));
                }

                // ----- ����Ƿ�����һ���ﵽ 5 ��ʤ�������� -----
                const int GOAL_TO_WIN = 5;
                if (red_score >= GOAL_TO_WIN) {
                    finished = true;
                    winner_color = static_cast<int>(ColorID::kRed);
                    game_instance->broadcast_message("RED HAS WON THE GAME!");
                    kill_losers_continuously();
                    return;
                }
                if (yellow_score >= GOAL_TO_WIN) {
                    finished = true;
                    winner_color = static_cast<int>(ColorID::kYellow);
                    game_instance->broadcast_message("YELLOW HAS WON THE GAME!");
                    kill_losers_continuously();
                    return;
                }
            }
        }

    }

    // ������ɱ��ʤ������ң��� finished==true ʱ�����ã�
    void kill_losers_continuously() {
        Simulation& sim = game_instance->simulation;
        if (winner_color < 0) return;

        for (uint16_t i = 0; i < ENTITY_CAP; ++i) {
            EntityID id(i, 0);
            Entity& ent = sim.get_ent(id);
            if (!ent.has_component(kFlower)) continue;
            if (static_cast<int>(ent.get_color()) != winner_color) {
                ent.health = 0; // ֱ�Ӹ� health���� killallmobs ����һ��
            }
        }
    }

    GameInstance* game_instance;
    bool started;
    bool finished;
    bool restart_message_sent;
    int last_broadcast_second;
    int elapsed;
    int winner_color;
    int red_score;
    int yellow_score;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point finish_time;
};

AsymmetricBattle::AsymmetricBattle(GameInstance* game)
    : internal(new AsymmetricBattleInternal(game)) {
}

void AsymmetricBattle::update() {
    internal->tick();
}