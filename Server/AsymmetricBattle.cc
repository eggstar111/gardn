#include "AsymmetricBattle.hh"
#include <Server/Game.hh>         
#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>
#include <chrono>
#include <string>

struct AsymmetricBattle::AsymmetricBattleInternal {
    explicit AsymmetricBattleInternal(GameInstance* game)
        : game_instance(game),
        finished(false),
        winner_color(-1),
        last_yellow_dummies(0),
        last_red_dummies(0){ }

    void tick() {
        if (finished) {
            kill_losers_continuously();
            if (!restart_message_sent) {
                auto now = std::chrono::steady_clock::now();
                int elapsed = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::seconds>(now - finish_time).count()
                    );
                if (elapsed >= 10) {
                    game_instance->broadcast_message("SERVER RESTARTING...");
                    restart_message_sent = true;
                    throw; // 或者调用服务器重启函数
                }
            }
            return;
        }
        Simulation& sim = game_instance->simulation;
        int yellow_dummies = 0;
        int red_dummies = 0;

        sim.for_each<kMob>([&](Simulation* sim, Entity const& ent) {
            if (ent.get_mob_id() != MobID::kTargetDummy) return;

            if (ent.get_color() == ColorID::kYellow) yellow_dummies++;
            else if (ent.get_color() == ColorID::kRed) red_dummies++;
            });


        // 检测新增 dummy 并播报
        if (yellow_dummies > last_yellow_dummies) {
            game_instance->broadcast_message("A dummy has been captured by YELLOW!");
        }
        if (red_dummies > last_red_dummies) {
            game_instance->broadcast_message("A dummy has been captured by RED!");
        }

        last_yellow_dummies = yellow_dummies;
        last_red_dummies = red_dummies;

        // 胜利条件
        if (yellow_dummies >= 5) {
            finished = true;
            finish_time = std::chrono::steady_clock::now();
            winner_color = static_cast<int>(ColorID::kYellow);
            game_instance->broadcast_message("YELLOW HAS WON THE GAME!");
            kill_losers_continuously();
            return;
        }
        else if (red_dummies >= 5) {
            finished = true;
            finish_time = std::chrono::steady_clock::now();
            winner_color = static_cast<int>(ColorID::kRed);
            game_instance->broadcast_message("RED HAS WON THE GAME!");
            kill_losers_continuously();
            return;
        }
    }

    void kill_losers_continuously() {
        if (winner_color < 0) return;

        Simulation& sim = game_instance->simulation;
        for (uint16_t i = 0; i < ENTITY_CAP; ++i) {
            EntityID id(i, 0);
            if (!sim.ent_exists(id)) continue;
            Entity& ent = sim.get_ent(id);
            if (!ent.has_component(kFlower)) continue;
            if (static_cast<int>(ent.get_color()) != winner_color) {
                ent.health = 0; // 直接击杀非胜利队玩家
            }
        }
    }

    GameInstance* game_instance;
    bool finished;
    int winner_color;
    int last_yellow_dummies;
    int last_red_dummies;
    std::chrono::steady_clock::time_point finish_time;
    bool restart_message_sent = false;
};

// ---------------- AsymmetricBattle 对外接口 ----------------
AsymmetricBattle::AsymmetricBattle(GameInstance* game)
    : internal(new AsymmetricBattleInternal(game)) {
}

void AsymmetricBattle::update() {
    internal->tick();
}
