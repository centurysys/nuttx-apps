switch "arm.nuttx.gcc.exe", "arm-none-eabi-gcc"
switch "arm.nuttx.gcc.linkerexe", "arm-none-eabi-gcc"
switch "gcc.options.size", "-Os"
switch "gcc.options.speed", "-O2"

switch "mm", "orc"
switch "os", "nuttx"

--cpu:arm
--opt:size
switch "passC", "-flto"
switch "nimcache", ".nimcache"
switch "d", "useStdLib"
switch "d", "useMalloc"
switch "d", "nimAllocPagesViaMalloc"
switch "d", "nimPage1k"
switch "d", "noSignalHandler"
#switch "d", "ssl"
switch "threads", "off"
