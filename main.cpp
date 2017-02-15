// This is an example program that showcases the various uses of FrameGen.

#include <iostream>
#include "FrameGen.hpp"

using namespace std;

int main(int argc, char* argv[]) {

    // Take a command line argument if available and make a frame generator with the entered noise level (0-2^16).
    FrameGen* F1;
    if(argc>1)
    F1 = new FrameGen(stoi(argv[1]));
    else
    F1 = new FrameGen(32);
    F1->setPath("exampleframes/");
    F1->generate("myfirstframe");
    F1->check(); // F1 automatically checks the last used file.

    // Make a separate generator and use it to put a thousand frames in a single file.
    FrameGen F2;
    F2.setPath("exampleframes/");
    F2.generateSingleFile("thousand",1000);
    F2.checkSingleFile();

    // Make yet another generator and use it to create frames in separate files.
    FrameGen F3;
    F3.setPath("exampleframes/lotsoffiles/");
    F3.generate("hundred",100);
    F3.check(100);

    // Compress a file and then decompress it again.
    // Since the old files are removed immediately, these two lines effectively do nothing. Comment out the decompression to view a compressed file.
    F3.compressFile("test.txt");
    F3.decompressFile("test.txt.comp");

    return 0;
}
