# FlameProfiler

Header only profiling library which generate a JSON file to use with Chrome://tracing

Instructions
------------

1. Add header file to your project
2. Instrument your code
3. Define ```USE_PROFILER```
4. Compile and run your program
5. Load generated ```profiler.json``` with Chrome://tracing

Instrumentation options
-----------------------

Zones are defined using the following commands:
```PZone("ZoneName")``` or ```PZone("ZoneName", "ZoneCategory")```

Zones are declared using RAII. Timing starts when a zone defintions is declared and ended when the corresponding scope ends.

```c++
void myFunction()
{
  PZone("MyZone"); // Timing starts here
  function1();
  // Scope ends and timing ends as well
} 
```

If multiple timing zones are desired inside one function these have to be separated by adding scopes around them.
```c++
void myAdvFunction()
{
  PZone("MainZone");
  {
    PZone("SubZone1");
    subfunction1();
  }
  {
    PZone("SubZone2");
    subfunction2();
  }
}
```

Metadata
--------
Per logfile metadata can be added using: ```PMetadata("Title", "Value")```

_**NOTE**_ Add meta data once! Do _not_ add metadata inside any loop!


**Examples:**

Add product name:

```PMetadata("product_name", "MyExecutable");```

Add compile date using built in compiler macro:

```PMetadata("build_date", __DATE__);```

Add compile time using built in compiler macro:

```PMetadata("build_time", __TIME__);```

Performance
-----------
The profiler will do timing with microsecond precision. Each log point is 128 bytes and stored in internal memory. The log is written to disk when the program exits. When ```USE_PROFILER``` is undefined the zone definition macros will expand to nothing, thus having no effect on the compiled executable.
