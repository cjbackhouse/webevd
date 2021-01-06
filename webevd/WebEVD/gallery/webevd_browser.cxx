#include "gallery/Event.h"

#include "webevd/WebEVD/WebEVDServer.h"

// We use a function try block to catch and report on all exceptions.
int main(int argc, char** argv)
{
  evd::WebEVDServer<gallery::Event> server;
  server.serve_browser();
  return 0;
}
