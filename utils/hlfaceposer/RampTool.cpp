#include <stdio.h>
#include "hlfaceposer.h"
#include "RampTool.h"
#include "mdlviewer.h"
#include "choreowidgetdrawhelper.h"
#include "TimelineItem.h"
#include "expressions.h"
#include "expclass.h"
#include "choreoevent.h"
#include "StudioModel.h"
#include "choreoscene.h"
#include "choreoactor.h"
#include "choreochannel.h"
#include "ChoreoView.h"
#include "InputProperties.h"
#include "ControlPanel.h"
#include "FlexPanel.h"
#include "mxExpressionTray.h"
#include "ExpressionProperties.h"
#include "vstdlib/strtools.h"
#include "faceposer_models.h"
#include "UtlBuffer.h"
#include "FileSystem.h"
#include "cmdlib.h"
#include "scriplib.h"
#include "iscenetokenprocessor.h"
#include "choreoviewcolors.h"
#include "MatSysWin.h"

RampTool *g_pRampTool = 0;

#define TRAY_HEIGHT 20
#define TRAY_ITEM_INSET 10

#define TAG_TOP ( TRAY_HEIGHT + 12 )
#define TAG_BOTTOM ( TAG_TOP + 20 )

#define MAX_TIME_ZOOM 1000
// 10% per step
#define TIME_ZOOM_STEP 2

float SnapTime( float input, float granularity );
StudioModel *FindAssociatedModel( CChoreoScene *scene, CChoreoActor *a );

RampTool::RampTool( mxWindow *parent )
: IFacePoserToolWindow( "RampTool", "Ramp" ), mxWindow( parent, 0, 0, 0, 0 )
{
	m_bSuppressLayout = false;

	SetAutoProcess( true );

	m_szEvent[0]		= 0;

	m_flScrub			= 0.0f;
	m_flScrubTarget		= 0.0f;
	m_nDragType			= DRAGTYPE_NONE;

	m_nClickedX			= 0;
	m_nClickedY			= 0;

	m_hPrevCursor		= 0;
	
	m_nStartX			= 0;
	m_nStartY			= 0;

	m_pLastEvent		= NULL;

	m_nMousePos[ 0 ] = m_nMousePos[ 1 ] = 0;

	m_nMinX				= 0;
	m_nMaxX				= 0;
	m_bUseBounds		= false;

	m_bLayoutIsValid = false;
	m_flPixelsPerSecond = 500.0f;

	m_flLastDuration = 0.0f;
	m_nTimeZoom = 100;
	m_nScrollbarHeight	= 12;
	m_nLeftOffset = 0;
	m_nLastHPixelsNeeded = -1;
	m_pHorzScrollBar = new mxScrollbar( this, 0, 0, 18, 100, IDC_RAMPHSCROLL, mxScrollbar::Horizontal );
	m_pHorzScrollBar->setVisible( false );

	m_bInSetEvent = false;
	m_flScrubberTimeOffset = 0.0f;
}

RampTool::~RampTool( void )
{
}

void RampTool::SetEvent( CChoreoEvent *event )
{
	if ( m_bInSetEvent )
		return;

	m_bInSetEvent = true;

	if ( event == m_pLastEvent )
	{
		if ( event && event->GetDuration() != m_flLastDuration )
		{
			m_flLastDuration = event->GetDuration();
			m_nLastHPixelsNeeded = -1;
			m_nLeftOffset = 0;
			InvalidateLayout();
		}

		m_bInSetEvent = false;
		return;
	}

	m_pLastEvent = event;

	m_szEvent[ 0 ] = 0;
	if ( event )
	{
		strcpy( m_szEvent, event->GetName() );
	}
	
	if ( event )
	{
		m_flLastDuration = event->GetDuration();
	}
	else
	{
		m_flLastDuration = 0.0f;
	}
	m_nLeftOffset = 0;
	m_nLastHPixelsNeeded = -1;
	InvalidateLayout();

	m_bInSetEvent = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoEvent *RampTool::GetSafeEvent( void )
{
	if ( !g_pChoreoView )
		return NULL;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return NULL;

	// Find event by name
	for ( int i = 0; i < scene->GetNumEvents() ; i++ )
	{
		CChoreoEvent *e = scene->GetEvent( i );
		if ( !e || !e->HasEndTime() )
			continue;

		if ( !_stricmp( m_szEvent, e->GetName() ) )
		{
			return e;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcHandle - 
//-----------------------------------------------------------------------------
void RampTool::GetScrubHandleRect( RECT& rcHandle, float scrub, bool clipped )
{
	float pixel = 0.0f;
	if ( w2() > 0 )
	{
		pixel = GetPixelForTimeValue( scrub );

		if  ( clipped )
		{
			pixel = clamp( pixel, SCRUBBER_HANDLE_WIDTH / 2, w2() - SCRUBBER_HANDLE_WIDTH / 2 );
		}
	}

	rcHandle.left = pixel- SCRUBBER_HANDLE_WIDTH / 2;
	rcHandle.right = pixel + SCRUBBER_HANDLE_WIDTH / 2;
	rcHandle.top = 2 + GetCaptionHeight();
	rcHandle.bottom = rcHandle.top + SCRUBBER_HANDLE_HEIGHT;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcHandle - 
//-----------------------------------------------------------------------------
void RampTool::DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper, RECT& rcHandle, float scrub, bool reference )
{
	HBRUSH br = CreateSolidBrush( reference ? RGB( 150, 0, 0 ) : RGB( 0, 150, 100 ) );

	COLORREF areaBorder = RGB( 230, 230, 220 );

	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcHandle.top, w2(), rcHandle.top );
	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcHandle.bottom, w2(), rcHandle.bottom );

	drawHelper.DrawFilledRect( br, rcHandle );

	// 
	char sz[ 32 ];
	sprintf( sz, "%.3f", scrub );

	CChoreoEvent *ev = GetSafeEvent();
	if ( ev )
	{
		float st, ed;
		st = ev->GetStartTime();
		ed = ev->GetEndTime();

		float dt = ed - st;
		if ( dt > 0.0f )
		{
			sprintf( sz, "%.3f", st + scrub );
		}
	}

	int len = drawHelper.CalcTextWidth( "Arial", 9, 500, sz );

	RECT rcText = rcHandle;

	int textw = rcText.right - rcText.left;

	rcText.left += ( textw - len ) / 2;

	drawHelper.DrawColoredText( "Arial", 9, 500, RGB( 255, 255, 255 ), rcText, sz );

	DeleteObject( br );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool RampTool::IsMouseOverScrubHandle( mxEvent *event )
{
	RECT rcHandle;
	GetScrubHandleRect( rcHandle, m_flScrub, true );
	InflateRect( &rcHandle, 2, 2 );

	POINT pt;
	pt.x = (short)event->x;
	pt.y = (short)event->y;
	if ( PtInRect( &rcHandle, pt ) )
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool RampTool::IsProcessing( void )
{
	if ( !GetSafeEvent() )
		return false;

	if ( m_flScrub != m_flScrubTarget )
		return true;

	return false;
}

bool RampTool::IsScrubbing( void ) const
{
	bool scrubbing = ( m_nDragType == DRAGTYPE_SCRUBBER ) ? true : false;
	return scrubbing;
}

void RampTool::SetScrubTime( float t )
{
	m_flScrub = t;
	CChoreoEvent *e = GetSafeEvent();
	if ( e && e->GetDuration() )
	{
		float realtime = e->GetStartTime() + m_flScrub;

		g_pChoreoView->SetScrubTime( realtime );
		g_pChoreoView->DrawScrubHandle();
	}
}

void RampTool::SetScrubTargetTime( float t )
{
	m_flScrubTarget = t;
	CChoreoEvent *e = GetSafeEvent();
	if ( e && e->GetDuration() )
	{
		float realtime = e->GetStartTime() + m_flScrubTarget;

		g_pChoreoView->SetScrubTargetTime( realtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void RampTool::Think( float dt )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	bool scrubbing = IsScrubbing();
	ScrubThink( dt, scrubbing );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void RampTool::ScrubThink( float dt, bool scrubbing )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	if ( m_flScrubTarget == m_flScrub && !scrubbing )
		return;

	float d = m_flScrubTarget - m_flScrub;
	int sign = d > 0.0f ? 1 : -1;

	float maxmove = dt;

	if ( sign > 0 )
	{
		if ( d < maxmove )
		{
			SetScrubTime( m_flScrubTarget );
		}
		else
		{
			SetScrubTime( m_flScrub + maxmove );
		}
	}
	else
	{
		if ( -d < maxmove )
		{
			SetScrubTime( m_flScrubTarget );
		}
		else
		{
			SetScrubTime( m_flScrub - maxmove );
		}
	}
	
	if ( scrubbing )
	{
		g_pMatSysWindow->Frame();
	}
}

void RampTool::DrawScrubHandles()
{
	RECT rcTray;

	RECT rcHandle;
	GetScrubHandleRect( rcHandle, m_flScrub, true );

	rcTray = rcHandle;
	rcTray.left = 0;
	rcTray.right = w2();

	CChoreoWidgetDrawHelper drawHelper( this, rcTray );
	DrawScrubHandle( drawHelper, rcHandle, m_flScrub, false );
}

void RampTool::redraw()
{
	if ( !ToolCanDraw() )
		return;

	CChoreoWidgetDrawHelper drawHelper( this );
	HandleToolRedraw( drawHelper );

	RECT rc;
	drawHelper.GetClientRect( rc );

	CChoreoEvent *ev = GetSafeEvent();
	if ( ev )
	{
		RECT rcText;
		drawHelper.GetClientRect( rcText );
		rcText.top += GetCaptionHeight()+1;
		rcText.bottom = rcText.top + 13;
		rcText.left += 5;
		rcText.right -= 5;

		OffsetRect( &rcText, 0, 12 );

		int current, total;

		g_pChoreoView->GetUndoLevels( current, total );
		if ( total > 0 )
		{
			RECT rcUndo = rcText;
			OffsetRect( &rcUndo, 0, 2 );

			drawHelper.DrawColoredText( "Small Fonts", 8, FW_NORMAL, RGB( 0, 100, 0 ), rcUndo,
				"Undo:  %i/%i", current, total );
		}

		rcText.left += 60;
		
		// Found it, write out description
		// 
		RECT rcTextLine = rcText;

		drawHelper.DrawColoredText( "Arial", 11, 900, RGB( 200, 0, 0 ), rcTextLine,
			"Event:  %s",
			ev->GetName() );

		RECT rcTimeLine;
		drawHelper.GetClientRect( rcTimeLine );
		rcTimeLine.left = 0;
		rcTimeLine.right = w2();
		rcTimeLine.top += ( GetCaptionHeight() + 50 );

		float lefttime = GetTimeValueForMouse( 0 );
		float righttime = GetTimeValueForMouse( w2() );

		DrawTimeLine( drawHelper, rcTimeLine, lefttime, righttime );

		OffsetRect( &rcText, 0, 28 );

		rcText.left = 5;

		RECT timeRect = rcText;

		timeRect.right = timeRect.left + 100;

		char sz[ 32 ];

		Q_snprintf( sz, sizeof( sz ), "%.2f", lefttime + ev->GetStartTime() );

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 0, 0 ), timeRect, sz );

		timeRect = rcText;

		Q_snprintf( sz, sizeof( sz ), "%.2f", righttime + ev->GetStartTime() );

		int textW = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, sz );

		timeRect.right = w2() - 10;
		timeRect.left = timeRect.right - textW;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 0, 0 ), timeRect, sz );
	}

	RECT rcHandle;
	GetScrubHandleRect( rcHandle, m_flScrub, true );
	DrawScrubHandle( drawHelper, rcHandle, m_flScrub, false );

	RECT rcSamples;
	GetSampleTrayRect( rcSamples );
	DrawSamples( drawHelper, rcSamples );

	DrawEventEnd( drawHelper );

	RECT rcTags = rc;
	rcTags.top = TAG_TOP + GetCaptionHeight();
	rcTags.bottom = TAG_BOTTOM + GetCaptionHeight();

	DrawTimingTags( drawHelper, rcTags );

	RECT rcPos;
	GetMouseOverPosRect( rcPos );
	DrawMouseOverPos( drawHelper, rcPos );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RampTool::ShowContextMenu( mxEvent *event, bool include_track_menus )
{
	// Construct main menu
	mxPopupMenu *pop = new mxPopupMenu();

	int current, total;
	g_pChoreoView->GetUndoLevels( current, total );
	if ( total > 0 )
	{
		if ( current > 0 )
		{
			pop->add( va( "Undo %s", g_pChoreoView->GetUndoDescription() ), IDC_UNDO_RT );
		}
		
		if ( current <= total - 1 )
		{
			pop->add( va( "Redo %s", g_pChoreoView->GetRedoDescription() ), IDC_REDO_RT );
		}
		pop->addSeparator();
	}

	CChoreoEvent *e = GetSafeEvent();
	if ( e )
	{
		if ( CountSelectedSamples() > 0 )
		{
			pop->add( va( "Delete" ), IDC_RT_DELETE );
			pop->add( "Deselect all", IDC_RT_DESELECT );
		}
		pop->add( "Select all", IDC_RT_SELECTALL );
	}

	pop->add( va( "Change scale..." ), IDC_RT_CHANGESCALE );

	pop->popup( this, (short)event->x, (short)event->y );
}

void RampTool::GetWorkspaceLeftRight( int& left, int& right )
{
	left = 0;
	right = w2();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RampTool::DrawFocusRect( void )
{
	HDC dc = GetDC( NULL );

	for ( int i = 0; i < m_FocusRects.Size(); i++ )
	{
		RECT rc = m_FocusRects[ i ].m_rcFocus;

		::DrawFocusRect( dc, &rc );
	}

	ReleaseDC( NULL, dc );
}

void RampTool::SetClickedPos( int x, int y )
{
	m_nClickedX = x;
	m_nClickedY = y;
}

float RampTool::GetTimeForClickedPos( void )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return 0.0f;

	float t = GetTimeValueForMouse( m_nClickedX );
	return t;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dragtype - 
//			startx - 
//			cursor - 
//-----------------------------------------------------------------------------
void RampTool::StartDragging( int dragtype, int startx, int starty, HCURSOR cursor )
{
	m_nDragType = dragtype;
	m_nStartX	= startx;
	m_nLastX	= startx;
	m_nStartY	= starty;
	m_nLastY	= starty;
	
	if ( m_hPrevCursor )
	{
		SetCursor( m_hPrevCursor );
		m_hPrevCursor = NULL;
	}
	m_hPrevCursor = SetCursor( cursor );

	m_FocusRects.Purge();

	RECT rcStart;
	rcStart.left = startx;
	rcStart.right = startx;

	bool addrect = true;
	switch ( dragtype )
	{
	case DRAGTYPE_SCRUBBER:
		{
			RECT rcScrub;
			GetScrubHandleRect( rcScrub, m_flScrub, true );

			rcStart = rcScrub;
			rcStart.left = ( rcScrub.left + rcScrub.right ) / 2;
			rcStart.right = rcStart.left;
			rcStart.top = rcScrub.bottom;

			rcStart.bottom = h2();
		}
		break;
	default:
		{
			rcStart.top = starty;
			rcStart.bottom = starty;
		}
		break;
	}


	if ( addrect )
	{
		AddFocusRect( rcStart );
	}
	
	DrawFocusRect();
}

void RampTool::OnMouseMove( mxEvent *event )
{
	int mx = (short)event->x;
	int my = (short)event->y;

	event->x = (short)mx;

	if ( m_nDragType != DRAGTYPE_NONE )
	{
		DrawFocusRect();

		for ( int i = 0; i < m_FocusRects.Size(); i++ )
		{
			CFocusRect *f = &m_FocusRects[ i ];
			f->m_rcFocus = f->m_rcOrig;

			switch ( m_nDragType )
			{
			default:
				{
					OffsetRect( &f->m_rcFocus, ( mx - m_nStartX ),	( my - m_nStartY ) );
				}
				break;
			case DRAGTYPE_SCRUBBER:
				{
					ApplyBounds( mx, my );
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( mx );
						t += m_flScrubberTimeOffset;
						ForceScrubPosition( t );
					}

					OffsetRect( &f->m_rcFocus, ( mx - m_nStartX ),	0 );
				}
				break;
			case DRAGTYPE_MOVEPOINTS_TIME:
			case DRAGTYPE_MOVEPOINTS_VALUE:
				{
					int dx = mx - m_nLastX;
					int dy = my - m_nLastY;

					if ( !( event->modifiers & mxEvent::KeyCtrl ) )
					{
						// Zero out motion on other axis
						if ( m_nDragType == DRAGTYPE_MOVEPOINTS_VALUE )
						{
							dx = 0;
							mx = m_nLastX;
						}
						else
						{
							dy = 0;
							my = m_nLastY;
						}
					}
					else
					{
						SetCursor( LoadCursor( NULL, IDC_SIZEALL ) );
					}

					RECT rcSamples;
					GetSampleTrayRect( rcSamples );

					int height = rcSamples.bottom - rcSamples.top;
					Assert( height > 0 );

					float dfdx = (float)dx / GetPixelsPerSecond();
					float dfdy = (float)dy / (float)height;

					MoveSelectedSamples( dfdx, dfdy );

					// Update the scrubber
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( mx );
						ForceScrubPosition( t );
						g_pMatSysWindow->Frame();
					}

					OffsetRect( &f->m_rcFocus, dx, dy );
				}
				break;
			case DRAGTYPE_SELECTION:
				{
					int dy = my - m_nLastY;	

					RECT rcFocus;
					
					rcFocus.left = m_nStartX < m_nLastX ? m_nStartX : m_nLastX;
					rcFocus.right = m_nStartX < m_nLastX ? m_nLastX : m_nStartX;

					rcFocus.top = m_nStartY < m_nLastY ? m_nStartY : m_nLastY;
					rcFocus.bottom = m_nStartY < m_nLastY ? m_nLastY : m_nStartY;

					POINT offset;
					offset.x = 0;
					offset.y = 0;
					ClientToScreen( (HWND)getHandle(), &offset );
					OffsetRect( &rcFocus, offset.x, offset.y );

					f->m_rcFocus = rcFocus;
				}
				break;
			}
		}

		DrawFocusRect();
	}
	else
	{
		if ( m_hPrevCursor )
		{
			SetCursor( m_hPrevCursor );
			m_hPrevCursor = NULL;
		}

		if ( IsMouseOverScrubHandle( event ) )
		{
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		}
		/*
		else if ( IsMouseOverTag( mx, my ) )
		{
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		}
		*/
	}

	m_nLastX = (short)event->x;
	m_nLastY = (short)event->y;
}

int	RampTool::handleEvent( mxEvent *event )
{
	int iret = 0;

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	switch ( event->event )
	{
	case mxEvent::Size:
		{
			int w, h;
			w = event->width;
			h = event->height;

			m_nLastHPixelsNeeded = 0;
			InvalidateLayout();
			iret = 1;
		}
		break;
	case mxEvent::MouseWheeled:
		{
			// Zoom time in  / out
			if ( event->height > 0 )
			{
				m_nTimeZoom = min( m_nTimeZoom + TIME_ZOOM_STEP, MAX_TIME_ZOOM );
			}
			else
			{
				m_nTimeZoom = max( m_nTimeZoom - TIME_ZOOM_STEP, TIME_ZOOM_STEP );
			}
			RepositionHSlider();
			redraw();
			iret = 1;
		}
		break;
	case mxEvent::MouseDown:
		{
			bool ctrldown = ( event->modifiers & mxEvent::KeyCtrl ) ? true : false;
			bool shiftdown = ( event->modifiers & mxEvent::KeyShift ) ? true : false;

			bool rightbutton = ( event->buttons & mxEvent::MouseRightButton ) ? true : false;

			iret = 1;

			int mx = (short)event->x;
			int my = (short)event->y;

			SetClickedPos( mx, my );

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			POINT pt;
			pt.x = mx;
			pt.y = my;

			RECT rcSamples;
			GetSampleTrayRect( rcSamples );

			bool insamplearea = PtInRect( &rcSamples, pt ) ? true : false;

			if ( m_nDragType == DRAGTYPE_NONE )
			{
				CExpressionSample *sample = GetSampleUnderMouse( event );

				if ( IsMouseOverScrubHandle( event ) )
				{
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( (short)event->x );
						m_flScrubberTimeOffset = m_flScrub - t;
						float maxoffset = 0.5f * (float)SCRUBBER_HANDLE_WIDTH / GetPixelsPerSecond();
						m_flScrubberTimeOffset = clamp( m_flScrubberTimeOffset, -maxoffset, maxoffset );
						t += m_flScrubberTimeOffset;
						ForceScrubPosition( t );
					}

					StartDragging( DRAGTYPE_SCRUBBER, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_SIZEWE ) );
				}
				else if ( insamplearea )
				{
					if ( sample )
					{
						if  ( shiftdown ) 
						{
							sample->selected = !sample->selected;
							redraw();
						}
						else if ( sample->selected )
						{
							g_pChoreoView->SetDirty( true );
							g_pChoreoView->PushUndo( "move ramp points" );

							StartDragging( 
								rightbutton ? DRAGTYPE_MOVEPOINTS_TIME : DRAGTYPE_MOVEPOINTS_VALUE, 
								m_nClickedX, m_nClickedY, 
								LoadCursor( NULL, rightbutton ? IDC_SIZEWE : IDC_SIZENS ) );
						}
						else
						{
							if  ( !shiftdown ) 
							{
								DeselectAll();
							}

							StartDragging( DRAGTYPE_SELECTION, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_ARROW ) );
						}
					}
					else if ( ctrldown )
					{
						CChoreoEvent *e = GetSafeEvent();
						if ( e )
						{
							// Add a sample point
							float t = GetTimeValueForMouse( mx );
							
							t = FacePoser_SnapTime( t );
							float value = 1.0f - (float)( (short)event->y - rcSamples.top ) / (float)( rcSamples.bottom - rcSamples.top );
							value = clamp( value, 0.0f, 1.0f );
							
							g_pChoreoView->SetDirty( true );
							g_pChoreoView->PushUndo( "Add ramp point" );

							e->AddRamp( t, value, false );

							e->ResortRamp();

							g_pChoreoView->PushRedo( "Add ramp point" );
							
							redraw();
						}
					}
					else
					{
						if ( event->buttons & mxEvent::MouseRightButton )
						{
							ShowContextMenu( event, false );
							iret = 1;
							return iret;
						}
						else
						{
							if  ( !shiftdown ) 
							{
								DeselectAll();
							}

							StartDragging( DRAGTYPE_SELECTION, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_ARROW ) );
						}
					}
				}
				else
				{
					if ( event->buttons & mxEvent::MouseRightButton )
					{
						ShowContextMenu( event, false );
						iret = 1;
						return iret;
					}
					else
					{
						if ( w2() > 0 )
						{
							float t = GetTimeValueForMouse( (short)event->x );

							SetScrubTargetTime( t );
						}
					}
				}

				CalcBounds( m_nDragType );
			}
		}
		break;
	case mxEvent::MouseDrag:
	case mxEvent::MouseMove:
		{
			int mx = (short)event->x;
			int my = (short)event->y;

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			OnMouseMove( event );

			iret = 1;
		}
		break;
	case mxEvent::MouseUp:
		{
			OnMouseMove( event );

			int mx = (short)event->x;
			int my = (short)event->y;

			if ( m_nDragType != DRAGTYPE_NONE )
			{
				DrawFocusRect();
			}

			if ( m_hPrevCursor )
			{
				SetCursor( m_hPrevCursor );
				m_hPrevCursor = 0;
			}

			switch ( m_nDragType )
			{
			case DRAGTYPE_NONE:
				break;
			case DRAGTYPE_SCRUBBER:
				{
					ApplyBounds( mx, my );

					int dx = mx - m_nStartX;
					int dy = my = m_nStartY;

					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( (short)event->x );
						t += m_flScrubberTimeOffset;
						ForceScrubPosition( t );
						m_flScrubberTimeOffset = 0.0f;
					}
				}
				break;
			case DRAGTYPE_MOVEPOINTS_VALUE:
			case DRAGTYPE_MOVEPOINTS_TIME:
				{
					g_pChoreoView->PushRedo( "move ramp points" );
				}
				break;
			case DRAGTYPE_SELECTION:
				{
					SelectPoints();
				}
				break;
			}

			m_nDragType = DRAGTYPE_NONE;

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			iret = 1;
		}
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				iret = 0;
				break;
			case IDC_UNDO_RT:
				{
					OnUndo();
				}
				break;
			case IDC_REDO_RT:
				{
					OnRedo();
				}
				break;
			case IDC_RT_DELETE:
				{
					Delete();
				}
				break;
			case IDC_RT_DESELECT:
				{
					DeselectAll();
				}
				break;
			case IDC_RT_SELECTALL:
				{
					SelectAll();
				}
				break;
			case IDC_RAMPHSCROLL:
				{
					int offset = 0;
					bool processed = true;

					switch ( event->modifiers )
					{
					case SB_THUMBTRACK:
						offset = event->height;
						break;
					case SB_PAGEUP:
						offset = m_pHorzScrollBar->getValue();
						offset -= 20;
						offset = max( offset, m_pHorzScrollBar->getMinValue() );
						break;
					case SB_PAGEDOWN:
						offset = m_pHorzScrollBar->getValue();
						offset += 20;
						offset = min( offset, m_pHorzScrollBar->getMaxValue() );
						break;
					case SB_LINEUP:
						offset = m_pHorzScrollBar->getValue();
						offset -= 10;
						offset = max( offset, m_pHorzScrollBar->getMinValue() );
						break;
					case SB_LINEDOWN:
						offset = m_pHorzScrollBar->getValue();
						offset += 10;
						offset = min( offset, m_pHorzScrollBar->getMaxValue() );
						break;
					default:
						processed = false;
						break;
					}

					if ( processed )
					{
						MoveTimeSliderToPos( offset );
					}
				}
				break;
			case IDC_RT_CHANGESCALE:
				{
					OnChangeScale();
				}
				break;
			}
		}
		break;
	case mxEvent::KeyDown:
		{
			iret = 1;
			switch ( event->key )
			{
			default:
				iret = 0;
				break;
			case VK_ESCAPE:
				DeselectAll();
				break;
			case VK_DELETE:
				Delete();
				break;
			}
		}
	}
	return iret;
}

void RampTool::ApplyBounds( int& mx, int& my )
{
	if ( !m_bUseBounds )
		return;

	mx = clamp( mx, m_nMinX, m_nMaxX );
}

void RampTool::CalcBounds( int movetype )
{
	switch ( movetype )
	{
	default:
	case DRAGTYPE_NONE:
		{
			m_bUseBounds = false;
			m_nMinX = 0;
			m_nMaxX = 0;
		}
		break;
	case DRAGTYPE_SCRUBBER:
		{
			m_bUseBounds = true;
			m_nMinX = 0;
			m_nMaxX = w2();
		}
		break;
	}
}

bool RampTool::PaintBackground()
{
	redraw();
	return false;
}

void RampTool::OnUndo( void )
{
	g_pChoreoView->Undo();
}

void RampTool::OnRedo( void )
{
	g_pChoreoView->Redo();
}

void RampTool::ForceScrubPositionFromSceneTime( float scenetime )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e || !e->GetDuration() )
		return;

	float t = scenetime - e->GetStartTime();
	m_flScrub = t;
	m_flScrubTarget = t;
	DrawScrubHandles();
}

void RampTool::ForceScrubPosition( float t )
{
	m_flScrub = t;
	m_flScrubTarget = t;
	
	CChoreoEvent *e = GetSafeEvent();
	if ( e && e->GetDuration() )
	{
		float realtime = e->GetStartTime() + t;

		g_pChoreoView->SetScrubTime( realtime );
		g_pChoreoView->SetScrubTargetTime( realtime );

		g_pChoreoView->DrawScrubHandle();
	}

	DrawScrubHandles();
}

void RampTool::SetMouseOverPos( int x, int y )
{
	m_nMousePos[ 0 ] = x;
	m_nMousePos[ 1 ] = y;
}

void RampTool::GetMouseOverPos( int &x, int& y )
{
	x = m_nMousePos[ 0 ];
	y = m_nMousePos[ 1 ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcPos - 
//-----------------------------------------------------------------------------
void RampTool::GetMouseOverPosRect( RECT& rcPos )
{
	rcPos.top = GetCaptionHeight() + 12;
	rcPos.left = w2() - 200;
	rcPos.right = w2() - 5;
	rcPos.bottom = rcPos.top + 13;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcPos - 
//-----------------------------------------------------------------------------
void RampTool::DrawMouseOverPos( CChoreoWidgetDrawHelper& drawHelper, RECT& rcPos )
{
	// Compute time for pixel x
	float t = GetTimeValueForMouse( m_nMousePos[ 0 ] );
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	t += e->GetStartTime();
	float snapped = FacePoser_SnapTime( t );

	// Found it, write out description
	// 
	char sz[ 128 ];
	if ( t != snapped )
	{
		Q_snprintf( sz, sizeof( sz ), "%s", FacePoser_DescribeSnappedTime( t ) );
	}
	else
	{
		Q_snprintf( sz, sizeof( sz ), "%.3f", t );
	}

	int len = drawHelper.CalcTextWidth( "Arial", 11, 900, sz );

	RECT rcText = rcPos;
	rcText.left = max( rcPos.left, rcPos.right - len );

	drawHelper.DrawColoredText( "Arial", 11, 900, RGB( 255, 50, 70 ), rcText, sz );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RampTool::DrawMouseOverPos()
{
	RECT rcPos;
	GetMouseOverPosRect( rcPos );

	CChoreoWidgetDrawHelper drawHelper( this, rcPos );
	DrawMouseOverPos( drawHelper, rcPos );
}

void RampTool::AddFocusRect( RECT& rc )
{
	RECT rcFocus = rc;

	POINT offset;
	offset.x = 0;
	offset.y = 0;
	ClientToScreen( (HWND)getHandle(), &offset );
	OffsetRect( &rcFocus, offset.x, offset.y );

	// Convert to screen space?
	CFocusRect fr;
	fr.m_rcFocus = rcFocus;
	fr.m_rcOrig = rcFocus;

	m_FocusRects.AddToTail( fr );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rc - 
//			left - 
//			right - 
//-----------------------------------------------------------------------------
void RampTool::DrawTimeLine( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, float left, float right )
{
	RECT rcLabel;
	float granularity = 0.5f;

	drawHelper.DrawColoredLine( RGB( 150, 150, 200 ), PS_SOLID, 1, rc.left, rc.top + 2, rc.right, rc.top + 2 );

	float f = SnapTime( left, granularity );
	while ( f < right )
	{
		float frac = ( f - left ) / ( right - left );
		if ( frac >= 0.0f && frac <= 1.0f )
		{
			rcLabel.left = GetPixelForTimeValue( f );
			rcLabel.top = rc.top + 5;
			rcLabel.bottom = rcLabel.top + 10;

			if ( f != left )
			{
				drawHelper.DrawColoredLine( RGB( 220, 220, 240 ), PS_DOT,  1, 
					rcLabel.left, rc.top, rcLabel.left, h2() );
			}

			char sz[ 32 ];
			sprintf( sz, "%.2f", f );

			int textWidth = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, sz );

			rcLabel.right = rcLabel.left + textWidth;

			OffsetRect( &rcLabel, -textWidth / 2, 0 );

			RECT rcOut = rcLabel;
			if ( rcOut.left <= 0 )
			{
				OffsetRect( &rcOut, -rcOut.left + 2, 0 );
			}

			drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 50, 150 ), rcOut, sz );

		}
		f += granularity;
	}
}

void RampTool::DrawTimingTags( CChoreoWidgetDrawHelper& drawHelper, RECT& rc )
{
	CChoreoEvent *rampevent = GetSafeEvent();
	if ( !rampevent )
		return;

	CChoreoScene *scene = rampevent->GetScene();
	if ( !scene )
		return;

	float starttime = GetTimeValueForMouse( 0 );
	float endtime = GetTimeValueForMouse( w2() );

	if ( endtime - starttime <= 0.0f )
		return;

	RECT rcText = rc;
	rcText.bottom = rcText.top + 10;

	drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 100, 200 ), rcText, "Timing Tags:" );

	// Loop through all events in scene

	int c = scene->GetNumEvents();
	int i;
	for ( i = 0; i < c; i++ )
	{
		CChoreoEvent *e = scene->GetEvent( i );
		if ( !e )
			continue;

		// See if time overlaps
		if ( !e->HasEndTime() )
			continue;

		if ( ( e->GetEndTime() - e->GetStartTime() ) < starttime )
			continue;

		if ( ( e->GetStartTime() - e->GetStartTime() ) > endtime )
			continue;

		if ( e->GetNumRelativeTags() > 0 )
		{
			DrawRelativeTagsForEvent( drawHelper, rc, rampevent, e, starttime, endtime );
		}
		if ( e->GetNumAbsoluteTags( CChoreoEvent::PLAYBACK ) > 0 )
		{
			DrawAbsoluteTagsForEvent( drawHelper, rc, rampevent, e, starttime, endtime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			&rc - 
//-----------------------------------------------------------------------------
void RampTool::DrawAbsoluteTagsForEvent( CChoreoWidgetDrawHelper& drawHelper, RECT &rc, CChoreoEvent *rampevent, CChoreoEvent *event, float starttime, float endtime )
{
	if ( !event )
		return;

	for ( int i = 0; i < event->GetNumAbsoluteTags( CChoreoEvent::PLAYBACK ); i++ )
	{
		CEventAbsoluteTag *tag = event->GetAbsoluteTag( CChoreoEvent::PLAYBACK, i );
		if ( !tag )
			continue;

		float tagtime = ( event->GetStartTime() + tag->GetTime() * event->GetDuration() ) - rampevent->GetStartTime();
		if ( tagtime < starttime || tagtime > endtime )
			continue;

		bool clipped = false;
		int left = GetPixelForTimeValue( tagtime, &clipped );
		if ( clipped )
			continue;

		RECT rcMark;
		rcMark = rc;
		rcMark.top = rc.bottom - 8;
		rcMark.bottom = rc.bottom;
		rcMark.left = left - 4;
		rcMark.right = left + 4;

		drawHelper.DrawTriangleMarker( rcMark, RGB( 0, 100, 250 ) );

		RECT rcText;
		rcText = rcMark;
		rcText.top -= 12;
		
		int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, tag->GetName() );
		rcText.left = left - len / 2;
		rcText.right = rcText.left + len + 2;

		rcText.bottom = rcText.top + 10;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 100, 200 ), rcText, tag->GetName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rc - 
//-----------------------------------------------------------------------------
void RampTool::DrawRelativeTagsForEvent( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, CChoreoEvent *rampevent, CChoreoEvent *event, float starttime, float endtime )
{
	if ( !event )
		return;

	//drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, PEColor( COLOR_PHONEME_TIMING_TAG ), rc, "Timing Tags:" );

	for ( int i = 0; i < event->GetNumRelativeTags(); i++ )
	{
		CEventRelativeTag *tag = event->GetRelativeTag( i );
		if ( !tag )
			continue;

		// 
		float tagtime = ( event->GetStartTime() + tag->GetPercentage() * event->GetDuration() ) - rampevent->GetStartTime();
		if ( tagtime < starttime || tagtime > endtime )
			continue;

		bool clipped = false;
		int left = GetPixelForTimeValue( tagtime, &clipped );
		if ( clipped )
			continue;

		//float frac = ( tagtime - starttime ) / ( endtime - starttime );

		//int left = rc.left + (int)( frac * ( float )( rc.right - rc.left ) + 0.5f );

		RECT rcMark;
		rcMark = rc;
		rcMark.top = rc.bottom - 8;
		rcMark.bottom = rc.bottom;
		rcMark.left = left - 4;
		rcMark.right = left + 4;

		drawHelper.DrawTriangleMarker( rcMark, RGB( 0, 100, 200 ) );

		RECT rcText;
		rcText = rc;
		rcText.bottom = rc.bottom - 10;
		rcText.top = rcText.bottom - 10;
	
		int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, tag->GetName() );
		rcText.left = left - len / 2;
		rcText.right = rcText.left + len + 2;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 100, 200 ), rcText, tag->GetName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int RampTool::ComputeHPixelsNeeded( void )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return 0;

	int pixels = 0;
	float maxtime = event->GetDuration();
	pixels = (int)( ( maxtime ) * GetPixelsPerSecond() + 10 );

	return pixels;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RampTool::RepositionHSlider( void )
{
	int pixelsneeded = ComputeHPixelsNeeded();

	if ( pixelsneeded <= w2() )
	{
		m_pHorzScrollBar->setVisible( false );
	}
	else
	{
		m_pHorzScrollBar->setVisible( true );
	}
	m_pHorzScrollBar->setBounds( 0, h2() - m_nScrollbarHeight, w2() - m_nScrollbarHeight, m_nScrollbarHeight );

	m_nLeftOffset = max( 0, m_nLeftOffset );
	m_nLeftOffset = min( pixelsneeded, m_nLeftOffset );

	m_pHorzScrollBar->setRange( 0, pixelsneeded );
	m_pHorzScrollBar->setValue( m_nLeftOffset );
	m_pHorzScrollBar->setPagesize( w2() );

	m_nLastHPixelsNeeded = pixelsneeded;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float RampTool::GetPixelsPerSecond( void )
{
	return m_flPixelsPerSecond * GetTimeZoomScale();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//-----------------------------------------------------------------------------
void RampTool::MoveTimeSliderToPos( int x )
{
	m_nLeftOffset = x;
	m_pHorzScrollBar->setValue( m_nLeftOffset );
	InvalidateRect( (HWND)m_pHorzScrollBar->getHandle(), NULL, TRUE );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RampTool::InvalidateLayout( void )
{
	if ( m_bSuppressLayout )
		return;

	if ( ComputeHPixelsNeeded() != m_nLastHPixelsNeeded )
	{
		RepositionHSlider();
	}

	m_bLayoutIsValid = false;
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : st - 
//			ed - 
//-----------------------------------------------------------------------------
void RampTool::GetStartAndEndTime( float& st, float& ed )
{
	st = m_nLeftOffset / GetPixelsPerSecond();
	ed = st + (float)w2() / GetPixelsPerSecond();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//			*clipped - 
// Output : int
//-----------------------------------------------------------------------------
int RampTool::GetPixelForTimeValue( float time, bool *clipped /*=NULL*/ )
{
	if ( clipped )
	{
		*clipped = false;
	}

	float st, ed;
	GetStartAndEndTime( st, ed );

	float frac = ( time - st ) / ( ed - st );
	if ( frac < 0.0 || frac > 1.0 )
	{
		if ( clipped )
		{
			*clipped = true;
		}
	}

	int pixel = ( int )( frac * w2() );
	return pixel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			clip - 
// Output : float
//-----------------------------------------------------------------------------
float RampTool::GetTimeValueForMouse( int mx, bool clip /*=false*/)
{
	float st, ed;
	GetStartAndEndTime( st, ed );

	if ( clip )
	{
		if ( mx < 0 )
		{
			return st;
		}
		if ( mx > w2() )
		{
			return ed;
		}
	}

	float frac = (float)( mx )  / (float)( w2() );
	return st + frac * ( ed - st );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float RampTool::GetTimeZoomScale( void )
{
	return ( float )m_nTimeZoom / 100.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : scale - 
//-----------------------------------------------------------------------------
void RampTool::SetTimeZoomScale( int scale )
{
	m_nTimeZoom = scale;
}

void RampTool::OnChangeScale( void )
{
	// Zoom time in  / out
	CInputParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Change Zoom" );
	strcpy( params.m_szPrompt, "New scale (e.g., 2.5x):" );

	Q_snprintf( params.m_szInputText, sizeof( params.m_szInputText ), "%.2f", (float)m_nTimeZoom / 100.0f );

	if ( !InputProperties( &params ) )
		return;

	m_nTimeZoom = clamp( (int)( 100.0f * atof( params.m_szInputText ) ), 1, MAX_TIME_ZOOM );

	m_nLastHPixelsNeeded = -1;
	m_nLeftOffset= 0;
	InvalidateLayout();
	Con_Printf( "Zoom factor %i %%\n", m_nTimeZoom );
}

void RampTool::DrawEventEnd( CChoreoWidgetDrawHelper& drawHelper )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	float duration = e->GetDuration();
	if ( !duration )
		return;

	int leftx = GetPixelForTimeValue( duration );
	if ( leftx >= w2() )
		return;

	RECT rcSample;
	GetSampleTrayRect( rcSample );

	drawHelper.DrawColoredLine(
		COLOR_CHOREO_ENDTIME, PS_SOLID, 1,
		leftx, rcSample.top, leftx, rcSample.bottom );
}

void RampTool::GetSampleTrayRect( RECT& rc )
{
	rc.left = 0;
	rc.right = w2();
	rc.top = GetCaptionHeight() + 65;

	rc.bottom = h2() - m_nScrollbarHeight-2;
}

void RampTool::DrawSamplesSimple( CChoreoWidgetDrawHelper& drawHelper, CChoreoEvent *e, bool clearbackground, COLORREF sampleColor, RECT &rcSamples )
{
	if ( clearbackground )
	{
		drawHelper.DrawFilledRect( RGB( 230, 230, 215 ), rcSamples );
	}

	if ( !e )
		return;

	float starttime = e->GetStartTime();
	float endtime = e->GetEndTime();

	COLORREF lineColor = sampleColor;
	COLORREF dotColor = RGB( 0, 0, 255 );
	COLORREF dotColorSelected = RGB( 240, 80, 20 );

	float fx = 0.0f;

	int width = rcSamples.right  - rcSamples.left;
	if ( width <= 0.0f )
		return;

	int height = rcSamples.bottom - rcSamples.top;
	int bottom = rcSamples.bottom;

	float timestepperpixel = e->GetDuration() / (float)width;

	float stoptime = endtime;
	
	float prev_value = e->GetIntensity( starttime );
	int prev_x = rcSamples.left;
	float prev_t = 0.0f;

	for ( float x = rcSamples.left; x < rcSamples.right; x+=3 )
	{
		float t = (float)( x - rcSamples.left ) * timestepperpixel;

		float value =  e->GetIntensity( starttime + t );

		// Draw segment
		drawHelper.DrawColoredLine( lineColor, PS_SOLID, 1,
			prev_x, bottom - prev_value * height,
			x, bottom - value * height );

		prev_x = x;
		prev_t = t;
		prev_value = value;
	}
}

void RampTool::DrawSamples( CChoreoWidgetDrawHelper& drawHelper, RECT &rcSamples )
{
	drawHelper.DrawFilledRect( RGB( 230, 230, 215 ), rcSamples );

	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	int rampCount = e->GetRampCount();
	if ( !rampCount )
		return;

	float starttime;
	float endtime;

	GetStartAndEndTime( starttime, endtime );

	COLORREF lineColor = RGB( 0, 0, 255 );
	COLORREF dotColor = RGB( 0, 0, 255 );
	COLORREF dotColorSelected = RGB( 240, 80, 20 );

	float fx = 0.0f;

	int height = rcSamples.bottom - rcSamples.top;
	int bottom = rcSamples.bottom;

	float timestepperpixel = 1.0f / GetPixelsPerSecond();

	float stoptime = min( endtime, e->GetDuration() );
	
	float prev_t = starttime;
	float prev_value = e->GetIntensity( prev_t );

	for ( float t = starttime-timestepperpixel; t <= stoptime; t += timestepperpixel )
	{
		float value =  e->GetIntensity( t + e->GetStartTime() );

		int prevx, x;

		bool clipped1, clipped2;
		x = GetPixelForTimeValue( t, &clipped1 );
		prevx = GetPixelForTimeValue( prev_t, &clipped2 );

		if ( !clipped1 && !clipped2 )
		{
			// Draw segment
			drawHelper.DrawColoredLine( lineColor, PS_SOLID, 1,
				prevx, bottom - prev_value * height,
				x, bottom - value * height );
		}

		prev_t = t;
		prev_value = value;

	}

	for ( int sample = 0; sample < rampCount; sample++ )
	{
		CExpressionSample *start = e->GetRamp( sample );

		/*
		int pixel = (int)( ( start->time / event_time ) * width + 0.5f);
		int x = m_rcBounds.left + pixel;
		float roundedfrac = (float)pixel / (float)width;
		*/
		float value = start->value;
		bool clipped = false;
		int x = GetPixelForTimeValue( start->time, &clipped );
		if ( clipped )
			continue;
		int y = bottom - value * height;

		int dotsize = 4;
		int dotSizeSelected = 5;

		COLORREF clr = dotColor;
		COLORREF clrSelected = dotColorSelected;

		drawHelper.DrawCircle( 
			start->selected ? clrSelected : clr, 
			x, y, 
			start->selected ? dotSizeSelected : dotsize,
			true );
	}
}

CExpressionSample *RampTool::GetSampleUnderMouse( mxEvent *event )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return NULL;

	RECT rcSamples;
	GetSampleTrayRect( rcSamples );

	POINT pt;
	pt.x = event->x;
	pt.y = event->y;

	if ( !PtInRect( &rcSamples, pt ) )
		return NULL;

	float timeperpixel = 1.0f / g_pRampTool->GetPixelsPerSecond();
	float closest_dist = 999999.f;
	CExpressionSample *bestsample = NULL;

	// Search for closes
	float t = GetTimeValueForMouse( event->x, false );

	for ( int i = 0; i < e->GetRampCount(); i++ )
	{
		CExpressionSample *sample = e->GetRamp( i );
		Assert( sample );

		float dist = fabs( sample->time - t );
		if ( dist < closest_dist )
		{
			bestsample = sample;
			closest_dist = dist;
		}

	}

	// Not close to any of them!!!
	if ( closest_dist > ( 5.0f * timeperpixel ) )
		return NULL;

	return bestsample;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RampTool::SelectPoints( void )
{
	RECT rcSelection;
	
	rcSelection.left = m_nStartX < m_nLastX ? m_nStartX : m_nLastX;
	rcSelection.right = m_nStartX < m_nLastX ? m_nLastX : m_nStartX;

	rcSelection.top = m_nStartY < m_nLastY ? m_nStartY : m_nLastY;
	rcSelection.bottom = m_nStartY < m_nLastY ? m_nLastY : m_nStartY;

	InflateRect( &rcSelection, 3, 3 );

	RECT rcSamples;
	GetSampleTrayRect( rcSamples );

	int width = w2();
	int height = rcSamples.bottom - rcSamples.top;

	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	float duration = e->GetDuration();

	float fleft = (float)GetTimeValueForMouse( rcSelection.left );
	float fright = (float)GetTimeValueForMouse( rcSelection.right );

	//fleft *= duration;
	//fright *= duration;

	float ftop = (float)( rcSelection.top - rcSamples.top ) / (float)height;
	float fbottom = (float)( rcSelection.bottom - rcSamples.top ) / (float)height;

	fleft = clamp( fleft, 0.0f, duration );
	fright = clamp( fright, 0.0f, duration );
	ftop = clamp( ftop, 0.0f, 1.0f );
	fbottom = clamp( fbottom, 0.0f, 1.0f );

	float timestepperpixel = 1.0f / GetPixelsPerSecond();
	float yfracstepperpixel = 1.0f / (float)height;

	float epsx = 2*timestepperpixel;
	float epsy = 2*yfracstepperpixel;

	for ( int i = 0; i < e->GetRampCount(); i++ )
	{
		CExpressionSample *sample = e->GetRamp( i );
		
		if ( sample->time + epsx < fleft )
			continue;

		if ( sample->time - epsx > fright )
			continue;

		//if ( (1.0f - sample->value ) + epsy < ftop )
		//	continue;

		//if ( (1.0f - sample->value ) - epsy > fbottom )
		//	continue;

		sample->selected = true;
	}

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int RampTool::CountSelectedSamples( void )
{
	int c = 0;
	CChoreoEvent *e = GetSafeEvent();
	if ( e )
	{
		int count = e->GetRampCount();

		for ( int i = 0; i < count; i++ )
		{
			CExpressionSample *sample = e->GetRamp( i );
			if ( !sample->selected )
				continue;

			c++;
		}
	}
	return c;
}

void RampTool::MoveSelectedSamples( float dfdx, float dfdy )
{
	int selecteditems = CountSelectedSamples();
	if ( !selecteditems )
		return;

	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	int c = e->GetRampCount();

	float duration = e->GetDuration();
	//dfdx *= duration;

	for ( int i = 0; i < c; i++ )
	{
		CExpressionSample *sample = e->GetRamp( i );
		if ( !sample || !sample->selected )
			continue;

		sample->time += dfdx;
		sample->time = clamp( sample->time, 0.0f, duration );

		sample->value -= dfdy;
		sample->value = clamp( sample->value, 0.0f, 1.0f );
	}
			
	e->ResortRamp();
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RampTool::DeselectAll( void )
{
	int i;

	int selecteditems = CountSelectedSamples();
	if ( !selecteditems )
		return;

	CChoreoEvent *e = GetSafeEvent();
	Assert( e );
	if ( !e )
		return;

	for ( i = e->GetRampCount() - 1; i >= 0 ; i-- )
	{
		CExpressionSample *sample = e->GetRamp( i );
		sample->selected = false;
	}
	
	redraw();
}

void RampTool::SelectAll( void )
{
	int i;

	CChoreoEvent *e = GetSafeEvent();
	Assert( e );
	if ( !e )
		return;

	for ( i = e->GetRampCount() - 1; i >= 0 ; i-- )
	{
		CExpressionSample *sample = e->GetRamp( i );
		sample->selected = true;
	}
	
	redraw();
}

void RampTool::Delete( void )
{
	int i;

	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	int selecteditems = CountSelectedSamples();
	if ( !selecteditems )
		return;

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Delete ramp points" );

	for ( i = e->GetRampCount() - 1; i >= 0 ; i-- )
	{
		CExpressionSample *sample = e->GetRamp( i );
		if ( !sample->selected )
			continue;

		e->DeleteRamp( i );
	}

	g_pChoreoView->PushRedo( "Delete ramp points" );

	redraw();
}
