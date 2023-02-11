switch "arm.standalone.gcc.exe", "arm-none-eabi-gcc"
switch "arm.standalone.gcc.linkerexe", "arm-none-eabi-gcc"
switch "arm.any.gcc.exe", "arm-none-eabi-gcc"
switch "arm.any.gcc.linkerexe", "arm-none-eabi-gcc"
switch "gcc.options.size", "-Os"
switch "gcc.options.speed", "-O2"

switch "mm", "arc"
switch "os", "any"

--cpu:arm
--opt:size
switch "nimcache", ".nimcache"
switch "d", "useMalloc"
switch "d", "noSignalHandler"
