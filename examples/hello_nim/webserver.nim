import std/asyncdispatch
import std/strformat
import std/strutils
import jester

router myrouter:
  get "/hello/@name":
    GC_runOrc()
    let username = @"name"
    var buf = newSeqOfCap[string](5)
    buf.add(&"Hello, {username} !")
    buf.add("--- Memory Information ---")
    buf.add(readFile("/proc/meminfo"))
    resp buf.join("\n").replace("\n", "<br/>")

proc runserver(): Future[void] {.async.} =
  let port = 80.Port
  let settings = newSettings(port = port)
  var server = initJester(myrouter, settings = settings)
  server.serve()

proc run_http_server() {.exportc, cdecl.} =
  waitFor runserver()
