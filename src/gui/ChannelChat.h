#pragma once

#include "StringTable.h"
#include <FL/Fl_Tile.H>
#include <vector>
#include <string>

class Model;
class IChatTabs;
class User;
class TextDisplay;
class StringTable;
class Fl_Input;

class ChannelChat: public Fl_Tile
{
public:
    ChannelChat(int x, int y, int w, int h, std::string const & channelName,
                IChatTabs& iChatTabs, Model & model);
    virtual ~ChannelChat();
    void leave();

private:
    IChatTabs & iChatTabs_;
    Model & model_;
    TextDisplay * text_;
    Fl_Input * input_;
    StringTable * userList_;

    std::string channelName_;

    static void onInput(Fl_Widget * w, void * data);
    int handle(int event);
    void append(std::string const & msg, bool interesting = false);
    StringTableRow makeRow(std::string const & userName);
    std::string flagsString(User const & user);

    // model signals
    void topic(std::string const & channelName, std::string const & author, time_t epochSeconds, std::string const & topic);
    void clients(std::string const & channelName, std::vector<std::string> const & clients);
    void userJoined(std::string const & channelName, std::string const & userName);
    void userLeft(std::string const & channelName, std::string const & userName, std::string const & reason);
    void said(std::string const & channelName, std::string const & userName, std::string const & message);

};
