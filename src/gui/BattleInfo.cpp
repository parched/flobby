// This file is part of flobby (GPL v2 or later), see the LICENSE file

#include "BattleInfo.h"
#include "StringTable.h"
#include "MapImage.h"
#include "Cache.h"

#include "model/Model.h"
#include "log/Log.h"

#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/Fl_Shared_Image.H>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <sstream>
#include <cassert>

BattleInfo::BattleInfo(int x, int y, int w, int h, Model & model, Cache & cache):
    Fl_Group(x,y,w,h),
    model_(model),
    cache_(cache),
    battleId_(-1)
{
    // heigth should be 128
    assert(h == 128);

    // TODO remove ? box(FL_FLAT_BOX);

    mapImageBox_ = new MapImage(x,y, h, h);
    mapImageBox_->box(FL_FLAT_BOX);
    mapImageBox_->callback(BattleInfo::onMapImage, this);

    x += 128;
    headerText_ = new Fl_Multiline_Output(x, y, w-h, h);
    headerText_->box(FL_THIN_DOWN_BOX);
    headerText_->wrap(1);

    resizable(headerText_);
    end();

    // model signal handlers
    model_.connectBattleChanged( boost::bind(&BattleInfo::battleChanged, this, _1) );
    model_.connectBattleClosed( boost::bind(&BattleInfo::battleClosed, this, _1) );
    model_.connectUserJoinedBattle( boost::bind(&BattleInfo::userJoinedBattle, this, _1, _2) );
    model_.connectUserLeftBattle( boost::bind(&BattleInfo::userLeftBattle, this, _1, _2) );
    model_.connectUserChanged( boost::bind(&BattleInfo::userChanged, this, _1) );

    reset();
}

BattleInfo::~BattleInfo()
{
}

void BattleInfo::setMapImage(Battle const & battle)
{
    Fl_Image * image = cache_.getMapImage(battle.mapName());
    if (image)
    {
        mapImageBox_->label(0);
        mapImageBox_->image(image);
        mapImageBox_->activate();
        currentMapImage_ = battle.mapName();
    }
    else if (!model_.getUnitSyncPath().empty())
    {
        mapImageBox_->image(0);
        std::string const msg = "click to\ndownload map\n" + battle.mapName();
        mapImageBox_->copy_label(msg.c_str());
        mapImageBox_->activate();
        currentMapImage_.clear();
    }
    else
    {
        // disable map download since we don't have unitsync yet
        mapImageBox_->image(0);
        mapImageBox_->label("");
        mapImageBox_->deactivate();
        currentMapImage_.clear();
    }
}

void BattleInfo::onMapImage(Fl_Widget* w, void* data)
{
    BattleInfo* o = static_cast<BattleInfo*>(data);
    o->handleOnMapImage();
}

void BattleInfo::handleOnMapImage()
{
    if (battleId_ == -1)
    {
        LOG(WARNING)<< "battleId_ == -1";
        return;
    }

    std::string const mapName = model_.getBattle(battleId_).mapName();
    if (mapName.empty())
    {
        LOG(WARNING)<< "mapName empty";
        return;
    }

    switch (Fl::event())
    {
        case FL_PUSH: // mouse button single click
            switch (Fl::event_button())
            {
                case FL_LEFT_MOUSE: // start download or display minimap
                {
                    if (mapImageBox_->image() == 0 && model_.downloadPr(mapName, Model::DT_MAP))
                    {
                        mapImageBox_->label("downloading...");
                        mapImageBox_->deactivate();
                    }
                    else
                    {
                        Fl_Image * image = cache_.getMapImage(mapName);
                        if (image && image != mapImageBox_->image())
                        {
                            mapImageBox_->image(image);
                            mapImageBox_->redraw();
                        }
                    }
                }
                break;
            }
            break;

        case FL_MOUSEWHEEL:
            Fl_Image * imageCurrent = mapImageBox_->image();
            if (imageCurrent != 0)
            {
                Fl_Image * imageHeight = cache_.getHeightImage(mapName);
                Fl_Image * imageMap = cache_.getMapImage(mapName);
                Fl_Image * imageMetal = cache_.getMetalImage(mapName);

                if (Fl::event_dy() < 0) // wheel up
                {
                    if (imageCurrent == imageMap)
                    {
                        mapImageBox_->image(imageHeight);
                        mapImageBox_->redraw();
                    }
                    else if (imageCurrent == imageMetal)
                    {
                        mapImageBox_->image(imageMap);
                        mapImageBox_->redraw();
                    }
                }
                else if (Fl::event_dy() > 0) // wheel down
                {
                    if (imageCurrent == imageMap)
                    {
                        mapImageBox_->image(imageMetal);
                        mapImageBox_->redraw();
                    }
                    else if (imageCurrent == imageHeight)
                    {
                        mapImageBox_->image(imageMap);
                        mapImageBox_->redraw();
                    }
                }
            }
            break;
    }
}

void BattleInfo::battle(Battle const & battle)
{
    battleId_ = battle.id();

    setMapImage(battle);
    setHeaderText(battle);
}

void BattleInfo::reset()
{
    battleId_ = -1;
    mapImageBox_->image(0);
    mapImageBox_->label(0);
    mapImageBox_->deactivate();
    currentMapImage_.clear();
    headerText_->value("");
}

void BattleInfo::onJoin(Fl_Widget* w, void* data)
{
    BattleInfo * bi = static_cast<BattleInfo*>(data);
    static_cast<void>(bi);
    // TODO be able to join from here ?
}

void BattleInfo::setHeaderText(Battle const & battle)
{
    std::ostringstream oss;
    oss << battle.title() << " / " << battle.founder() << " / " << battle.engineVersionLong() << "\n"
        << battle.mapName() << "\n"
        << battle.modName() << "\n"
        << "Users:";

    for (Battle::BattleUsers::value_type pair : battle.users())
    {
        assert(pair.second);
        User const & u = *pair.second;
        oss << "  " << u.name();
    }

    headerText_->value(oss.str().c_str());
}

void BattleInfo::refresh()
{
    if (battleId_ != -1)
    {
        Battle const & b = model_.getBattle(battleId_);
        setMapImage(b);
    }
}

void BattleInfo::battleClosed(const Battle & battle)
{
    if (battle.id() == battleId_)
    {
        reset();
    }
}

void BattleInfo::battleChanged(const Battle & battle)
{
    if (battle.id() == battleId_)
    {
        if (currentMapImage_ != battle.mapName())
        {
            setMapImage(battle);
        }
        setHeaderText(battle);
    }
}

void BattleInfo::userJoinedBattle(User const & user, const Battle & battle)
{
    if (battle.id() == battleId_)
    {
        setHeaderText(battle);
    }
}

void BattleInfo::userLeftBattle(User const & user, const Battle & battle)
{
    if (battle.id() == battleId_)
    {
        setHeaderText(battle);
    }
}

void BattleInfo::userChanged(User const & user)
{
    int const battleId = user.joinedBattle();
    if ( battleId == battleId_)
    {
        // TODO ?
    }
}
