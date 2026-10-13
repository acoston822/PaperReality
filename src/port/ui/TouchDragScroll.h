#pragma once

namespace PaperboatGui {
// Swipe-to-scroll for the window currently being drawn (a settings pane or a dropdown list). Call once per frame,
// inside the window, after its contents and before ending it. A press that starts on a slider, text field or colour
// picker belongs to that widget (see TouchDragScrollBlock); any other press that moves past a small threshold
// scrolls the window and is cancelled for the widget under the finger, so a swipe never toggles a checkbox or picks
// a dropdown entry. A quick flick keeps gliding after the finger lifts. Mouse drags scroll the same way.
void TouchDragScroll();

// Call right after drawing a widget that needs to keep its own drags (sliders, inputs, colour pickers) when it is
// the active item.
void TouchDragScrollBlock();

// Applies the same swipe/flick scrolling to every open popup list (dropdowns such as the Menu Theme colours and any
// other scrolling popup) without each one needing a hook. Call once per frame after all UI has been drawn.
void TouchDragScrollPopups();
} // namespace PaperboatGui
