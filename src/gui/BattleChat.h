// This file is part of flobby (GPL v2 or later), see the LICENSE file

#pragma once

#include "LogFile.h"
#include <FL/Fl_Group.H>
#include <string>

class User;
class Battle;
class Model;
class VoteLine;
class TextDisplay2;
class ChatInput;

class BattleChat: public Fl_Group
{
public:
    BattleChat(int x, int y, int w, int h, Model & model);
    virtual ~BattleChat();

    void battleJoined(Battle const & battle); // call when joining a battle
    void addInfo(std::string const & msg);
    void close(); // call when "me" left battle

    ChatInput& getChatInput() { return *input_; } // needed for name completion

private:
    Model & model_;
    VoteLine * voteLine_;
    TextDisplay2 * textDisplay_;
    ChatInput * input_;
    std::string battleHost_;
    LogFile logFile_;

    void battleChatMsg(std::string const & userName, std::string const & msg);
    void onText(std::string const & text);
    bool inGameMessage(std::string const & msg, std::string & userNameOut, std::string & msgOut);
};

