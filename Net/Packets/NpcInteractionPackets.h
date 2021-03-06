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
#pragma once
#include "../OutPacket.h"

namespace jrc
{
//! Packet which requests a dialogue with a server-sided npc.
//! Opcode: TALK_TO_NPC(58)
class TalkToNPCPacket : public OutPacket
{
public:
    TalkToNPCPacket(std::int32_t oid) : OutPacket(TALK_TO_NPC)
    {
        write_int(oid);
    }
};

//! Packet which sends a response to an npc dialogue to the server.
//! Opcode: NPC_TALK_MORE(60)
class NpcTalkMorePacket : public OutPacket
{
public:
    NpcTalkMorePacket(std::int8_t last_msg, std::int8_t response)
        : OutPacket(NPC_TALK_MORE)
    {
        write_byte(last_msg);
        write_byte(response);
    }

    NpcTalkMorePacket(std::string_view response) : NpcTalkMorePacket(2, 1)
    {
        write_string(response);
    }

    NpcTalkMorePacket(std::int32_t selection) : NpcTalkMorePacket(4, 1)
    {
        write_int(selection);
    }
};

//! Packet which tells the server of an interaction with an npc shop.
//! Opcode: NPC_SHOP_ACTION(61)
class NpcShopActionPacket : public OutPacket
{
public:
    //! Requests that an item should be bought from or sold to a npc shop.
    NpcShopActionPacket(std::int16_t slot,
                        std::int32_t itemid,
                        std::int16_t qty,
                        bool buy)
        : NpcShopActionPacket(buy ? BUY : SELL)
    {
        write_short(slot);
        write_int(itemid);
        write_short(qty);
    }

    //! Requests that an item should be recharged at a npc shop.
    NpcShopActionPacket(std::int16_t slot) : NpcShopActionPacket(RECHARGE)
    {
        write_short(slot);
    }

    //! Requests exiting from a npc shop.
    NpcShopActionPacket() : NpcShopActionPacket(LEAVE)
    {
    }

protected:
    enum Mode : std::int8_t { BUY, SELL, RECHARGE, LEAVE };

    NpcShopActionPacket(Mode mode) : OutPacket(NPC_SHOP_ACTION)
    {
        write_byte(mode);
    }
};
} // namespace jrc
