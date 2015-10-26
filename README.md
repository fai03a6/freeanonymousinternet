FAI Core integration/staging tree
=====================================

http://f-a-i.net

What is ΦFAI?
----------------
The Free Anonymous Internet project, or FAI for short,Φ as symbol, is a decentralized ‘deep web’ service that is using blockchain technology to create a private, secure, peer-to-peer alternative to the regular world wide web.
 Some people call this kind of thing ‘web 3.0’ to denote the continued progression from broadcast style sites on web 1.0, to the peer-to-peer communication and user-driven content of social networking giants in web 2.0, to the kind of user-driven platforms and P2P protocols which now seem likely to drive the next big internet revolution. 
FAI comes with its own digital currency based on the Bitcoin code, but also enables its users to publish their own media content and to browse content posted by others in complete privacy – without anybody being able to spy on what you are doing. There is even a built-in decentralized marketplace to buy and sell products with other users.
FAI enables decentralized, serverless dyanic websites or apps programmed in HTML, CSS and JS with FAI API. 

For more information, as well as an immediately useable, binary version of
the FAI Core software, see http://f-a-i.net

Versions
----------
Version1(V1.0.0) is the core of FAI system. It consists of FAI network, FAI blockchain, FAI account and FAI currency system. Contents can be published and read via command line UI.
Version2 is a GUI system with web-browser GUI and the native applications running on FAI core.
V1 and V2 follows exactly the same FAI network protocol, and they can run together in FAI network ,  and V1 will not be ruled out of the network in the future.
V1 and V2 wallet file formats are different. Private key tranfer among different versions can be acheived by dumpprivkey and importprivkey rpc commands.


License
-------

Fai Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see http://opensource.org/licenses/MIT.

Development process
-------------------

Developers work in their own trees, then submit pull requests when they think
their feature or bug fix is ready.

If it is a simple/trivial/non-controversial change, then one of the Fai
development team members simply pulls it.

If it is a *more complicated or potentially controversial* change, then the patch
submitter will be asked to start a discussion (if they haven't already) .

The patch will be accepted if there is broad consensus that it is a good thing.
Developers should expect to rework and resubmit patches if the code doesn't
match the project's coding conventions (see [doc/coding.md](doc/coding.md)) or are
controversial.

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags] are created
regularly to indicate new official, stable release versions of Faicoin.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people lots of money.

### Manual Quality Assurance (QA) Testing

Large changes should have a test plan, and should be tested by somebody other
than the developer who wrote the code.


Development tips and tricks
---------------------------

**compiling for debugging**

Run configure with the --enable-debug option, then make. Or run configure with
CXXFLAGS="-g -ggdb -O0" or whatever debug flags you need.

**debug.log**

If the code is behaving strangely, take a look in the debug.log file in the data directory;
error and debugging messages are written there.

The -debug=... command-line option controls debugging; running with just -debug will turn
on all categories (and give you a very large debug.log file).

The Qt code routes qDebug() output to debug.log under category "qt": run with -debug=qt
to see it.

**testnet and regtest modes**

Run with the -testnet option to run with "play Faicoins" on the test network, if you
are testing multi-machine code that needs to operate across the internet.

If you are testing something that can run on one machine, run with the -regtest option.
In regression test mode, blocks can be created on-demand; see qa/rpc-tests/ for tests
that run in -regtest mode.

**DEBUG_LOCKORDER**

Fai Core is a multithreaded application, and deadlocks or other multithreading bugs
can be very difficult to track down. Compiling with -DDEBUG_LOCKORDER (configure
CXXFLAGS="-DDEBUG_LOCKORDER -g") inserts run-time checks to keep track of which locks
are held, and adds warnings to the debug.log file if inconsistencies are detected.
