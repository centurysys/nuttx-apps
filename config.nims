############################################################################
# apps/config.nims
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

import std/os
import std/strutils

switch "os", "nuttx"
switch "mm", "orc"

switch "arm.nuttx.gcc.exe", "arm-none-eabi-gcc"
switch "arm64.nuttx.gcc.exe", "aarch64-none-elf-gcc"
switch "riscv32.nuttx.gcc.exe", "riscv64-unknown-elf-gcc"

switch "nimcache", ".nimcache"
switch "d", "useStdLib"
switch "d", "useMalloc"
switch "d", "nimAllocPagesViaMalloc"
switch "d", "noSignalHandler"
#switch "d", "ssl"
switch "threads", "off"
switch "noMain", "on"
switch "compileOnly", "on"
switch "noLinking", "on"


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
      let arch = keyval[1].strip(chars = {'"'})
      case arch
      of "arm", "arm64":
        result.arch = arch
      of "riscv":
        result.arch = "riscv32"
    of "DEBUG_NOOPT":
      result.opt = oNone
    of "DEBUG_FULLOPT":
      result.opt = oSize
    of "DEBUG_SYMBOLS":
      result.debugSymbols = true
    of "RAM_SIZE":
      result.ramSize = keyval[1].parseInt
  echo "* arch:    " & result.arch
  echo "* opt:     " & $result.opt
  echo "* debug:   " & $result.debugSymbols
  echo "* ramSize: " & $result.ramSize

proc setup_cfg(cfg: DotConfig) =
  switch("cpu", cfg.arch)
  if cfg.opt == oSize:
    switch("define", "release")
    switch("opt", "size")
  if cfg.debugSymbols:
    switch("lineDir", "on")
    switch("stackTrace", "on")
    switch("lineTrace", "on")
  let ramKilloBytes = cfg.ramSize.killoBytes
  if ramKilloBytes < 32:
    switch("define", "nimPage256")
  elif ramKilloBytes < 512:
    switch("define", "nimPage512")
  elif ramKilloBytes < 2048:
    switch("define", "nimPage1k")
  if ramKilloBytes < 512:
    switch("MemAlign", "4")


let topdir = getEnv("TOPDIR")
let cfg = read_config(topdir & "/.config")
cfg.setup_cfg()
