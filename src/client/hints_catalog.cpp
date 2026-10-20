#include "client/hints_catalog.hpp"

#include <algorithm>
#include <array>

namespace tibia::client {
namespace {

constexpr std::array<AvailableHint, 14> hint_catalog{{
    {1, L"First Login", LR"hint(<b>Welcome to Tibia!
<br=7><n>Greetings wanderer! You are about to enter the mysterious world of Tibia. Fight fearsome monsters, find untold treasures and make your character a true hero of the land! There are many different ways to play Tibia, but keep in mind that there are many others players out there who want to enjoy the game just like you, so please play fair and follow the Tibia Rules at all times.
<br=7>To move your character use the cursor keys or click on a spot on the game window or in the automap. Right-clicking on items, monsters, characters or simply on the map will open context menus in which you can select appropriate actions such as attacking or using things. Items can be moved around by using drag&drop.
<br=7>If you need assistance, click on the 'Help' button to the right. There are several help options to choose from. For instance, the 'Client Help' will provide you with information on important objects on the map or parts of the client program. Also, a flashing exclamation mark will appear in the top right corner from time to time to inform you that further hints like this one you are reading right now are available. To review previous hints click on 'Help', then 'Hints'. For detailed information please read the manual and the 'FAQ' on our offical website www.tibia.com.)hint"},
    {2, L"NPC Cipfried", LR"hint(You have met an NPC (Non-Player-Character) called Cipfried. To talk to NPCs or other players that are close to you type any text you wish to say on the keyboard and press Enter. You will find your conversations are logged in the lower section of the adventure screen. This section of the screen is called the 'Default Console'.
<br=5>To start a conversation with Cipfried say 'hello'. NPCs react on certain keywords. For example, if you would like to find out more an NPCs you should ask him about his 'job' or 'name'.
<br=5>Note that Cipfried can give you certain 'hints', and he can 'heal' you if your are heavily wounded or poisoned. To find out more you should try out further keywords. Conversations are generally ended by saying 'bye'.)hint"},
    {3, L"Rookgaard Crossroad", LR"hint(You have taken your first steps into the village of Rookgaard. To the south you can see the temple where you have started out. Cipfried, the kind priest living there, will gladly heal some of your wounds whenever you are seriously injured. It is a good idea to visit the academy now. Go north until you reach a large building. The Academy's famous library contains many important books on Tibia's history and on its inhabitants. Also, Seymour, the Academy's master, is happy to chat with all new citizens.
<br=7>To the east you will find Obi's shop. The old smith sells and buys weapons, armors, and equipment. Moreover, there are further shops in the village you should take a look at.
<br=7>To get an idea of what combat is like it is a good idea to look for some animals to hunt. Follow the path to the west or the east in order to enter the woods close to the village. Don't worry - you won't run into any dangerous enemies there. Once you feel prepared for your first battle you should start exploring the village sewers. You can enter them through the sewer grates.)hint"},
    {4, L"NPC Seymour", LR"hint(The academy is the village's main building. It is run by Seymour. Seymour always has some 'mission' for new Tibians. Please read the hints on the boards.
<br=7>The library contains important books concerning various aspects of Tibia, including its history and its fauna. A small training centre has been set up in the cellar where new players can try out some basic elements of the game.)hint"},
    {5, L"Buying & Selling", LR"hint(In this shop you can trade with an NPC. Just greet him or her with a hearty 'hello' to start a conversation. You will receive a list of the items that are in stock if you ask shop owners about their 'offers'. If you are interested in buying an object type 'buy <name of object>'. The shop owner will tell you the price and ask you to confirm the deal. Type 'yes' and hit Enter if you agree to the trade.
<br=7>NPC shop owners typically buy objects, too. If you would like to earn an extra gold coin by selling some spare object type 'sell <name of object>'. Again you will be asked to confirm your offer by typing 'yes' in order for the trade to take place.)hint"},
    {6, L"Underground", LR"hint(You have now entered a dungeon. These areas are often dangerous because they are popular hideouts of all kinds of nasty creatures. If you should bump into a monster you try fighting it. Right-click onto the creature and select 'Attack'.
<br=7>It is a good idea to carefully watch your hit points during battle. If you lose too many hit points in too short a time you should run for it. Remember that even though death in Tibia is not final, it is a costly matter because you may loose hard-earned abilities or objects.
<br=7>To get a detailed list of all creatures close to you press the 'Battle' button to the right to open the battle window. You can use the battle window to attack monsters. Simply right-click on the creature's name on the list and select 'Attack'.
<br=7>For details on combat modes use the 'Client Help' and place the magnifying glass on the combat control buttons to the right hand side of the screen.Note that dungeons are dark and dreary places, so you should always make sure to have a light source such as a torch with you. Also, keep in mind that many dungeons should only be entered if you have a rope with you.)hint"},
    {7, L"First Experience Points", LR"hint(<b>Congratulations!
<br=7><n>You have gained your first experience points (XP)! You need to collect a certain amount of XP to reach a new 'level' and to improve your character's abilities. Also, keep in mind that dead monsters often contain valuable items such as money or food. If you collect enough hard cash you can buy equipment such as better weapons and armour at a shop or from a player.
<br=7>You should use the best weapons, armour and shields money can buy in order to improve your character's performance in combat. To equip items place them in the corresponding inventory slot. Remember that wounds can be healed by eating food, although this takes a while.)hint"},
    {8, L"Advancing to Level 2", LR"hint(You advanced to Level 2! Your character's abilites have improved:
<br=7>-<indent=15>You will be more difficult to kill because your number of hit points has increased.<indent=0>
-<indent=15>You are walking faster.<indent=0>
-<indent=15>You can carry additional weight.<indent=0>
-<indent=15>You have gained mana which you will need later on to cast spells.<indent=0>
-<indent=15>You can now yell messages. Also, from now on you are allowed to chat with other players on public channels.<indent=0>
-<indent=15>Finally, you may now leave the safety of the village via the bridge to the north. Watch out! There are dangerous creatures lurking in the wilderness!)hint"},
    {9, L"First Skill Advance", LR"hint(You can check your character's current abilities by pressing the button labelled 'Skills' at the right hand side of the screen. This will open a window containing important information such as your character's experience, hit points, and skills.
<br=5>Skill levels advance if the respective skill is used as often as possible. For example, if the sword is your weapon of choice and you have a sword equipped whenever you storm into combat, your sword skill will eventually rise. Needless to say, the higher the skill the better your character.)hint"},
    {10, L"Reaching Level 8", LR"hint(Characters of Level 8 or higher may leave Rookgaard and choose a vocation. There are fours paths to choose from: Be a valorous knight, a nimble paladin, a mysterious druid or a mighty sorcerer! To choose your destiny visit the oracle in the academy's tower or the gatekeeper in Rookgaard's premium area.
<br=5><b>Warning:<n> Characters who do not leave Rookgaard while they are still level 8 will have reduced stats and skills on the main continent! You should make sure to leave Rookgaard as soon as possible.
<br=5><h>Knights<n> are powerful melee fighters. They have the largest amount of hit points of all vocations and their combat skills are unrivalled. However, knights can cast very few spells. <h>Paladins<n> are the best distance fighters, and they perform reasonably well in melee. Since the Paladin also is a respectable magic user who knows how to cast many important spells he is the best all-around vocation. <h>Druids<n> are rather bad melee fighters, but their large number of mana and an impressive arsenal of spells make up for this drawback. The druids' focus is on healing spells and on the benevolent aspects of magic, although they know how to cast many offensive spells as well. Like druids, <h>sorcerers<n> focus on magic. Being worse melee fighters than even their spell-casting cousins, sorcerers rely completely on their devastating spells to survive. High-level sorcerers are feared far and wide because of their deadly magic.)hint"},
    {11, L"Continent", LR"hint(<b>Congratulations!
<br=7><n>You have finally mastered Rookgaard and found your way to the mainland. There, you will get more of everything - more areas, more creatures and more items! Also, many additional features will now be available to you: You can store items in depots, you can send parcels and letters to other players ... and of course, you can use magic!
<br=7>Travel to various cities to find NPCs who serve as masters of your vocation. These masters will sell spells to you. Once you have learnt a spell you can cast it by uttering the magic words. Mana is used whenever a spell is successfully cast, but thankfully characters continually regenerate mana as long as they have enough to eat.
<br=7>If you are playing on a PvP (Player versus Player) or on a PvP-enforced world you will notice another important difference to Rookgaard: Players can now actively attack and even kill each other! However, on PvP worlds there are limits on how many player characters may be killed within certain periods of time. Unlimited playerkilling is only allowed on PvP-enforced worlds, while characters playing on Non-PvP worlds cannot attack each other just like on Rookgaard.)hint"},
    {12, L"Depot", LR"hint(You have entered a depot - a warehouse where players can safely store items. Each major town has one, and players can store a certain amount of items in each of them, the limit being 1000 items per depot for free players and a staggering 2000 items per depot for premium players. To access your personal depot step on the field in front of any locker and open it like a normal container. Don't worry - your items will be perfectly safe in depots!
<br=7>Depots typically contain mailboxes which you can use to send letters or parcels to other characters. To buy a parcel or a letter talk to one of the numerous post officials.
<br=7>To send a parcel or a letter write the name of the addressee in the first line and his home town in the second line. Note that this information must be entered on a label if you are planning to send a parcel. Do not forget to place the label in the parcel before you post it!
<br=7>To send off your letter or parcel simply move it onto the mailbox. If everything is fine the letter or parcel will be delivered right away.)hint"},
    {13, L"Player Killing", LR"hint(<b>Watch out!
<n><br=5>You have just encountered a marked player. The little skull mark underneath the name indicates that he has been involved in violence. All characters are free to attack marked players. If a marked player is killed the character who has killed him will not receive a mark himself.
<br=7>On the other hand, if you attack unmarked players you will be marked yourself, and all other players are free to attack you!
<br=7>Please note that the skulls come in various colours. For further details please see the manual on the website.)hint"},
    {14, L"Level 20 and Security Information", LR"hint(<b>Congratulations!
<br=7><n>You have reached level 20!
<br=5>If you are a premium player and if your character has the necessary funds, you can now promote your character to the elite rank of his vocation!
<br=7>We would like to take the opportunity to remind you that you should keep your account secure. Remember there might be people who try to steal it!
<br=7>Please take a minute and read the following security hints carefully. For further information please see our website. 
<br=10>1.<INDENT=15>Never give your password and/or account number away! <INDENT=0>
<br=2>2.<INDENT=15>Use a secure password.<INDENT=0>
<br=2>3.<INDENT=15>Do not store account data on your computer.<INDENT=0>
<br=2>3.<INDENT=15>Do not download modified clients or tools for Tibia!<INDENT=0>
<br=2>4.<INDENT=15>Do not share or trade accounts!<INDENT=0>
<br=2>5.<INDENT=15>Protect your computer.<INDENT=0>
<br=2>6.<INDENT=15>Keep an eye on the registered email address.<INDENT=0>
<br=2>7.<INDENT=15>Personalise your account!<INDENT=0>
<br=2>)hint"}
}};

}

const AvailableHint* find_hint(std::int32_t id) noexcept {
    const auto found = std::find_if(hint_catalog.begin(), hint_catalog.end(),
        [id](const AvailableHint& hint) { return hint.id == id; });
    return found == hint_catalog.end() ? nullptr : &*found;
}

std::vector<AvailableHint> available_hints(
    std::span<const std::int32_t> known_hint_ids) {
    std::vector<AvailableHint> result;
    result.reserve(hint_catalog.size());
    for (const auto& hint : hint_catalog) {
        if (std::find(known_hint_ids.begin(), known_hint_ids.end(), hint.id) !=
            known_hint_ids.end())
            result.push_back(hint);
    }
    return result;
}

}
