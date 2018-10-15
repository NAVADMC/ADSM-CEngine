# ADSM-CEngine (Animal Disease Spread Model-CEngine)
ADSM is an application with Desktop and Web Based Frontend GUI for creating a simulation model to assist decision making and education in evaluating animal disease incursions.
This CEngine is the C code that runs the stochastic modeling simulation in the background.
Scenario parameters are sent to the CEngine via a Scenario SQLite file.
Results are written to the stdout, and supplemental result files are written to a directory alongside the SQLite scenario file.

## Compile CEngine Executable
The CEngine needs to be compiled for testing and deployment both.

Windows is the primary target currently as the ADSM Frontend application is mostly used on Windows.

Compiling on Linux isn't fully supported yet for two reasons:

1. The compile output of the CEngine compile process isn't yet deploy friendly. It isn't statically compiled and as such has a lot of dependent files that aren't easily distributed.
2. The ADSM Frontend application compile process requires manual intervention on Linux to get the output files collected into the proper folders for the executable to find them. This second reason doesn't matter when you are talking about doing a deployment on a Web Server as the ADMS GUI application does not need to be compiled for that sort of deployment.

To compile ADSM-CEngine, you will need two system packages installed already: Python3.4.x (x64) and Git (on the system path).

Python: https://www.python.org/downloads/release/python-343/  
Git: https://git-scm.com/downloads

### x86-64 Windows 7 - 10
Exactly follow these steps to compile the CEngine Executable for Windows.

1. Download and install msys2: http://repo.msys2.org/distrib/x86_64/msys2-x86_64-20161025.exe  
   **NOTE:** the home direcotry when in the msys shell is shows as '/home/username'; this directory is located at 'C:\msys2\home\username'
1. Open the msys2 terminal (NOT mingw) and run `pacman -Syu`
1. Close the terminal and reopen it. Note: It may be hard to force close the terminal, but this is the correct step
1. `pacman -Su`  
1. Install build tools needed by ADSM.  
`pacman -S pkg-config autoconf automake-wrapper gcc make bison python`
1. Install packages needed by ADSM. Specific versions should be installed and newer versions should be tested before the version numbers are incremented in these instructions.
`pacman -U http://repo.msys2.org/msys/x86_64/glib2-2.48.2-1-x86_64.pkg.tar.xz`  
`pacman -U http://repo.msys2.org/msys/x86_64/glib2-devel-2.48.2-1-x86_64.pkg.tar.xz`  
`pacman -U http://repo.msys2.org/mingw/x86_64/mingw-w64-x86_64-gsl-2.5-1-any.pkg.tar.xz`  
`pacman -U http://repo.msys2.org/msys/x86_64/libsqlite-devel-3.21.0-2-x86_64.pkg.tar.xz`  
`pacman -U http://repo.msys2.org/mingw/x86_64/mingw-w64-x86_64-shapelib-1.4.1-1-any.pkg.tar.xz`  
`pacman -U http://repo.msys2.org/mingw/x86_64/mingw-w64-x86_64-json-glib-1.2.8-1-any.pkg.tar.xz`
1. `ln -s /mingw64/include/json-glib-1.0/json-glib /mingw64/include/json-glib`
1. Close the terminal
1. Download the General Polygon Clipper Library (GPC): http://www.cs.man.ac.uk/~toby/gpc/assets/gpc232-release.zip
1. Unpack gpc (easiest if you unpack into your msys home directory 'C:\msys2\home\username\') 
1. Open the msys terminal and cd into the unpacked gpc directory
1. `mkdir -p /usr/local/lib`  
1. `mkdir -p /usr/local/include/gpcl/`  
1. `mkdir -p /share/aclocal/`   
1. `mkdir -p /mingw64/share/aclocal`  
1. `cc -c -o gpc.o gpc.c`  
1. `ar r libgpcl.a gpc.o`  
1. `cp libgpcl.a /usr/local/lib/`  
1. `cp gpc.h /usr/local/include/`   
1. `cp gpc.h /usr/local/include/gpcl/`
1. In your favorite text editor, add the following lines to '~/.bash_profile':
   ```bash
   export PATH=$PATH:/mingw64/bin  
   export CFLAGS='-I/usr/local/include -I/mingw64/include -I/mingw64/include/glib-2.0'   
   export LDFLAGS='-L/usr/local/lib -L/usr/lib -L/mingw64/lib'  
   export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/mingw64/lib/pkgconfig
   
   ```
1. Close the msys terminal.
1. Clone the ADSM-CEngine project to your desired directory:
   1. Open a Command Window in the directory you keep your projects (DRIVE:\\\\path\\to\\projects)
   1. `git clone git@github.com:NAVADMC/ADSM-CEngine.git`
1. Close the Command Window.
1. Open the msys terminal.
1. cd into the CEngine directory.
1. `sh bootstrap`  
   1. If you get an error like "' is already registered with AC_CONFIG_FILES.", this means that DOS/Windows style line endings have been introduced into the 'configure.ac' file.
   1. Open 'configure.ac' in Notepad++.
   1. Click through Edit -> EOL Conversion -> UNIX/OSX Format.
   1. Save the file and run command again.
1. `./configure --disable-debug`  
1. `make`  
   * The make command will build the ADSM executables but then stop on an error when it reaches the doc/diagrams directory (the free diagramming tool Dia is not available as a precompiled MSYS2 package)
   * The compiled output will be './main_loop/adsm_simulation.exe'
   * **NOTE:** You won't be able to run the output in place as it needs to be in the same directory as some dependent dll files (which are in the main ADSM repo already).
1. Take the './main_loop/adsm_simulation.exe' file and put it into ADSM Frontend application in 'ADSM/bin/'.
1. If once in the 'ADSM/bin' folder it won't run due to a missing dependency, look for the proper dll in your msys installation folders and move it to be alongside the 'adsm_simulation.exe' file in the 'ADSM/bin' folder

**NOTE:** To update the pkglist.txt, in the msys2 terminal, run `pacman -Qe > /path/to/pkglist.txt`

### x86-64 Debian Linux (Ubuntu preferred)
Follow these steps to compile the CEngine Executable for Linux.

1. Open a terminal
1. `sudo su`
1. `apt-get update`
1. `apt-get upgrade`
1. `apt-get install make gcc pkg-config autoconf automake bison python libglib2-dev libgd3 libgd-dev libgsl2 libgsl-dev sqlite3 libsqlite3-dev shapelib libshp-dev libjson-glib-1.0-0 proj-bin libproj-dev flex`
1. `wget http://www.cs.man.ac.uk/~toby/gpc/assets/gpc232-release.zip`
1. (unzip gpc)
1. `cd gpc232-release`
1. `mkdir -p /usr/local/lib`  
1. `mkdir -p /usr/local/include/gpcl/`  
1. `mkdir -p /share/aclocal/`
1. `cc -c -o gpc.o gpc.c`  
1. `ar r libgpcl.a gpc.o`  
1. `cp libgpcl.a /usr/local/lib/`  
1. `cp gpc.h /usr/local/include/`  
1. `cp gpc.h /usr/local/include/gpcl/` 
1. `export CFLAGS='-I/usr/local/include'`
1. `export LDFLAGS='-L/usr/local/lib -L/usr/lib'`
1. `cd /path/to/projects; git clone git@github.com:NAVADMC/ADSM-CEngine.git`
1. `cd /path/to/projects/ADSM-CEngine`
1. In your favorite text editor, modify './bootstrap' to comment out the Windows aclocal line and uncomment the Linux aclocal line
1. `sh bootstrap`
1. `./configure --disable-debug`
1. Change the `python` link to point to `python3` (this is a temp fix)
   1. `mv /usr/bin/python /usr/bin/python-back`
   1. `ln -s /usr/bin/python3 /usr/bin/python`
1. `make`
   * The make command will build the ADSM executables but then stop on an error when it reaches the doc/diagrams directory (compiling the free diagramming tool Dia is not covered here)
   * The compiled output will be './main_loop/adsm_simulation'
1. Undo your python change
   1. `rm /usr/bin/python`
   1. `mv /usr/bin/python-back /usr/bin/python`
1. Take the './main_loop/adsm_simulation' file and put it into ADSM Frontend application in 'ADSM/bin/'.
1. **WARNING** The 'adsm_simulation' will properly run on your system once in 'ADSM/bin/'.  
   HOWEVER! It will NOT run on another system that doesn't have the proper dependencies installed already (in exactly the correct location).  
   This is due to this compilation process not being statically linked and is the reason why distribution on Linux is not yet supported.

## ADSM-CEngine Breakdown
* From the perspective of the ADSM Frontend, the C Engine is a black box that accepts a DB input and creates stdout text output.  This is by design because the C Engine can also run as an independent command line program.  
* C Engine has its own internal sqlite3 parser to read in the entries from `ScenarioCreator.models` and start a simulation.  
* When the user selects the "Run Simulation" button it routes to `Results.views.run_simulation`
    * From there it creates a number of python Threads to spawn and watch 1 C Engine Process a piece.
    * `simulation_process()` watches the lines streaming off of its C Engine process and adds new database entries each day.  Only the Python modifies the database through Django at this phase.
    * On the last day, the C Engine opens a write connection and populates a series of UnitStat entries with summary information.  
        * Note that we've run into a number of concurrency issues at this step.  
* `Results.output_parser.parse_daily_strings()` is used every simulation day to create a series of `Results.models` instances.  
    * `DailyControls` contains everything that is only once per day.  
    * `DailyByZone`, `DailyByZoneAndProductionType` and `DailyByProductionType` have several copies output each day to match the objects to which they refer.  3 entries for 3 zones etc.
    * `Results.models.py` is nothing but a capture structure defined experimentally by the output from the C Engine.  Modify as necessary
        * Currently, `Results.output_grammar.grammars` holds the best understanding of the C Engine output.  
        * This was determined experimentally in [IPython Notebook](http://ipython.org/notebook.html) using [Output_Tables.ipynb](https://github.com/NAVADMC/ADSM/blob/master/development_scripts/Output_Tables.py) (the .py version is more readable if you don't have [IPython Notebook installed](https://store.continuum.io/cshop/anaconda/)).
* The C Engine code contains built-in documentation in the format expected by [Doxygen](http://doxygen.org/). Navigate to the `CEngine` folder of the source, run the command `doxygen`, then open `/doc/html/index.html` in a browser. The main points from the documentation:
    * The simulator is made up of a number of largely independent modules.  A module may
        * encapsulate knowledge (e.g., how long the incubating period lasts)
        * simulate a biological process (e.g., disease spread by airborne virus)
        * simulate one rule in a response policy (e.g., “when an infected unit is detected, establish a zone around it”)
        * monitor, count, bookkeep.
    * Which modules are instantiated depends on the input parameters.
    * When a module takes any interesting action, it generates an event. Other modules may react to these events. (Following the [Publish-Subscribe Pattern](http://en.wikipedia.org/wiki/Publish%E2%80%93subscribe_pattern).)
    * The main loop simply generates “New Day” events until one of the simulation’s stop conditions is met.
    * For a concrete example of the publish-subscribe system in action, consider the diagram below, which shows some of the effects a single Exposure event may have. <img src="http://navadmc.github.io/ADSM/images/example_chain_of_events.svg" width="100%">
        * When a NewDay event occurs, the contact spread module may generate Exposure events. An Exposure event is picked up by the population module, which resolves any competing changes from Exposure, Vaccination, and Destruction events, and then generates an Infection event. The disease module picks up the Infection event and decides the duration of the latent, infectious, and immune periods. (Red pathway)
        * The Exposure event is also picked up by the exposure monitor, which counts exposures. Earlier in the simulation, the exposure monitor would have generated a DeclarationOfOutputs event, containing references to the values it tracks; that event would have been picked up by the table writer module, so it can output that count of exposures on every simulation day.
        * The Exposure event is also picked up by the contact recorder module. There, the Exposure event does not have an effect right away. But if later a Detection event occurs, and the trace module is active, then the trace module generates an AttemptToTrace event to ask which other premises have had contact with the detected diseased place. The contact recorder module searches its stored records of Exposure events, and may generate a TraceResult event. The TraceResult event can further result in the traced premises being quarantined, having zones established around them, and/or being examined for signs of disease (which in turn can lead to more Detection events). (Blue pathway)
    * In short, the publish-subscribe design makes it easy to create a simple simulation by instantiating a few modules, or a complex simulation by instantiating many modules; it makes it easy to improve or replace modules in a piecewise fashion, as long as the new modules communicate in the same language of Exposure, Detection, etc. events; and it makes it easy to separate actions from bookkeeping and to separate bookkeeping from output formats.
