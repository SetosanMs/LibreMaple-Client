//////////////////////////////////////////////////////////////////////////////
// This file is part of the LibreMaple MMORPG client                        //
// Copyright © 2015-2016 Daniel Allendorf, 2018-2019 LibreMaple Team        //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "SetfieldHandlers.h"

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Console.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UICharSelect.h"
#include "../../IO/Window.h"
#include "../../Timer.h"
#include "../Packets/GameplayPackets.h"
#include "Helpers/ItemParser.h"
#include "Helpers/LoginParser.h"

namespace jrc
{
void SetfieldHandler::transition(std::int32_t map_id,
                                 std::uint8_t portal_id) const
{
    static constexpr const float fade_step = 0.025f;

    Window::get().fadeout(fade_step, [map_id, portal_id] {
        GraphicsGL::get().clear();
        Stage::get().load(map_id, portal_id);
        UI::get().enable();
        Timer::get().start();
        GraphicsGL::get().unlock();
    });

    GraphicsGL::get().lock();
    Stage::get().clear();
    Timer::get().start();
}

void SetfieldHandler::handle(InPacket& recv) const
{
    std::int32_t channel = recv.read_int();
    Stage::get().set_channel(static_cast<std::uint8_t>(channel));
    std::int8_t mode1 = recv.read_byte();
    std::int8_t mode2 = recv.read_byte();
    if (mode1 == 0 && mode2 == 0) {
        change_map(recv, channel);
    } else {
        set_field(recv);
    }
}

void SetfieldHandler::change_map(InPacket& recv, std::int32_t) const
{
    recv.skip(3);

    std::int32_t map_id = recv.read_int();
    auto portal_id = static_cast<std::uint8_t>(recv.read_byte());

    transition(map_id, portal_id);

    PlayerUpdatePacket{}.dispatch();
}

void SetfieldHandler::set_field(InPacket& recv) const
{
    recv.skip(23);

    std::int32_t cid = recv.read_int();

    auto charselect = UI::get().get_element<UICharSelect>();
    if (charselect) {
        const CharEntry& player_entry = charselect->get_character(cid);
        if (player_entry.cid != cid) {
            return;
        }

        Stage::get().loadplayer(player_entry);
    }

    LoginParser::parse_stats(recv);

    Player& player = Stage::get().get_player();

    recv.read_byte(); // 'buddycap'
    if (recv.read_bool()) {
        recv.read_string(); // 'linkedname'
    }

    parse_inventory(recv, player.get_inventory());
    parse_skillbook(recv, player.get_skills());
    parse_cooldowns(recv, player);
    parse_questlog(recv, player.get_quests());
    parse_minigame(recv);
    parse_ring1(recv);
    parse_ring2(recv);
    parse_ring3(recv);
    parse_telerock(recv, player.get_telerock());
    parse_monsterbook(recv, player.get_monsterbook());
    parse_nyinfo(recv);
    parse_areainfo(recv);

    player.recalc_stats(true);

    std::uint8_t portal_id = player.get_stats().get_portal();
    std::int32_t map_id = player.get_stats().get_map_id();

    transition(map_id, portal_id);

    PlayerUpdatePacket{}.dispatch();

    Sound{Sound::GAME_START}.play();

    UI::get().change_state(UI::GAME);
}

void SetfieldHandler::parse_inventory(InPacket& recv, Inventory& invent) const
{
    invent.set_meso(recv.read_int());
    invent.set_slotmax(InventoryType::EQUIP,
                       static_cast<std::uint8_t>(recv.read_byte()));
    invent.set_slotmax(InventoryType::USE,
                       static_cast<std::uint8_t>(recv.read_byte()));
    invent.set_slotmax(InventoryType::SETUP,
                       static_cast<std::uint8_t>(recv.read_byte()));
    invent.set_slotmax(InventoryType::ETC,
                       static_cast<std::uint8_t>(recv.read_byte()));
    invent.set_slotmax(InventoryType::CASH,
                       static_cast<std::uint8_t>(recv.read_byte()));

    recv.skip(8);

    for (std::size_t i = 0; i < 3; ++i) {
        InventoryType::Id inv
            = i == 0 ? InventoryType::EQUIPPED : InventoryType::EQUIP;
        std::int16_t pos = recv.read_short();
        while (pos != 0) {
            std::int16_t slot = i == 1 ? -pos : pos;
            ItemParser::parse_item(recv, inv, slot, invent);
            pos = recv.read_short();
        }
    }

    recv.skip(2);

    InventoryType::Id toparse[4] = {InventoryType::USE,
                                    InventoryType::SETUP,
                                    InventoryType::ETC,
                                    InventoryType::CASH};

    for (auto inv : toparse) {
        std::int8_t pos = recv.read_byte();
        while (pos != 0) {
            ItemParser::parse_item(recv, inv, pos, invent);
            pos = recv.read_byte();
        }
    }
}

void SetfieldHandler::parse_skillbook(InPacket& recv, Skillbook& skills) const
{
    std::int16_t size = recv.read_short();
    for (std::int16_t i = 0; i < size; ++i) {
        std::int32_t skill_id = recv.read_int();
        std::int32_t level = recv.read_int();
        std::int64_t expiration = recv.read_long();
        bool fourthtjob = ((skill_id % 100000) / 10000 == 2);
        std::int32_t masterlevel = fourthtjob ? recv.read_int() : 0;
        skills.set_skill(skill_id, level, masterlevel, expiration);
    }
}

void SetfieldHandler::parse_cooldowns(InPacket& recv, Player& player) const
{
    std::int16_t size = recv.read_short();
    for (std::int16_t i = 0; i < size; ++i) {
        std::int32_t skill_id = recv.read_int();
        std::int32_t cooltime = recv.read_short();
        player.add_cooldown(skill_id, cooltime);
    }
}

void SetfieldHandler::parse_questlog(InPacket& recv, Questlog& quests) const
{
    std::int16_t size = recv.read_short();
    for (std::int16_t i = 0; i < size; ++i) {
        std::int16_t qid = recv.read_short();
        if (quests.is_started(qid)) {
            std::int16_t qidl = quests.get_last_started();
            quests.add_in_progress(qidl, qid, recv.read_string());
            --i;
        } else {
            quests.add_started(qid, recv.read_string());
        }
    }

    size = recv.read_short();
    for (std::int16_t i = 0; i < size; ++i) {
        std::int16_t qid = recv.read_short();
        std::int64_t time = recv.read_long();
        quests.add_completed(qid, time);
    }
}

void SetfieldHandler::parse_ring1(InPacket& recv) const
{
    std::int16_t rsize = recv.read_short();
    for (std::int16_t i = 0; i < rsize; ++i) {
        recv.read_int();
        recv.read_padded_string(13);
        recv.read_int();
        recv.read_int();
        recv.read_int();
        recv.read_int();
    }
}

void SetfieldHandler::parse_ring2(InPacket& recv) const
{
    std::int16_t rsize = recv.read_short();
    for (std::int16_t i = 0; i < rsize; ++i) {
        recv.read_int();
        recv.read_padded_string(13);
        recv.read_int();
        recv.read_int();
        recv.read_int();
        recv.read_int();
        recv.read_int();
    }
}

void SetfieldHandler::parse_ring3(InPacket& recv) const
{
    std::int16_t rsize = recv.read_short();
    for (std::int16_t i = 0; i < rsize; ++i) {
        recv.read_int();
        recv.read_int();
        recv.read_int();
        recv.read_short();
        recv.read_int();
        recv.read_int();
        recv.read_padded_string(13);
        recv.read_padded_string(13);
    }
}

void SetfieldHandler::parse_minigame(InPacket& recv) const
{
    std::int16_t mgsize = recv.read_short();
    for (std::int16_t i = 0; i < mgsize; ++i) {
        // TODO
    }
}

void SetfieldHandler::parse_monsterbook(InPacket& recv,
                                        Monsterbook& monsterbook) const
{
    monsterbook.set_cover(recv.read_int());

    recv.skip(1);

    std::int16_t size = recv.read_short();
    for (std::int16_t i = 0; i < size; ++i) {
        std::int16_t cid = recv.read_short();
        std::int8_t mblv = recv.read_byte();

        monsterbook.add_card(cid, mblv);
    }
}

void SetfieldHandler::parse_telerock(InPacket& recv, Telerock& trock) const
{
    for (std::size_t i = 0; i < 5; ++i) {
        trock.addlocation(recv.read_int());
    }

    for (std::size_t i = 0; i < 10; ++i) {
        trock.addviplocation(recv.read_int());
    }
}

void SetfieldHandler::parse_nyinfo(InPacket& recv) const
{
    std::int16_t nysize = recv.read_short();
    for (std::int16_t i = 0; i < nysize; ++i) {
        // TODO
    }
}

void SetfieldHandler::parse_areainfo(InPacket& recv) const
{
    // std::unordered_map<std::int16_t, std::string> area_info;
    std::int16_t ar_size = recv.read_short();
    for (std::int16_t i = 0; i < ar_size; ++i) {
        [[maybe_unused]] std::int16_t area = recv.read_short();
        recv.read_string(); // area_info[area] = recv.read_string();
    }
}
} // namespace jrc
