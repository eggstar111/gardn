#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Game.hh>

using namespace Ui;

static Element *make_divider() {
    return new Element(400, 6, { .fill = 0x20000000, .line_width = 3, .round_radius = 3, .v_justify = Style::Top });
}

static Element *make_date(std::string const date) {
    return new Ui::StaticText(20, date, { .fill = 0xffffffff, .h_justify = Style::Left, .v_justify = Style::Top });
}

static Element *make_paragraph(std::string const contents) {
    return new Ui::HContainer({
        new Ui::StaticText(14, "-", { .fill = 0xffffffff, .v_justify = Style::Top }),
        new Ui::StaticParagraph(360, 14, contents)
    }, 0, 10, { .h_justify = Style::Left, .v_justify = Style::Top });
}

static Element *make_entries(std::initializer_list<std::string const> contents) {
    Element *elt = new Ui::VContainer({}, 5, 3, { .h_justify = Style::Left, .v_justify = Style::Top });
    for (std::string const str : contents)
        elt->add_child(make_paragraph(str));
    return elt;
}

static Element *make_changelog_contents() {
    Element *elt = new Ui::VContainer({
        make_date("2025年8月21日"),
        make_entries({
            "修复了因为我忘了0 % n == 0为真导致蚁后无限繁殖的蚆蛒",
            "游戏已开源。链接：https://github.com/XNORIGC/gardn",
            "（复刻自https://github.com/trigonal-bacon/gardn）",
            "删除了我忘了删除的为了测试运算符优先级而随机编写的表达式",
            "修复了更新日志单行过长的问题",
            "格式化了一些代码",
            "游戏现在有开发分支了。链接：http://gardn-dev.camvan.xyz/",
            "开源：https://github.com/XNORIGC/gardn/tree/dev",
            "复制了几个有用的配置文件到开源仓库里",
            "拉取了原仓库代码",
            "删除了我不知道怎么打出来的艾特符号",
            "编辑了一些索引文件",
            "拉取了maxnest0x0/gardn仓库的代码",
            "游戏现在有聊天了，感谢maxnest0x0。按[Enter]以聊天",
            "唯一品质的花瓣的粒子效果颜色改为了黑色",
            "神话品质的花瓣现在有了粒子效果",
            "是唯一品质花瓣粒子效果改颜色前的样子",
            "添加了命令。用聊天来调用命令。不过目前在生产分支下不可用",
            "添加了手动恢复分数和花瓣的功能，感谢maxnest0x0",
            "这适用于那些在游戏中途离开但没有死亡的玩家",
            "但这仅在服务端采用WASM编译时可用，而我们采用的是原生编译",
            "现在玩家在重生时也会给一片玫瑰了",
            "聊天最大长度乘二",
            "现在玩家每10级就能获得额外的花瓣槽，之前是15级",
            "玩家重生时选择区域的算法不变",
            "现在生物不会在玩家面前贴脸自然生成了",
            "提高了花瓣槽上限从8至12"
        }),
        make_divider(),
        make_date("2025年8月20日"),
        make_entries({
            "日志自此始。以北京时间记",
            "游戏现在可玩了。链接：http://gardn.camvan.xyz/",
            "（注意是HTTP不是HTTPS，因为我懒得配置SSL证书）",
            "切换至二队模式",
            "现在玩家在初生时会给一片玫瑰了",
            "生物经验和掉落率乘三。掉落率如果大于1则为1",
            "增强了骨头的护甲从4至14",
            "为玩家分配队伍更公平了",
            "削弱了蝎子和蜘蛛的追赶速度，它们现在不再能追上花朵了",
            "蒲公英现在可发射了",
            "配置了WS_URL",
            "花粉不再取代花朵掉落物，而是作为额外掉落物，并且概率乘三",
            "蚁后生成的兵蚁不再自然消失",
            "并且蚁后每十分钟生成一只蚁后",
            "并且生成的蚁后也不会自然消失",
            "并且生成的蚁后也会生成兵蚁和蚁后",
            "削弱了泡泡的加速度从30至20",
            "修改了WS_URL",
            "昵称最大长度乘二",
            "挖掘者出现概率乘三"
        }),
        make_divider(),
        make_date("August 20th 2025"),
        make_entries({
            "Added chat"
        }),
        make_divider(),
        make_date("August 14th 2025"),
        make_entries({
            "Added ability to manually restore score and petals for players who leave the game without dying"
        }),
        make_divider(),
        make_date("August 10th 2025"),
        make_entries({
            "Added TDM",
            "Made some faster mobs slower",
            "Various bugfixes"
        }),
        make_divider(),
        make_date("August 4th 2025"),
        make_entries({
            "Increased max slot number to 12"
        }),
        make_divider(),
        make_date("July 31st 2025"),
        make_entries({
            "Added 1 new petal",
            "Changed initial petal spawn delay from 2.5s to 1s"
        }),
        make_divider(),
        make_date("June 15th 2025"),
        make_entries({
            "Finished base game",
            "Added 3 new petals",
            "Drop rates are now autobalanced",
            "Spawning a petal for the first time now takes 2.5 extra seconds"
        }),
        make_divider(),
        make_date("June 11th 2025"),
        make_entries({
            "Added 1 new mob",
            "Balanced some mob stats",
            "Revamped petal descriptions"
        }),
        make_divider(),
        make_date("June 7th 2025"),
        make_entries({
            "Finished changelog",
            "Tweaked some petal stats",
            "Added settings and mob gallery"
        }),
        make_divider(),
        make_date("June 6th 2025"),
        make_entries({
            "Added Changelog",
            "Added 2 new petals",
            "Added 1 new mob",
        }),
        make_divider(),
        make_date("March 23rd 2025"),
        make_entries({
            "Game is now playable",
            "Added spawner petals"
        }),
        make_divider(),
        new Ui::StaticText(14, "Older changelog entries not available")
    }, 0, 10, {});
    return new Ui::ScrollContainer(elt, 300);
}

Element *Ui::make_changelog() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(25, "Changelog"),
        make_changelog_contents()
    }, 15, 10, { 
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
        },
        .should_render = [](){
            return Ui::panel_open == Panel::kChangelog && Game::should_render_title_ui();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    Ui::Panel::changelog = elt;
    return elt;
}
