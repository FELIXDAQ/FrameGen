// This is an example program that showcases the various uses of FrameGen.

#include <iostream>
#include "FrameGen.hpp"

int main(int argc, char* argv[]) {
    
    // Take a command line argument if available and make a frame generator with the entered noise level (0-2^16).
    framegen::FrameGen* F1;
    if(argc>1)
        F1 = new framegen::FrameGen(atoi(argv[1]));
    else
        F1 = new framegen::FrameGen(32);
    F1->setPath("exampleframes/");
    F1->generate("myfirstframe");
    F1->check(); // Take filename parameters from F1.
    framegen::check("exampleframes/myfirstframe0.frame"); // Do the same thing using just the path and filename.
    
    // Make a separate generator and use it to put a thousand frames in a single file.
    framegen::FrameGen F2;
    F2.setPath("exampleframes/");
    F2.generateSingleFile("thousand",1000);
    F2.checkSingleFile();
    framegen::checkSingleFile("exampleframes/thousand.frame");
    
    // Make yet another generator and use it to create frames in separate files.
    framegen::FrameGen F3;
    F3.setPath("exampleframes/lotsoffiles/");
    F3.generate("hundred",100);
    F3.check(100); // Range checks need FrameGen parameters.
    
    // Compress a file and then decompress it again.
    // Since the old files are removed immediately, these two lines effectively do nothing. Comment out the decompression to view a compressed file.
    framegen::compressFile("test.txt");
    framegen::decompressFile("test.txt.comp");
    
    // Create a frame, fill it with the 21st frame of a file and then print it to another file.
    framegen::Frame Fr;
    Fr.load("exampleframes/thousand.frame", 20);
    Fr.print("exampleframes/printed.frame",'h'); // The 'h' option prints the frame in hexadecimal notation.
    // Extract and set the WIB header and a COLDATA block.
    framegen::WIB_header head(Fr.getWIBHeader());
    framegen::COLDATA_block block(Fr.getCOLDATABlock(2));
    Fr.setWIBHeader(head);
    Fr.setCOLDATABlock(1, block);
    
    return 0;
}
