#include "input/Cursor.hpp"

#if defined(PLATFORM_WINDOWS)

#include <windows.h>

void Cursor::SetState(Cursor::State state)
{
  if (state == State::UNKNOWN) return;

  if (state == State::HIDDEN)
  {
    // Hide cursor
    i32 count;
    do { count = ::ShowCursor(false); }
    while (count >= 0);

    return;
  }

  // Show cursor
  i32 count;
  do { count = ::ShowCursor(true); }
  while (count < 0);
}

Cursor::State Cursor::GetState()
{
  CURSORINFO cursorInfo = {};
  cursorInfo.cbSize = sizeof(CURSORINFO);

  if (!::GetCursorInfo(&cursorInfo)) return State::UNKNOWN;
  return cursorInfo.flags == CURSOR_SHOWING ? State::SHOWN : State::HIDDEN;
}

#endif // PLATFORM_WINDOWS