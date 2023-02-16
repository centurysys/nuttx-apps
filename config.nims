import std/os
import std/strutils

switch "os", "nuttx"
switch "mm", "orc"

switch "arm.nuttx.gcc.exe", "arm-none-eabi-gcc"
switch "arm.nuttx.gcc.linkerexe", "arm-none-eabi-gcc"
switch "arm64.nuttx.gcc.exe", "aarch64-none-elf-gcc"
switch "arm64.nuttx.gcc.linkerexe", "aarch64-none-elf-gcc"

switch "nimcache", ".nimcache"
switch "d", "useStdLib"
switch "d", "useMalloc"
switch "d", "nimAllocPagesViaMalloc"
switch "d", "noSignalHandler"
#switch "d", "ssl"
switch "threads", "off"

type
  OptFlag = enum
    oNone
    oSize
  DotConfig = object
    arch: string
    opt: OptFlag
    debugSymbols: bool
    ramSize: int

proc killoBytes(bytes: int): int =
  result = (bytes / 1024).int

proc read_config(cfg: string): DotConfig =
  for line in cfg.readFile.splitLines:
    if not line.startsWith("CONFIG_"):
      continue
    let keyval = line.replace("CONFIG_", "").split("=")
    if keyval.len != 2:
      continue
    case keyval[0]
    of "ARCH":
      result.arch = keyval[1].strip(chars = {'"'})
    of "DEBUG_NOOPT":
      result.opt = oNone
    of "DEBUG_FULLOPT":
      result.opt = oSize
    of "DEBUG_SYMBOLS":
      result.debugSymbols = true
    of "RAM_SIZE":
      result.ramSize = keyval[1].parseInt

proc setup_cfg(cfg: DotConfig) =
  switch("cpu", cfg.arch)
  if cfg.opt == oSize:
    switch("define", "release")
    switch("opt", "size")
  let ramKilloBytes = cfg.ramSize.killoBytes
  if ramKilloBytes < 32:
    switch("define", "nimPage256")
  elif ramKilloBytes < 512:
    switch("define", "nimPage512")
  elif ramKilloBytes < 2048:
    switch("define", "nimPage1k")
  if ramKilloBytes < 512:
    switch("MemAline", "4")


let topdir = getEnv("TOPDIR")
let cfg = read_config(topdir & "/.config")
cfg.setup_cfg()
