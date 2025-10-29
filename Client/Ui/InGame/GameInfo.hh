#pragma once

#include <Client/Ui/Element.hh>
#include <Client/Ui/TextInput.hh>
namespace Ui {
    class LevelBar final : public Element {
    public:
        LerpFloat progress;
        uint32_t level;
        LevelBar();
        virtual void on_render(Renderer &) override;
    };

    class LeaderboardSlot final : public Element {
    public:
        uint8_t pos;
        LerpFloat ratio;
        LeaderboardSlot(uint8_t);

        virtual void on_render(Renderer &) override;
    };

    class Minimap final : public Element {
    public:
        Minimap(float);
        virtual void on_render(Renderer &) override;
    };

    class Chat final : public TextInput {
    public:
        Chat(std::string& ref, float width, float height, uint32_t max, Style s = {});

        virtual void on_render(Renderer&) override;
        virtual void on_render_skip(Renderer&) override;
    };

    class BroadcastDisplay final : public Element {
    public:
        BroadcastDisplay();
        virtual void on_render(Renderer&) override;
    };

    class OverlevelTimer final : public Element {
    public:
        OverlevelTimer(float);
        virtual void on_render(Renderer &) override;
    };

    class MobileJoyStick final : public Element {
        float joystick_x;
        float joystick_y;
        float joystick_radius;
        uint32_t persistent_touch_id;
        uint8_t is_pressed;
    public:
        MobileJoyStick(float, float, float);
        virtual void on_render(Renderer &) override;
        virtual void on_event(uint8_t) override;
    };

    Element *make_leaderboard();
    Element *make_level_bar();
    Element *make_minimap();
    Element *make_chat();
    Element *make_broadcast_display();
    Element *make_overlevel_indicator();
    Element *make_mobile_attack_button();
    Element *make_mobile_defend_button();
    Element* make_mobile_yy_button();
    Element *make_mobile_joystick();
}