/*
    Copyright (C) 2000-2007 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __gtk2_ardour_editor_items_h__
#define __gtk2_ardour_editor_items_h__

enum ItemType {
	RegionItem,
	StreamItem,
	WaveItem,
	PlayheadCursorItem,
	MarkerItem,
	MarkerBarItem,
	RangeMarkerBarItem,
	CdMarkerBarItem,
	VideoBarItem,
	SkipBarItem,
	SelectionItem,
	ControlPointItem,
	GainLineItem,
	AutomationLineItem,
	MeterMarkerItem,
	TempoMarkerItem,
	MeterBarItem,
	TempoBarItem,
	RegionViewNameHighlight,
	RegionViewName,
	StartSelectionTrimItem,
	EndSelectionTrimItem,
	AutomationTrackItem,
	FadeInItem,
	FadeInHandleItem,
	FadeInTrimHandleItem,
	FadeOutItem,
	FadeOutHandleItem,
	FadeOutTrimHandleItem,
	NoteItem,
	FeatureLineItem,
	LeftFrameHandle,
	RightFrameHandle,
	StartCrossFadeItem,
	EndCrossFadeItem,
	CrossfadeViewItem,
	TimecodeRulerItem,
	MinsecRulerItem,
	BBTRulerItem,
	SamplesRulerItem,
	DropZoneItem,
        ClockRulerItem,
        LeftDragHandle,
        RightDragHandle,
        
	/* don't remove this */

	NoItem
};

#endif /* __gtk2_ardour_editor_items_h__ */
