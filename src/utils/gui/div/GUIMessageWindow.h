/****************************************************************************/
/// @file    GUIMessageWindow.h
/// @author  Daniel Krajzewicz
/// @date    Tue, 25 Nov 2003
/// @version $Id$
///
// A logging window for the gui
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.sourceforge.net/
// Copyright (C) 2001-2012 DLR (http://www.dlr.de/) and contributors
/****************************************************************************/
//
//   This file is part of SUMO.
//   SUMO is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
/****************************************************************************/
#ifndef GUIMessageWindow_h
#define GUIMessageWindow_h


// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include <string>
#include <fx.h>
#include <utils/gui/events/GUIEvent.h>


// ===========================================================================
// class definitions
// ===========================================================================
/**
 * @class GUIMessageWindow
 * @brief A logging window for the gui
 *
 * This class displays messages incoming to the gui from either the load or
 *  the run thread.
 *
 * The text is colored in dependence to its type (messages: green, warnings: yellow,
 *  errors: red)
 *
 * Each time a new message is passed, the window is reopened.
 */
class GUIMessageWindow : public FXText {
public:
    /** @brief Constructor
     *
     * @param[in] parent The parent window
     */
    GUIMessageWindow(FXComposite* parent) ;


    /// @brief Destructor
    ~GUIMessageWindow() ;


    /// @brief Adds a a separator to this log window
    void addSeparator() ;


    /** @brief Adds new text to the window
     *
     * The type of the text is determined by the first parameter
     *
     * @param[in] eType The type of the event the message was generated by
     * @param[in] msg The message
     * @see GUIEventType
     */
    void appendText(GUIEventType eType, const std::string& msg) ;


    /// @brief Clears the window
    void clear() ;


private:
    /// @brief The text colors used
    FXHiliteStyle* myStyles;

};


#endif

/****************************************************************************/

