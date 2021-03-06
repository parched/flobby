// This file is part of flobby (GPL v2 or later), see the LICENSE file

#include "Tabs.h"
#include "Prefs.h"
#include "ServerTab.h"
#include "ChannelChatTab.h"
#include "PrivateChatTab.h"
#include "PopupMenu.h"
#include "ChatSettingsDialog.h"
#include "TextFunctions.h"

#include "model/Model.h"
#include "log/Log.h"

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Input.H>
#include <FL/fl_ask.H>
#include <FL/Fl.H>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <sstream>
#include <cassert>

Tabs::Tabs(int x, int y, int w, int h, Model & model):
    Fl_Tabs(x,y,w,h),
    model_(model),
    chatSettingsDialog_(0)
{
    selection_color(FL_BACKGROUND2_COLOR);

    client_area(x,y,w,h);
    logUsersTab_ = new ServerTab(x,y,w,h, *this, model_);

    resizable(logUsersTab_);

    end();

    // model signals
    model_.connectConnected( boost::bind(&Tabs::connected, this, _1) );
    model_.connectSaidPrivate( boost::bind(&Tabs::saidPrivate, this, _1, _2) );
    model_.connectChannelJoined( boost::bind(&Tabs::channelJoined, this, _1) );
}

Tabs::~Tabs()
{
}

void Tabs::initTiles()
{
    logUsersTab_->initTiles();
}

template <typename M>
typename M::mapped_type Tabs::createChat(std::string const & name, M & map)
{
    // assumes chat do not exists
    typename M::const_iterator it = map.find(name);
    assert(it == map.end());

    int x, y, w, h;
    client_area(x,y,w,h);
    typedef typename std::remove_pointer<typename M::mapped_type>::type Type;
    assert(chatSettingsDialog_ != 0);
    Type * chat = new Type(x,y,w,h, name, *this, model_, *chatSettingsDialog_);
    add(chat);
    chat->hide();
    map[name] = chat;

    return chat;
}

void Tabs::openPrivateChat(std::string const & userName)
{
    PrivateChatTab * pc;
    PrivateChatTabs::const_iterator it = privateChatTabs_.find(userName);
    if (it == privateChatTabs_.end())
    {
        pc = createChat(userName, privateChatTabs_);
    }
    else
    {
        assert(it->second);
        pc = it->second;

        // re-add it if it was closed
        if (find(pc) == children())
        {
            add(pc);
        }
    }

    value(pc);
    pc->show();
}

void Tabs::openChannelChat(std::string const & channelName)
{
    auto it = channelChatTabs_.find(channelName);
    ChannelChatTab * cc;
    if (it == channelChatTabs_.end())
    {
        cc = createChat(channelName, channelChatTabs_);
    }
    else
    {
        assert(it->second);
        cc = it->second;

        // re-add it if it was closed
        if (find(cc) == children())
        {
            add(cc);
        }
    }

    value(cc);
    cc->show();
}

void Tabs::saidPrivate(std::string const & userName, std::string const & msg)
{
    PrivateChatTab * pc;

    auto it = privateChatTabs_.find(userName);
    if (it == privateChatTabs_.end())
    {
        pc = createChat(userName, privateChatTabs_);

        // connecting to a signal during signal invocation does not guarantee signal being delivered (its unspecified), so add message if chat text is empty
        // this changed when switching to Boost.Signals2, Signals did deliver the signal for me but was unspecified as well
        if (pc->text_->buffer()->length() == 0)
        {
            pc->said(userName, msg);
        }
    }
    else
    {
        pc = it->second;
        // re-add it if it was closed
        if (find(pc) == children())
        {
            add(pc);
        }
    }
}

void Tabs::channelJoined(std::string const & channelName)
{
    openChannelChat(channelName);
}

int Tabs::handle(int event)
{
    // close chats and channels (left double clicking or context menu)
    // channels are left when closed
    // chats are not destroyed, they will be just be re-added when needed
    if (event == FL_PUSH)
    {
        Fl_Widget* chat =  which(Fl::event_x(), Fl::event_y());

        if (chat != 0)
        {
            if (chat == logUsersTab_)
            {
                if (handleServerTabClick())
                {
                    return 1;
                }
            }

            // private chat
            for (auto & pair : privateChatTabs_)
            {
                if (pair.second == chat && handlePrivateChatClick(pair.second))
                {
                    return 1;
                }
            }

            // channel chat
            for (auto & pair : channelChatTabs_)
            {
                if (pair.second == chat && handleChannelChatClick(pair.second))
                {
                    return 1;
                }
            }

        }
    }
    else if (event == FL_SHORTCUT)
    {
        int const key = Fl::event_key();
        if ( children() > 0 && key >= '0' && key <= '9' && (Fl::event_state() & FL_ALT) )
        {
            int const index = (key == '0') ? children()-1 : key - '1';
            Fl_Widget* tab = child( std::min(index, children()-1) );
            value(tab);
            tab->show();
            return 1;
        }
    }

    return Fl_Tabs::handle(event);
}

int Tabs::handlePrivateChatClick(PrivateChatTab* pc)
{
    int handled = 0;
    if (Fl::event_button() == FL_LEFT_MOUSE && Fl::event_clicks())
    {
        closeTab(pc);
        handled = 1;
    }
    else if (Fl::event_button() == FL_RIGHT_MOUSE && Fl::event_clicks() == 0)
    {
        PopupMenu menu;
        menu.add("Close", 1);
        if (LogFile::enabled() && boost::filesystem::exists(pc->logPath()))
        {
            menu.add("Open log", 2);
        }

        // add "Join <battlename>" and "Open user web page"
        int battleId;
        std::string zkAccountID;
        try {
            User const& user = model_.getUser(pc->userName()); // throws if user not online

            battleId = user.joinedBattle();
            if (battleId != -1)
            {
                Battle const& battle = model_.getBattle(battleId);
                std::string joinText = "Join " + battle.title();
                menu.add(joinText, 3);
            }

            zkAccountID = user.zkAccountID();
            if (!zkAccountID.empty()) {
                menu.add("Open user web page", 4);
            }
        }
        catch (std::invalid_argument const & e) {
            LOG(DEBUG)<< e.what();
        }

        int const id = menu.show();
        switch (id)
        {
        case 1:
            closeTab(pc);
            handled = 1;
            break;

        case 2:
            LogFile::openLogFile(pc->logPath());
            handled = 1;
            break;

        case 3:
            try {
                Battle const & battle = model_.getBattle(battleId); // throws if battle do not exist
                if (battle.passworded()) {
                    char const * password = fl_input("Enter battle password");
                    if (password != NULL) {
                        model_.joinBattle(battleId, password);
                    }
                }
                else {
                    model_.joinBattle(battleId);
                }
            }
            catch (std::invalid_argument const & e) {
                LOG(WARNING)<< e.what();
            }
            handled = 1;
            break;
        case 4: {
            std::string const link("http://zero-k.info/Users/Detail/" + zkAccountID);
            flOpenUri(link);
            break;
        }
        }
    }

    return handled;
}

int Tabs::handleChannelChatClick(ChannelChatTab* cc)
{
    int handled = 0;
    if (Fl::event_button() == FL_LEFT_MOUSE && Fl::event_clicks())
    {
        cc->leave();
        closeTab(cc);
        handled = 1;
    }
    else if (Fl::event_button() == FL_RIGHT_MOUSE && Fl::event_clicks() == 0)
    {
        PopupMenu menu;
        menu.add("Leave", 1);
        if (LogFile::enabled() && boost::filesystem::exists(cc->logPath()))
        {
            menu.add("Open log", 2);
        }

        int const id = menu.show();
        switch (id)
        {
        case 1:
            cc->leave();
            closeTab(cc);
            handled = 1;
            break;

        case 2:
            LogFile::openLogFile(cc->logPath());
            handled = 1;
            break;
        }
    }

    return handled;
}

int Tabs::handleServerTabClick()
{
    int handled = 0;
    if (Fl::event_button() == FL_RIGHT_MOUSE && Fl::event_clicks() == 0)
    {
        PopupMenu menu;
        if (LogFile::enabled() && boost::filesystem::exists(logUsersTab_->logPath()))
        {
            menu.add("Open log", 1);
        }

        if (menu.size() > 0)
        {
            int const id = menu.show();
            switch (id)
            {
            case 1:
                LogFile::openLogFile(logUsersTab_->logPath());
                handled = 1;
                break;
            }
        }
    }

    return handled;
}

void Tabs::draw()
{
    // workaround for bugged drawing in Fl_Tabs, needed when closing tabs
    fl_color(FL_BACKGROUND_COLOR);
    fl_rectf(x(), y(), w(), logUsersTab_->y() - y());

    Fl_Tabs::draw();
}

void Tabs::closeTab(Fl_Widget* w)
{
    remove(w);
    w->hide();
    redraw();
}

bool Tabs::closeChat(Fl_Widget* w)
{
    for (auto & pair : privateChatTabs_)
    {
        if (pair.second == w)
        {
            remove(w);
            w->hide();
            redraw();
            return true;
        }
    }

    for (auto & pair : channelChatTabs_)
    {
        if (pair.second == w)
        {
            remove(w);
            ChannelChatTab * cc = static_cast<ChannelChatTab*>(w);
            cc->leave();
            w->hide();
            redraw();
            return true;
        }
    }

    return false;
}

void Tabs::redrawTabs()
{
    redraw_tabs();
}

int Tabs::getSplitPos()
{
    return logUsersTab_->getSplitPos();
}

void Tabs::setSplitPos(int x, void* ignore)
{
    if (logUsersTab_ != ignore)
    {
        logUsersTab_->setSplitPos(x);
    }

    for (auto& pair : channelChatTabs_)
    {
        ChannelChatTab* cc = pair.second;
        if (cc != ignore)
        {
            cc->setSplitPos(x);
        }
    }
}

void Tabs::connected(bool connected)
{
    if (!connected)
    {
        for (auto & pair : privateChatTabs_)
        {
            PrivateChatTab * pc = pair.second;
            pc->append("Disconnected from server", -1);
        }

        for (auto & pair : channelChatTabs_)
        {
            ChannelChatTab * cc = pair.second;
            cc->append("Disconnected from server", -1);
            cc->leave();
        }
        redraw();
    }
    else // connected
    {
        for (auto & pair : privateChatTabs_)
        {
            PrivateChatTab * pc = pair.second;
            pc->append("Connected to server", -1);
        }

        for (auto & pair : channelChatTabs_)
        {
            ChannelChatTab * cc = pair.second;
            cc->append("Connected to server", -1);
        }
        redraw();
    }
}
