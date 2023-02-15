import std/asyncdispatch
import std/strformat
import jester

router myrouter:
  get "/hello/@name":
    let username = @"name"
    resp &"Hello, {username} !"

proc runserver(): Future[void] {.async.} =
  let port = 80.Port
  let settings = newSettings(port = port)
  var server = initJester(myrouter, settings = settings)
  server.serve()

proc run_http_server() {.exportc, cdecl.} =
  waitFor runserver()
