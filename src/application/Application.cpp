#include "application/Application.hpp"
#include "application/ApplicationInfo.hpp"

static b8 quit = false;

void Application::Quit()
{
  quit = true;
}

b8 Application::HasRequestedQuit()
{
  return quit;
}