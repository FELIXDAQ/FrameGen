# FrameGen
This is a generator program for WIB frames. The file format that is produced can be viewed diagrammatically on the last page of <a href="http://docs.dunescience.org/cgi-bin/RetrieveFile?docid=1701&filename=ProtoDUNE_to_FELIX.pdf&version=1">this document</a>. It is a preliminary format, so fundamental changes are expected soon. Additionally, many variables have been chosen at random: error bits are randomly set with fixed probability, for example, and the COLDATA blocks consist of noise exclusively.

Testing compression is a main reason for the creation of a WIB frame generator. Compression and decompression functions have therefore been incorporated as well and form a major focus of the generator. They have to be called to come into action, so by default no compression is applied to created frames.

Right now, the native zlib compression algorithm is the only compression method to be incorporated. More algorithms and more integrated compression are to follow. In order to configure zlib, run the commands "./configure; make test; make install" in the zlib-1.2.11 folder. The README located in that same folder contains more information.

## Building the package
In order to build the package, create a build directory:
```
mkdir build; cd build
```

Run CMake:
```
cmake ..
```

Then build the package:
```
make
```

To install the library and include file:
```
make install
```
