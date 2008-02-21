//  $Id: plibdrv.cpp 757 2006-09-11 22:27:39Z hiker $
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004 Steve Baker <sjbaker1@airmail.net>
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include <map>

#include <SDL/SDL.h>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "input.hpp"
#include "actionmap.hpp"
#include "user_config.hpp"
#include "sdldrv.hpp"
#include "material_manager.hpp"
#include "kart_properties_manager.hpp"
#include "game_manager.hpp"
#include "herring_manager.hpp"
#include "collectable_manager.hpp"
#include "attachment_manager.hpp"
#include "projectile_manager.hpp"
#include "loader.hpp"
#include "gui/menu_manager.hpp"
#include "player.hpp"
#include "gui/font.hpp"
#include "user_config.hpp"

Input *sensedInput = 0;
ActionMap *actionMap = 0;

SDL_Surface *mainSurface;
long flags;
StickInfo **stickInfos = 0;

InputDriverMode mode = BOOTSTRAP;

#define DEADZONE_MOUSE 150
#define DEADZONE_MOUSE_SENSE 200
#define DEADZONE_JOYSTICK 1000

#define MULTIPLIER_MOUSE 750

/** Helper values to store and track the relative mouse movements. If these
  * values exceed the deadzone value the input is reported to the game. This
  * Makes the mouse behave like an analog axis on a gamepad/joystick.
  */
int mouseValX = 0;
int mouseValY = 0;

//-----------------------------------------------------------------------------
void drv_init()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_TIMER);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

    flags = SDL_OPENGL | SDL_HWSURFACE;
        
    //detect if previous resolution crashed STK
    if (user_config->m_crashed)
    {
    	//STK crashed last time
    	user_config->m_crashed = false;  //reset flag
    	// set window mode as a precaution
    	user_config->m_fullscreen = false;
    	// blacklist the res if not already done
    	std::ostringstream o;
    	o << user_config->m_width << "x" << user_config->m_height;
    	std::string res = o.str();
    	if (std::find(user_config->m_blacklist_res.begin(),
    	  user_config->m_blacklist_res.end(),res) == user_config->m_blacklist_res.end())
    	{
    		user_config->m_blacklist_res.push_back (o.str());
    	}
    	//use prev screen res settings if available
    	if (user_config->m_width != user_config->m_prev_width
    		|| user_config->m_height != user_config->m_prev_height)
    	{
    		user_config->m_width = user_config->m_prev_width;
    		user_config->m_height = user_config->m_prev_height;
    	}
    	else //set 'safe' resolution to return to
    	{
    		user_config->m_width = user_config->m_prev_width = 800;
    		user_config->m_height = user_config->m_prev_height = 600;
    	}
    }
    
	if(user_config->m_fullscreen)
        flags |= SDL_FULLSCREEN;
        
    setVideoMode(false);

    SDL_JoystickEventState(SDL_ENABLE);

    const int numSticks = SDL_NumJoysticks();
    stickInfos = new StickInfo *[numSticks];
    for (int i = 0; i < numSticks; i++)
        stickInfos[i] = new StickInfo(i);
	
    SDL_WM_SetCaption("SuperTuxKart", NULL);

	// Get into menu mode initially.
	drv_setMode(MENU);
}

//-----------------------------------------------------------------------------
void
showPointer()
{
  SDL_ShowCursor(SDL_ENABLE);
}
//-----------------------------------------------------------------------------
void
hidePointer()
{
  SDL_ShowCursor(SDL_DISABLE);
}
//-----------------------------------------------------------------------------
void drv_toggleFullscreen(bool resetTextures)
{
    user_config->m_fullscreen = !user_config->m_fullscreen;

    flags = SDL_OPENGL | SDL_HWSURFACE;

    if(user_config->m_fullscreen)
    {
        flags |= SDL_FULLSCREEN;

        if(menu_manager->isSomewhereOnStack(MENUID_RACE))
          showPointer();
          
    	// Store settings in user config file in case new video mode
    	// causes a crash
    	user_config->m_crashed = true; //set flag. 
    	user_config->saveConfig();
    }
    else if(menu_manager->isSomewhereOnStack(MENUID_RACE))
        hidePointer();
            
    setVideoMode(resetTextures);
    
}

//-----------------------------------------------------------------------------
void setVideoMode(bool resetTextures)
{
    //Is SDL_FreeSurface necessary? SDL wiki says not??
    SDL_FreeSurface(mainSurface);
    mainSurface = SDL_SetVideoMode(user_config->m_width, user_config->m_height, 0, flags);

#if defined(WIN32) || defined(__APPLE__)
    if(resetTextures)
    {
        // Clear plib internal texture cache
        loader->endLoad();

        // Windows needs to reload all textures, display lists, ... which means
        // that all models have to be reloaded. So first, free all textures,
        // models, then reload the textures from materials.dat, then reload
        // all models, textures etc.

	//        startScreen             -> removeTextures();
        attachment_manager      -> removeTextures();
        projectile_manager      -> removeTextures();
        herring_manager         -> removeTextures();
        kart_properties_manager -> removeTextures();
        collectable_manager     -> removeTextures();

        material_manager->reInit();


        collectable_manager     -> loadCollectables();
        kart_properties_manager -> loadKartData();
        herring_manager         -> loadDefaultHerrings();
        projectile_manager      -> loadData();
        attachment_manager      -> loadModels();

	//        startScreen             -> installMaterial();

        //FIXME: the font reinit funcs should be inside the font class
        //Reinit fonts
        delete_fonts();
        init_fonts();
    }
#endif
}

//-----------------------------------------------------------------------------
void drv_deinit()
{
    const int NUM_STICKS = SDL_NumJoysticks();
    for (int i=0;i<NUM_STICKS;i++)
        delete stickInfos[i];

    delete [] stickInfos;

    SDL_FreeSurface(mainSurface);

    SDL_Quit();
}

//-----------------------------------------------------------------------------
/** Handles the conversion from some input to a GameAction and its distribution
  * to the currently active menu.
  * It also handles whether the game is currently sensing input. It does so by
  * suppressing the distribution of the input as a GameAction. Instead the
  * input is stored in 'sensedInput' and GA_SENSE_COMPLETE is distributed. If
  * however the input in question has resolved to GA_LEAVE this is treated as
  * an attempt of the user to cancel the sensing. In that case GA_SENSE_CANCEL
  * is distributed.
  *
  * Note: It is the obligation of the called menu to switch of the sense mode.
  *
  */
void input(InputType type, int id0, int id1, int id2, int value)
{
    BaseGUI* menu = menu_manager->getCurrentMenu();

	GameAction ga = actionMap->getEntry(type, id0, id1, id2);

	if (menu != NULL)
	{
		// Act different in input sensing mode.
		if (mode == INPUT_SENSE)
		{
			// Input sensing should be canceled.
			if (ga == GA_LEAVE)
			{
				menu->handle(GA_SENSE_CANCEL, value);
			}
			// Stores the sensed input when the button/key/axes/<whatever> is
			// released only and is not used in a fixed mapping.
			else if (!(value || user_config->isFixedInput(type, id0, id1, id2)))
			{
				sensedInput->type = type;
				sensedInput->id0 = id0;
				sensedInput->id1 = id1;
				sensedInput->id2 = id2;

				// Notify the completion of the input sensing.
				menu->handle(GA_SENSE_COMPLETE, 0);
			}
		}
		else if (ga != GA_NULL)
		{
			// Lets the currently active menu handle the GameAction.
			menu->handle(ga, value);
		}
	}
}

//-----------------------------------------------------------------------------
/** Reads the SDL event loop, does some tricks with certain values and calls
  * input() is appropriate.
  *
  * Digital inputs get the value of 32768 when pressed (key/button press,
  * digital axis) because this is what SDL provides. Relative mouse inputs
  * which do not fit into this scheme are converted to match. This is done to
  * relieve the KartAction implementor from the need to think about different
  * input devices and how SDL treats them. The same input gets the value of 0
  * when released.
  *
  * Analog axes can have any value from [0, 32768].
  *
  * There are no negative values. Instead this is reported as an axis with a
  * negative direction. This simplifies input configuration and allows greater
  * flexibility (= treat 4 directions as four buttons).
  *
  */
void sdl_input()
{
    SDL_Event ev;

    while(SDL_PollEvent(&ev))
    {
        switch(ev.type)
        {
        case SDL_QUIT:
            game_manager->abort();
            break;

        case SDL_KEYUP:
           	input(IT_KEYBOARD, ev.key.keysym.sym, 0, 0, 0);
            break;
        case SDL_KEYDOWN:
			if (mode == LOWLEVEL)
			{
				// Unicode translation in SDL is only done for keydown events.
				// Therefore for lowlevel keyboard handling we provide no notion
				// of whether a key was pressed or released.
				menu_manager->getCurrentMenu()
					->inputKeyboard(ev.key.keysym.sym,
									ev.key.keysym.unicode);
			}
            input(IT_KEYBOARD, ev.key.keysym.sym,
				  ev.key.keysym.unicode, 0, 32768);

            break;

        case SDL_MOUSEMOTION:
			// Reports absolute pointer values on a separate path to the menu
			// system to avoid the trouble that arises because all other input
			// methods have only one value to inspect (pressed/release,
			// axis value) while the pointer has two.
			if (!mode)
			{
				BaseGUI* menu = menu_manager->getCurrentMenu();
				if (menu != NULL)
					menu->inputPointer(ev.motion.x, mainSurface->h - ev.motion.y);
			}
			// If sensing input mouse movements are made less sensitive in order
			// to avoid it being detected unwantedly.
			else if (mode == INPUT_SENSE)
			{
				if (ev.motion.xrel <= -DEADZONE_MOUSE_SENSE)
					input(IT_MOUSEMOTION, 0, AD_NEGATIVE, 0, 0);
				else if (ev.motion.xrel >= DEADZONE_MOUSE_SENSE)
					input(IT_MOUSEMOTION, 0, AD_POSITIVE, 0, 0);

				if (ev.motion.yrel <= -DEADZONE_MOUSE_SENSE)
					input(IT_MOUSEMOTION, 1, AD_NEGATIVE, 0, 0);
				else if (ev.motion.yrel >= DEADZONE_MOUSE_SENSE)
					input(IT_MOUSEMOTION, 1, AD_POSITIVE, 0, 0);
			}
			else
			{
				// Calculates new values for the mouse helper variables. It
				// keeps them in the [-32768, 32768] range. The same values are
				// used by SDL for stick axes. 
				mouseValX = std::max(-32768, std::min(32768,
													  mouseValX + ev.motion.xrel
													  * MULTIPLIER_MOUSE));
				mouseValY = std::max(-32768,
									 std::min(32768, mouseValY + ev.motion.yrel
													 * MULTIPLIER_MOUSE));
			}
			break;
        case SDL_MOUSEBUTTONUP:
            input(IT_MOUSEBUTTON, ev.button.button, 0, 0, 0);
            break;

        case SDL_MOUSEBUTTONDOWN:
            input(IT_MOUSEBUTTON, ev.button.button, 0, 0, 32768);
            break;

        case SDL_JOYAXISMOTION:
			// If the joystick axis exceeds the deadzone report the input.
			// In menu mode (mode = MENU = 0) the joystick number is reported
			// to be zero in all cases. This has the neat effect that all
			// joysticks can be used to control the menu.
            if(ev.jaxis.value <= -DEADZONE_JOYSTICK)
			{
                input(IT_STICKMOTION, !mode ? 0 : ev.jaxis.which,
					  ev.jaxis.axis, AD_NEGATIVE, -ev.jaxis.value);
				stickInfos[ev.jaxis.which]->prevAxisDirections[ev.jaxis.axis]
					= AD_NEGATIVE;
			}
			else if(ev.jaxis.value >= DEADZONE_JOYSTICK)
			{
				input(IT_STICKMOTION, !mode ? 0 : ev.jaxis.which,
					  ev.jaxis.axis, AD_POSITIVE, ev.jaxis.value);
				stickInfos[ev.jaxis.which]->prevAxisDirections[ev.jaxis.axis]
					= AD_POSITIVE;
			}
			else
			{
				// Axis stands still: This is reported once for digital axes and
				// can be called multipled times for analog ones. Uses the
				// previous direction in which the axis was triggered to
				// determine which one has to be brought into the released
				// state. This allows us to regard two directions of an axis
				// as completely independent input variants (as if they where
				// two buttons).				
				if (stickInfos[ev.jaxis.which]
					->prevAxisDirections[ev.jaxis.axis] == AD_NEGATIVE)
               		input(IT_STICKMOTION, !mode ? 0 : ev.jaxis.which,
						  ev.jaxis.axis, AD_NEGATIVE, 0);
				else if (stickInfos[ev.jaxis.which]
						 ->prevAxisDirections[ev.jaxis.axis] == AD_POSITIVE)
               		input(IT_STICKMOTION, !mode ? 0 : ev.jaxis.which,
						  ev.jaxis.axis, AD_POSITIVE, 0);
				
				stickInfos[ev.jaxis.which]->prevAxisDirections[ev.jaxis.axis]
					= AD_NEUTRAL;
			}

            break;
        case SDL_JOYBUTTONUP:
			// See the SDL_JOYAXISMOTION case label because of !mode thingie.
            input(IT_STICKBUTTON, !mode ? 0 : ev.jbutton.which, ev.jbutton.button, 0,
                  0);
            break;
        case SDL_JOYBUTTONDOWN:
			// See the SDL_JOYAXISMOTION case label because of !mode thingie.
            input(IT_STICKBUTTON, !mode ? 0 : ev.jbutton.which, ev.jbutton.button, 0,
                  32768);
            break;
        case SDL_USEREVENT:
        // used in display_res_confirm for the countdown timer
        (menu_manager->getCurrentMenu())->countdown();
        
        }  // switch
    }   // while (SDL_PollEvent())

    // Makes mouse behave like an analog axis.
	if (mouseValX <= -DEADZONE_MOUSE)
	  input(IT_MOUSEMOTION, 0, AD_NEGATIVE, 0, -mouseValX);
	else if (mouseValX >= DEADZONE_MOUSE)
	  input(IT_MOUSEMOTION, 0, AD_POSITIVE, 0, mouseValX);
	else
	  mouseValX = 0;

	if (mouseValY <= -DEADZONE_MOUSE)
	  input(IT_MOUSEMOTION, 1, AD_NEGATIVE, 0, -mouseValY);
	else if (mouseValY >= DEADZONE_MOUSE)
	  input(IT_MOUSEMOTION, 1, AD_POSITIVE, 0, mouseValY);
	else
	  mouseValY = 0;

}

//-----------------------------------------------------------------------------
/** Retrieves the Input instance that has been prepared in the input sense
  * mode.
  *
  * The Instance has valid values of the last input sensing operation *only*
  * if called immediately after a BaseGUI::handle() implementation received
  * GA_SENSE_COMPLETE.
  *
  * It is wrong to call it when not in input sensing mode anymore.
  */
Input &
drv_getSensedInput()
{
	assert (mode == INPUT_SENSE);

	// sensedInput should be available in input sense mode.
	assert (sensedInput);
	
	return *sensedInput;
}

//-----------------------------------------------------------------------------
/** Queries the input driver whether it is in the given expected mode.
  */
bool
drv_isInMode(InputDriverMode expMode)
{
	return mode == expMode;
}
//-----------------------------------------------------------------------------
/** Sets the mode of the input driver.
  * 
  * Switching of the input driver's modes is only needed for special menus
  * (those who need typing or input sensing) and the MenuManager (switch to
  * ingame/menu mode). Therefore there is a limited amount of legal combinations
  * of current and next input driver modes: From the menu mode you can switch
  * to any other mode and from any other mode only back to the menu mode.
  *
  * In menu mode the pointer is visible (and reports absolute values through
  * BaseGUI::inputKeyboard()) and the BaseGUI::handle() implementations can
  * receive GameAction values from GA_FIRST_MENU to GA_LAST_MENU.
  *
  * In ingame mode the pointer is invisible (and reports relative values)
  * and the BaseGUI::handle() implementations can receive GameAction values
  * from GA_FIRST_INGAME to GA_LAST_INGAME.
  *
  * In input sense mode the pointer is invisible (any movement reports are
  * suppressed). If an input happens it is stored internally and can be
  * retrieved through drv_getSensedInput() *after* GA_SENSE_COMPLETE has been
  * distributed to a menu (Normally the menu that received GA_SENSE_COMPLETE
  * will request the sensed input ...). If GA_SENSE_CANCEL is received instead
  * the user decided to cancel input sensing. No other game action values are
  * distributed in this mode.
  * 
  * In lowlevel mode the pointer is invisible (and reports relative values - 
  * this is just a side effect). BaseGUI::handle() can receive GameAction
  * values from GA_FIRST_MENU to GA_LAST_MENU. Additionally each key press is
  * distributed through BaseGUI::inputKeyboard(). This happens *before* the
  * same keypress is processed to be distributed as a GameAction. This was done
  * to make the effects of changing the input driver's mode from
  * BaseGUI::handle() implementations less strange. The way it is implemented
  * makes sure that such a change only affects the next keypress or keyrelease.
  * The same is not true for mode changes from within a BaseGUI::inputKeyboard()
  * implementation. It is therefore discouraged.
  *
  * And there is the bootstrap mode. You cannot switch to it and only leave it
  * once per application instance. It is the state the input driver is first.
  *
  */
void
drv_setMode(InputDriverMode newMode)
{
	switch (newMode)
	{
	case MENU:
		switch (mode)
		{
		case INGAME:
			// Leaving ingame mode.
				
			if (actionMap)
				delete actionMap;
	
			// Reset the helper values for the relative mouse movement
			// supresses to the notification of them as an input.
			mouseValX = mouseValY = 0;
			
			showPointer();
			
			// Fall through expected.
		case BOOTSTRAP:
			// Leaving boot strap mode.
				
			// Installs the action map for the menu.
			actionMap = user_config->newMenuActionMap();
			
			mode = MENU;

			break;
		case INPUT_SENSE:
			// Leaving input sense mode.
				
			showPointer();
			
			// The order is deliberate just in case someone starts to make
			// STK multithreaded: sensedInput must not be 0 when
			// mode == INPUT_SENSE.
			mode = MENU;
			
		    delete sensedInput;
    		sensedInput = 0;

			break;
		case LOWLEVEL:
			// Leaving lowlevel mode.
				
		    SDL_EnableUNICODE(SDL_DISABLE);

			showPointer();

			mode = MENU;
			
			break;
		default:
			// Something is broken.
			assert (false);
		}
		
		break;
	case INGAME:
		// We must be in menu mode now in order to switch.
		assert (mode == MENU);
	
		if (actionMap)
			delete actionMap;

		// Installs the action map for the ingame mode.
		actionMap = user_config->newIngameActionMap();

		hidePointer();

		mode = INGAME;
		
		break;
	case INPUT_SENSE:
		// We must be in menu mode now in order to switch.
		assert (mode == MENU);
		
		// Reset the helper values for the relative mouse movement supresses to
		// the notification of them as an input.
		mouseValX = mouseValY = 0;

		sensedInput = new Input();
	
		hidePointer();
	
		mode = INPUT_SENSE;
		
		break;
	case LOWLEVEL:
		// We must be in menu mode now in order to switch.
		assert (mode == MENU);
		
	    SDL_EnableUNICODE(SDL_ENABLE);

		hidePointer();

		mode = LOWLEVEL;

		break;
	default:
		// Invalid mode.
		assert(false);
	}
}

StickInfo::StickInfo(int index)
{
	sdlJoystick = SDL_JoystickOpen(index);
	const int count = SDL_JoystickNumAxes(sdlJoystick);
	prevAxisDirections = new AxisDirection[count];
	
	for (int i = 0; i < count; i++)
		prevAxisDirections[i] = AD_NEUTRAL;
}

StickInfo::~StickInfo()
{
	delete prevAxisDirections;
	
	SDL_JoystickClose(sdlJoystick);
}
