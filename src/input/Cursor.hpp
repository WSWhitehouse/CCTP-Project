#ifndef SNOWFLAKE_CURSOR_HPP
#define SNOWFLAKE_CURSOR_HPP

#include "pch.hpp"

namespace Cursor
{

  enum class State : i16
  {
    UNKNOWN = -1,
    SHOWN,
    HIDDEN
  };

  void SetState(State state);
  State GetState();

} // namespace Cursor

#endif //SNOWFLAKE_CURSOR_HPP
