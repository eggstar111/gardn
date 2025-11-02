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

        // 如果已经结束，则持续击杀非胜利队玩家并返回
        if (finished) {
            ++elapsed;
            if (elapsed >= 10 * TPS) {
                game_instance->broadcast_message("SERVER RESTARTING...");
                throw;  // 保持原逻辑，强制退出触发自动重启
            }

            kill_losers_continuously();
            return;
        }

        Simulation& sim = game_instance->simulation;

        // ----- 如果场上不存在 soccer，则生成一个 soccer 在 MAP_DATA[3] 中心处 -----
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
                // 使用 MAP_DATA[3] 的中心作为默认生成点（匹配你给出的公式，i 取 3）
                float cx = (MAP_DATA[3].left + MAP_DATA[3].right) * 0.5f;
                float cy = (MAP_DATA[3].top + MAP_DATA[3].bottom) * 0.5f;

                Entity& mob = alloc_mob(&sim, MobID::kSoccer, cx, cy, NULL_ENTITY);
                mob.set_parent(NULL_ENTITY);
                mob.set_color(ColorID::kGray);
                mob.base_entity = NULL_ENTITY;
            }
            else {
                // ----- 处理 soccer 进球判断 -----
                Entity& soccer = *soccer_ent;
                float sx = soccer.get_x();

                // 若足球进入红队门（x < MAP_DATA[0].right） => 红队进球
                if (sx < MAP_DATA[0].right) {
                    red_score += 1;
                    // 把足球重置到中场（MAP_DATA[3] 中心）
                    float cx = (MAP_DATA[3].left + MAP_DATA[3].right) * 0.5f;
                    float cy = (MAP_DATA[3].top + MAP_DATA[3].bottom) * 0.5f;
                    soccer.set_x(cx);
                    soccer.set_y(cy);
                    game_instance->broadcast_message(std::string("RED SCORED! Score: RED ") + std::to_string(red_score) + " - YELLOW " + std::to_string(yellow_score));
                }
                // 若足球进入黄队门（x > MAP_DATA[6].left） => 黄队进球
                else if (sx > MAP_DATA[6].left) {
                    yellow_score += 1;
                    float cx = (MAP_DATA[3].left + MAP_DATA[3].right) * 0.5f;
                    float cy = (MAP_DATA[3].top + MAP_DATA[3].bottom) * 0.5f;
                    soccer.set_x(cx);
                    soccer.set_y(cy);
                    game_instance->broadcast_message(std::string("YELLOW SCORED! Score: RED ") + std::to_string(red_score) + " - YELLOW " + std::to_string(yellow_score));
                }

                // ----- 检查是否有任一方达到 5 球（胜利条件） -----
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

    // 持续击杀非胜利队玩家（在 finished==true 时被调用）
    void kill_losers_continuously() {
        Simulation& sim = game_instance->simulation;
        if (winner_color < 0) return;

        for (uint16_t i = 0; i < ENTITY_CAP; ++i) {
            EntityID id(i, 0);
            Entity& ent = sim.get_ent(id);
            if (!ent.has_component(kFlower)) continue;
            if (static_cast<int>(ent.get_color()) != winner_color) {
                ent.health = 0; // 直接改 health，和 killallmobs 命令一致
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