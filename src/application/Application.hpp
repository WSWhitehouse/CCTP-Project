#ifndef SNOWFLAKE_APPLICATION_HPP
#define SNOWFLAKE_APPLICATION_HPP

#include "pch.hpp"

namespace Application
{
  /** @brief Request to quit the application. */
  void Quit();

  /** @brief Has the application requested to quit. */
  b8 HasRequestedQuit();

} // namespace Application


#endif //SNOWFLAKE_APPLICATION_HPP
