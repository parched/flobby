#include "ChannelChat.h"
#include "TextDisplay.h"
#include "IChatTabs.h"
#include "Prefs.h"

#include "model/Model.h"

#include <FL/Fl_Input.H>
#include <FL/Fl.H>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

// we use the split setting of the ServerMessages tab in all channel tabs
static char const * PrefServerMessagesSplitH = "ServerMessagesSplitH";

ChannelChat::ChannelChat(int x, int y, int w, int h, std::string const & channelName,
                         IChatTabs& iChatTabs, Model & model):
    Fl_Tile(x,y,w,h),
    iChatTabs_(iChatTabs),
    model_(model),
    channelName_(channelName)
{
    // make tab name unique
    std::string const tabName("#"+channelName);
    copy_label(tabName.c_str());

    // left side (text and input)
    int const leftW = 0.75*w;
    Fl_Group * left = new Fl_Group(x, y, leftW, h);
    int const ih = 24; // input height
    text_ = new TextDisplay(x, y, leftW, h-ih);
    input_ = new Fl_Input(x, y+h-ih, leftW, ih);
    input_->callback(ChannelChat::onInput, this);
    input_->when(FL_WHEN_ENTER_KEY);
    left->resizable(text_);
    left->end();

    // right side (user list)
    int const rightW = w - leftW;
    userList_ = new StringTable(x+leftW, y, rightW, h, "ChannelUserList",
            { "name" });

    // setup split
    {
        int x;
        prefs.get(PrefServerMessagesSplitH, x, 0);
        if (x != 0)
        {
            position(userList_->x(), 0, x, 0);
        }
    }

    end();

    // model signals
    model_.connectChannelTopicSignal( boost::bind(&ChannelChat::topic, this, _1, _2, _3, _4) );
    model_.connectChannelClients( boost::bind(&ChannelChat::clients, this, _1, _2) );
    model_.connectUserJoinedChannel( boost::bind(&ChannelChat::userJoined, this, _1, _2) );
    model_.connectUserLeftChannel( boost::bind(&ChannelChat::userLeft, this, _1, _2, _3) );
    model_.connectSaidChannel( boost::bind(&ChannelChat::said, this, _1, _2, _3) );

}

ChannelChat::~ChannelChat()
{
}

int ChannelChat::handle(int event)
{
    switch (event)
    {
    case FL_SHOW:
        labelcolor(FL_BLACK);
        Fl::focus(input_);
        break;
    }
    return Fl_Tile::handle(event);
}

void ChannelChat::onInput(Fl_Widget * w, void * data)
{
    ChannelChat * cc = static_cast<ChannelChat*>(data);

    std::string msg(cc->input_->value());
    boost::trim(msg);

    if (!msg.empty())
    {
        cc->model_.sayChannel(cc->channelName_, msg);

        // TODO ??? always scroll on input
    }
    cc->input_->value("");
}

StringTableRow ChannelChat::makeRow(std::string const & userName)
{
    return StringTableRow( userName,
        {
            userName
        } );
}

std::string ChannelChat::flagsString(User const & user)
{
    std::ostringstream oss;
    oss << (user.status().inGame() ? "G" : "");
    return oss.str();
}

void ChannelChat::topic(std::string const & channelName, std::string const & author, time_t epochSeconds, std::string const & topic)
{
    if (channelName == channelName_)
    {
        append("Topic: " + topic);

        std::string timeString(ctime(&epochSeconds));
        boost::algorithm::erase_all(timeString, "\n");

        append("Topic set " + timeString + " by " + author);
    }
}

void ChannelChat::clients(std::string const & channelName, std::vector<std::string> const & clients)
{
    if (channelName == channelName_)
    {
        for (std::string const & userName : clients)
        {
            userList_->addRow(makeRow(userName));
        }
    }
}

void ChannelChat::userJoined(std::string const & channelName, std::string const & userName)
{
    if (channelName == channelName_)
    {
        userList_->addRow(makeRow(userName));
        std::ostringstream oss;
        oss << "@C" << FL_DARK2 << "@." << userName << " joined";

        append(oss.str());
    }
}

void ChannelChat::userLeft(std::string const & channelName, std::string const & userName, std::string const & reason)
{
    if (channelName == channelName_)
    {

        userList_->removeRow(userName);

        std::ostringstream oss;
        oss << "@C" << FL_DARK2 << "@." << userName << " left";

        if (!reason.empty())
        {
            oss << " (" << reason << ")";
        }
        append(oss.str());
    }
}

void ChannelChat::said(std::string const & channelName, std::string const & userName, std::string const & message)
{
    if (channelName == channelName_)
    {
        append(userName + ": " + message, true);
    }
}

void ChannelChat::leave()
{
    model_.leaveChannel(channelName_);
    text_->clear();
    userList_->clear();
}

void ChannelChat::append(std::string const & msg, bool interesting)
{
    text_->append(msg);
    // make ChatTabs redraw header
    if (interesting && !visible() && labelcolor() != FL_RED)
    {
        labelcolor(FL_RED);
        iChatTabs_.redrawTabs();
    }

}
