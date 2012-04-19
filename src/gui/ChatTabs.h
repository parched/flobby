#pragma once

#include "IChat.h"
#include "IChatTabs.h"

#include <FL/Fl_Tabs.H>
#include <map>
#include <string>

class ServerMessages;
class ChannelChat;
class PrivateChat;
class Model;
class Fl_Tabs;

class ChatTabs: public Fl_Tabs, public IChat, public IChatTabs
{
public:
    ChatTabs(int x, int y, int w, int h, Model & model);
    virtual ~ChatTabs();

    void initTiles();

private:
    Model & model_;

    ServerMessages * server_;

    typedef std::map<std::string, PrivateChat*> PrivateChats;
    PrivateChats privateChats_;

    typedef std::map<std::string, ChannelChat*> ChannelChats;
    ChannelChats channelChats_;

    template <typename M>
    void createChat(std::string const & name, M & map); // used to create both private and channel tabs
    bool closeChat(Fl_Widget* w); // returns true if chat closed

    // model signal handlers
    void saidPrivate(std::string const & userName, std::string const & msg); // msg from other, needed here to create new private chat tabs
    void channelJoined(std::string const & channelName);

    // IChat
    void openPrivateChat(std::string const & userName);
    void openChannelChat(std::string const & channelName);

    // IChatTabs (used by child windows)
    void redrawTabs();

    int handle(int event);
    void draw();
};
