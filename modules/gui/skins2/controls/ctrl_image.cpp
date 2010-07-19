/*****************************************************************************
 * ctrl_image.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "ctrl_image.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../events/evt_generic.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../src/vlcproc.hpp"
#include "../src/scaled_bitmap.hpp"
#include "../src/art_bitmap.hpp"
#include "../utils/position.hpp"


CtrlImage::CtrlImage( intf_thread_t *pIntf, GenericBitmap &rBitmap,
                      CmdGeneric &rCommand, resize_t resizeMethod,
                      const UString &rHelp, VarBool *pVisible, bool art ):
    CtrlFlat( pIntf, rHelp, pVisible ),
    m_pBitmap( &rBitmap ), m_pOriginalBitmap( &rBitmap ),
    m_rCommand( rCommand ), m_resizeMethod( resizeMethod ), m_art( art ),
    m_x( 0 ), m_y( 0 )
{
    // Create an initial unscaled image in the buffer
    m_pImage = OSFactory::instance( pIntf )->createOSGraphics(
                                    rBitmap.getWidth(), rBitmap.getHeight() );
    m_pImage->drawBitmap( *m_pBitmap );

    // Observe the variable
    if( m_art )
    {
        VlcProc *pVlcProc = VlcProc::instance( getIntf() );
        pVlcProc->getStreamArtVar().addObserver( this );

        ArtBitmap::initArtBitmap( getIntf() );
    }

}


CtrlImage::~CtrlImage()
{
    delete m_pImage;

    if( m_art )
    {
        VlcProc *pVlcProc = VlcProc::instance( getIntf() );
        pVlcProc->getStreamArtVar().delObserver( this );

        ArtBitmap::freeArtBitmap( );
    }
}


void CtrlImage::handleEvent( EvtGeneric &rEvent )
{
    // No FSM for this simple transition
    if( rEvent.getAsString() == "mouse:right:up:none" )
    {
        CmdDlgShowPopupMenu( getIntf() ).execute();
    }
    else if( rEvent.getAsString() == "mouse:left:up:none" )
    {
        CmdDlgHidePopupMenu( getIntf() ).execute();
        CmdDlgHideVideoPopupMenu( getIntf() ).execute();
        CmdDlgHideAudioPopupMenu( getIntf() ).execute();
        CmdDlgHideMiscPopupMenu( getIntf() ).execute();
    }
    else if( rEvent.getAsString() == "mouse:left:dblclick:none" )
    {
        m_rCommand.execute();
    }
}


bool CtrlImage::mouseOver( int x, int y ) const
{
    if( x >= 0 && x < getPosition()->getWidth() &&
        y >= 0 && y < getPosition()->getHeight() )
    {
        // convert the coordinates to make them fit to the
        // size of the original image if needed
        switch( m_resizeMethod )
        {
        case kMosaic:
            x %= m_pImage->getWidth();
            y %= m_pImage->getHeight();
            break;

        case kScaleAndRatioPreserved:
            x -= m_x;
            y -= m_y;
            break;

        case kScale:
            break;
        }
        return m_pImage->hit( x, y );
    }

    return false;
}


void CtrlImage::draw( OSGraphics &rImage, int xDest, int yDest )
{
    const Position *pPos = getPosition();
    if( !pPos )
        return;

    int width = pPos->getWidth();
    int height = pPos->getHeight();
    if( width <= 0 || height <= 0 )
        return;

    if( m_resizeMethod == kScale )
    {
        // Use scaling method
        if( width != m_pImage->getWidth() ||
            height != m_pImage->getHeight() )
        {
            OSFactory *pOsFactory = OSFactory::instance( getIntf() );
            // Rescale the image with the actual size of the control
            ScaledBitmap bmp( getIntf(), *m_pBitmap, width, height );
            delete m_pImage;
            m_pImage = pOsFactory->createOSGraphics( width, height );
            m_pImage->drawBitmap( bmp, 0, 0 );
        }
        rImage.drawGraphics( *m_pImage, 0, 0, xDest, yDest );
    }
    else if( m_resizeMethod == kMosaic )
    {
        // Use mosaic method
        while( width > 0 )
        {
            int curWidth = __MIN( width, m_pImage->getWidth() );
            height = pPos->getHeight();
            int curYDest = yDest;
            while( height > 0 )
            {
                int curHeight = __MIN( height, m_pImage->getHeight() );
                rImage.drawGraphics( *m_pImage, 0, 0, xDest, curYDest,
                                     curWidth, curHeight );
                curYDest += curHeight;
                height -= m_pImage->getHeight();
            }
            xDest += curWidth;
            width -= m_pImage->getWidth();
        }
    }
    else if( m_resizeMethod == kScaleAndRatioPreserved )
    {
        int w0 = m_pBitmap->getWidth();
        int h0 = m_pBitmap->getHeight();

        int scaled_height = width * h0 / w0;
        int scaled_width  = height * w0 / h0;

        // new image scaled with aspect ratio preserved
        // and centered inside the control boundaries
        int w, h;
        if( scaled_height > height )
        {
            w = scaled_width;
            h = height;
            m_x = ( width - w ) / 2;
            m_y = 0;
        }
        else
        {
            w = width;
            h = scaled_height;
            m_x = 0;
            m_y = ( height - h ) / 2;
        }

        // rescale the image if size changed
        if( w != m_pImage->getWidth() ||
            h != m_pImage->getHeight() )
        {
            OSFactory *pOsFactory = OSFactory::instance( getIntf() );
            ScaledBitmap bmp( getIntf(), *m_pBitmap, w, h );
            delete m_pImage;
            m_pImage = pOsFactory->createOSGraphics( w, h );
            m_pImage->drawBitmap( bmp, 0, 0 );
        }

        // draw the scaled image at offset (m_x, m_y) from control origin
        rImage.drawGraphics( *m_pImage, 0, 0, xDest + m_x, yDest + m_y );
    }
}


void CtrlImage::onUpdate( Subject<VarString> &rVariable, void* arg )
{
    VlcProc *pVlcProc = VlcProc::instance( getIntf() );

    if( &rVariable == &pVlcProc->getStreamArtVar() )
    {
        string str = ((VarString&)rVariable).get();
        GenericBitmap* pArt = (GenericBitmap*) ArtBitmap::getArtBitmap( str );

        m_pBitmap = pArt ? pArt : m_pOriginalBitmap;

        msg_Dbg( getIntf(), "art file %s to be displayed (wxh = %ix%i)",
                            str.c_str(),
                            m_pBitmap->getWidth(),
                            m_pBitmap->getHeight() );

        delete m_pImage;
        m_pImage = OSFactory::instance( getIntf() )->createOSGraphics(
                                        m_pBitmap->getWidth(),
                                        m_pBitmap->getHeight() );
        m_pImage->drawBitmap( *m_pBitmap );

        notifyLayout();
    }
}


