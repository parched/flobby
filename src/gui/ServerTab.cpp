// This file is part of flobby (GPL v2 or later), see the LICENSE file

#include "ServerTab.h"
#include "TextDisplay2.h"
#include "UserList.h"
#include "Prefs.h"
#include "ITabs.h"
#include "PopupMenu.h"
#include "Sound.h"
#include "TextFunctions.h"
#include "FlobbyConfig.h"

#include "model/Model.h"
#include "log/Log.h"

#include <FL/fl_ask.H>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <cassert>

ServerTab::ServerTab(int x, int y, int w, int h,
                               ITabs& iTabs, Model & model):
    Fl_Tile(x,y,w,h, "Server"),
    iTabs_(iTabs),
    model_(model),
    logFile_("messages")
{
    callback(ServerTab::callbackSplit, this);

    // limit split drag
    resizable( new Fl_Box(this->x()+100, this->y(), this->w()-(100+100), this->h()) );

    // left side (text and input)
    int const leftW = 0.75*w;
    Fl_Group * left = new Fl_Group(x, y, leftW, h);
    int const ih = FL_NORMAL_SIZE*2; // input height
    text_ = new TextDisplay2(x, y, leftW, h-ih, &logFile_);
    input_ = new ChatInput(x, y+h-ih, leftW, ih);
    input_->connectText( boost::bind(&ServerTab::onInput, this, _1) );
    input_->connectComplete( boost::bind(&ServerTab::onComplete, this, _1, _2, _3, _4) );
    left->resizable(text_);
    left->end();

    // right side (user list)
    int const rightW = w - leftW;
    userList_ = new UserList(x+leftW, y, rightW, h, model_, iTabs_, true); // we save the prefs for UserList

    end();

    text_->append("flobby " FLOBBY_VERSION);
    text_->append("type /help to see built-in commands");
    text_->append("lines entered not starting with / will be sent to lobby server");

    // model signals
    model_.connectConnected( boost::bind(&ServerTab::connected, this, _1) );
    model_.connectServerInfo( boost::bind(&ServerTab::serverInfo, this, _1) );
    model_.connectLoginResult( boost::bind(&ServerTab::loginResult, this, _1, _2) );
    model_.connectServerMsg( boost::bind(&ServerTab::message, this, _1, _2) );
    model_.connectUserJoined( boost::bind(&ServerTab::userJoined, this, _1) );
    model_.connectUserLeft( boost::bind(&ServerTab::userLeft, this, _1) );
    model_.connectRing( boost::bind(&ServerTab::ring, this, _1) );
    model_.connectDownloadDone( boost::bind(&ServerTab::downloadDone, this, _1, _2, _3) );
}

ServerTab::~ServerTab()
{
    prefs().set(PrefServerMessagesSplitH, userList_->x());
}

void ServerTab::initTiles()
{
    int x;
    x = std::max(w()-200, 100);
    prefs().get(PrefServerMessagesSplitH, x, x);
    position(userList_->x(), 0, x, 0);
}

int ServerTab::getSplitPos()
{
    return userList_->x();
}

void ServerTab::setSplitPos(int x)
{
    init_sizes();
    position(userList_->x(), 0, x, 0);
}

void ServerTab::callbackSplit(Fl_Widget*, void *data)
{
    ServerTab* st = static_cast<ServerTab*>(data);

    st->iTabs_.setSplitPos(st->userList_->x(), st);
}

void ServerTab::resize(int x, int y, int w, int h)
{
    int userListW = userList_->w();
    Fl_Tile::resize(x, y, w, h);
    if (w > userListW+100)
    {
        setSplitPos(this->w()-userListW);
    }
}

void ServerTab::serverInfo(ServerInfo const & si)
{
    std::ostringstream oss;
    oss << "ServerInfo: " << si;
    append(oss.str());
}

void ServerTab::loginResult(bool success, std::string const & info)
{
    if (success)
    {
        std::vector<User const *> users = model_.getUsers();

        for (auto u : users)
        {
            assert(u);
            userList_->add(*u);
        }
    }
    else
    {
        append("Login failed: " + info, 1);
    }
}

void ServerTab::connected(bool connected)
{
    if (!connected)
    {
        userList_->clear();
        append("Disconnected from server\n", 1); // extra newline for clarity
    }
}

void ServerTab::message(std::string const & msg, int interest)
{
    append(msg, interest);
}

void ServerTab::userJoined(User const & user)
{
    userList_->add(user);
}

void ServerTab::userLeft(User const & user)
{
    userList_->remove(user.name());
}

void ServerTab::ring(std::string const & userName)
{
    append("ring from " + userName, 1);
    Sound::beep();
}

int ServerTab::handle(int event)
{
    switch (event)
    {
    case FL_SHOW:
        labelcolor(FL_FOREGROUND_COLOR);
        Fl::focus(userList_);
        break;
    }
    return Fl_Tile::handle(event);
}

void ServerTab::append(std::string const & msg, int interest /* = 0 */)
{
    logFile_.log(msg);

    text_->append(msg, interest);
    // make ChatTabs redraw header
    if (interest >= 0 && !visible() && labelcolor() != FL_RED)
    {
        labelcolor(FL_RED);
        iTabs_.redrawTabs();
    }

}

void ServerTab::downloadDone(Model::DownloadType downloadType, std::string const & name, bool success)
{
    if (success)
    {
        append("download done: " + name);
    }
    else
    {
        append("download failed: " + name, 1);
    }
}

void ServerTab::onInput(std::string const & text)
{
    std::string textTrimmed = text;
    boost::trim(textTrimmed);

    append(textTrimmed, -2);

    std::string result = model_.serverCommand(textTrimmed);
    if (!result.empty())
    {
        append("'" + textTrimmed + "' returned:\n" + result, 0);
    }
}

void ServerTab::onComplete(std::string const & text, std::size_t pos, std::string const& ignore, CompleteResult& result)
{
    auto const pairWordPos = getLastWord(text, pos);

    std::string const userName = userList_->completeUserName(pairWordPos.first, ignore);

    if (!userName.empty())
    {
        result.match_ = userName;
        result.newText_ = text.substr(0, pairWordPos.second) + userName + text.substr(pos);
        result.newPos_ = pairWordPos.second + userName.length();
    }
}

std::string ServerTab::logPath()
{
    return logFile_.path();
}
