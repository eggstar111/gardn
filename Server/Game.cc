#include <Server/Game.hh>

#include <Server/Client.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>
#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/Map.hh>

static void _update_client(Simulation *sim, Client *client) {
    if (client == nullptr) return;
    if (!client->verified) return;
    if (sim == nullptr) return;
    if (!sim->ent_exists(client->camera)) return;
    std::set<EntityID> in_view;
    std::vector<EntityID> deletes;
    in_view.insert(client->camera);
    Entity &camera = sim->get_ent(client->camera);
    if (sim->ent_exists(camera.get_player())) 
        in_view.insert(camera.get_player());
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kClientUpdate);
    writer.write<EntityID>(client->camera);
    sim->spatial_hash.query(camera.get_camera_x(), camera.get_camera_y(), 
    960 / camera.get_fov() + 50, 540 / camera.get_fov() + 50, [&](Simulation *, Entity &ent){
        in_view.insert(ent.id);
    });

    sim->for_each<kMob>([&](Simulation*, Entity& ent) {
        if (ent.get_mob_id() == MobID::kTargetDummy) {
            in_view.insert(ent.id);
        }
    });

    sim->for_each<kFlower>([&](Simulation*, Entity& ent) {
        if (ent.id != client->camera && ent.get_team() == camera.get_team()) {
            in_view.insert(ent.id);
        }
    });

    for (EntityID const &i: client->in_view) {
        if (!in_view.contains(i)) {
            writer.write<EntityID>(i);
            deletes.push_back(i);
        }
    }

    for (EntityID const &i : deletes)
        client->in_view.erase(i);

    writer.write<EntityID>(NULL_ENTITY);
    //upcreates
    for (EntityID id: in_view) {
        DEBUG_ONLY(assert(sim->ent_exists(id));)
        Entity &ent = sim->get_ent(id);
        uint8_t create = !client->in_view.contains(id);
        writer.write<EntityID>(id);
        writer.write<uint8_t>(create | (ent.pending_delete << 1));
        ent.write(&writer, BitMath::at(create, 0));
        client->in_view.insert(id);
    }
   
    writer.write<EntityID>(NULL_ENTITY);
    //write arena stuff
    writer.write<uint8_t>(client->seen_arena);
    sim->arena_info.write(&writer, client->seen_arena);
    client->seen_arena = 1;
    client->send_packet(writer.packet, writer.at - writer.packet);
}

GameInstance::GameInstance() : simulation(), clients(), team_manager(&simulation) {}

void GameInstance::init() {
    for (uint32_t i = 0; i < ENTITY_CAP / 2; ++i)
        Map::spawn_random_mob(&simulation, frand() * ARENA_WIDTH, frand() * ARENA_HEIGHT);
    #ifdef GAMEMODE_TDM
    team_manager.add_team(ColorID::kYellow);
    team_manager.add_team(ColorID::kRed);
    for (uint32_t i = 1; i <= 5; ++i) {
        float cx = (MAP_DATA[i].left + MAP_DATA[i].right) * 0.5f;
        float cy = (MAP_DATA[i].top + MAP_DATA[i].bottom) * 0.5f;
        Entity& mob = alloc_mob(&simulation, MobID::kTargetDummy, cx, cy, NULL_ENTITY);
        mob.set_parent(NULL_ENTITY);
        mob.set_color(ColorID::kGray);
        mob.base_entity = NULL_ENTITY;
    }
    #endif
}

void GameInstance::tick() {
    simulation.tick();
    for (Client *client : clients)
        _update_client(&simulation, client);
    simulation.post_tick();
}

void GameInstance::add_client(Client *client) {
    DEBUG_ONLY(assert(client->game != this);)
    if (client->game != nullptr)
        client->game->remove_client(client);
    client->game = this;
    clients.insert(client);
    Entity &ent = simulation.alloc_ent();
    ent.add_component(kCamera);
    ent.add_component(kRelations);
    #ifdef GAMEMODE_TDM
    EntityID team = team_manager.get_random_team();
    ent.set_team(team);
    ent.set_color(simulation.get_ent(team).get_color());
    ++simulation.get_ent(team).player_count;
    #else
    ent.set_team(ent.id);
    ent.set_color(ColorID::kYellow); 
    #endif
    
    ent.set_fov(BASE_FOV);
    ent.set_respawn_level(1);
        for (uint32_t i = 0; i < loadout_slots_at_level(ent.get_respawn_level()); ++i)
            ent.set_inventory(i, PetalID::kBasic);
        ent.set_inventory(loadout_slots_at_level(ent.get_respawn_level()), PetalID::kRose);
        ent.set_inventory(loadout_slots_at_level(ent.get_respawn_level()) + 1, PetalID::kBubble);

        if (frand() < 0.0001 && PetalTracker::get_count(&simulation, PetalID::kUniqueBasic) == 0)
            ent.set_inventory(0, PetalID::kUniqueBasic);
    for (uint32_t i = 0; i < loadout_slots_at_level(ent.get_respawn_level()); ++i)
        PetalTracker::add_petal(&simulation, ent.get_inventory(i));
    client->camera = ent.id;
    client->seen_arena = 0;
}

void GameInstance::remove_client(Client *client) {
    DEBUG_ONLY(assert(client->game == this);)
    clients.erase(client);
    if (simulation.ent_exists(client->camera)) {
        Entity &c = simulation.get_ent(client->camera);
        if (simulation.ent_exists(c.get_team()))
            --simulation.get_ent(c.get_team()).player_count;
        if (simulation.ent_exists(c.get_player()))
            simulation.request_delete(c.get_player());
        for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
            PetalTracker::remove_petal(&simulation, c.get_inventory(i));
        simulation.request_delete(client->camera);
    }
    client->game = nullptr;
}

void GameInstance::chat(EntityID sender, std::string const& text) {
    for (Client* client : clients) {
        // �ж�ͬ�Ӻ��Ӿ�
        Simulation* sim = &simulation;
        if (!sim->ent_exists(sender)) continue;

        Entity& sender_ent = sim->get_ent(sender);
        Entity& camera = sim->get_ent(client->camera);

        bool send = false;
        if (sender_ent.get_team() == camera.get_team()) {
            send = true; // ͬ����Զ�ɼ�
        }
        else if (client->in_view.contains(sender_ent.id)) {
            send = true;
        }

        if (send) {
            Writer writer(Server::OUTGOING_PACKET);
            writer.write<uint8_t>(Clientbound::kChat);
            writer.write<EntityID>(sender);   // ����ʵ�� ID
            writer.write<std::string>(text);
            client->send_packet(writer.packet, writer.at - writer.packet);
        }
    }
}

void GameInstance::broadcast_message(std::string const& msg) {
    for (Client* client : clients) {

        Writer writer(Server::OUTGOING_PACKET);
        writer.write<uint8_t>(Clientbound::kBroadcast); // ������ö������
        writer.write<std::string>(msg);

        client->send_packet(writer.packet, writer.at - writer.packet);
    }
}