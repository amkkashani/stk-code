//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009-2015 Marianne Gagnon
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

#include "guiengine/engine.hpp"
#include "guiengine/modaldialog.hpp"
#include "guiengine/screen_keyboard.hpp"
#include "guiengine/widgets/text_box_widget.hpp"
#include "guiengine/widgets/CGUIEditBox.hpp"
#include "utils/ptr_vector.hpp"
#include "utils/translation.hpp"
#include "utils/utf8/unchecked.h"

#include <IGUIElement.h>
#include <IGUIEnvironment.h>
#include <IGUIButton.h>

using namespace irr;

class MyCGUIEditBox : public CGUIEditBox
{
    PtrVector<GUIEngine::ITextBoxWidgetListener, REF> m_listeners;

public:

    MyCGUIEditBox(const wchar_t* text, bool border, gui::IGUIEnvironment* environment,
                 gui:: IGUIElement* parent, s32 id, const core::rect<s32>& rectangle) :
        CGUIEditBox(text, border, environment, parent, id, rectangle, translations->isRTLLanguage())
    {
        if (translations->isRTLLanguage()) setTextAlignment(irr::gui::EGUIA_LOWERRIGHT, irr::gui::EGUIA_CENTER);
    }

    void addListener(GUIEngine::ITextBoxWidgetListener* listener)
    {
        m_listeners.push_back(listener);
    }

    void clearListeners()
    {
        m_listeners.clearWithoutDeleting();
    }

    virtual bool OnEvent(const SEvent& event)
    {
        bool out = CGUIEditBox::OnEvent(event);

        if (event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.PressedDown)
        {
            for (unsigned int n=0; n<m_listeners.size(); n++)
            {
                m_listeners[n].onTextUpdated();
            }
        }
        if (event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.Key == IRR_KEY_RETURN)
        {
            for (unsigned int n=0; n<m_listeners.size(); n++)
            {
                if (m_listeners[n].onEnterPressed(Text))
                {
                    Text = L"";
                    CursorPos = 0;
                }
            }
        }
        return out;
    }

};

using namespace GUIEngine;
using namespace irr::core;
using namespace irr::gui;
using namespace irr;

// -----------------------------------------------------------------------------

TextBoxWidget::TextBoxWidget() : Widget(WTYPE_TEXTBOX)
{
}

// -----------------------------------------------------------------------------

void TextBoxWidget::add()
{
    rect<s32> widget_size = rect<s32>(m_x, m_y, m_x + m_w, m_y + m_h);

    // Don't call TextBoxWidget::getText(), which assumes that the irrlicht
    // widget already exists.
    const stringw& text = Widget::getText();

    m_element = new MyCGUIEditBox(text.c_str(), true /* border */, GUIEngine::getGUIEnv(),
                                  (m_parent ? m_parent : GUIEngine::getGUIEnv()->getRootGUIElement()),
                                  getNewID(), widget_size);

    if (m_deactivated)
        m_element->setEnabled(false);

    //m_element = GUIEngine::getGUIEnv()->addEditBox(text.c_str(), widget_size,
    //                                               true /* border */, m_parent, getNewID());
    m_id = m_element->getID();
    m_element->setTabOrder(m_id);
    m_element->setTabGroup(false);
    m_element->setTabStop(true);
}

// -----------------------------------------------------------------------------

void TextBoxWidget::addListener(ITextBoxWidgetListener* listener)
{
    ((MyCGUIEditBox*)m_element)->addListener(listener);
}

// -----------------------------------------------------------------------------

void TextBoxWidget::clearListeners()
{
    ((MyCGUIEditBox*)m_element)->clearListeners();
}

// -----------------------------------------------------------------------------

stringw TextBoxWidget::getText() const
{
    const IGUIEditBox* textCtrl =  Widget::getIrrlichtElement<IGUIEditBox>();
    assert(textCtrl != NULL);

    return stringw(textCtrl->getText());
}

// -----------------------------------------------------------------------------

void TextBoxWidget::setPasswordBox(bool passwordBox, wchar_t passwordChar)
{
    IGUIEditBox* textCtrl =  Widget::getIrrlichtElement<IGUIEditBox>();
    assert(textCtrl != NULL);
    textCtrl->setPasswordBox(passwordBox, passwordChar);
}

// -----------------------------------------------------------------------------

EventPropagation TextBoxWidget::focused(const int playerID)
{
    assert(playerID == 0); // No support for multiple players in text areas!

    // special case : to work, the text box must receive "irrLicht focus", STK focus is not enough
    GUIEngine::getGUIEnv()->setFocus(m_element);
    setWithinATextBox(true);
    return EVENT_LET;
}

// -----------------------------------------------------------------------------

void TextBoxWidget::unfocused(const int playerID, Widget* new_focus)
{
    assert(playerID == 0); // No support for multiple players in text areas!

    setWithinATextBox(false);
    
    GUIEngine::getGUIEnv()->removeFocus(m_element);
}

// -----------------------------------------------------------------------------

void TextBoxWidget::elementRemoved()
{
    // normally at this point normal widgets have been deleted by irrlicht already.
    // but this is a custom widget and the gui env does not appear to want to
    // manage it. so we free it manually
    m_element->drop();
    Widget::elementRemoved();
}

// -----------------------------------------------------------------------------

void TextBoxWidget::setActive(bool active)
{
    Widget::setActive(active);

    if (m_element != NULL)
    {
        IGUIEditBox* textCtrl = Widget::getIrrlichtElement<IGUIEditBox>();
        assert(textCtrl != NULL);
        textCtrl->setEnabled(active);
    }
}   // setActive

// -----------------------------------------------------------------------------

EventPropagation TextBoxWidget::onActivationInput(const int playerID)
{
    if (GUIEngine::ScreenKeyboard::shouldUseScreenKeyboard())
    {
        ((MyCGUIEditBox*)m_element)->openScreenKeyboard();
    }

    // The onWidgetActivated() wasn't used at all before, so always block
    // event to avoid breaking something
    return EVENT_BLOCK;
}

// -----------------------------------------------------------------------------
EventPropagation TextBoxWidget::rightPressed(const int playerID)
{
    if (((MyCGUIEditBox*)m_element)->getTextCount() ==
        ((MyCGUIEditBox*)m_element)->getCursorPosInBox())
        return EVENT_BLOCK;

    return EVENT_LET;
}   // rightPressed

// -----------------------------------------------------------------------------
EventPropagation TextBoxWidget::leftPressed (const int playerID)
{
    if (((MyCGUIEditBox*)m_element)->getCursorPosInBox() == 0)
        return EVENT_BLOCK;

    return EVENT_LET;
}   // leftPressed

// ============================================================================
/* This callback will allow copying android edittext data directly to editbox,
 * which will allow composing text to be auto updated. */
#ifdef ANDROID
#include "jni.h"

#if !defined(ANDROID_PACKAGE_CALLBACK_NAME)
    #error
#endif

#define MAKE_EDITTEXT_CALLBACK(x) JNIEXPORT void JNICALL Java_ ## x##_STKEditText_editText2STKEditbox(JNIEnv* env, jobject this_obj, jstring text, jint start, jint end, jint composing_start, jint composing_end)
#define ANDROID_EDITTEXT_CALLBACK(PKG_NAME) MAKE_EDITTEXT_CALLBACK(PKG_NAME)

extern "C"
ANDROID_EDITTEXT_CALLBACK(ANDROID_PACKAGE_CALLBACK_NAME)
{
    TextBoxWidget* tb = dynamic_cast<TextBoxWidget*>(getFocusForPlayer(0));
    if (!tb || text == NULL)
        return;

    const char* utf8_text = env->GetStringUTFChars(text, NULL);
    if (utf8_text == NULL)
        return;

    // Use utf32 for emoji later
    static_assert(sizeof(wchar_t) == sizeof(uint32_t), "Invalid wchar size");
    std::vector<wchar_t> utf32line;
    utf8::unchecked::utf8to32(utf8_text, utf8_text + strlen(utf8_text),
        back_inserter(utf32line));
    utf32line.push_back(0);

    core::stringw to_editbox(&utf32line[0]);
    tb->getIrrlichtElement<MyCGUIEditBox>()->fromAndroidEditText(
        to_editbox, start, end, composing_start, composing_end);
    env->ReleaseStringUTFChars(text, utf8_text);
}
#endif
